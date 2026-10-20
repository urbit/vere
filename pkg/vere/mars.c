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
**   See the u3a_blob block in allocate.h for the record:
**     use_h = les_h + live bob-atom cardinality
**     les_h = holds the king has (mars memory only; zeroed at boot)
**     own_o = a durable holder in the current epoch names the file: an
**             input committed in this epoch, or the epoch's image
**
**   Blob files live at $pier/.urb/log/0i<epoch>/bob/<mug>/<seq>: an
**   epoch owns the files its events and its image name, and they die
**   with the epoch at chop.  Deletion by refcount: use_h == 0 and not
**   owned (u3a_blob_dead).
**
**   Holds are counted grants.  every message mars hands the king (a
**   poke's effects, a nack's goofs, a peek's result) grants one hold
**   per distinct blob id it names, issued as the message is encoded
**   (u3s_ram_xeno_bids) so the file outlives mars's own atoms.  the
**   king counts what it received per id and returns the lot with
**   [%blrl mug seq n] when its last bob over the id dies.  no expiry:
**   king and mars die together, holds are memory only, and the boot gc
**   reclaims whatever a dead king held.
**
**   Lifecycle:
**     1. king stages a large file to .urb/stg/, learns its content mug,
**        and plans the event with a stub (a bob of that mug, seq 0)
**        where the bob belongs and the staging path riding along
**     2. mars installs the file (rename into the epoch's store) and
**        puts the real bob in the stub's place, then computes the event
**     3. the event's effects grant the king a hold on every blob they
**        name, this one included if it comes back out
**     4. mars commits: _mars_fact marks the blob owned by the epoch and
**        writes the event's BLOBS row in the event's transaction
**     5. the king's last bob over the id dies: %blrl returns its holds
**        (one per message that named the id)
**     6. arvo drops the atom (e.g. |rm + |tomb): _me_bob_dead takes it
**        off use_h; an owned record stays as a ghost until the next roll
**     7. roll carries every held blob into the new epoch's store, owns
**        what a root reaches, and writes that live set as the epoch's
**        first BLOBS row; chop deletes old epochs whole, files included,
**        and touches no count
**     *. boot: _find_home zeroes les_h and own_o (neither outlives a
**        process); u3_disk_blob_own marks the epoch's rows through the
**        snapshot's event and replay marks the rest; u3_disk_blob_gc
**        reclaims any unowned blob nothing holds
*/

c3_c tac_c[256];  //  tracing label

/* _mars_blob_del(): mars-side del_f: delete the blob file once nothing
**   holds it (u3a_blob_dead).  an owned record with no other holder
**   stays as a ghost until the next roll.  called when the last live
**   atom goes, and when the king's last hold does.
*/
static void
_mars_blob_del(c3_h mug_h, c3_h seq_h)
{
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);

  //  a dry run deletes nothing: it skips replay, so a record can read
  //  unowned while an input after the snapshot still names its file
  //
  if (  (u3C.wag_h & u3o_dryrun)
     || !blb_u
     || (c3n == u3a_blob_dead(blb_u)) )
  {
    return;
  }

  u3b_wipe(u3C.bob_c, mug_h, seq_h);
  u3a_blob_drop(mug_h, seq_h);
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
      [%poke mil=@ pax=(list @t) ovum]  ::  pax: files staged for it
      [%blrl mug=@ seq=@ n=@]           ::  the king returns n holds
      [%sync %save ~]
  ==
+$  gift                                                ::  mars -> urth
  $%  [%live ~]
      [%flog cord]
      [%slog pri=@ tank]
      [%peek p=(each (unit (cask)) goof)]
      [%poke p=(each (list ovum) (list goof))]
      [%ripe [pro=%3 kel=wynn] [who=@p fake=?] eve=@ mug=@ epo=@]
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
      u3m_grab(sac, u3_none);
      u3C.wag_h &= ~u3o_check_corrupt;
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

        //  blobs the serf holds open: zero between events, so anything
        //  here is a reader that never closed
        //
        fprintf(fil_u, "blob handles: %zu (%" PRIc3_d " bytes mapped)\r\n",
                u3b_hands(), u3b_mapped());
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

/* _mars_blob_sure(): refuse an event holding a bob the store cannot back.
**
**   [bid_u] is the distinct ids the writ's tap decoded, stubs excluded;
**   the bobs the install puts in the stubs' places name files it just
**   wrote.  tap mints a bank record for every blob id it decodes, so an event
**   naming a file that is gone arrives with a fresh record over a
**   missing file.  committing it would log an event that can never
**   replay.  a record the king does not hold and no epoch owns is
**   backed only if its file exists (the event's own atoms count for
**   nothing here: tap minted them); otherwise the poke is nacked with
**   a %blob goof.  returns the goof list, or u3_none when every bob is
**   backed.
*/
static u3_weak
_mars_blob_sure(const u3s_bids* bid_u)
{
  u3_noun tan = u3_nul;

  for ( c3_w i_w = 0; i_w < bid_u->len_w; i_w++ ) {
    c3_h      mug_h = u3a_bid_mug(bid_u->bid_d[i_w]);
    c3_h      seq_h = u3a_bid_seq(bid_u->bid_d[i_w]);
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);

    if ( blb_u && (blb_u->les_h || (c3y == blb_u->own_o)) ) {
      continue;
    }
    if ( c3y == u3b_live(u3C.bob_c, mug_h, seq_h) ) {
      continue;
    }

    {
      c3_c msg_c[128];
      snprintf(msg_c, sizeof(msg_c),
               "blob " U3A_BID_FMT " is not in the store",
               mug_h, seq_h);
      fprintf(stderr, "mars: poke refused: %s\r\n", msg_c);
      tan = u3nc(u3nc(c3__leaf, u3i_tape(msg_c)), tan);
    }
  }

  if ( u3_nul == tan ) {
    return u3_none;
  }
  return u3nc(u3nc(c3__blob, tan), u3_nul);
}

/* _mars_ram(): encode a message for the king into [gif_u], grant the
**   king a hold on every blob id it names, and free the noun.
**
**   the grant comes before the free: the noun may be the last atom
**   over an id mars holds, and the file must outlive it for the king.
*/
static void
_mars_ram(u3_noun pro, u3_gift* gif_u)
{
  u3s_bids bid_u = {0};

  u3s_ram_xeno_bids(pro, &gif_u->len_d, &gif_u->hun_y, &bid_u);
  u3a_blob_hold(bid_u.bid_d, bid_u.len_w);
  c3_free(bid_u.bid_d);
  u3z(pro);
}

/* _mars_fill(): rebuild [non] with every stub replaced by the bob of
**   the installed id with its mug, marking each id used in [hit_y]
**   (when given).  recursive on heads, a loop along tails, so a long
**   list costs no stack.  u3_none if a stub matches no id.  retains
**   [non].
*/
static u3_weak
_mars_fill(u3_noun non, const c3_d* bid_d, c3_w len_w, c3_y* hit_y)
{
  if ( c3n == u3a_is_cell(non) ) {
    if ( (c3y == u3a_is_bob(non)) && (0 == u3a_bob_seq(non)) ) {
      c3_h mug_h = u3a_bob_mug(non);

      for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
        if ( u3a_bid_mug(bid_d[i_w]) == mug_h ) {
          if ( hit_y ) {
            hit_y[i_w] = 1;
          }
          return u3i_blob(mug_h, u3a_bid_seq(bid_d[i_w]));
        }
      }
      return u3_none;
    }
    return u3k(non);
  }

  {
    u3_noun* hed = 0;
    c3_w     cap_w = 0, num_w = 0;
    u3_noun  tal = non;
    u3_weak  out;

    while ( c3y == u3a_is_cell(tal) ) {
      u3_weak one = _mars_fill(u3h(tal), bid_d, len_w, hit_y);

      if ( u3_none == one ) {
        goto fail;
      }
      if ( num_w == cap_w ) {
        cap_w = cap_w ? (2 * cap_w) : 16;
        hed   = c3_realloc(hed, cap_w * sizeof(*hed));
      }
      hed[num_w++] = one;
      tal = u3t(tal);
    }

    if ( u3_none == (out = _mars_fill(tal, bid_d, len_w, hit_y)) ) {
      goto fail;
    }

    while ( num_w-- ) {
      out = u3nc(hed[num_w], out);
    }
    c3_free(hed);
    return out;

  fail:
    while ( num_w-- ) {
      u3z(hed[num_w]);
    }
    c3_free(hed);
    return u3_none;
  }
}

