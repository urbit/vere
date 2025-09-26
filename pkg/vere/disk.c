/// @file

#include "noun.h"
#include "events.h"
#include "vere.h"
#include "version.h"
#include "db/lmdb.h"
#include <types.h>

#include "migrate.h"
#include "v4.h"

struct _u3_disk_walk {
  u3_lmdb_walk  itr_u;
  u3_disk*      log_u;
  c3_o          liv_o;
};

#undef VERBOSE_DISK
#undef DISK_TRACE_JAM
#undef DISK_TRACE_CUE

/* _disk_commit_done(): commit complete.
 */
static void
_disk_commit_done(u3_disk* log_u)
{
  c3_d eve_d = log_u->sav_u.eve_d;
  c3_w len_w = log_u->sav_u.len_w;
  c3_o ret_o = log_u->sav_u.ret_o;

#ifdef VERBOSE_DISK
  c3_c* msg_c = ( c3y == ret_o ) ? "complete" : "failed";

  if ( 1 == len_w ) {
    fprintf(stderr, "disk: (%" PRIu64 "): commit: %s\r\n", eve_d, msg_c);
  }
  else {
    fprintf(stderr, "disk: (%" PRIu64 "-%" PRIu64 "): commit: %s\r\n",
                    eve_d,
                    eve_d + (len_w - 1),
                    msg_c);
  }
#endif

  if ( c3y == ret_o ) {
    log_u->dun_d += len_w;
  }

  if ( log_u->sav_u.don_f ) {
    log_u->sav_u.don_f(log_u->sav_u.ptr_v, eve_d + (len_w - 1), ret_o);
  }
 
  {
    u3_feat* fet_u = log_u->put_u.ext_u;

    while ( fet_u && (fet_u->eve_d <= log_u->dun_d) ) {
      log_u->put_u.ext_u = fet_u->nex_u;
      c3_free(fet_u->hun_y);
      c3_free(fet_u);
      fet_u = log_u->put_u.ext_u;
    }
  }

  if ( !log_u->put_u.ext_u ) {
    log_u->put_u.ent_u = 0;
  }
}

static void
_disk_commit(u3_disk* log_u);

/* _disk_commit_after_cb(): on the main thread, finish write
*/
static void
_disk_commit_after_cb(uv_work_t* ted_u, c3_i sas_i)
{
  u3_disk* log_u = ted_u->data;

  log_u->sav_u.ted_o = c3n;

  if ( UV_ECANCELED != sas_i ) {
    _disk_commit_done(log_u);
    _disk_commit(log_u);
  }
}

/* _disk_commit_cb(): off the main thread, write event-batch.
*/
static void
_disk_commit_cb(uv_work_t* ted_u)
{
  u3_disk* log_u = ted_u->data;

  log_u->sav_u.ret_o = u3_lmdb_save(log_u->mdb_u,
                                    log_u->sav_u.eve_d,
                                    log_u->sav_u.len_w,
                            (void**)log_u->sav_u.byt_y,
                                    log_u->sav_u.siz_i);
}

/* _disk_commit_start(): queue async event-batch write.
*/
static void
_disk_commit_start(u3_disk* log_u)
{
  u3_assert( c3n == log_u->sav_u.ted_o );
  log_u->sav_u.ted_o = c3y;

  //  queue asynchronous work to happen on another thread
  //
  uv_queue_work(u3L, &log_u->sav_u.ted_u, _disk_commit_cb,
                                          _disk_commit_after_cb);
}

/* u3_disk_etch(): serialize an event for persistence. RETAIN [eve]
*/
size_t
u3_disk_etch(u3_disk* log_u,
             u3_noun    eve,
             c3_l     mug_l,
             c3_y**   out_y)
{
  size_t len_i;
  c3_y*  dat_y;
#ifdef DISK_TRACE_JAM
  u3t_event_trace("disk etch", 'B');
#endif

  //  XX check version number in log_u
  //  XX needs api redesign to limit allocations
  //
  {
    u3_atom mat = u3qe_jam(eve);
    c3_w  len_w = u3r_met(3, mat);

    len_i = 4 + len_w;
    dat_y = c3_malloc(len_i);

    dat_y[0] = mug_l & 0xff;
    dat_y[1] = (mug_l >> 8) & 0xff;
    dat_y[2] = (mug_l >> 16) & 0xff;
    dat_y[3] = (mug_l >> 24) & 0xff;
    u3r_bytes(0, len_w, dat_y + 4, mat);

    u3z(mat);
  }

#ifdef DISK_TRACE_JAM
  u3t_event_trace("disk etch", 'E');
#endif

  *out_y = dat_y;
  return len_i;
}

/* _disk_batch(): create a write batch
*/
static c3_o
_disk_batch(u3_disk* log_u)
{
  u3_feat* fet_u = log_u->put_u.ext_u;
  c3_w     len_w = log_u->sen_d - log_u->dun_d;

  if ( !len_w || (c3y == log_u->sav_u.ted_o) ) {
    return c3n;
  }

  len_w = c3_min(len_w, 100);

  u3_assert( fet_u );
  u3_assert( (1ULL + log_u->dun_d ) == fet_u->eve_d );

  log_u->sav_u.ret_o = c3n;
  log_u->sav_u.eve_d = fet_u->eve_d;
  log_u->sav_u.len_w = len_w;

  for ( c3_w i_w = 0ULL; i_w < len_w; ++i_w) {
    u3_assert( fet_u );
    u3_assert( (log_u->sav_u.eve_d + i_w) == fet_u->eve_d );

    log_u->sav_u.byt_y[i_w] = fet_u->hun_y;
    log_u->sav_u.siz_i[i_w] = fet_u->len_i;

    fet_u  = fet_u->nex_u;
  }

  log_u->hit_w[len_w]++;

  return c3y;
}

/* _disk_commit(): commit all available events, if idle.
*/
static void
_disk_commit(u3_disk* log_u)
{
  if ( c3y == _disk_batch(log_u) ) {
    #ifdef VERBOSE_DISK
        if ( 1 == len_w ) {
          fprintf(stderr, "disk: (%" PRIu64 "): commit: request\r\n",
                          log_u->sav_u.eve_d);
        }
        else {
          fprintf(stderr, "disk: (%" PRIu64 "-%" PRIu64 "): commit: request\r\n",
                          log_u->sav_u.eve_d,
                          (log_u->sav_u.eve_d + log_u->sav_u.len_w - 1));
        }
    #endif
    
        _disk_commit_start(log_u);
      }
}

/* _disk_plan(): enqueue serialized fact (feat) for persistence.
*/
static void
_disk_plan(u3_disk* log_u,
           c3_l     mug_l,
           u3_noun    job)
{
  u3_feat* fet_u = c3_malloc(sizeof(*fet_u));
  fet_u->eve_d = ++log_u->sen_d;
  fet_u->len_i = u3_disk_etch(log_u, job, mug_l, &fet_u->hun_y);
  fet_u->nex_u = 0;

  if ( !log_u->put_u.ent_u ) {
    u3_assert( !log_u->put_u.ext_u );
    log_u->put_u.ent_u = log_u->put_u.ext_u = fet_u;
  }
  else {
    log_u->put_u.ent_u->nex_u = fet_u;
    log_u->put_u.ent_u = fet_u;
  }
}

/* u3_disk_plan(): enqueue completed event for persistence.
*/
void
u3_disk_plan(u3_disk* log_u, u3_fact* tac_u)
{
  u3_assert( (1ULL + log_u->sen_d) == tac_u->eve_d );

  _disk_plan(log_u, tac_u->mug_l, tac_u->job);

  _disk_commit(log_u);
}

/* u3_disk_plan_list(): enqueue completed event list, without autocommit.
*/
void
u3_disk_plan_list(u3_disk* log_u, u3_noun lit)
{
  u3_noun i, t = lit;

  while ( u3_nul != t ) {
    u3x_cell(t, &i, &t);
    //  NB, boot mugs are 0
    //
    _disk_plan(log_u, 0, i);
  }

  u3z(lit);
}

/* u3_disk_sync(): commit planned events.
*/
c3_o
u3_disk_sync(u3_disk* log_u)
{
  c3_o ret_o = c3n;

  //  XX max 100
  //
  if ( c3y == _disk_batch(log_u) ) {
    ret_o = u3_lmdb_save(log_u->mdb_u,
                         log_u->sav_u.eve_d,
                         log_u->sav_u.len_w,
                 (void**)log_u->sav_u.byt_y,
                         log_u->sav_u.siz_i);

    log_u->sav_u.ret_o = ret_o;

    //  XX don't want callbacks
    //
    _disk_commit_done(log_u);
  }

  return ret_o;
}

/* u3_disk_async(): active autosync with callbacks.
*/
void
u3_disk_async(u3_disk*     log_u,
              void*        ptr_v,
              u3_disk_news don_f)
{
  //  XX add flag to control autosync
  //
  log_u->sav_u.ptr_v = ptr_v;
  log_u->sav_u.don_f = don_f;
}

