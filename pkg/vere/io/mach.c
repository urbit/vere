/// @file

#include "retrieve.h"
#include "uv.h"
#include "vere.h"

#include "noun.h"
#include <stdio.h>

#ifndef U3_OS_windows
#include <netinet/tcp.h>
#endif

//  connect timeout
//
#define MACH_CONNECT_MS  30000

//  time a closing connection may spend flushing its outgoing queue
//
#define MACH_DRAIN_MS    30000

//  keepalive: probe after 120s idle, then every 10s; six unanswered
//  probes kill the connection, so a dead peer is found in 180s
//
#define MACH_KEEPIDLE_S  120
#define MACH_KEEPINTVL_S 10
#define MACH_KEEPCNT     6

//  sent bytes unacknowledged this long also kill the connection, so a
//  dead peer is found in 180s whether or not data is in flight
//
#define MACH_RXT_S       180

//  how often to check a write in flight for progress
//
#define MACH_STALL_MS    10000

//  most bytes in one %received event
//
#define MACH_EVENT_MAX   (1 << 20)

//  most bytes queued per connection. a full incoming queue stops
//  reading, so tcp slows the peer down, until a %received takes it;
//  a full outgoing queue closes the connection
//
#define MACH_IN_MAX      MACH_EVENT_MAX
#define MACH_OUT_MAX     (16 << 20)

//  most bytes in one write. on unix the stall timer sees partial
//  writes, but on windows libuv reports a write only once it ends
//
#define MACH_WRITE_MAX   (64 << 10)

struct _u3_mach;

/* u3_mbuf: queued bytes
 */
typedef struct _u3_mbuf {
  c3_y*            buf_y;  // bytes, owned
  c3_w             len_w;  // length
  c3_w             pos_w;  // bytes already consumed
  struct _u3_mbuf* nex_u;  // next buffer
} u3_mbuf;

/* u3_mque: queue of buffers
 */
typedef struct _u3_mque {
  u3_mbuf* hed_u;  // first buffer
  u3_mbuf* tal_u;  // last buffer
  c3_w     len_w;  // unconsumed bytes in queue
} u3_mque;

/* u3_tcon: tcp connection
 */
typedef struct _u3_tcon {
  c3_d             id_d;   // id from mach
  uv_tcp_t*        tcp_u;  // tcp handle, owned
  uv_timer_t       tim_u;  // connect, stall, then drain timer
  struct _u3_mach* mac_u;  // driver, for planning events
  struct _u3_tcon* nex_u;  // next tcon in list
  c3_o             opn_o;  // opened
  c3_o             clo_o;  // %close received, draining
  const c3_c*      cat_c;  // peer closed; category for %closed
  const c3_c*      why_c;  // peer closed; cause for %closed
  u3_mque          out_u;  // outgoing bytes
  c3_o             wri_o;  // write in flight
  c3_w             wrl_w;  // bytes in the write in flight
  size_t           lef_i;  // bytes of it unwritten, at last check
  c3_d             pro_d;  // loop time of last write progress, ms
  c3_o             paz_o;  // reading paused, incoming queue full
  u3_mque          inc_u;  // incoming bytes
  u3_ovum*         egg_u;  // %received event in flight
} u3_tcon;

/* u3_mach: list of connections
 */
typedef struct _u3_mach {
  u3_auto  car_u;  //  driver
  u3_tcon* con_u;  //  connection list
} u3_mach;

/* _mach_que_push(): append bytes to a queue, taking ownership.
 */
static void
_mach_que_push(u3_mque* que_u, c3_y* buf_y, c3_w len_w)
{
  u3_mbuf* buf_u = c3_malloc(sizeof(*buf_u));

  buf_u->buf_y = buf_y;
  buf_u->len_w = len_w;
  buf_u->pos_w = 0;
  buf_u->nex_u = 0;

  if ( que_u->tal_u ) {
    que_u->tal_u->nex_u = buf_u;
  }
  else {
    que_u->hed_u = buf_u;
  }

  que_u->tal_u  = buf_u;
  que_u->len_w += len_w;
}

/* _mach_que_pop(): free the first buffer in a queue.
 */
static void
_mach_que_pop(u3_mque* que_u)
{
  u3_mbuf* buf_u = que_u->hed_u;

  que_u->len_w -= buf_u->len_w - buf_u->pos_w;
  que_u->hed_u  = buf_u->nex_u;

  if ( !que_u->hed_u ) {
    que_u->tal_u = 0;
  }

  c3_free(buf_u->buf_y);
  c3_free(buf_u);
}

