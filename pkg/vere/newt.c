/// @file

/*
**  implements noun blob messages with trivial framing (v1 protocol).
**
**  v1 message format:
**    [0x1:1][tag:1][length:8 LE][data:N]
**
**  tag=0x0 (inline):
**    [0x1][0x0][length:8][payload:N]
**    - 10-byte header + payload bytes
**
**  tag=0x1 (file-backed):
**    [0x1][0x1][file_size:8][path_len:2][path:N]
**    - 12-byte header + path string
**    - for large payloads (>1GB threshold)
**    - file stored in $PIER/.urb/ipc/
**
**  the implementation is relatively inefficient and could
**  lose a few copies, mallocs, etc.
*/

#include "vere.h"

#include "noun.h"

#include <sys/stat.h>
#include <unistd.h>

#define NEWT_MMAP_THRESHOLD (1ULL << 30)  //  1GB

/* _newt_ipc_dir(): ensure IPC directory exists, return path (caller must free).
*/
static c3_c*
_newt_ipc_dir(const c3_c* pir_c)
{
  c3_c* ipc_c = c3_malloc(strlen(pir_c) + 10);
  sprintf(ipc_c, "%s/.urb/ipc", pir_c);

  struct stat st;
  if ( stat(ipc_c, &st) == -1 ) {
    if ( mkdir(ipc_c, 0700) == -1 ) {
      c3_free(ipc_c);
      return 0;
    }
  }
  return ipc_c;
}

/* _newt_file_path(): generate and return mmap file path (caller must free).
*/
static c3_c*
_newt_file_path(const c3_c* pir_c, c3_d eve_d, c3_l mug_l)
{
  c3_c* ipc_c = _newt_ipc_dir(pir_c);
  if ( !ipc_c ) return 0;

  c3_c* pat_c = c3_malloc(strlen(ipc_c) + 32);
  sprintf(pat_c, "%s/%" PRIu64 "-%08x.jam", ipc_c, eve_d, mug_l);
  c3_free(ipc_c);
  return pat_c;
}

/* _newt_file_save(): write payload to mmap file.
*/
static c3_o
_newt_file_save(const c3_c* pat_c, c3_d len_d, c3_y* byt_y)
{
  c3_y* map_y;

  if ( c3n == u3u_mmap("newt", (c3_c*)pat_c, len_d, &map_y) ) {
    return c3n;
  }

  memcpy(map_y, byt_y, len_d);

  if ( c3n == u3u_mmap_save("newt", (c3_c*)pat_c, len_d, map_y) ) {
    u3u_munmap(len_d, map_y);
    return c3n;
  }

  u3u_munmap(len_d, map_y);
  return c3y;
}

/* _newt_file_read(): read payload from mmap file.
*/
static c3_o
_newt_file_read(const c3_c* pat_c, c3_d exp_d, c3_d* len_d, c3_y** out_y)
{
  c3_y* map_y;
  c3_d  got_d;

  if ( c3n == u3u_mmap_read("newt", (c3_c*)pat_c, &got_d, &map_y) ) {
    return c3n;
  }

  if ( got_d != exp_d ) {
    fprintf(stderr, "newt: file size mismatch %" PRIu64 " != %" PRIu64 "\r\n",
            got_d, exp_d);
    u3u_munmap(got_d, map_y);
    return c3n;
  }

  c3_y* byt_y = c3_malloc(got_d);
  memcpy(byt_y, map_y, got_d);
  u3u_munmap(got_d, map_y);

  *len_d = got_d;
  *out_y = byt_y;
  return c3y;
}

/* _newt_file_drop(): delete mmap file.
*/
static void
_newt_file_drop(const c3_c* pat_c)
{
  if ( unlink(pat_c) != 0 ) {
    fprintf(stderr, "newt: failed to delete %s\r\n", pat_c);
  }
}

/* _newt_mess_head(): await next msg header.
*/
static void
_newt_mess_head(u3_mess* mes_u)
{
  mes_u->sat_e = u3_mess_head;
  mes_u->hed_u.has_y = 0;
}

/* _newt_mess_tail(): await msg body.
*/
static void
_newt_mess_tail(u3_mess* mes_u, c3_d len_d)
{
  u3_meat* met_u = c3_malloc(len_d + sizeof(*met_u));
  met_u->nex_u   = 0;
  met_u->len_d   = len_d;

  mes_u->sat_e = u3_mess_tail;
  mes_u->tal_u.has_d = 0;
  mes_u->tal_u.met_u = met_u;
}