/* u3_disk_sift(): parse a persisted event buffer.
*/
c3_o
u3_disk_sift(u3_disk* log_u,
             size_t   len_i,
             c3_y*    dat_y,
             c3_l*    mug_l,
             u3_noun*   job)
{
  if ( 4 >= len_i ) {
    return c3n;
  }

#ifdef DISK_TRACE_CUE
  u3t_event_trace("disk sift", 'B');
#endif

  //  XX check version in log_u
  //
  *mug_l = dat_y[0]
         ^ (dat_y[1] <<  8)
         ^ (dat_y[2] << 16)
         ^ (dat_y[3] << 24);

  //  XX u3m_soft?
  //
  *job = u3ke_cue(u3i_bytes(len_i - 4, dat_y + 4));

#ifdef DISK_TRACE_CUE
  u3t_event_trace("disk sift", 'E');
#endif

  return c3y;
}

struct _cd_list {
  u3_disk* log_u;
  u3_noun    eve;
  c3_l     mug_l;
};

/* _disk_read_list_cb(): lmdb read callback, invoked for each event in order
*/
static c3_o
_disk_read_list_cb(void* ptr_v, c3_d eve_d, size_t val_i, void* val_p)
{
  struct _cd_list* ven_u = ptr_v;
  u3_disk* log_u = ven_u->log_u;

  {
    u3_noun job;
    c3_l  mug_l;

    if ( c3n == u3_disk_sift(log_u, val_i, (c3_y*)val_p, &mug_l, &job) ) {
      return c3n;
    }

    ven_u->mug_l = mug_l;
    ven_u->eve   = u3nc(job, ven_u->eve);
  }

  return c3y;
}

/* u3_disk_read_list(): synchronously read a cons list of events.
*/
u3_weak
u3_disk_read_list(u3_disk* log_u, c3_d eve_d, c3_d len_d, c3_l* mug_l)
{
  struct _cd_list ven_u = { log_u, u3_nul, 0 };

  if ( c3n == u3_lmdb_read(log_u->mdb_u, &ven_u,
                           eve_d, len_d, _disk_read_list_cb) )
  {
    // XX test normal (not subcommand) replay with and without, 
    //    then run |mass each time
    u3z(ven_u.eve);
    return u3_none;
  }

  *mug_l = ven_u.mug_l;
  return u3kb_flop(ven_u.eve);
}

/* u3_disk_walk_init(): init iterator.
*/
u3_disk_walk*
u3_disk_walk_init(u3_disk* log_u,
                  c3_d     eve_d,
                  c3_d     len_d)
{
  u3_disk_walk* wok_u = c3_malloc(sizeof(*wok_u));
  c3_d          max_d = eve_d + len_d - 1;

  wok_u->log_u = log_u;
  wok_u->liv_o = u3_lmdb_walk_init(log_u->mdb_u,
                                  &wok_u->itr_u,
                                   eve_d,
                                   c3_min(max_d, log_u->dun_d));

  if ( c3n == wok_u->liv_o ) {
    c3_free(wok_u);
    return 0;
  }

  return wok_u;
}

/* u3_disk_walk_live(): check if live.
*/
c3_o
u3_disk_walk_live(u3_disk_walk* wok_u)
{
  if ( wok_u->itr_u.nex_d > wok_u->itr_u.las_d ) {
    wok_u->liv_o = c3n;
  }

  return wok_u->liv_o;
}

/* u3_disk_walk_step(): get next fact.
*/
c3_o
u3_disk_walk_step(u3_disk_walk* wok_u, u3_fact* tac_u)
{
  u3_disk* log_u = wok_u->log_u;
  size_t   len_i;
  void*    buf_v;

  tac_u->eve_d = wok_u->itr_u.nex_d;

  if ( c3n == u3_lmdb_walk_next(&wok_u->itr_u, &len_i, &buf_v) ) {
    fprintf(stderr, "disk: (%" PRIu64 "): read fail\r\n", tac_u->eve_d);
    return wok_u->liv_o = c3n;
  }

  if ( c3n == u3_disk_sift(log_u, len_i,
                           (c3_y*)buf_v,
                           &tac_u->mug_l,
                           &tac_u->job) )
  {
    fprintf(stderr, "disk: (%" PRIu64 "): sift fail\r\n", tac_u->eve_d);
    return wok_u->liv_o = c3n;
  }

  return c3y;
}

/* u3_disk_walk_done(): close iterator.
*/
void
u3_disk_walk_done(u3_disk_walk* wok_u)
{
  u3_lmdb_walk_done(&wok_u->itr_u);
  c3_free(wok_u);
}

/* _disk_save_meta(): serialize atom, save as metadata at [key_c].
*/
static c3_o
_disk_save_meta(MDB_env* mdb_u, const c3_c* key_c, c3_w len_w, c3_y* byt_y)
{
  //  strip trailing zeroes.
  //
  while ( len_w && !byt_y[len_w - 1] ) {
    len_w--;
  }

  return u3_lmdb_save_meta(mdb_u, key_c, len_w, byt_y);
}

/* u3_disk_save_meta(): save metadata.
*/
c3_o
u3_disk_save_meta(MDB_env* mdb_u, const u3_meta* met_u)
{
  u3_assert( c3y == u3a_is_cat(met_u->lif_w) );

  u3_noun who = u3i_chubs(2, met_u->who_d);

  if (  (c3n == _disk_save_meta(mdb_u, "version", sizeof(c3_w), (c3_y*)&met_u->ver_w))
     || (c3n == _disk_save_meta(mdb_u, "who", 2 * sizeof(c3_d), (c3_y*)met_u->who_d))
     || (c3n == _disk_save_meta(mdb_u, "fake", sizeof(c3_o), (c3_y*)&met_u->fak_o))
     || (c3n == _disk_save_meta(mdb_u, "life", sizeof(c3_w), (c3_y*)&met_u->lif_w)) )
  {
    u3z(who);
    return c3n;
  }

  u3z(who);
  return c3y;
}


/* u3_disk_save_meta_meta(): save meta metadata.
*/
c3_o
u3_disk_save_meta_meta(c3_c* log_c, const u3_meta* met_u)
{
  MDB_env* dbm_u;

  if ( 0 == (dbm_u = u3_lmdb_init(log_c, u3_Host.ops_u.siz_i)) ) {
    fprintf(stderr, "disk: failed to initialize meta-lmdb\r\n");
    return c3n;
  }

  if ( c3n == u3_disk_save_meta(dbm_u, met_u) ) {
    fprintf(stderr, "disk: failed to save metadata\r\n");
    return c3n;
  }

  u3_lmdb_exit(dbm_u);

  return c3y;
}


typedef struct {
  ssize_t hav_i;
  c3_y    buf_y[16];
} _mdb_val;


/* _disk_meta_read_cb(): copy [val_p] to atom [ptr_v] if present.
*/
static void
_disk_meta_read_cb(void* ptr_v, ssize_t val_i, void* val_v)
{
  _mdb_val* val_u = ptr_v;
  c3_y*     dat_y = (c3_y*)val_v;

  memset(val_u->buf_y, 0, sizeof(val_u->buf_y));
  val_u->hav_i = val_i;

  if ( 0 < val_i ) {
    memcpy(val_u->buf_y, dat_y, c3_min(val_i, sizeof(val_u->buf_y)));
  }
}

