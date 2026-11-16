/// @file

#include "vere.h"
#include "noun.h"

#ifndef U3_OS_windows
#include <netinet/tcp.h>
#endif

#ifdef U3_OS_osx
#include <sys/sysctl.h>
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

//  read buffer, and so most bytes in one %received event, where the
//  host's limits can't be read. windows autotunes its receive buffer
//  to about 16 MB
//
#define MACH_READ_DEFAULT  (16 << 20)

//  most outgoing bytes libuv may hold for a connection; a %send past
//  this closes it
//
#define MACH_OUT_MAX     (16 << 20)

//  most bytes in one write. a connection with bytes to send that
//  finishes no write in MACH_RXT_S dies, so this is also the least a
//  peer must take in that time
//
#define MACH_WRITE_MAX   (64 << 10)

struct _u3_mach;

/* u3_tcon: tcp connection
 */
typedef struct _u3_tcon {
  c3_d             id_d;   // id from mach
  uv_tcp_t*        tcp_u;  // tcp handle
  uv_timer_t       tim_u;  // connect, stall, then drain timer
  struct _u3_mach* mac_u;  // driver, for planning events
  struct _u3_tcon* nex_u;  // next tcon in list
  c3_o             opn_o;  // opened?
  c3_o             clo_o;  // %close received?
  c3_o             ded_o;  // freeing; callbacks must do nothing
  u3_ovum*         egg_u;  // planned %received event
  c3_o             wok_o;  // planned event in work?
} u3_tcon;

/* u3_mwri: one write, and the bytes it sends
 */
typedef struct _u3_mwri {
  uv_write_t       req_u;  // request, first so we can cast back
  u3_tcon*         con_u;  // connection
  c3_y             buf_y[];
} u3_mwri;

/* u3_mach: list of connections
 */
typedef struct _u3_mach {
  u3_auto  car_u;  //  driver
  u3_tcon* con_u;  //  connection list
  c3_y*    buf_y;  //  read buffer, siz_w bytes
  c3_w     siz_w;  //  read buffer size, most bytes in one event
  c3_w     rcv_w;  //  SO_RCVBUF to ask for; 0 leaves it to the os
  c3_o     bus_o;  //  read buffer in use?
} u3_mach;

/* _mach_out_size(): bytes libuv has yet to hand the kernel.
 */
static size_t
_mach_out_size(u3_tcon* con_u)
{
  return uv_stream_get_write_queue_size((uv_stream_t*)con_u->tcp_u);
}

/* _mach_event_plan(): inject an event into %mach.
 */
static u3_ovum*
_mach_event_plan(u3_mach* mac_u, u3_noun cad)
{
  u3_noun wir = u3nc(c3__mach, u3_nul);
  return u3_auto_plan(&mac_u->car_u, u3_ovum_init(0, c3__m, wir, cad));
}

/* _mach_conn_find(): find a connection by id.
 */
static u3_tcon*
_mach_conn_find(u3_mach* mac_u, c3_d id_d)
{
  u3_tcon* con_u = mac_u->con_u;

  while ( con_u && (id_d != con_u->id_d) ) {
    con_u = con_u->nex_u;
  }

  return con_u;
}

/* _mach_conn_free_cb(): free a connection once its timer has closed.
 */
static void
_mach_conn_free_cb(uv_handle_t* han_u)
{
  c3_free(han_u->data);
}

/* _mach_tcp_close_cb(): the socket has closed; close the timer.
 */
static void
_mach_tcp_close_cb(uv_handle_t* han_u)
{
  u3_tcon* con_u = han_u->data;

  c3_free(han_u);
  uv_close((uv_handle_t*)&con_u->tim_u, _mach_conn_free_cb);
}

