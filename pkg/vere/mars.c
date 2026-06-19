/* worker/mars.c
**
**  the main loop of a mars process.
*/
#include "c3/c3.h"
#include "version.h"
#include "noun.h"
#include "pace.h"
#include "types.h"
#include "vere.h"
#include "ivory.h"
#include "ur/ur.h"
#include "db/lmdb.h"
#include "blob.h"
#include <mars.h>
#include <stdio.h>
#include <sys/time.h>

/* Blob storage lifecycle
**
**   blb_p HAMT: bid -> u3a_blob* (loom offset), bid = (mug_h << 32) | seq_h.
**   See the u3a_blob block in allocate.h for the counter design:
**     use_w = total refs: eve_w + les_h + live bob-atom cardinality
**     eve_w = event-log refcount (bumped on commit, rebuilt on chop)
**     les_h = king-held leases (durable in the LMDB LEASES table; the
**             loom les_h is zeroed on boot and rebuilt from that table)
**
**   Blob files live at $pier/.urb/bob/<mug>/<seq>.
**   Deletion condition: use_w == 0.
**
**   Lifecycle:
**     1. king stages a large file to .urb/bob/stg/, sends a %blob writ
**     2. mars installs it (rename into bob/<mug>/<seq>), commits a LEASES
**        row, and issues the king's lease (les_h += 1, 15-min TTL) — the
**        row is durable before the install is acked, so a mars crash in
**        the install->commit window cannot lose it
**     3. king injects the event holding the bob atom (RAM-serialized as
**        a blob ref); while it still references the blob, the king's
**        renewal timer sends %blas every 5 min so the lease can't
**        expire before commit
**     4. mars commits: _mars_fact bumps eve_w, writes blob-refs to LMDB
**     5. king's last reference dies: %blrl releases the lease (drops one
**        les_h unit and its LEASES row)
**     6. arvo drops the atom (e.g. |rm + |tomb): _me_bob_dead drops the
**        cardinality contribution from use_w
**     7. chop rebuilds eve_w from the LMDB BLOBS table for retained
**        epochs, then deletes files (and on-disk orphans) at use_w == 0
**     *. boot: _find_home zeroes les_h, _mars_play_leases restores it
**        from LEASES (pruning expired/dead rows), then u3_disk_blob_gc
**        reclaims any blob now provably unreferenced
*/

/* _mars_lease: PQ entry for lease TTL expiry.
**
**   tracks a single les_h increment for a blob, mirrored by exactly one
**   row in the LMDB LEASES table (identified by [lea_d]).  if the king
**   releases the lease (%blrl) before expiry, ded_o is set to c3y and
**   the PQ sweeper skips the decrement; the row is deleted at release.
**   if the king crashes or leaks, the TTL fires, les_h is decremented,
**   and the row is deleted by the sweeper.
*/
typedef struct __mars_lease {
  c3_d  exp_d;        //  expiry time (Unix ms)
  c3_d  lea_d;        //  unique lease id (LEASES row discriminator)
  c3_h  mug_h;        //  blob mug
  c3_h  seq_h;        //  blob seq within mug bucket
  c3_o  ded_o;        //  c3y if lease already released
} _mars_lease;

c3_c tac_c[256];  //  tracing label

/* _mars_lease_pq: min-heap of _mars_lease*, keyed by lea_u->exp_d.
**
**   C-heap structure (not in loom). Leases are owned by the PQ —
**   it is the sole place that c3_free()s them. Released leases (%blrl)
**   are marked ded_o=c3y in place; the sweeper pops and frees them
**   when they bubble to the top.
*/
typedef struct _mars_lease_pq {
  _mars_lease** arr_u;
  c3_z        len_z;
  c3_z        cap_z;
} _mars_lease_pq;

static _mars_lease_pq _mars_pq = { 0, 0, 0 };

//  monotonic lease-id source.  Seeded above any id restored from the
//  LEASES table at boot so fresh leases never collide with durable rows.
//
static c3_d _mars_lea_d = 0;

static inline void
_mars_pq_swap(_mars_lease_pq* pq_u, c3_z i_z, c3_z j_z)
{
  _mars_lease* t_u = pq_u->arr_u[i_z];
  pq_u->arr_u[i_z] = pq_u->arr_u[j_z];
  pq_u->arr_u[j_z] = t_u;
}

static void
_mars_pq_up(_mars_lease_pq* pq_u, c3_z i_z)
{
  while ( i_z > 0 ) {
    c3_z p_z = (i_z - 1) >> 1;
    if ( pq_u->arr_u[p_z]->exp_d <= pq_u->arr_u[i_z]->exp_d ) break;
    _mars_pq_swap(pq_u, p_z, i_z);
    i_z = p_z;
  }
}

static void
_mars_pq_down(_mars_lease_pq* pq_u, c3_z i_z)
{
  c3_z n_z = pq_u->len_z;
  while ( 1 ) {
    c3_z l_z = (i_z << 1) + 1;
    c3_z r_z = l_z + 1;
    c3_z s_z = i_z;
    if ( l_z < n_z && pq_u->arr_u[l_z]->exp_d < pq_u->arr_u[s_z]->exp_d ) s_z = l_z;
    if ( r_z < n_z && pq_u->arr_u[r_z]->exp_d < pq_u->arr_u[s_z]->exp_d ) s_z = r_z;
    if ( s_z == i_z ) break;
    _mars_pq_swap(pq_u, i_z, s_z);
    i_z = s_z;
  }
}

static void
_mars_pq_push(_mars_lease_pq* pq_u, _mars_lease* lea_u)
{
  if ( pq_u->len_z == pq_u->cap_z ) {
    pq_u->cap_z = pq_u->cap_z ? (pq_u->cap_z << 1) : 16;
    pq_u->arr_u = c3_realloc(pq_u->arr_u, pq_u->cap_z * sizeof(*pq_u->arr_u));
  }
  pq_u->arr_u[pq_u->len_z++] = lea_u;
  _mars_pq_up(pq_u, pq_u->len_z - 1);
}

static _mars_lease*
_mars_pq_peek(_mars_lease_pq* pq_u)
{
  return pq_u->len_z ? pq_u->arr_u[0] : 0;
}

static _mars_lease*
_mars_pq_pop(_mars_lease_pq* pq_u)
{
  if ( !pq_u->len_z ) return 0;
  _mars_lease* r_u = pq_u->arr_u[0];
  pq_u->arr_u[0] = pq_u->arr_u[--pq_u->len_z];
  if ( pq_u->len_z ) _mars_pq_down(pq_u, 0);
  return r_u;
}

/* _mars_pq_kill_one(): mark the first live lease for (mug_h, seq_h) as
**   released, returning its entry (so its LMDB row can be deleted) or 0
**   if none is live.  The sweeper frees the entry when it surfaces.
**
**   linear scan: the PQ is small (a handful of in-flight leases) and
**   release is rare.  ded_o entries are skipped so each %blrl retires
**   exactly one lease unit.
*/
static _mars_lease*
_mars_pq_kill_one(_mars_lease_pq* pq_u, c3_h mug_h, c3_h seq_h)
{
  for ( c3_z i_z = 0; i_z < pq_u->len_z; i_z++ ) {
    _mars_lease* lea_u = pq_u->arr_u[i_z];
    if (  (c3n == lea_u->ded_o)
       && (mug_h == lea_u->mug_h)
       && (seq_h == lea_u->seq_h) )
    {
      lea_u->ded_o = c3y;
      return lea_u;
    }
  }
  return 0;
}

/* _mars_blob_del(): delete blob file and clean up blb_p entry.
**
**   invariant guard: a blob file must survive until NO event-log ref,
**   lease, or loom atom references it.  use_w == 0 with eve_w or les_h
**   nonzero means the counters are corrupt — never destroy data on an
**   accounting bug; report loudly and keep the file.
*/
static void
_mars_blob_del(c3_h mug_h, c3_h seq_h)
{
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);

  if ( blb_u && (blb_u->eve_w || blb_u->les_h) ) {
    fprintf(stderr, "mars: blob %08" PRIx32 "/%08" PRIx32 ": refusing "
                    "delete: use=%" PRIc3_w " eve=%" PRIc3_w
                    " les=%" PRIc3_h " (counter corruption)\r\n",
                    mug_h, seq_h,
                    blb_u->use_w, blb_u->eve_w, blb_u->les_h);
    return;
  }

  u3_blob_wipe(u3C.dir_c, mug_h, seq_h);
  u3a_blob_drop(mug_h, seq_h);
}

/* _blob_maybe_delete(): delete blob iff use_w == 0.
*/
static void
_blob_maybe_delete(c3_h mug_h, c3_h seq_h)
{
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  if ( !blb_u ) return;

  if ( 0 == blb_u->use_w ) {
    _mars_blob_del(mug_h, seq_h);
  }
}

/* _mars_lease_take(): issue a king lease on (mug_h, seq_h).
**
**   bumps les_h + use_w, durably records the lease in the LEASES table,
**   then pushes a 15-min TTL PQ entry.  renewal (%blas) is just another
**   take: each adds one durable row + PQ entry; old ones decay at expiry
**   (the les_h > 0 guard in the sweeper prevents underflow).
**
**   the row is committed BEFORE the caller acks the king, so a lease the
**   king believes it holds always survives a mars crash.  on commit
**   failure the in-memory bump is rolled back and c3n is returned; the
**   blob is then protected only by whatever other refs exist (the king
**   retries, or the file is reclaimed as an orphan).
**
**   [new_o]: create the blb_p entry if absent (install only).  renewals
**   must not create: a %blas in flight when the blob is deleted would
**   otherwise resurrect a phantom entry for a file that no longer
**   exists.  a renewal for an absent entry is a no-op success.
*/
static c3_o
_mars_lease_take(MDB_env* env_u, c3_h mug_h, c3_h seq_h, c3_o new_o)
{
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  if ( !blb_u ) {
    if ( c3n == new_o ) {
      return c3y;
    }
    blb_u = u3a_blob_new(mug_h, seq_h);
  }
  blb_u->les_h += 1;
  blb_u->use_w += 1;

  c3_d exp_d;
  {
    struct timeval tv_u;
    gettimeofday(&tv_u, 0);
    exp_d = (c3_d)tv_u.tv_sec * 1000ULL
          + (c3_d)tv_u.tv_usec / 1000ULL
          + 900000ULL;  //  15 min TTL
  }

  c3_d bid_d = ((c3_d)mug_h << 32) | (c3_d)seq_h;
  c3_d lea_d = ++_mars_lea_d;

  //  durably record the lease before it is observable to the king
  //
  if ( c3n == u3_lmdb_save_lease(env_u, bid_d, exp_d, lea_d) ) {
    blb_u->les_h -= 1;
    blb_u->use_w -= 1;
    return c3n;
  }

  {
    _mars_lease* lea_u = c3_malloc(sizeof(*lea_u));
    lea_u->mug_h = mug_h;
    lea_u->seq_h = seq_h;
    lea_u->exp_d = exp_d;
    lea_u->lea_d = lea_d;
    lea_u->ded_o = c3n;
    _mars_pq_push(&_mars_pq, lea_u);
  }

  return c3y;
}