/* _newt_mess_file(): await file path.
*/
static void
_newt_mess_file(u3_mess* mes_u, c3_d len_d, c3_s pat_s)
{
  mes_u->sat_e = u3_mess_file;
  mes_u->fil_u.len_d = len_d;
  mes_u->fil_u.pat_s = pat_s;
  mes_u->fil_u.has_s = 0;
  mes_u->fil_u.pat_c = c3_malloc(pat_s + 1);
}

/* _newt_meat_plan(): enqueue complete msg.
*/
static void
_newt_meat_plan(u3_moat* mot_u, u3_meat* met_u)
{
  if ( mot_u->ent_u ) {
    mot_u->ent_u->nex_u = met_u;
    mot_u->ent_u = met_u;
  }
  else {
    mot_u->ent_u = mot_u->ext_u = met_u;
  }
}

static void
_newt_meat_next_cb(uv_timer_t* tim_u);

/* _newt_meat_poke(): deliver completed msg.
*/
static void
_newt_meat_poke(u3_moat* mot_u)
{
  u3_meat* met_u = mot_u->ext_u;

  if ( met_u ) {
    uv_timer_start(&mot_u->tim_u, _newt_meat_next_cb, 0, 0);

    if ( c3y == mot_u->pok_f(mot_u->ptr_v, met_u->len_d, met_u->hun_y) ) {
      mot_u->ext_u = met_u->nex_u;

      if ( !mot_u->ext_u ) {
        mot_u->ent_u = 0;
      }

      c3_free(met_u);
    }
  }
}

/* _newt_meat_next_cb(): handle next msg after timer.
*/
static void
_newt_meat_next_cb(uv_timer_t* tim_u)
{
  u3_moat* mot_u = tim_u->data;
  _newt_meat_poke(mot_u);
}