/* _mach_conn_free(): unlink a connection, close its handles, then free it.
 *
 *   libuv owns a handle until its close callback runs. closing the
 *   socket runs the callback of every request on it, some with
 *   UV_ECANCELED and some with the status they finished with, and
 *   only then _mach_tcp_close_cb(). so we close the timer and free
 *   the connection after the socket, and every request callback
 *   checks .ded_o and touches nothing else.
 */
static void
_mach_conn_free(u3_tcon* con_u)
{
  u3_tcon** pre_u = &con_u->mac_u->con_u;

  while ( *pre_u && (con_u != *pre_u) ) {
    pre_u = &(*pre_u)->nex_u;
  }

  if ( *pre_u ) {
    *pre_u = con_u->nex_u;
  }

  con_u->ded_o = c3y;

  //  the event outlives this call; detach it
  if ( con_u->egg_u ) {
    con_u->egg_u->ptr_v = 0;
    con_u->egg_u = 0;
  }

  uv_timer_stop(&con_u->tim_u);

  if ( con_u->tcp_u ) {
    uv_close((uv_handle_t*)con_u->tcp_u, _mach_tcp_close_cb);
    con_u->tcp_u = 0;
  }
  else {
    uv_close((uv_handle_t*)&con_u->tim_u, _mach_conn_free_cb);
  }
}

/* _mach_conn_shut(): tell %mach a connection closed, free it.
 *
 *   a %received still on its way to %mach was planned first, so
 *   %mach hears it before this %closed.
 */
static void
_mach_conn_shut(u3_tcon* con_u, const c3_c* cat_c, const c3_c* why_c)
{
  _mach_event_plan(con_u->mac_u,
             u3nq(u3i_string("closed"), u3i_chub(con_u->id_d),
                  u3i_string(cat_c), u3i_string(why_c)));
  _mach_conn_free(con_u);
}

static void
_mach_tcp_alloc_cb(uv_handle_t* han_u, size_t siz_i, uv_buf_t* buf_u);

static void
_mach_tcp_read_cb(uv_stream_t* str_u, ssize_t red_i, const uv_buf_t* buf_u);

/* _mach_conn_read_start(): start reading; c3n if that killed the connection.
 */
static c3_o
_mach_conn_read_start(u3_tcon* con_u)
{
  c3_i sas_i = uv_read_start((uv_stream_t*)con_u->tcp_u,
                             _mach_tcp_alloc_cb, _mach_tcp_read_cb);

  if ( sas_i ) {
    u3l_log("mach: %" PRIu64 ": read: %s", con_u->id_d, uv_strerror(sas_i));
    _mach_conn_shut(con_u, "peer", "reset");
    return c3n;
  }

  return c3y;
}

/* _mach_event_received_news(): a %received event changed state.
 */
static void
_mach_event_received_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_tcon* con_u = egg_u->ptr_v;

  if ( !con_u ) {
    return;
  }

  if ( u3_ovum_work == new_e ) {
    con_u->wok_o = c3y;
    return;
  }

  con_u->egg_u = 0;

  if ( u3_ovum_done == new_e ) {
    //  %mach has the bytes; read more, unless the agent asked to go
    //
    if ( c3n == con_u->clo_o ) {
      _mach_conn_read_start(con_u);
    }
  }
  else {
    //  dropped: the stream has a gap, so it can't go on
    //
    u3l_log("mach: %" PRIu64 ": %%received dropped", con_u->id_d);
    _mach_conn_shut(con_u, "local", "dropped");
  }
}

/* _mach_event_received_bail(): a %received event crashed.
 */
static void
_mach_event_received_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3_tcon* con_u = egg_u->ptr_v;

  u3_auto_bail_slog(egg_u, lud);
  u3_ovum_free(egg_u);

  //  the stream has a gap, so it can't go on
  //
  if ( con_u ) {
    con_u->egg_u = 0;
    u3l_log("mach: %" PRIu64 ": %%received failed", con_u->id_d);
    _mach_conn_shut(con_u, "local", "dropped");
  }
}