/* _mach_que_free(): free every buffer in a queue.
 */
static void
_mach_que_free(u3_mque* que_u)
{
  while ( que_u->hed_u ) {
    _mach_que_pop(que_u);
  }
}

/* _mach_que_take(): copy up to [max_w] bytes off the front of a queue.
 */
static c3_w
_mach_que_take(u3_mque* que_u, c3_y* buf_y, c3_w max_w)
{
  c3_w len_w = 0;

  while ( que_u->hed_u && (len_w < max_w) ) {
    u3_mbuf* buf_u = que_u->hed_u;
    c3_w     cop_w = c3_min(buf_u->len_w - buf_u->pos_w, max_w - len_w);

    memcpy(buf_y + len_w, buf_u->buf_y + buf_u->pos_w, cop_w);
    len_w        += cop_w;
    buf_u->pos_w += cop_w;
    que_u->len_w -= cop_w;

    if ( buf_u->pos_w == buf_u->len_w ) {
      //  pop adjusts len_w by the unconsumed rest, which is now zero
      //
      _mach_que_pop(que_u);
    }
  }

  return len_w;
}

/* _mach_plan(): inject an event into %mach.
 */
static u3_ovum*
_mach_plan(u3_mach* mac_u, u3_noun cad)
{
  u3_noun wir = u3nc(c3__mach, u3_nul);
  return u3_auto_plan(&mac_u->car_u, u3_ovum_init(0, c3__m, wir, cad));
}

/* _mach_plan_closed(): tell %mach a connection closed, giving
 * a $wye as category and cause.
 */
static void
_mach_plan_closed(u3_mach*    mac_u,
                  c3_d        id_d,
                  const c3_c* cat_c,
                  const c3_c* why_c)
{
  _mach_plan(mac_u,
             u3nq(u3i_string("closed"), u3i_chub(id_d),
                  u3i_string(cat_c), u3i_string(why_c)));
}

/* _mach_con_find(): find a connection by id.
 */
static u3_tcon*
_mach_con_find(u3_mach* mac_u, c3_d id_d)
{
  u3_tcon* con_u = mac_u->con_u;

  while ( con_u && (id_d != con_u->id_d) ) {
    con_u = con_u->nex_u;
  }

  return con_u;
}

/* _mach_tcp_free_cb(): free a closed tcp handle.
 */
static void
_mach_tcp_free_cb(uv_handle_t* han_u)
{
  c3_free(han_u);
}

/* _mach_con_free_cb(): free a connection once its timer has closed.
 */
static void
_mach_con_free_cb(uv_handle_t* han_u)
{
  c3_free(han_u->data);
}

/* _mach_sock_close(): close the socket, connecting or connected.
 *
 *   libuv cancels a closing handle's requests, so their callbacks
 *   check for UV_ECANCELED and touch nothing else; the connection
 *   may be freed by then.
 */
static void
_mach_sock_close(u3_tcon* con_u)
{
  if ( con_u->tcp_u ) {
    uv_close((uv_handle_t*)con_u->tcp_u, _mach_tcp_free_cb);
    con_u->tcp_u = 0;
  }
}

/* _mach_live(): is the connection open, with a socket to use?
 */
static c3_o
_mach_live(u3_tcon* con_u)
{
  return __( (c3y == con_u->opn_o) && con_u->tcp_u );
}

/* _mach_con_free(): unlink a connection, close its handles, then free it.
 *
 *   libuv owns a handle until its close callback runs, so the
 *   struct holding the timer is freed in _mach_con_free_cb().
 */
static void
_mach_con_free(u3_tcon* con_u)
{
  u3_tcon** pre_u = &con_u->mac_u->con_u;

  while ( *pre_u && (con_u != *pre_u) ) {
    pre_u = &(*pre_u)->nex_u;
  }

  if ( *pre_u ) {
    *pre_u = con_u->nex_u;
  }

  _mach_sock_close(con_u);

  //  the event outlives us; detach it
  //
  if ( con_u->egg_u ) {
    con_u->egg_u->ptr_v = 0;
    con_u->egg_u = 0;
  }

  _mach_que_free(&con_u->out_u);
  _mach_que_free(&con_u->inc_u);

  uv_timer_stop(&con_u->tim_u);
  uv_close((uv_handle_t*)&con_u->tim_u, _mach_con_free_cb);
}

/* _mach_con_shut(): tell %mach a connection closed, and free it now.
 */
