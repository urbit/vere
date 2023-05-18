/// @file

#include "noun.h"
#include "vere.h"
#include "version.h"
#include "db/lmdb.h"

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

/* DISK FORMAT
 */

#define U3D_VER1   1
#define U3D_VERLAT U3L_VER1

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

/* _disk_serialize_v1(): serialize events in format v1.
*/
static c3_w
_disk_serialize_v1(u3_fact* tac_u, c3_y** out_y)
{
#ifdef DISK_TRACE_JAM
  u3t_event_trace("king disk jam", 'B');
#endif

  {
    u3_atom mat = u3qe_jam(tac_u->job);
    c3_w  len_w = u3r_met(3, mat);
    c3_y* dat_y = c3_malloc(4 + len_w);
    dat_y[0] = tac_u->mug_l & 0xff;
    dat_y[1] = (tac_u->mug_l >> 8) & 0xff;
    dat_y[2] = (tac_u->mug_l >> 16) & 0xff;
    dat_y[3] = (tac_u->mug_l >> 24) & 0xff;
    u3r_bytes(0, len_w, dat_y + 4, mat);

#ifdef DISK_TRACE_JAM
    u3t_event_trace("king disk jam", 'E');
#endif

    u3z(mat);

    *out_y = dat_y;
    return len_w + 4;
  }
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

    req_u->siz_i[i_d] = _disk_serialize_v1(tac_u, &req_u->byt_y[i_d]);

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

/* _disk_read_one_cb(): lmdb read callback, invoked for each event in order
*/
static c3_o
_disk_read_one_cb(void* ptr_v, c3_d eve_d, size_t val_i, void* val_p)
{
  u3_read* red_u = ptr_v;
  u3_disk* log_u = red_u->log_u;
  u3_fact* tac_u;

  if ( 4 >= val_i ) {
    return c3n;
  }

  {
    u3_noun job;
    c3_y* dat_y = val_p;
    c3_l  mug_l = dat_y[0]
                ^ (dat_y[1] <<  8)
                ^ (dat_y[2] << 16)
                ^ (dat_y[3] << 24);

#ifdef DISK_TRACE_CUE
    u3t_event_trace("king disk cue", 'B');
#endif

    //  XX u3m_soft?
    //
    job = u3ke_cue(u3i_bytes(val_i - 4, dat_y + 4));

#ifdef DISK_TRACE_CUE
    u3t_event_trace("king disk cue", 'E');
#endif

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
                  c3_d     who_d[2],
                  c3_o     fak_o,
                  c3_w     lif_w)
{
  u3_assert( c3y == u3a_is_cat(lif_w) );

  if (  (c3n == _disk_save_meta(mdb_u, "version", 1))
     || (c3n == _disk_save_meta(mdb_u, "who", u3i_chubs(2, who_d)))
     || (c3n == _disk_save_meta(mdb_u, "fake", fak_o))
     || (c3n == _disk_save_meta(mdb_u, "life", lif_w)) )
  {
    return c3n;
  }

  return c3y;
}

/* _disk_meta_read_cb(): copy [val_p] to atom [ptr_v] if present.
*/
static void
_disk_meta_read_cb(void* ptr_v, size_t val_i, void* val_p)
{
  u3_weak* mat = ptr_v;

  if ( val_p ) {
    *mat = u3i_bytes(val_i, val_p);
  }
}

/* _disk_read_meta(): read metadata at [key_c], deserialize.
*/
static u3_weak
_disk_read_meta(MDB_env* mdb_u, const c3_c* key_c)
{
  u3_weak dat = u3_none;
  u3_lmdb_read_meta(mdb_u, &dat, key_c, _disk_meta_read_cb);
  return dat;
}

/* u3_disk_read_meta(): read metadata.
*/
c3_o
u3_disk_read_meta(MDB_env* mdb_u,
                  c3_d*    who_d,
                  c3_o*    fak_o,
                  c3_w*    lif_w)
{
  u3_weak ver, who, fak, lif;

  if ( u3_none == (ver = _disk_read_meta(mdb_u, "version")) ) {
    fprintf(stderr, "disk: read meta: no version\r\n");
    return c3n;
  }
  if ( u3_none == (who = _disk_read_meta(mdb_u, "who")) ) {
    fprintf(stderr, "disk: read meta: no indentity\r\n");
    return c3n;
  }
  if ( u3_none == (fak = _disk_read_meta(mdb_u, "fake")) ) {
    fprintf(stderr, "disk: read meta: no fake bit\r\n");
    return c3n;
  }
  if ( u3_none == (lif = _disk_read_meta(mdb_u, "life")) ) {
    fprintf(stderr, "disk: read meta: no lifecycle length\r\n");
    return c3n;
  }

  {
    c3_o val_o = c3y;

    if ( 1 != ver ) {
      fprintf(stderr, "disk: read meta: unknown version %u\r\n", ver);
      val_o = c3n;
    }
    else if ( !((c3y == fak ) || (c3n == fak )) ) {
      fprintf(stderr, "disk: read meta: invalid fake bit\r\n");
      val_o = c3n;
    }
    else if ( c3n == u3a_is_cat(lif) ) {
      fprintf(stderr, "disk: read meta: invalid lifecycle length\r\n");
      val_o = c3n;
    }

    if ( c3n == val_o ) {
      u3z(ver); u3z(who); u3z(fak); u3z(lif);
      return c3n;
    }
  }

  if ( who_d ) {
    u3r_chubs(0, 2, who_d, who);
  }

  if ( fak_o ) {
    *fak_o = fak;
  }

  if ( lif_w ) {
    *lif_w = lif;
  }

  u3z(who);
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
    u3l_log("disk: unable to open %s", paf_c);
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

    //  migrate to the correct disk format if necessary
    if ( c3n == u3_disk_migrate(log_u) ) {
      fprintf(stderr, "disk: failed to migrate to v%d\r\n", U3D_VER1);
      c3_free(log_u);
      return 0;
    }

    //  get latest epoch number
    c3_d lat_d;
    if ( c3n == u3_disk_epoc_last(log_u, &lat_d) ) {
      fprintf(stderr, "disk: failed to load epoch number\r\n");
      c3_free(log_u);
      return 0;
    }

    //  get binary version from latest epoch
    c3_c ver_w[8193];
    if ( c3n == u3_disk_epoc_vere(log_u, lat_d, ver_w) ) {
      fprintf(stderr, "disk: failed to load epoch version\r\n");
      c3_free(log_u);
      return 0;
    }

    //  set path to latest epoch
    c3_c epo_c[8193];
    snprintf(epo_c, 8192, "%s/0i%" PRIu64, log_c, lat_d);

    //  initialize latest epoch's db
    if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c)) ) {
      fprintf(stderr, "disk: failed to initialize database\r\n");
      c3_free(log_u);
      return 0;
    }
    fprintf(stderr, "disk: loaded epoch 0i%" PRIu64 "\r\n", lat_d);

    //  get first/last event numbers from lmdb
    c3_d fir_d, las_d;
    if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
      fprintf(stderr, "disk: failed to get first/last event numbers\r\n");
      return 0;
    }

    //  initialize dun_d/sen_d values
    if ( 0 == las_d ) {               //  fresh epoch (no events in lmdb yet)
      if ( 0 == lat_d ) {             //  first epoch
        log_u->dun_d = 0;
      } else {                        //  not first epoch
        log_u->dun_d = lat_d;         //  set dun_d to last event in prev epoch
      }
    } else {                          //  not fresh epoch            
      log_u->dun_d = las_d;           //  set dun_d to last event in lmdb        
    }
    log_u->sen_d = log_u->dun_d;

    //  if binary version of latest epoch is not the same as the
    //  running binary, then we need to create a new epoch
    if ( 0 != strcmp(ver_w, URBIT_VERSION) ) {
      if ( c3n == u3_disk_epoc_init(log_u) ) {
        fprintf(stderr, "disk: failed to initialize epoch\r\n");
        c3_free(log_u);
        return 0;
      }
    }

    //  mark the log as live
    log_u->liv_o = c3y;
  }