/* u3_mars_blob_fill(): put installed bobs in a noun's stubs.
*/
u3_weak
u3_mars_blob_fill(u3_noun non, const c3_d* bid_d, c3_w len_w)
{
  return _mars_fill(non, bid_d, len_w, 0);
}

/* _mars_stg_ours(): [stg_c] names a file directly in the pier's staging
**   directory, as u3b_stage makes them.  the directories are compared
**   resolved, so the king's spelling of the pier need not match ours.
*/
static c3_o
_mars_stg_ours(const c3_c* stg_c)
{
  const c3_c* nam_c = strrchr(stg_c, '/');
  c3_c        dir_c[8192];
  c3_c*       our_c;
  c3_c*       its_c;
  c3_o        ret_o;

  if ( !nam_c || !nam_c[1] || ((nam_c - stg_c) >= (c3_ws)sizeof(dir_c)) ) {
    return c3n;
  }

  memcpy(dir_c, stg_c, nam_c - stg_c);
  dir_c[nam_c - stg_c] = 0;
  its_c = realpath(dir_c, 0);

  u3b_stg_dir(dir_c, u3C.dir_c);
  our_c = realpath(dir_c, 0);

  ret_o = __( our_c && its_c && (0 == strcmp(our_c, its_c)) );
  free(our_c);
  free(its_c);
  return ret_o;
}