/* _mach_tcp_alloc_cb(): lend the driver's read buffer.
 *
 *   one buffer serves every connection, since _mach_tcp_read_cb()
 *   copies the bytes into an event before the next read. should
 *   libuv ask again before then, we make a spare.
 */
static void
_mach_tcp_alloc_cb(uv_handle_t* han_u, size_t siz_i, uv_buf_t* buf_u)
{
  u3_tcon* con_u = han_u->data;
  u3_mach* mac_u = con_u->mac_u;

  if ( c3n == mac_u->bus_o ) {
    mac_u->bus_o = c3y;
    buf_u->base  = (c3_c*)mac_u->buf_y;
  }
  else {
    buf_u->base = c3_malloc(mac_u->siz_w);
  }

  buf_u->len = mac_u->siz_w;
}

/* _mach_tcp_read_cb(): bytes arrived, or the peer closed.
 *
 *   one %received goes to %mach at a time: we stop reading until
 *   %mach has taken this one, and the kernel buffers what follows.
 */
static void
_mach_tcp_read_cb(uv_stream_t* str_u, ssize_t red_i, const uv_buf_t* buf_u)
{
  u3_tcon* con_u = str_u->data;
  u3_mach* mac_u = con_u->mac_u;
  u3_noun  dat   = ( red_i > 0 ) ? u3i_bytes(red_i, (c3_y*)buf_u->base) : 0;

  if ( buf_u->base == (c3_c*)mac_u->buf_y ) {
    mac_u->bus_o = c3n;
  }
  else {
    c3_free(buf_u->base);
  }

  if ( red_i < 0 ) {
    //  keepalive and retransmission failures show up as a
    //  timed-out read
    const c3_c* why_c = ( UV_EOF == red_i )       ? "done"
                      : ( UV_ETIMEDOUT == red_i ) ? "dead"
                      : "reset";

    _mach_conn_shut(con_u, "peer", why_c);
    return;
  }

  if ( !red_i ) {
    return;
  }

  uv_read_stop(str_u);

  {
    u3_noun cad = u3nq(u3i_string("received"), u3i_chub(con_u->id_d),
                       u3i_word((c3_w)red_i), dat);

    con_u->egg_u = _mach_event_plan(con_u->mac_u, cad);
    con_u->wok_o = c3n;
    u3_auto_peer(con_u->egg_u, con_u,
                 _mach_event_received_news, _mach_event_received_bail);
  }
}

/* _mach_shutdown_cb(): our fin went out after the last write.
 */
static void
_mach_shutdown_cb(uv_shutdown_t* req_u, c3_i sas_i)
{
  u3_tcon* con_u = req_u->data;

  c3_free(req_u);

  if ( c3y == con_u->ded_o ) {
    return;
  }

  _mach_conn_shut(con_u, "close", "done");
}

static void
_mach_conn_drain_cb(uv_timer_t* tim_u);

/* _mach_conn_drain(): stop reading, and close once libuv has sent
 * what the agent queued.
 *
 *   uv_shutdown() waits for every write before it sends our fin.
 */
static void
_mach_conn_drain(u3_tcon* con_u)
{
  uv_shutdown_t* req_u = c3_malloc(sizeof(*req_u));
  c3_i           sas_i;

  uv_read_stop((uv_stream_t*)con_u->tcp_u);
  uv_timer_start(&con_u->tim_u, _mach_conn_drain_cb, MACH_DRAIN_MS, 0);

  req_u->data = con_u;

  if ( 0 != (sas_i = uv_shutdown(req_u, (uv_stream_t*)con_u->tcp_u,
                                 _mach_shutdown_cb)) )
  {
    u3l_log("mach: %" PRIu64 ": shutdown: %s", con_u->id_d, uv_strerror(sas_i));
    c3_free(req_u);
    _mach_conn_shut(con_u, "close", "done");
  }
}

/* _mach_stall_cb(): no write finished in MACH_RXT_S; the peer stopped
 * taking bytes.
 */