static void
_mach_con_shut(u3_tcon* con_u, const c3_c* cat_c, const c3_c* why_c)
{
  _mach_plan_closed(con_u->mac_u, con_u->id_d, cat_c, why_c);
  _mach_con_free(con_u);
}

static void
_mach_pump(u3_tcon* con_u);

/* _mach_con_dead(): the peer is gone; close the socket, then
 * deliver what we already received before telling %mach.
 */
static void
_mach_con_dead(u3_tcon* con_u, const c3_c* cat_c, const c3_c* why_c)
{
  con_u->cat_c = cat_c;
  con_u->why_c = why_c;
  uv_timer_stop(&con_u->tim_u);
  _mach_sock_close(con_u);
  _mach_que_free(&con_u->out_u);
  con_u->wri_o = c3n;
  _mach_pump(con_u);
}

static void
_mach_tcp_alloc_cb(uv_handle_t* han_u, size_t siz_i, uv_buf_t* buf_u);

static void
_mach_tcp_read_cb(uv_stream_t* str_u, ssize_t red_i, const uv_buf_t* buf_u);

/* _mach_read_start(): start reading; c3n if that killed the connection.
 */
static c3_o
_mach_read_start(u3_tcon* con_u)
{
  c3_i sas_i = uv_read_start((uv_stream_t*)con_u->tcp_u,
                             _mach_tcp_alloc_cb, _mach_tcp_read_cb);

  if ( sas_i ) {
    u3l_log("mach: %" PRIu64 ": read: %s", con_u->id_d, uv_strerror(sas_i));
    _mach_con_dead(con_u, "peer", "reset");
    return c3n;
  }

  return c3y;
}

/* _mach_read_stop(): stop reading.
 */
static void
_mach_read_stop(u3_tcon* con_u)
{
  if ( con_u->tcp_u ) {
    uv_read_stop((uv_stream_t*)con_u->tcp_u);
  }
}

/* _mach_received_news(): a %received event changed state.
 */
static void
_mach_received_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_tcon* con_u = egg_u->ptr_v;

  if ( !con_u || (u3_ovum_work == new_e) ) {
    return;
  }

  con_u->egg_u = 0;

  if ( u3_ovum_done == new_e ) {
    _mach_pump(con_u);
  }
  else {
    //  dropped: the stream has a gap, so it can't go on
    //
    u3l_log("mach: %" PRIu64 ": %%received dropped", con_u->id_d);
    _mach_con_shut(con_u, "local", "dropped");
  }
}

/* _mach_received_bail(): a %received event crashed.
 */
static void
_mach_received_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3_tcon* con_u = egg_u->ptr_v;

  u3_auto_bail_slog(egg_u, lud);
  u3_ovum_free(egg_u);

  //  the stream has a gap, so it can't go on
  //
  if ( con_u ) {
    con_u->egg_u = 0;
    u3l_log("mach: %" PRIu64 ": %%received failed", con_u->id_d);
    _mach_con_shut(con_u, "local", "dropped");
  }
}

/* _mach_pump(): send the next %received event, if none is in flight,
 * and resume reading once the queue has room; once a dead connection
 * has delivered everything, close it.
 */
static void
_mach_pump(u3_tcon* con_u)
{
  if ( con_u->egg_u ) {
    return;
  }

  if ( con_u->inc_u.len_w ) {
    c3_w     len_w = c3_min(con_u->inc_u.len_w, MACH_EVENT_MAX);
    u3i_slab sab_u;

    u3i_slab_init(&sab_u, 3, len_w);
    _mach_que_take(&con_u->inc_u, sab_u.buf_y, len_w);

    u3_noun dat = u3nc(u3i_word(len_w), u3i_slab_mint(&sab_u));
    u3_noun cad = u3nt(u3i_string("received"), u3i_chub(con_u->id_d), dat);

    con_u->egg_u = _mach_plan(con_u->mac_u, cad);
    u3_auto_peer(con_u->egg_u, con_u,
                 _mach_received_news, _mach_received_bail);

    if (  (c3y == con_u->paz_o)
       && (con_u->inc_u.len_w < MACH_IN_MAX)
       && (c3y == _mach_live(con_u))
       && (c3n == con_u->clo_o) )
    {
      con_u->paz_o = c3n;
      _mach_read_start(con_u);
    }
  }
  else if ( con_u->why_c ) {
    _mach_con_shut(con_u, con_u->cat_c, con_u->why_c);
  }
}

/* _mach_tcp_alloc_cb(): make a buffer for a plain read.
 */