/* u3_newt_decode(): decode a (partial) v1 message buffer
*/
c3_o
u3_newt_decode(u3_moat* mot_u, c3_y* buf_y, c3_d len_d)
{
  u3_mess* mes_u = &mot_u->mes_u;

  while ( len_d ) {
    switch ( mes_u->sat_e ) {

      //  read header: [ver:1][tag:1][len:8] = 10 bytes minimum
      //
      case u3_mess_head: {
        c3_y* hed_y = mes_u->hed_u.hed_y;
        c3_y  has_y = mes_u->hed_u.has_y;

        //  accumulate up to 10 bytes
        //
        c3_y  ned_y = 10 - has_y;
        c3_y  cop_y = c3_min(ned_y, len_d);

        memcpy(hed_y + has_y, buf_y, cop_y);
        buf_y += cop_y;
        len_d -= cop_y;
        has_y += cop_y;

        if ( has_y < 10 ) {
          mes_u->hed_u.has_y = has_y;
          continue;
        }

        //  validate version
        //
        if ( 0x1 != hed_y[0] ) {
          fprintf(stderr, "newt: unknown version 0x%x\r\n", hed_y[0]);
          return c3n;
        }

        c3_y tag_y = hed_y[1];
        c3_d val_d = (((c3_d)hed_y[2]) <<  0)
                   | (((c3_d)hed_y[3]) <<  8)
                   | (((c3_d)hed_y[4]) << 16)
                   | (((c3_d)hed_y[5]) << 24)
                   | (((c3_d)hed_y[6]) << 32)
                   | (((c3_d)hed_y[7]) << 40)
                   | (((c3_d)hed_y[8]) << 48)
                   | (((c3_d)hed_y[9]) << 56);

        if ( !val_d ) {
          fprintf(stderr, "newt: zero length message\r\n");
          return c3n;
        }

        if ( 0x0 == tag_y ) {
          //  inline: val_d is payload length
          //
          _newt_mess_tail(mes_u, val_d);
        }
        else if ( 0x1 == tag_y ) {
          //  file: need 2 more bytes for path_len
          //
          if ( has_y < 12 ) {
            c3_y ned2_y = 12 - has_y;
            c3_y cop2_y = c3_min(ned2_y, len_d);
            memcpy(hed_y + has_y, buf_y, cop2_y);
            buf_y += cop2_y;
            len_d -= cop2_y;
            has_y += cop2_y;

            if ( has_y < 12 ) {
              mes_u->hed_u.has_y = has_y;
              //  stash file size in fil_u for later
              //
              mes_u->fil_u.len_d = val_d;
              continue;
            }
          }

          c3_s pat_s = (((c3_s)hed_y[10]) << 0)
                     | (((c3_s)hed_y[11]) << 8);

          if ( !pat_s ) {
            fprintf(stderr, "newt: zero path length\r\n");
            return c3n;
          }

          _newt_mess_file(mes_u, val_d, pat_s);
        }
        else {
          fprintf(stderr, "newt: unknown tag 0x%x\r\n", tag_y);
          return c3n;
        }
      } break;

      case u3_mess_tail: {
        u3_meat* met_u = mes_u->tal_u.met_u;
        c3_d     has_d = mes_u->tal_u.has_d;
        c3_d     ned_d = met_u->len_d - has_d;
        c3_d     cop_d = c3_min(ned_d, len_d);

        memcpy(met_u->hun_y + has_d, buf_y, cop_d);
        buf_y += cop_d;
        len_d -= cop_d;
        ned_d -= cop_d;

        //  moar bytes needed, yield
        //
        if ( ned_d ) {
          mes_u->tal_u.has_d = (has_d + cop_d);
        }
        //  message completed, enqueue and await next header
        //
        else {
          _newt_meat_plan(mot_u, met_u);
          _newt_mess_head(mes_u);
        }
      } break;

      case u3_mess_file: {
        c3_s has_s = mes_u->fil_u.has_s;
        c3_s ned_s = mes_u->fil_u.pat_s - has_s;
        c3_s cop_s = c3_min(ned_s, len_d);

        memcpy(mes_u->fil_u.pat_c + has_s, buf_y, cop_s);
        buf_y += cop_s;
        len_d -= cop_s;
        ned_s -= cop_s;

        if ( ned_s ) {
          mes_u->fil_u.has_s = (has_s + cop_s);
        }
        else {
          mes_u->fil_u.pat_c[mes_u->fil_u.pat_s] = 0;

          //  mmap the file, copy to meat, delete file
          //
          c3_d  dat_d;
          c3_y* dat_y;

          if ( c3n == _newt_file_read(mes_u->fil_u.pat_c,
                                      mes_u->fil_u.len_d,
                                      &dat_d, &dat_y) )
          {
            c3_free(mes_u->fil_u.pat_c);
            _newt_mess_head(mes_u);
            return c3n;
          }

          _newt_file_drop(mes_u->fil_u.pat_c);
          c3_free(mes_u->fil_u.pat_c);

          //  create meat and enqueue
          //
          u3_meat* met_u = c3_malloc(dat_d + sizeof(*met_u));
          met_u->nex_u = 0;
          met_u->len_d = dat_d;
          memcpy(met_u->hun_y, dat_y, dat_d);
          c3_free(dat_y);

          _newt_meat_plan(mot_u, met_u);
          _newt_mess_head(mes_u);
        }
      } break;
    }
  }
  return c3y;
}

/* _newt_read_cb(): stream input callback.
*/
static void
_newt_read_cb(uv_stream_t*    str_u,
              ssize_t         len_i,
              const uv_buf_t* buf_u)
{
  u3_moat* mot_u = (void *)str_u;

  if ( 0 > len_i ) {
    c3_free(buf_u->base);
    uv_read_stop((uv_stream_t*)&mot_u->pyp_u);

    if ( UV_EOF != len_i ) {
      fprintf(stderr, "newt: read failed %s\r\n", uv_strerror(len_i));
    }

    mot_u->bal_f(mot_u->ptr_v, len_i, uv_strerror(len_i));
  }
  //  EAGAIN/EWOULDBLOCK
  //
  else if ( 0 == len_i ) {
    c3_free(buf_u->base);
  }
  else {
    if ( c3n == u3_newt_decode(mot_u, (c3_y*)buf_u->base, (c3_d)len_i) ) {
      mot_u->bal_f(mot_u->ptr_v, -1, "newt-decode");
    }

    c3_free(buf_u->base);
    _newt_meat_poke(mot_u);
  }
}

/* _newt_alloc(): libuv-style allocator.
*/
static void
_newt_alloc(uv_handle_t* had_u,
            size_t len_i,
            uv_buf_t* buf_u)
{
  //  XX pick an appropriate size
  //
  void* ptr_v = c3_malloc(len_i);

  *buf_u = uv_buf_init(ptr_v, len_i);
}