/*
::  peek=[gang (each path $%([%once @tas @tas path] [%beam @tas beam]))]
::  ovum=ovum
::
|$  [peek ovum]
|%
+$  task                                                ::  urth -> mars
  $%  [%live ?(%meld %pack) ~] :: XX rename
      [%exit ~]
      [%peek mil=@ peek]
      [%poke mil=@ ovum]
      [%sync %save ~]
  ==
+$  gift                                                ::  mars -> urth
  $%  [%live ~]
      [%flog cord]
      [%slog pri=@ tank]
      [%peek p=(each (unit (cask)) goof)]
      [%poke p=(each (list ovum) (list goof))]
      [%ripe [pro=%2 kel=wynn] [who=@p fake=?] eve=@ mug=@]
      [%sync eve=@ mug=@]
  ==
--
*/

/*  mars memory-threshold levels
*/
enum {
  _mars_mas_init = 0,  //  initial
  _mars_mas_hit1 = 1,  //  past low threshold
  _mars_mas_hit0 = 2   //  have high threshold
};

/*  mars post-op flags
*/
enum {
  _mars_fag_none = 0,       //  nothing to do
  _mars_fag_hit1 = 1 << 0,  //  hit low threshold
  _mars_fag_hit0 = 1 << 1,  //  hit high threshold
  _mars_fag_mute = 1 << 2,  //  mutated kernel
  _mars_fag_much = 1 << 3,  //  bytecode hack
  _mars_fag_vega = 1 << 4,  //  kernel reset
};

/* _mars_quac: convert a quac to a noun.
*/
u3_noun
_mars_quac(u3m_quac* mas_u)
{
  u3_noun list = u3_nul;
  c3_w i_w = 0;
  if ( mas_u->qua_u != NULL ) {
    while ( mas_u->qua_u[i_w] != NULL ) {
      list = u3nc(_mars_quac(mas_u->qua_u[i_w]), list);
      i_w++;
    }
  }
  list = u3kb_flop(list);

  u3_noun mas = u3nt(u3i_string(mas_u->nam_c), u3i_word(mas_u->siz_w), list);

  c3_free(mas_u->nam_c);
  c3_free(mas_u->qua_u);
  c3_free(mas_u);

  return mas;
}

/* _mars_quacs: convert an array of quacs to a noun list.
*/
u3_noun
_mars_quacs(u3m_quac** all_u)
{
  u3_noun list = u3_nul;
  c3_w i_w = 0;
  while ( all_u[i_w] != NULL ) {
    list = u3nc(_mars_quac(all_u[i_w]), list);
    i_w++;
  }
  c3_free(all_u);
  return u3kb_flop(list);
}

/* _mars_print_quacs: print an array of quacs.
*/
void
_mars_print_quacs(FILE* fil_u, u3m_quac** all_u)
{
  fprintf(fil_u, "\r\n");
  c3_w i_w = 0;
  while ( all_u[i_w] != NULL ) {
    u3a_print_quac(fil_u, 0, all_u[i_w]);
    i_w++;
  }
}

/* _mars_grab(): garbage collect, checking for profiling.
*/
static u3_noun
_mars_grab(u3_noun sac, c3_o pri_o)
{
  if ( u3_nul == sac) {
    if ( u3C.wag_h & (u3o_debug_ram | u3o_check_corrupt) ) {
      u3m_grab(sac);
    }
    return u3_nul;
  }
  else {
    FILE* fil_u;

#ifdef U3_MEMORY_LOG
    u3_noun now;

    {
      struct timeval tim_u;
      gettimeofday(&tim_u, 0);
      now = u3m_time_in_tv(&tim_u);
    }

    {
      u3_noun wen = u3dc("scot", c3__da, now);
      c3_c* wen_c = u3r_string(wen);

      c3_c nam_c[2048];
      snprintf(nam_c, 2048, "%s/.urb/put/mass", u3C.dir_c);

      struct stat st;
      if ( -1 == stat(nam_c, &st) ) {
        c3_mkdir(nam_c, 0700);
      }

      c3_c man_c[2054];
      snprintf(man_c, 2053, "%s/%s-serf.txt", nam_c, wen_c);

      fil_u = c3_fopen(man_c, "w");
      fprintf(fil_u, "%s\r\n", wen_c);

      c3_free(wen_c);
      u3z(wen);
    }
#else
    {
      fil_u = stderr;
    }
#endif

    u3_assert( u3R == &(u3H->rod_u) );

    u3a_mark_init();

    u3m_quac* pro_u = u3a_prof(fil_u, sac);
    c3_w      sac_w = u3a_mark_noun(sac);

    if ( NULL == pro_u ) {
      fflush(fil_u);
      u3z(sac);
      return u3_nul;
    } else {
      u3m_quac** all_u = c3_malloc(sizeof(*all_u) * 11);
      all_u[0] = pro_u;

      u3m_quac** var_u = u3m_mark();
      all_u[1] = var_u[0];
      all_u[2] = var_u[1];
      all_u[3] = var_u[2];
      all_u[4] = var_u[3];
      c3_free(var_u);

      c3_w tot_w = all_u[0]->siz_w + all_u[1]->siz_w + all_u[2]->siz_w
                     + all_u[3]->siz_w + all_u[4]->siz_w;

      all_u[5] = c3_calloc(sizeof(*all_u[5]));
      all_u[5]->nam_c = strdup("space profile");
      all_u[5]->siz_w = sac_w * sizeof(c3_w);

      tot_w += all_u[5]->siz_w;

      all_u[6] = c3_calloc(sizeof(*all_u[6]));
      all_u[6]->nam_c = strdup("total marked");
      all_u[6]->siz_w = tot_w;

      all_u[7] = c3_calloc(sizeof(*all_u[7]));
      all_u[7]->nam_c = strdup("free lists");
      all_u[7]->siz_w = u3a_idle(u3R) * sizeof(c3_w);

      //  XX sweep could be optional, gated on u3o_debug_ram or somesuch
      //  only u3a_mark_done() is required
      all_u[8] = c3_calloc(sizeof(*all_u[8]));
      all_u[8]->nam_c = strdup("sweep");
      all_u[8]->siz_w = u3a_sweep() * sizeof(c3_w);

      all_u[9] = c3_calloc(sizeof(*all_u[9]));
      all_u[9]->nam_c = strdup("loom");
      all_u[9]->siz_w = u3C.wor_i * sizeof(c3_w);

      all_u[10] = NULL;

      if ( c3y == pri_o ) {
        _mars_print_quacs(fil_u, all_u);
      }
      fflush(fil_u);

#ifdef U3_MEMORY_LOG
      {
        fclose(fil_u);
      }
#endif

      u3_noun mas = _mars_quacs(all_u);
      u3z(sac);

      return mas;
    }
  }
}

/* _mars_blob_bobs_atom(): u3a_walk_fore atom callback — collect bob atoms.
*/
static void
_mars_blob_bobs_atom(u3_atom a, void* ptr_v)
{
  if ( c3y != u3a_is_bob(a) ) {
    return;
  }
  //  grow the C-heap array
  //
  struct { c3_d* ids; c3_z len; c3_z cap; } *acc = ptr_v;
  if ( acc->len == acc->cap ) {
    acc->cap = acc->cap ? acc->cap * 2 : 8;
    acc->ids  = c3_realloc(acc->ids, acc->cap * sizeof(c3_d));
  }
  c3_h mug_h = u3a_bob_mug(a);
  c3_h seq_h = u3a_bob_seq(a);
  acc->ids[acc->len++] = ((c3_d)mug_h << 32) | (c3_d)seq_h;
}

/* _mars_blob_bobs_cell(): u3a_walk_fore cell callback — always descend.
*/
static c3_o
_mars_blob_bobs_cell(u3_noun n, void* ptr_v)
{
  (void)n; (void)ptr_v;
  return c3y;
}

/* _mars_fact(): commit a fact and enqueue its effects.
*/
static void
_mars_fact(u3_mars* mar_u,
           u3_noun    job,
           u3_noun    pro)
{
  //  find all bob atoms in the committed event and increment their
  //  event-log refcount (eve_w on the u3a_blob).  Also bumps use_w
  //  so the blob survives the lifetime of the log entry.
  //
  {
    struct { c3_d* ids; c3_z len; c3_z cap; } acc = {0, 0, 0};
    u3a_walk_fore(job, &acc, _mars_blob_bobs_atom, _mars_blob_bobs_cell);

    for ( c3_z i_z = 0; i_z < acc.len; i_z++ ) {
      c3_h mug_h = (c3_h)(acc.ids[i_z] >> 32);
      c3_h seq_h = (c3_h)(acc.ids[i_z] & 0xFFFFFFFF);

      u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
      if ( blb_u ) {
        blb_u->eve_w += 1;
        blb_u->use_w += 1;
      }
      else {
        //  the blob's lease expired and the file was deleted before
        //  this event committed: the log now references a blob we
        //  cannot produce, and replay of this event will fail.
        //
        fprintf(stderr, "mars: commit (%" PRIu64 "): blob %08" PRIx32
                        "/%08" PRIx32 " missing from bank: lease expired "
                        "before commit; this event will not replay\r\n",
                        mar_u->dun_d, mug_h, seq_h);
      }
    }

    //  persist blob refs to LMDB for this event
    //
    if ( acc.len ) {
      u3_lmdb_save_blobs(mar_u->log_u->mdb_u,
                         mar_u->dun_d,
                         acc.ids,
                         acc.len);
    }

    c3_free(acc.ids);
  }

  {
    u3_fact tac_u = {
      .job   = job,
      .mug_h = mar_u->mug_h,
      .eve_d = mar_u->dun_d
    };

    u3_disk_plan(mar_u->log_u, &tac_u);
    u3z(job);
  }

  {
    u3_gift* gif_u = c3_malloc(sizeof(*gif_u));
    gif_u->nex_u = 0;
    gif_u->sat_e = u3_gift_fact_e;
    gif_u->eve_d = mar_u->dun_d;

    u3s_ram_xeno(pro, &gif_u->len_d, &gif_u->hun_y);
    u3z(pro);

    if ( !mar_u->gif_u.ent_u ) {
      u3_assert( !mar_u->gif_u.ext_u );
      mar_u->gif_u.ent_u = mar_u->gif_u.ext_u = gif_u;
    }
    else {
      mar_u->gif_u.ent_u->nex_u = gif_u;
      mar_u->gif_u.ent_u = gif_u;
    }
  }
}