static void
_mach_tcp_alloc_cb(uv_handle_t* han_u, size_t siz_i, uv_buf_t* buf_u)
{
  buf_u->base = c3_malloc(siz_i);
  buf_u->len  = siz_i;
}

/* _mach_tcp_read_cb(): bytes arrived on a plain socket, or the peer
 * closed.
 *
 *   the buffer becomes the queue's, trimmed to what we read.
 */
static void
_mach_tcp_read_cb(uv_stream_t* str_u, ssize_t red_i, const uv_buf_t* buf_u)
{
  u3_tcon* con_u = str_u->data;

  if ( red_i <= 0 ) {
    c3_free(buf_u->base);

    if ( red_i < 0 ) {
      //  keepalive and retransmission failures show up as a
      //  timed-out read
      //
      const c3_c* why_c = ( UV_EOF == red_i )       ? "done"
                        : ( UV_ETIMEDOUT == red_i ) ? "dead"
                        : "reset";

      u3l_log("mach: %" PRIu64 ": %s", con_u->id_d, uv_strerror(red_i));
      _mach_con_dead(con_u, "peer", why_c);
    }
    return;
  }

  c3_y* buf_y = c3_realloc(buf_u->base, red_i);
  _mach_que_push(&con_u->inc_u, buf_y, (c3_w)red_i);

  //  the queue may pass the cap by one read, at most
  //
  if ( con_u->inc_u.len_w >= MACH_IN_MAX ) {
    uv_read_stop(str_u);
    con_u->paz_o = c3y;
  }

  _mach_pump(con_u);
}

/* _mach_shutdown_cb(): our fin went out after the last write.
 */
static void
_mach_shutdown_cb(uv_shutdown_t* req_u, c3_i sas_i)
{
  u3_tcon* con_u = req_u->data;

  c3_free(req_u);

  if ( UV_ECANCELED == sas_i ) {
    return;
  }

  if ( sas_i ) {
    u3l_log("mach: %" PRIu64 ": shutdown: %s", con_u->id_d, uv_strerror(sas_i));
  }

  _mach_con_shut(con_u, "close", "done");
}

/* _mach_flushed(): a closing connection has sent everything; send
 * our fin, then end it.
 */
static void
_mach_flushed(u3_tcon* con_u)
{
  uv_shutdown_t* req_u = c3_malloc(sizeof(*req_u));
  c3_i           sas_i;

  req_u->data = con_u;

  if ( 0 == (sas_i = uv_shutdown(req_u, (uv_stream_t*)con_u->tcp_u,
                                 _mach_shutdown_cb)) )
  {
    return;
  }

  u3l_log("mach: %" PRIu64 ": shutdown: %s", con_u->id_d, uv_strerror(sas_i));
  c3_free(req_u);
  _mach_con_shut(con_u, "close", "done");
}

static void
_mach_write_next(u3_tcon* con_u);

/* _mach_write_done(): a write finished, or failed with reason [why_c].
 */
static void
_mach_write_done(u3_tcon* con_u, const c3_c* why_c)
{
  con_u->wri_o = c3n;

  if ( why_c ) {
    _mach_con_dead(con_u, "peer", why_c);
    return;
  }

  {
    u3_mbuf* buf_u = con_u->out_u.hed_u;

    buf_u->pos_w       += con_u->wrl_w;
    con_u->out_u.len_w -= con_u->wrl_w;

    if ( buf_u->pos_w == buf_u->len_w ) {
      _mach_que_pop(&con_u->out_u);
    }
  }

  if ( !con_u->out_u.hed_u ) {
    if ( c3y == con_u->clo_o ) {
      _mach_flushed(con_u);
      return;
    }

    uv_timer_stop(&con_u->tim_u);
  }

  _mach_write_next(con_u);
}

/* _mach_tcp_write_cb(): a plain write finished.
 */
static void
_mach_tcp_write_cb(uv_write_t* req_u, c3_i sas_i)
{
  u3_tcon* con_u = req_u->data;

  c3_free(req_u);

  if ( UV_ECANCELED == sas_i ) {
    return;
  }

  if ( sas_i ) {
    u3l_log("mach: %" PRIu64 ": write: %s", con_u->id_d, uv_strerror(sas_i));
  }

  _mach_write_done(con_u, !sas_i                   ? 0
                        : (UV_ETIMEDOUT == sas_i) ? "dead"
                        : "reset");
}

/* _mach_stall_cb(): check the write in flight; if no bytes have left
 * it for MACH_RXT_S, the peer stopped taking them.
 */