/* u3_disk_read_meta(): read metadata.
*/
c3_o
u3_disk_read_meta(MDB_env* mdb_u, u3_meta* met_u)
{
  c3_w ver_w, lif_w;
  c3_d who_d[2];
  c3_o fak_o;

  _mdb_val val_u;

  //  version
  //
  u3_lmdb_read_meta(mdb_u, &val_u, "version", _disk_meta_read_cb);

  ver_w = val_u.buf_y[0];

  //  identity
  //
  u3_lmdb_read_meta(mdb_u, &val_u, "who", _disk_meta_read_cb);

  if ( 0 > val_u.hav_i ) {
    fprintf(stderr, "disk: read meta: no identity\r\n");
    return c3n;
  }
  else if ( 16 < val_u.hav_i ) {
    //  NB: non-fatal
    //
    fprintf(stderr, "disk: read meta: strange identity\r\n");
  }

  c3_y* byt_y = val_u.buf_y;

  who_d[0] = (c3_d)byt_y[0]
           | (c3_d)byt_y[1] << 8
           | (c3_d)byt_y[2] << 16
           | (c3_d)byt_y[3] << 24
           | (c3_d)byt_y[4] << 32
           | (c3_d)byt_y[5] << 40
           | (c3_d)byt_y[6] << 48
           | (c3_d)byt_y[7] << 56;

  byt_y += 8;
  who_d[1] = (c3_d)byt_y[0]
           | (c3_d)byt_y[1] << 8
           | (c3_d)byt_y[2] << 16
           | (c3_d)byt_y[3] << 24
           | (c3_d)byt_y[4] << 32
           | (c3_d)byt_y[5] << 40
           | (c3_d)byt_y[6] << 48
           | (c3_d)byt_y[7] << 56;

  //  fake bit
  //
  u3_lmdb_read_meta(mdb_u, &val_u, "fake", _disk_meta_read_cb);

  if ( 0 > val_u.hav_i ) {
    fprintf(stderr, "disk: read meta: no fake bit\r\n");
    return c3n;
  }
  else if ( 0 == val_u.hav_i ) {
    fak_o = 0;
  }
  else if ( (1 == val_u.hav_i) || !((*val_u.buf_y) >> 1) ) {
    fak_o = (*val_u.buf_y) & 1;
  }
  else {
    fprintf(stderr, "disk: read meta: invalid fake bit %u %zd\r\n",
                    *val_u.buf_y, val_u.hav_i);
    return c3n;
  }

  //  life
  //
  u3_lmdb_read_meta(mdb_u, &val_u, "life", _disk_meta_read_cb);

  if ( 0 > val_u.hav_i ) {
    fprintf(stderr, "disk: read meta: no lifecycle length\r\n");
    return c3n;
  }
  else if ( 4 < val_u.hav_i ) {
    //  NB: non-fatal
    //
    fprintf(stderr, "disk: read meta: strange life\r\n");
  }

  byt_y = val_u.buf_y;
  lif_w = (c3_w)byt_y[0]
        | (c3_w)byt_y[1] << 8
        | (c3_w)byt_y[2] << 16
        | (c3_w)byt_y[3] << 24;

  {
    c3_o val_o = c3y;

    if ( U3D_VERLAT < ver_w ) {
      fprintf(stderr, "disk: read meta: unknown version %u\r\n", ver_w);
      val_o = c3n;
    }
    else if ( !((c3y == fak_o ) || (c3n == fak_o )) ) {
      fprintf(stderr, "disk: read meta: invalid fake bit\r\n");
      val_o = c3n;
    }
    else if ( c3n == u3a_is_cat(lif_w) ) {
      fprintf(stderr, "disk: read meta: invalid lifecycle length\r\n");
      val_o = c3n;
    }

    if ( c3n == val_o ) {
      return c3n;
    }
  }

  //  NB: we read metadata from LMDB even when met_u is null because sometimes
  //      because sometimes we call this just to ensure metadata exists
  if ( met_u ) {
    met_u->ver_w = ver_w;
    memcpy(met_u->who_d, who_d, 2 * sizeof(c3_d));
    met_u->fak_o = fak_o;
    met_u->lif_w = lif_w;
  }

  return c3y;
}

/* _disk_lock(): lockfile path.
*/
static c3_c*
_disk_lock(c3_c* pax_c)
{
  c3_w  len_w = strlen(pax_c) + sizeof("/.vere.lock");
  c3_c* paf_c = c3_malloc(len_w);
  c3_i  wit_i;

  wit_i = snprintf(paf_c, len_w, "%s/.vere.lock", pax_c);
  u3_assert(wit_i + 1 == len_w);
  return paf_c;
}

/* _disk_acquire(): acquire a lockfile, killing anything that holds it.
*/
static c3_i
_disk_acquire(c3_c* pax_c)
{
  c3_c* paf_c    = _disk_lock(pax_c);
  c3_y  dat_y[13] = {0};
  c3_w  pid_w    = 0;
  c3_i  fid_i, ret_i;

  if ( -1 == (fid_i = c3_open(paf_c, O_RDWR|O_CREAT, 0666)) ) {
    fprintf(stderr, "disk: failed to open/create lock file\r\n");
    goto fail;
  }

  {
    c3_y  len_y = 0;
    c3_y* buf_y = dat_y;

    do {
      c3_zs ret_zs;

      if ( -1 == (ret_zs = read(fid_i, buf_y, 1)) ) {
        if ( EINTR == errno ) continue;

        fprintf(stderr, "disk: failed to read lockfile: %s\r\n",
                        strerror(errno));
        goto fail;
      }

      if ( !ret_zs ) break;
      else if ( 1 != ret_zs ) {
        fprintf(stderr, "disk: strange lockfile read %zd\r\n", ret_zs);
        goto fail;
      }

      len_y++;
      buf_y++;
    }
    while ( len_y < (sizeof(dat_y) - 1) ); // null terminate


    if ( len_y ) {
      if (  (1 != sscanf((c3_c*)dat_y, "%" SCNu32 "%n", &pid_w, &ret_i))
         || (0 >= ret_i)
         || ('\n' != *(dat_y + ret_i)) )
      {
        fprintf(stderr, "disk: lockfile is corrupt\r\n");
      }
    }
  }

#ifndef U3_OS_windows
  {
    struct flock lok_u;
    memset((void *)&lok_u, 0, sizeof(lok_u));
    lok_u.l_type = F_WRLCK;
    lok_u.l_whence = SEEK_SET;
    lok_u.l_start = 0;
    lok_u.l_len = 1;

    while (  (ret_i = fcntl(fid_i, F_SETLK, &lok_u))
          && (EINTR == (ret_i = errno)) );

    if ( ret_i ) {
      if ( pid_w ) {
        fprintf(stderr, "pier: locked by PID %u\r\n", pid_w);
      }
      else {
        fprintf(stderr, "pier: strange: locked by empty lockfile\r\n");
      }

      goto fail;
    }
  }
#endif

  ret_i = snprintf((c3_c*)dat_y, sizeof(dat_y), "%u\n", getpid());

  if ( 0 >= ret_i ) {
    fprintf(stderr, "disk: failed to write lockfile\r\n");
    goto fail;
  }

  {
    c3_y  len_y = (c3_y)ret_i;
    c3_y* buf_y = dat_y;

    do {
      c3_zs ret_zs;

      if (  (-1 == (ret_zs = write(fid_i, buf_y, len_y)))
         && (EINTR != errno) )
      {
        fprintf(stderr, "disk: lockfile write failed %s\r\n",
                        strerror(errno));
        goto fail;
      }

      if ( 0 < ret_zs ) {
        len_y -= ret_zs;
        buf_y += ret_zs;
      }
    }
    while ( len_y );
  }

  if ( -1 == c3_sync(fid_i) ) {
    fprintf(stderr, "disk: failed to sync lockfile: %s\r\n",
                    strerror(errno));
    goto fail;
  }

  c3_free(paf_c);
  return fid_i;

fail:
  kill(getpid(), SIGTERM);
  sleep(1); u3_assert(0);
}

/* _disk_release(): release a lockfile.
*/
static void
_disk_release(c3_c* pax_c, c3_i fid_i)
{
  c3_c* paf_c = _disk_lock(pax_c);
  c3_unlink(paf_c);
  c3_free(paf_c);
  close(fid_i);
}

/* u3_disk_exit(): close the log.
*/
void
u3_disk_exit(u3_disk* log_u)
{
  //  try to cancel write thread
  //  shortcircuit cleanup if we cannot
  //
  if (  (c3y == log_u->sav_u.ted_o)
     && (0 > uv_cancel(&log_u->sav_u.req_u)) )
  {
    // u3l_log("disk: unable to cleanup\r\n");
    return;
  }

  //  close database
  //
  u3_lmdb_exit(log_u->mdb_u);

  //  dispose planned writes
  //
  {
    u3_feat* fet_u = log_u->put_u.ext_u;

    while ( fet_u && (fet_u->eve_d <= log_u->dun_d) ) {
      log_u->put_u.ext_u = fet_u->nex_u;
      c3_free(fet_u->hun_y);
      c3_free(fet_u);
      fet_u = log_u->put_u.ext_u;
    }
  }

  _disk_release(log_u->dir_u->pax_c, log_u->lok_i);

  u3_dire_free(log_u->dir_u);
  u3_dire_free(log_u->urb_u);
  u3_dire_free(log_u->com_u);

  c3_free(log_u);

#if defined(DISK_TRACE_JAM) || defined(DISK_TRACE_CUE)
  u3t_trace_close();
#endif
}

/* u3_disk_info(): status info as a (list mass).
*/
u3_noun
u3_disk_info(u3_disk* log_u)
{
  u3_noun lit = u3i_list(
    u3_pier_mase("live",        log_u->liv_o),
    u3_pier_mase("event", u3i_chub(log_u->dun_d)),
    u3_none);

  //  XX revise, include batches
  //
  if ( log_u->put_u.ext_u ) {
    lit = u3nc(
      u3_pier_mass(
        c3__save,
        u3i_list(
          u3_pier_mase("save-start", u3i_chub(log_u->put_u.ext_u->eve_d)),
          u3_pier_mase("save-final", u3i_chub(log_u->put_u.ent_u->eve_d)),
          u3_none)),
      lit);
  }

  return u3_pier_mass(c3__disk, lit);
}