/* _mars_gift(): enqueue response message.
*/
static void
_mars_gift(u3_mars* mar_u, u3_noun pro)
{
  u3_gift* gif_u = c3_malloc(sizeof(*gif_u));
  gif_u->nex_u = 0;
  gif_u->sat_e = u3_gift_rest_e;
  gif_u->ptr_v = 0;

  u3s_ram_xeno(pro, &gif_u->len_d, &gif_u->hun_y);
  u3z(pro);

  if ( !mar_u->gif_u.ent_u ) {
    u3_assert( !mar_u->gif_u.ext_u );
    mar_u->gif_u.ent_u = mar_u->gif_u.ext_u = gif_u;
  }
  else {
    mar_u->gif_u.ent_u->nex_u = gif_u;
    mar_u->gif_u.ent_u = gif_u;
  }
}

/* _mars_make_crud(): construct error-notification event.
*/
static u3_noun
_mars_make_crud(u3_noun job, u3_noun dud)
{
  u3_noun now, ovo, new;
  u3x_cell(job, &now, &ovo);

  new = u3nt(u3k(now),
             u3nt(u3_blip, c3__arvo, u3_nul),
             u3nt(c3__crud, dud, u3k(ovo)));

  u3z(job);
  return new;
}

/* _mars_curb(): check for memory threshold
*/
static inline c3_t
_mars_curb(c3_w pre_w, c3_w pos_w, c3_w hes_w)
{
  return (pre_w > hes_w) && (pos_w <= hes_w);
}

/* _mars_sure_feck(): event succeeded, send effects.
*/
static u3_noun
_mars_sure_feck(u3_mars* mar_u, c3_w pre_w, u3_noun vir)
{
  //  intercept |mass, observe |reset
  //
  {
    u3_noun riv = vir;
    c3_w    i_w = 0;

    while ( u3_nul != riv ) {
      u3_noun fec = u3t(u3h(riv));

      //  assumes a max of one %mass effect per event
      //
      if ( c3__mass == u3h(fec) ) {
        //  save a copy of the %mass data
        //
        mar_u->sac = u3k(u3t(fec));
        //  replace the %mass data with ~
        //
        //    For efficient transmission to daemon.
        //
        riv = u3kb_weld(u3qb_scag(i_w, vir),
                        u3nc(u3nt(u3k(u3h(u3h(riv))), c3__mass, u3_nul),
                             u3qb_slag(1 + i_w, vir)));
        u3z(vir);
        vir = riv;
        break;
      }

      //  reclaim memory from persistent caches on |reset
      //
      if ( c3__vega == u3h(fec) ) {
        mar_u->fag_w |= _mars_fag_vega;
      }

      riv = u3t(riv);
      i_w++;
    }
  }

  //  after a successful event, we check for memory pressure.
  //
  //    if we've exceeded either of two thresholds, we reclaim
  //    from our persistent caches, and notify the daemon
  //    (via a "fake" effect) that arvo should trim state
  //    (trusting that the daemon will enqueue an appropriate event).
  //    For future flexibility, the urgency of the notification is represented
  //    by a *decreasing* number: 0 is maximally urgent, 1 less so, &c.
  //
  //    high-priority: 2^22 contiguous words remaining (~8 MB)
  //    low-priority:  2^27 contiguous words remaining (~536 MB)
  //    XX maybe use 2^23 (~16 MB) and 2^26 (~268 MB?
  //
  //    XX these thresholds should trigger notifications sent to the king
  //    instead of directly triggering these remedial actions.
  //
  {
    u3_noun pri = u3_none;
    c3_w pos_w = u3a_open(u3R);

    //  if contiguous free space shrunk, check thresholds
    //  (and track state to avoid thrashing)
    //
    if ( pos_w < pre_w ) {
      if (  (_mars_mas_hit0 != mar_u->mas_w)
         && _mars_curb(pre_w, pos_w, 1 << 25) )
      {
        mar_u->mas_w  = _mars_mas_hit0;
        mar_u->fag_w |= _mars_fag_hit0;
        pri           = 0;
      }
      else if (  (_mars_mas_init == mar_u->mas_w)
              && _mars_curb(pre_w, pos_w, 1 << 27) )
      {
        mar_u->mas_w  = _mars_mas_hit1;
        mar_u->fag_w |= _mars_fag_hit1;
        pri         = 1;
      }
    }
    else if ( _mars_mas_init != mar_u->mas_w ) {
      if ( ((1 << 26) + (1 << 27)) < pos_w ) {
        mar_u->mas_w = _mars_mas_init;
      }
      else if (  (_mars_mas_hit0 == mar_u->mas_w)
              && ((1 << 26) < pos_w) )
      {
        mar_u->mas_w = _mars_mas_hit1;
      }
    }

    //  reclaim memory from persistent caches periodically
    //
    //    XX this is a hack to work two things
    //    - bytecode caches grow rapidly and can't be simply capped
    //    - we don't make very effective use of our free lists
    //
    if ( !(mar_u->dun_d % 1024ULL) ) {
      mar_u->fag_w |= _mars_fag_much;
    }

    //  notify daemon of memory pressure via "fake" effect
    //
    if ( u3_none != pri ) {
      u3_noun cad = u3nc(u3nt(u3_blip, c3__arvo, u3_nul),
                         u3nc(c3__trim, pri));
      vir = u3nc(cad, vir);
    }
  }

  return vir;
}

/* _mars_peek(): dereference namespace.
*/
static u3_noun
_mars_peek(c3_w mil_w, u3_noun sam)
{
  c3_t  tac_t = !!( u3C.wag_h & u3o_trace );
  c3_c  lab_c[2056];

  // XX refactor tracing
  //
  if ( tac_t ) {
    c3_c* foo_c = u3m_pretty(u3t(sam));

    {
      snprintf(lab_c, 2056, "peek %s", foo_c);
      c3_free(foo_c);
    }

    u3t_event_trace(lab_c, 'B');
  }

  u3_noun pro = u3v_soft_peek(mil_w, sam);

  if ( tac_t ) {
    u3t_event_trace(lab_c, 'E');
  }

  return pro;
}

/* _mars_poke(): attempt to compute an event. [*eve] is RETAINED.
*/
static c3_o
_mars_poke(c3_w mil_w, u3_noun* eve, u3_noun* out)
{
  c3_t tac_t = !!( u3C.wag_h & u3o_trace );
  c3_c tag_c[9];
  c3_o ret_o;

  // XX refactor tracing, avoid allocation
  //
  if ( tac_t ) {
    u3_noun wir = u3h(u3t(*eve));
    u3_noun tag = u3h(u3t(u3t(*eve)));
    c3_c* wir_c = u3m_pretty_path(wir);
    c3_w  len_w;

    u3r_bytes(0, 8, (c3_y*)tag_c, tag);
    tag_c[8] = 0;

    //  ellipses for trunctation
    //
    if ( sizeof(tac_c) <
         snprintf(tac_c, sizeof(tac_c), "poke %%%s on %s", tag_c, wir_c) )
    {
      memset(tac_c + (sizeof(tac_c) - 4), '.', 3);
      tac_c[255] = 0;
    }

    u3t_event_trace(tac_c, 'b');
    c3_free(wir_c);
  }

#ifdef U3_EVENT_TIME_DEBUG
  struct timeval b4;
  gettimeofday(&b4, 0);

  {
    u3_noun tag = u3h(u3t(u3t(*eve)));
    u3r_bytes(0, 8, (c3_y*)tag_c, tag);
    tag_c[8] = 0;

    if ( c3__belt != tag ) {
      u3l_log("mars: (%" PRIu64 ") %%%s\r\n", u3A->eve_d + 1, tag_c);
    }
  }
#endif

  {
    u3_noun pro;

    if ( c3y == (ret_o = u3v_poke_sure(mil_w, u3k(*eve), &pro)) ) {
      *out = pro;
    }
    else if ( c3__evil == u3h(pro) ) {
      *out = u3nc(pro, u3_nul);
    }
    else {
      u3_noun dud = pro;

      *eve = _mars_make_crud(*eve, u3k(dud));

      if ( c3y == (ret_o = u3v_poke_sure(mil_w, u3k(*eve), &pro)) ) {
        *out = pro;
        u3z(dud);
      }
      else {
        *out = u3nt(dud, pro, u3_nul);
      }
    }
  }

#ifdef U3_EVENT_TIME_DEBUG
  {
    c3_w      ms_w, clr_w;
    struct timeval f2, d0;
    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);

    ms_w  = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    clr_w = ms_w > 1000 ? 1 : ms_w < 100 ? 2 : 3; //  red, green, yellow

    if ( clr_w != 2 ) {
      u3l_log("\x1b[3%dm%%%s (%" PRIu64 ") %4d.%02dms\x1b[0m\n",
              clr_w, tag_c, u3A->eve_d + 1, ms_w,
              (int) (d0.tv_usec % 1000) / 10);
    }
  }
#endif

  if ( tac_t ) {
    u3t_event_trace(tac_c, 'e');
  }

  return ret_o;
}