static void
_mach_stall_cb(uv_timer_t* tim_u)
{
  u3_tcon* con_u = tim_u->data;

  u3l_log("mach: %" PRIu64 ": stalled, %zu bytes unsent",
          con_u->id_d, _mach_out_size(con_u));
  _mach_conn_shut(con_u, "peer", "dead");
}

/* _mach_stall_time(): give the peer MACH_RXT_S from now to take more
 * bytes, or stop timing if libuv has none left to send.
 *
 *   only for an open connection that is not draining; the timer
 *   counts the connect, then the drain, otherwise.
 */
static void
_mach_stall_time(u3_tcon* con_u)
{
  if ( (c3n == con_u->opn_o) || (c3y == con_u->clo_o) ) {
    return;
  }

  if ( _mach_out_size(con_u) ) {
    uv_timer_start(&con_u->tim_u, _mach_stall_cb, MACH_RXT_S * 1000, 0);
  }
  else {
    uv_timer_stop(&con_u->tim_u);
  }
}

/* _mach_tcp_write_cb(): a write finished; the peer is taking bytes.
 */
static void
_mach_tcp_write_cb(uv_write_t* req_u, c3_i sas_i)
{
  u3_tcon* con_u = ((u3_mwri*)req_u)->con_u;

  c3_free(req_u);

  if ( c3y == con_u->ded_o ) {
    return;
  }

  if ( sas_i ) {
    _mach_conn_shut(con_u, "peer", (UV_ETIMEDOUT == sas_i) ? "dead" : "reset");
    return;
  }

  _mach_stall_time(con_u);
}

/* _mach_conn_open(): the connection is ready; start reading, and time
 * the bytes queued while we connected.
 */
static void
_mach_conn_open(u3_tcon* con_u)
{
  uv_timer_stop(&con_u->tim_u);
  con_u->opn_o = c3y;

  //  %close came while we were connecting: tell %mach nothing, read
  //  nothing, and flush what the agent queued
  if ( c3y == con_u->clo_o ) {
    _mach_conn_drain(con_u);
    return;
  }

  _mach_event_plan(con_u->mac_u,
             u3nc(u3i_string("opened"), u3i_chub(con_u->id_d)));

  if ( c3y == _mach_conn_read_start(con_u) ) {
    _mach_stall_time(con_u);
  }
}

/* _mach_conn_timeout_cb(): give up on a connection that took too long.
 */
static void
_mach_conn_timeout_cb(uv_timer_t* tim_u)
{
  u3_tcon* con_u = tim_u->data;

  _mach_conn_shut(con_u, "connect", "timeout");
}

/* _mach_conn_drain_cb(): give up flushing a closing connection.
 */
static void
_mach_conn_drain_cb(uv_timer_t* tim_u)
{
  u3_tcon* con_u = tim_u->data;

  u3l_log("mach: close %" PRIu64 ": drain timeout, %zu bytes unsent",
          con_u->id_d, _mach_out_size(con_u));
  _mach_conn_shut(con_u, "close", "timeout");
}

/* _mach_conn_keepalive(): set keepalive and retransmission timing, the
 * same on every platform.
 *
 *   uv_tcp_keepalive() sets only the idle time, and libuv picks the
 *   probe interval and count, so we set those ourselves. keepalive
 *   covers only idle connections; while sent bytes wait for an ack,
 *   each os gives up retransmitting after its own time, so we fix
 *   that too: TCP_USER_TIMEOUT on linux (ms), TCP_RXT_CONNDROPTIME on
 *   macos (s), TCP_MAXRT on windows (s).
 */
