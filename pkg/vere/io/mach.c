/// @file

#include "h2o/socket.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"
#include "retrieve.h"
#include "uv.h"
#include "vere.h"

#include "noun.h"
#include <stdio.h>

#ifndef U3_OS_windows
#include <netinet/tcp.h>
#endif

//  connect timeout, covering dns, tcp connect, and tls handshake
//
#define MACH_CONNECT_MS  30000

//  happy eyeballs (rfc 8305): start each connection attempt this long
//  after the one before, or at once if the one before fails
//
#define MACH_STAGGER_MS  250

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

//  most bytes in one %received event
//
#define MACH_EVENT_MAX   (1 << 20)

//  most bytes queued in either direction, per connection. a full
//  outgoing queue closes the connection; a full incoming queue stops
//  reading, so tcp slows the peer down, until arvo drains it to half
//
#define MACH_QUEUE_MAX   (64 << 20)
#define MACH_QUEUE_LOW   (MACH_QUEUE_MAX / 2)

struct _u3_mach;
struct _u3_tcon;

/* u3_madr: an address to try
 */
typedef struct _u3_madr {
  struct sockaddr_storage sas_u;  //  address and port
  c3_w                    hoi_w;  //  host it came from
  struct _u3_madr*        nex_u;  //  next address
} u3_madr;

/* u3_matt: a connection attempt in flight
 *
 *   freed in its connect callback, which libuv also calls, with
 *   UV_ECANCELED, when we close the handle of an attempt that lost.
 */
typedef struct _u3_matt {
  uv_connect_t     req_u;  //  libuv request
  uv_tcp_t*        tcp_u;  //  tcp handle, owned until it wins
  c3_w             hoi_w;  //  host it connects to
  struct _u3_tcon* con_u;  //  connection
  struct _u3_matt* nex_u;  //  next attempt
} u3_matt;

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
  c3_o             sec_o;  // tls
  c3_o             tuf_o;  // hosts are domains, from a %turf
  c3_c**           hos_c;  // hosts, owned
  c3_w             hon_w;  // number of hosts
  c3_w             hoi_w;  // host we connected to
  c3_w             lok_w;  // dns lookups in flight
  c3_o             rez_o;  // some host resolved
  c3_s             por_s;  // port
  u3_madr*         adr_u;  // addresses not yet tried, owned
  u3_madr*         adt_u;  // last address not yet tried
  u3_matt*         att_u;  // attempts in flight, owned
  uv_tcp_t*        tcp_u;  // tcp handle once connected, owned,
                           // unless h2o took it
  h2o_socket_t*    sok_u;  // h2o socket for tls once connected, owned
  uv_timer_t       tim_u;  // connect timer, then drain timer
  uv_timer_t       sta_u;  // stagger timer between attempts
  c3_w             hac_w;  // timers still closing
  struct _u3_mach* mac_u;  // driver, for planning events
  struct _u3_tcon* nex_u;  // next tcon in list
  c3_o             opn_o;  // opened, after any tls handshake
  c3_o             clo_o;  // %close received, draining
  const c3_c*      cat_c;  // peer closed; category for %closed
  const c3_c*      why_c;  // peer closed; cause for %closed
  u3_mque          out_u;  // outgoing bytes
  c3_o             wri_o;  // write in flight
  c3_o             paz_o;  // reading paused, incoming queue full
  u3_mque          inc_u;  // incoming bytes
  u3_ovum*         egg_u;  // %received event in flight
} u3_tcon;

/* u3_mach: list of connections
 */
typedef struct _u3_mach {
  u3_auto  car_u;  //  driver
  u3_tcon* con_u;  //  connection list
  SSL_CTX* tls_u;  //  client tls context
} u3_mach;

/* u3_mres: pending dns lookup
 *
 *   holds the id, not the connection, which may be freed first.
 */
typedef struct _u3_mres {
  uv_getaddrinfo_t adr_u;  //  libuv request
  u3_mach*         mac_u;  //  driver
  c3_d             id_d;   //  connection id
  c3_w             hoi_w;  //  host being looked up
} u3_mres;

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

/* _mach_turf_split(): split a hostname into a turf, tld first.
 *
 *   "example.com" -> ~['com' 'example']. the inverse of
 *   _mach_turf_join(), which rejects empty labels and labels with
 *   dots, so we get back the turf the agent gave us.
 */