/* u3_newt_read(): start stream reading.
*/
void
u3_newt_read(u3_moat* mot_u)
{
  //  zero-initialize completed msg queue
  //
  mot_u->ent_u = mot_u->ext_u = 0;

  //  store pointer for libuv handle callback
  //
  mot_u->pyp_u.data = mot_u;
  mot_u->tim_u.data = mot_u;

  //  await next msg header
  //
  _newt_mess_head(&mot_u->mes_u);

  {
    c3_i sas_i;

    if ( 0 != (sas_i = uv_read_start((uv_stream_t*)&mot_u->pyp_u,
                                     _newt_alloc,
                                     _newt_read_cb)) )
    {
      fprintf(stderr, "newt: read failed %s\r\n", uv_strerror(sas_i));
      mot_u->bal_f(mot_u->ptr_v, sas_i, uv_strerror(sas_i));
    }
  }
}

/* _moat_stop_cb(): finalize stop/close input stream..
*/
static void
_moat_stop_cb(uv_handle_t* han_u)
{
  u3_moat* mot_u = han_u->data;
  mot_u->bal_f(mot_u->ptr_v, -1, "");
}

/* u3_newt_moat_stop(); newt stop/close input stream.
*/
void
u3_newt_moat_stop(u3_moat* mot_u, u3_moor_bail bal_f)
{
  mot_u->pyp_u.data = mot_u;

  if ( bal_f ) {
    mot_u->bal_f = bal_f;
  }

  uv_close((uv_handle_t*)&mot_u->pyp_u, _moat_stop_cb);
  uv_close((uv_handle_t*)&mot_u->tim_u, 0);

  //  dispose in-process message
  //
  if ( u3_mess_tail == mot_u->mes_u.sat_e ) {
    c3_free(mot_u->mes_u.tal_u.met_u);
    _newt_mess_head(&mot_u->mes_u);
  }
  else if ( u3_mess_file == mot_u->mes_u.sat_e ) {
    c3_free(mot_u->mes_u.fil_u.pat_c);
    _newt_mess_head(&mot_u->mes_u);
  }

  //  dispose pending messages
  {
    u3_meat* met_u = mot_u->ext_u;
    u3_meat* nex_u;

    while ( met_u ) {
      nex_u = met_u->nex_u;
      c3_free(met_u);
      met_u = nex_u;
    }

    mot_u->ent_u = mot_u->ext_u = 0;
  }
}

/* u3_newt_moat_info(): status info as $mass.
*/
u3_noun
u3_newt_moat_info(u3_moat* mot_u)
{
  u3_meat*  met_u = mot_u->ext_u;
  c3_w      len_w = 0;

  while ( met_u ) {
    len_w++;
    met_u = met_u->nex_u;
  }
  return u3_pier_mass(
    c3__moat,
    u3i_list(u3_pier_mase("pending-inbound", u3i_word(len_w)),
             u3_none));
}

/* u3_newt_moat_slog(); print status info.
*/
void
u3_newt_moat_slog(u3_moat* mot_u)
{
  u3_meat* met_u = mot_u->ext_u;
  c3_w     len_w = 0;

    while ( met_u ) {
      len_w++;
      met_u = met_u->nex_u;
    }

  if ( len_w ) {
    u3l_log("    newt: %u inbound ipc messages pending", len_w);
  }
}

/* n_req: write request for newt
*/
typedef struct _n_req {
  uv_write_t wri_u;
  u3_mojo*   moj_u;
  c3_y*      buf_y;
  c3_y       hed_y[10];  //  v1 inline header
} n_req;

/* _newt_write_cb(): generic write callback.
*/
static void
_newt_write_cb(uv_write_t* wri_u, c3_i sas_i)
{
  n_req*   req_u = (n_req*)wri_u;
  u3_mojo* moj_u = req_u->moj_u;

  c3_free(req_u->buf_y);
  c3_free(req_u);

  if ( 0 != sas_i ) {
    if ( UV_ECANCELED == sas_i ) {
      fprintf(stderr, "newt: write canceled\r\n");
    }
    else {
      fprintf(stderr, "newt: write failed %s\r\n", uv_strerror(sas_i));
      moj_u->bal_f(moj_u->ptr_v, sas_i, uv_strerror(sas_i));
    }
  }
}

/* _mojo_stop_cb(): finalize stop/close output stream..
*/
static void
_mojo_stop_cb(uv_handle_t* han_u)
{
  u3_mojo* moj_u = han_u->data;
  moj_u->bal_f(moj_u->ptr_v, -1, "");
}

/* u3_newt_mojo_stop(); newt stop/close output stream.
*/
void
u3_newt_mojo_stop(u3_mojo* moj_u, u3_moor_bail bal_f)
{
  moj_u->pyp_u.data = moj_u;

  if ( bal_f ) {
    moj_u->bal_f = bal_f;
  }

  uv_close((uv_handle_t*)&moj_u->pyp_u, _mojo_stop_cb);
}