static c3_o
_mach_conn_keepalive(u3_tcon* con_u)
{
  uv_tcp_t*  tcp_u = con_u->tcp_u;
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

/* _mach_tcp_connect_cb(): the tcp connect finished.
 */
static void
_mach_tcp_connect_cb(uv_connect_t* req_u, c3_i sas_i)
{
  u3_tcon* con_u = req_u->data;

  c3_free(req_u);

  if ( c3y == con_u->ded_o ) {
    return;
  }

  if ( sas_i ) {
    _mach_conn_shut(con_u, "connect",
                   (UV_ETIMEDOUT == sas_i) ? "timeout" : "refused");
    return;
  }

  //  a socket without our keepalive timing would behave differently
  //  on each platform, so we count it as a failed connect
  if ( c3n == _mach_conn_keepalive(con_u) ) {
    _mach_conn_shut(con_u, "connect", "refused");
    return;
  }

  uv_tcp_nodelay(con_u->tcp_u, 1);
  _mach_conn_open(con_u);
}

/* _mach_addr_read(): read a mach addr into a socket address. c3n if
 * the addr is malformed.
 */
static c3_o
_mach_addr_read(u3_noun adr, struct sockaddr_storage* sas_u)
{
  u3_noun hed, p, q;
  c3_w    por_w;

  if (  (c3n == u3r_trel(adr, &hed, &p, &q))
     || (c3n == u3r_safe_word(q, &por_w)) || (por_w > 65535) )
  {
    return c3n;
  }

  memset(sas_u, 0, sizeof(*sas_u));

  //  ipv4
  if ( c3__if == hed ) {
    struct sockaddr_in* sin_u = (struct sockaddr_in*)sas_u;
    c3_w                ip_w;

    if ( c3n == u3r_safe_word(p, &ip_w) ) {
      return c3n;
    }

    sin_u->sin_family      = AF_INET;
    sin_u->sin_port        = htons((c3_s)por_w);
    sin_u->sin_addr.s_addr = htonl(ip_w);
#ifdef SIN6_LEN
    sin_u->sin_len         = sizeof(*sin_u);
#endif

    return c3y;
  }
  //  ipv6
  else if ( c3__is == hed ) {
    struct sockaddr_in6* sin_u = (struct sockaddr_in6*)sas_u;
    c3_y                 ip_y[16];

    if ( (c3n == u3a_is_atom(p)) || (u3r_met(3, p) > 16) ) {
      return c3n;
    }

    sin_u->sin6_family = AF_INET6;
    sin_u->sin6_port   = htons((c3_s)por_w);
#ifdef SIN6_LEN
    sin_u->sin6_len    = sizeof(*sin_u);
#endif

    //  atoms are little-endian; in6_addr is big-endian
    u3r_bytes(0, 16, ip_y, p);
    for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
      sin_u->sin6_addr.s6_addr[i_w] = ip_y[15 - i_w];
    }

    return c3y;
  }

  return c3n;
}

/* _mach_conn_connect(): start connecting to an address,
 * time the connection attempt.
 */
