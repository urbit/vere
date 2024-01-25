/// @file

#include "noun.h"
#include "events.h"
#include "vere.h"
#include "version.h"
#include "db/lmdb.h"
#include <types.h>

struct _cd_read {
  uv_timer_t       tim_u;
  c3_d             eve_d;
  c3_d             len_d;
  struct _u3_fact* ent_u;               //  queue entry
  struct _u3_fact* ext_u;               //  queue exit
  struct _u3_disk* log_u;
};

struct _cd_save {
  c3_o             ret_o;               //  result
  c3_d             eve_d;               //  first event
  c3_d             len_d;               //  number of events
  c3_y**           byt_y;               //  array of bytes
  size_t*          siz_i;               //  array of lengths
  struct _u3_disk* log_u;
};

struct _u3_disk_walk {
  u3_lmdb_walk  itr_u;
  u3_disk*      log_u;
  c3_o          liv_o;
};

// for u3_lmdb_init() calls
static const size_t siz_i =
#if (defined(U3_CPU_aarch64) && defined(U3_OS_linux))
  // 500 GiB is as large as musl on aarch64 wants to allow
  0x7d00000000;
#else
  0x10000000000;
#endif

#undef VERBOSE_DISK
#undef DISK_TRACE_JAM
#undef DISK_TRACE_CUE

static void
_disk_commit(u3_disk* log_u);

/* _disk_free_save(): free write batch
*/
static void
_disk_free_save(struct _cd_save* req_u)
{
  while ( req_u->len_d-- ) {
    c3_free(req_u->byt_y[req_u->len_d]);
  }

  c3_free(req_u->byt_y);
  c3_free(req_u->siz_i);
  c3_free(req_u);
}

/* _disk_commit_done(): commit complete.
 */
static void
_disk_commit_done(struct _cd_save* req_u)
{
  u3_disk* log_u = req_u->log_u;
  c3_d     eve_d = req_u->eve_d;
  c3_d     len_d = req_u->len_d;
  c3_o     ret_o = req_u->ret_o;

  if ( c3n == ret_o ) {
    log_u->cb_u.write_bail_f(log_u->cb_u.ptr_v, eve_d + (len_d - 1ULL));

#ifdef VERBOSE_DISK
    if ( 1ULL == len_d ) {
      fprintf(stderr, "disk: (%" PRIu64 "): commit: failed\r\n", eve_d);
    }
    else {
      fprintf(stderr, "disk: (%" PRIu64 "-%" PRIu64 "): commit: failed\r\n",
                      eve_d,
                      eve_d + (len_d - 1ULL));
    }
#endif
  }
  else {
    log_u->dun_d = eve_d + (len_d - 1ULL);
    log_u->cb_u.write_done_f(log_u->cb_u.ptr_v, log_u->dun_d);

#ifdef VERBOSE_DISK
    if ( 1ULL == len_d ) {
      fprintf(stderr, "disk: (%" PRIu64 "): commit: complete\r\n", eve_d);
    }
    else {
      fprintf(stderr, "disk: (%" PRIu64 "-%" PRIu64 "): commit: complete\r\n",
                      eve_d,
                      eve_d + (len_d - 1ULL));
    }
#endif
  }

  {
    u3_fact* tac_u = log_u->put_u.ext_u;

    while ( tac_u && (tac_u->eve_d <= log_u->dun_d) ) {
      log_u->put_u.ext_u = tac_u->nex_u;
      u3_fact_free(tac_u);
      tac_u = log_u->put_u.ext_u;
    }
  }

  if ( !log_u->put_u.ext_u ) {
    log_u->put_u.ent_u = 0;
  }

  _disk_free_save(req_u);

  _disk_commit(log_u);
}

/* _disk_commit_after_cb(): on the main thread, finish write
*/
static void
_disk_commit_after_cb(uv_work_t* ted_u, c3_i sas_i)
{
  struct _cd_save* req_u = ted_u->data;

  if ( UV_ECANCELED == sas_i ) {
    _disk_free_save(req_u);
  }
  else {
    ted_u->data = 0;
    req_u->log_u->ted_o = c3n;
    _disk_commit_done(req_u);
  }
}

/* _disk_commit_cb(): off the main thread, write event-batch.
*/
static void
_disk_commit_cb(uv_work_t* ted_u)
{
  struct _cd_save* req_u = ted_u->data;
  req_u->ret_o = u3_lmdb_save(req_u->log_u->mdb_u,
                              req_u->eve_d,
                              req_u->len_d,
                              (void**)req_u->byt_y, // XX safe?
                              req_u->siz_i);
}