/* _mars_work(): perform a task.
*/
static c3_o
_mars_work(u3_mars* mar_u, u3_noun jar)
{
  u3_noun tag, dat, pro;

  //  lease expiry sweeper: decay leases the king never released
  //  (crashed or leaking king, or counts left over from renewal).
  //  Uses a min-heap PQ keyed by exp_d: peek at the root, stop once
  //  the earliest-expiring lease is still in the future.
  //
  //  Released leases are marked ded_o=c3y by %blrl and left in the
  //  PQ; they are freed here when they bubble to the top.
  //
  {
    struct timeval tv_u;
    gettimeofday(&tv_u, 0);
    c3_d now_d = (c3_d)tv_u.tv_sec * 1000ULL + (c3_d)tv_u.tv_usec / 1000ULL;

    while ( 1 ) {
      _mars_lease* top_u = _mars_pq_peek(&_mars_pq);
      if ( !top_u ) break;

      //  dead lease (already released) — free and continue scanning
      //
      if ( c3y == top_u->ded_o ) {
        _mars_pq_pop(&_mars_pq);
        c3_free(top_u);
        continue;
      }

      //  earliest expiry is still in the future — stop
      //
      if ( !top_u->exp_d || now_d <= top_u->exp_d ) {
        break;
      }

      //  expired lease — decrement les_h and use_w, drop the durable
      //  row, check deletion
      //
      _mars_pq_pop(&_mars_pq);

      {
        u3a_blob* blb_u = u3a_blob_get(top_u->mug_h, top_u->seq_h);
        if ( blb_u && blb_u->les_h > 0 ) {
          blb_u->les_h -= 1;
          blb_u->use_w -= 1;
        }
      }

      {
        c3_d bid_d = ((c3_d)top_u->mug_h << 32) | (c3_d)top_u->seq_h;
        u3_lmdb_delete_lease(mar_u->log_u->mdb_u,
                             bid_d, top_u->exp_d, top_u->lea_d);
      }

      _blob_maybe_delete(top_u->mug_h, top_u->seq_h);

      c3_free(top_u);
    }
  }

  if ( c3n == u3r_cell(jar, &tag, &dat) ) {
    fprintf(stderr, "mars: fail a\r\n");
    u3z(jar);
    return c3n;
  }

  switch ( tag ) {
    default: {
      fprintf(stderr, "mars: fail b\r\n");
      u3z(jar);
      return c3n;
    }

    case c3__poke: {
      u3_noun tim, job;
      c3_h  mil_h;
      c3_w  pre_w;

      if ( (c3n == u3r_cell(dat, &tim, &job)) ||
           (c3n == u3r_safe_half(tim, &mil_h)) )
      {
        fprintf(stderr, "mars: poke fail\r\n");
        u3z(jar);
        return c3n;
      }

      //  XX better timestamps
      //
      {
        u3_noun now;
        struct timeval tim_u;
        gettimeofday(&tim_u, 0);

        now   = u3m_time_in_tv(&tim_u);
        job = u3nc(now, u3k(job));
      }
      u3z(jar);

      pre_w = u3a_open(u3R);
      mar_u->sen_d++;

      if ( c3y == _mars_poke(mil_h, &job, &pro) ) {
        mar_u->dun_d = mar_u->sen_d;
        mar_u->mug_h = u3r_mug(u3A->roc);
        mar_u->fag_w |= _mars_fag_mute;

        pro = _mars_sure_feck(mar_u, pre_w, pro);

        _mars_fact(mar_u, job, u3nt(c3__poke, c3y, pro));
      }
      else {
        mar_u->sen_d = mar_u->dun_d;
        u3z(job);
        _mars_gift(mar_u, u3nt(c3__poke, c3n, pro));
      }

      u3_assert( mar_u->dun_d == u3A->eve_d );
    } break;

    case c3__peek: {
      u3_noun tim, sam, pro;
      c3_h  mil_h;

      if ( (c3n == u3r_cell(dat, &tim, &sam)) ||
           (c3n == u3r_safe_half(tim, &mil_h)) )
      {
        u3z(jar);
        return c3n;
      }

      u3k(sam); u3z(jar);
      _mars_gift(mar_u, u3nc(c3__peek, _mars_peek(mil_h, sam)));
    } break;

    case c3__sync: {
      u3_noun nul;

      if (  (c3n == u3r_p(dat, c3__save, &nul))
         || (u3_nul != nul) )
      {
        u3z(jar);
        return c3n;
      }

      mar_u->sat_e = u3_mars_save_e;
    } break;

    //  $%  [%live ?(%meld %pack) ~] :: XX rename
    //
    case c3__live: {
      u3_noun com, arg;

      if ( (c3n == u3r_cell(dat, &com, &arg)) )
      {
        u3z(jar);
        return c3n;
      }

      switch ( com ) {
        default: {
          u3z(jar);
          return c3n;
        }

        case c3__pack: {
          u3z(jar);
          u3a_print_memory(stderr, "mars: pack: gained", u3m_pack());
        } break;

        case c3__meld: {
          c3_o per_o = c3n, for_o = c3n;
          u3_noun pers, ford;
          if ( c3y == u3r_cell(arg, &pers, &ford)
            && pers < 2
            && ford < 2) {
            per_o = pers;
            for_o = ford;
          }
          u3z(jar);
          u3a_print_memory(stderr, "mars: meld: gained",
            u3_meld_all(stderr, per_o, for_o));
        } break;
      }

      _mars_gift(mar_u, u3nc(c3__live, u3_nul));
    } break;

    //  $:  %quiz
    //      $%  [%quac ~]
    //          [%quic ~]
    //  ==  ==
    case c3__quiz: {
      switch ( u3h(dat) ) {
        case c3__quac: {
          u3z(jar);
          u3_noun res = u3_mars_grab(c3n);
          if ( u3_none == res ) {
            return c3n;
          } else {
            _mars_gift(mar_u, u3nt(c3__quiz, c3__quac, res));
          }
        } break;

        case c3__quic: {
          u3z(jar);
          c3_d pen_d = 4ULL * u3a_open(u3R);
          c3_d dil_d = 4ULL * u3a_idle(u3R);
          fprintf(stderr, "open: %" PRIu64 "\r\n", pen_d);
          fprintf(stderr, "idle: %" PRIu64 "\r\n", dil_d);

          _mars_gift(mar_u, u3nt(c3__quiz, c3__quic,
                                 u3nt(u3nc(c3__open, u3i_chub(pen_d)),
                                 u3nc(c3__idle, u3i_chub(dil_d)),
                                 u3_nul)));
        } break;

        default: {
          u3z(jar);
          return c3n;
        }
      }
    } break;

    case c3__exit: {
      u3z(jar);
      mar_u->sat_e = u3_mars_exit_e;
    } break;

    case c3__blob: {
      //  [%blob path-atom]  — install staging file from king
      //
      c3_h mug_h = 0;
      c3_h seq_h = 0;
      c3_o ok_o  = c3n;

      //  extract path string from atom
      //
      c3_d len_d = u3r_met(3, dat);
      if ( len_d > 0 && len_d < 8192 ) {
        c3_c stg_c[8192] = {0};
        u3r_bytes(0, (c3_w)len_d, (c3_y*)stg_c, dat);

        ok_o = u3_blob_move_stg(u3C.dir_c, stg_c, &mug_h, &seq_h);

        if ( c3y == ok_o ) {
          //  issue the king's install lease: protects the blob until
          //  the event referencing it commits (eve_w takes over).  the
          //  lease is committed to LMDB here, before the ack below, so
          //  the king never believes it holds a lease mars can lose.
          //
          ok_o = _mars_lease_take(mar_u->log_u->mdb_u, mug_h, seq_h, c3y);

          if ( c3y == ok_o ) {
            //  save blob bank to snapshot so entries survive crash
            //
            mar_u->fag_w |= _mars_fag_mute;
          }
        }
      }
      else {
        fprintf(stderr, "mars: blob: bad path atom len %" PRIu64 "\r\n", len_d);
      }

      u3z(jar);

      if ( c3y == ok_o ) {
        _mars_gift(mar_u, u3nt(c3__blob, c3y,
                               u3nc(u3i_word(mug_h), u3i_word(seq_h))));
      }
      else {
        _mars_gift(mar_u, u3nc(c3__blob, c3n));
      }
    } break;

    //  %blas: king acquires/renews a lease on a blob.  sent by the
    //  king's renewal timer for every blob it still references, so a
    //  pending event can't outlive the 15-min TTL.
    //
    case c3_s4('b','l','a','s'): {
      u3_noun mug_n, seq_n;
      if ( c3n == u3r_cell(dat, &mug_n, &seq_n) ) {
        u3z(jar);
        return c3n;
      }
      c3_h mug_h = 0;
      c3_h seq_h = 0;
      u3r_safe_half(mug_n, &mug_h);
      u3r_safe_half(seq_n, &seq_h);

      //  renewal: another durable lease unit, ignoring commit failure
      //  (the king will renew again before the prior lease's TTL)
      //
      _mars_lease_take(mar_u->log_u->mdb_u, mug_h, seq_h, c3n);

      u3z(jar);
    } break;

    //  %blrl: king releases a lease on a blob
    //
    case c3_s4('b','l','r','l'): {
      u3_noun mug_n, seq_n;
      if ( c3n == u3r_cell(dat, &mug_n, &seq_n) ) {
        u3z(jar);
        return c3n;
      }
      c3_h mug_h = 0;
      c3_h seq_h = 0;
      u3r_safe_half(mug_n, &mug_h);
      u3r_safe_half(seq_n, &seq_h);

      //  retire one live lease unit: mark its PQ entry dead (the
      //  sweeper frees it when it surfaces), decrement, and drop its
      //  durable row.  no live entry means the unit already expired —
      //  a no-op, never a double decrement.
      //
      {
        _mars_lease* lea_u = _mars_pq_kill_one(&_mars_pq, mug_h, seq_h);
        if ( lea_u ) {
          u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
          if ( blb_u && blb_u->les_h > 0 ) {
            blb_u->les_h -= 1;
            blb_u->use_w -= 1;
          }

          c3_d bid_d = ((c3_d)mug_h << 32) | (c3_d)seq_h;
          u3_lmdb_delete_lease(mar_u->log_u->mdb_u,
                               bid_d, lea_u->exp_d, lea_u->lea_d);

          _blob_maybe_delete(mug_h, seq_h);
        }
      }

      u3z(jar);
    } break;
  }

  return c3y;
}

/* _mars_post(): update mars state post-task.
*/
void
_mars_post(u3_mars* mar_u)
{
  if ( mar_u->fag_w & _mars_fag_hit1 ) {
    if ( u3C.wag_h & u3o_verbose ) {
      u3l_log("mars: threshold 1: %"PRIc3_w, u3h_wyt(u3R->cax.per_p));
      u3l_log("mars: threshold 1: %"PRIc3_w, u3h_wyt(u3R->cax.for_p));
    }
    u3h_trim_to(u3R->cax.per_p, u3h_wyt(u3R->cax.per_p) / 2);
    u3h_trim_to(u3R->cax.for_p, u3h_wyt(u3R->cax.for_p) / 2);
    u3m_reclaim();
  }

  if ( mar_u->fag_w & _mars_fag_much ) {
    u3m_reclaim();
  }

  if ( mar_u->fag_w & _mars_fag_vega ) {
    u3h_trim_to(u3R->cax.per_p, u3h_wyt(u3R->cax.per_p) / 2);
    u3m_reclaim();
  }

  //  XX this runs on replay too, |mass s/b elsewhere
  //
  if ( mar_u->fag_w & _mars_fag_mute ) {
    u3z(_mars_grab(mar_u->sac, c3y));
    mar_u->sac   = u3_nul;
  }

  if ( mar_u->fag_w & _mars_fag_hit0 ) {
    if ( u3C.wag_h & u3o_verbose ) {
      u3l_log("mars: threshold 0: per_p %"PRIc3_w, u3h_wyt(u3R->cax.per_p));
      u3l_log("mars: threshold 0: for_p %"PRIc3_w, u3h_wyt(u3R->cax.for_p));
    }
    u3h_free(u3R->cax.per_p);
    u3R->cax.per_p = u3h_new_cache(u3C.per_w);
    u3h_free(u3R->cax.for_p);
    u3R->cax.for_p = u3h_new_cache(u3C.per_w);
    u3a_print_memory(stderr, "mars: pack: gained", u3m_pack());
    u3l_log("");
  }

  if ( u3C.wag_h & u3o_toss ) {
    u3m_toss();
  }

  mar_u->fag_w = _mars_fag_none;
}