static void
_mach_conn_connect(u3_mach* mac_u, c3_d id_d, u3_noun adr)
{
  struct sockaddr_storage sas_u;
  uv_connect_t*           req_u;
  u3_tcon*                con_u;
  c3_i                    sas_i;

  if ( _mach_conn_find(mac_u, id_d) ) {
    //  %mach never reuses an id; a %closed here would close the
    //  live connection in %mach, so we only log
    u3l_log("mach: connect: id %" PRIu64 " already open", id_d);
    return;
  }

  if ( c3n == _mach_addr_read(adr, &sas_u) ) {
    _mach_event_plan(mac_u, u3nq(u3i_string("closed"), u3i_chub(id_d),
                           u3i_string("connect"), u3i_string("invalid")));
    return;
  }

  con_u = c3_calloc(sizeof(*con_u));
  con_u->id_d  = id_d;
  con_u->mac_u = mac_u;
  con_u->opn_o = c3n;
  con_u->clo_o = c3n;
  con_u->ded_o = c3n;
  con_u->wok_o = c3n;
  con_u->nex_u = mac_u->con_u;
  mac_u->con_u = con_u;

  uv_timer_init(u3L, &con_u->tim_u);
  con_u->tim_u.data = con_u;

  con_u->tcp_u = c3_malloc(sizeof(*con_u->tcp_u));

  //  make the socket now, so we can size its buffer before connecting
  //
  if ( 0 != (sas_i = uv_tcp_init_ex(u3L, con_u->tcp_u, sas_u.ss_family)) ) {
    u3l_log("mach: connect %" PRIu64 ": %s", id_d, uv_strerror(sas_i));
    c3_free(con_u->tcp_u);
    con_u->tcp_u = 0;
    _mach_conn_shut(con_u, "connect", "refused");
    return;
  }

  con_u->tcp_u->data = con_u;

  //  see _mach_rcv_limits(); ask before tcp picks its window scale
  //
  if ( mac_u->rcv_w ) {
    c3_i rcv_i = mac_u->rcv_w;
    uv_recv_buffer_size((uv_handle_t*)con_u->tcp_u, &rcv_i);
  }

  req_u = c3_malloc(sizeof(*req_u));
  req_u->data = con_u;

  if ( 0 != (sas_i = uv_tcp_connect(req_u, con_u->tcp_u,
                                    (const struct sockaddr*)&sas_u,
                                    _mach_tcp_connect_cb)) )
  {
    u3l_log("mach: connect %" PRIu64 ": %s", id_d, uv_strerror(sas_i));
    c3_free(req_u);
    _mach_conn_shut(con_u, "connect", "refused");
    return;
  }

  uv_timer_start(&con_u->tim_u, _mach_conn_timeout_cb, MACH_CONNECT_MS, 0);
}

/* _mach_conn_send_bytes(): queue bytes to send on a connection.
 */
static void
_mach_conn_send_bytes(u3_mach* mac_u, c3_d id_d, c3_w len_w, u3_atom dat)
{
  u3_tcon* con_u = _mach_conn_find(mac_u, id_d);

  if ( !con_u ) {
    u3l_log("mach: send %" PRIu64 ": no connection", id_d);
    return;
  }

  if ( c3y == con_u->clo_o ) {
    u3l_log("mach: send %" PRIu64 ": closing, dropped %u bytes", id_d, len_w);
    return;
  }

  if ( !len_w ) {
    return;
  }

  if ( (_mach_out_size(con_u) + len_w) > MACH_OUT_MAX ) {
    u3l_log("mach: send %" PRIu64 ": outgoing queue full", id_d);
    _mach_conn_shut(con_u, "local", "overflowed");
    return;
  }

  //  XX libuv on windows marks a socket writable only once it has
  //  connected, so a %send before %connected may fail there with
  //  UV_EPIPE and close the connection with [%peer %reset]. it also
  //  hands every write to the kernel at once, so the write queue size
  //  shrinks only as whole slices finish. untested on windows
  //
  //  one write per slice, so each that finishes shows the peer is
  //  still taking bytes
  //
  for ( c3_w pos_w = 0; pos_w < len_w; pos_w += MACH_WRITE_MAX ) {
    c3_w     siz_w = c3_min(len_w - pos_w, MACH_WRITE_MAX);
    u3_mwri* wri_u = c3_malloc(sizeof(*wri_u) + siz_w);
    uv_buf_t buf_u = uv_buf_init((c3_c*)wri_u->buf_y, siz_w);
    c3_i     sas_i;

    wri_u->con_u = con_u;
    u3r_bytes(pos_w, siz_w, wri_u->buf_y, dat);

    if ( 0 != (sas_i = uv_write(&wri_u->req_u, (uv_stream_t*)con_u->tcp_u,
                                &buf_u, 1, _mach_tcp_write_cb)) )
    {
      u3l_log("mach: %" PRIu64 ": write: %s", id_d, uv_strerror(sas_i));
      c3_free(wri_u);
      _mach_conn_shut(con_u, "peer", "reset");
      return;
    }
  }

  //  start timing, unless writes already in flight are being timed
  //
  if ( !uv_is_active((uv_handle_t*)&con_u->tim_u) ) {
    _mach_stall_time(con_u);
  }
}