static void
_mach_stall_cb(uv_timer_t* tim_u)
{
  u3_tcon* con_u = tim_u->data;
  c3_d     now_d = uv_now(u3L);

  if ( con_u->tcp_u && (c3y == con_u->wri_o) ) {
    size_t lef_i = uv_stream_get_write_queue_size((uv_stream_t*)con_u->tcp_u);

    if ( lef_i < con_u->lef_i ) {
      con_u->lef_i = lef_i;
      con_u->pro_d = now_d;
    }
  }

  if ( (now_d - con_u->pro_d) < (MACH_RXT_S * 1000) ) {
    return;
  }

  u3l_log("mach: %" PRIu64 ": stalled, %u bytes unsent",
          con_u->id_d, con_u->out_u.len_w);
  _mach_con_dead(con_u, "peer", "dead");
}

/* _mach_write_next(): start writing the next slice of the queue, and
 * unless we're draining on %close, watch it for progress.
 */
static void
_mach_write_next(u3_tcon* con_u)
{
  if (  (c3n == _mach_live(con_u))
     || (c3y == con_u->wri_o)
     || !con_u->out_u.hed_u )
  {
    return;
  }

  u3_mbuf*    buf_u = con_u->out_u.hed_u;
  c3_w        len_w = c3_min(buf_u->len_w - buf_u->pos_w, MACH_WRITE_MAX);
  uv_buf_t    buf_v = uv_buf_init((c3_c*)buf_u->buf_y + buf_u->pos_w, len_w);
  uv_write_t* req_u = c3_malloc(sizeof(*req_u));
  c3_i        sas_i;

  con_u->wri_o = c3y;
  con_u->wrl_w = len_w;
  req_u->data  = con_u;

  if ( 0 != (sas_i = uv_write(req_u, (uv_stream_t*)con_u->tcp_u,
                              &buf_v, 1, _mach_tcp_write_cb)) )
  {
    u3l_log("mach: %" PRIu64 ": write: %s", con_u->id_d, uv_strerror(sas_i));
    c3_free(req_u);
    _mach_write_done(con_u, "reset");
    return;
  }

  if ( c3n == con_u->clo_o ) {
    con_u->lef_i = uv_stream_get_write_queue_size((uv_stream_t*)con_u->tcp_u);
    con_u->pro_d = uv_now(u3L);
    uv_timer_start(&con_u->tim_u, _mach_stall_cb,
                   MACH_STALL_MS, MACH_STALL_MS);
  }
}

/* _mach_open(): the connection is ready; start reading and writing.
 */
static void
_mach_open(u3_tcon* con_u)
{
  uv_timer_stop(&con_u->tim_u);
  con_u->opn_o = c3y;

  _mach_plan(con_u->mac_u,
             u3nc(u3i_string("opened"), u3i_chub(con_u->id_d)));

  if ( c3y == _mach_read_start(con_u) ) {
    _mach_write_next(con_u);
  }
}

/* _mach_timeout_cb(): give up on a connection that took too long.
 */
static void
_mach_timeout_cb(uv_timer_t* tim_u)
{
  u3_tcon* con_u = tim_u->data;

  u3l_log("mach: connect %" PRIu64 ": timeout", con_u->id_d);
  _mach_con_shut(con_u, "connect", "timeout");
}

/* _mach_drain_cb(): give up flushing a closing connection.
 */
static void
_mach_drain_cb(uv_timer_t* tim_u)
{
  u3_tcon* con_u = tim_u->data;

  u3l_log("mach: close %" PRIu64 ": drain timeout, %u bytes unsent",
          con_u->id_d, con_u->out_u.len_w);
  _mach_con_shut(con_u, "close", "timeout");
}

/* _mach_keepalive(): set keepalive and retransmission timing, the
 * same on every platform.
 *
 *   uv_tcp_keepalive() sets only the idle time, and libuv picks the
 *   probe interval and count, so we set those ourselves. keepalive
 *   covers only idle connections; while sent bytes wait for an ack,
 *   each os gives up retransmitting after its own time, so we bound
 *   that too: TCP_USER_TIMEOUT on linux (ms), TCP_RXT_CONNDROPTIME on
 *   macos (s), TCP_MAXRT on windows (s).
 */