/* _mars_damp_file(): write sampling-profiler output.
*/
static void
_mars_damp_file(void)
{
  if ( u3C.wag_h & u3o_debug_cpu ) {
    FILE* fil_u;
    u3_noun now;

    {
      struct timeval tim_u;
      gettimeofday(&tim_u, 0);
      now = u3m_time_in_tv(&tim_u);
    }

    {
      u3_noun wen = u3dc("scot", c3__da, now);
      c3_c* wen_c = u3r_string(wen);

      c3_c nam_c[2048];
      snprintf(nam_c, 2048, "%s/.urb/put/profile", u3C.dir_c);

      struct stat st;
      if ( -1 == stat(nam_c, &st) ) {
        c3_mkdir(nam_c, 0700);
      }

      c3_c man_c[2054];
      snprintf(man_c, 2053, "%s/%s.txt", nam_c, wen_c);

      fil_u = c3_fopen(man_c, "w");

      c3_free(wen_c);
      u3z(wen);
    }

    u3t_damp(fil_u);

    {
      fclose(fil_u);
    }
  }
}

/* _mars_flush(): send pending gifts.
*/
static void
_mars_flush(u3_mars* mar_u)
{
top:
  {
    u3_gift* gif_u = mar_u->gif_u.ext_u;

    //  XX gather i/o
    //
    while (  gif_u
          && (  (u3_gift_rest_e == gif_u->sat_e)
             || (gif_u->eve_d <= mar_u->log_u->dun_d)) )
    {
      u3_newt_send_vers(mar_u->out_u, 0x01, gif_u->len_d, gif_u->hun_y);

      mar_u->gif_u.ext_u = gif_u->nex_u;
      c3_free(gif_u);
      gif_u = mar_u->gif_u.ext_u;
    }

    if ( !mar_u->gif_u.ext_u ) {
      mar_u->gif_u.ent_u = 0;
    }
  }

  if (  (u3_mars_work_e != mar_u->sat_e)
     && (mar_u->log_u->dun_d == mar_u->dun_d) )
  {
    if ( u3_mars_save_e == mar_u->sat_e ) {
      u3m_save();
      mar_u->sav_u.eve_d = mar_u->dun_d;
      _mars_gift(mar_u,
        u3nt(c3__sync, u3i_chub(mar_u->dun_d), mar_u->mug_h));
      mar_u->sat_e = u3_mars_work_e;
      goto top;
    }
    else if ( u3_mars_exit_e == mar_u->sat_e ) {
      u3m_save();
      u3_disk_exit(mar_u->log_u);
      u3s_cue_xeno_done(mar_u->sil_u);
      u3t_trace_close();
      _mars_damp_file();

      //  XX dispose [mar_u], exit cb ?
      //
      u3m_stop();
      exit(0);
    }
  }
}

/* _mars_step_trace(): initialize or rotate trace file.
*/
static void
_mars_step_trace(const c3_c* dir_c)
{
  if ( u3C.wag_h & u3o_trace ) {
    c3_w trace_cnt_w = u3t_trace_cnt();
    if ( trace_cnt_w == 0  && u3t_file_cnt() == 0 ) {
      u3t_trace_open(dir_c);
    }
    else if ( trace_cnt_w >= 100000 ) {
      u3t_trace_close();
      u3t_trace_open(dir_c);
    }
  }
}

/* u3_mars_kick(): maybe perform a task.
*/
c3_o
u3_mars_kick(void* ram_u, c3_y ver_y, c3_d len_d, c3_y* hun_y)
{
  u3_mars* mar_u = ram_u;
  c3_o ret_o = c3n;

  _mars_step_trace(mar_u->dir_c);

  //  XX optimize for stateless tasks w/ peek-next
  //
  if ( u3_mars_work_e == mar_u->sat_e ) {
    u3_weak jar = ( 0x01 == ver_y )
                ? u3s_tap_xeno(len_d, hun_y)
                : u3s_cue_xeno_with(mar_u->sil_u, len_d, hun_y);

    if (  (u3_none == jar)
       || (c3n == _mars_work(mar_u, jar)) )
    {
      fprintf(stderr, "mars: bad\r\n");
      exit(1);
    }

    _mars_post(mar_u);
    ret_o = c3y;
  }

  _mars_flush(mar_u);

  return ret_o;
}

/* _mars_timer_cb(): mars timer callback.
*/
static void
_mars_timer_cb(uv_timer_t* tim_u)
{
  u3_mars* mar_u = tim_u->data;

  if ( mar_u->dun_d > mar_u->sav_u.eve_d ) {
    mar_u->sat_e = u3_mars_save_e;
  }

  _mars_flush(mar_u);
}

/* _mars_disk_cb(): mars commit result callback.
*/
static void
_mars_disk_cb(void* ptr_v, c3_d eve_d, c3_o ret_o)
{
  u3_mars* mar_u = ptr_v;

  if ( c3n == ret_o ) {
    //  XX better
    //
    fprintf(stderr, "mars: commit fail\r\n");
    exit(1);
  }

  _mars_flush(mar_u);
}

/* _mars_poke_play(): replay an event.
*/
static u3_weak
_mars_poke_play(u3_mars* mar_u, const u3_fact* tac_u)
{
  u3_noun gon = u3m_soft(0, u3v_poke, tac_u->job);
  u3_noun tag, dat;
  u3x_cell(gon, &tag, &dat);

  //  event failed, produce trace
  //
  if ( u3_blip != tag ) {
    return gon;
  }

  //  event succeeded, check mug
  //
  {
    u3_noun cor = u3t(dat);
    c3_h  mug_h;

    if ( tac_u->mug_h && (tac_u->mug_h != (mug_h = u3r_mug(cor))) ) {
      fprintf(stderr, "play (%" PRIc3_d "): mug mismatch "
                      "expected %08u, actual %08u\r\n",
                      tac_u->eve_d, tac_u->mug_h, mug_h);

      if ( !(u3C.wag_h & u3o_soft_mugs) ) {
        u3z(gon);
        return u3nc(c3__awry, u3_nul);
      }
    }

    u3z(u3A->roc);
    u3A->roc = u3k(cor);
    u3A->eve_d++;
  }

  u3z(gon);
  return u3_none;
}

/* _mars_show_time(): print date, truncated to seconds.
*/
static u3_noun
_mars_show_time(u3_noun wen)
{
  return u3dc("scot", c3__da, u3kc_lsh(6, 1, u3kc_rsh(6, 1, wen)));
}

typedef enum {
  _play_yes_e,  //  success
  _play_mem_e,  //  %meme
  _play_int_e,  //  %intr
  _play_log_e,  //  event log fail
  _play_mug_e,  //  mug mismatch
  _play_bad_e   //  total failure
} _mars_play_e;

/* _mars_play_blobs_cb(): u3_lmdb_walk_blobs callback — increment eve_w
** (and use_w) for each blob ref in a replayed event.
**
**   a blob whose file is missing from the store is unreproducible
**   data the log still depends on: report it (all of them — keep
**   scanning) and flag the replay as failed via [ptr_v].
*/
static void
_mars_play_blobs_cb(void* ptr_v, c3_d eve_d, c3_d* ids_d, c3_z len_z)
{
  c3_o* oky_o = ptr_v;

  for ( c3_z i_z = 0; i_z < len_z; i_z++ ) {
    c3_h mug_h = (c3_h)(ids_d[i_z] >> 32);
    c3_h seq_h = (c3_h)(ids_d[i_z] & 0xFFFFFFFFULL);

    if ( c3n == u3_blob_live(u3C.dir_c, mug_h, seq_h) ) {
      fprintf(stderr, "play (%" PRIu64 "): blob %08" PRIx32 "/%08" PRIx32
                      " missing from store: the log references data "
                      "that no longer exists\r\n",
                      eve_d, mug_h, seq_h);
      *oky_o = c3n;
      continue;
    }

    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    if ( !blb_u ) blb_u = u3a_blob_new(mug_h, seq_h);
    blb_u->eve_w += 1;
    blb_u->use_w += 1;
  }
}

/* _mars_play_blobs(): rebuild blob event-log refcounts for replayed
** events in [fir_d, las_d].  snapshot has counts correct up to snapshot
** time; replay covers the gap from snapshot to head.
**
**   returns c3n if any referenced blob file is missing — replay of
**   those events cannot reproduce their state, so the caller must
**   fail hard rather than continue with silent corruption.
**
**   NB: opens its own read txn, so it must run after the batch's disk
**   walk is done — a nested read txn on the same thread fails with
**   MDB_BAD_RSLOT.
*/
static c3_o
_mars_play_blobs(u3_mars* mar_u, c3_d fir_d, c3_d las_d)
{
  c3_o oky_o = c3y;

  if ( las_d < fir_d ) return c3y;

  u3_lmdb_walk_blobs(mar_u->log_u->mdb_u, fir_d, las_d,
                     &oky_o, _mars_play_blobs_cb);
  return oky_o;
}

/* _mars_lease_row: one LEASES row staged for post-walk deletion.
*/
typedef struct {
  c3_d  bid_d;
  c3_d  exp_d;
  c3_d  lea_d;
} _mars_lease_row;

/* _mars_lease_play: accumulator for _mars_play_leases.
*/
typedef struct {
  c3_d             now_d;   //  wall-clock now (ms)
  c3_d             max_d;   //  highest lea id seen (any row)
  _mars_lease_row* row_u;   //  rows to delete after the walk closes
  c3_z             len_z;
  c3_z             cap_z;
} _mars_lease_play;

/* _mars_play_leases_cb(): u3_lmdb_walk_leases callback — restore one
**   durable lease into les_h + the PQ, or stage it for deletion if it
**   has expired or its file is gone.
**
**   the read txn is open during the walk, so deletions are deferred
**   (collected here, applied after the walk) to avoid MDB_BAD_RSLOT.
*/
static void
_mars_play_leases_cb(void* ptr_v, c3_d bid_d, c3_d exp_d, c3_d lea_d)
{
  _mars_lease_play* pla_u = ptr_v;
  c3_h mug_h = (c3_h)(bid_d >> 32);
  c3_h seq_h = (c3_h)(bid_d & 0xFFFFFFFF);

  //  seed the id source above every row that ever existed, so a fresh
  //  lease never collides with one still pending deletion
  //
  if ( lea_d > pla_u->max_d ) pla_u->max_d = lea_d;

  //  expired, or the file no longer exists — stage the row for deletion
  //  and do not restore (a phantom entry would point at a missing file)
  //
  if (  (exp_d && (pla_u->now_d > exp_d))
     || (c3n == u3_blob_live(u3C.dir_c, mug_h, seq_h)) )
  {
    if ( pla_u->len_z == pla_u->cap_z ) {
      pla_u->cap_z = pla_u->cap_z ? (pla_u->cap_z << 1) : 8;
      pla_u->row_u = c3_realloc(pla_u->row_u,
                                pla_u->cap_z * sizeof(*pla_u->row_u));
    }
    pla_u->row_u[pla_u->len_z].bid_d = bid_d;
    pla_u->row_u[pla_u->len_z].exp_d = exp_d;
    pla_u->row_u[pla_u->len_z].lea_d = lea_d;
    pla_u->len_z += 1;
    return;
  }

  //  live lease — restore one les_h unit and re-arm the TTL with the
  //  persisted deadline
  //
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  if ( !blb_u ) blb_u = u3a_blob_new(mug_h, seq_h);
  blb_u->les_h += 1;
  blb_u->use_w += 1;

  {
    _mars_lease* lea_u = c3_malloc(sizeof(*lea_u));
    lea_u->mug_h = mug_h;
    lea_u->seq_h = seq_h;
    lea_u->exp_d = exp_d;
    lea_u->lea_d = lea_d;
    lea_u->ded_o = c3n;
    _mars_pq_push(&_mars_pq, lea_u);
  }
}