#if defined(DISK_TRACE_JAM) || defined(DISK_TRACE_CUE)
  u3t_trace_open(pax_c);
#endif

  return log_u;
}

/* u3_disk_epoc_good: check for valid epoch.
*/
c3_o u3_disk_epoc_good(u3_disk* log_u, c3_d epo_d) {
  /*  a "valid" epoch is currently defined as a writable folder in 
   *  the <pier>/.urb/log directory named with a @ui and contains:
   *  - a writable data.mdb file which can be opened by lmdb (add db checking?)
   *  - a writable epoc.txt file
   *  - a writable vere.txt file
   *
   *  XX: should we check if the correct event sequence exists in the data.mdb file?
   *
   *  note that this does not check if the epoch even contains snapshot files, 
   *  let alone whether they're valid or not
   */

  c3_o ret_o = c3y;  //  return code

  //  file paths
  c3_c epo_c[8193], dat_c[8193], epv_c[8193], biv_c[8193];
  snprintf(epo_c, sizeof(epo_c), "%s/0i%" PRIu64, log_u->com_u->pax_c, epo_d);
  snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", epo_c);
  snprintf(epv_c, sizeof(epv_c), "%s/epoc.txt", epo_c);
  snprintf(biv_c, sizeof(biv_c), "%s/vere.txt", epo_c);

  c3_o dir_o = c3n;  //  directory is writable
  c3_o dat_o = c3n;  //  data.mdb is writable
  c3_o mdb_o = c3n;  //  data.mdb can be opened
  c3_o epv_o = c3n;  //  epoc.txt is writable
  c3_o biv_o = c3n;  //  vere.txt is writable

  //  check if dir is writable
  if ( 0 == access(epo_c, W_OK) ) {
    dir_o = c3y;
  }

  //  check if data.mdb is writable
  if ( 0 == access(dat_c, W_OK) ) {
    dat_o = c3y;
    //  check if we can open data.mdb
    MDB_env* env_u;
    if ( 0 != (env_u = u3_lmdb_init(epo_c)) ) {
      mdb_o = c3y;
    }
    u3_lmdb_exit(env_u);
  }

  //  check if epoc.txt is writable
  if ( 0 == access(epv_c, W_OK) ) {
    epv_o = c3y;
  }

  //  check if vere.txt is writable
  if ( 0 == access(biv_c, W_OK) ) {
    biv_o = c3y;
  }

  //  print error messages
  if ( c3n == dir_o ) {
    fprintf(stderr, "disk: epoch 0i%" PRIu64 " is not writable\r\n", epo_d);
    ret_o = c3n;
  }
  if ( c3n == dat_o ) {
    fprintf(stderr, "disk: epoch 0i%" PRIu64 "/data.mdb is not writable\r\n", epo_d);
    ret_o = c3n;
  }
  if ( c3n == mdb_o ) {
    fprintf(stderr, "disk: epoch 0i%" PRIu64 "/data.mdb can't be opened\r\n", epo_d);
    ret_o = c3n;
  }
  if ( c3n == epv_o ) {
    fprintf(stderr, "disk: epoch 0i%" PRIu64 "/epoc.txt is not writable\r\n", epo_d);
    ret_o = c3n;
  }
  if ( c3n == biv_o ) {
    fprintf(stderr, "disk: epoch 0i%" PRIu64 "/vere.txt is not writable\r\n", epo_d);
    ret_o = c3n;
  }

  return ret_o;
}
 