static c3_o
_mach_keepalive(u3_tcon* con_u, uv_tcp_t* tcp_u)
{
  uv_os_fd_t fd_u;
  c3_i       ivl_i = MACH_KEEPINTVL_S;
  c3_i       cnt_i = MACH_KEEPCNT;
  c3_i       sas_i;
#if defined(U3_OS_linux)
  c3_i       rxt_i = MACH_RXT_S * 1000;
  c3_i       rxo_i = TCP_USER_TIMEOUT;
#elif defined(U3_OS_osx)
  c3_i       rxt_i = MACH_RXT_S;
  c3_i       rxo_i = TCP_RXT_CONNDROPTIME;
#elif defined(U3_OS_windows)
  DWORD      rxt_i = MACH_RXT_S;
  c3_i       rxo_i = TCP_MAXRT;
#else
#error "mach: no retransmission timeout for this platform"
#endif

  if (  (0 != (sas_i = uv_tcp_keepalive(tcp_u, 1, MACH_KEEPIDLE_S)))
     || (0 != (sas_i = uv_fileno((uv_handle_t*)tcp_u, &fd_u))) )
  {
    u3l_log("mach: connect %" PRIu64 ": keepalive: %s",
            con_u->id_d, uv_strerror(sas_i));
    return c3n;
  }

#ifdef U3_OS_windows
  SOCKET sok_u = (SOCKET)fd_u;
#else
  c3_i   sok_u = fd_u;
#endif

  if (  setsockopt(sok_u, IPPROTO_TCP, TCP_KEEPINTVL,
                   (const c3_c*)&ivl_i, sizeof(ivl_i))
     || setsockopt(sok_u, IPPROTO_TCP, TCP_KEEPCNT,
                   (const c3_c*)&cnt_i, sizeof(cnt_i))
     || setsockopt(sok_u, IPPROTO_TCP, rxo_i,
                   (const c3_c*)&rxt_i, sizeof(rxt_i)) )
  {
#ifdef U3_OS_windows
    u3l_log("mach: connect %" PRIu64 ": keepalive: error %d",
            con_u->id_d, WSAGetLastError());
#else
    u3l_log("mach: connect %" PRIu64 ": keepalive: %s",
            con_u->id_d, strerror(errno));
#endif
    return c3n;
  }

  return c3y;
}

/* _mach_connect_cb(): the tcp connect finished.
 */
static void
_mach_connect_cb(uv_connect_t* req_u, c3_i sas_i)
{
  u3_tcon* con_u = req_u->data;

  c3_free(req_u);

  //  we closed the handle; con_u may be gone
  //
  if ( UV_ECANCELED == sas_i ) {
    return;
  }

  if ( sas_i ) {
    u3l_log("mach: connect %" PRIu64 ": %s", con_u->id_d, uv_strerror(sas_i));
    _mach_con_shut(con_u, "connect",
                   (UV_ETIMEDOUT == sas_i) ? "timeout" : "refused");
    return;
  }

  //  a socket without our keepalive timing would behave differently
  //  on each platform, so we count it as a failed connect
  //
  if ( c3n == _mach_keepalive(con_u, con_u->tcp_u) ) {
    _mach_con_shut(con_u, "connect", "refused");
    return;
  }

  uv_tcp_nodelay(con_u->tcp_u, 1);
  _mach_open(con_u);
}

/* _mach_addr(): read a fief into a socket address, and print it into
 * [adr_c] for logs. c3n if the fief is malformed.
 */
static c3_o
_mach_addr(u3_noun fef, struct sockaddr_storage* sas_u, c3_c* adr_c, size_t len_i)
{
  u3_noun hed, p, q;
  c3_w    por_w;

  if (  (c3n == u3r_trel(fef, &hed, &p, &q))
     || (c3n == u3r_safe_word(q, &por_w)) || (por_w > 65535) )
  {
    return c3n;
  }

  memset(sas_u, 0, sizeof(*sas_u));

  // IPv4
  if ( c3__if == hed ) {
    c3_w           ip_w;
    struct in_addr add_u;

    if ( c3n == u3r_safe_word(p, &ip_w) ) {
      return c3n;
    }

    add_u.s_addr = htonl(ip_w);

    if (  uv_inet_ntop(AF_INET, &add_u, adr_c, len_i)
       || uv_ip4_addr(adr_c, (c3_i)por_w, (struct sockaddr_in*)sas_u) )
    {
      return c3n;
    }

    return c3y;
  }
  // IPv6
  else if ( c3__is == hed ) {
    c3_y ip_y[16];
    c3_y net_y[16];

    if ( (c3n == u3a_is_atom(p)) || (u3r_met(3, p) > 16) ) {
      return c3n;
    }

    //  atoms are little-endian; in6_addr is big-endian
    u3r_bytes(0, 16, ip_y, p);
    for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
      net_y[i_w] = ip_y[15 - i_w];
    }

    if (  uv_inet_ntop(AF_INET6, net_y, adr_c, len_i)
       || uv_ip6_addr(adr_c, (c3_i)por_w, (struct sockaddr_in6*)sas_u) )
    {
      return c3n;
    }

    return c3y;
  }

  return c3n;
}