/* _mars_play_leases(): rebuild les_h from the LEASES table at boot.
**
**   _find_home already zeroed les_h; this restores it durably so a
**   blob the king still holds (but mars has not yet committed an event
**   for) survives a mars restart with its remaining TTL intact.  must
**   run after the disk is open and after eve_w is rebuilt by replay.
*/
static void
_mars_play_leases(u3_mars* mar_u)
{
  _mars_lease_play pla_u = { 0, 0, 0, 0, 0 };
  {
    struct timeval tv_u;
    gettimeofday(&tv_u, 0);
    pla_u.now_d = (c3_d)tv_u.tv_sec * 1000ULL + (c3_d)tv_u.tv_usec / 1000ULL;
  }

  u3_lmdb_walk_leases(mar_u->log_u->mdb_u, &pla_u, _mars_play_leases_cb);

  //  apply deferred deletions now that the read txn has closed
  //
  for ( c3_z i_z = 0; i_z < pla_u.len_z; i_z++ ) {
    u3_lmdb_delete_lease(mar_u->log_u->mdb_u,
                         pla_u.row_u[i_z].bid_d,
                         pla_u.row_u[i_z].exp_d,
                         pla_u.row_u[i_z].lea_d);
  }

  if ( _mars_lea_d < pla_u.max_d ) _mars_lea_d = pla_u.max_d;

  c3_free(pla_u.row_u);
}

/* _mars_play_batch(): replay a batch of events, return status and batch date.
*/
static _mars_play_e
_mars_play_batch(u3_mars* mar_u,
                 c3_o     mug_o,
                 c3_h     bat_h,
                 c3_c**   wen_c)
{
  u3_disk*      log_u = mar_u->log_u;
  c3_d          fir_d = mar_u->dun_d + 1;
  u3_disk_walk* wok_u = u3_disk_walk_init(log_u, fir_d, bat_h);
  u3_fact       tac_u;
  u3_noun         dud;
  u3_weak         wen = u3_none;

  if ( !wok_u ) {
    fprintf(stderr, "play: failed to open event log iterator\r\n");
    return _play_log_e;
  }

  while ( c3y == u3_disk_walk_live(wok_u) ) {
    if ( c3n == u3_disk_walk_step(wok_u, &tac_u) ) {
      u3_disk_walk_done(wok_u);
      (void)_mars_play_blobs(mar_u, fir_d, mar_u->dun_d);
      return _play_log_e;
    }

    u3_assert( ++mar_u->sen_d == tac_u.eve_d );

    if ( u3_none == wen ) {
      wen = _mars_show_time(u3k(u3h(tac_u.job)));
    }

    if ( u3_none != (dud = _mars_poke_play(mar_u, &tac_u)) ) {
      c3_m mot_m;

      mar_u->sen_d = mar_u->dun_d;
      u3_disk_walk_done(wok_u);
      (void)_mars_play_blobs(mar_u, fir_d, mar_u->dun_d);

      u3_assert( c3y == u3r_safe_half(u3h(dud), &mot_m) );

      switch ( mot_m ) {
        case c3__meme: {
          fprintf(stderr, "play (%" PRIu64 "): %%meme\r\n", tac_u.eve_d);
          u3z(dud); u3z(wen);
          return _play_mem_e;
        }

        case c3__intr: {
          fprintf(stderr, "play (%" PRIu64 "): %%intr\r\n", tac_u.eve_d);
          u3z(dud); u3z(wen);
          return _play_int_e;
        }

        case c3__awry: {
          fprintf(stderr, "play (%" PRIu64 "): %%awry\r\n", tac_u.eve_d);
          u3z(dud); u3z(wen);
          return _play_mug_e;
        }

        default: {
          fprintf(stderr, "play (%" PRIu64 "): failed\r\n", tac_u.eve_d);
          u3_pier_punt_goof("play", dud);
          u3z(wen);
          //  XX say something uplifting
          //
          return _play_bad_e;
        }
      }
    }

    mar_u->dun_d = mar_u->sen_d;
  }

  u3_disk_walk_done(wok_u);

  //  missing blob files make the replayed range unreproducible:
  //  fail hard rather than continue with silent corruption
  //
  if ( c3n == _mars_play_blobs(mar_u, fir_d, mar_u->dun_d) ) {
    u3z(wen);
    return _play_bad_e;
  }

  *wen_c = u3r_string(wen);
  u3z(wen);
  return _play_yes_e;
}

static c3_o
_mars_do_boot(u3_disk* log_u, c3_d eve_d, u3_noun cax)
{
  u3_weak eve;
  c3_h  mug_h;

  //  hack to recover structural sharing
  //
  u3m_hate(1 << 18);

  //  XX this function should only ever be called in epoch 0
  //  XX read_list reads *up-to* eve_d, should be exact
  //
  if ( u3_none == (eve = u3_disk_read_list(log_u, 1, eve_d, &mug_h)) ) {
    fprintf(stderr, "boot: read failed\r\n");
    u3m_love(u3_nul);
    return c3n;
  }

  //  hack to recover structural sharing
  //
  u3_noun xev = u3m_love(u3ke_cue(u3ke_jam(u3nc(cax, eve))));
  u3z(cax);
  u3x_cell(xev, &cax, &eve);
  u3k(eve); u3k(cax);
  u3z(xev);
  xev = cax;

  //  prime memo cache
  //
  while ( u3_nul != cax ) {
    u3z_save_m(u3z_memo_keep, 144 + c3__nock, u3h(u3h(cax)),
               u3t(u3h(cax)));
    cax = u3t(cax);
  }
  u3z(xev);

  //  install an ivory pill to support stack traces
  //
  //    XX support -J
  //
  // {
  //   c3_d  len_d = u3_Ivory_pill_len;
  //   c3_y* byt_y = u3_Ivory_pill;
  //   u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  //   u3_weak pil;

  //   if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
  //     u3l_log("lite: unable to cue ivory pill");
  //     exit(1);
  //   }

  //   u3s_cue_xeno_done(sil_u);

  //   if ( c3n == u3v_boot_lite(pil)) {
  //     u3l_log("lite: boot failed");
  //     exit(1);
  //   }
  // }

  u3l_log("--------------- bootstrap starting ----------------");

  u3l_log("boot: 1-%"PRIc3_w, u3qb_lent(eve));

  //  XX check mug if available
  //
  if ( c3n == u3v_boot(eve) ) {
    return c3n;
  }

  u3l_log("--------------- bootstrap complete ----------------");
  return c3y;
}

/* _mars_sign_init(): initialize daemon signal handlers.
*/
static void
_mars_sign_init(u3_mars* mar_u)
{
  //  handle SIGINFO (if available)
  //
#ifdef SIGINFO
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->sil_u.data = mar_u;

    sig_u->num_i = SIGINFO;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }
#endif

  //  handle SIGUSR1 (fallback for SIGINFO)
  //
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->sil_u.data = mar_u;

    sig_u->num_i = SIGUSR1;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }
}

/* _mars_sign_cb: signal callback.
*/
static void
_mars_sign_cb(uv_signal_t* sil_u, c3_i num_i)
{
  u3_mars* mar_u = sil_u->data;

  switch ( num_i ) {
    default: {
      u3l_log("\r\nmars: mysterious signal %d\r\n", num_i);
    } break;

    //  fallthru if defined
    //
#ifdef SIGINFO
    case SIGINFO:
#endif
    case SIGUSR1: {
      //  XX add u3_mars_slog()
      //
      u3_disk_slog(mar_u->log_u);
    } break;
  }
}

/* _mars_sign_move(): enable daemon signal handlers
*/
static void
_mars_sign_move(void)
{
  u3_usig* sig_u;

  for ( sig_u = u3_Host.sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_signal_start(&sig_u->sil_u, _mars_sign_cb, sig_u->num_i);
  }
}

/* _mars_sign_hold(): disable daemon signal handlers
*/
static void
_mars_sign_hold(void)
{
  u3_usig* sig_u;

  for ( sig_u = u3_Host.sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_signal_stop(&sig_u->sil_u);
  }
}

/* _mars_sign_close(): dispose daemon signal handlers
*/
static void
_mars_sign_close(void)
{
  u3_usig* sig_u;

  for ( sig_u = u3_Host.sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_close((uv_handle_t*)&sig_u->sil_u, (uv_close_cb)free);
  }
}