/* u3_disk_slog(): print status info.
*/
void
u3_disk_slog(u3_disk* log_u)
{
  u3l_log("  disk: live=%s, event=%" PRIu64,
          ( c3y == log_u->liv_o ) ? "&" : "|",
          log_u->dun_d);

  {
    c3_w len_w, i_w;

    u3l_log("    batch:");

    for ( i_w = 0; i_w < 100; i_w++ ) {
      len_w = log_u->hit_w[i_w];
      if ( len_w ) {
        u3l_log("      %u: %u", i_w, len_w);
      }
    }
  }

  if ( log_u->put_u.ext_u ) {
    if ( log_u->put_u.ext_u != log_u->put_u.ent_u ) {
      u3l_log("    save: %" PRIu64 "-%" PRIu64,
              log_u->put_u.ext_u->eve_d,
              log_u->put_u.ent_u->eve_d);
    }
    else {
      u3l_log("    save: %" PRIu64, log_u->put_u.ext_u->eve_d);
    }
  }
}

/* _disk_epoc_meta: read metadata from epoch.
*/
static c3_o
_disk_epoc_meta(u3_disk*    log_u,
                c3_d        epo_d,
                const c3_c* met_c,
                c3_w        max_w,
                c3_c*       buf_c)
{
  struct stat buf_u;
  c3_w red_w, len_w;
  c3_i ret_i, fid_i;
  c3_c*       pat_c;

  ret_i = asprintf(&pat_c, "%s/0i%" PRIc3_d "/%s.txt",
                   log_u->com_u->pax_c, epo_d, met_c);
  u3_assert( ret_i > 0 );

  fid_i = c3_open(pat_c, O_RDONLY, 0644);
  c3_free(pat_c);

  if ( (fid_i < 0) || (fstat(fid_i, &buf_u) < 0) ) {
    fprintf(stderr, "disk: failed to open %s.txt in epoch 0i%" PRIc3_d "\r\n",
                    met_c, epo_d);
    return c3n;
  }
  else if ( buf_u.st_size >= max_w ) {
    fprintf(stderr, "disk: %s.txt in epoch 0i%" PRIc3_d " too large "
                    "(%" PRIc3_z ")\r\n",
                    met_c, epo_d, (c3_z)buf_u.st_size);
    return c3n;
  }

  len_w = buf_u.st_size;
  red_w = read(fid_i, buf_c, len_w);
  close(fid_i);

  if ( len_w != red_w ) {
    fprintf(stderr, "disk: failed to read %s.txt in epoch 0i%" PRIc3_d "\r\n",
                    met_c, epo_d);
    return c3n;
  }

  //  trim trailing whitespace
  //
  do {
    buf_c[len_w] = 0;
  }
  while ( len_w-- && isspace(buf_c[len_w]) );

  return c3y;
}

/* _disk_epoc_zero: make epoch zero.
*/
static c3_o
_disk_epoc_zero(c3_c* pax_c)
{
  //  create new epoch directory if it doesn't exist
  c3_c epo_c[8193];
  c3_i epo_i;
  snprintf(epo_c, sizeof(epo_c), "%s/0i0", pax_c);
  c3_d ret_d = c3_mkdir(epo_c, 0700);
  if ( ( ret_d < 0 ) && ( errno != EEXIST ) ) {
    fprintf(stderr, "disk: epoch 0i0 mkdir failed: %s\r\n", strerror(errno));
    return c3n;
  }

  //  open epoch directory
#ifndef U3_OS_windows
  if ( -1 == (epo_i = c3_open(epo_c, O_RDONLY)) ) {
    fprintf(stderr, "disk: open epoch dir 0i0 failed: %s\r\n", strerror(errno));
    goto fail1;
  }

  //  sync epoch directory
  if ( -1 == c3_sync(epo_i) ) {  //  XX fdatasync on linux?
    fprintf(stderr, "disk: sync epoch dir 0i0 failed: %s\r\n", strerror(errno));
    goto fail2;
  }
#endif

  //  create epoch version file, overwriting any existing file
  c3_c epi_c[8193];
  c3_c epv_c[8193];
  snprintf(epi_c, sizeof(epv_c), "%s/epoc.tmp", epo_c);
  snprintf(epv_c, sizeof(epv_c), "%s/epoc.txt", epo_c);
  FILE* epv_f = fopen(epi_c, "w");  // XX errors

  if (  !epv_f
     || (0 > fprintf(epv_f, "%d", U3E_VERLAT))
     || fflush(epv_f)
     || (-1 == c3_sync(fileno(epv_f)))
     || fclose(epv_f)
     || (-1 == rename(epi_c, epv_c)) )
  {
    fprintf(stderr, "disk: write epoc.txt failed %s\r\n", strerror(errno));
    goto fail3;
  }

  //  create binary version file, overwriting any existing file
  c3_c bii_c[8193];
  c3_c biv_c[8193];
  snprintf(bii_c, sizeof(biv_c), "%s/vere.tmp", epo_c);
  snprintf(biv_c, sizeof(epv_c), "%s/vere.txt", epo_c);
  FILE* biv_f = fopen(bii_c, "w");
  if (  !biv_f
     || (0 > fprintf(biv_f, URBIT_VERSION))
     || fflush(biv_f)
     || (-1 == c3_sync(fileno(biv_f)))
     || fclose(biv_f)
     || (-1 == rename(bii_c, biv_c)) )
  {
    fprintf(stderr, "disk: write vere.txt failed %s\r\n", strerror(errno));
    goto fail3;
  }

#ifndef U3_OS_windows
  if ( -1 == c3_sync(epo_i) ) {  //  XX fdatasync on linux?
    fprintf(stderr, "disk: sync epoch dir 0i0 failed: %s\r\n", strerror(errno));
    goto fail3;
  }
  close(epo_i);
#endif

  //  success
  return c3y;

 fail3:
  c3_unlink(epv_c);
  c3_unlink(biv_c);
#ifndef U3_OS_windows
 fail2:
  close(epo_i);
 fail1:
#endif
  c3_rmdir(epo_c);
  return c3n;
}

/* _disk_epoc_roll: epoch rollover.
*/
static c3_o
_disk_epoc_roll(u3_disk* log_u, c3_d epo_d)
{
  u3_assert(epo_d);

  fprintf(stderr, "disk: rolling to epoch %" PRIc3_d "\r\n", epo_d);
  //  check if any epoch directories exist
  c3_d lat_d;
  if ( c3n == u3_disk_epoc_last(log_u, &lat_d) ) {
    fprintf(stderr, "disk: failed to get last epoch\r\n");
    return c3n;
  }

  //  create new epoch directory if it doesn't exist
  c3_c epo_c[8193];
  c3_i epo_i;
  snprintf(epo_c, sizeof(epo_c), "%s/0i%" PRIc3_d, log_u->com_u->pax_c, epo_d);
  c3_d ret_d = c3_mkdir(epo_c, 0700);
  if ( ( ret_d < 0 ) && ( errno != EEXIST ) ) {
    fprintf(stderr, "disk: create epoch dir %" PRIc3_d " failed: %s\r\n",
                    epo_d, strerror(errno));
    return c3n;
  }

  //  copy snapshot files (skip if first epoch)
  c3_c chk_c[8193];
  snprintf(chk_c, 8192, "%s/.urb/chk", log_u->dir_u->pax_c);
  if ( c3n == u3e_backup(chk_c, epo_c, c3y) ) {
    fprintf(stderr, "disk: copy epoch snapshot failed\r\n");
    goto fail1;
  }

#ifndef U3_OS_windows
  if ( -1 == (epo_i = c3_open(epo_c, O_RDONLY)) ) {
    fprintf(stderr, "disk: open epoch dir %" PRIc3_d " failed: %s\r\n",
                    epo_d, strerror(errno));
    goto fail1;
  }

  if ( -1 == c3_sync(epo_i) ) {  //  XX fdatasync on linux?
    fprintf(stderr, "disk: sync epoch dir %" PRIc3_d " failed: %s\r\n",
                    epo_d, strerror(errno));
    goto fail2;
  }
#endif

  //  create epoch version file, overwriting any existing file
  c3_c epi_c[8193];
  c3_c epv_c[8193];
  snprintf(epi_c, sizeof(epv_c), "%s/epoc.tmp", epo_c);
  snprintf(epv_c, sizeof(epv_c), "%s/epoc.txt", epo_c);
  FILE* epv_f = fopen(epi_c, "w");

  if (  !epv_f
     || (0 > fprintf(epv_f, "%d", U3E_VERLAT))
     || fflush(epv_f)
     || (-1 == c3_sync(fileno(epv_f)))
     || fclose(epv_f)
     || (-1 == rename(epi_c, epv_c)) )
  {
    fprintf(stderr, "disk: write epoc.txt failed %s\r\n", strerror(errno));
    goto fail3;
  }

  //  create binary version file, overwriting any existing file
  c3_c bii_c[8193];
  c3_c biv_c[8193];
  snprintf(bii_c, sizeof(biv_c), "%s/vere.tmp", epo_c);
  snprintf(biv_c, sizeof(epv_c), "%s/vere.txt", epo_c);
  FILE* biv_f = fopen(bii_c, "w");
  if (  !biv_f
     || (0 > fprintf(biv_f, URBIT_VERSION))
     || fflush(biv_f)
     || (-1 == c3_sync(fileno(biv_f)))
     || fclose(biv_f)
     || (-1 == rename(bii_c, biv_c)) )
  {
    fprintf(stderr, "disk: write vere.txt failed %s\r\n", strerror(errno));
    goto fail3;
  }

#ifndef U3_OS_windows
  if ( -1 == c3_sync(epo_i) ) {  //  XX fdatasync on linux?
    fprintf(stderr, "disk: sync epoch dir 0i0 failed: %s\r\n", strerror(errno));
    goto fail3;
  }
#endif

  //  get metadata from old log, update version
  u3_meta old_u;
  if ( c3y != u3_disk_read_meta(log_u->mdb_u, &old_u) ) {
    fprintf(stderr, "disk: failed to read metadata\r\n");
    goto fail3;
  }
  u3_lmdb_exit(log_u->mdb_u);
  log_u->mdb_u = 0;

  //  initialize db of new epoch
  if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c, u3_Host.ops_u.siz_i)) ) {
    fprintf(stderr, "disk: failed to initialize database\r\n");
    c3_free(log_u);
    goto fail3;
  }

  // write the metadata to the database
  old_u.ver_w = U3D_VERLAT;
  if ( c3n == u3_disk_save_meta(log_u->mdb_u, &old_u) ) {
    fprintf(stderr, "disk: failed to save metadata\r\n");
    goto fail3;
  }