/* _mars_blob_install(): install the staged files an event brings and
**   put their bobs where the king left stubs.
**
**   every path in [pax] is installed, or the event is refused with a
**   %blob goof: the staged files are consumed either way (nothing
**   retries them), and a file installed for a refused event, or one no
**   stub asked for, is wiped again by minting and dropping its bob,
**   since nothing holds it.  on success [*job] is the filled event.
**   returns the goof list, or u3_none.  transfers [pax].
*/
static u3_weak
_mars_blob_install(u3_noun* job, u3_noun pax)
{
  u3s_bids bid_u = {0};
  u3_noun  tan   = u3_nul;
  u3_noun  pak   = pax;

  while ( u3_nul != pak ) {
    if ( (c3n == u3du(pak)) || (c3n == u3a_is_atom(u3h(pak))) ) {
      fprintf(stderr, "mars: poke refused: malformed staging list\r\n");
      tan = u3nc(u3nc(c3__leaf, u3i_string("malformed staging list")), tan);
      break;
    }

    c3_c* stg_c = u3r_string(u3h(pak));
    c3_h  mug_h = 0;
    c3_h  seq_h = 0;

    //  a path outside the staging directory is not ours to install or
    //  remove
    //
    if ( c3n == _mars_stg_ours(stg_c) ) {
      c3_c msg_c[128];
      snprintf(msg_c, sizeof(msg_c), "not a staging file: %.96s", stg_c);
      fprintf(stderr, "mars: poke refused: %s\r\n", msg_c);
      tan = u3nc(u3nc(c3__leaf, u3i_tape(msg_c)), tan);
    }
    else if ( c3y == u3b_move_stg(u3C.bob_c, stg_c, &mug_h, &seq_h) ) {
      u3s_bids_push(&bid_u, u3a_bid(mug_h, seq_h));
    }
    else {
      c3_c msg_c[128];
      snprintf(msg_c, sizeof(msg_c), "blob install failed: %s", stg_c);
      fprintf(stderr, "mars: poke refused: %s\r\n", msg_c);
      tan = u3nc(u3nc(c3__leaf, u3i_tape(msg_c)), tan);
      c3_unlink(stg_c);
    }
    c3_free(stg_c);
    pak = u3t(pak);
  }
  u3z(pax);

  //  a stub names its file by mug alone: two installs of one mug and
  //  different content cannot be told apart, so the event is refused
  //  rather than given the wrong file.  one content twice is one id
  //
  for ( c3_w i_w = 0; (u3_nul == tan) && (i_w < bid_u.len_w); i_w++ ) {
    for ( c3_w j_w = i_w + 1; j_w < bid_u.len_w; j_w++ ) {
      if (  (u3a_bid_mug(bid_u.bid_d[i_w]) == u3a_bid_mug(bid_u.bid_d[j_w]))
         && (bid_u.bid_d[i_w] != bid_u.bid_d[j_w]) )
      {
        fprintf(stderr, "mars: poke refused: two staged files share a mug\r\n");
        tan = u3nc(u3nc(c3__leaf,
                        u3i_string("two staged files share a mug")),
                   u3_nul);
        break;
      }
    }
  }

  c3_y* hit_y = bid_u.len_w ? c3_calloc(bid_u.len_w) : 0;

  if ( u3_nul == tan ) {
    u3_weak new = _mars_fill(*job, bid_u.bid_d, bid_u.len_w, hit_y);

    if ( u3_none == new ) {
      fprintf(stderr, "mars: poke refused: a stub names no staged file\r\n");
      tan = u3nc(u3nc(c3__leaf, u3i_string("blob stub names no staged file")),
                 u3_nul);
    }
    else {
      u3z(*job);
      *job = new;
    }
  }

  //  nothing holds what was installed for a refused event, or what no
  //  stub asked for: mint and drop, and the file goes
  //
  for ( c3_w i_w = 0; i_w < bid_u.len_w; i_w++ ) {
    if ( (u3_nul != tan) || !hit_y[i_w] ) {
      c3_d bid_d = bid_u.bid_d[i_w];
      u3z(u3i_blob(u3a_bid_mug(bid_d), u3a_bid_seq(bid_d)));
    }
  }
  c3_free(hit_y);
  c3_free(bid_u.bid_d);

  if ( u3_nul == tan ) {
    return u3_none;
  }
  return u3nc(u3nc(c3__blob, tan), u3_nul);
}