/* u3_disk_epoc_init: create new epoch.
*/
c3_o u3_disk_epoc_init(u3_disk* log_u) {
  //  set new epoch number
  c3_d lat_d, new_d;
  c3_o eps_o = u3_disk_epoc_last(log_u, &lat_d);
  if ( c3n == eps_o ) {
    //  no epochs yet, so create first one 0i0
    new_d = 0;
  } else if ( c3n == u3_disk_epoc_good(log_u, lat_d) ) {
    //  last epoch is invalid, so overwrite it
    new_d = lat_d;
  } else {
    //  last epoch is valid, so create next one

    //  get first/last event numbers
    c3_d fir_d, las_d;
    if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
      fprintf(stderr, "disk: failed to get first/last event numbers\r\n");
      return c3n;
    }
    new_d = las_d;  //  create next epoch
  }

  //  create new epoch directory if it doesn't exist
  c3_c epo_c[8193];
  snprintf(epo_c, sizeof(epo_c), "%s/0i%" PRIu64, log_u->com_u->pax_c, new_d);
  c3_d ret_d = c3_mkdir(epo_c, 0700);
  if ( ( ret_d < 0 ) && ( errno != EEXIST ) ) {
    fprintf(stderr, "disk: failed to create epoch directory %" PRIu64 "\r\n", new_d);
    return c3n;
  }

  //  create epoch version file, overwriting any existing file
  c3_c epv_c[8193];
  snprintf(epv_c, sizeof(epv_c), "%s/epoc.txt", epo_c);
  FILE* epv_f = fopen(epv_c, "w");
  fprintf(epv_f, "%d\n", U3D_VER1);
  fclose(epv_f);

  //  create binary version file, overwriting any existing file
  c3_c biv_c[8193];
  snprintf(biv_c, sizeof(biv_c), "%s/vere.txt", epo_c);
  FILE* biv_f = fopen(biv_c, "w");
  fprintf(biv_f, URBIT_VERSION);
  fclose(biv_f);

  //  copy snapshot (skip if first epoch)
  if ( new_d > 0 ) {
    if ( c3n == u3e_backup(epo_c, c3y) ) {
      fprintf(stderr, "disk: failed to copy snapshot to new epoch\r\n");
      goto fail;
    }
  }

  //  get metadata from old epoch or unmigrated event log's db
  c3_d     who_d[2];
  c3_o     fak_o;
  c3_w     lif_w;
  if ( c3y == eps_o ) {
    if ( c3y != u3_disk_read_meta(log_u->mdb_u, who_d, &fak_o, &lif_w) ) {
      fprintf(stderr, "disk: failed to read metadata\r\n");
      goto fail;
    }
  }

  //  initialize db of new epoch
  if ( c3y == u3_Host.ops_u.nuu || new_d > 0 ) {
    c3_c dat_c[8193];
    snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", epo_c);
    // if ( c3n == c3_unlink(dat_c) ) {
    //   fprintf(stderr, "disk: failed to rm 0i%" PRIu64 "/data.mdb\r\n", new_d);
    //   goto fail;
    // };
    if ( 0 == (log_u->mdb_u = u3_lmdb_init(epo_c)) ) {
      fprintf(stderr, "disk: failed to initialize database\r\n");
      c3_free(log_u);
      goto fail;
    }
  }

  // write the metadata to the database
  if ( c3y == eps_o ) {
    if ( c3n == u3_disk_save_meta(log_u->mdb_u, who_d, fak_o, lif_w) ) {
      fprintf(stderr, "disk: failed to save metadata\r\n");
      goto fail;
    }
  }

  //  load new epoch directory and set it in log_u
  log_u->epo_u = u3_foil_folder(epo_c);

  //  success
  return c3y;