/* _mach_connect(): start connecting to a fief, and time it.
 */
static void
_mach_connect(u3_mach* mac_u, c3_d id_d, u3_noun fef)
{
  struct sockaddr_storage sas_u;
  c3_c                    adr_c[INET6_ADDRSTRLEN];
  uv_connect_t*           req_u;
  u3_tcon*                con_u;
  c3_i                    sas_i;

  if ( _mach_con_find(mac_u, id_d) ) {
    u3l_log("mach: connect: id %" PRIu64 " already open", id_d);
    return;
  }

  if ( c3n == _mach_addr(fef, &sas_u, adr_c, sizeof(adr_c)) ) {
    u3l_log("mach: connect %" PRIu64 ": bad fief", id_d);
    _mach_plan_closed(mac_u, id_d, "connect", "invalid");
    return;
  }

  con_u = c3_calloc(sizeof(*con_u));
  con_u->id_d  = id_d;
  con_u->mac_u = mac_u;
  con_u->opn_o = c3n;
  con_u->clo_o = c3n;
  con_u->wri_o = c3n;
  con_u->paz_o = c3n;
  con_u->nex_u = mac_u->con_u;
  mac_u->con_u = con_u;

  uv_timer_init(u3L, &con_u->tim_u);
  con_u->tim_u.data = con_u;

  u3l_log("mach: connect %" PRIu64 " %s port %u", id_d, adr_c,
          ntohs(((struct sockaddr_in*)&sas_u)->sin_port));

  con_u->tcp_u = c3_malloc(sizeof(*con_u->tcp_u));

  if ( 0 != (sas_i = uv_tcp_init(u3L, con_u->tcp_u)) ) {
    u3l_log("mach: connect %" PRIu64 ": %s", id_d, uv_strerror(sas_i));
    c3_free(con_u->tcp_u);
    con_u->tcp_u = 0;
    _mach_con_shut(con_u, "connect", "refused");
    return;
  }

  con_u->tcp_u->data = con_u;

  req_u = c3_malloc(sizeof(*req_u));
  req_u->data = con_u;

  if ( 0 != (sas_i = uv_tcp_connect(req_u, con_u->tcp_u,
                                    (const struct sockaddr*)&sas_u,
                                    _mach_connect_cb)) )
  {
    u3l_log("mach: connect %" PRIu64 ": %s", id_d, uv_strerror(sas_i));
    c3_free(req_u);
    _mach_con_shut(con_u, "connect", "refused");
    return;
  }

  uv_timer_start(&con_u->tim_u, _mach_timeout_cb, MACH_CONNECT_MS, 0);
}

/* _mach_send(): queue bytes to send on a connection.
 */
static void
_mach_send(u3_mach* mac_u, c3_d id_d, c3_w len_w, u3_atom dat)
{
  u3_tcon* con_u = _mach_con_find(mac_u, id_d);

  if ( !con_u ) {
    u3l_log("mach: send %" PRIu64 ": no connection", id_d);
    return;
  }

  if ( (c3y == con_u->clo_o) || con_u->why_c ) {
    u3l_log("mach: send %" PRIu64 ": closing, dropped %u bytes", id_d, len_w);
    return;
  }

  if ( !len_w ) {
    return;
  }

  if ( (con_u->out_u.len_w + (c3_d)len_w) > MACH_OUT_MAX ) {
    u3l_log("mach: send %" PRIu64 ": outgoing queue full", id_d);
    _mach_con_shut(con_u, "local", "overflowed");
    return;
  }

  c3_y* buf_y = c3_malloc(len_w);
  u3r_bytes(0, len_w, buf_y, dat);

  _mach_que_push(&con_u->out_u, buf_y, len_w);
  _mach_write_next(con_u);
}

/* _mach_close(): close a connection, flushing what we owe the peer.
 */