#ifndef U3_OS_windows
  if ( -1 == c3_sync(epo_i) ) {  //  XX fdatasync on linux?
    fprintf(stderr, "disk: sync epoch dir %" PRIc3_d " failed: %s\r\n",
                    epo_d, strerror(errno));
    goto fail3;
  }

  close(epo_i);
#endif

  fprintf(stderr, "disk: created epoch %" PRIc3_d "\r\n", epo_d);

  //  load new epoch directory and set it in log_u
  log_u->epo_d = epo_d;
  log_u->ver_w = U3D_VERLAT;

  //  success
  return c3y;

fail3:
  c3_unlink(epv_c);
  c3_unlink(biv_c);
#ifndef U3_OS_windows
fail2:
  close(epo_i);
#endif
fail1:
  c3_rmdir(epo_c);
  return c3n;
}

/* _disk_epoc_kill: delete an epoch.
*/
static c3_o
_disk_epoc_kill(u3_disk* log_u, c3_d epo_d)
{
  //  get epoch directory
  c3_c epo_c[8193];
  snprintf(epo_c, sizeof(epo_c), "%s/0i%" PRIc3_d, log_u->com_u->pax_c, epo_d);

  //  delete files in epoch directory
  u3_dire* dir_u = u3_foil_folder(epo_c);
  u3_dent* den_u = dir_u->all_u;
  while ( den_u ) {
    c3_c fil_c[8193];
    snprintf(fil_c, sizeof(fil_c), "%s/%s", epo_c, den_u->nam_c);
    if ( 0 != c3_unlink(fil_c) ) {
      fprintf(stderr, "disk: failed to delete file in epoch directory\r\n");
      return c3n;
    }
    den_u = den_u->nex_u;
  }

  //  delete epoch directory
  if ( 0 != c3_rmdir(epo_c) ) {
    fprintf(stderr, "disk: failed to delete epoch directory\r\n");
    return c3n;
  }

  //  cleanup
  u3_dire_free(dir_u);

  //  success
  return c3y;
}

/* u3_disk_epoc_last: get latest epoch number.
*/
c3_o
u3_disk_epoc_last(u3_disk* log_u, c3_d* lat_d)
{
  u3_dire* die_u = u3_foil_folder(log_u->com_u->pax_c);
  u3_dent* den_u = die_u->dil_u;
  c3_o     ret_o = c3n;
  c3_d     epo_d = 0;
  c3_d     val_d;
  c3_i     car_i;

  while ( den_u ) {
    if (  (1 == sscanf(den_u->nam_c, "0i%" SCNu64 "%n", &val_d, &car_i))
       && (0 < car_i)
       && ('\0' == *(den_u->nam_c + car_i)) )
    {
      ret_o = c3y;   //  NB: returns yes if the directory merely exists
      epo_d = c3_max(epo_d, val_d);
    }

    den_u = den_u->nex_u;
  }

  u3_dire_free(die_u);

  *lat_d = epo_d;

  return ret_o;
}

/* u3_disk_epoc_list: get descending epoch numbers, "mcut" pattern.
*/
c3_z
u3_disk_epoc_list(u3_disk* log_u, c3_d* sot_d)
{
  u3_dire* ned_u = u3_foil_folder(log_u->com_u->pax_c);
  u3_dent* den_u = ned_u->dil_u;
  c3_z     len_z = 0;
  c3_i     car_i;
  c3_d     val_d;

  while ( den_u ) {  //  count epochs
    if (  (1 == sscanf(den_u->nam_c, "0i%" SCNu64 "%n", &val_d, &car_i))
       && (0 < car_i)
       && ('\0' == *(den_u->nam_c + car_i)) )
    {
      len_z++;
    }

    den_u = den_u->nex_u;
  }

  if ( !sot_d ) {
    u3_dire_free(ned_u);
    return len_z;
  }

  len_z = 0;
  den_u = ned_u->dil_u;

  while ( den_u ) {
    if (  (1 == sscanf(den_u->nam_c, "0i%" SCNu64 "%n", &val_d, &car_i))
       && (0 < car_i)
       && ('\0' == *(den_u->nam_c + car_i)) )
    {
      sot_d[len_z++] = val_d;
    }

    den_u = den_u->nex_u;
  }

  //  sort sot_d naively in descending order
  //
  c3_d tmp_d;
  for ( c3_z i_z = 0; i_z < len_z; i_z++ ) {
    for ( c3_z j_z = i_z + 1; j_z < len_z; j_z++ ) {
      if ( sot_d[i_z] < sot_d[j_z] ) {
        tmp_d = sot_d[i_z];
        sot_d[i_z] = sot_d[j_z];
        sot_d[j_z] = tmp_d;
      }
    }
  }

  u3_dire_free(ned_u);
  return len_z;
}

/* _disk_migrate_epoc: migrates disk format.
 */