fail:
  c3_unlink(epv_c);
  c3_unlink(biv_c);
  c3_rmdir(epo_c);
  return c3n;
}

/* u3_disk_epoc_kill: delete an epoch.
*/
c3_o u3_disk_epoc_kill(u3_disk* log_u, c3_d epo_d) {
  //  get epoch directory
  c3_c epo_c[8193];
  snprintf(epo_c, sizeof(epo_c), "%s/0i%" PRIu64, log_u->com_u->pax_c, epo_d);

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

  //  success
  return c3y;
}

/* u3_disk_epoc_last: get latest epoch number.
*/
c3_o u3_disk_epoc_last(u3_disk* log_u, c3_d* lat_d) {
  c3_o ret_d = c3n;  //  return no if no epoch directories exist
  *lat_d = 0;        //  initialize lat_d to 0
  u3_dent* den_u = u3_foil_folder(log_u->com_u->pax_c)->dil_u;
  while ( den_u ) {
    c3_d epo_d = 0;
    if ( 1 == sscanf(den_u->nam_c, "0i%" PRIu64, &epo_d) ) {
      ret_d = c3y;   //  NB: returns yes if the directory merely exists
    }
    *lat_d = c3_max(epo_d, *lat_d);  //  update the latest epoch number
    den_u = den_u->nex_u;
  }

  return ret_d;
}