/* _mach_conn_close(): close a connection, flushing what we owe the peer.
 */
static void
_mach_conn_close(u3_mach* mac_u, c3_d id_d)
{
  u3_tcon* con_u = _mach_conn_find(mac_u, id_d);

  if ( !con_u ) {
    u3l_log("mach: close %" PRIu64 ": no connection", id_d);
    return;
  }

  if ( c3y == con_u->clo_o ) {
    return;
  }

  //  nothing to flush
  //
  if ( !_mach_out_size(con_u) ) {
    _mach_conn_shut(con_u, "close", "done");
    return;
  }

  con_u->clo_o = c3y;

  //  still connecting: the connect timer keeps running, and
  //  _mach_conn_open() starts the flush
  //
  if ( c3n == con_u->opn_o ) {
    return;
  }

  _mach_conn_drain(con_u);
}

/* _mach_io_kick(): apply effects.
 */
static c3_o
_mach_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_mach* mac_u = (u3_mach*)car_u;
  u3_noun  i_wir, tag, tmp;
  c3_o     ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3__mach != i_wir)
     || (c3n == u3r_cell(cad, &tag, &tmp)) )
  {
    ret_o = c3n;
  }
  //  [%connect id=@ud =addr]
  //
  else if ( c3y == u3r_sing_c("connect", tag) ) {
    u3_noun id, adr;
    c3_d    id_d;

    if (  (c3n == u3r_cell(tmp, &id, &adr))
       || (c3n == u3r_safe_chub(id, &id_d)) )
    {
      ret_o = c3n;
    }
    else {
      ret_o = c3y;
      _mach_conn_connect(mac_u, id_d, adr);
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
      _mach_conn_send_bytes(mac_u, id_d, len_w, q);
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
      _mach_conn_close(mac_u, id_d);
    }
  }
  else {
    ret_o = c3n;
  }

  u3z(wir);
  u3z(cad);
  return ret_o;
}

/* _mach_born_news(): initialization complete on %born.
 */
static void
_mach_born_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_auto* car_u = egg_u->car_u;

  if ( u3_ovum_done == new_e ) {
    car_u->liv_o = c3y;
  }
}

/* _mach_born_bail(): %born failed.
 */
static void
_mach_born_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3l_log("mach: %%born failure");
  u3z(lud);
  u3_ovum_free(egg_u);
}

/* _mach_io_talk(): notify %mach that we're live.
 */
static void
_mach_io_talk(u3_auto* car_u)
{
  u3_noun wir = u3nc(c3__mach, u3_nul);
  u3_noun cad = u3nc(c3__born, u3_nul);

  u3_auto_peer(
    u3_auto_plan(car_u, u3_ovum_init(0, c3__m, wir, cad)),
    0,
    _mach_born_news,
    _mach_born_bail);

  car_u->liv_o = c3y;
}

/* _mach_io_exit(): close every connection and free the driver.
 */
static void
_mach_io_exit(u3_auto* car_u)
{
  u3_mach* mac_u = (u3_mach*)car_u;

  while ( mac_u->con_u ) {
    u3_tcon* con_u = mac_u->con_u;

    //  u3_auto_exit() already freed every event still queued, so
    //  only an event in work is safe to detach
    if ( c3n == con_u->wok_o ) {
      con_u->egg_u = 0;
    }

    _mach_conn_free(con_u);
  }

  c3_free(mac_u->buf_y);
  c3_free(mac_u);
}

/* _mach_rcv_read(): read one unsigned number from a host setting.
 */