/* u3_mars_play(): replay up to [eve_d], snapshot every [sap_d].
*/
c3_d
u3_mars_play(u3_mars* mar_u, c3_d eve_d, c3_d sap_d)
{
  u3_disk* log_u = mar_u->log_u;
  c3_d     pay_d = 0;

  if ( !eve_d ) {
    eve_d = log_u->dun_d;
  }
  else if ( eve_d <= mar_u->dun_d ) {
    u3l_log("mars: already computed %" PRIu64 "", eve_d);
    u3l_log("      state=%" PRIu64 ", log=%" PRIu64 "",
            mar_u->dun_d, log_u->dun_d);
    return pay_d;
  }
  else {
    eve_d = c3_min(eve_d, log_u->dun_d);
  }

  if ( mar_u->dun_d == log_u->dun_d ) {
    return pay_d;
  }

  pay_d = eve_d - mar_u->dun_d;

  if ( !mar_u->dun_d ) {
    u3_meta met_u;

    if ( c3n == u3_disk_read_meta(log_u->mdb_u, &met_u) ) {
      fprintf(stderr, "mars: disk read meta fail\r\n");
      //  XX exit code, cb
      //
      exit(1);
    }

    if ( c3n == _mars_do_boot(mar_u->log_u, met_u.lif_h, u3_nul) ) {
      fprintf(stderr, "mars: boot fail\r\n");
      //  XX exit code, cb
      //
      exit(1);
    }

    mar_u->sen_d = mar_u->dun_d = met_u.lif_h;
    u3m_save();
  }

  u3l_log("---------------- playback starting ----------------");

  if ( (1ULL + eve_d) == log_u->dun_d ) {
    u3l_log("play: event %" PRIu64 "", log_u->dun_d);
  }
  else if ( eve_d != log_u->dun_d ) {
    u3l_log("play: events %" PRIu64 "-%" PRIu64 " of %" PRIu64 "",
            (c3_d)(1ULL + mar_u->dun_d),
            eve_d,
            log_u->dun_d);
  }
  else {
    u3l_log("play: events %" PRIu64 "-%" PRIu64 "",
            (c3_d)(1ULL + mar_u->dun_d),
            eve_d);
  }

  {
    c3_d  pas_d = mar_u->dun_d;  // last snapshot
    c3_d  mem_d = 0;             // last event to meme
    c3_h  try_h = 0;             // [mem_d] retry count
    c3_c* wen_c;

    while ( mar_u->dun_d < eve_d ) {
      _mars_step_trace(mar_u->dir_c);

      //  XX get batch from args
      //
      switch ( _mars_play_batch(mar_u, c3y, 1024, &wen_c) ) {
        case _play_yes_e: {
          c3_c* now_c;

          {
            u3_noun          now;
            struct timeval tim_u;
            gettimeofday(&tim_u, 0);

            now   = _mars_show_time(u3m_time_in_tv(&tim_u));
            now_c = u3r_string(now);
            u3z(now);
          }

          u3m_reclaim();

          if ( sap_d && ((mar_u->dun_d - pas_d) >= sap_d) ) {
            u3m_save();
            pas_d = mar_u->dun_d;
            u3l_log("play (%" PRIu64 "): save (%s, now=%s)",
                    mar_u->dun_d, wen_c, now_c);
          }
          else {
            u3l_log("play (%" PRIu64 "): done (%s, now=%s)",
                    mar_u->dun_d, wen_c, now_c);
          }

          c3_free(now_c);
          c3_free(wen_c);
        } break;

        case _play_mem_e: {
          if ( mem_d != mar_u->dun_d ) {
            mem_d = mar_u->dun_d;
            try_h = 0;
          }
          else if ( 3 == ++try_h ) {
            fprintf(stderr, "play (%" PRIu64 "): failed, out of loom\r\n",
                            mar_u->dun_d + 1);
            u3m_save();
            //  XX check loom size, suggest --loom X
            //  XX exit code, cb
            //
            u3_disk_exit(log_u);
            exit(1);
          }

          //  XX pack before meld?
          //
          if ( u3C.wag_h & u3o_auto_meld ) {
            u3a_print_memory(stderr, "mars: meld: gained", u3_meld_all(stderr, c3y, c3n));
          }
          else {
            u3a_print_memory(stderr, "mars: pack: gained", u3m_pack());
          }
        } break;

        case _play_int_e: {
          fprintf(stderr, "play (%" PRIu64 "): interrupted\r\n", mar_u->dun_d + 1);
          u3m_save();
          //  XX exit code, cb
          //
          u3_disk_exit(log_u);
          exit(1);
        } break;

        //  XX handle any specifically?
        //
        case _play_log_e:
        case _play_mug_e:
        case _play_bad_e: {
          fprintf(stderr, "play (%" PRIu64 "): failed\r\n", mar_u->dun_d + 1);
          u3m_save();
          //  XX exit code, cb
          //
          u3_disk_exit(log_u);
          exit(1);
        } break;
      }
    }
  }

  u3l_log("---------------- playback complete ----------------");
  u3m_save();

  if (  (mar_u->dun_d == log_u->dun_d)
     && !log_u->epo_d
     && !(u3C.wag_h & u3o_yolo) )
  {
    u3_disk_roll(mar_u->log_u, mar_u->dun_d);
  }

  return pay_d;
}

/* u3_mars_load(): load pier.
*/
void
u3_mars_load(u3_mars* mar_u, u3_disk_load_e lod_e)
{
  //  initialize persistence
  //
  if ( !(mar_u->log_u = u3_disk_load(mar_u->dir_c, lod_e)) ) {
    fprintf(stderr, "mars: disk init fail\r\n");
    exit(1); // XX
  }

  mar_u->sen_d = mar_u->dun_d = u3A->eve_d;
  mar_u->mug_h = u3r_mug(u3A->roc);

  if ( c3n == u3_disk_read_meta(mar_u->log_u->mdb_u, &(mar_u->met_u)) ) {
    fprintf(stderr, "mars: disk meta fail\r\n");
    u3_disk_exit(mar_u->log_u);
    exit(1); // XX
  }
}

/* u3_mars_work(): init mars
*/
void
u3_mars_work(u3_mars* mar_u)
{
  mar_u->sil_u = u3s_cue_xeno_init();

  //  start signal handlers
  //
  _mars_sign_init(mar_u);
  _mars_sign_move();

  //  Initalize the spin stack
  u3t_sstack_init(mar_u->met_u.who_d);

  //  wire up signal controls
  //
  u3C.sign_hold_f = _mars_sign_hold;
  u3C.sign_move_f = _mars_sign_move;

  //  wire up blob delete callback (pkg/noun can't link pkg/vere)
  //
  u3C.blob_del_f = _mars_blob_del;

  //  XX do something better
  //
  if ( mar_u->log_u->dun_d > mar_u->dun_d ) {
    u3_disk_exit(mar_u->log_u);
    exit(0);
  }

  //  restore durable king leases (replay has rebuilt eve_w; _find_home
  //  zeroed les_h), then reclaim any blob now provably unreferenced.
  //  ordering matters: les_h must be back before the gc reads use_w.
  //
  _mars_play_leases(mar_u);
  u3_disk_blob_gc(mar_u->log_u);

  //  send ready status message
  //
  //    XX version negotiation
  //
  {
    c3_d  len_d;
    c3_y* hun_y;
    u3_noun wyn = u3_nul;
    u3_noun msg = u3nq(c3__ripe,
                       u3nc(2, wyn),
                       u3nc(u3i_chubs(2, mar_u->met_u.who_d),
                            mar_u->met_u.fak_o),
                       u3nc(u3i_chub(mar_u->dun_d),
                            mar_u->mug_h));

    u3s_ram_xeno(msg, &len_d, &hun_y);
    u3_newt_send_vers(mar_u->out_u, 0x01, len_d, hun_y);
    u3z(msg);
  }

  u3_disk_async(mar_u->log_u, mar_u, _mars_disk_cb);

  uv_timer_init(u3L, &(mar_u->sav_u.tim_u));
  uv_timer_start(&(mar_u->sav_u.tim_u),
                 _mars_timer_cb,
                 u3_Host.ops_u.sap_h * 1000,
                 u3_Host.ops_u.sap_h * 1000);

  mar_u->sav_u.eve_d = mar_u->dun_d;
  mar_u->sav_u.tim_u.data = mar_u;
}

#define VERE_NAME  "vere"
#define VERE_ZUSE  408
#define VERE_LULL  320
#define VERE_ARVO  234
#define VERE_HOON  135
#define VERE_NOCK  4

/* _mars_wyrd_card(): construct %wyrd.
*/
static u3_noun
_mars_wyrd_card(c3_m nam_m, c3_h ver_h, c3_l sev_l)
{
  //  XX ghetto (scot %ta)
  //
  u3_noun ver = u3nq(c3__vere, u3i_string(U3_VERE_PACE), u3i_string("~." URBIT_VERSION), u3_nul);
  u3_noun sen = u3i_string("0v1s.vu178");
  u3_noun kel;

  //  special case versions requiring the full stack
  //
  if (  ((c3__zuse == nam_m) && (VERE_ZUSE == ver_h))
     || ((c3__lull == nam_m) && (VERE_LULL == ver_h))
     || ((c3__arvo == nam_m) && (VERE_ARVO == ver_h)) )
  {
    kel = u3nl(u3nc(c3__zuse, VERE_ZUSE),
               u3nc(c3__lull, VERE_LULL),
               u3nc(c3__arvo, VERE_ARVO),
               u3nc(c3__hoon, VERE_HOON),
               u3nc(c3__nock, VERE_NOCK));
  }
  //  XX speculative!
  //
  else {
    kel = u3nc(nam_m, u3i_half(ver_h));
  }

  return u3nt(c3__wyrd, u3nc(sen, ver), kel);
}

/* _mars_sift_pill(): extract boot formulas and module/userspace ova from pill
*/
static c3_o
_mars_sift_pill(u3_noun  pil,
                u3_noun* bot,
                u3_noun* mod,
                u3_noun* use,
                u3_noun* cax)
{
  u3_noun pil_p, pil_q;
  *cax = u3_nul;

  if ( c3n == u3r_cell(pil, &pil_p, &pil_q) ) {
    return c3n;
  }

  {
    //  XX use faster cue
    //
    u3_noun pro = u3m_soft(0, u3ke_cue, u3k(pil_p));
    u3_noun mot, tag, dat;

    if (  (c3n == u3r_trel(pro, &mot, &tag, &dat))
       || (u3_blip != mot) )
    {
      u3m_p("mot", u3h(pro));
      fprintf(stderr, "boot: failed: unable to parse pill\r\n");
      return c3n;
    }

    if ( c3y == u3r_sing_c("ivory", tag) ) {
      fprintf(stderr, "boot: failed: unable to boot from ivory pill\r\n");
      return c3n;
    }
    else if ( (c3__pill != tag) && (c3__cash != tag) ) {
      if ( c3y == u3a_is_atom(tag) ) {
        u3m_p("pill", tag);
      }
      fprintf(stderr, "boot: failed: unrecognized pill\r\n");
      return c3n;
    }

    {
      u3_noun typ;
      c3_c* typ_c;

      if ( (c3__cash == tag) && (c3y == u3du(dat)) ) {
        *cax = u3t(dat);
        dat = u3h(dat);
      }

      if ( c3n == u3r_qual(dat, &typ, bot, mod, use) ) {
        fprintf(stderr, "boot: failed: unable to extract pill\r\n");
        return c3n;
      }

      if ( c3y == u3a_is_atom(typ) ) {
        c3_c* typ_c = u3r_string(typ);
        fprintf(stderr, "boot: parsing %%%s pill\r\n", typ_c);
        c3_free(typ_c);
      }
    }

    u3k(*bot); u3k(*mod); u3k(*use), u3k(*cax);
    u3z(pro);
  }

  //  optionally replace filesystem in userspace
  //
  if ( u3_nul != pil_q ) {
    c3_w len_w = 0;
    u3_noun ova = *use;
    u3_noun new = u3_nul;
    u3_noun ovo, tag;

    while ( u3_nul != ova ) {
      ovo = u3h(ova);
      tag = u3h(u3t(ovo));

      if (  (c3__into == tag)
         || (  (c3__park == tag)
            && (c3__base == u3h(u3t(u3t(ovo)))) ) )
      {
        u3_assert( 0 == len_w );
        len_w++;
        ovo = u3t(pil_q);
      }

      new = u3nc(u3k(ovo), new);
      ova = u3t(ova);
    }

    u3_assert( 1 == len_w );

    u3z(*use);
    *use = u3kb_flop(new);
  }

  u3z(pil);

  return c3y;
}