/* _disk_commit_start(): queue async event-batch write.
*/
static void
_disk_commit_start(struct _cd_save* req_u)
{
  u3_disk* log_u = req_u->log_u;

  u3_assert( c3n == log_u->ted_o );
  log_u->ted_o = c3y;
  log_u->ted_u.data = req_u;

  //  queue asynchronous work to happen on another thread
  //
  uv_queue_work(u3L, &log_u->ted_u, _disk_commit_cb,
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
static struct _cd_save*
_disk_batch(u3_disk* log_u, c3_d len_d)
{
  u3_fact* tac_u = log_u->put_u.ext_u;

  u3_assert( (1ULL + log_u->dun_d) == tac_u->eve_d );
  u3_assert( log_u->sen_d == log_u->put_u.ent_u->eve_d );

  struct _cd_save* req_u = c3_malloc(sizeof(*req_u));
  req_u->log_u = log_u;
  req_u->ret_o = c3n;
  req_u->eve_d = tac_u->eve_d;
  req_u->len_d = len_d;
  req_u->byt_y = c3_malloc(len_d * sizeof(c3_y*));
  req_u->siz_i = c3_malloc(len_d * sizeof(size_t));

  for ( c3_d i_d = 0ULL; i_d < len_d; ++i_d) {
    u3_assert( (req_u->eve_d + i_d) == tac_u->eve_d );

    req_u->siz_i[i_d] = u3_disk_etch(log_u, tac_u->job,
                                     tac_u->mug_l, &req_u->byt_y[i_d]);

    tac_u = tac_u->nex_u;
  }

  return req_u;
}

/* _disk_commit(): commit all available events, if idle.
*/
static void
_disk_commit(u3_disk* log_u)
{
  if (  (c3n == log_u->ted_o)
     && (log_u->sen_d > log_u->dun_d) )
  {
    c3_d len_d = log_u->sen_d - log_u->dun_d;
    struct _cd_save* req_u = _disk_batch(log_u, len_d);

#ifdef VERBOSE_DISK
    if ( 1ULL == len_d ) {
      fprintf(stderr, "disk: (%" PRIu64 "): commit: request\r\n",
                      req_u->eve_d);
    }
    else {
      fprintf(stderr, "disk: (%" PRIu64 "-%" PRIu64 "): commit: request\r\n",
                      req_u->eve_d,
                      (req_u->eve_d + len_d - 1ULL));
    }
#endif

    _disk_commit_start(req_u);
  }
}

/* u3_disk_plan(): enqueue completed event for persistence.
*/
void
u3_disk_plan(u3_disk* log_u, u3_fact* tac_u)
{
  u3_assert( (1ULL + log_u->sen_d) == tac_u->eve_d );
  log_u->sen_d++;

  if ( !log_u->put_u.ent_u ) {
    u3_assert( !log_u->put_u.ext_u );
    log_u->put_u.ent_u = log_u->put_u.ext_u = tac_u;
  }
  else {
    log_u->put_u.ent_u->nex_u = tac_u;
    log_u->put_u.ent_u = tac_u;
  }

  _disk_commit(log_u);
}

/* u3_disk_boot_plan(): enqueue boot sequence, without autocommit.
*/
void
u3_disk_boot_plan(u3_disk* log_u, u3_noun job)
{
  //  NB, boot mugs are 0
  //
  u3_fact* tac_u = u3_fact_init(++log_u->sen_d, 0, job);

  if ( !log_u->put_u.ent_u ) {
    u3_assert( !log_u->put_u.ext_u );
    u3_assert( 1ULL == log_u->sen_d );

    log_u->put_u.ent_u = log_u->put_u.ext_u = tac_u;
  }
  else {
    log_u->put_u.ent_u->nex_u = tac_u;
    log_u->put_u.ent_u = tac_u;
  }

#ifdef VERBOSE_DISK
  fprintf(stderr, "disk: (%" PRIu64 "): db boot plan\r\n", tac_u->eve_d);
#endif
}

/* u3_disk_boot_save(): commit boot sequence.
*/
void
u3_disk_boot_save(u3_disk* log_u)
{
  u3_assert( !log_u->dun_d );
  _disk_commit(log_u);
}

static void
_disk_read_free(u3_read* red_u)
{
  //  free facts (if the read failed)
  //
  {
    u3_fact* tac_u = red_u->ext_u;
    u3_fact* nex_u;

    while ( tac_u ) {
      nex_u = tac_u->nex_u;
      u3_fact_free(tac_u);
      tac_u = nex_u;
    }
  }

  c3_free(red_u);
}

/* _disk_read_close_cb():
*/
static void
_disk_read_close_cb(uv_handle_t* had_u)
{
  u3_read* red_u = had_u->data;
  _disk_read_free(red_u);
}

static void
_disk_read_close(u3_read* red_u)
{
  u3_disk* log_u = red_u->log_u;

  //  unlink request
  //
  {
    if ( red_u->pre_u ) {
      red_u->pre_u->nex_u = red_u->nex_u;
    }
    else {
      log_u->red_u = red_u->nex_u;
    }

    if ( red_u->nex_u ) {
      red_u->nex_u->pre_u = red_u->pre_u;
    }
  }

  uv_close(&red_u->had_u, _disk_read_close_cb);
}

/* _disk_read_done_cb(): finalize read, invoke callback with response.
*/
static void
_disk_read_done_cb(uv_timer_t* tim_u)
{
  u3_read* red_u = tim_u->data;
  u3_disk* log_u = red_u->log_u;
  u3_info  pay_u = { .ent_u = red_u->ent_u, .ext_u = red_u->ext_u };

  u3_assert( red_u->ent_u );
  u3_assert( red_u->ext_u );
  red_u->ent_u = 0;
  red_u->ext_u = 0;

  log_u->cb_u.read_done_f(log_u->cb_u.ptr_v, pay_u);
  _disk_read_close(red_u);
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

/* _disk_read_one_cb(): lmdb read callback, invoked for each event in order
*/
static c3_o
_disk_read_one_cb(void* ptr_v, c3_d eve_d, size_t val_i, void* val_p)
{
  u3_read* red_u = ptr_v;
  u3_disk* log_u = red_u->log_u;
  u3_fact* tac_u;

  {
    u3_noun job;
    c3_l  mug_l;

    if ( c3n == u3_disk_sift(log_u, val_i, (c3_y*)val_p, &mug_l, &job) ) {
      return c3n;
    }

    tac_u = u3_fact_init(eve_d, mug_l, job);
  }

  if ( !red_u->ent_u ) {
    u3_assert( !red_u->ext_u );

    u3_assert( red_u->eve_d == eve_d );
    red_u->ent_u = red_u->ext_u = tac_u;
  }
  else {
    u3_assert( (1ULL + red_u->ent_u->eve_d) == eve_d );
    red_u->ent_u->nex_u = tac_u;
    red_u->ent_u = tac_u;
  }

  return c3y;
}

/* _disk_read_start_cb(): the read from the db, trigger response
*/
static void
_disk_read_start_cb(uv_timer_t* tim_u)
{
  u3_read* red_u = tim_u->data;
  u3_disk* log_u = red_u->log_u;

  //  read events synchronously
  //
  if ( c3n == u3_lmdb_read(log_u->mdb_u,
                           red_u,
                           red_u->eve_d,
                           red_u->len_d,
                           _disk_read_one_cb) )
  {
    log_u->cb_u.read_bail_f(log_u->cb_u.ptr_v, red_u->eve_d);
    _disk_read_close(red_u);
  }
  //  finish the read asynchronously
  //
  else {
    uv_timer_start(&red_u->tim_u, _disk_read_done_cb, 0, 0);
  }
}

/* u3_disk_read(): read [len_d] events starting at [eve_d].
*/
void
u3_disk_read(u3_disk* log_u, c3_d eve_d, c3_d len_d)
{
  u3_read* red_u = c3_malloc(sizeof(*red_u));
  red_u->log_u = log_u;
  red_u->eve_d = eve_d;
  red_u->len_d = len_d;
  red_u->ent_u = red_u->ext_u = 0;
  red_u->pre_u = 0;
  red_u->nex_u = log_u->red_u;

  if ( log_u->red_u ) {
    log_u->red_u->pre_u = red_u;
  }
  log_u->red_u = red_u;

  //  perform the read asynchronously
  //
  uv_timer_init(u3L, &red_u->tim_u);

  red_u->tim_u.data = red_u;
  uv_timer_start(&red_u->tim_u, _disk_read_start_cb, 0, 0);
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
_disk_save_meta(MDB_env* mdb_u, const c3_c* key_c, u3_atom dat)
{
  c3_w  len_w = u3r_met(3, dat);
  c3_y* byt_y = c3_malloc(len_w);
  u3r_bytes(0, len_w, byt_y, dat);

  {
    c3_o ret_o = u3_lmdb_save_meta(mdb_u, key_c, len_w, byt_y);
    c3_free(byt_y);
    return ret_o;
  }
}

/* u3_disk_save_meta(): save metadata.
*/
c3_o
u3_disk_save_meta(MDB_env* mdb_u,
                  c3_w     ver_w,
                  c3_d     who_d[2],
                  c3_o     fak_o,
                  c3_w     lif_w)
{
  u3_assert( c3y == u3a_is_cat(lif_w) );

  if (  (c3n == _disk_save_meta(mdb_u, "version", ver_w))
     || (c3n == _disk_save_meta(mdb_u, "who", u3i_chubs(2, who_d)))
     || (c3n == _disk_save_meta(mdb_u, "fake", fak_o))
     || (c3n == _disk_save_meta(mdb_u, "life", lif_w)) )
  {
    return c3n;
  }

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
u3_disk_read_meta(MDB_env* mdb_u,
                  c3_w*    ver_w,
                  c3_d*    who_d,
                  c3_o*    fak_o,
                  c3_w*    lif_w)
{
  _mdb_val val_u;

  //  version
  //
  u3_lmdb_read_meta(mdb_u, &val_u, "version", _disk_meta_read_cb);

  if ( 0 > val_u.hav_i ) {
    fprintf(stderr, "disk: read meta: no version\r\n");
    return c3n;
  }
  else if ( (1 != val_u.hav_i) || (U3D_VERLAT < *val_u.buf_y) ) {
    fprintf(stderr, "disk: read meta: unknown version %u\r\n", *val_u.buf_y);
    return c3n;
  }

  if ( ver_w ) {
    c3_y* byt_y = val_u.buf_y;
    *ver_w = (c3_w)byt_y[0];
  }

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

  if ( who_d ) {
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
  }

  //  fake bit
  //
  u3_lmdb_read_meta(mdb_u, &val_u, "fake", _disk_meta_read_cb);

  if ( 0 > val_u.hav_i ) {
    fprintf(stderr, "disk: read meta: no fake bit\r\n");
    return c3n;
  }
  else if ( 0 == val_u.hav_i ) {
    if ( fak_o ) {
      *fak_o = 0;
    }
  }
  else if ( (1 == val_u.hav_i) || !((*val_u.buf_y) >> 1) ) {
    if ( fak_o ) {
      *fak_o = (*val_u.buf_y) & 1;
    }
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

  if ( lif_w ) {
    c3_y* byt_y = val_u.buf_y;
    *lif_w = (c3_w)byt_y[0]
           | (c3_w)byt_y[1] << 8
           | (c3_w)byt_y[2] << 16
           | (c3_w)byt_y[3] << 24;
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

/* u3_disk_acquire(): acquire a lockfile, killing anything that holds it.
*/
static void
u3_disk_acquire(c3_c* pax_c)
{
  c3_c* paf_c = _disk_lock(pax_c);
  c3_w  pid_w;
  FILE* loq_u;

  if ( NULL != (loq_u = c3_fopen(paf_c, "r")) ) {
    if ( 1 != fscanf(loq_u, "%" SCNu32, &pid_w) ) {
      u3l_log("lockfile %s is corrupt!", paf_c);
      kill(getpid(), SIGTERM);
      sleep(1); u3_assert(0);
    }
    else if (pid_w != getpid()) {
      c3_w i_w;

      int ret = kill(pid_w, SIGTERM);

      if ( -1 == ret && errno == EPERM ) {
        u3l_log("disk: permission denied when trying to kill process %d!", pid_w);
        kill(getpid(), SIGTERM);
        sleep(1); u3_assert(0);
      }

      if ( -1 != ret ) {
        u3l_log("disk: stopping process %d, live in %s...",
                pid_w, pax_c);

        for ( i_w = 0; i_w < 16; i_w++ ) {
          sleep(1);
          if ( -1 == kill(pid_w, SIGTERM) ) {
            break;
          }
        }
        if ( 16 == i_w ) {
          for ( i_w = 0; i_w < 16; i_w++ ) {
            if ( -1 == kill(pid_w, SIGKILL) ) {
              break;
            }
            sleep(1);
          }
        }
        if ( 16 == i_w ) {
          u3l_log("disk: process %d seems unkillable!", pid_w);
          u3_assert(0);
        }
        u3l_log("disk: stopped old process %u", pid_w);
      }
    }
    fclose(loq_u);
    c3_unlink(paf_c);
  }

  if ( NULL == (loq_u = c3_fopen(paf_c, "w")) ) {
    u3l_log("disk: unable to open %s: %s", paf_c, strerror(errno));
    u3_assert(0);
  }

  fprintf(loq_u, "%u\n", getpid());

  {
    c3_i fid_i = fileno(loq_u);
    c3_sync(fid_i);
  }

  fclose(loq_u);
  c3_free(paf_c);
}

/* u3_disk_release(): release a lockfile.
*/
static void
u3_disk_release(c3_c* pax_c)
{
  c3_c* paf_c = _disk_lock(pax_c);

  c3_unlink(paf_c);
  c3_free(paf_c);
}

/* u3_disk_exit(): close the log.
*/
void
u3_disk_exit(u3_disk* log_u)
{
  //  cancel all outstanding reads
  //
  {
    u3_read* red_u = log_u->red_u;

    while ( red_u ) {
      _disk_read_close(red_u);
      red_u = red_u->nex_u;
    }
  }

  //  try to cancel write thread
  //  shortcircuit cleanup if we cannot
  //
  if (  (c3y == log_u->ted_o)
     && (0 > uv_cancel(&log_u->req_u)) )
  {
    // u3l_log("disk: unable to cleanup");
    return;
  }

  //  close database
  //
  u3_lmdb_exit(log_u->mdb_u);

  //  dispose planned writes
  //

  {
    u3_fact* tac_u = log_u->put_u.ext_u;
    u3_fact* nex_u;

    while ( tac_u ) {
      nex_u = tac_u->nex_u;
      u3_fact_free(tac_u);
      tac_u = nex_u;
    }
  }

  u3_disk_release(log_u->dir_u->pax_c);

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
  u3_read* red_u = log_u->red_u;
  u3_noun red = u3_nul;
  u3_noun lit = u3i_list(
    u3_pier_mase("live",        log_u->liv_o),
    u3_pier_mase("event", u3i_chub(log_u->dun_d)),
    u3_none);

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

  while ( red_u ) {
    red = u3nc(
      u3_pier_mass(
        u3dc("scot", c3__ux, u3i_chub((c3_d)red_u)),
        u3i_list(
          u3_pier_mase("start", u3i_chub(red_u->eve_d)),
          u3_pier_mase("final", u3i_chub(red_u->eve_d + red_u->len_d - 1)),
          u3_none)),
      red);
    red_u = red_u->nex_u;
  }
  lit = u3nc(u3_pier_mass(c3__read, red), lit);
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
    u3_read* red_u = log_u->red_u;

    while ( red_u ) {
      u3l_log("    read: %" PRIu64 "-%" PRIu64,
              red_u->eve_d,
              (red_u->eve_d + red_u->len_d) - 1);
      red_u = red_u->nex_u;
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

/* u3_disk_epoc_init: create new epoch.
*/
c3_o
u3_disk_epoc_init(u3_disk* log_u, c3_d epo_d)
{
  //  check if any epoch directories exist
  c3_d lat_d;
  c3_o eps_o = u3_disk_epoc_last(log_u, &lat_d);

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
  if ( epo_d > 0 ) {
    c3_c chk_c[8193];
    snprintf(chk_c, 8192, "%s/.urb/chk", u3_Host.dir_c);
    if ( c3n == u3e_backup(chk_c, epo_c, c3y) ) {
      fprintf(stderr, "disk: copy epoch snapshot failed\r\n");
      goto fail1;
    }
  }

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

  //  create epoch version file, overwriting any existing file
  c3_c epv_c[8193];
  snprintf(epv_c, sizeof(epv_c), "%s/epoc.txt", epo_c);
  FILE* epv_f = fopen(epv_c, "w");  // XX errors
  fprintf(epv_f, "%d", U3D_VER3);
  fclose(epv_f);

  //  create binary version file, overwriting any existing file
  c3_c biv_c[8193];
  snprintf(biv_c, sizeof(biv_c), "%s/vere.txt", epo_c);
  FILE* biv_f = fopen(biv_c, "w");  //  XX errors
  fprintf(biv_f, URBIT_VERSION);
  fclose(biv_f);

  //  get metadata from old epoch or unmigrated event log's db
  c3_w     ver_w;
  c3_d     who_d[2];
  c3_o     fak_o;
  c3_w     lif_w;
  if ( c3y == eps_o ) {  //  skip if no epochs yet
    if ( c3y != u3_disk_read_meta(log_u->mdb_u, &ver_w, who_d, &fak_o, &lif_w) ) {
      fprintf(stderr, "disk: failed to read metadata\r\n");
      goto fail3;
    }

    u3_lmdb_exit(log_u->mdb_u);
    log_u->mdb_u = 0;
  }

  //  initialize db of new epoch
  if ( c3y == u3_Host.ops_u.nuu || epo_d > 0 ) {
    c3_c dat_c[8193];
    snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", epo_c);

    if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c, siz_i)) ) {
      fprintf(stderr, "disk: failed to initialize database\r\n");
      c3_free(log_u);
      goto fail3;
    }
  }

  // write the metadata to the database
  if ( c3y == eps_o ) {
    if ( c3n == u3_disk_save_meta(log_u->mdb_u, log_u->ver_w, who_d, fak_o, lif_w) ) {
      fprintf(stderr, "disk: failed to save metadata\r\n");
      goto fail3;
    }
  }

  if ( -1 == c3_sync(epo_i) ) {  //  XX fdatasync on linux?
    fprintf(stderr, "disk: sync epoch dir %" PRIc3_d " failed: %s\r\n",
                    epo_d, strerror(errno));
    goto fail3;
  }

  close(epo_i);

  //  load new epoch directory and set it in log_u
  log_u->epo_d = epo_d;

  //  success
  return c3y;

fail3:
  c3_unlink(epv_c);
  c3_unlink(biv_c);
fail2:
  close(epo_i);
fail1:
  c3_rmdir(epo_c);
  return c3n;
}

/* u3_disk_epoc_kill: delete an epoch.
*/
c3_o
u3_disk_epoc_kill(u3_disk* log_u, c3_d epo_d)
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

  *lat_d = 0;

  while ( den_u ) {
    c3_d epo_d = 0;
    if ( 1 == sscanf(den_u->nam_c, "0i%" PRIc3_d, &epo_d) ) {
      ret_o = c3y;   //  NB: returns yes if the directory merely exists
      *lat_d = c3_max(epo_d, *lat_d);  //  update the latest epoch number
    }
    den_u = den_u->nex_u;
  }

  u3_dire_free(die_u);

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

  while ( den_u ) {  //  count epochs
    len_z++;
    den_u = den_u->nex_u;
  }

  if ( !sot_d ) {
    u3_dire_free(ned_u);
    return len_z;
  }

  len_z = 0;
  den_u = ned_u->dil_u;

  while ( den_u ) {
    if ( 1 == sscanf(den_u->nam_c, "0i%" PRIc3_d, (sot_d + len_z)) ) {
      len_z++;
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

/* _disk_need_migrate: does the disk need to be migrated?
*/
static c3_o
_disk_need_migrate(u3_disk* log_u)
{
  c3_c dut_c[8193];
  snprintf(dut_c, sizeof(dut_c), "%s/data.mdb", log_u->com_u->pax_c);

  //  check if data.mdb is readable in log directory
  //
  if ( 0 == access(dut_c, F_OK)) {
    //  check if data.mdb has version 3
    c3_w ver_w;
    if ( c3y == u3_disk_read_meta(log_u->mdb_u, &ver_w, 0, 0, 0) &&
         ver_w == U3D_VER3 )
    {
      fprintf(stderr, "disk: failed to read metadata\r\n");
      return c3n;
    }

    return c3y;
  }

  return c3n;
}

/* _disk_migrate: migrates disk format.
 */
static c3_o
_disk_migrate(u3_disk* log_u, c3_d eve_d)
{
  /*  migration steps:
   *  0. detect whether we need to migrate or not
   *     a. if it's a fresh boot via u3_Host.ops_u.nuu -> skip migration
   *     b. if data.mdb is readable in log directory -> execute migration
   *        if not -> skip migration (returns yes)
   *  1. initialize epoch 0i0 (first call to u3_disk_epoc_init())
   *     a. creates epoch directory
   *     b. creates epoch version file
   *     c. creates binary version file
   *     d. initializes database
   *     e. reads metadata from old database
   *     f. writes metadata to new database
   *     g. loads new epoch directory and sets it in log_u
   *  2. create hard links to data.mdb and lock.mdb in 0i0/
   *  3. rollover to new epoch (second call to u3_disk_epoc_init())
   *     a. same as 1a-g but also copies current snapshot between c/d steps
   *  4. delete backup snapshot (c3_unlink() and c3_rmdir() calls)
   *  5. delete old data.mdb and lock.mdb files (c3_unlink() calls)
   */

  //  check if lock.mdb is readable in log directory
  c3_o luk_o = c3n;
  c3_c luk_c[8193];
  snprintf(luk_c, sizeof(luk_c), "%s/lock.mdb", log_u->com_u->pax_c);
  if ( 0 == access(luk_c, R_OK) ) {
    luk_o = c3y;
  }

  fprintf(stderr, "disk: migrating disk to v%d format\r\n", U3D_VER3);

  //  migrate existing pier which has either:
  //  - not started the migration, or
  //  - crashed before completing the migration

  //  get first/last event numbers from pre-migrated lmdb
  c3_d fir_d, las_d;
  if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
    fprintf(stderr, "disk: failed to get first/last event numbers\r\n");
    return c3n;
  }

  // ensure there's a current snapshot
  if ( eve_d != las_d ) {
    fprintf(stderr, "disk: snapshot is out of date, please "
                    "start/shutdown your pier gracefully first\r\n");
    fprintf(stderr, "disk: eve_d (%" PRIc3_d ") != las_d (%" PRIc3_d ")\r\n",
                    eve_d, las_d);
    return c3n;
  }

  //  initialize first epoch "0i0"
  if ( c3n == u3_disk_epoc_init(log_u, 0) ) {
    fprintf(stderr, "disk: failed to initialize first epoch\r\n");
    return c3n;
  }

  //  create hard links to data.mdb and lock.mdb in 0i0/
  c3_c epo_c[8193], dut_c[8193], dat_c[8193], lok_c[8193];
  snprintf(epo_c, sizeof(epo_c), "%s/0i0", log_u->com_u->pax_c);
  snprintf(dut_c, sizeof(dut_c), "%s/data.mdb", log_u->com_u->pax_c);
  snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", epo_c);
  snprintf(lok_c, sizeof(lok_c), "%s/lock.mdb", epo_c);

  if ( 0 < c3_link(dut_c, dat_c) ) {
    fprintf(stderr, "disk: failed to create data.mdb hard link\r\n");
    return c3n;
  }
  if ( c3y == luk_o ) {  //  only link lock.mdb if it exists
    if ( 0 < c3_link(luk_c, lok_c) ) {
      fprintf(stderr, "disk: failed to create lock.mdb hard link\r\n");
      return c3n;
    }
  }

  //  rollover to new epoch
  if ( c3n == u3_disk_epoc_init(log_u, las_d) ) {
    fprintf(stderr, "disk: failed to initialize new epoch\r\n");
    return c3n;
  }

  //  delete backup snapshot
  c3_c bhk_c[8193], nop_c[8193], sop_c[8193];
  snprintf(bhk_c, sizeof(bhk_c), "%s/.urb/bhk", u3_Host.dir_c);
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

  //  delete old lock.mdb and data.mdb files
  if ( c3_unlink(luk_c) ) {
    fprintf(stderr, "disk: failed to unlink lock.mdb: %s\r\n",
                    strerror(errno));
  }
  if ( c3_unlink(dut_c) ) {
    fprintf(stderr, "disk: failed to unlink data.mdb: %s\r\n",
                    strerror(errno));
    return c3n;  //  migration succeeds only if we can unlink data.mdb
  }

  //  success
  fprintf(stderr, "disk: migrated disk to v%d format\r\n", U3D_VER3);

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

/* u3_disk_kindly(): do the needful.
*/
void
u3_disk_kindly(u3_disk* log_u, c3_d eve_d)
{
  if ( c3y == _disk_need_migrate(log_u) ) {
    if ( c3n == _disk_migrate(log_u, eve_d) ) {
      fprintf(stderr, "disk: failed to migrate event log\r\n");
      exit(1);
    }
  }
  else if ( c3y == _disk_vere_diff(log_u) ) {
    if ( c3n == u3_disk_epoc_init(log_u, log_u->dun_d) ) {
      fprintf(stderr, "disk: failed to initialize epoch\r\n");
      exit(1);
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
_disk_epoc_load(u3_disk* log_u, c3_d lat_d)
{
  //  check latest epoc version
  //
  {
    c3_c ver_c[8];
    c3_w ver_w;
    if ( c3n == _disk_epoc_meta(log_u, lat_d, "epoc",
                                sizeof(ver_c) - 1, ver_c) )
    {
      fprintf(stderr, "disk: failed to load epoch 0i%" PRIc3_d " version\r\n",
                      lat_d);

      return _epoc_gone;
    }

    if ( 1 != sscanf(ver_c, "%d", &ver_w) ) {
      fprintf(stderr, "disk: failed to parse epoch version: '%s'\r\n", ver_c);
      return _epoc_fail;
    }

    if ( U3D_VER3 != ver_w ) {
      fprintf(stderr, "disk: unknown epoch version: '%s', expected '%d'\r\n",
                      ver_c, U3D_VER3);
      return _epoc_late;
    }

    log_u->ver_w = ver_w;
  }

  //  set path to latest epoch
  c3_c epo_c[8193];
  snprintf(epo_c, 8192, "%s/0i%" PRIc3_d, log_u->com_u->pax_c, lat_d);

  //  initialize latest epoch's db
  if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c, siz_i)) ) {
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

  if (  (c3n == u3_Host.ops_u.nuu )
     && !fir_d
     && !las_d
     && (c3n == u3_disk_read_meta(log_u->mdb_u, 0, 0, 0, 0)) )
  {
    u3_lmdb_exit(log_u->mdb_u);
    log_u->mdb_u = 0;
    return _epoc_void;
  }

  //  initialize dun_d/sen_d values
  log_u->dun_d = ( 0 != las_d ) ? las_d : lat_d;
  log_u->sen_d = log_u->dun_d;

  return _epoc_good;
}

/* u3_disk_init(): load or create pier directories and event log.
*/
u3_disk*
u3_disk_init(c3_c* pax_c, u3_disk_cb cb_u)
{
  u3_disk* log_u = c3_calloc(sizeof(*log_u));
  log_u->liv_o = c3n;
  log_u->ted_o = c3n;
  log_u->cb_u  = cb_u;
  log_u->red_u = 0;
  log_u->put_u.ent_u = log_u->put_u.ext_u = 0;

  //  create/load pier directory
  //
  {
    if ( 0 == (log_u->dir_u = u3_foil_folder(pax_c)) ) {
      fprintf(stderr, "disk: failed to load pier at %s\r\n", pax_c);
      c3_free(log_u);
      return 0;
    }
  }

  //  acquire lockfile.
  //
  u3_disk_acquire(pax_c);

  //  create/load $pier/.urb
  //
  {
    c3_c* urb_c = c3_malloc(6 + strlen(pax_c));

    strcpy(urb_c, pax_c);
    strcat(urb_c, "/.urb");

    if ( 0 == (log_u->urb_u = u3_foil_folder(urb_c)) ) {
      fprintf(stderr, "disk: failed to load /.urb in %s\r\n", pax_c);
      c3_free(urb_c);
      c3_free(log_u);
      return 0;
    }
    c3_free(urb_c);
  }

  //  create/load $pier/.urb/put and $pier/.urb/get
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

  //  create/load $pier/.urb/log
  //
  {
    c3_c log_c[8193];
    snprintf(log_c, sizeof(log_c), "%s/.urb/log", pax_c);

    if ( 0 == (log_u->com_u = u3_foil_folder(log_c)) ) {
      fprintf(stderr, "disk: failed to load /.urb/log in %s\r\n", pax_c);
      c3_free(log_u);
      return 0;
    }

    //  load the old data.mdb file
    fprintf(stderr, "disk: loading old format\r\n");

    if ( 0 == (log_u->mdb_u = u3_lmdb_init(log_c, siz_i)) ) {
      fprintf(stderr, "disk: failed to initialize lmdb\r\n");
      c3_free(log_u);
      return 0;
    }

    //  if fresh boot, initialize disk U3D_VER3
    //
    if ( c3y == u3_Host.ops_u.nuu ) {
      //  initialize first epoch "0i0"
      if ( c3n == u3_disk_epoc_init(log_u, 0) ) {
        fprintf(stderr, "disk: failed to initialize first epoch\r\n");
        return 0;
      }

      //  set its meta table version key to U3D_VER3 to ensure that if our pier
      //  is booted with <= vere-v2, it crashes instead of potentially
      //  corrupting its event log
      if ( c3n == u3_disk_save_meta(log_u->mdb_u, U3D_VER3, pir_u->) ) {
        fprintf(stderr, "disk: failed to save metadata version\r\n");
        c3_free(log_u);
        return 0;
      }
    }
    else if ( c3y == _disk_need_migrate(log_u) ) {
      c3_d fir_d;
      if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &log_u->dun_d) ) {
        fprintf(stderr, "disk: failed to load latest event from lmdb\r\n");
        // XX dispose mdb_u
        c3_free(log_u);
        return 0;
      }

      //  we keep the old event log around forever
      //  
      //  U3D_VER2 in meta table means in-progress migration to vere-v3
      //  U3D_VER3 in meta table means migration complete
      //
      //  this prevents the pier from booting with <= vere-v2
      if ( c3n == _disk_save_meta(log_u->mdb_u, "version", U3D_VER2) ) {
        fprintf(stderr, "disk: failed to save metadata version\r\n");
        c3_free(log_u);
        return 0;
      }
      else {
        log_u->sen_d = log_u->dun_d;
        log_u->ver_w = 0;

        return log_u;
      }
    }

    //  get latest epoch number
    c3_d lat_d;
    if ( c3n == u3_disk_epoc_last(log_u, &lat_d) ) {
      fprintf(stderr, "disk: failed to load epoch number\r\n");
      c3_free(log_u);
      return 0;
    }

    c3_o try_o = c3n;

try_init:
    {
      _epoc_kind kin_e = _disk_epoc_load(log_u, lat_d);

      switch ( kin_e ) {
        case _epoc_good: {
          //  mark the latest epoch directory
          log_u->epo_d = lat_d;

          //  mark the log as live
          log_u->liv_o = c3y;

#if defined(DISK_TRACE_JAM) || defined(DISK_TRACE_CUE)
          u3t_trace_open(pax_c);
#endif

          return log_u;
        } break;

        case _epoc_void: // XX could handle more efficiently

        case _epoc_gone: {
          // XX if there is no version number, the epoc is invalid
          // backup and try previous

          if ( c3y == try_o ) {
            fprintf(stderr, "multiple bad epochs, bailing out\r\n");
            c3_free(log_u);
            return 0;
          }

          c3_z len_z = u3_disk_epoc_list(log_u, 0);

          if ( len_z <= 1 ) {
            fprintf(stderr, "only epoch is bad, bailing out\r\n");
            c3_free(log_u);
            return 0;
          }

          c3_d* sot_d = c3_malloc(len_z * sizeof(*sot_d));
          u3_disk_epoc_list(log_u, sot_d);

          fprintf(stderr, "disk: latest epoch is 0i%" PRIc3_d " is bogus; "
                          "falling back to previous at 0i%" PRIc3_d "\r\n",
                          lat_d, sot_d[1]);

          u3_disk_epoc_kill(log_u, lat_d);

          lat_d = sot_d[1];
          try_o = c3y;
          c3_free(sot_d);
          goto try_init;
        } break;

        case _epoc_fail: {
          fprintf(stderr, "failed to load epoch\r\n");
          c3_free(log_u);
          return 0;
        } break;

        case _epoc_late: {
          fprintf(stderr, "upgrade runtime version\r\n");
          c3_free(log_u);
          return 0;
        }
      }
    }
  }
}