static c3_o
_disk_migrate_epoc(u3_disk* log_u, c3_d eve_d)
{
  /*  migration steps:
   *  1. initialize epoch 0i0 (see _disk_epoc_zero)
   *  2. create hard links to data.mdb and lock.mdb in 0i0/
   *  3. use scratch space to initialize new log/data.db in log/tmp
   *  4. save old metadata to new db in scratch space
   *  5. clobber old log/data.mdb with new log/tmp/data.mdb
   *  6. open epoch lmdb and set it in log_u
   */
  
  //  NB: requires that log_u->mdb_u is initialized to log/data.mdb
  //  XX: put old log in separate pointer (old_u?)?

  //  get metadata from old log, update version
  u3_meta olm_u;
  if ( c3y != u3_disk_read_meta(log_u->mdb_u, &olm_u) ) {
    fprintf(stderr, "disk: failed to read metadata\r\n");
    return c3n;
  }

  //  finish with old log
  u3_lmdb_exit(log_u->mdb_u);
  log_u->mdb_u = 0;

  //  check if lock.mdb is readable in log directory
  c3_o luk_o = c3n;
  c3_c luk_c[8193];
  snprintf(luk_c, sizeof(luk_c), "%s/lock.mdb", log_u->com_u->pax_c);
  if ( 0 == access(luk_c, R_OK) ) {
    luk_o = c3y;
  }

  fprintf(stderr, "disk: migrating disk to v%d format\r\n", U3D_VERLAT);

  //  initialize first epoch "0i0"
  if ( c3n == _disk_epoc_zero(log_u->com_u->pax_c) ) {
    fprintf(stderr, "disk: failed to initialize first epoch\r\n");
    return c3n;
  }

  //  create hard links to data.mdb and lock.mdb in 0i0/
  c3_c epo_c[8193], dut_c[8193], dat_c[8193], lok_c[8193];
  snprintf(epo_c, sizeof(epo_c), "%s/0i0", log_u->com_u->pax_c);
  snprintf(dut_c, sizeof(dut_c), "%s/data.mdb", log_u->com_u->pax_c);
  snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", epo_c);
  snprintf(lok_c, sizeof(lok_c), "%s/lock.mdb", epo_c);

  if ( c3_link(dut_c, dat_c) && (EEXIST != errno) ) {
    fprintf(stderr, "disk: failed to create data.mdb hard link\r\n");
    return c3n;
  }
  if ( c3y == luk_o ) {  //  only link lock.mdb if it exists
    if ( c3_link(luk_c, lok_c) && (EEXIST != errno) ) {
      fprintf(stderr, "disk: failed to create lock.mdb hard link\r\n");
      return c3n;
    }
  }

  //  delete backup snapshot
  c3_c bhk_c[8193], nop_c[8193], sop_c[8193];
  snprintf(bhk_c, sizeof(bhk_c), "%s/.urb/bhk", log_u->dir_u->pax_c);
  snprintf(nop_c, sizeof(nop_c), "%s/north.bin", bhk_c);
  snprintf(sop_c, sizeof(sop_c), "%s/south.bin", bhk_c);
  if ( c3_unlink(nop_c) ) {
    fprintf(stderr, "disk: failed to delete bhk/north.bin: %s\r\n",
                    strerror(errno));
  }
  else if ( c3_unlink(sop_c) ) {
    fprintf(stderr, "disk: failed to delete bhk/south.bin: %s\r\n",
                    strerror(errno));
  }
  else {
    if ( c3_rmdir(bhk_c) ) {
      fprintf(stderr, "disk: failed to delete bhk/: %s\r\n",
                      strerror(errno));
    }
  }

  //  use scratch space to initialize new log/data.db
  //  and clobber old log/data.db
  //
  c3_c tmp_c[8193];
  snprintf(tmp_c, sizeof(tmp_c), "%s/tmp", log_u->com_u->pax_c);
  if ( c3_mkdir(tmp_c, 0700) && (errno != EEXIST) ) {
    fprintf(stderr, "disk: failed to create log/tmp: %s\r\n",
                    strerror(errno));
    return c3n;
  }

  if ( 0 == (log_u->mdb_u = u3_lmdb_init(tmp_c, u3_Host.ops_u.siz_i)) ) {
    fprintf(stderr, "disk: failed to initialize database at %s\r\n",
                    tmp_c);
    return c3n;
  }

  olm_u.ver_w = U3D_VERLAT;
  if ( c3n == u3_disk_save_meta(log_u->mdb_u, &olm_u) ) {
    fprintf(stderr, "disk: failed to save metadata\r\n");
    return c3n;
  }
  
  //  atomic truncation of old log
  //
  u3_lmdb_exit(log_u->mdb_u);
  log_u->mdb_u = 0;

  c3_c trd_c[8193];
  snprintf(trd_c, sizeof(trd_c), "%s/data.mdb", tmp_c);
  if ( c3_rename(trd_c, dut_c) ) {
    fprintf(stderr, "disk: failed mv %s to %s: %s\r\n", trd_c, dut_c,
                    strerror(errno));
    return c3n;
  }

  snprintf(trd_c, sizeof(trd_c), "%s/lock.mdb", tmp_c);
  if ( c3_unlink(trd_c) && (errno != ENOENT) ) {
    fprintf(stderr, "disk: failed to delete log/tmp: %s\r\n",
                    strerror(errno));
  }
  if ( c3_rmdir(tmp_c) ) {
    fprintf(stderr, "disk: failed to delete log/tmp: %s\r\n",
                    strerror(errno));
  }

  if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c, u3_Host.ops_u.siz_i)) ) {
    fprintf(stderr, "disk: failed to initialize database at %s\r\n",
                    epo_c);
    return c3n;
  }

  //  success
  fprintf(stderr, "disk: migrated disk to v%d format\r\n", U3D_VERLAT);

  return c3y;
}

/* _disk_vere_diff(): checks if vere version mismatches latest epoch's.
*/
static c3_o
_disk_vere_diff(u3_disk* log_u)
{
  c3_c ver_c[128];

  if ( c3n == _disk_epoc_meta(log_u, log_u->epo_d, "vere",
                             sizeof(ver_c) - 1, ver_c) )
  {
    return c3y; // assume mismatch if we can't read version
  }

  if ( 0 != strcmp(ver_c, URBIT_VERSION) ) {
    return c3y;
  }

  return c3n;
}

/* u3_disk_chop(): delete all but the latest 2 epocs.
*/
void
u3_disk_chop(u3_disk* log_u, c3_d eve_d)
{
  c3_z  len_z = u3_disk_epoc_list(log_u, 0);
  c3_d* sot_d = c3_malloc(len_z * sizeof(c3_d));
  u3_disk_epoc_list(log_u, sot_d);

  if ( len_z <= 2 ) {
    fprintf(stderr, "chop: nothing to do, try running roll first\r\n"
                    "chop: for more info see "
                    "https://docs.urbit.org/user-manual/running/vere#chop\r\n");
    exit(0);  //  enjoy
  }

  //  delete all but the last two epochs
  //
  //    XX parameterize the number of epochs to chop
  //
  for ( c3_z i_z = 2; i_z < len_z; i_z++ ) {
    fprintf(stderr, "chop: deleting epoch 0i%" PRIu64 "\r\n",
                    sot_d[i_z]);
    if ( c3y != _disk_epoc_kill(log_u, sot_d[i_z]) ) {
      fprintf(stderr, "chop: failed to delete epoch 0i%" PRIu64 "\r\n", sot_d[i_z]);
      exit(1);
    }
  }

  // cleanup
  c3_free(sot_d);

  // success
  fprintf(stderr, "chop: event log truncation complete\r\n");
}

/* u3_disk_roll(): rollover to a new epoc.
*/
void
u3_disk_roll(u3_disk* log_u, c3_d eve_d)
{
  //  XX get fir_d from log_u
  c3_d fir_d, las_d;

  if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
    fprintf(stderr, "roll: failed to read first/last event numbers\r\n");
    exit(1);
  }

  if ( fir_d == las_d ) {
    fprintf(stderr, "roll: latest epoch is empty\r\n");
    exit(0);
  }

  if ( (eve_d != las_d) || (eve_d != log_u->dun_d) ) {
    fprintf(stderr, "roll: shenanigans!\r\n");
    exit(1);
  }

  else if ( c3n == _disk_epoc_roll(log_u, eve_d) ) {
    fprintf(stderr, "roll: failed to create new epoch\r\n");
    exit(1);
  }
}

static void
_disk_unlink_stale_loom(c3_c* dir_c)
{
  c3_c bin_c[8193];
  snprintf(bin_c, 8193, "%s/.urb/chk/north.bin", dir_c);

  if ( c3_unlink(bin_c) && (ENOENT != errno) ) {
    fprintf(stderr, "mars: failed to unlink %s: %s\r\n",
                    bin_c, strerror(errno));
    exit(1);
  }

  snprintf(bin_c, 8193, "%s/.urb/chk/south.bin", dir_c);

  if ( c3_unlink(bin_c) && (ENOENT != errno) ) {
    fprintf(stderr, "mars: failed to unlink %s: %s\r\n",
                    bin_c, strerror(errno));
    exit(1);
  }
}

static c3_i
_disk_load_stale_loom(c3_c* dir_c, c3_z len_z)
{
  // map at fixed address.
  //
  {
    void* map_v = mmap((void *)u3_Loom_v4,
                       len_z,
                       (PROT_READ | PROT_WRITE),
                       (MAP_ANON | MAP_FIXED | MAP_PRIVATE),
                       -1, 0);

    if ( -1 == (c3_ps)map_v ) {
      map_v = mmap((void *)0,
                   len_z,
                   (PROT_READ | PROT_WRITE),
                   (MAP_ANON | MAP_PRIVATE),
                   -1, 0);

      u3l_log("boot: mapping %zuMB failed", len_z >> 20);
      u3l_log("see https://docs.urbit.org/manual/getting-started/self-hosted/cloud-hosting"
              " for adding swap space");
      if ( -1 != (c3_ps)map_v ) {
        u3l_log("if porting to a new platform, try U3_OS_LoomBase %p",
                map_v);
      }
      exit(1);
    }

    u3C.wor_i = len_z >> 2;
    u3l_log("loom: mapped %zuMB", len_z >> 20);
  }

  {
    c3_z lom_z;
    c3_i nod_i = u3e_image_open_any("/.urb/chk/north", dir_c, &lom_z);

    u3_assert( -1 != nod_i );

    fprintf(stderr, "loom: %p fid_i %d len %zu\r\n", u3_Loom_v4, nod_i, lom_z);

    //  XX respect --no-demand flag
    //
    if ( MAP_FAILED == mmap(u3_Loom_v4,
                            lom_z,
                            (PROT_READ | PROT_WRITE),
                            (MAP_FIXED | MAP_PRIVATE),
                            nod_i, 0) )
    {
      fprintf(stderr, "loom: file-backed mmap failed: %s\r\n",
                      strerror(errno));
      u3_assert(0);
    }

    const c3_z pag_z = 1U << (u3a_page + 2);
    void*      ptr_v = (c3_y*)u3_Loom_v4 + (len_z - pag_z);
    c3_zs     ret_zs;
    c3_i       sod_i = u3e_image_open_any("/.urb/chk/south", dir_c, &lom_z);

    u3_assert( -1 != nod_i );
    u3_assert( pag_z == lom_z );

    if ( pag_z != (ret_zs = pread(sod_i, ptr_v, pag_z, 0)) ) {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: blit south partial read: %"PRIc3_zs"\r\n",
                        ret_zs);
      }
      else {
        fprintf(stderr, "loom: blit south read: %s\r\n", strerror(errno));
      }
      u3_assert(0);
    }

    close(sod_i);

    return nod_i;
  }
}

