/// @file

#include "vere.h"

#include "blob.h"
#include "noun.h"

//  stag: large bodies staged to .urb/stg on the thread pool
//
//    a driver that receives a body over U3_BLOB_THRESH writes it to a
//    staging file before it plans the event that carries it.  the write
//    and the mug run on the pool, not the loop; one job runs at a time,
//    so a burst of uploads holds one pool thread and leaves the rest to
//    the disk commit.  the driver plans its event from don_f.
//

static u3_stag* hed_u;          //  queue, oldest first; the head may run
static u3_stag* tal_u;          //  queue tail
static c3_o     run_o = c3n;    //  the head is on the pool

/* u3_pool_work(): queue [wor_u] on the thread pool.
**
**   the pool's threads take the signal mask of the thread that starts
**   them, at the first submission: keep the longjmp-ing signals out of
**   it, or one that arrives during a critical section (which blocks
**   them on this thread only) runs its handler on a worker and unwinds
**   this thread's stack from there.
*/
void
u3_pool_work(uv_work_t* wor_u, uv_work_cb wok_f, uv_after_work_cb aft_f)
{
#ifndef U3_OS_windows
  sigset_t sig_u, old_u;
  sigemptyset(&sig_u);
  sigaddset(&sig_u, SIGINT);
  sigaddset(&sig_u, SIGTERM);
  sigaddset(&sig_u, SIGVTALRM);
  pthread_sigmask(SIG_BLOCK, &sig_u, &old_u);
#endif

  uv_queue_work(u3L, wor_u, wok_f, aft_f);

#ifndef U3_OS_windows
  pthread_sigmask(SIG_SETMASK, &old_u, 0);
#endif
}

/* _stag_free(): free a job and what it owns.
*/
static void
_stag_free(u3_stag* tag_u)
{
  c3_free(tag_u->buf_y);
  c3_free(tag_u->nam_c);
  u3z(tag_u->som);
  c3_free(tag_u);
}

/* _stag_file(): stage the file at [nam_c], unless its mug is [gum_h].
*/
static void
_stag_file(u3_stag* tag_u)
{
  struct stat buf_u;
  c3_i        fid_i = c3_open(tag_u->nam_c, O_RDONLY, 0644);

  if ( (0 > fid_i) || (0 > fstat(fid_i, &buf_u)) ) {
    fprintf(stderr, "stag: open %s: %s\r\n", tag_u->nam_c, strerror(errno));
    if ( 0 <= fid_i ) {
      close(fid_i);
    }
    return;
  }

  tag_u->len_d = (c3_d)buf_u.st_size;

  if (  tag_u->gum_h
     && (c3y == u3b_mug_fd(fid_i, &tag_u->mug_h))
     && (tag_u->gum_h == tag_u->mug_h) )
  {
    tag_u->sam_o = c3y;
  }
  else {
    tag_u->ok_o = u3b_stage_fd(tag_u->pax_c, fid_i, tag_u->len_d,
                               tag_u->stg_c, &tag_u->mug_h);
  }

  close(fid_i);
}

/* _stag_work_cb(): on the pool: stage the job's body.  touches no loom.
*/
static void
_stag_work_cb(uv_work_t* wor_u)
{
  u3_stag* tag_u = wor_u->data;

  if ( tag_u->buf_y ) {
    tag_u->ok_o = u3b_stage(tag_u->pax_c, tag_u->buf_y, tag_u->len_d,
                            tag_u->stg_c, &tag_u->mug_h);
  }
  else {
    _stag_file(tag_u);
  }
}

static void
_stag_next(void);

/* _stag_after_cb(): on the loop: report the job, then start the next.
*/
static void
_stag_after_cb(uv_work_t* wor_u, c3_i sas_i)
{
  u3_stag* tag_u = wor_u->data;

  u3_assert( tag_u == hed_u );
  hed_u = tag_u->nex_u;
  if ( !hed_u ) {
    tal_u = 0;
  }
  run_o = c3n;

  if ( tag_u->ptr_v ) {
    tag_u->don_f(tag_u);
  }
  else if ( c3y == tag_u->ok_o ) {
    c3_unlink(tag_u->stg_c);
  }

  _stag_free(tag_u);
  _stag_next();
}

/* _stag_next(): put the head of the queue on the pool.
*/
static void
_stag_next(void)
{
  if ( hed_u && (c3n == run_o) ) {
    run_o = c3y;
    hed_u->wor_u.data = hed_u;
    u3_pool_work(&hed_u->wor_u, _stag_work_cb, _stag_after_cb);
  }
}

/* u3_stag_new(): a staging job for [ptr_v], reported to [don_f].
*/
u3_stag*
u3_stag_new(void* ptr_v, void (*don_f)(u3_stag*))
{
  u3_stag* tag_u = c3_calloc(sizeof(*tag_u));

  tag_u->pax_c = u3C.dir_c;
  tag_u->som   = u3_nul;
  tag_u->ptr_v = ptr_v;
  tag_u->don_f = don_f;
  tag_u->ok_o  = c3n;
  tag_u->sam_o = c3n;
  return tag_u;
}

/* u3_stag_plan(): queue a staging job.
*/
void
u3_stag_plan(u3_stag* tag_u)
{
  tag_u->nex_u = 0;

  if ( tal_u ) {
    tal_u->nex_u = tag_u;
  }
  else {
    hed_u = tag_u;
  }
  tal_u = tag_u;

  _stag_next();
}

/* u3_stag_drop(): forget every job for [ptr_v].
*/
void
u3_stag_drop(void* ptr_v)
{
  u3_stag** las_u = &hed_u;
  u3_stag*  pre_u = 0;

  while ( *las_u ) {
    u3_stag* tag_u = *las_u;

    if ( tag_u->ptr_v != ptr_v ) {
      pre_u = tag_u;
      las_u = &tag_u->nex_u;
    }
    //  the running job finishes on the pool; its result is discarded
    //
    else if ( (tag_u == hed_u) && (c3y == run_o) ) {
      tag_u->ptr_v = 0;
      pre_u = tag_u;
      las_u = &tag_u->nex_u;
    }
    else {
      *las_u = tag_u->nex_u;
      if ( tal_u == tag_u ) {
        tal_u = pre_u;
      }
      _stag_free(tag_u);
    }
  }
}