/* u3_newt_send(): write buffer to stream (v1 protocol).
**   eve_d: event number (for file naming)
**   mug_l: mug hash (for file naming)
*/
void
u3_newt_send(u3_mojo* moj_u, c3_d len_d, c3_y* byt_y,
             c3_d eve_d, c3_l mug_l)
{
  //  use file-backed for large payloads when pier path available
  //
  if ( len_d > NEWT_MMAP_THRESHOLD && moj_u->pir_c ) {
    c3_c* pat_c = _newt_file_path(moj_u->pir_c, eve_d, mug_l);

    if ( pat_c && c3y == _newt_file_save(pat_c, len_d, byt_y) ) {
      c3_s pat_s = strlen(pat_c);
      c3_y* hed_y = c3_malloc(12 + pat_s);

      //  [0x1][0x1][file_size:8][path_len:2][path]
      //
      hed_y[0]  = 0x1;
      hed_y[1]  = 0x1;                         //  tag: file
      hed_y[2]  = (len_d        & 0xff);
      hed_y[3]  = ((len_d >>  8) & 0xff);
      hed_y[4]  = ((len_d >> 16) & 0xff);
      hed_y[5]  = ((len_d >> 24) & 0xff);
      hed_y[6]  = ((len_d >> 32) & 0xff);
      hed_y[7]  = ((len_d >> 40) & 0xff);
      hed_y[8]  = ((len_d >> 48) & 0xff);
      hed_y[9]  = ((len_d >> 56) & 0xff);
      hed_y[10] = (pat_s        & 0xff);
      hed_y[11] = ((pat_s >> 8) & 0xff);
      memcpy(hed_y + 12, pat_c, pat_s);

      n_req* req_u = c3_malloc(sizeof(*req_u));
      req_u->moj_u = moj_u;
      req_u->buf_y = hed_y;

      uv_buf_t buf_u = uv_buf_init((c3_c*)hed_y, 12 + pat_s);

      c3_i sas_i;
      if ( 0 != (sas_i = uv_write(&req_u->wri_u,
                                  (uv_stream_t*)&moj_u->pyp_u,
                                  &buf_u, 1,
                                  _newt_write_cb)) )
      {
        _newt_file_drop(pat_c);
        c3_free(pat_c);
        c3_free(req_u);
        c3_free(byt_y);
        fprintf(stderr, "newt: file send failed %s\r\n", uv_strerror(sas_i));
        moj_u->bal_f(moj_u->ptr_v, sas_i, uv_strerror(sas_i));
        return;
      }

      c3_free(pat_c);
      c3_free(byt_y);
      return;
    }

    if ( pat_c ) c3_free(pat_c);
    //  fallback to inline
  }

  //  inline: [0x1][0x0][length:8][payload]
  //
  n_req* req_u = c3_malloc(sizeof(*req_u));
  req_u->moj_u = moj_u;
  req_u->buf_y = byt_y;

  req_u->hed_y[0] = 0x1;
  req_u->hed_y[1] = 0x0;                       //  tag: inline
  req_u->hed_y[2] = (len_d        & 0xff);
  req_u->hed_y[3] = ((len_d >>  8) & 0xff);
  req_u->hed_y[4] = ((len_d >> 16) & 0xff);
  req_u->hed_y[5] = ((len_d >> 24) & 0xff);
  req_u->hed_y[6] = ((len_d >> 32) & 0xff);
  req_u->hed_y[7] = ((len_d >> 40) & 0xff);
  req_u->hed_y[8] = ((len_d >> 48) & 0xff);
  req_u->hed_y[9] = ((len_d >> 56) & 0xff);

  {
    uv_buf_t buf_u[2] = {
      uv_buf_init((c3_c*)req_u->hed_y, 10),
      uv_buf_init((c3_c*)req_u->buf_y, len_d)
    };

    c3_i sas_i;

    if ( 0 != (sas_i = uv_write(&req_u->wri_u,
                                (uv_stream_t*)&moj_u->pyp_u,
                                buf_u, 2,
                                _newt_write_cb)) )
    {
      c3_free(req_u);
      fprintf(stderr, "newt: inline send failed %s\r\n", uv_strerror(sas_i));
      moj_u->bal_f(moj_u->ptr_v, sas_i, uv_strerror(sas_i));
    }
  }
}