static void
_disk_migrate_loom(c3_c* dir_c, c3_d eve_d)
{
  c3_i fid_i = _disk_load_stale_loom(dir_c, (size_t)1 << u3_Host.ops_u.lom_y); // XX confirm
  c3_w lom_w = *(u3_Loom_v4 + u3C.wor_i - 1);

  //  NB: all fallthru, all the time
  //
  switch ( lom_w ) {
    case U3V_VER1: u3_migrate_v2(eve_d);
    case U3V_VER2: u3_migrate_v3(eve_d);
    case U3V_VER3: u3_migrate_v4(eve_d);
    case U3V_VER4: {
      u3m_init((size_t)1 << u3_Host.ops_u.lom_y);
      u3e_live(c3n, strdup(dir_c));
      u3m_pave(c3y);
      u3_migrate_v5(eve_d);
      u3m_save();
    }
  }

  munmap(u3_Loom_v4, (size_t)1 << u3_Host.ops_u.lom_y);
  close(fid_i);
}

static void
_disk_migrate_old(u3_disk* log_u)
{
  c3_d fir_d, las_d;
  if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
    fprintf(stderr, "disk: failed to get first/last event numbers\r\n");
    exit(1);
  }

  log_u->sen_d = log_u->dun_d = las_d;

  switch ( log_u->ver_w ) {
    case U3D_VER1: {
      _disk_migrate_loom(log_u->dir_u->pax_c, las_d);

      //  set version to 2 (migration in progress)
      log_u->ver_w = U3D_VER2;
      if ( c3n == _disk_save_meta(log_u->mdb_u, "version", 4, (c3_y*)&log_u->ver_w) ) {
        fprintf(stderr, "disk: failed to set version to 2\r\n");
        exit(1);
      }
    }  // fallthru

    case U3D_VER2: {
      _disk_unlink_stale_loom(log_u->dir_u->pax_c);
      u3m_boot(log_u->dir_u->pax_c, (size_t)1 << u3_Host.ops_u.lom_y); // XX confirm

      if ( c3n == _disk_migrate_epoc(log_u, las_d) ) {
        fprintf(stderr, "disk: failed to migrate event log\r\n");
        exit(1);
      }

      if ( c3n == _disk_epoc_roll(log_u, las_d) ) {
        fprintf(stderr, "disk: failed to initialize epoch\r\n");
        exit(1);
      }
    } break;

    default: {
      fprintf(stderr, "disk: unknown old log version: %d\r\n", log_u->ver_w);
      u3_assert(0);
    }
  }
}

typedef enum {
  _epoc_good = 0,  // load successfully
  _epoc_gone = 1,  // version missing, total failure
  _epoc_fail = 2,  // transient failure (?)
  _epoc_void = 3,  // empty event log (cheaper to recover?)
  _epoc_late = 4   // format from the future
} _epoc_kind;

/* _disk_epoc_load(): load existing epoch, enumerating failures
*/
static _epoc_kind
_disk_epoc_load(u3_disk* log_u, c3_d lat_d, u3_disk_load_e lod_e)
{
  //  check latest epoc version
  //
  c3_w ver_w;
  {
    c3_c ver_c[8];
    c3_i car_i;

    if ( c3n == _disk_epoc_meta(log_u, lat_d, "epoc",
                                sizeof(ver_c) - 1, ver_c) )
    {
      fprintf(stderr, "disk: failed to load epoch 0i%" PRIc3_d " version\r\n",
                      lat_d);

      return _epoc_gone;
    }

    if ( !(  (1 == sscanf(ver_c, "%" SCNu32 "%n", &ver_w, &car_i))
          && (0 < car_i)
          && ('\0' == *(ver_c + car_i)) ) )
    {
      fprintf(stderr, "disk: failed to parse epoch version: '%s'\r\n", ver_c);
      return _epoc_fail;
    }

    if ( (U3E_VER1 > ver_w) || (U3E_VERLAT < ver_w) ) {
      fprintf(stderr, "disk: unknown epoch version: '%s', expected '%d' - '%d'\r\n",
                      ver_c, U3E_VER1, U3E_VERLAT);
      return _epoc_late;
    }
  }

  //  set path to latest epoch
  c3_c epo_c[8193];
  snprintf(epo_c, 8192, "%s/0i%" PRIc3_d, log_u->com_u->pax_c, lat_d);

  //  initialize latest epoch's db
  if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c, u3_Host.ops_u.siz_i)) ) {
    fprintf(stderr, "disk: failed to initialize database at %s\r\n",
                    epo_c);
    return _epoc_fail;
  }

  fprintf(stderr, "disk: loaded epoch 0i%" PRIc3_d "\r\n", lat_d);

  //  get first/last event numbers from lmdb
  c3_d fir_d, las_d;
  if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
    fprintf(stderr, "disk: failed to get first/last event numbers\r\n");
    u3_lmdb_exit(log_u->mdb_u);
    log_u->mdb_u = 0;
    return _epoc_fail;
  }

  if (  (u3_dlod_boot != lod_e)
     && !fir_d
     && !las_d
     && (c3n == u3_disk_read_meta(log_u->mdb_u, 0)) )
  {
    u3_lmdb_exit(log_u->mdb_u);
    log_u->mdb_u = 0;
    return _epoc_void;
  }

  //  initialize dun_d/sen_d values
  //
  //    XX save [fir_d] in struct, check on play
  //
  log_u->dun_d = ( 0 != las_d ) ? las_d : lat_d;
  log_u->sen_d = log_u->dun_d;
  log_u->epo_d = lat_d;

  //  NB: by virtue of getting here, we know the *pier* version number is at least 3
  //  (ie, this is the epoch system)
  //
  switch ( ver_w ) {
    case U3E_VER1: {
      if ( u3_dlod_epoc == lod_e ) {
        fprintf(stderr, "migration required, replay disallowed\r\n");
        exit(1);
      }

      _disk_migrate_loom(log_u->dir_u->pax_c, log_u->dun_d);
      u3m_stop();
      u3m_boot(log_u->dir_u->pax_c, (size_t)1 << u3_Host.ops_u.lom_y); // XX confirm

      if ( c3n == _disk_epoc_roll(log_u, log_u->dun_d) ) {
        fprintf(stderr, "disk: failed to initialize epoch during loom migration\r\n");
        exit(1);
      }

      _disk_unlink_stale_loom(log_u->dir_u->pax_c);
      return _epoc_good;
    } break;

    case U3E_VER2: {
      if ( u3_dlod_epoc == lod_e ) {
        c3_c chk_c[8193];
        snprintf(chk_c, 8193, "%s/.urb/chk", log_u->dir_u->pax_c);

        if ( 0 == log_u->epo_d ) {
          //  if epoch 0 is the latest, delete the snapshot files in chk/
          c3_c bin_c[8193];
          snprintf(bin_c, 8193, "%s/image.bin", chk_c);
          if ( c3_unlink(bin_c) && (ENOENT != errno) ) {
            fprintf(stderr, "mars: failed to unlink %s: %s\r\n",
                            bin_c, strerror(errno));
            exit(1);
          }
        }
        else if ( 0 != u3e_backup(epo_c, chk_c, c3y) ) {
          //  copy the latest epoch's snapshot files into chk/
          fprintf(stderr, "mars: failed to copy snapshot\r\n");
          exit(1);
        }
      }

      u3m_boot(log_u->dir_u->pax_c, (size_t)1 << u3_Host.ops_u.lom_y); // XX confirm

      if ( log_u->dun_d < u3A->eve_d ) {
        //  XX bad, add to enum
        fprintf(stderr, "mars: corrupt pier, snapshot (%" PRIu64
                        ") from future (log=%" PRIu64 ")\r\n",
                        u3A->eve_d, log_u->dun_d);
        exit(1);
      }
      else if ( u3A->eve_d < log_u->epo_d ) {
        //  XX goto full replay
        fprintf(stderr, "mars: corrupt pier, snapshot (%" PRIu64
                        ") out of epoch (%" PRIu64 ")\r\n",
                        u3A->eve_d, log_u->epo_d);
        exit(1);
      }

      if (  (u3C.wag_w & u3o_yolo)  // XX better argument to disable autoroll
         || (!log_u->epo_d && log_u->dun_d && !u3A->eve_d)
         || (c3n == _disk_vere_diff(log_u)) )
      {
        return _epoc_good;
      }
      else if ( log_u->dun_d != u3A->eve_d ) {
        //  XX stale snapshot, new binary, error out
        //  XX bad, add to enum
        fprintf(stderr, "stale snapshot, downgrade runtime to replay\r\n");
        exit(1);
      }
      else if ( c3n == _disk_epoc_roll(log_u, log_u->dun_d) ) {
        fprintf(stderr, "disk: failed to initialize epoch\r\n");
        exit(1);
      }

      return _epoc_good;
    } break;

    default: u3_assert(0);
  }

  u3_assert(!"unreachable");
}