/* u3_disk_epoc_vere: get binary version from epoch.
*/
c3_o
u3_disk_epoc_vere(u3_disk* log_u, c3_d epo_d, c3_c* ver_w) {
  c3_c ver_c[8193];
  snprintf(ver_c, sizeof(ver_c), "%s/0i%" PRIu64 "/vere.txt", 
                  log_u->com_u->pax_c, epo_d);

  FILE* fil_u = fopen(ver_c, "r");
  if ( NULL == fil_u ) {
    fprintf(stderr, "disk: failed to open vere.txt in epoch 0i%" PRIu64 
                    "\r\n", epo_d);
    return c3n;
  }

  if ( 1 != fscanf(fil_u, "%s", ver_w) ) {
    fprintf(stderr, "disk: failed to read vere.txt in epoch 0i%" PRIu64 
                    "\r\n", epo_d);
    return c3n;
  }
  return c3y;
}

/* u3_disk_migrate: migrates disk format.
 */
c3_o u3_disk_migrate(u3_disk* log_u)
{
  /*  migration steps (* indicates idempotency):
   *  0. detect whether we need to migrate or not *
   *     a. if it's a fresh boot via u3_Host.ops_u.nuu -> skip migration *
   *     b. if data.mdb is readable in log directory -> execute migration *
   *  1. initialize epoch 0i0 (first call to u3_disk_epoc_init()) *
   *     a. creates epoch directory *
   *     b. creates epoch version file *
   *     c. creates binary version file *
   *     d. initializes database *
   *     e. reads metadata from old database *
   *     f. writes metadata to new database *
   *     g. loads new epoch directory and sets it in log_u *
   *  2. create hard links to data.mdb and lock.mdb in 0i0/ *
   *  3. rollover to new epoch (second call to u3_disk_epoc_init())
   *     a. same as 1a-g but also copies current snapshot between c/d steps
   *  4. delete backup snapshot (c3_unlink() and c3_rmdir() calls)
   *  5. delete old data.mdb and lock.mdb files (c3_unlink() calls)
   *
   *  this function keeps track of its progress in a <pier>/.urb/log/trek 
   *  directory with empty files named after the corresponding stage which 
   *  has been completed in the migration process:
   *  1. `trek/1` -- completed through step 3
   *  2. `trek/2` -- completed through step 4
   *  3. `trek/3` -- completed entire migration except removal of trek/
   *
   *  when migration is completed, the trek/ directory is removed
   */

  //  check if data.mdb is readable in log directory
  c3_o dat_o = c3n;
  c3_c dat_c[8193];
  snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", log_u->com_u->pax_c);
  if ( 0 == access(dat_c, R_OK) ) {
    dat_o = c3y;
  }

  //  check if lock.mdb is readable in log directory
  c3_o lok_o = c3n;
  c3_c lok_c[8193];
  snprintf(lok_c, sizeof(dat_c), "%s/data.mdb", log_u->com_u->pax_c);
  if ( 0 == access(dat_c, R_OK) ) {
    lok_o = c3y;
  }

  if ( c3y == u3_Host.ops_u.nuu ) {
    //  initialize disk v1 on fresh boots
    fprintf(stderr, "disk: initializing disk with v%d format\r\n", U3D_VER1);

    //  initialize first epoch "0i0"
    if ( c3n == u3_disk_epoc_init(log_u) ) {
      fprintf(stderr, "disk: migrate: failed to initialize first epoch\r\n");
      return c3n;
    }
  } else if ( c3y == dat_o ) {
    //  migrate existing pier which has either:
    //  - not started the migration, or 
    //  - crashed before completing the migration
    
    // //  create trek/ directory to track migration progress
    // c3_c trk_c[8193];
    // snprintf(trk_c, sizeof(trk_c), "%s/trek", log_u->com_u->pax_c);
    // if ( 0 != access(trk_c, F_OK) ) {
    //   if ( 0 != c3_mkdir(trk_c, 0700) ) {
    //     fprintf(stderr, "disk: migrate: failed to create trek/ directory\r\n");
    //     return c3n;
    //   }
    // } else {
    //   fprintf(stderr, "disk: migrate: previous migration detected\r\n");
    //   fprintf(stderr, "disk: migrate: resuming migration...\r\n");
    // }

    //  initialize pre-migrated lmdb
    // MDB_env* old_u;
    {
      if ( 0 == (log_u->mdb_u = u3_lmdb_init(log_u->com_u->pax_c)) ) {
        fprintf(stderr, "disk: failed to initialize database\r\n");
        return c3n;
      }
    }

    //  get first/last event numbers from pre-migrated lmdb
    c3_d fir_d, las_d;
    if ( c3n == u3_lmdb_gulf(log_u->mdb_u, &fir_d, &las_d) ) {
      fprintf(stderr, "disk: failed to get first/last event numbers\r\n");
      return c3n;
    }

    // ensure there's a current snapshot
    if ( u3_Host.eve_d != las_d ) {
      fprintf(stderr, "disk: migrate: error: snapshot is out of date, please "
                      "start/shutdown your pier gracefully first\r\n");
      fprintf(stderr, "disk: migrate: eve_d (%" PRIu64 ") != las_d (%" PRIu64 ")\r\n",
                      u3_Host.eve_d, las_d);
      return c3n;
    }

    //  initialize first epoch "0i0"
    if ( c3n == u3_disk_epoc_init(log_u) ) {
      fprintf(stderr, "disk: migrate: failed to initialize first epoch\r\n");
      return c3n;
    }

    //  create hard links to data.mdb and lock.mdb in 0i0/
    c3_c dut_c[8193], luk_c[8193];  //  old paths
    snprintf(dut_c, sizeof(dut_c), "%s/data.mdb", log_u->com_u->pax_c);
    snprintf(luk_c, sizeof(luk_c), "%s/lock.mdb", log_u->com_u->pax_c);

    c3_c epo_c[8193], dat_c[8193], lok_c[8193];  //  new paths
    snprintf(epo_c, sizeof(epo_c), "%s/0i0", log_u->com_u->pax_c);
    snprintf(dat_c, sizeof(dat_c), "%s/data.mdb", epo_c);
    snprintf(lok_c, sizeof(lok_c), "%s/lock.mdb", epo_c);

    if ( 0 < c3_link(dut_c, dat_c) ) {
      fprintf(stderr, "disk: migrate: failed to create data.mdb hard link\r\n");
      fprintf(stderr, "errno %d: %s\r\n", errno, strerror(errno));
      return c3n;
    }
    if ( c3y == lok_o ) {  //  only link lock.mdb if it exists
      if ( 0 < c3_link(luk_c, lok_c) ) {
        fprintf(stderr, "disk: migrate: failed to create lock.mdb hard link\r\n");
        c3_rename(dat_c, dut_c);
        return c3n;
      }
    }

    //  rollover to new epoch
    // log_u->mdb_u = old_u;
    if ( c3n == u3_disk_epoc_init(log_u) ) {
      fprintf(stderr, "roll: error: failed to initialize new epoch\r\n");
      return c3n;
    }

    //  delete backup snapshot
    c3_c bhk_c[8193], nop_c[8193], sop_c[8193];
    snprintf(bhk_c, sizeof(bhk_c), "%s/.urb/bhk", u3_Host.dir_c);
    snprintf(nop_c, sizeof(nop_c), "%s/north.bin", bhk_c);
    snprintf(sop_c, sizeof(sop_c), "%s/south.bin", bhk_c);
    if ( c3n == c3_unlink(nop_c) ) {
      fprintf(stderr, "disk: migrate: failed to delete bhk/north.bin\r\n");
    } else if ( c3n == c3_unlink(sop_c) ) {
      fprintf(stderr, "disk: migrate: failed to delete bhk/south.bin\r\n");
    } else {
      if ( c3n == c3_rmdir(bhk_c) ) {
        fprintf(stderr, "disk: migrate: failed to delete bhk/\r\n");
      }
    }

    //  delete old lock.mdb and data.mdb files
    if ( 0 != c3_unlink(luk_c) ) {
      fprintf(stderr, "disk: migrate: failed to unlink lock.mdb\r\n");
    }
    if ( 0 != c3_unlink(dut_c) ) {
      fprintf(stderr, "disk: migrate: failed to unlink data.mdb\r\n");
      return c3n;  //  migration succeeds only if we can unlink data.mdb
    }

    //  success
    fprintf(stderr, "disk: migrated disk to v%d format\r\n", U3D_VER1);
  }

  return c3y;
}