static u3_noun
_mach_turf_split(const c3_c* hos_c)
{
  u3_noun     tuf = u3_nul;
  const c3_c* dot_c;

  while ( (dot_c = strchr(hos_c, '.')) ) {
    tuf   = u3nc(u3i_bytes(dot_c - hos_c, (const c3_y*)hos_c), tuf);
    hos_c = dot_c + 1;
  }

  return u3nc(u3i_string(hos_c), tuf);
}

/* _mach_plan_opened(): tell %mach a connection opened, and, for a
 * %turf, through which domain.
 */
static void
_mach_plan_opened(u3_tcon* con_u)
{
  u3_noun tuf = ( c3y == con_u->tuf_o )
                ? u3nc(u3_nul, _mach_turf_split(con_u->hos_c[con_u->hoi_w]))
                : u3_nul;

  _mach_plan(con_u->mac_u,
             u3nt(u3i_string("opened"), u3i_chub(con_u->id_d), tuf));
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

/* _mach_con_free_cb(): free a connection once both timers have closed.
 */
static void
_mach_con_free_cb(uv_handle_t* han_u)
{
  u3_tcon* con_u = han_u->data;

  if ( --con_u->hac_w ) {
    return;
  }

  for ( c3_w i_w = 0; i_w < con_u->hon_w; i_w++ ) {
    c3_free(con_u->hos_c[i_w]);
  }

  c3_free(con_u->hos_c);
  c3_free(con_u);
}

/* _mach_sock_close(): close the socket, whether connecting,
 * plain, or tls.
 *
 *   h2o may still call back on a closing socket, so its callbacks
 *   check for the cleared data pointer. libuv cancels a closing
 *   handle's requests, so their callbacks check for UV_ECANCELED
 *   and touch nothing else; the connection may be freed by then.
 */
static void
_mach_sock_close(u3_tcon* con_u)
{
  if ( con_u->sok_u ) {
    con_u->sok_u->data = 0;
    h2o_socket_close(con_u->sok_u);
    con_u->sok_u = 0;
  }

  if ( con_u->tcp_u ) {
    uv_close((uv_handle_t*)con_u->tcp_u, _mach_tcp_free_cb);
    con_u->tcp_u = 0;
  }
}

/* _mach_attempts_stop(): stop connecting: close every attempt in
 * flight, and drop the addresses not yet tried.
 */
static void
_mach_attempts_stop(u3_tcon* con_u)
{
  uv_timer_stop(&con_u->sta_u);

  while ( con_u->att_u ) {
    u3_matt* att_u = con_u->att_u;

    con_u->att_u = att_u->nex_u;
    uv_close((uv_handle_t*)att_u->tcp_u, _mach_tcp_free_cb);
  }

  while ( con_u->adr_u ) {
    u3_madr* adr_u = con_u->adr_u;

    con_u->adr_u = adr_u->nex_u;
    c3_free(adr_u);
  }

  con_u->adt_u = 0;
}

/* _mach_live(): is the connection open, with a socket to use?
 */
static c3_o
_mach_live(u3_tcon* con_u)
{
  return __( (c3y == con_u->opn_o) && (con_u->sok_u || con_u->tcp_u) );
}

/* _mach_con_free(): unlink a connection, close its handles, then free it.
 *
 *   libuv owns a handle until its close callback runs, so the
 *   struct holding the timers is freed in _mach_con_free_cb().
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

  _mach_attempts_stop(con_u);
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
  con_u->hac_w = 2;
  uv_close((uv_handle_t*)&con_u->tim_u, _mach_con_free_cb);
  uv_close((uv_handle_t*)&con_u->sta_u, _mach_con_free_cb);
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
  _mach_sock_close(con_u);
  _mach_que_free(&con_u->out_u);
  con_u->wri_o = c3n;
  _mach_pump(con_u);
}

static void
_mach_tls_read_cb(h2o_socket_t* sok_u, const c3_c* err_c);

static void
_mach_tcp_alloc_cb(uv_handle_t* han_u, size_t siz_i, uv_buf_t* buf_u);

static void
_mach_tcp_read_cb(uv_stream_t* str_u, ssize_t red_i, const uv_buf_t* buf_u);

/* _mach_read_start(): start reading; c3n if that killed the connection.
 */
static c3_o
_mach_read_start(u3_tcon* con_u)
{
  c3_i sas_i;

  if ( con_u->sok_u ) {
    h2o_socket_read_start(con_u->sok_u, _mach_tls_read_cb);
    return c3y;
  }

  sas_i = uv_read_start((uv_stream_t*)con_u->tcp_u,
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
  if ( con_u->sok_u ) {
    h2o_socket_read_stop(con_u->sok_u);
  }
  else if ( con_u->tcp_u ) {
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
 * and resume reading once the queue drains to half; once a dead
 * connection has delivered everything, close it.
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
       && (con_u->inc_u.len_w <= MACH_QUEUE_LOW)
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

/* _mach_tls_read_cb(): bytes arrived on a tls socket, or the peer closed.
 */
static void
_mach_tls_read_cb(h2o_socket_t* sok_u, const c3_c* err_c)
{
  u3_tcon* con_u = sok_u->data;

  if ( !con_u ) {
    return;
  }

  if ( err_c ) {
    //  h2o reports eof and errors alike as "I/O error",
    //  so we can't tell a reset from eof
    //
    u3l_log("mach: %" PRIu64 ": %s", con_u->id_d, err_c);

    if ( err_c == h2o_socket_error_ssl_decode ) {
      _mach_con_dead(con_u, "tls", "decode");
    }
    else {
      _mach_con_dead(con_u, "peer", ( (err_c == h2o_socket_error_closed)
                                   || (err_c == h2o_socket_error_io) )
                                   ? "done"
                                   : "reset");
    }
    return;
  }

  c3_w len_w = sok_u->input->size;

  if ( !len_w ) {
    return;
  }

  c3_y* buf_y = c3_malloc(len_w);
  memcpy(buf_y, sok_u->input->bytes, len_w);
  h2o_buffer_consume(&sok_u->input, len_w);

  _mach_que_push(&con_u->inc_u, buf_y, len_w);

  //  the queue may pass the cap by one read, at most
  //
  if ( con_u->inc_u.len_w >= MACH_QUEUE_MAX ) {
    h2o_socket_read_stop(sok_u);
    con_u->paz_o = c3y;
  }

  _mach_pump(con_u);
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
  if ( con_u->inc_u.len_w >= MACH_QUEUE_MAX ) {
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

/* _mach_flushed(): a closing connection has sent everything; end it.
 *
 *   a plain socket sends its fin first. h2o sends a tls close_notify
 *   when it closes the socket.
 */
static void
_mach_flushed(u3_tcon* con_u)
{
  if ( con_u->tcp_u ) {
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
  }

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

  _mach_que_pop(&con_u->out_u);

  if ( (c3y == con_u->clo_o) && !con_u->out_u.hed_u ) {
    _mach_flushed(con_u);
    return;
  }

  _mach_write_next(con_u);
}

/* _mach_tls_write_cb(): a tls write finished.
 */
static void
_mach_tls_write_cb(h2o_socket_t* sok_u, const c3_c* err_c)
{
  u3_tcon* con_u = sok_u->data;

  if ( !con_u ) {
    return;
  }

  if ( err_c ) {
    u3l_log("mach: %" PRIu64 ": write: %s", con_u->id_d, err_c);
  }

  _mach_write_done(con_u, err_c ? "reset" : 0);
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

/* _mach_write_next(): start writing the next queued buffer.
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

  u3_mbuf* buf_u = con_u->out_u.hed_u;

  con_u->wri_o = c3y;

  if ( con_u->sok_u ) {
    h2o_iovec_t vec_u = h2o_iovec_init(buf_u->buf_y, buf_u->len_w);
    h2o_socket_write(con_u->sok_u, &vec_u, 1, _mach_tls_write_cb);
  }
  else {
    uv_buf_t    buf_v = uv_buf_init((c3_c*)buf_u->buf_y, buf_u->len_w);
    uv_write_t* req_u = c3_malloc(sizeof(*req_u));
    c3_i        sas_i;

    req_u->data = con_u;

    if ( 0 != (sas_i = uv_write(req_u, (uv_stream_t*)con_u->tcp_u,
                                &buf_v, 1, _mach_tcp_write_cb)) )
    {
      u3l_log("mach: %" PRIu64 ": write: %s", con_u->id_d, uv_strerror(sas_i));
      c3_free(req_u);
      _mach_write_done(con_u, "reset");
    }
  }
}

/* _mach_open(): the connection is ready; start reading and writing.
 */
static void
_mach_open(u3_tcon* con_u)
{
  uv_timer_stop(&con_u->tim_u);
  con_u->opn_o = c3y;

  _mach_plan_opened(con_u);

  if ( c3y == _mach_read_start(con_u) ) {
    _mach_write_next(con_u);
  }
}

/* _mach_handshake_cb(): tls handshake finished.
 */
static void
_mach_handshake_cb(h2o_socket_t* sok_u, const c3_c* err_c)
{
  u3_tcon* con_u = sok_u->data;

  if ( !con_u ) {
    return;
  }

  //  h2o gives a failed verification as openssl's reason for it, or
  //  as one of its own certificate errors; anything else means the
  //  handshake itself broke
  //
  if ( err_c ) {
    c3_o hak_o = __( (err_c == h2o_socket_error_io)
                  || (err_c == h2o_socket_error_closed)
                  || !strcmp(err_c, "ssl handshake failure") );

    u3l_log("mach: tls %" PRIu64 ": %s", con_u->id_d, err_c);
    _mach_con_shut(con_u, "tls", (c3y == hak_o) ? "handshake" : "certificate");
    return;
  }

  _mach_open(con_u);
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

static void
_mach_attempt_cb(uv_connect_t* req_u, c3_i sas_i);

/* _mach_attempts_over(): if no attempt is in flight, no address is
 * left to try, and no lookup is left to wait for, give up. c3y if
 * that freed the connection.
 */
static c3_o
_mach_attempts_over(u3_tcon* con_u)
{
  if ( con_u->att_u || con_u->adr_u || con_u->lok_w ) {
    return c3n;
  }

  _mach_con_shut(con_u, "connect",
                 (c3y == con_u->rez_o) ? "refused" : "unresolved");
  return c3y;
}

static void
_mach_stagger_cb(uv_timer_t* tim_u);

/* _mach_attempt_next(): start an attempt on the next address, and
 * time the one after it; give up if nothing is left.
 */
static void
_mach_attempt_next(u3_tcon* con_u)
{
  uv_timer_stop(&con_u->sta_u);

  while ( con_u->adr_u ) {
    u3_madr*  adr_u = con_u->adr_u;
    uv_tcp_t* tcp_u = c3_malloc(sizeof(*tcp_u));
    u3_matt*  att_u;
    c3_i      sas_i;

    if ( !(con_u->adr_u = adr_u->nex_u) ) {
      con_u->adt_u = 0;
    }

    if ( 0 != (sas_i = uv_tcp_init(u3L, tcp_u)) ) {
      u3l_log("mach: connect %" PRIu64 ": %s", con_u->id_d, uv_strerror(sas_i));
      c3_free(tcp_u);
      c3_free(adr_u);
      continue;
    }

    att_u = c3_malloc(sizeof(*att_u));
    att_u->req_u.data = att_u;
    att_u->tcp_u = tcp_u;
    att_u->hoi_w = adr_u->hoi_w;
    att_u->con_u = con_u;

    sas_i = uv_tcp_connect(&att_u->req_u, tcp_u,
                           (const struct sockaddr*)&adr_u->sas_u,
                           _mach_attempt_cb);
    c3_free(adr_u);

    if ( sas_i ) {
      u3l_log("mach: connect %" PRIu64 ": %s", con_u->id_d, uv_strerror(sas_i));
      c3_free(att_u);
      uv_close((uv_handle_t*)tcp_u, _mach_tcp_free_cb);
      continue;
    }

    att_u->nex_u = con_u->att_u;
    con_u->att_u = att_u;

    uv_timer_start(&con_u->sta_u, _mach_stagger_cb, MACH_STAGGER_MS, 0);
    return;
  }

  _mach_attempts_over(con_u);
}

/* _mach_stagger_cb(): the last attempt has had its head start.
 */
static void
_mach_stagger_cb(uv_timer_t* tim_u)
{
  _mach_attempt_next(tim_u->data);
}

/* _mach_attempt_cb(): a tcp connect finished. the first to succeed
 * wins, and we close the rest; a failure starts the next at once.
 */
static void
_mach_attempt_cb(uv_connect_t* req_u, c3_i sas_i)
{
  u3_matt*  att_u = req_u->data;
  u3_tcon*  con_u = att_u->con_u;
  uv_tcp_t* tcp_u = att_u->tcp_u;
  c3_w      hoi_w = att_u->hoi_w;
  u3_matt** pre_u;

  //  we closed this attempt's handle; con_u may be gone
  //
  if ( UV_ECANCELED == sas_i ) {
    c3_free(att_u);
    return;
  }

  pre_u = &con_u->att_u;
  while ( *pre_u != att_u ) {
    pre_u = &(*pre_u)->nex_u;
  }
  *pre_u = att_u->nex_u;
  c3_free(att_u);

  //  a socket without our keepalive timing would behave differently
  //  on each platform, so we count it as a failed attempt
  //
  if ( (0 != sas_i) || (c3n == _mach_keepalive(con_u, tcp_u)) ) {
    if ( sas_i ) {
      u3l_log("mach: connect %" PRIu64 ": %s", con_u->id_d, uv_strerror(sas_i));
    }
    uv_close((uv_handle_t*)tcp_u, _mach_tcp_free_cb);
    _mach_attempt_next(con_u);
    return;
  }

  _mach_attempts_stop(con_u);
  con_u->tcp_u = tcp_u;
  con_u->hoi_w = hoi_w;

  if ( 1 < con_u->hon_w ) {
    u3l_log("mach: connect %" PRIu64 ": via %s",
            con_u->id_d, con_u->hos_c[hoi_w]);
  }

  uv_tcp_nodelay(con_u->tcp_u, 1);

  //  plain connections stay on libuv, which tells eof from reset
  //
  if ( c3n == con_u->sec_o ) {
    con_u->tcp_u->data = con_u;
    _mach_open(con_u);
    return;
  }

  //  h2o takes the handle for tls, and frees it through our
  //  callback on close
  //
  con_u->sok_u = h2o_uv_socket_create((uv_stream_t*)con_u->tcp_u,
                                      _mach_tcp_free_cb);
  con_u->sok_u->data = con_u;
  con_u->tcp_u = 0;

  //  the connect timer keeps running through the handshake
  //
  h2o_socket_ssl_handshake(con_u->sok_u, con_u->mac_u->tls_u,
                           con_u->hos_c[con_u->hoi_w], _mach_handshake_cb);
}

/* _mach_queue(): queue a host's addresses, alternating ipv6 and ipv4,
 * ipv6 first, in resolver order within each family.
 *
 *   getaddrinfo() orders by each os's own rfc 6724 policy, so we
 *   order the families ourselves to act the same everywhere.
 */
static void
_mach_queue(u3_tcon* con_u, c3_w hoi_w, struct addrinfo* aif_u)
{
  struct addrinfo* six_u = aif_u;
  struct addrinfo* fou_u = aif_u;
  c3_o             six_o = c3y;

  for (;;) {
    while ( six_u && (AF_INET6 != six_u->ai_family) ) {
      six_u = six_u->ai_next;
    }
    while ( fou_u && (AF_INET != fou_u->ai_family) ) {
      fou_u = fou_u->ai_next;
    }

    if ( !six_u && !fou_u ) {
      return;
    }

    struct addrinfo** nex_u = ( (c3y == six_o) && six_u ) ? &six_u
                            : fou_u                       ? &fou_u
                            : &six_u;
    struct addrinfo*  cur_u = *nex_u;

    *nex_u = cur_u->ai_next;
    six_o  = __(nex_u != &six_u);

    if ( cur_u->ai_addrlen > sizeof(struct sockaddr_storage) ) {
      continue;
    }

    u3_madr* adr_u = c3_calloc(sizeof(*adr_u));

    memcpy(&adr_u->sas_u, cur_u->ai_addr, cur_u->ai_addrlen);
    adr_u->hoi_w = hoi_w;

    if ( con_u->adt_u ) {
      con_u->adt_u->nex_u = adr_u;
    }
    else {
      con_u->adr_u = adr_u;
    }
    con_u->adt_u = adr_u;
  }
}

/* _mach_resolve_cb(): a lookup finished; queue its addresses, in
 * the order lookups arrive, and start connecting if we're idle.
 */
static void
_mach_resolve_cb(uv_getaddrinfo_t* adr_u,
                 c3_i              sas_i,
                 struct addrinfo*  aif_u)
{
  u3_mres* res_u = adr_u->data;
  u3_tcon* con_u = _mach_con_find(res_u->mac_u, res_u->id_d);
  c3_w     hoi_w = res_u->hoi_w;

  c3_free(res_u);

  //  gone, or already connected
  //
  if ( !con_u || con_u->tcp_u || con_u->sok_u ) {
    if ( con_u ) {
      con_u->lok_w--;
    }
    uv_freeaddrinfo(aif_u);
    return;
  }

  con_u->lok_w--;

  if ( 0 != sas_i ) {
    u3l_log("mach: resolve %" PRIu64 " %s: %s",
            con_u->id_d, con_u->hos_c[hoi_w], uv_strerror(sas_i));
    _mach_attempts_over(con_u);
    return;
  }

  con_u->rez_o = c3y;
  _mach_queue(con_u, hoi_w, aif_u);
  uv_freeaddrinfo(aif_u);

  //  the stagger timer runs only while an attempt is young; if it
  //  isn't running, the next attempt is due now
  //
  if ( !uv_is_active((uv_handle_t*)&con_u->sta_u) ) {
    _mach_attempt_next(con_u);
  }
}

/* _mach_resolve(): start looking up a host.
 */
static void
_mach_resolve(u3_tcon* con_u, c3_w hoi_w)
{
  c3_c*           hos_c = con_u->hos_c[hoi_w];
  c3_c            por_c[6];  //  "65535" plus the terminating null
  struct addrinfo hin_u;
  u3_mres*        res_u;
  c3_i            sas_i;

  snprintf(por_c, sizeof(por_c), "%u", con_u->por_s);

  memset(&hin_u, 0, sizeof(hin_u));
  hin_u.ai_family   = AF_UNSPEC;
  hin_u.ai_socktype = SOCK_STREAM;
  hin_u.ai_protocol = IPPROTO_TCP;

  res_u = c3_malloc(sizeof(*res_u));
  res_u->adr_u.data = res_u;
  res_u->mac_u = con_u->mac_u;
  res_u->id_d  = con_u->id_d;
  res_u->hoi_w = hoi_w;

  //  libuv copies hos_c and por_c before returning
  //
  if ( 0 != (sas_i = uv_getaddrinfo(u3L, &res_u->adr_u, _mach_resolve_cb,
                                    hos_c, por_c, &hin_u)) )
  {
    u3l_log("mach: resolve %" PRIu64 " %s: %s",
            con_u->id_d, hos_c, uv_strerror(sas_i));
    c3_free(res_u);
    return;
  }

  con_u->lok_w++;
}

/* _mach_turf_join(): join a reversed list of knots into a hostname.
 *
 *   ~['com' 'example'] -> "example.com"
 */
static c3_c*
_mach_turf_join(u3_noun tuf)
{
  u3_noun i, t = tuf;
  c3_w    len_w = 0;

  //  each label must be a non-empty atom without dots, so
  //  _mach_turf_split() can give the turf back
  //
  while ( c3y == u3r_cell(t, &i, &t) ) {
    c3_w met_w;

    if (  (c3n == u3a_is_atom(i))
       || (0 == (met_w = u3r_met(3, i)))
       || (met_w > 253) )
    {
      return 0;
    }

    for ( c3_w j_w = 0; j_w < met_w; j_w++ ) {
      if ( '.' == u3r_byte(j_w, i) ) {
        return 0;
      }
    }

    len_w += met_w + 1;
  }

  //  must be a null-terminated, non-empty list, within dns limits
  //
  if ( (u3_nul != t) || (0 == len_w) || (len_w > 254) ) {
    return 0;
  }

  c3_c* hos_c = c3_malloc(len_w);
  c3_w  pos_w = len_w - 1;

  hos_c[pos_w] = 0;
  t = tuf;

  while ( c3y == u3r_cell(t, &i, &t) ) {
    c3_w met_w = u3r_met(3, i);

    pos_w -= met_w;
    u3r_bytes(0, met_w, (c3_y*)hos_c + pos_w, i);

    if ( pos_w ) {
      hos_c[--pos_w] = '.';
    }
  }

  return hos_c;
}

/* _mach_ip(): copy an %if or %is address out as a string.
 */
static c3_c*
_mach_ip(u3_noun hed, u3_noun p)
{
  // IPv4
  if ( c3__if == hed ) {
    c3_w ip_w;
    c3_c buf_c[INET_ADDRSTRLEN];

    if ( c3n == u3r_safe_word(p, &ip_w) ) {
      return 0;
    }

    struct in_addr add_u = { .s_addr = htonl(ip_w) };
    if ( uv_inet_ntop(AF_INET, &add_u, buf_c, sizeof(buf_c)) ) {
      return 0;
    }

    return strdup(buf_c);
  }
  // IPv6
  else if ( c3__is == hed ) {
    c3_y ip_y[16];
    c3_y net_y[16];
    c3_c buf_c[INET6_ADDRSTRLEN];

    if ( c3n == u3a_is_atom(p) || (u3r_met(3, p) > 16) ) {
      return 0;
    }

    //  atoms are little-endian; in6_addr is big-endian
    u3r_bytes(0, 16, ip_y, p);
    for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
      net_y[i_w] = ip_y[15 - i_w];
    }

    if ( uv_inet_ntop(AF_INET6, net_y, buf_c, sizeof(buf_c)) ) {
      return 0;
    }

    return strdup(buf_c);
  }

  return 0;
}

/* _mach_hosts(): copy a fief's address out as the hosts to try, in
 * order: every domain of a %turf, or the one ip address. returns
 * how many, or 0 if the fief is malformed.
 */
static c3_w
_mach_hosts(u3_noun hed, u3_noun p, c3_c*** hos_c)
{
  if ( c3__turf != hed ) {
    c3_c* ip_c = _mach_ip(hed, p);

    if ( !ip_c ) {
      return 0;
    }

    *hos_c = c3_malloc(sizeof(c3_c*));
    (*hos_c)[0] = ip_c;
    return 1;
  }

  u3_noun i, t = p;
  c3_w    hon_w = 0;

  while ( c3y == u3r_cell(t, &i, &t) ) {
    hon_w++;
  }

  if ( (u3_nul != t) || (0 == hon_w) ) {
    return 0;
  }

  *hos_c = c3_malloc(hon_w * sizeof(c3_c*));
  t = p;

  for ( c3_w i_w = 0; i_w < hon_w; i_w++ ) {
    u3r_cell(t, &i, &t);

    if ( 0 == ((*hos_c)[i_w] = _mach_turf_join(i)) ) {
      while ( i_w-- ) {
        c3_free((*hos_c)[i_w]);
      }
      c3_free(*hos_c);
      return 0;
    }
  }

  return hon_w;
}

/* _mach_connect(): copy out a fief, start the timer, and resolve
 * the first host.
 */
static void
_mach_connect(u3_mach* mac_u, c3_d id_d, c3_o sec_o, u3_noun fef)
{
  u3_noun hed, val, p, q;
  c3_w    por_w, hon_w;
  c3_c**  hos_c;

  if ( _mach_con_find(mac_u, id_d) ) {
    u3l_log("mach: connect: id %" PRIu64 " already open", id_d);
    return;
  }

  if (  (c3n == u3r_cell(fef, &hed, &val))
     || (c3n == u3r_cell(val, &p, &q))
     || (c3n == u3r_safe_word(q, &por_w)) || (por_w > 65535)
     || (0 == (hon_w = _mach_hosts(hed, p, &hos_c))) )
  {
    u3l_log("mach: connect %" PRIu64 ": bad fief", id_d);
    _mach_plan_closed(mac_u, id_d, "connect", "invalid");
    return;
  }

  u3_tcon* con_u = c3_calloc(sizeof(*con_u));
  con_u->id_d  = id_d;
  con_u->sec_o = sec_o;
  con_u->tuf_o = __(c3__turf == hed);
  con_u->hos_c = hos_c;
  con_u->hon_w = hon_w;
  con_u->rez_o = c3n;
  con_u->por_s = (c3_s)por_w;
  con_u->mac_u = mac_u;
  con_u->opn_o = c3n;
  con_u->clo_o = c3n;
  con_u->wri_o = c3n;
  con_u->paz_o = c3n;
  con_u->nex_u = mac_u->con_u;
  mac_u->con_u = con_u;

  uv_timer_init(u3L, &con_u->tim_u);
  uv_timer_init(u3L, &con_u->sta_u);
  con_u->tim_u.data = con_u;
  con_u->sta_u.data = con_u;

  //  certificates name hosts, so tls needs a domain
  //
  if ( (c3y == sec_o) && (c3__turf != hed) ) {
    u3l_log("mach: connect %" PRIu64 ": tls needs a domain", id_d);
    _mach_con_shut(con_u, "tls", "invalid");
    return;
  }

  u3l_log("mach: connect %" PRIu64 " %s:%u%s, %u host%s",
          id_d, hos_c[0], por_w, (c3y == sec_o) ? " (tls)" : "",
          hon_w, (1 == hon_w) ? "" : "s");

  //  look up every host at once; one timer covers the lot
  //
  uv_timer_start(&con_u->tim_u, _mach_timeout_cb, MACH_CONNECT_MS, 0);

  for ( c3_w i_w = 0; i_w < hon_w; i_w++ ) {
    _mach_resolve(con_u, i_w);
  }

  _mach_attempts_over(con_u);
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

  if ( (con_u->out_u.len_w + (c3_d)len_w) > MACH_QUEUE_MAX ) {
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
  //  [%connect id=@ud secure=? =fief]
  //
  else if ( c3y == u3r_sing_c("connect", tag) ) {
    u3_noun id, sec, fef;
    c3_d    id_d;

    if (  (c3n == u3r_trel(tmp, &id, &sec, &fef))
       || (c3n == u3r_safe_chub(id, &id_d))
       || (c3n == u3a_is_cat(sec)) || (sec > 1) )
    {
      ret_o = c3n;
    } else {
      ret_o = c3y;
      _mach_connect(mac_u, id_d, (c3_o)sec, fef);
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

  SSL_CTX_free(mac_u->tls_u);
  c3_free(mac_u);
}

/* _mach_tls_verify_cb(): check the peer certificate names the host.
 *
 *   h2o checks the name itself only when the handshake ends with
 *   nothing left to send. under tls 1.3 the client sends the last
 *   message, so h2o skips the check; we make openssl do it instead.
 *   h2o sets the host as the sni name before the handshake starts.
 */
static c3_i
_mach_tls_verify_cb(c3_i ok_i, X509_STORE_CTX* sto_u)
{
  SSL*        ssl_u;
  const c3_c* hos_c;

  //  only the peer's own certificate, at depth 0, names the host
  //
  if ( !ok_i || (0 != X509_STORE_CTX_get_error_depth(sto_u)) ) {
    return ok_i;
  }

  ssl_u = X509_STORE_CTX_get_ex_data(sto_u,
                                     SSL_get_ex_data_X509_STORE_CTX_idx());
  hos_c = ssl_u ? SSL_get_servername(ssl_u, TLSEXT_NAMETYPE_host_name) : 0;

  if (  !hos_c
     || (1 != X509_check_host(X509_STORE_CTX_get_current_cert(sto_u),
                              hos_c, 0, 0, 0)) )
  {
    X509_STORE_CTX_set_error(sto_u, X509_V_ERR_HOSTNAME_MISMATCH);
    return 0;
  }

  return 1;
}

/* _mach_init_tls(): make a client tls context that checks certificates.
 */
static SSL_CTX*
_mach_init_tls(void)
{
  SSL_CTX* tls_u = SSL_CTX_new(TLS_client_method());

  SSL_CTX_set_min_proto_version(tls_u, TLS1_2_VERSION);
  SSL_CTX_set_options(tls_u, SSL_OP_NO_COMPRESSION);
  u3K.ssl_x509_f(SSL_CTX_get_cert_store(tls_u));
  SSL_CTX_set_verify(tls_u, SSL_VERIFY_PEER, _mach_tls_verify_cb);
  SSL_CTX_set_session_cache_mode(tls_u, SSL_SESS_CACHE_OFF);

  return tls_u;
}

/* u3_mach_io_init(): initialize tcp driver.
 */
u3_auto *u3_mach_io_init(u3_pier *pir_u) {
  u3_mach *mac_u = c3_calloc(sizeof(*mac_u));
  mac_u->tls_u = _mach_init_tls();

  u3_auto *car_u = &mac_u->car_u;
  car_u->nam_m = c3__mach;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _mach_io_talk;
  car_u->io.kick_f = _mach_io_kick;
  car_u->io.exit_f = _mach_io_exit;

  return car_u;
}