/* u3_disk_make(): make pier directories.
*/
c3_o
u3_disk_make(c3_c* pax_c)
{
  //  make pier directory
  //
  if ( -1 == c3_mkdir(pax_c, 0700) ) {
    fprintf(stderr, "disk: failed to make pier at %s: %s\r\n",
                    pax_c, strerror(errno));
    return c3n;
  }

  //  make $pier/.urb
  {
    c3_c* urb_c = c3_malloc(6 + strlen(pax_c));

    strcpy(urb_c, pax_c);
    strcat(urb_c, "/.urb");

    if ( -1 == c3_mkdir(urb_c, 0700) ) {
      fprintf(stderr, "disk: failed to make /.urb in %s: %s\r\n",
                      pax_c, strerror(errno));
      c3_free(urb_c);
      return c3n;
    }

    c3_free(urb_c);
  }

  //  make $pier/.urb/put and $pier/.urb/get
  //
  {
    c3_c* dir_c = c3_malloc(10 + strlen(pax_c));

    strcpy(dir_c, pax_c);
    strcat(dir_c, "/.urb/put");
    c3_mkdir(dir_c, 0700);

    strcpy(dir_c, pax_c);
    strcat(dir_c, "/.urb/get");
    c3_mkdir(dir_c, 0700);

    c3_free(dir_c);
  }

  //  make $pier/.urb/log
  //
  {
    c3_c log_c[8193];
    snprintf(log_c, sizeof(log_c), "%s/.urb/log", pax_c);

    if ( -1 == c3_mkdir(log_c, 0700) ) {
      fprintf(stderr, "disk: failed to make /.urb/log in %s: %s\r\n",
                      pax_c, strerror(errno));
      return c3n;
    }

    //  make epoch zero
    //
    if ( c3n == _disk_epoc_zero(log_c) ) {
      fprintf(stderr, "disk: failed to make epoch zero\r\n");
      return c3n;
    }
  }

  return c3y;
}

static u3_dire*
_disk_require_dir(const c3_c* dir_c)
{
  struct stat dir_u;

  if ( stat(dir_c, &dir_u) || !S_ISDIR(dir_u.st_mode) ) {
    return 0;
  }

  return u3_dire_init(dir_c);
}

/* u3_disk_load(): load pier directories, log, and snapshot.
*/
u3_disk*
u3_disk_load(c3_c* pax_c, u3_disk_load_e lod_e)
{
  u3_disk* log_u = c3_calloc(sizeof(*log_u));
  log_u->lok_i = -1;
  log_u->liv_o = c3n;
  log_u->sav_u.ted_o = c3n;
  log_u->sav_u.ted_u.data = log_u;
  log_u->put_u.ent_u = log_u->put_u.ext_u = 0;

  //  load pier directory
  //
  if ( 0 == (log_u->dir_u = _disk_require_dir(pax_c)) ) {
    fprintf(stderr, "disk: failed to load pier at %s\r\n", pax_c);
    c3_free(log_u);
    return 0;
  }

  //  acquire lockfile.
  //
  log_u->lok_i = _disk_acquire(pax_c);

  //  load $pier/.urb
  //
  {
    c3_c* urb_c = c3_malloc(6 + strlen(pax_c));

    strcpy(urb_c, pax_c);
    strcat(urb_c, "/.urb");

    if ( 0 == (log_u->urb_u = _disk_require_dir(urb_c)) ) {
      fprintf(stderr, "disk: failed to load /.urb in %s\r\n", pax_c);
      c3_free(urb_c);
      c3_free(log_u); // XX leaks dire(s)
      return 0;
    }
    c3_free(urb_c);
  }

  //  load $pier/.urb/log
  //
  {
    c3_c log_c[8193];
    snprintf(log_c, sizeof(log_c), "%s/.urb/log", pax_c);

    if ( 0 == (log_u->com_u = _disk_require_dir(log_c)) ) {
      fprintf(stderr, "disk: failed to load /.urb/log in %s\r\n", pax_c);
      c3_free(log_u); // XX leaks dire(s)
      return 0;
    }

    //  XX move this into u3_disk_make
    //
    if ( u3_dlod_boot == lod_e ) {
      if ( _epoc_good != _disk_epoc_load(log_u, 0, lod_e) ) {
        fprintf(stderr, "disk: failed to initialize lmdb\r\n");
        c3_free(log_u); // XX leaks dire(s)
        return 0;
      }

      log_u->liv_o = c3y;
      return log_u;
    }

    //  read metadata (version) from old log / top-level
    //
    {
      u3_meta met_u;
      if (  (0 == (log_u->mdb_u = u3_lmdb_init(log_c, u3_Host.ops_u.siz_i)))
         || (c3n == u3_disk_read_meta(log_u->mdb_u, &met_u)) )
      {
        fprintf(stderr, "disk: failed to read metadata\r\n");
        c3_free(log_u); // XX leaks dire(s)
        return 0;
      }
      log_u->ver_w = met_u.ver_w;
    }

    if ( U3D_VERLAT < log_u->ver_w ) {
      fprintf(stderr, "disk: unknown log version: %d\r\n", log_u->ver_w);
      c3_free(log_u); // XX leaks dire(s)
      return 0;
    }
    else if ( U3D_VERLAT > log_u->ver_w ) {
      if ( u3_dlod_epoc == lod_e ) {
        fprintf(stderr, "migration required, replay disallowed\r\n");
        exit(1);
      }
      _disk_migrate_old(log_u);
      log_u->liv_o = c3y;
      return log_u;
    }

    //  close top-level lmdb
    u3_lmdb_exit(log_u->mdb_u);
    log_u->mdb_u = 0;

    //  get latest epoch number
    c3_d lat_d;
    if ( c3n == u3_disk_epoc_last(log_u, &lat_d) ) {
      fprintf(stderr, "disk: failed to load epoch number\r\n");
      c3_free(log_u); // XX leaks dire(s)
      return 0;
    }

    c3_o try_o = c3n;

try_init:
    {
      _epoc_kind kin_e = _disk_epoc_load(log_u, lat_d, lod_e);

      switch ( kin_e ) {
        case _epoc_good: {
#if defined(DISK_TRACE_JAM) || defined(DISK_TRACE_CUE)
          u3t_trace_open(pax_c);
#endif

          log_u->liv_o = c3y;
          return log_u;
        } break;

        case _epoc_void: // XX could handle more efficiently

        case _epoc_gone: {
          // XX if there is no version number, the epoc is invalid
          // backup and try previous

          if ( c3y == try_o ) {
            fprintf(stderr, "multiple bad epochs, bailing out\r\n");
            c3_free(log_u); // XX leaks dire(s)
            return 0;
          }

          c3_z dir_z  = u3_disk_epoc_list(log_u, 0);
          c3_d* sot_d = c3_malloc(dir_z * sizeof(*sot_d));
          c3_z len_z  = u3_disk_epoc_list(log_u, sot_d);

          if ( len_z <= 1 ) {
            fprintf(stderr, "only epoch is bad, bailing out\r\n");
            c3_free(log_u); // XX leaks dire(s)
            return 0;
          }

          fprintf(stderr, "disk: latest epoch is 0i%" PRIc3_d " is bogus; "
                          "falling back to previous at 0i%" PRIc3_d "\r\n",
                          lat_d, sot_d[1]);

          _disk_epoc_kill(log_u, lat_d);

          lat_d = sot_d[1];
          try_o = c3y;
          c3_free(sot_d);
          goto try_init;
        } break;

        case _epoc_fail: {
          fprintf(stderr, "failed to load epoch\r\n");
          c3_free(log_u); // XX leaks dire(s)
          return 0;
        } break;

        case _epoc_late: {
          fprintf(stderr, "upgrade runtime version\r\n");
          c3_free(log_u); // XX leaks dire(s)
          return 0;
        }
      }
    }
  }
}