#if defined(U3_OS_osx)
static c3_w
_mach_rcv_read(const c3_c* nam_c, c3_w col_w)
{
  c3_w   val_w = 0;
  size_t len_i = sizeof(val_w);

  (void)col_w;
  return ( sysctlbyname(nam_c, &val_w, &len_i, 0, 0) ) ? 0 : val_w;
}
#elif defined(U3_OS_linux)
static c3_w
_mach_rcv_read(const c3_c* nam_c, c3_w col_w)
{
  FILE*         fil_u = fopen(nam_c, "r");
  unsigned long val_l = 0;

  if ( !fil_u ) {
    return 0;
  }

  for ( c3_w i_w = 0; i_w <= col_w; i_w++ ) {
    if ( 1 != fscanf(fil_u, "%lu", &val_l) ) {
      val_l = 0;
      break;
    }
  }

  fclose(fil_u);

  //  linux doubles what we ask for, into an int
  return ( val_l > (INT_MAX / 2) ) ? (INT_MAX / 2) : (c3_w)val_l;
}
#endif

/* _mach_rcv_limits(): size the read buffer and the SO_RCVBUF request.
 *
 *   the kernel's receive buffer caps both the tcp window and how many
 *   bytes one read can take, so we want it as big as the host allows.
 *   left alone, it autotunes up to one limit; asked for, it gets up
 *   to another, and stops autotuning. so we ask only where the second
 *   limit is higher. root can raise either, so we read them at start.
 *
 *   macos: autotunes to net.inet.tcp.autorcvbufmax (4 MiB); grants up
 *   to kern.ipc.maxsockbuf (8 MiB), so we ask.
 *   linux: autotunes to the third field of net.ipv4.tcp_rmem (6 MB,
 *   32 MB on new kernels); grants up to net.core.rmem_max (208 KiB),
 *   which it doubles for overhead, so we usually don't ask.
 *   elsewhere we leave the buffer alone and guess its size.
 *
 *   we read with a buffer as big as the kernel's, so one event can
 *   take all it holds.
 */
static void
_mach_rcv_limits(u3_mach* mac_u)
{
  c3_w aut_w = 0;  //  autotuning limit
  c3_w ask_w = 0;  //  most we can ask for
  c3_w got_w = 0;  //  what that gets us

#if defined(U3_OS_osx)
  aut_w = _mach_rcv_read("net.inet.tcp.autorcvbufmax", 0);
  ask_w = _mach_rcv_read("kern.ipc.maxsockbuf", 0);
  got_w = ask_w;
#elif defined(U3_OS_linux)
  aut_w = _mach_rcv_read("/proc/sys/net/ipv4/tcp_rmem", 2);
  ask_w = _mach_rcv_read("/proc/sys/net/core/rmem_max", 0);
  got_w = 2 * ask_w;
#endif

  if ( !aut_w && !got_w ) {
    mac_u->siz_w = MACH_READ_DEFAULT;
    mac_u->rcv_w = 0;
  }
  else if ( got_w > aut_w ) {
    mac_u->siz_w = got_w;
    mac_u->rcv_w = ask_w;
  }
  else {
    mac_u->siz_w = aut_w;
    mac_u->rcv_w = 0;
  }

  u3l_log("mach: read buffer %u, SO_RCVBUF %u", mac_u->siz_w, mac_u->rcv_w);
}

/* u3_mach_io_init(): initialize mach driver.
 */
u3_auto*
u3_mach_io_init(u3_pier* pir_u)
{
  u3_mach* mac_u = c3_calloc(sizeof(*mac_u));
  u3_auto* car_u = &mac_u->car_u;

  _mach_rcv_limits(mac_u);
  mac_u->buf_y = c3_malloc(mac_u->siz_w);
  mac_u->bus_o = c3n;

  car_u->nam_m = c3__mach;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _mach_io_talk;
  car_u->io.kick_f = _mach_io_kick;
  car_u->io.exit_f = _mach_io_exit;

  return car_u;
}