/* _mars_fact(): commit a fact and enqueue its effects.
*/
static void
_mars_fact(u3_mars* mar_u,
           u3_noun    job,
           u3_noun    pro)
{
  //  the disk owns, for this epoch, every blob the event names as it
  //  encodes it, and writes them as the event's BLOBS row
  //
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

    _mars_ram(pro, gif_u);

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

  _mars_ram(pro, gif_u);

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
    //  %evil, and %file (out of descriptors, transient), refuse the
    //  event rather than commit a %crud: replay saves and exits on %file
    //
    else if ( (c3__evil == u3h(pro)) || (c3__file == u3h(pro)) ) {
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

/* _mars_work(): perform a task.  [bid_u] is the distinct blob ids the
**   writ named, as its tap collected them.
*/
static c3_o
_mars_work(u3_mars* mar_u, u3_noun jar, const u3s_bids* bid_u)
{
  u3_noun tag, dat, pro;

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
      u3_noun tim, pax, job;
      c3_h  mil_h;
      c3_w  pre_w;

      if ( (c3n == u3r_trel(dat, &tim, &pax, &job)) ||
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
      u3k(pax);
      u3z(jar);

      //  the files the event brings are installed first and their bobs
      //  put where the king left stubs; a failure refuses the event
      //
      if ( u3_nul != pax ) {
        u3_weak lud = _mars_blob_install(&job, pax);

        if ( u3_none != lud ) {
          u3z(job);
          _mars_gift(mar_u, u3nt(c3__poke, c3n, lud));
          break;
        }
        mar_u->fag_w |= _mars_fag_mute;
      }
      else {
        u3z(pax);

        //  a stub with no staged file would reach the computation and
        //  the log naming nothing
        //
        if ( bid_u->nub_w ) {
          u3_noun tan = u3nc(u3nc(c3__leaf,
                                  u3i_string("blob stub with no staged file")),
                             u3_nul);
          fprintf(stderr, "mars: poke refused: a stub with no staged file\r\n");
          u3z(job);
          _mars_gift(mar_u, u3nt(c3__poke, c3n, u3nc(u3nc(c3__blob, tan), u3_nul)));
          break;
        }
      }

      //  an event holding a bob the store cannot back is refused before
      //  it runs; freeing the job drops the record tap minted for it
      //
      {
        u3_weak lud = _mars_blob_sure(bid_u);

        if ( u3_none != lud ) {
          u3z(job);
          _mars_gift(mar_u, u3nt(c3__poke, c3n, lud));
          break;
        }
      }

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
      u3z(jar);
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

    //  %blrl: the king returns [n] holds on a blob: its last bob over
    //  the id died.  a count, not a flag: a hold granted after the king
    //  sent this must survive it.  clamped, never below zero.
    //
    case c3_s4('b','l','r','l'): {
      u3_noun mug_n, seq_n, num_n;
      if ( c3n == u3r_trel(dat, &mug_n, &seq_n, &num_n) ) {
        u3z(jar);
        return c3n;
      }
      c3_h mug_h = 0;
      c3_h seq_h = 0;
      c3_h num_h = 0;
      u3r_safe_half(mug_n, &mug_h);
      u3r_safe_half(seq_n, &seq_h);
      u3r_safe_half(num_n, &num_h);

      {
        u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);

        if ( blb_u ) {
          num_h = c3_min(num_h, blb_u->les_h);
          blb_u->les_h -= num_h;
          blb_u->use_h -= num_h;
          _mars_blob_del(mug_h, seq_h);
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
    u3s_bids bid_u = {0};
    u3_weak  jar   = ( 0x01 == ver_y )
                   ? u3s_tap_xeno_bids(len_d, hun_y, &bid_u)
                   : u3s_cue_xeno_with(mar_u->sil_u, len_d, hun_y);

    if (  (u3_none == jar)
       || (c3n == _mars_work(mar_u, jar, &bid_u)) )
    {
      fprintf(stderr, "mars: bad\r\n");
      exit(1);
    }
    c3_free(bid_u.bid_d);

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
  _play_fil_e,  //  %file
  _play_int_e,  //  %intr
  _play_log_e,  //  event log fail
  _play_mug_e,  //  mug mismatch
  _play_blo_e,  //  a blob the log names is missing from the store
  _play_bad_e   //  total failure
} _mars_play_e;

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
      if ( c3n == u3_disk_blob_own(log_u, fir_d, mar_u->dun_d) ) {
        return _play_blo_e;
      }
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

      //  mark what the batch replayed before the failure, after the
      //  disk walk (one read txn per thread); a missing file outranks
      //  the failure, since a retry would resume past the check
      //
      if ( c3n == u3_disk_blob_own(log_u, fir_d, mar_u->dun_d) ) {
        u3z(dud); u3z(wen);
        return _play_blo_e;
      }

      u3_assert( c3y == u3r_safe_half(u3h(dud), &mot_m) );

      switch ( mot_m ) {
        case c3__meme: {
          fprintf(stderr, "play (%" PRIu64 "): %%meme\r\n", tac_u.eve_d);
          u3z(dud); u3z(wen);
          return _play_mem_e;
        }

        case c3__file: {
          fprintf(stderr, "play (%" PRIu64 "): %%file\r\n", tac_u.eve_d);
          u3z(dud); u3z(wen);
          return _play_fil_e;
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

  //  mark what the batch's events own, after the disk walk (one read
  //  txn per thread).  a missing file makes the range unreproducible:
  //  fail hard rather than continue with silent corruption
  //
  if ( c3n == u3_disk_blob_own(log_u, fir_d, mar_u->dun_d) ) {
    u3z(wen);
    return _play_blo_e;
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
    c3_w  bat_w;

    while ( mar_u->dun_d < eve_d ) {
      _mars_step_trace(mar_u->dir_c);

      //  XX get batch from args
      //
      bat_w = c3_min(1024ULL, eve_d - mar_u->dun_d);

      switch ( _mars_play_batch(mar_u, c3y, bat_w, &wen_c) ) {
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

        case _play_fil_e: {
          fprintf(stderr, "play (%" PRIu64 "): failed, out of file "
                          "descriptors\r\n", mar_u->dun_d + 1);
          u3m_save();
          u3_disk_exit(log_u);
          exit(1);
        } break;

        case _play_int_e: {
          fprintf(stderr, "play (%" PRIu64 "): interrupted\r\n", mar_u->dun_d + 1);
          u3m_save();
          //  XX exit code, cb
          //
          u3_disk_exit(log_u);
          exit(1);
        } break;

        //  the snapshot must not move past an event whose blob is
        //  gone: exit unsaved, and the next boot fails the same way
        //
        case _play_blo_e: {
          fprintf(stderr, "play: failed: the log references blob data "
                          "that no longer exists\r\n");
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

  //  ownership is rebuilt from the manifest: _find_home zeroed the
  //  snapshot's flags; mark every blob the epoch's rows name, from the
  //  image's row at the epoch's first key through the snapshot's event.
  //  replay marks the rest.  a missing file is fatal here as in replay:
  //  the image depends on it
  //
  if ( c3n == u3_disk_blob_own(mar_u->log_u,
                               mar_u->log_u->epo_d,
                               mar_u->dun_d) )
  {
    fprintf(stderr, "mars: the image depends on blob data that no longer "
                    "exists\r\n");
    u3_disk_exit(mar_u->log_u);
    exit(1);
  }

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
  if ( mar_u->log_u->dun_d > mar_u->dun_d && !(u3C.wag_h & u3o_dryrun) ) {
    u3_disk_exit(mar_u->log_u);
    exit(0);
  }

  //  boot marked ownership from the epoch's rows through the snapshot,
  //  replay marked the events after it, and no hold outlives a restart.
  //  so the gc may reclaim any unowned blob nothing holds, once the log
  //  is caught up: short of that (a dry run skips replay) an input with
  //  no record yet would read as an orphan
  //
  if ( mar_u->dun_d == mar_u->log_u->dun_d ) {
    u3_disk_blob_gc();
  }

  //  send ready status message
  //
  //    XX version negotiation
  //
  {
    c3_d  len_d;
    c3_y* hun_y;
    u3_noun wyn = u3_nul;
    u3_noun msg = u3nq(c3__ripe,
                       u3nc(3, wyn),
                       u3nc(u3i_chubs(2, mar_u->met_u.who_d),
                            mar_u->met_u.fak_o),
                       u3nt(u3i_chub(mar_u->dun_d),
                            mar_u->mug_h,
                            u3i_chub(mar_u->log_u->epo_d)));

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
u3_mars_boot(void* ram_u, c3_y ver_y, c3_d len_d, c3_y* hun_y)
{
  u3_mars*     mar_u = ram_u;
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