static void
_mach_close(u3_mach* mac_u, c3_d id_d)
{
  u3_tcon* con_u = _mach_con_find(mac_u, id_d);

  if ( !con_u ) {
    u3l_log("mach: close %" PRIu64 ": no connection", id_d);
    return;
  }

  if ( c3y == con_u->clo_o ) {
    return;
  }

  u3l_log("mach: close %" PRIu64, id_d);

  //  nothing to flush, or no way to flush it
  //
  if ( (c3n == _mach_live(con_u)) || !con_u->out_u.hed_u ) {
    _mach_con_shut(con_u, "close", "done");
    return;
  }

  //  stop delivering; the agent asked to go
  //
  con_u->clo_o = c3y;
  _mach_read_stop(con_u);
  _mach_que_free(&con_u->inc_u);

  uv_timer_start(&con_u->tim_u, _mach_drain_cb, MACH_DRAIN_MS, 0);
}

/* _mach_io_kick(): apply effects.
 */
static c3_o _mach_io_kick(u3_auto *car_u, u3_noun wir, u3_noun cad) {
  u3_mach* mac_u = (u3_mach*)car_u;
  u3_noun i_wir;
  c3_o ret_o;
  u3_noun tag, tmp;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3__mach != i_wir)
     || (c3n == u3r_cell(cad, &tag, &tmp)) )
  {
    ret_o = c3n;
  }
  //  [%connect id=@ud =fief]
  //
  else if ( c3y == u3r_sing_c("connect", tag) ) {
    u3_noun id, fef;
    c3_d    id_d;

    if (  (c3n == u3r_cell(tmp, &id, &fef))
       || (c3n == u3r_safe_chub(id, &id_d)) )
    {
      ret_o = c3n;
    } else {
      ret_o = c3y;
      _mach_connect(mac_u, id_d, fef);
    }
  }
  //  [%send id=@ud =octs]
  //
  else if ( c3__send == tag ) {
    u3_noun id, p, q;
    c3_d    id_d;
    c3_w    len_w;

    if (  (c3n == u3r_trel(tmp, &id, &p, &q))
       || (c3n == u3r_safe_chub(id, &id_d))
       || (c3n == u3r_safe_word(p, &len_w))
       || (c3n == u3a_is_atom(q)) )
    {
      ret_o = c3n;
    }
    else {
      ret_o = c3y;
      _mach_send(mac_u, id_d, len_w, q);
    }
  }
  //  [%close id=@ud]
  //
  else if ( c3y == u3r_sing_c("close", tag) ) {
    c3_d id_d;

    if ( c3n == u3r_safe_chub(tmp, &id_d) ) {
      ret_o = c3n;
    }
    else {
      ret_o = c3y;
      _mach_close(mac_u, id_d);
    }
  }
  else {
    ret_o = c3n;
  }

  u3z(wir);
  u3z(cad);
  return ret_o;
}

/* _mach_born_news(): callback for %born.
 */
static void _mach_born_news(u3_ovum *egg_u, u3_ovum_news new_e) {
  u3_auto *car_u = egg_u->car_u;

  if (u3_ovum_done == new_e) {
    car_u->liv_o = c3y;
  }
}

/* _mach_born_bail(): callback for failed %born.
 */
static void _mach_born_bail(u3_ovum *egg_u, u3_noun lud) {
  u3l_log("mach: %%born failure");
  u3z(lud);
  u3_ovum_free(egg_u);
}

/* _mach_io_talk(): notify %mach that we're live.
 */
static void _mach_io_talk(u3_auto *car_u) {
  u3_noun wir = u3nc(c3__mach, u3_nul);
  u3_noun cad = u3nc(c3__born, u3_nul);

  u3_auto_peer(
      u3_auto_plan(car_u, u3_ovum_init(0, c3__m, wir, cad)),
      0,
      _mach_born_news,
      _mach_born_bail);

  car_u->liv_o = c3y;
}

/* _mach_io_exit(): shut down the driver.
 */
static void _mach_io_exit(u3_auto *car_u) {
  u3_mach* mac_u = (u3_mach*)car_u;

  while ( mac_u->con_u ) {
    _mach_con_free(mac_u->con_u);
  }

  c3_free(mac_u);
}

/* u3_mach_io_init(): initialize tcp driver.
 */
u3_auto *u3_mach_io_init(u3_pier *pir_u) {
  u3_mach *mac_u = c3_calloc(sizeof(*mac_u));

  u3_auto *car_u = &mac_u->car_u;
  car_u->nam_m = c3__mach;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _mach_io_talk;
  car_u->io.kick_f = _mach_io_kick;
  car_u->io.exit_f = _mach_io_exit;

  return car_u;
}