/* _mars_boot_make(): construct boot sequence
*/
static c3_o
_mars_boot_make(u3_boot_opts* inp_u,
                u3_noun         com,
                u3_noun*        ova,
                u3_noun*        xac,
                u3_meta*      met_u)
{
  //  set the disk version
  //
  met_u->ver_h = U3D_VERLAT;

  u3_noun pil, ven, mor, who;

  //  parse boot command
  //
  if ( c3n == u3r_trel(com, &pil, &ven, &mor) ) {
    fprintf(stderr, "boot: invalid command\r\n");
    return c3n;
  }

  //  parse boot event
  //
  {
    u3_noun tag, dat;

    if ( c3n == u3r_cell(ven, &tag, &dat) ) {
      return c3n;
    }

    switch ( tag ) {
      default: {
        fprintf(stderr, "boot: unknown boot event\r\n");
        u3m_p("tag", tag);
        return c3n;
      }

      case c3__fake: {
        met_u->fak_o = c3y;
        who          = dat;
      } break;

      case c3__dawn: {
        met_u->fak_o = c3n;
        who          = u3h(u3t(u3h(dat)));
      } break;
    }
  }

  //  validate and extract identity
  //
  if (  (c3n == u3a_is_atom(who))
     || (1 < u3r_met(7, who)) )
  {
    fprintf(stderr, "boot: invalid identity\r\n");
    u3m_p("who", who);
    return c3n;
  }

  u3r_chubs(0, 2, met_u->who_d, who);

  {
    u3_noun bot, mod, use, cax;

    //  parse pill
    //
    if ( c3n == _mars_sift_pill(u3k(pil), &bot, &mod, &use, &cax) ) {
      return c3n;
    }

    met_u->lif_h = u3qb_lent(bot);

    //  break symmetry in the module sequence
    //
    //    version negotation, verbose, identity, entropy
    //
    {
      u3_noun cad, wir = u3nt(u3_blip, c3__arvo, u3_nul);

      cad = u3nc(c3__wack, u3i_halfs(17, inp_u->eny_h));
      mod = u3nc(u3nc(u3k(wir), cad), mod);

      cad = u3nc(c3__whom, u3k(who));
      mod = u3nc(u3nc(u3k(wir), cad), mod);

      cad = u3nt(c3__verb, u3_nul, !inp_u->veb_o);
      mod = u3nc(u3nc(u3k(wir), cad), mod);

      cad = _mars_wyrd_card(inp_u->ver_u.nam_m,
                            inp_u->ver_u.ver_h,
                            inp_u->sev_l);
      mod = u3nc(u3nc(wir, cad), mod);  //  transfer [wir]
    }

    //  prepend legacy boot event to the userspace sequence
    //
    //    XX do something about this wire
    //
    {
      u3_noun wir = u3nq(c3__d, c3__term, '1', u3_nul);
      u3_noun cad = u3nt(c3__boot, inp_u->lit_o, u3k(ven));
      use = u3nc(u3nc(wir, cad), use);
    }

    //  add props before/after the userspace sequence
    //
    {
      u3_noun pre = u3_nul;
      u3_noun aft = u3_nul;

      while ( u3_nul != mor ) {
        u3_noun mot = u3h(mor);

        switch ( u3h(mot) ) {
          case c3__prop: {
            u3_noun ter, met, ves;

            if ( c3n == u3r_trel(u3t(mot), &met, &ter, &ves) ) {
              //  XX fatal error?
              //
              u3m_p("invalid prop", u3t(mot));
              break;
            }

            if ( c3__fore == ter ) {
              u3m_p("prop: fore", met);
              pre = u3kb_weld(pre, u3k(ves));
            }
            else if ( c3__hind == ter ) {
              u3m_p("prop: hind", met);
              aft = u3kb_weld(aft, u3k(ves));
            }
            else {
              //  XX fatal error?
              //
              u3m_p("unrecognized prop tier", ter);
            }
          } break;

          //  XX fatal error?
          //
          default: u3m_p("unrecognized boot sequence enhancement", u3h(mot));
        }

        mor = u3t(mor);
      }

      use = u3kb_weld(pre, u3kb_weld(use, aft));
    }

    //  timestamp events, cons list
    //
    {
      u3_noun now = u3m_time_in_tv(&inp_u->tim_u);
      u3_noun bit = u3qc_bex(48);       //  1/2^16 seconds
      u3_noun eve = u3kb_flop(bot);

      {
        u3_noun  lit = u3kb_weld(mod, use);
        u3_noun i, t = lit;

        while ( u3_nul != t ) {
          u3x_cell(t, &i, &t);
          now = u3ka_add(now, u3k(bit));
          eve = u3nc(u3nc(u3k(now), u3k(i)), eve);
        }

        u3z(lit);
      }

      *ova = u3kb_flop(eve);
      u3z(now); u3z(bit);
    }

    //  cache
    //
    {
      u3_noun tmp = cax;
      c3_o gud_o = c3y;
      while ( u3_nul != tmp ) {
        if ( (c3n == u3a_is_cell(tmp)) ||
             (c3n == u3a_is_cell(u3h(tmp))) ||
             (c3n == u3a_is_cell(u3h(u3h(tmp)))) )
        {
          gud_o = c3n;
        }
        tmp = u3t(tmp);
      }

      if ( c3n == gud_o ) {
        u3l_log("mars: got bad cache");
        u3z(cax);
        *xac = u3_nul;
      }
      else {
        *xac = cax;
      }
    }
  }

  u3z(com);

  return c3y;
}

/* u3_mars_make(): construct a pier.
*/
void
u3_mars_make(u3_mars* mar_u)
{
  //  XX s/b unnecessary
  u3_Host.ops_u.nuu = c3y;

  if ( c3n == u3_disk_make(mar_u->dir_c) ) {
    fprintf(stderr, "boot: disk make fail\r\n");
    exit(1);
  }

  //  NB: initializes loom
  //
  if ( !(mar_u->log_u = u3_disk_load(mar_u->dir_c, u3_dlod_boot)) ) {
    fprintf(stderr, "boot: disk init fail\r\n");
    exit(1);
  }
}

/* u3_mars_boot(): boot a ship.
*
*  $=  com
*  $:  pill=[p=@ q=(unit ovum)]
*      $=  vent
*      $%  [%fake p=ship]
*          [%dawn p=dawn-event]
*      ==
*      more=(list prop)
*  ==
*
*/
c3_o
u3_mars_boot(u3_mars* mar_u, c3_y ver_y, c3_d len_d, c3_y* hun_y)
{
  u3_disk*     log_u = mar_u->log_u;
  u3_boot_opts inp_u;
  u3_meta      met_u;
  u3_noun   com, ova, cax;

  inp_u.veb_o = __( u3C.wag_h & u3o_verbose );
  inp_u.lit_o = c3n; // unimplemented in arvo

  //  XX source kelvin from args?
  //
  inp_u.ver_u.nam_m = c3__zuse;
  inp_u.ver_u.ver_h = 408;

  gettimeofday(&inp_u.tim_u, 0);
  c3_rand(inp_u.eny_h);

  {
    u3_noun now = u3m_time_in_tv(&inp_u.tim_u);
    inp_u.sev_l = u3r_mug(now);
    u3z(now);
  }

  {
    //  pick decoder by protocol version (0x01 = ram, 0x00 = jam)
    //
    u3_weak jar = ( 0x01 == ver_y )
                ? u3s_tap_xeno(len_d, hun_y)
                : u3s_cue_xeno(len_d, hun_y);
    if (  (u3_none == jar)
       || (c3n == u3r_p(jar, c3__boot, &com)) )
    {
      fprintf(stderr, "boot: parse fail\r\n");
      exit(1);
    }
    else {
      u3k(com);
      u3z(jar);
    }
  }

  if ( c3n == _mars_boot_make(&inp_u, com, &ova, &cax, &met_u) ) {
    fprintf(stderr, "boot: preparation failed\r\n");
    exit(1);  //  XX cleanup
  }

  if ( c3n == u3_disk_save_meta_meta(log_u->com_u->pax_c, &met_u) ) {
    fprintf(stderr, "boot: failed to save top-level metadata\r\n");
    exit(1);  //  XX cleanup
  }

  if ( c3n == u3_disk_save_meta(log_u->mdb_u, &met_u) ) {
    exit(1);  //  XX cleanup
  }

  u3_disk_plan_list(log_u, ova);

  if ( c3n == u3_disk_sync(log_u) ) {
    exit(1);  //  XX cleanup
  }

  _mars_step_trace(mar_u->dir_c);

  if ( c3n == _mars_do_boot(log_u, log_u->dun_d, cax) ) {
    exit(1);  //  XX cleanup
  }

  u3m_save();

  //  XX move to caller? close uv handles?
  //
  u3_disk_exit(log_u);
  exit(0);

  return c3y;
}

/* u3_mars_grab(): garbage collect.
*/
u3_noun
u3_mars_grab(c3_o pri_o)
{
  u3_noun sac = u3_nul;
  u3_noun res = u3_nul;

  u3_assert( u3R == &(u3H->rod_u) );

  {
    u3_noun sam, gon;

    {
      u3_noun pax = u3nc(c3__whey, u3_nul);
      u3_noun lyc = u3nc(u3_nul, u3_nul);
      sam = u3nt(lyc, c3n, u3nq(c3__once, u3_blip, u3_blip, pax));
    }

    gon = u3m_soft(0, u3v_peek, sam);

    {
      u3_noun tag, dat, val;
      u3x_cell(gon, &tag, &dat);

      if (  (u3_blip == tag)
         && (u3_nul  != dat)
         && (c3y == u3r_pq(u3t(dat), c3__omen, 0, &val))
         && (c3y == u3r_p(val, c3__mass, &sac)) )
      {
        u3k(sac);
      }
    }

    u3z(gon);
  }

  if ( u3_nul != sac ) {
    res = _mars_grab(sac, pri_o);
  }
  else {
    fprintf(stderr, "sac is empty\r\n");

    u3a_mark_init();
    u3m_quac** var_u = u3m_mark();

    c3_w tot_w = 0;
    c3_w i_w = 0;
    while ( var_u[i_w] != NULL ) {
      tot_w += var_u[i_w]->siz_w;
      u3a_quac_free(var_u[i_w]);
      i_w++;
    }
    c3_free(var_u);

    u3a_print_memory(stderr, "total marked", tot_w / 4);
    u3a_print_memory(stderr, "free lists", u3a_idle(u3R));
    //  XX sweep could be optional, gated on u3o_debug_ram or somesuch
    //  only u3a_mark_done() is required
    u3a_print_memory(stderr, "sweep", u3a_sweep());
    fprintf(stderr, "\r\n");
  }

  fflush(stderr);

  return res;
}
