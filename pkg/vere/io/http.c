/// @file

#include "vere.h"

#include <limits.h>
#include <string.h>

#include "h2o.h"
#include "h2o/websocket.h"
#include "noun.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "version.h"

typedef struct _u3_h2o_serv {
  h2o_globalconf_t fig_u;             //  h2o global config
  h2o_context_t    ctx_u;             //  h2o ctx
  h2o_accept_ctx_t cep_u;             //  h2o accept ctx
  h2o_hostconf_t*  hos_u;             //  h2o host config
  h2o_handler_t*   han_u;             //  h2o request handler
} u3_h2o_serv;

/* u3_rsat: http request state.
*/
  typedef enum {
    u3_rsat_init = 0,                   //  initialized
    u3_rsat_plan = 1,                   //  planned
    u3_rsat_peek = 2,                   //  peek planned
    u3_rsat_ripe = 3                    //  responded
  } u3_rsat;

typedef struct _u3_hws u3_hws;

/* u3_hreq: incoming http request.
*/
  typedef struct _u3_hreq {
    h2o_req_t*       rec_u;             //  h2o request
    c3_w             seq_l;             //  sequence within connection
    u3_rsat          sat_e;             //  request state
    uv_timer_t*      tim_u;             //  timeout
    void*            gen_u;             //  response generator
    struct _u3_preq* peq_u;             //  scry-backed (rsat_peek only)
    u3_hws*          wsu_u;             //  websocket session (optional)
    struct _u3_hcon* hon_u;             //  connection backlink
    struct _u3_hreq* nex_u;             //  next in connection's list
    struct _u3_hreq* pre_u;             //  prev in connection's list
  } u3_hreq;

  /* u3_preq: scry-backed http request.
  */
    typedef struct _u3_preq {
      struct _u3_hreq* req_u;           //  originating request (nullable)
      struct _u3_httd* htd_u;           //  device backpointer
      u3_noun          pax;             //  partial scry path
      c3_o             las_o;           //  was scry at now
    } u3_preq;

/* u3_hcon: incoming http connection.
*/
  typedef struct _u3_hcon {
    uv_tcp_t         wax_u;             //  client stream handler
    h2o_conn_t*      con_u;             //  h2o connection
    h2o_socket_t*    sok_u;             //  h2o connection socket
    c3_w             ipf_w;             //  client ipv4
    c3_w             coq_l;             //  connection number
    c3_w             seq_l;             //  next request number
    struct _u3_http* htp_u;             //  server backlink
    struct _u3_hreq* req_u;             //  request list
    struct _u3_hcon* nex_u;             //  next in server's list
    struct _u3_hcon* pre_u;             //  prev in server's list
  } u3_hcon;

/* u3_http: http server.
*/
  typedef struct _u3_http {
    uv_tcp_t         wax_u;             //  server stream handler
    void*            h2o_u;             //  libh2o configuration
    c3_w             sev_l;             //  server number
    c3_w             coq_l;             //  next connection number
    c3_s             por_s;             //  running port
    c3_o             dis;               //  manually-configured port
    c3_o             sec;               //  logically secure
    c3_o             lop;               //  loopback-only
    c3_o             liv;               //  c3n == shutdown
    struct _u3_hcon* hon_u;             //  connection list
    struct _u3_http* nex_u;             //  next in list
    struct _u3_httd* htd_u;             //  device backpointer
  } u3_http;

/* u3_form: http config from %eyre
*/
  typedef struct _u3_form {
    c3_o             pro;               //  proxy
    c3_o             log;               //  keep access log
    c3_o             red;               //  redirect to HTTPS
    uv_buf_t         key_u;             //  PEM RSA private key
    uv_buf_t         cer_u;             //  PEM certificate chain
  } u3_form;

/* u3_hfig: general http configuration
*/
  typedef struct _u3_hfig {
    u3_form*         for_u;             //  config from %eyre
    c3_c*            key_c;             //  auth token key
    u3_noun          ses;               //  valid session tokens
    struct _u3_hreq* seq_u;             //  open slog requests
    uv_timer_t*      sit_u;             //  slog stream heartbeat
  } u3_hfig;

/* u3_httd: general http device
*/
typedef struct _u3_httd {
  u3_auto            car_u;             //  driver
  c3_l               sev_l;             //  instance number
  u3_hfig            fig_u;             //  http configuration
  u3_http*           htp_u;             //  http servers
  SSL_CTX*           tls_u;             //  server SSL_CTX*
  u3p(u3h_root)      sax_p;             //  url->scry cache
  u3p(u3h_root)      nax_p;             //  scry->noun cache
  u3_hws*            web_u;             //  websocket sessions
  c3_w               wid_l;             //  next websocket id
} u3_httd;

typedef enum {
  u3_hws_pending = 0,
  u3_hws_open    = 1,
  u3_hws_closing = 2,
  u3_hws_closed  = 3,
} u3_hwsat;

struct _u3_hws {
  h2o_websocket_conn_t* woc_u;          // h2o websocket connection
  u3_httd*              htd_u;          // device backpointer
  u3_hreq*              req_u;          // pending request (handshake)
  u3_hws*              nex_u;          // next in list
  u3_hws*              pre_u;          // prev in list
  u3_hwsat             sat_e;          // websocket state
  c3_w                 wid_l;          // websocket id
  c3_l                 sev_l;          // server id
  c3_l                 coq_l;          // connection id
  c3_l                 seq_l;          // request id
  c3_c                 key_c[29];      // sec-websocket-key buffer
};

static u3_weak _http_rec_to_httq(h2o_req_t* rec_u);
static u3_hreq* _http_req_prepare(h2o_req_t* rec_u, u3_hreq* (*new_f)(u3_hcon*, h2o_req_t*));
static void _http_serv_free(u3_http* htp_u);
static void _http_serv_start_all(u3_httd* htd_u);
static void _http_form_free(u3_httd* htd_u);
static void _http_start_respond(u3_hreq* req_u,
                    u3_noun status,
                    u3_noun headers,
                    u3_noun data,
                    u3_noun complete);
static void _http_ws_handshake(u3_hreq* req_u, u3_noun req, const c3_c* client_key);
static void _http_ws_message_cb(h2o_websocket_conn_t *conn, const struct wslay_event_on_msg_recv_arg *arg);
static void _http_ws_link(u3_httd* htd_u, u3_hws* web_u);
static void _http_ws_unlink(u3_hws* web_u);
static u3_hws* _http_ws_find(u3_httd* htd_u, c3_w wid_l);
static void _http_ws_disconnect(u3_hws* web_u);
static void _http_ws_plan_event(u3_hws* web_u, u3_noun event);
static void _http_ws_close_all(u3_httd* htd_u);
static void _http_ws_detach_request(u3_hreq* req_u);

static const c3_i TCP_BACKLOG = 16;
static const c3_w HEARTBEAT_TIMEOUT = 20 * 1000;

/* _http_close_cb(): uv_close_cb that just free's handle
*/
static void
_http_close_cb(uv_handle_t* han_u)
{
  c3_free(han_u);
}

/* _http_vec_to_meth(): convert h2o_iovec_t to meth
*/
static u3_weak
_http_vec_to_meth(h2o_iovec_t vec_u)
{
  return ( 0 == strncmp(vec_u.base, "GET",     vec_u.len) ) ? u3i_string("GET") :
         ( 0 == strncmp(vec_u.base, "PUT",     vec_u.len) ) ? u3i_string("PUT")  :
         ( 0 == strncmp(vec_u.base, "POST",    vec_u.len) ) ? u3i_string("POST") :
         ( 0 == strncmp(vec_u.base, "HEAD",    vec_u.len) ) ? u3i_string("HEAD") :
         ( 0 == strncmp(vec_u.base, "CONNECT", vec_u.len) ) ? u3i_string("CONNECT") :
         ( 0 == strncmp(vec_u.base, "DELETE",  vec_u.len) ) ? u3i_string("DELETE") :
         ( 0 == strncmp(vec_u.base, "OPTIONS", vec_u.len) ) ? u3i_string("OPTIONS") :
         ( 0 == strncmp(vec_u.base, "TRACE",   vec_u.len) ) ? u3i_string("TRACE") :
         ( 0 == strncmp(vec_u.base, "PATCH",   vec_u.len) ) ? u3i_string("PATCH") :
         u3_none;
}

/* _http_vec_to_atom(): convert h2o_iovec_t to atom (cord)
*/
static u3_noun
_http_vec_to_atom(h2o_iovec_t vec_u)
{
  return u3i_bytes(vec_u.len, (const c3_y*)vec_u.base);
}

/* _http_vec_to_octs(): convert h2o_iovec_t to (unit octs)
*/
static u3_noun
_http_vec_to_octs(h2o_iovec_t vec_u)
{
  if ( 0 == vec_u.len ) {
    return u3_nul;
  }

  // XX correct size_t -> atom?
  return u3nt(u3_nul, u3i_chubs(1, (const c3_d*)&vec_u.len),
                      _http_vec_to_atom(vec_u));
}

/* _cttp_bods_free(): free body structure.
*/
static void
_cttp_bods_free(u3_hbod* bod_u)
{
  while ( bod_u ) {
    u3_hbod* nex_u = bod_u->nex_u;

    c3_free(bod_u);
    bod_u = nex_u;
  }
}

/* _cttp_bod_from_octs(): translate octet-stream noun into body.
*/
static u3_hbod*
_cttp_bod_from_octs(u3_noun oct)
{
  c3_w len_w;

  if ( !_(u3a_is_cat(u3h(oct))) ) {     //  2GB max
    u3m_bail(c3__fail); return 0;
  }
  len_w = u3h(oct);

  {
    u3_hbod* bod_u = c3_malloc(1 + len_w + sizeof(*bod_u));
    bod_u->hun_y[len_w] = 0;
    bod_u->len_w = len_w;
    u3r_bytes(0, len_w, bod_u->hun_y, u3t(oct));

    bod_u->nex_u = 0;

    u3z(oct);
    return bod_u;
  }
}

/* _cttp_bods_to_vec(): translate body buffers to array of h2o_iovec_t
*/
static h2o_iovec_t*
_cttp_bods_to_vec(u3_hbod* bod_u, c3_w* tot_w)
{
  h2o_iovec_t* vec_u;
  c3_w len_w;

  {
    u3_hbod* bid_u = bod_u;
    len_w = 0;

    while( bid_u ) {
      len_w++;
      bid_u = bid_u->nex_u;
    }
  }

  vec_u = c3_malloc(sizeof(h2o_iovec_t) * len_w);
  len_w = 0;

  while( bod_u ) {
    vec_u[len_w] = h2o_iovec_init(bod_u->hun_y, bod_u->len_w);
    len_w++;
    bod_u = bod_u->nex_u;
  }

  *tot_w = len_w;

  return vec_u;
}

/* _http_heds_to_noun(): convert h2o_header_t to (list (pair @t @t))
*/
static u3_noun
_http_heds_to_noun(h2o_header_t* hed_u, c3_d hed_d)
{
  u3_noun hed = u3_nul;
  c3_d dex_d  = hed_d;

  h2o_header_t deh_u;

  while ( 0 < dex_d ) {
    deh_u = hed_u[--dex_d];
    hed = u3nc(u3nc(_http_vec_to_atom(*deh_u.name),
                    _http_vec_to_atom(deh_u.value)), hed);
  }

  return hed;
}

/* _http_heds_free(): free header linked list
*/
static void
_http_heds_free(u3_hhed* hed_u)
{
  while ( hed_u ) {
    u3_hhed* nex_u = hed_u->nex_u;

    c3_free(hed_u->nam_c);
    c3_free(hed_u->val_c);
    c3_free(hed_u);
    hed_u = nex_u;
  }
}

/* _http_hed_new(): create u3_hhed from nam/val cords
*/
static u3_hhed*
_http_hed_new(u3_atom nam, u3_atom val)
{
  c3_w     nam_w = u3r_met(3, nam);
  c3_w     val_w = u3r_met(3, val);
  u3_hhed* hed_u = c3_malloc(sizeof(*hed_u));

  hed_u->nam_c = c3_malloc(1 + nam_w);
  hed_u->val_c = c3_malloc(1 + val_w);
  hed_u->nam_c[nam_w] = 0;
  hed_u->val_c[val_w] = 0;
  hed_u->nex_u = 0;
  hed_u->nam_w = nam_w;
  hed_u->val_w = val_w;

  u3r_bytes(0, nam_w, (c3_y*)hed_u->nam_c, nam);
  u3r_bytes(0, val_w, (c3_y*)hed_u->val_c, val);

  return hed_u;
}

/* _http_heds_from_noun(): convert (list (pair @t @t)) to u3_hhed
*/
static u3_hhed*
_http_heds_from_noun(u3_noun hed)
{
  u3_noun deh = hed;
  u3_noun i_hed;

  u3_hhed* hed_u = 0;

  while ( u3_nul != hed ) {
    i_hed = u3h(hed);
    u3_hhed* nex_u = _http_hed_new(u3h(i_hed), u3t(i_hed));
    nex_u->nex_u = hed_u;

    hed_u = nex_u;
    hed = u3t(hed);
  }

  u3z(deh);
  return hed_u;
}

/* _http_req_is_auth(): returns c3y if rec_u contains a valid auth cookie
*/
static c3_o
_http_cookie_has_token(u3_hfig* fig_u, h2o_iovec_t coo_u)
{
  if ( NULL == coo_u.base || 0 == coo_u.len ) {
    return c3n;
  }

  c3_c* key_c = fig_u->key_c;
  c3_c  val_c[128];
  c3_y  val_y = 0;
  size_t  i_i = 0;
  size_t  j_i = 0;

  while ( i_i < coo_u.len ) {
    if ( ('\0' == key_c[j_i]) && ('=' == coo_u.base[i_i]) ) {
      i_i++;
      while ( i_i < coo_u.len
           && ';' != coo_u.base[i_i]
           && val_y < sizeof(val_c) )
      {
        val_c[val_y++] = coo_u.base[i_i++];
      }
      break;
    }
    else if ( coo_u.base[i_i] == key_c[j_i] ) {
      j_i++;
    }
    else {
      j_i = 0;
    }
    i_i++;
  }

  if ( 0 == val_y ) {
    return c3n;
  }

  u3_noun tok = u3i_bytes(val_y, (const c3_y*)val_c);
  c3_o aut = u3kdi_has(u3k(fig_u->ses), tok);
  u3_assert( (c3y == aut) || (c3n == aut) );
  u3z(tok);
  return aut;
}

static c3_o
_http_req_is_auth(u3_hfig* fig_u, h2o_req_t* rec_u)
{
  ssize_t idx_i = -1;

  while ( 1 ) {
    idx_i = h2o_find_header_by_str(&rec_u->headers,
                                   H2O_STRLIT("cookie"),
                                   idx_i);
    if ( -1 == idx_i ) {
      break;
    }

    h2o_iovec_t coo_u = rec_u->headers.entries[idx_i].value;
    if ( c3y == _http_cookie_has_token(fig_u, coo_u) ) {
      return c3y;
    }
  }

  return c3n;
}

/* _http_req_find(): find http request in connection by sequence.
*/
static u3_hreq*
_http_req_find(u3_hcon* hon_u, c3_w seq_l)
{
  u3_hreq* req_u = hon_u->req_u;

  //  XX glories of linear search
  //
  while ( req_u ) {
    if ( seq_l == req_u->seq_l ) {
      return req_u;
    }
    req_u = req_u->nex_u;
  }
  return 0;
}

/* _http_req_link(): link http request to connection
*/
static void
_http_req_link(u3_hcon* hon_u, u3_hreq* req_u)
{
  req_u->hon_u = hon_u;
  req_u->seq_l = hon_u->seq_l++;
  req_u->nex_u = hon_u->req_u;

  if ( 0 != req_u->nex_u ) {
    req_u->nex_u->pre_u = req_u;
  }
  hon_u->req_u = req_u;
}

/* _http_req_unlink(): remove http request from connection
*/
static void
_http_req_unlink(u3_hreq* req_u)
{
  if ( 0 != req_u->pre_u ) {
    req_u->pre_u->nex_u = req_u->nex_u;

    if ( 0 != req_u->nex_u ) {
      req_u->nex_u->pre_u = req_u->pre_u;
    }
  }
  else {
    req_u->hon_u->req_u = req_u->nex_u;

    if ( 0 != req_u->nex_u ) {
      req_u->nex_u->pre_u = 0;
    }
  }
}

/* _http_seq_link(): store slog stream request in state
*/
static void
_http_seq_link(u3_hcon* hon_u, u3_hreq* req_u)
{
  u3_hfig* fig_u = &hon_u->htp_u->htd_u->fig_u;
  req_u->hon_u = hon_u;
  req_u->seq_l = hon_u->seq_l++;
  req_u->nex_u = fig_u->seq_u;

  if ( 0 != req_u->nex_u ) {
    req_u->nex_u->pre_u = req_u;
  }
  fig_u->seq_u = req_u;
}

/* _http_seq_unlink(): remove slog stream request from state
*/
static void
_http_seq_unlink(u3_hreq* req_u)
{
  u3_hfig* fig_u = &req_u->hon_u->htp_u->htd_u->fig_u;
  if ( 0 != req_u->pre_u ) {
    req_u->pre_u->nex_u = req_u->nex_u;

    if ( 0 != req_u->nex_u ) {
      req_u->nex_u->pre_u = req_u->pre_u;
    }
  }
  else {
    fig_u->seq_u = req_u->nex_u;

    if ( 0 != req_u->nex_u ) {
      req_u->nex_u->pre_u = 0;
    }
  }

  //  unlink from async scry request if present
  //
  if ( req_u->peq_u ) {
    req_u->peq_u->req_u = 0;
  }
}

/* _http_req_to_duct(): translate srv/con/req to duct
*/
static u3_noun
_http_req_to_duct(u3_hreq* req_u)
{
  return u3nc(u3i_string("http-server"),
              u3nq(u3dc("scot", c3__uv, req_u->hon_u->htp_u->sev_l),
                   u3dc("scot", c3__ud, req_u->hon_u->coq_l),
                   u3dc("scot", c3__ud, req_u->seq_l),
                   u3_nul));
}

/* _http_req_kill(): kill http request in %eyre.
*/
static void
_http_req_kill(u3_hreq* req_u)
{
  u3_httd* htd_u = req_u->hon_u->htp_u->htd_u;
  u3_noun wir    = _http_req_to_duct(req_u);
  u3_noun cad    = u3nc(u3i_string("cancel-request"), u3_nul);

  u3_auto_plan(&htd_u->car_u, u3_ovum_init(0, c3__e, wir, cad));
}

/* _http_ws_link(): link websocket session to device list.
*/
static void
_http_ws_link(u3_httd* htd_u, u3_hws* web_u)
{
  web_u->htd_u = htd_u;
  web_u->nex_u = htd_u->web_u;
  web_u->pre_u = 0;

  if ( 0 != web_u->nex_u ) {
    web_u->nex_u->pre_u = web_u;
  }
  htd_u->web_u = web_u;
}

/* _http_ws_unlink(): unlink websocket session from device list.
*/
static void
_http_ws_unlink(u3_hws* web_u)
{
  if ( 0 != web_u->pre_u ) {
    web_u->pre_u->nex_u = web_u->nex_u;

    if ( 0 != web_u->nex_u ) {
      web_u->nex_u->pre_u = web_u->pre_u;
    }
  }
  else if ( web_u->htd_u->web_u == web_u ) {
    web_u->htd_u->web_u = web_u->nex_u;

    if ( 0 != web_u->nex_u ) {
      web_u->nex_u->pre_u = 0;
    }
  }

  web_u->nex_u = 0;
  web_u->pre_u = 0;
}

/* _http_ws_find(): locate websocket session by id.
*/
static u3_hws*
_http_ws_find(u3_httd* htd_u, c3_w wid_l)
{
  u3_hws* web_u = htd_u->web_u;

  while ( 0 != web_u ) {
    if ( wid_l == web_u->wid_l ) {
      return web_u;
    }
    web_u = web_u->nex_u;
  }

  return 0;
}

/* _http_ws_to_duct(): rebuild duct for websocket session.
*/
static u3_noun
_http_ws_to_duct(u3_hws* web_u)
{
  return u3nc(u3i_string("http-server"),
              u3nq(u3dc("scot", c3__uv, web_u->sev_l),
                   u3dc("scot", c3__ud, web_u->coq_l),
                   u3dc("scot", c3__ud, web_u->seq_l),
                   u3_nul));
}

/* _http_ws_plan_event(): queue websocket event for eyre.
*/
static void
_http_ws_plan_event(u3_hws* web_u, u3_noun event)
{
  u3_noun wir = _http_ws_to_duct(web_u);
  u3_noun pay = u3nc(u3i_chub((c3_d)web_u->wid_l), event);
  u3_noun cad = u3nc(u3i_string("websocket-event"), pay);

  // u3l_log("http: ws emit wid=%u event=%s", web_u->wid_l, u3r_string(u3h(cad)));

  u3_auto_plan(&web_u->htd_u->car_u, u3_ovum_init(0, c3__e, wir, cad));
}

/* _http_ws_close_all(): close all active websocket sessions.
*/
static void
_http_ws_close_all(u3_httd* htd_u)
{
  u3_hws* web_u = htd_u->web_u;

  while ( 0 != web_u ) {
    u3_hws* nex_u = web_u->nex_u;
    _http_ws_disconnect(web_u);
    web_u = nex_u;
  }
}

/* _http_ws_detach_request(): release websocket session tied to request (if pending).
*/
static void
_http_ws_detach_request(u3_hreq* req_u)
{
  if ( 0 == req_u->wsu_u ) {
    return;
  }

  u3_hws* web_u = req_u->wsu_u;
  req_u->wsu_u = 0;

  if ( 0 != web_u->req_u && web_u->req_u == req_u ) {
    web_u->req_u = 0;
    web_u->sat_e = u3_hws_closed;
    _http_ws_unlink(web_u);
    c3_free(web_u);
  }
}

typedef struct _u3_hgen {
  h2o_generator_t neg_u;             // response callbacks
  c3_o            red;               // ready to send
  enum {                             //
    u3_hgen_wait = 0,                //  more expected
    u3_hgen_done = 1,                //  complete
    u3_hgen_fail = 2                 //  failed
  } sat_e;
  u3_hbod*        bod_u;             // pending body
  u3_hbod*        nud_u;             // pending free
  u3_hhed*        hed_u;             // pending free
  u3_hreq*        req_u;             // originating request
} u3_hgen;

/* _http_req_close(): clean up & deallocate request
*/
static void
_http_req_close(u3_hreq* req_u)
{
  if ( 0 != req_u->wsu_u ) {
    _http_ws_detach_request(req_u);
  }

  //  client canceled request before response
  //
  if ( u3_rsat_plan == req_u->sat_e ) {
    _http_req_kill(req_u);
  }

  if ( 0 != req_u->tim_u ) {
    uv_close((uv_handle_t*)req_u->tim_u, _http_close_cb);
    req_u->tim_u = 0;
  }
}

/* _http_req_done(): request finished, deallocation callback
*/
static void
_http_req_done(void* ptr_v)
{
  u3_hreq* req_u = (u3_hreq*)ptr_v;
  _http_req_close(req_u);
  _http_req_unlink(req_u);
}

/* _http_seq_done(): slog stream request finished, deallocation callback
*/
static void
_http_seq_done(void* ptr_v)
{
  u3_hreq* seq_u = (u3_hreq*)ptr_v;
  _http_req_close(seq_u);
  _http_seq_unlink(seq_u);
}

static void
_http_hgen_send(u3_hgen* gen_u);

/* _http_req_timer_cb(): request timeout callback
*/
static void
_http_req_timer_cb(uv_timer_t* tim_u)
{
  u3_hreq* req_u = tim_u->data;

  switch ( req_u->sat_e ) {
    case u3_rsat_init: u3_assert(0);

    case u3_rsat_plan: {
      _http_req_kill(req_u);
      req_u->sat_e = u3_rsat_ripe;

      c3_c* msg_c = "gateway timeout";
      h2o_send_error_generic(req_u->rec_u, 504, msg_c, msg_c, 0);

      if ( 0 != req_u->wsu_u ) {
        _http_ws_detach_request(req_u);
      }
    } break;

    case u3_rsat_peek: {
      req_u->peq_u->req_u = 0;
      c3_c* msg_c = "gateway timeout";
      h2o_send_error_generic(req_u->rec_u, 504, msg_c, msg_c, 0);
    } break;

    case u3_rsat_ripe: {
      u3_hgen* gen_u = req_u->gen_u;

      //  inform %eyre if response was incomplete
      //
      if ( u3_hgen_wait == gen_u->sat_e ) {
        _http_req_kill(req_u);
      }

      gen_u->sat_e = u3_hgen_fail;

      if ( c3y == gen_u->red ) {
        _http_hgen_send(gen_u);
      }
    } break;
  }
}

/* _http_req_new(): receive standard http request.
*/
static u3_hreq*
_http_req_new(u3_hcon* hon_u, h2o_req_t* rec_u)
{
  u3_hreq* req_u = h2o_mem_alloc_shared(&rec_u->pool, sizeof(*req_u),
                                        _http_req_done);
  memset(req_u, 0, sizeof(*req_u));
  req_u->rec_u = rec_u;
  req_u->sat_e = u3_rsat_init;

  _http_req_link(hon_u, req_u);

  return req_u;
}

/* _http_seq_new(): receive slog stream http request.
*/
static u3_hreq*
_http_seq_new(u3_hcon* hon_u, h2o_req_t* rec_u)
{
  u3_hreq* req_u = h2o_mem_alloc_shared(&rec_u->pool, sizeof(*req_u),
                                        _http_seq_done);
  memset(req_u, 0, sizeof(*req_u));
  req_u->rec_u = rec_u;
  req_u->sat_e = u3_rsat_plan;

  _http_seq_link(hon_u, req_u);

  return req_u;
}

static void
_http_cache_respond(u3_hreq* req_u, u3_noun nun);

static void
_http_scry_respond(u3_hreq* req_u, u3_noun nun);

typedef struct _byte_range {
  c3_z beg_z;
  c3_z end_z;
} byte_range;

/* _chunk_align(): align range to a nearby chunk
*/
static void
_chunk_align(byte_range* rng_u)
{
  c3_z siz_z = 4194304;  // 4MiB

  if ( SIZE_MAX != rng_u->beg_z ) {
    if ( rng_u->beg_z > rng_u->end_z ) {
      rng_u->beg_z = SIZE_MAX;
      rng_u->end_z = SIZE_MAX;
    }
    else {
      // XX an out-of-bounds request could be aligned to in-bounds
      // resulting in a 200 or 206 response instead of 416.
      // browsers should have the total length from content-range,
      // and send reasonable range requests.
      //
      rng_u->beg_z = (rng_u->beg_z / siz_z) * siz_z;
      rng_u->end_z = (rng_u->beg_z + siz_z) - 1;
    }
  }
  else if ( SIZE_MAX != rng_u->end_z ) {
    // round up to multiple of siz_z
    rng_u->end_z = siz_z * ((rng_u->end_z / siz_z) + 1);
  }
}

/* _parse_range(): get a range from '-' delimited text
*/
static byte_range
_parse_range(c3_c* txt_c, c3_w len_w)
{
  c3_c* hep_c = memchr(txt_c, '-', len_w);
  byte_range rng_u;
  rng_u.beg_z = SIZE_MAX;
  rng_u.end_z = SIZE_MAX;

  if ( hep_c ) {
    rng_u.beg_z = h2o_strtosize(txt_c, hep_c - txt_c);
    rng_u.end_z = h2o_strtosize(hep_c + 1, len_w - ((hep_c + 1) - txt_c));
    // strange -> [SIZE_MAX SIZE_MAX]
    if (  ((SIZE_MAX == rng_u.beg_z) && (hep_c != txt_c))
       || ((SIZE_MAX == rng_u.end_z) && (len_w - ((hep_c + 1) - txt_c) > 0))
       || ((SIZE_MAX != rng_u.beg_z) && (rng_u.beg_z > rng_u.end_z)) )
    {
      rng_u.beg_z = SIZE_MAX;
      rng_u.end_z = SIZE_MAX;
    }
  }
  return rng_u;
}

/* _get_range(): get a _byte_range from headers
*/
static c3_o
_get_range(h2o_headers_t req_headers, byte_range* rng_u)
{
  rng_u->beg_z = SIZE_MAX;
  rng_u->end_z = SIZE_MAX;

  c3_w inx_w = h2o_find_header(&req_headers, H2O_TOKEN_RANGE, -1);
  if ( UINT32_MAX == inx_w) {
    return c3n;
  }

  if (  (req_headers.entries[inx_w].value.len >= 6)
     && (0 == memcmp("bytes=", req_headers.entries[inx_w].value.base, 6)) )
  {
    byte_range tmp_u = _parse_range(req_headers.entries[inx_w].value.base + 6,
                                    req_headers.entries[inx_w].value.len - 6);
    rng_u->beg_z = tmp_u.beg_z;
    rng_u->end_z = tmp_u.end_z;
  }

  return c3y;
}

/* _http_scry_cb(): respond and maybe cache scry result
*/
static void
_http_scry_cb(void* vod_p, u3_noun nun)
{
  u3_preq* peq_u = vod_p;
  u3_httd* htd_u = peq_u->htd_u;
  u3_hreq* req_u = peq_u->req_u;
  u3_hfig* fig_u = &req_u->hon_u->htp_u->htd_u->fig_u;
  c3_o auth = _http_req_is_auth(fig_u, req_u->rec_u);

  if ( req_u ) {
    u3_assert(u3_rsat_peek == req_u->sat_e);
    req_u->peq_u = 0;
    _http_scry_respond(req_u, u3k(nun));
  }

  // cache only if peek was not at now, and nun isn't u3_nul
  if (  (c3n == peq_u->las_o)
     && (u3_nul != nun) )
  {
    u3_noun key = u3nc(auth, u3k(peq_u->pax));
    u3h_put(htd_u->nax_p, key, nun);
    u3z(key);
  }
  else {
    u3z(nun);
  }

  u3z(peq_u->pax);
  c3_free(peq_u);
}

/* _beam: ship desk case spur
*/
typedef struct _beam {
  u3_weak  who;
  u3_weak  des;
  u3_weak  cas;
  u3_weak  pur;
} beam;

/* _free_beam(): free a beam
*/
static void
_free_beam(beam* bem)
{
  u3z(bem->who);
  u3z(bem->des);
  u3z(bem->cas);
  u3z(bem->pur);
}

/* _get_beam(): get a _beam from url
*/
static beam
_get_beam(u3_hreq* req_u, c3_c* txt_c, c3_w len_w)
{
  beam bem;

  //  get beak
  //
  for ( c3_w i_w = 0; i_w < 3; ++i_w ) {
    u3_noun* wer;
    if ( 0 == i_w ) {
      wer = &bem.who;
    }
    else if ( 1 == i_w ) {
      wer = &bem.des;
    }
    else {
      wer = &bem.cas;
    }

    // find '//'
    if (  (len_w >= 2)
       && ('/' == txt_c[0])
       && ('/' == txt_c[1]) )
    {
      *wer = u3_nul;
      txt_c++;
      len_w--;
    }
    // skip '/'
    else if ( (len_w > 0) && ('/' == txt_c[0]) ) {
      txt_c++;
      len_w--;
    }

    // '='
    if ( (len_w > 0) && ('=' == txt_c[0]) ) {
      if ( 0 == i_w ) {
        u3_http* htp_u = req_u->hon_u->htp_u;
        u3_httd* htd_u = htp_u->htd_u;
        *wer = u3dc("scot", 'p', u3i_chubs(2, htd_u->car_u.pir_u->who_d));
      }
      else if ( 1 == i_w ) {
        *wer = c3__base;
      }
      else {
        req_u->peq_u->las_o = c3y;
      }
      txt_c++;
      len_w--;
    }
    // slice cord
    else {
      c3_c* nex_c;
      c3_c* tis_c = memchr(txt_c, '=', len_w);
      c3_c* fas_c = memchr(txt_c, '/', len_w);

      if ( tis_c && fas_c ) {
        nex_c = c3_min(tis_c, fas_c);
      }
      else {
        nex_c = ( tis_c ) ? tis_c : fas_c;
      }

      if ( !nex_c ) {
        *wer = u3_none;
        return bem;
      }
      else {
        c3_w dif_w = (c3_p)(nex_c - txt_c);
        *wer = u3i_bytes(dif_w, (const c3_y*)txt_c);
        txt_c = nex_c;
        len_w = len_w - dif_w;
      }
    }
  }

  // get spur
  u3_noun tmp = u3dc("rush", u3i_bytes(len_w, (const c3_y*)txt_c), u3v_wish("stap"));
  bem.pur = ( u3_nul == tmp ) ? u3_none : u3k(u3t(tmp));
  u3z(tmp);

  return bem;
}

/* _http_req_dispatch(): dispatch http request
*/
static void
_http_req_dispatch(u3_hreq* req_u, u3_noun req)
{
  u3_assert(u3_rsat_init == req_u->sat_e);
  req_u->sat_e = u3_rsat_plan;

  {
    u3_http* htp_u = req_u->hon_u->htp_u;
    u3_httd* htd_u = htp_u->htd_u;

    c3_c* bas_c = req_u->rec_u->input.path.base;
    c3_w len_w = req_u->rec_u->input.path.len;

    // check if base url starts with '/_~_/'
    if (  (len_w < 6)
       || (0 != memcmp("/_~_/", bas_c, 5)) )
    {
      // no: inject to arvo
      u3_noun wir = _http_req_to_duct(req_u);
      u3_noun cad;
      u3_noun adr = u3nc(c3__ipv4, u3i_words(1, &req_u->hon_u->ipf_w));
      //  XX loopback automatically secure too?
      //
      u3_noun dat = u3nt(htp_u->sec, adr, req);

      cad = ( c3y == req_u->hon_u->htp_u->lop )
            ? u3nc(u3i_string("request-local"), dat)
            : u3nc(u3i_string("request"), dat);
      u3_auto_plan(&htd_u->car_u, u3_ovum_init(0, c3__e, wir, cad));
    }
    else {
      // '/_~_/' found
      bas_c = bas_c + 4;  //  retain '/' after /_~_
      len_w = len_w - 4;

      req_u->peq_u        = c3_malloc(sizeof(*req_u->peq_u));
      req_u->peq_u->req_u = req_u;
      req_u->peq_u->htd_u = htd_u;
      req_u->peq_u->las_o = c3n;
      req_u->sat_e = u3_rsat_peek;
      req_u->peq_u->pax = u3_nul;

      u3_hfig* fig_u = &req_u->hon_u->htp_u->htd_u->fig_u;
      h2o_req_t* rec_u = req_u->rec_u;

      //  set gang to [~ ~] or ~
      u3_noun gang;
      c3_o auth = _http_req_is_auth(fig_u, rec_u);
      if ( auth == c3y ) {
        gang = u3nc(u3_nul, u3_nul);
      }
      else {
        gang = u3_nul;
      }

      beam bem = _get_beam(req_u, bas_c, len_w);
      if (  (u3_none == bem.who)
         || (u3_none == bem.des)
         || (u3_none == bem.cas)
         || (u3_none == bem.pur) )
      {
        c3_c* msg_c = "bad request";
        h2o_send_error_generic(req_u->rec_u, 400, msg_c, msg_c, 0);
        u3z(gang);
        u3z(req_u->peq_u->pax);
        _free_beam(&bem);
        return;
      }

      h2o_headers_t req_headers = req_u->rec_u->headers;
      byte_range rng_u;
      c3_o rng_o = _get_range(req_headers, &rng_u);

      // prepare spur for eyre range scry
      //
      u3_noun spur;
      if ( c3n == rng_o ) {
        // full range: '/range/0//foo'
        spur = u3nq(u3i_string("range"), c3_s1('0'), u3_blip, u3k(bem.pur));
      }
      else {
        _chunk_align(&rng_u);

        u3_atom beg = ( SIZE_MAX == rng_u.beg_z) ?
                      u3_blip : u3dc("scot", c3__ud, u3i_chub(rng_u.beg_z));
        u3_atom end = ( SIZE_MAX == rng_u.end_z) ?
                      u3_blip : u3dc("scot", c3__ud, u3i_chub(rng_u.end_z));

        spur = u3nq(u3i_string("range"), beg, end, u3k(bem.pur));
      }

      // peek or respond from cache
      //
      if ( c3y == req_u->peq_u->las_o ) {
        u3_noun our = u3dc("scot", 'p', u3i_chubs(2, htd_u->car_u.pir_u->who_d));
        if ( c3y == u3r_sing(our, bem.who) ) {
          u3_pier_peek_last(htd_u->car_u.pir_u, gang, c3__ex,
                            u3k(bem.des), spur, req_u->peq_u, _http_scry_cb);
        }
        else {
          c3_c* msg_c = "bad request";
          h2o_send_error_generic(req_u->rec_u, 400, msg_c, msg_c, 0);
          u3z(gang);
          u3z(spur);
          u3z(req_u->peq_u->pax);
        }
        u3z(our);
      }
      else {
        u3_noun bam = u3nq(u3k(bem.who), u3k(bem.des), u3k(bem.cas), spur);
        u3_noun key = u3nc(auth, u3k(bam));
        u3_weak nac = u3h_get(htd_u->nax_p, key);
        u3z(key);

        if (  (u3_none == nac)
           || ((u3_nul == gang) && (c3y == u3r_at(14, nac))) )
        {
          // maybe cache, then serve subsequent range requests from cache
          u3z(req_u->peq_u->pax);
          req_u->peq_u->pax = u3k(bam);
          u3_pier_peek(htd_u->car_u.pir_u, gang, u3nt(0, c3__ex, bam),
                       req_u->peq_u, _http_scry_cb);
          u3z(nac);
        }
        else {
          _http_scry_respond(req_u, nac);
          u3z(bam);
          u3z(gang);
        }
      }
      _free_beam(&bem);
    }
  }
}

/* _http_ws_handshake(): handle websocket handshake request.
*/
static void
_http_ws_handshake(u3_hreq* req_u, u3_noun req, const c3_c* client_key)
{
  //  capture the owning HTTP server/device so we can reuse shared state
  //
  u3_http* htp_u = req_u->hon_u->htp_u;
  u3_httd* htd_u = htp_u->htd_u;

  //  websocket ids start at 1; initialize the counter lazily
  //
  if ( 0 == htd_u->wid_l ) {
    htd_u->wid_l = 1;
  }

  //  allocate a websocket session, mark it pending, and record the
  //  identifiers needed to upgrade/respond later (server/connection/request)
  //
  u3_hws* web_u = c3_calloc(sizeof(*web_u));
  web_u->sat_e = u3_hws_pending;
  web_u->wid_l = htd_u->wid_l++;
  web_u->sev_l = htp_u->sev_l;
  web_u->coq_l = req_u->hon_u->coq_l;
  web_u->seq_l = req_u->seq_l;
  web_u->req_u = req_u;
  web_u->woc_u = 0;

  //  stash the Sec-WebSocket-Key so h2o can finalize the upgrade later
  //
  ssize_t key_index = h2o_find_header_by_str(&req_u->rec_u->headers,
                                             H2O_STRLIT("sec-websocket-key"), -1);
  size_t key_len = ( -1 != key_index )
                     ? req_u->rec_u->headers.entries[key_index].value.len
                     : 0;
  if ( key_len >= sizeof(web_u->key_c) ) {
    key_len = sizeof(web_u->key_c) - 1;
  }
  memcpy(web_u->key_c, client_key, key_len);
  web_u->key_c[key_len] = 0;

  //  link the session into the device list and note the pending plan on req
  //
  _http_ws_link(htd_u, web_u);
  req_u->wsu_u = web_u;
  req_u->sat_e = u3_rsat_plan;

  //  build the `%http-server` duct: ["http-server" uv-id conn-id req-id]
  //
  u3_noun wir = _http_req_to_duct(req_u);
  //  peer IPv4 as `[c3__ipv4 (atom ip-address)]`
  //
  c3_w ipf_w = req_u->hon_u->ipf_w;
  u3_noun adr = u3nc(c3__ipv4, u3i_words(1, &ipf_w));
  //
  u3_noun dat = u3nt(htp_u->sec, adr, u3k(req));
  //
  u3_noun pay = u3nc(u3i_chub((c3_d)web_u->wid_l), dat);
  //  final ovum: ["websocket-handshake" pay], i.e. [%websocket-handshake websocket-id=@ secure=? =address:eyre =request:http]
  //
  u3_noun cad = u3nc(u3i_string("websocket-handshake"), pay);

  //  enqueue the ovum for %eyre and drop the temporary request noun
  //
  u3_auto_plan(&htd_u->car_u, u3_ovum_init(0, c3__e, wir, cad));

  u3z(req);
}

/* _http_ws_accept(): accept websocket handshake.
*/
static void
_http_ws_accept(u3_hws* web_u)
{
  if ( 0 == web_u || u3_hws_pending != web_u->sat_e ) {
    return;
  }

  u3_hreq* req_u = web_u->req_u;
  if ( 0 == req_u ) {
    return;
  }

  if ( 0 != req_u->tim_u ) {
    uv_timer_stop(req_u->tim_u);
  }

  req_u->sat_e = u3_rsat_ripe;

  // u3l_log("http: ws accept wid=%u req=%p rec=%p", web_u->wid_l, (void*)req_u, (void*)req_u->rec_u);

  //  guard against h2o freeing the request while we upgrade; if that happens,
  //  _http_req_close() will no longer tear down the websocket session.
  web_u->req_u = 0;

  web_u->woc_u = h2o_upgrade_to_websocket(req_u->rec_u,
                                          web_u->key_c,
                                          web_u,
                                          _http_ws_message_cb);

  if ( 0 == web_u->woc_u ) {
    c3_c* msg_c = "websocket upgrade failed";
    h2o_send_error_generic(req_u->rec_u, 500, msg_c, msg_c, 0);
    web_u->req_u = req_u;
    req_u->wsu_u = 0;
    web_u->sat_e = u3_hws_closed;
    _http_ws_unlink(web_u);
    c3_free(web_u);
    return;
  }

  web_u->sat_e = u3_hws_open;
  req_u->wsu_u = web_u;

  // u3l_log("http: ws proceed scheduled wid=%u conn=%p", web_u->wid_l, (void*)web_u->woc_u);

  /* h2o will invoke h2o_websocket_proceed() once the HTTP upgrade completes
   * (see on_complete in lib/websocket.c). Calling it here would dereference a
   * null sock before the upgrade is finalized.
   */
}

/* _http_ws_reject(): reject websocket handshake.
*/
static void
_http_ws_reject(u3_hws* web_u)
{
  if ( 0 == web_u || u3_hws_pending != web_u->sat_e ) {
    return;
  }

  web_u->sat_e = u3_hws_closed;

  if ( 0 != web_u->req_u ) {
    u3_hreq* req_u = web_u->req_u;
    if ( 0 != req_u->tim_u ) {
      uv_timer_stop(req_u->tim_u);
    }
    req_u->sat_e = u3_rsat_ripe;
    req_u->wsu_u = 0;

    c3_c* msg_c = "websocket rejected";
    h2o_send_error_generic(req_u->rec_u, 403, msg_c, msg_c, 0);
  }

  _http_ws_unlink(web_u);
  c3_free(web_u);
}

/* _http_ws_disconnect(): close websocket connection.
*/
static void
_http_ws_disconnect(u3_hws* web_u)
{
  if ( 0 == web_u ) {
    return;
  }

  if ( u3_hws_pending == web_u->sat_e ) {
    _http_ws_reject(web_u);
    return;
  }

  if ( (u3_hws_open == web_u->sat_e) && (0 != web_u->woc_u) ) {
    web_u->sat_e = u3_hws_closing;
    h2o_websocket_close(web_u->woc_u);
  }
}

/* _http_ws_send_message(): serialize and send websocket message to client.
*/
static c3_o
_http_ws_send_message(u3_hws* web_u, u3_noun msg)
{
  if ( 0 == web_u || u3_hws_open != web_u->sat_e || 0 == web_u->woc_u ) {
    u3z(msg);
    return c3n;
  }

  u3_noun opcode = u3h(msg);
  u3_noun body   = u3t(msg);

  c3_w opc_w;
  if ( c3n == u3r_safe_word(opcode, &opc_w) ) {
    u3l_log("http: bad websocket opcode");
    u3z(msg);
    return c3n;
  }

  c3_y* buf_y = 0;
  size_t len_w = 0;

  if ( u3_nul != body ) {
    if ( u3_nul != u3h(body) ) {
      u3l_log("http: malformed websocket message body");
      u3z(msg);
      return c3n;
    }

    u3_noun oct = u3t(body);
    c3_d len_d = u3r_chub(0, u3h(oct));
    if ( len_d > SIZE_MAX ) {
      u3l_log("http: websocket message too large");
      u3z(msg);
      return c3n;
    }
    len_w = (size_t)len_d;

    if ( 0 != len_w ) {
      buf_y = c3_malloc(len_w);
      u3r_bytes(0, len_w, buf_y, u3t(oct));
    }
  }

  struct wslay_event_msg out = {
      .opcode = (uint8_t)(opc_w & 0xFF),
      .msg = buf_y,
      .msg_length = len_w,
  };

  int sas_i = wslay_event_queue_msg(web_u->woc_u->ws_ctx, &out);
  if ( 0 != sas_i ) {
    u3l_log("http: websocket queue failed (%d)", sas_i);
    if ( buf_y ) {
      c3_free(buf_y);
    }
    u3z(msg);
    return c3n;
  }

  if ( buf_y ) {
    c3_free(buf_y);
  }

  h2o_websocket_proceed(web_u->woc_u);
  u3z(msg);
  return c3y;
}

/* _http_ws_message_cb(): websocket receive / close callback.
*/
static void
_http_ws_message_cb(h2o_websocket_conn_t *conn,
                    const struct wslay_event_on_msg_recv_arg *arg)
{
  u3_hws* web_u = (u3_hws*)conn->data;

  if ( 0 == web_u ) {
    return;
  }

  if ( NULL == arg ) {
    if ( u3_hws_closed != web_u->sat_e ) {
      web_u->sat_e = u3_hws_closed;
      _http_ws_plan_event(web_u, u3nc(u3i_string("disconnect"), u3_nul));
    }

    _http_ws_unlink(web_u);
    c3_free(web_u);
    return;
  }

  if ( WSLAY_CONNECTION_CLOSE == arg->opcode ) {
    if ( u3_hws_closed != web_u->sat_e ) {
      web_u->sat_e = u3_hws_closed;
      _http_ws_plan_event(web_u, u3nc(u3i_string("disconnect"), u3_nul));
    }
    return;
  }

  u3_noun payload;
  if ( 0 == arg->msg_length ) {
    payload = u3_nul;
  }
  else {
    u3_noun octs = u3nc(u3i_chub((c3_d)arg->msg_length),
                        u3i_bytes(arg->msg_length, (const c3_y*)arg->msg));
    payload = u3nc(u3_nul, octs);
  }

  u3_noun event = u3nc(u3i_string("message"),
                       u3nc(u3i_chub((c3_d)arg->opcode), payload));

  _http_ws_plan_event(web_u, event);
}

/* _http_cache_respond(): respond with a simple-payload:http
*/
static void
_http_cache_respond(u3_hreq* req_u, u3_noun nun)
{
  h2o_req_t* rec_u = req_u->rec_u;
  u3_httd* htd_u = req_u->hon_u->htp_u->htd_u;

  if ( u3_nul == nun ) {
    u3_weak req = _http_rec_to_httq(rec_u);
    if ( u3_none == req ) {
      if ( (u3C.wag_w & u3o_verbose) ) {
        u3l_log("strange %.*s request", (c3_i)rec_u->method.len,
                rec_u->method.base);
      }
      c3_c* msg_c = "bad request";
      h2o_send_error_generic(rec_u, 400, msg_c, msg_c, 0);
    }
    else {
      u3_hreq* req_u = _http_req_prepare(rec_u, _http_req_new);
      _http_req_dispatch(req_u, req);
    }
  }
  else if ( u3_none == u3r_at(7, nun) ) {
    h2o_send_error_500(rec_u, "Internal Server Error", "scry failed", 0);
  }
  else {
    u3_noun auth, response_header, data;
    u3x_qual(u3t(u3t(nun)), &auth, 0, &response_header, &data);
    u3_noun status, headers;
    u3x_cell(response_header, &status, &headers);

    // check auth
    if ( (c3y == auth)
      && (c3n == _http_req_is_auth(&htd_u->fig_u, rec_u)) )
    {
      h2o_send_error_403(rec_u, "Unauthorized", "unauthorized", 0);
    }
    else {
      req_u->sat_e = u3_rsat_plan;
      _http_start_respond(req_u, u3k(status), u3k(headers), u3k(data), c3y);
    }
  }
  u3z(nun);
}

/* _http_scry_respond(): respond with a simple-payload:http
*/
static void
_http_scry_respond(u3_hreq* req_u, u3_noun nun)
{
  h2o_req_t* rec_u = req_u->rec_u;
  u3_httd* htd_u = req_u->hon_u->htp_u->htd_u;

  if ( u3_nul == nun ) {
    u3_weak req = _http_rec_to_httq(rec_u);
    if ( u3_none == req ) {
      if ( (u3C.wag_w & u3o_verbose) ) {
        u3l_log("strange %.*s request", (c3_i)rec_u->method.len,
                rec_u->method.base);
      }
      c3_c* msg_c = "bad request";
      h2o_send_error_generic(rec_u, 400, msg_c, msg_c, 0);
    }
    else {
      h2o_send_error_500(rec_u, "Internal Server Error", "scry failed", 0);
    }
  }
  else if ( u3_none == u3r_at(7, nun) ) {
    h2o_send_error_500(rec_u, "Internal Server Error", "scry failed", 0);
  }
  else {
    u3_noun auth, response_header, data;
    u3x_qual(u3t(u3t(nun)), &auth, 0, &response_header, &data);
    u3_noun status, headers;
    u3x_cell(response_header, &status, &headers);

    // check auth
    if ( (c3y == auth)
      && (c3n == _http_req_is_auth(&htd_u->fig_u, rec_u)) )
    {
      h2o_send_error_403(rec_u, "Unauthorized", "unauthorized", 0);
    }
    else {
      req_u->sat_e = u3_rsat_plan;
      _http_start_respond(req_u, u3k(status), u3k(headers), u3k(data), c3y);
    }
  }
  u3z(nun);
}

/* _http_cache_scry_cb(): insert scry result into noun cache
*/
static void
_http_cache_scry_cb(void* vod_p, u3_noun nun)
{
  u3_preq* peq_u = vod_p;
  u3_httd* htd_u = peq_u->htd_u;
  u3_hreq* req_u = peq_u->req_u;

  if ( req_u ) {
    u3_assert(u3_rsat_peek == req_u->sat_e);
    req_u->peq_u = 0;
    _http_cache_respond(req_u, u3k(nun));
  }

  u3h_put(htd_u->nax_p, peq_u->pax, nun);
  u3z(peq_u->pax);
  c3_free(peq_u);
}

/* _http_req_cache(): attempt to serve http request from cache
*/
static c3_o
_http_req_cache(u3_hreq* req_u)
{
  u3_assert(u3_rsat_init == req_u->sat_e);

  h2o_iovec_t method = req_u->rec_u->method;
  if (0 != strncmp("GET", method.base, method.len)) {
    return c3n;
  }

  u3_httd* htd_u = req_u->hon_u->htp_u->htd_u;

  u3_noun url = u3dc("scot", 't', _http_vec_to_atom(req_u->rec_u->path));
  u3_weak sac = u3h_get(htd_u->sax_p, url);
  u3z(url);
  
  if ( u3_none == sac ) {
    return c3n;
  }

  u3_weak nac = u3h_get(htd_u->nax_p, sac);
  if ( u3_none == nac ) {
    // noun not in cache; scry it
    req_u->peq_u        = c3_malloc(sizeof(*req_u->peq_u));
    req_u->peq_u->req_u = req_u;
    req_u->peq_u->htd_u = htd_u;
    req_u->peq_u->pax   = u3k(sac);

    req_u->sat_e = u3_rsat_peek;

    u3_noun gang = u3nc(u3_nul, u3_nul);
    u3_pier_peek_last(htd_u->car_u.pir_u, gang, c3__ex,
                      u3_nul, sac, req_u->peq_u, _http_cache_scry_cb);
    return c3y;
  }
  u3z(sac);
  _http_cache_respond(req_u, nac);
  return c3y;
}

/* _http_hgen_dispose(): dispose response generator and buffers
*/
static void
_http_hgen_dispose(void* ptr_v)
{
  u3_hgen* gen_u = (u3_hgen*)ptr_v;
  _http_heds_free(gen_u->hed_u);
  gen_u->hed_u = 0;
  _cttp_bods_free(gen_u->nud_u);
  gen_u->nud_u = 0;
  _cttp_bods_free(gen_u->bod_u);
  gen_u->bod_u = 0;
}

/* _http_hgen_send(): send (some/more of a) response.
*/
static void
_http_hgen_send(u3_hgen* gen_u)
{
  u3_hreq*     req_u = gen_u->req_u;
  h2o_req_t*   rec_u = req_u->rec_u;
  c3_w         len_w;
  h2o_iovec_t* vec_u = _cttp_bods_to_vec(gen_u->bod_u, &len_w);

  //  not ready again until _proceed
  //
  u3_assert( c3y == gen_u->red );
  gen_u->red = c3n;

  //  stash [bod_u] to free later
  //
  _cttp_bods_free(gen_u->nud_u);
  gen_u->nud_u = gen_u->bod_u;
  gen_u->bod_u = 0;

  switch ( gen_u->sat_e ) {
    case u3_hgen_wait: {
      h2o_send(rec_u, vec_u, len_w, H2O_SEND_STATE_IN_PROGRESS);
      uv_timer_start(req_u->tim_u, _http_req_timer_cb, 45 * 1000, 0);
    } break;

    case u3_hgen_done: {
      //  close connection if shutdown pending
      //
      u3_h2o_serv* h2o_u = req_u->hon_u->htp_u->h2o_u;

      if ( 0 != h2o_u->ctx_u.shutdown_requested ) {
        rec_u->http1_is_persistent = 0;
      }

      h2o_send(rec_u, vec_u, len_w, H2O_SEND_STATE_FINAL);
    } break;

    case u3_hgen_fail: {
      h2o_send(rec_u, vec_u, len_w, H2O_SEND_STATE_ERROR);
    } break;
  }

  c3_free(vec_u);
}

/* _http_hgen_stop(): h2o is closing an in-progress response.
*/
static void
_http_hgen_stop(h2o_generator_t* neg_u, h2o_req_t* rec_u)
{
  u3_hgen* gen_u = (u3_hgen*)neg_u;

  //  response not complete, enqueue cancel
  //
  if ( u3_hgen_wait == gen_u->sat_e ) {
    _http_req_kill(gen_u->req_u);
  }
}

/* _http_hgen_proceed(): h2o is ready for more response data.
*/
static void
_http_hgen_proceed(h2o_generator_t* neg_u, h2o_req_t* rec_u)
{
  u3_hgen* gen_u = (u3_hgen*)neg_u;
  u3_hreq* req_u = gen_u->req_u;

  // sanity check
  u3_assert( rec_u == req_u->rec_u );

  gen_u->red = c3y;

  if ( gen_u->bod_u || (u3_hgen_wait != gen_u->sat_e) ) {
    _http_hgen_send(gen_u);
  }
}

/* _http_start_respond(): write a [%http-response %start ...] to h2o_req_t->res
*/
static void
_http_start_respond(u3_hreq* req_u,
                    u3_noun status,
                    u3_noun headers,
                    u3_noun data,
                    u3_noun complete)
{
  if ( u3_rsat_plan != req_u->sat_e ) {
    u3l_log("http: %%start not sane");
    u3z(status); u3z(headers); u3z(data); u3z(complete);
    return;
  }

  req_u->sat_e = u3_rsat_ripe;

  uv_timer_stop(req_u->tim_u);

  h2o_req_t* rec_u = req_u->rec_u;

  rec_u->res.status = status;
  rec_u->res.reason = (status < 200) ? "weird" :
                      (status < 300) ? "ok" :
                      (status < 400) ? "moved" :
                      (status < 500) ? "missing" :
                      "hosed";

  u3_hhed* hed_u = _http_heds_from_noun(u3k(headers));
  u3_hhed* deh_u = hed_u;

  c3_i has_len_i = 0;

  while ( 0 != hed_u ) {
    if ( 0x200 <= rec_u->version ) {
      h2o_strtolower(hed_u->nam_c, hed_u->nam_w);

      if ( 0 == strncmp(hed_u->nam_c, "connection", 10) ) {
        hed_u = hed_u->nex_u;
        continue;
      }
    }
    if ( 0 == strncmp(hed_u->nam_c, "content-length", 14) ) {
      has_len_i = 1;
    }
    else {
      h2o_add_header_by_str(&rec_u->pool, &rec_u->res.headers,
                            hed_u->nam_c, hed_u->nam_w, 0, 0,
                            hed_u->val_c, hed_u->val_w);
    }

    hed_u = hed_u->nex_u;
  }

  u3_hgen* gen_u = h2o_mem_alloc_shared(&rec_u->pool, sizeof(*gen_u),
                                        _http_hgen_dispose);
  gen_u->neg_u = (h2o_generator_t){ _http_hgen_proceed, _http_hgen_stop };
  gen_u->red   = c3y;
  gen_u->sat_e = ( c3y == complete ) ? u3_hgen_done : u3_hgen_wait;
  gen_u->bod_u = ( u3_nul == data ) ?
                 0 : _cttp_bod_from_octs(u3k(u3t(data)));
  gen_u->nud_u = 0;
  gen_u->hed_u = deh_u;
  gen_u->req_u = req_u;

  //  if we don't explicitly set this field, h2o will send with
  //  transfer-encoding: chunked
  //
  if ( 1 == has_len_i ) {
    rec_u->res.content_length = ( 0 == gen_u->bod_u ) ?
                                0 : gen_u->bod_u->len_w;
  }

  req_u->gen_u = gen_u;

  h2o_start_response(rec_u, &gen_u->neg_u);

  _http_hgen_send(gen_u);

  u3z(status); u3z(headers); u3z(data); u3z(complete);
}

/* _http_continue_respond(): apply [%http-response %continue ...].
*/
static void
_http_continue_respond(u3_hreq* req_u,
                       u3_noun   data,
                       u3_noun complete)
{
  if ( u3_rsat_ripe != req_u->sat_e ) {
    u3l_log("http: %%continue before %%start");
    u3z(data); u3z(complete);
    return;
  }

  u3_hgen* gen_u = req_u->gen_u;

  uv_timer_stop(req_u->tim_u);

  gen_u->sat_e = ( c3y == complete ) ? u3_hgen_done : u3_hgen_wait;

  if ( u3_nul != data ) {
    u3_hbod* bod_u = _cttp_bod_from_octs(u3k(u3t(data)));

    if ( 0 == gen_u->bod_u ) {
      gen_u->bod_u = bod_u;
    }
    else {
      u3_hbod* pre_u = gen_u->bod_u;

      while ( 0 != pre_u->nex_u ) {
        pre_u = pre_u->nex_u;
      }

      pre_u->nex_u = bod_u;
    }
  }

  if ( c3y == gen_u->red ) {
    _http_hgen_send(gen_u);
  }

  u3z(data); u3z(complete);
}

/* _http_cancel_respond(): apply [%http-response %cancel ~].
*/
static void
_http_cancel_respond(u3_hreq* req_u)
{
  switch ( req_u->sat_e ) {
    case u3_rsat_init: u3_assert(0);
    case u3_rsat_peek: u3_assert(0);

    case u3_rsat_plan: {
      req_u->sat_e = u3_rsat_ripe; // XX confirm

      c3_c* msg_c = "hosed";
      h2o_send_error_generic(req_u->rec_u, 500, msg_c, msg_c, 0);

      if ( 0 != req_u->wsu_u ) {
        _http_ws_detach_request(req_u);
      }
    } break;

    case u3_rsat_ripe: {
      u3_hgen* gen_u = req_u->gen_u;

      uv_timer_stop(req_u->tim_u);
      gen_u->sat_e = u3_hgen_fail;

      if ( c3y == gen_u->red ) {
        _http_hgen_send(gen_u);
      }
    }
  }
}

/* _http_rec_to_httq(): convert h2o_req_t to httq
*/
static u3_weak
_http_rec_to_httq(h2o_req_t* rec_u)
{
  u3_noun med = _http_vec_to_meth(rec_u->method);

  if ( u3_none == med ) {
    return u3_none;
  }

  u3_noun url = _http_vec_to_atom(rec_u->path);
  u3_noun hed = _http_heds_to_noun(rec_u->headers.entries,
                                   rec_u->headers.size);

  // restore host header
  hed = u3nc(u3nc(u3i_string("host"),
                  _http_vec_to_atom(rec_u->authority)),
             hed);

  u3_noun bod = _http_vec_to_octs(rec_u->entity);

  return u3nq(med, url, hed, bod);
}

typedef struct _h2o_uv_sock {         //  see private st_h2o_uv_socket_t
  h2o_socket_t     sok_u;             //  socket
  uv_stream_t*     han_u;             //  client stream handler (u3_hcon)
} h2o_uv_sock;

/* _http_rec_sock(): u3 http connection from h2o request; hacky.
*/
static u3_hcon*
_http_rec_sock(h2o_req_t* rec_u)
{
  h2o_uv_sock* suv_u = (h2o_uv_sock*)rec_u->conn->
                         callbacks->get_socket(rec_u->conn);
  u3_hcon*     hon_u = (u3_hcon*)suv_u->han_u;

  //  sanity check
  //
  u3_assert( hon_u->sok_u == &suv_u->sok_u );

  return hon_u;
}

/* _http_req_prepare(): creates u3 req from h2o req and initializes its timer
*/
static u3_hreq*
_http_req_prepare(h2o_req_t* rec_u,
                  u3_hreq* (*new_f)(u3_hcon*, h2o_req_t*))
{
  u3_hcon* hon_u = _http_rec_sock(rec_u);
  u3_hreq* seq_u = new_f(hon_u, rec_u);

  seq_u->tim_u = c3_malloc(sizeof(*seq_u->tim_u));
  seq_u->tim_u->data = seq_u;
  uv_timer_init(u3L, seq_u->tim_u);
  uv_timer_start(seq_u->tim_u, _http_req_timer_cb, 600 * 1000, 0);

  return seq_u;
}

/* _http_seq_accept(): handle incoming http request on slogstream endpoint
*/
static c3_i
_http_seq_accept(h2o_handler_t* han_u, h2o_req_t* rec_u)
{
  u3_hcon* hon_u = _http_rec_sock(rec_u);
  c3_o     aut_o = _http_req_is_auth(&hon_u->htp_u->htd_u->fig_u, rec_u);

  //  if the request is not authenticated, reject it
  //
  if ( c3n == aut_o ) {
    u3_hreq* req_u = _http_req_prepare(rec_u, _http_req_new);
    req_u->sat_e = u3_rsat_plan;
    _http_start_respond(req_u, 403, u3_nul, u3_nul, c3y);
  }
  //  if it is authenticated, send slogstream/sse headers
  //
  else {
    u3_hreq* req_u = _http_req_prepare(rec_u, _http_seq_new);
    u3_noun  hed   = u3nl(u3nc(u3i_string("Content-Type"),
                               u3i_string("text/event-stream")),
                          u3nc(u3i_string("Cache-Control"),
                               u3i_string("no-cache")),
                          u3nc(u3i_string("Connection"),
                               u3i_string("keep-alive")),
                          u3_none);

    _http_start_respond(req_u, 200, hed, u3_nul, c3n);

    //TODO  auth token may expire at some point. if we want to close the
    //      slogstream when that happens, we need to store the token that
    //      was used alongside it...
  }

  return 0;
}

/* _http_sat_accept(): handle incoming http request on status endpoint
*/
static c3_i
_http_sat_accept(h2o_handler_t* han_u, h2o_req_t* rec_u)
{
  c3_o bus_o;
  {
    u3_hcon* hon_u = _http_rec_sock(rec_u);
    u3_httd* htd_u = hon_u->htp_u->htd_u;
    u3_pier* pir_u = htd_u->car_u.pir_u;
    bus_o = pir_u->god_u->pin_o;
  }

  if ( c3y == bus_o ) {
    rec_u->res.status = 429;
    rec_u->res.reason = "busy";
  }
  else {
    rec_u->res.status = 204;
    rec_u->res.reason = "no content";
  }

  rec_u->res.content_length = 0;
  h2o_send_inline(rec_u, NULL, 0);

  return 0;
}

/* _http_rec_accept(); handle incoming http request from h2o.
*/
static c3_i
_http_rec_accept(h2o_handler_t* han_u, h2o_req_t* rec_u)
{
  u3_weak req = _http_rec_to_httq(rec_u);

  if ( u3_none == req ) {
    if ( (u3C.wag_w & u3o_verbose) ) {
      u3l_log("strange %.*s request", (c3_i)rec_u->method.len,
              rec_u->method.base);
    }
    c3_c* msg_c = "bad request";
    h2o_send_error_generic(rec_u, 400, msg_c, msg_c, 0);
  }
  else {
    u3_hreq* req_u = _http_req_prepare(rec_u, _http_req_new);
    const c3_c* client_key = 0;
    c3_i ws_rc = h2o_is_websocket_handshake(rec_u, &client_key);

    if ( (0 == ws_rc) && (0 != client_key) ) {
      _http_ws_handshake(req_u, req, client_key);
    }
    else {
      if ( ws_rc < 0 ) {
        if ( 0 != req_u->tim_u ) {
          uv_timer_stop(req_u->tim_u);
        }
        req_u->sat_e = u3_rsat_ripe;
        u3l_log("http: invalid websocket handshake");
        c3_c* msg_c = "bad websocket handshake";
        h2o_send_error_generic(rec_u, 400, msg_c, msg_c, 0);
        u3z(req);
      }
      else if ( c3n == _http_req_cache(req_u) ) {
        _http_req_dispatch(req_u, req);
      }
      else {
        u3z(req);
      }
    }
  }

  return 0;
}

/* _http_conn_find(): find http connection in server by sequence.
*/
static u3_hcon*
_http_conn_find(u3_http *htp_u, c3_w coq_l)
{
  u3_hcon* hon_u = htp_u->hon_u;

  //  XX glories of linear search
  //
  while ( hon_u ) {
    if ( coq_l == hon_u->coq_l ) {
      return hon_u;
    }
    hon_u = hon_u->nex_u;
  }
  return 0;
}

/* _http_conn_link(): link http request to connection
*/
static void
_http_conn_link(u3_http* htp_u, u3_hcon* hon_u)
{
  hon_u->htp_u = htp_u;
  hon_u->coq_l = htp_u->coq_l++;
  hon_u->nex_u = htp_u->hon_u;

  if ( 0 != hon_u->nex_u ) {
    hon_u->nex_u->pre_u = hon_u;
  }
  htp_u->hon_u = hon_u;
}

/* _http_conn_unlink(): remove http request from connection
*/
static void
_http_conn_unlink(u3_hcon* hon_u)
{
  if ( 0 != hon_u->pre_u ) {
    hon_u->pre_u->nex_u = hon_u->nex_u;

    if ( 0 != hon_u->nex_u ) {
      hon_u->nex_u->pre_u = hon_u->pre_u;
    }
  }
  else {
    hon_u->htp_u->hon_u = hon_u->nex_u;

    if ( 0 != hon_u->nex_u ) {
      hon_u->nex_u->pre_u = 0;
    }
  }
}

/* _http_conn_free(): free http connection on close.
*/
static void
_http_conn_free(uv_handle_t* han_t)
{
  u3_hcon* hon_u = (u3_hcon*)han_t;
  u3_http* htp_u = hon_u->htp_u;
  u3_h2o_serv* h2o_u = htp_u->h2o_u;

  u3_assert( 0 == hon_u->req_u );

#if 0
  {
    c3_w len_w = 0;

    u3_hcon* noh_u = htp_u->hon_u;

    while ( 0 != noh_u ) {
      len_w++;
      noh_u = noh_u->nex_u;
    }

    u3l_log("http conn free %d of %u server %d", hon_u->coq_l, len_w, htp_u->sev_l);
  }
#endif

  _http_conn_unlink(hon_u);

#if 0
  {
    c3_w len_w = 0;

    u3_hcon* noh_u = htp_u->hon_u;

    while ( 0 != noh_u ) {
      len_w++;
      noh_u = noh_u->nex_u;
    }

    u3l_log("http conn free %u remaining", len_w);
  }
#endif

  if ( (0 == htp_u->hon_u) && (0 != h2o_u->ctx_u.shutdown_requested) ) {
#if 0
    u3l_log("http conn free %d free server %d", hon_u->coq_l, htp_u->sev_l);
#endif
    _http_serv_free(htp_u);
  }

  c3_free(hon_u);
}

/* _http_conn_new(): create and accept http connection.
*/
static u3_hcon*
_http_conn_new(u3_http* htp_u)
{
  u3_hcon* hon_u = c3_malloc(sizeof(*hon_u));
  hon_u->seq_l = 1;
  hon_u->ipf_w = 0;
  hon_u->req_u = 0;
  hon_u->sok_u = 0;
  hon_u->con_u = 0;
  hon_u->pre_u = 0;

  _http_conn_link(htp_u, hon_u);

#if 0
  u3l_log("http conn neww %d server %d", hon_u->coq_l, htp_u->sev_l);
#endif

  return hon_u;
}

/* _http_serv_find(): find http server by sequence.
*/
static u3_http*
_http_serv_find(u3_httd* htd_u, c3_l sev_l)
{
  u3_http* htp_u = htd_u->htp_u;

  //  XX glories of linear search
  //
  while ( htp_u ) {
    if ( sev_l == htp_u->sev_l ) {
      return htp_u;
    }
    htp_u = htp_u->nex_u;
  }
  return 0;
}

/* _http_serv_link(): link http server to global state.
*/
static void
_http_serv_link(u3_httd* htd_u, u3_http* htp_u)
{
  // XX link elsewhere initially, relink on start?

  if ( 0 != htd_u->htp_u ) {
    htp_u->sev_l = 1 + htd_u->htp_u->sev_l;
  }
  else {
    htp_u->sev_l = htd_u->sev_l;
  }

  htp_u->nex_u = htd_u->htp_u;
  htp_u->htd_u = htd_u;
  htd_u->htp_u = htp_u;
}

/* _http_serv_unlink(): remove http server from global state.
*/
static void
_http_serv_unlink(u3_http* htp_u)
{
  // XX link elsewhere initially, relink on start?
#if 0
  u3l_log("http serv unlink %d", htp_u->sev_l);
#endif
  u3_http* pre_u = htp_u->htd_u->htp_u;

  if ( pre_u == htp_u ) {
    pre_u = htp_u->nex_u;
  }
  else {
    //  XX glories of linear search
    //
    while ( pre_u ) {
      if ( pre_u->nex_u == htp_u ) {
        pre_u->nex_u = htp_u->nex_u;
      }
      else pre_u = pre_u->nex_u;
    }
  }
}

/* _http_h2o_context_dispose(): h2o_context_dispose, inlined and cleaned up.
*/
static void
_http_h2o_context_dispose(h2o_context_t* ctx)
{
  h2o_globalconf_t *config = ctx->globalconf;
  size_t i, j;

  for (i = 0; config->hosts[i] != NULL; ++i) {
    h2o_hostconf_t *hostconf = config->hosts[i];
    for (j = 0; j != hostconf->paths.size; ++j) {
      h2o_pathconf_t *pathconf = hostconf->paths.entries + j;
      h2o_context_dispose_pathconf_context(ctx, pathconf);
    }
    h2o_context_dispose_pathconf_context(ctx, &hostconf->fallback_path);
  }

  c3_free(ctx->_pathconfs_inited.entries);
  c3_free(ctx->_module_configs);

  h2o_timeout_dispose(ctx->loop, &ctx->zero_timeout);
  h2o_timeout_dispose(ctx->loop, &ctx->hundred_ms_timeout);
  h2o_timeout_dispose(ctx->loop, &ctx->handshake_timeout);
  h2o_timeout_dispose(ctx->loop, &ctx->http1.req_timeout);
  h2o_timeout_dispose(ctx->loop, &ctx->http2.idle_timeout);

  // NOTE: linked in http2/connection, never unlinked
  h2o_timeout_unlink(&ctx->http2._graceful_shutdown_timeout);

  h2o_timeout_dispose(ctx->loop, &ctx->http2.graceful_shutdown_timeout);
  h2o_timeout_dispose(ctx->loop, &ctx->proxy.io_timeout);
  h2o_timeout_dispose(ctx->loop, &ctx->one_sec_timeout);

  h2o_filecache_destroy(ctx->filecache);
  ctx->filecache = NULL;

  /* clear storage */
  for (i = 0; i != ctx->storage.size; ++i) {
    h2o_context_storage_item_t *item = ctx->storage.entries + i;
    if (item->dispose != NULL) {
        item->dispose(item->data);
    }
  }

  c3_free(ctx->storage.entries);

  h2o_multithread_unregister_receiver(ctx->queue, &ctx->receivers.hostinfo_getaddr);
  h2o_multithread_destroy_queue(ctx->queue);

  if (ctx->_timestamp_cache.value != NULL) {
    h2o_mem_release_shared(ctx->_timestamp_cache.value);
  }

  // NOTE: explicit uv_run removed
}

/* _http_serv_really_free(): free http server.
*/
static void
_http_serv_really_free(u3_http* htp_u)
{
  u3_assert( 0 == htp_u->hon_u );

  if ( 0 != htp_u->h2o_u ) {
    u3_h2o_serv* h2o_u = htp_u->h2o_u;

    if ( 0 != h2o_u->cep_u.ssl_ctx ) {
      SSL_CTX_free(h2o_u->cep_u.ssl_ctx);
    }

    h2o_config_dispose(&h2o_u->fig_u);

    // XX h2o_cleanup_thread if not restarting?

    c3_free(htp_u->h2o_u);
    htp_u->h2o_u = 0;
  }

  _http_serv_unlink(htp_u);
  c3_free(htp_u);
}

/* http_serv_free_cb(): timer callback for freeing http server.
*/
static void
http_serv_free_cb(uv_timer_t* tim_u)
{
  u3_http* htp_u = tim_u->data;

#if 0
  u3l_log("http serv free cb %d", htp_u->sev_l);
#endif

  _http_serv_really_free(htp_u);

  uv_close((uv_handle_t*)tim_u, _http_close_cb);
}

/* _http_serv_free(): begin to free http server.
*/
static void
_http_serv_free(u3_http* htp_u)
{
#if 0
  u3l_log("http serv free %d", htp_u->sev_l);
#endif

  u3_assert( 0 == htp_u->hon_u );

  if ( 0 == htp_u->h2o_u ) {
    _http_serv_really_free(htp_u);
  }
  else {
    u3_h2o_serv* h2o_u = htp_u->h2o_u;

    _http_h2o_context_dispose(&h2o_u->ctx_u);

    // NOTE: free deferred to allow timers to be closed
    // this is a heavy-handed workaround for the lack of
    // close callbacks in h2o_timer_t
    // it's unpredictable how many event-loop turns will
    // be required to finish closing the underlying uv_timer_t
    // and we can't free until that's done (or we have UB)
    // testing reveals 5s to be a long enough deferral
    uv_timer_t* tim_u = c3_malloc(sizeof(*tim_u));

    tim_u->data = htp_u;

    uv_timer_init(u3L, tim_u);
    uv_timer_start(tim_u, http_serv_free_cb, 5000, 0);
  }
}

/* _http_serv_close_cb(): http server uv_close callback.
*/
static void
_http_serv_close_cb(uv_handle_t* han_u)
{
  u3_http* htp_u = (u3_http*)han_u;
  u3_httd* htd_u = htp_u->htd_u;
  htp_u->liv = c3n;

  // otherwise freed by the last linked connection
  if ( 0 == htp_u->hon_u ) {
    _http_serv_free(htp_u);
  }

  // restart if all linked servers have been shutdown
  {
    htp_u = htd_u->htp_u;
    c3_o res = c3y;

    while ( 0 != htp_u ) {
      if ( c3y == htp_u->liv ) {
        res = c3n;
      }
      htp_u = htp_u->nex_u;
    }

    if ( (c3y == res) && (0 != htd_u->fig_u.for_u) ) {
      _http_serv_start_all(htd_u);
    }
  }
}

/* _http_serv_close(): close http server gracefully.
*/
static void
_http_serv_close(u3_http* htp_u)
{
  u3_h2o_serv* h2o_u = htp_u->h2o_u;
  h2o_context_request_shutdown(&h2o_u->ctx_u);

#if 0
  u3l_log("http serv close %d %p", htp_u->sev_l, &htp_u->wax_u);
#endif

  uv_close((uv_handle_t*)&htp_u->wax_u, _http_serv_close_cb);
}

/* _http_serv_new(): create new http server.
*/
static u3_http*
_http_serv_new(u3_httd* htd_u, c3_s por_s, c3_o dis, c3_o sec, c3_o lop)
{
  u3_http* htp_u = c3_malloc(sizeof(*htp_u));

  htp_u->coq_l = 1;
  htp_u->por_s = por_s;
  htp_u->dis = dis;
  htp_u->sec = sec;
  htp_u->lop = lop;
  htp_u->liv = c3y;
  htp_u->h2o_u = 0;
  htp_u->hon_u = 0;
  htp_u->nex_u = 0;

  _http_serv_link(htd_u, htp_u);

  return htp_u;
}

/* _http_serv_accept(): accept new http connection.
*/
static void
_http_serv_accept(u3_http* htp_u)
{
  u3_hcon* hon_u = _http_conn_new(htp_u);

  uv_tcp_init(u3L, &hon_u->wax_u);

  c3_i sas_i;

  if ( 0 != (sas_i = uv_accept((uv_stream_t*)&htp_u->wax_u,
                               (uv_stream_t*)&hon_u->wax_u)) ) {
    if ( (u3C.wag_w & u3o_verbose) ) {
      u3l_log("http: accept: %s", uv_strerror(sas_i));
    }

    uv_close((uv_handle_t*)&hon_u->wax_u, _http_conn_free);
    return;
  }

  hon_u->sok_u = h2o_uv_socket_create((uv_stream_t*)&hon_u->wax_u,
                                      _http_conn_free);

  h2o_accept(&((u3_h2o_serv*)htp_u->h2o_u)->cep_u, hon_u->sok_u);

  // capture h2o connection (XX fragile)
  hon_u->con_u = (h2o_conn_t*)hon_u->sok_u->data;

  struct sockaddr_in adr_u;
  h2o_socket_getpeername(hon_u->sok_u, (struct sockaddr*)&adr_u);
  hon_u->ipf_w = ( adr_u.sin_family != AF_INET ) ?
                 0 : ntohl(adr_u.sin_addr.s_addr);
}

/* _http_serv_listen_cb(): uv_connection_cb for uv_listen
*/
static void
_http_serv_listen_cb(uv_stream_t* str_u, c3_i sas_i)
{
  u3_http* htp_u = (u3_http*)str_u;

  if ( 0 != sas_i ) {
    u3l_log("http: listen_cb: %s", uv_strerror(sas_i));
  }
  else {
    _http_serv_accept(htp_u);
  }
}

/* _http_serv_init_h2o(): initialize h2o ctx and handlers for server.
*/
static u3_h2o_serv*
_http_serv_init_h2o(SSL_CTX* tls_u, c3_o log, c3_o red)
{
  u3_h2o_serv* h2o_u = c3_calloc(sizeof(*h2o_u));

  h2o_config_init(&h2o_u->fig_u);
  h2o_u->fig_u.server_name = h2o_iovec_init(
                               H2O_STRLIT("urbit/vere-" URBIT_VERSION));

  //  set maximum request size to 512 MiB
  //
  h2o_u->fig_u.max_request_entity_size = 512 * 1024 * 1024;

  // XX default pending vhost/custom-domain design
  // XX revisit the effect of specifying the port
  h2o_u->hos_u = h2o_config_register_host(&h2o_u->fig_u,
                                          h2o_iovec_init(H2O_STRLIT("default")),
                                          65535);

  h2o_u->cep_u.ctx = (h2o_context_t*)&h2o_u->ctx_u;
  h2o_u->cep_u.hosts = h2o_u->fig_u.hosts;
  h2o_u->cep_u.ssl_ctx = tls_u;

  h2o_u->han_u = h2o_create_handler(&h2o_u->hos_u->fallback_path,
                                    sizeof(*h2o_u->han_u));
  if ( c3y == red ) {
    // XX h2o_redirect_register
    h2o_u->han_u->on_req = _http_rec_accept;
  }
  else {
    h2o_u->han_u->on_req = _http_rec_accept;
  }

  //  register runtime endpoints
  //
  {
    h2o_pathconf_t* pac_u;
    h2o_handler_t*  han_u;

    //  slog stream
    //
    pac_u = h2o_config_register_path(h2o_u->hos_u, "/~_~/slog", 0);
    han_u = h2o_create_handler(pac_u, sizeof(*han_u));
    han_u->on_req = _http_seq_accept;

    //  status (per spinner)
    //
    pac_u = h2o_config_register_path(h2o_u->hos_u, "/~_~/healthz", 0);
    han_u = h2o_create_handler(pac_u, sizeof(*han_u));
    han_u->on_req = _http_sat_accept;
  }

  if ( c3y == log ) {
    // XX move this to post serv_start and put the port in the name
#if 0
    c3_c* pax_c = u3_Host.dir_c;
    u3_noun now = u3dc("scot", c3__da, u3k(u3A->now));
    c3_c* now_c = u3r_string(now);
    c3_c* nam_c = ".access.log";
    c3_w len_w = 1 + strlen(pax_c) + 1 + strlen(now_c) + strlen(nam_c);

    c3_c* paf_c = c3_malloc(len_w);
    snprintf(paf_c, len_w, "%s/%s%s", pax_c, now_c, nam_c);

    h2o_access_log_filehandle_t* fil_u =
      h2o_access_log_open_handle(paf_c, 0, H2O_LOGCONF_ESCAPE_APACHE);

    h2o_access_log_register(&h2o_u->hos_u->fallback_path, fil_u);

    c3_free(paf_c);
    c3_free(now_c);
    u3z(now);
#endif
  }

  // XX h2o_compress_register

  h2o_context_init(&h2o_u->ctx_u, u3L, &h2o_u->fig_u);

  return h2o_u;
}

/* _http_serv_start(): start http server.
*/
static void
_http_serv_start(u3_http* htp_u)
{
  u3_pier*            pir_u = htp_u->htd_u->car_u.pir_u;
  struct sockaddr_in  adr_u;

  memset(&adr_u, 0, sizeof(adr_u));
  adr_u.sin_family = AF_INET;
  adr_u.sin_addr.s_addr = ( c3y == htp_u->lop ) ?
                          htonl(INADDR_LOOPBACK) :
                          INADDR_ANY;

  if ( 0 != u3_Host.ops_u.bin_c && c3n == htp_u->lop ) {
    inet_pton(AF_INET, u3_Host.ops_u.bin_c, &adr_u.sin_addr);
  }

  uv_tcp_init(u3L, &htp_u->wax_u);

  /*  Try ascending ports.
  */
  while ( 1 ) {
    c3_i sas_i;

    adr_u.sin_port = htons(htp_u->por_s);

    if ( 0 != (sas_i = uv_tcp_bind(&htp_u->wax_u,
                                   (const struct sockaddr*)&adr_u, 0)) ||
         0 != (sas_i = uv_listen((uv_stream_t*)&htp_u->wax_u,
                                 TCP_BACKLOG, _http_serv_listen_cb)) ) {
      if ( UV_EADDRNOTAVAIL == sas_i ) {
        u3l_log("http: ip address not available");
        u3_king_bail();
      }
      if ( c3y == htp_u->dis ) {
        u3l_log("http: listen (%" PRIu16 "): %s", htp_u->por_s,
                uv_strerror(sas_i));
        u3_king_bail();
      }
      if ( (UV_EADDRINUSE == sas_i) || (UV_EACCES == sas_i) ) {
        if ( (c3y == htp_u->sec) && (443 == htp_u->por_s) ) {
          htp_u->por_s = 8443;
        }
        else if ( (c3n == htp_u->sec) && (80 == htp_u->por_s) ) {
          htp_u->por_s = 8080;
        }
        else {
          htp_u->por_s++;
          //  XX
          //
          if ( c3n == htp_u->lop ) {
            if ( c3y == htp_u->sec ) {
              pir_u->pes_s = htp_u->por_s;
            }
            else {
              pir_u->per_s = htp_u->por_s;
            }
          }
        }

        continue;
      }

      u3l_log("http: listen: %s", uv_strerror(sas_i));

      _http_serv_free(htp_u);
      return;
    }

    u3l_log("http: %s live on %s://localhost:%d",
            (c3y == htp_u->lop) ? "loopback" : "web interface",
            (c3y == htp_u->sec) ? "https" : "http",
            htp_u->por_s);

    break;
  }
}

static uv_buf_t
_http_wain_to_buf(u3_noun wan)
{
  c3_w len_w = u3_mcut_path(0, 0, (c3_c)10, u3k(wan));
  c3_c* buf_c = c3_malloc(1 + len_w);

  u3_mcut_path(buf_c, 0, (c3_c)10, wan);
  buf_c[len_w] = 0;

  return uv_buf_init(buf_c, len_w);
}

/* _http_init_tls: initialize OpenSSL context
*/
static SSL_CTX*
_http_init_tls(uv_buf_t key_u, uv_buf_t cer_u)
{
  // XX require 1.1.0 and use TLS_server_method()
  SSL_CTX* tls_u = SSL_CTX_new(SSLv23_server_method());
  // XX use SSL_CTX_set_max_proto_version() and SSL_CTX_set_min_proto_version()
  SSL_CTX_set_options(tls_u, SSL_OP_NO_SSLv2 |
                             SSL_OP_NO_SSLv3 |
                             // SSL_OP_NO_TLSv1 | // XX test
                             SSL_OP_NO_COMPRESSION);

  SSL_CTX_set_default_verify_paths(tls_u);
  SSL_CTX_set_session_cache_mode(tls_u, SSL_SESS_CACHE_OFF);
  SSL_CTX_set_cipher_list(tls_u,
                          "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:"
                          "ECDH+AES128:DH+AES:ECDH+3DES:DH+3DES:RSA+AESGCM:"
                          "RSA+AES:RSA+3DES:!aNULL:!MD5:!DSS");

  // enable ALPN for HTTP 2 support
#if H2O_USE_ALPN
  {
    SSL_CTX_set_ecdh_auto(tls_u, 1);
    h2o_ssl_register_alpn_protocols(tls_u, h2o_http2_alpn_protocols);
  }
#endif

  {
    BIO* bio_u = BIO_new_mem_buf(key_u.base, key_u.len);
    EVP_PKEY* pky_u = PEM_read_bio_PrivateKey(bio_u, 0, 0, 0);
    c3_i sas_i = SSL_CTX_use_PrivateKey(tls_u, pky_u);

    EVP_PKEY_free(pky_u);
    BIO_free(bio_u);

    if( 0 == sas_i ) {
      u3l_log("http: load private key failed:");
      FILE* fil_u = u3_term_io_hija();
      ERR_print_errors_fp(fil_u);
      u3_term_io_loja(1, fil_u);

      SSL_CTX_free(tls_u);

      return 0;
    }
  }

  {
    BIO* bio_u = BIO_new_mem_buf(cer_u.base, cer_u.len);
    X509* xer_u = PEM_read_bio_X509_AUX(bio_u, 0, 0, 0);
    c3_i sas_i = SSL_CTX_use_certificate(tls_u, xer_u);

    X509_free(xer_u);

    if( 0 == sas_i ) {
      u3l_log("http: load certificate failed:");
      FILE* fil_u = u3_term_io_hija();
      ERR_print_errors_fp(fil_u);
      u3_term_io_loja(1,fil_u);

      BIO_free(bio_u);
      SSL_CTX_free(tls_u);

      return 0;
    }

    // get any additional CA certs, ignoring errors
    while ( 0 != (xer_u = PEM_read_bio_X509(bio_u, 0, 0, 0)) ) {
      // XX require 1.0.2 or newer and use SSL_CTX_add0_chain_cert
      SSL_CTX_add_extra_chain_cert(tls_u, xer_u);
    }

    BIO_free(bio_u);
  }

  return tls_u;
}

/* _http_write_ports_file(): update .http.ports
*/
static void
_http_write_ports_file(u3_httd* htd_u, c3_c *pax_c)
{
  c3_c* nam_c = ".http.ports";
  c3_w len_w = 1 + strlen(pax_c) + 1 + strlen(nam_c);

  c3_c* paf_c = c3_malloc(len_w);
  snprintf(paf_c, len_w, "%s/%s", pax_c, nam_c);

  c3_i por_i = c3_open(paf_c, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  c3_free(paf_c);

  u3_http* htp_u = htd_u->htp_u;
  u3_pier* pir_u = htd_u->car_u.pir_u;

  c3_c temp[32];
  while ( 0 != htp_u ) {
    if ( 0 < htp_u->por_s ) {
      u3_write_fd(por_i, temp, snprintf(temp, 32, "%u %s %s\n", htp_u->por_s,
                     (c3y == htp_u->sec) ? "secure" : "insecure",
                     (c3y == htp_u->lop) ? "loopback" : "public"));
    }

    htp_u = htp_u->nex_u;
  }

  c3_sync(por_i);
  close(por_i);
}

/* _http_release_ports_file(): remove .http.ports
*/
static void
_http_release_ports_file(c3_c *pax_c)
{
  c3_c* nam_c = ".http.ports";
  c3_w len_w = 1 + strlen(pax_c) + 1 + strlen(nam_c);
  c3_c* paf_c = c3_malloc(len_w);
  c3_i  wit_i;

  wit_i = snprintf(paf_c, len_w, "%s/%s", pax_c, nam_c);
  u3_assert(wit_i > 0);
  u3_assert(len_w == (c3_w)wit_i + 1);

  c3_unlink(paf_c);
  c3_free(paf_c);
}

static u3_hreq*
_http_search_req(u3_httd* htd_u,
                 c3_l     sev_l,
                 c3_l     coq_l,
                 c3_l     seq_l)
{
  u3_http* htp_u;
  u3_hcon* hon_u;
  u3_hreq* req_u;
  c3_w bug_w = u3C.wag_w & u3o_verbose;

  if ( !(htp_u = _http_serv_find(htd_u, sev_l)) ) {
    if ( bug_w ) {
      u3l_log("http: server not found: %x", sev_l);
    }
    return 0;
  }
  else if ( !(hon_u = _http_conn_find(htp_u, coq_l)) ) {
    if ( bug_w ) {
      u3l_log("http: connection not found: %x/%d", sev_l, coq_l);
    }
    return 0;
  }
  else if ( !(req_u = _http_req_find(hon_u, seq_l)) ) {
    if ( bug_w ) {
      u3l_log("http: request not found: %x/%d/%d",
              sev_l, coq_l, seq_l);
    }
    return 0;
  }

  return req_u;
}

/* _http_serv_start_all(): initialize and start servers based on saved config.
*/
static void
_http_serv_start_all(u3_httd* htd_u)
{
  u3_http*  htp_u;
  u3_pier*  pir_u = htd_u->car_u.pir_u;
  c3_s      por_s;
  u3_noun   sec = u3_nul;
  u3_noun   non = u3_none;
  u3_noun   dis;
  u3_form*  for_u = htd_u->fig_u.for_u;

  u3_assert( 0 != for_u );

  // if the SSL_CTX existed, it'll be freed with the servers
  htd_u->tls_u = 0;

  //  HTTPS server.
  if ( (0 != for_u->key_u.base) && (0 != for_u->cer_u.base) ) {
    htd_u->tls_u = _http_init_tls(for_u->key_u, for_u->cer_u);

    // Note: if tls_u is used for additional servers,
    // its reference count must be incremented with SSL_CTX_up_ref

    if ( 0 != htd_u->tls_u ) {
      if ( 0 == pir_u->pes_s ) {
        por_s = ( c3y == for_u->pro ) ? 8443 : 443;
        dis = c3n;
      }
      else {
        por_s = pir_u->pes_s;
        dis = c3y;
      }
      htp_u = _http_serv_new(htd_u, por_s, dis, c3y, c3n);
      htp_u->h2o_u = _http_serv_init_h2o(htd_u->tls_u, for_u->log, for_u->red);

      _http_serv_start(htp_u);
      sec = u3nc(u3_nul, htp_u->por_s);
    }
  }

  //  HTTP server.
  {
    if ( 0 == pir_u->per_s ) {
      por_s = ( c3y == for_u->pro ) ? 8080 : 80;
      dis = c3n;
    }
    else {
      por_s = pir_u->per_s;
      dis = c3y;
    }
    htp_u = _http_serv_new(htd_u, por_s, dis, c3n, c3n);
    htp_u->h2o_u = _http_serv_init_h2o(0, for_u->log, for_u->red);

    _http_serv_start(htp_u);
    non = htp_u->por_s;
  }

  //  Loopback server.
  {
    por_s = 12321;
    htp_u = _http_serv_new(htd_u, por_s, c3n, c3n, c3y);
    htp_u->h2o_u = _http_serv_init_h2o(0, for_u->log, for_u->red);

    _http_serv_start(htp_u);
  }

  //  send listening ports to %eyre
  {
    u3_assert( u3_none != non );

    //  XX remove [sen]
    //
    u3_noun wir = u3nt(u3i_string("http-server"),
                       u3dc("scot", c3__uv, htd_u->sev_l),
                       u3_nul);
    u3_noun cad = u3nt(c3__live, non, sec);

    u3_auto_plan(&htd_u->car_u, u3_ovum_init(0, c3__e, wir, cad));
  }

  _http_write_ports_file(htd_u, u3_Host.dir_c);
  _http_form_free(htd_u);
}

/* _http_serv_restart(): gracefully shutdown, then start servers.
*/
static void
_http_serv_restart(u3_httd* htd_u)
{
  u3_http* htp_u = htd_u->htp_u;

  if ( 0 == htp_u ) {
    _http_serv_start_all(htd_u);
  }
  else {
    u3l_log("http: restarting servers to apply configuration");

    while ( 0 != htp_u ) {
      if ( c3y == htp_u->liv ) {
        _http_serv_close(htp_u);
      }
      htp_u = htp_u->nex_u;
    }

    _http_release_ports_file(u3_Host.dir_c);
  }
}

/* _http_form_free(): free and unlink saved config.
*/
static void
_http_form_free(u3_httd* htd_u)
{
  u3_form* for_u = htd_u->fig_u.for_u;

  if ( 0 == for_u ) {
    return;
  }

  _http_ws_close_all(htd_u);

  if ( 0 != for_u->key_u.base ) {
    c3_free(for_u->key_u.base);
  }

  if ( 0 != for_u->cer_u.base ) {
    c3_free(for_u->cer_u.base);
  }

  c3_free(for_u);
  htd_u->fig_u.for_u = 0;
}

/* _http_auth_free(): free stored auth token state
*/
static void
_http_auth_free(u3_httd* htd_u)
{
  u3z(htd_u->fig_u.ses);
  htd_u->fig_u.ses = u3_nul;
  c3_free(htd_u->fig_u.key_c);
}

/* u3_http_ef_form(): apply configuration, restart servers.
*/
void
u3_http_ef_form(u3_httd* htd_u, u3_noun fig)
{
  u3_noun sec, pro, log, red;

  if ( (c3n == u3r_qual(fig, &sec, &pro, &log, &red) ) ||
       // confirm sec is a valid (unit ^)
       !( u3_nul == sec || ( c3y == u3du(sec) &&
                             c3y == u3du(u3t(sec)) &&
                             u3_nul == u3h(sec) ) ) ||
       // confirm valid flags ("loobeans")
       !( c3y == pro || c3n == pro ) ||
       !( c3y == log || c3n == log ) ||
       !( c3y == red || c3n == red ) ) {
    u3l_log("http: form: invalid card");
    u3z(fig);
    return;
  }

  u3_form* for_u = c3_malloc(sizeof(*for_u));
  for_u->pro = (c3_o)pro;
  for_u->log = (c3_o)log;
  for_u->red = (c3_o)red;

  if ( u3_nul != sec ) {
    u3_noun key = u3h(u3t(sec));
    u3_noun cer = u3t(u3t(sec));

    for_u->key_u = _http_wain_to_buf(u3k(key));
    for_u->cer_u = _http_wain_to_buf(u3k(cer));
  }
  else {
    for_u->key_u = uv_buf_init(0, 0);
    for_u->cer_u = uv_buf_init(0, 0);
  }

  u3z(fig);
  _http_form_free(htd_u);

  htd_u->fig_u.for_u = for_u;

  _http_serv_restart(htd_u);

  htd_u->car_u.liv_o = c3y;
}

/* u3_http_ef_form(): store set of auth tokens
*/
void
u3_http_ef_auth(u3_httd* htd_u, u3_noun fig)
{
  u3z(htd_u->fig_u.ses);
  htd_u->fig_u.ses = fig;
}

/* _http_io_talk(): start http I/O.
*/
static void
_http_io_talk(u3_auto* car_u)
{
  u3_httd* htd_u = (u3_httd*)car_u;

  //  XX remove [sen]
  //
  u3_noun wir = u3nt(u3i_string("http-server"),
                     u3dc("scot", c3__uv, htd_u->sev_l),
                     u3_nul);
  u3_noun cad = u3nc(c3__born, u3_nul);

  u3_auto_plan(car_u, u3_ovum_init(0, c3__e, wir, cad));

  //  XX set liv_o on done/swap?
  //
}

/* _http_ef_http_server(): dispatch an %http-server effect from %light.
*/
void
_http_ef_http_server(u3_httd* htd_u,
                     c3_l     sev_l,
                     c3_l     coq_l,
                     c3_l     seq_l,
                     u3_noun    tag,
                     u3_noun    dat)
{
  u3_hreq* req_u;
  u3_hws*  web_u;

  //  sets server configuration
  //
  if ( c3y == u3r_sing_c("set-config", tag) ) {
    u3_http_ef_form(htd_u, u3k(dat));
  }
  else if ( c3y == u3r_sing_c("sessions", tag) ) {
    u3_http_ef_auth(htd_u, u3k(dat));
  }
  //  handles a cache notification
  //
  else if ( c3y == u3r_sing_c("grow", tag) ) {
    // cache paths are /cache/(scot %ud aeon)/(scot %t url)
    u3_noun pax = u3k(dat);
    u3_noun url = u3h(u3t(u3t(pax)));
    u3h_put(htd_u->sax_p, url, pax);
  }
  //  responds to an open request
  //
  else if ( c3y == u3r_sing_c("websocket-response", tag) ) {
    u3_noun wid = u3k(u3h(dat));
    u3_noun res = u3k(u3t(dat));

    c3_w wid_w;
    if ( c3n == u3r_safe_word(wid, &wid_w) ) {
      u3l_log("http: invalid websocket id");
    }
    else if ( 0 == (web_u = _http_ws_find(htd_u, wid_w)) ) {
      u3l_log("http: unknown websocket id %u", wid_w);
    }
    else {
      u3_noun typ = u3h(res);

      if ( c3y == u3r_sing_c("accept", typ) ) {
        _http_ws_accept(web_u);
      }
      else if ( c3y == u3r_sing_c("reject", typ) ) {
        _http_ws_reject(web_u);
      }
      else if ( c3y == u3r_sing_c("disconnect", typ) ) {
        _http_ws_disconnect(web_u);
      }
      else if ( c3y == u3r_sing_c("message", typ) ) {
        _http_ws_send_message(web_u, u3k(u3t(res)));
      }
      else {
        u3l_log("http: unexpected websocket response");
      }
    }

    u3z(wid);
    u3z(res);
  }
  else if ( 0 != (req_u = _http_search_req(htd_u, sev_l, coq_l, seq_l)) ) {
    if ( c3y == u3r_sing_c("response", tag) ) {
      u3_noun response = dat;

      if ( 0 != req_u->wsu_u && (u3C.wag_w & u3o_verbose) ) {
        u3l_log("http: ws pending got http response sev=%u coq=%u seq=%u wid=%u",
                (c3_w)sev_l,
                (c3_w)coq_l,
                (c3_w)seq_l,
                req_u->wsu_u->wid_l);
      }

    if ( c3y == u3r_sing_c("start", u3h(response)) ) {
      //  Separate the %start message into its components.
      //
      u3_noun response_header, data, complete;
      u3_noun status, headers;
        u3x_trel(u3t(response), &response_header, &data, &complete);
        u3x_cell(response_header, &status, &headers);

        _http_start_respond(req_u, u3k(status), u3k(headers), u3k(data),
                            u3k(complete));
      }
      else if ( c3y == u3r_sing_c("continue", u3h(response)) ) {
        //  Separate the %continue message into its components.
        //
        u3_noun data, complete;
        u3x_cell(u3t(response), &data, &complete);

        _http_continue_respond(req_u, u3k(data), u3k(complete));
      }
      else if (c3y == u3r_sing_c("cancel", u3h(response))) {
        _http_cancel_respond(req_u);
      }
      else {
        u3l_log("http: strange response");
      }
    }
    else {
      u3l_log("http: strange response");
    }
  }

  u3z(tag);
  u3z(dat);
}

/* _http_stream_slog(): emit slog to open connections
*/
static void
_http_stream_slog(void* vop_p, c3_w pri_w, u3_noun tan)
{
  u3_httd* htd_u = (u3_httd*)vop_p;
  u3_hreq* seq_u = htd_u->fig_u.seq_u;

  //  only do the work if there are open slog streams
  //
  if ( 0 != seq_u ) {
    u3_weak data = u3_none;

    if ( c3y == u3a_is_atom(tan) ) {
      u3_noun lin = u3i_list(u3i_string("data:"),
                             u3k(tan),
                             c3_s2('\n', '\n'),
                             u3_none);
      u3_atom txt = u3qc_rap(3, lin);
      data = u3nt(u3_nul, u3r_met(3, txt), txt);
      u3z(lin);
    }
    else {
      u3_weak wol = u3_none;

      //  if we have no arvo kernel and can't evaluate nock,
      //  only send %leaf tanks
      //
      if ( 0 == u3A->roc ) {
        if ( c3__leaf == u3h(tan) ) {
          wol = u3nc(u3k(u3t(tan)), u3_nul);
        }
      }
      else {
        u3_noun blu = u3_term_get_blew(0);
        c3_l  col_l = u3h(blu);
        wol = u3dc("wash", u3nc(0, col_l), u3k(tan));
        u3z(blu);
      }

      if ( u3_none != wol ) {
        u3_noun low = wol;
        u3_noun paz = u3_nul;
        while ( u3_nul != low ) {
          u3_noun lin = u3i_list(u3i_string("data:"),
                                 u3qc_rap(3, u3h(low)),
                                 c3_s2('\n', '\n'),
                                 u3_none);
          paz = u3kb_weld(paz, lin);
          low = u3t(low);
        }
        u3_atom txt = u3qc_rap(3, paz);
        data = u3nt(u3_nul, u3r_met(3, txt), txt);
        u3z(paz);
      }

      u3z(wol);
    }

    if ( u3_none != data ) {
      while ( 0 != seq_u ) {
        _http_continue_respond(seq_u, u3k(data), c3n);
        seq_u = seq_u->nex_u;
      }
    }

    u3z(data);
  }

  u3z(tan);
}

/* _http_seq_heartbeat_cb(): send heartbeat to slog streams and restart timer
*/
static void
_http_seq_heartbeat_cb(uv_timer_t* tim_u)
{
  u3_httd* htd_u = tim_u->data;
  u3_hreq* seq_u = htd_u->fig_u.seq_u;

  if ( 0 != seq_u ) {
    u3_noun dat = u3nt(u3_nul, 1, c3_s1('\n'));
    while ( 0 != seq_u ) {
      _http_continue_respond(seq_u, u3k(dat), c3n);
      seq_u = seq_u->nex_u;
    }
    u3z(dat);
  }

  uv_timer_start(htd_u->fig_u.sit_u, _http_seq_heartbeat_cb,
                 HEARTBEAT_TIMEOUT, 0);
}

/* _http_io_kick(): apply effects.
*/
static c3_o
_http_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_httd* htd_u = (u3_httd*)car_u;

  u3_noun tag, dat, i_wir, t_wir;

  if (  (c3n == u3r_cell(wir, &i_wir, &t_wir))
     || (c3n == u3r_cell(cad, &tag, &dat))
     || (c3n == u3r_sing_c("http-server", i_wir)) )
  {
    u3z(wir); u3z(cad);
    return c3n;
  }

  //  XX this needs to be rewritten, it defers (c3n) in cases it should not
  //
  {
    u3_noun pud = t_wir;
    u3_noun p_pud, t_pud, tt_pud, q_pud, r_pud, s_pud;
    c3_l    sev_l, coq_l, seq_l;


    if ( (c3n == u3r_cell(pud, &p_pud, &t_pud)) ||
         (c3n == u3v_lily(c3__uv, u3k(p_pud), &sev_l)) )
    {
      u3z(wir); u3z(cad);
      return c3n;
    }

    if ( u3_nul == t_pud ) {
      coq_l = seq_l = 0;
    }
    else {
      if ( (c3n == u3r_cell(t_pud, &q_pud, &tt_pud)) ||
           (c3n == u3v_lily(c3__ud, u3k(q_pud), &coq_l)) )
      {
        u3z(wir); u3z(cad);
        return c3n;
      }

      if ( u3_nul == tt_pud ) {
        seq_l = 0;
      } else {
        if ( (c3n == u3r_cell(tt_pud, &r_pud, &s_pud)) ||
             (u3_nul != s_pud) ||
             (c3n == u3v_lily(c3__ud, u3k(r_pud), &seq_l)) )
        {
          u3z(wir); u3z(cad);
          return c3n;
        }
      }
    }

    _http_ef_http_server(htd_u, sev_l, coq_l, seq_l, u3k(tag), u3k(dat));
    u3z(wir); u3z(cad);
    return c3y;
  }
}

/* _http_io_exit(): shut down http.
*/
static void
_http_io_exit(u3_auto* car_u)
{
  u3_httd* htd_u = (u3_httd*)car_u;

  _http_ws_close_all(htd_u);

  u3h_free(htd_u->sax_p);
  u3h_free(htd_u->nax_p);

  //  dispose of configuration to avoid restarts
  //
  _http_form_free(htd_u);
  _http_auth_free(htd_u);

  //  close all servers
  //
  //  XX broken
  //
  // for ( u3_http* htp_u = htd_u->htp_u; htp_u; htp_u = htp_u->nex_u ) {
  //   _http_serv_close(htp_u);
  // }

  {
    u3_atom lin = u3i_string("data:urbit shutting down\n\n");
    u3_noun dat = u3nt(u3_nul, u3r_met(3, lin), lin);
    u3_hreq* seq_u = htd_u->fig_u.seq_u;
    while ( 0 != seq_u ) {
      _http_continue_respond(seq_u, u3k(dat), c3y);
      seq_u = seq_u->nex_u;
    }
    u3z(dat);
  }

  _http_release_ports_file(u3_Host.dir_c);
}

/* _http_io_info(): produce status info.
*/
static u3_noun
_http_io_info(u3_auto* car_u)
{
  u3_httd*  htd_u = (u3_httd*)car_u;
  u3_http*  htp_u = htd_u->htp_u;
  c3_w      sec_w = 0;
  u3_hreq*  seq_u = htd_u->fig_u.seq_u;
  u3_noun   res;

  //  XX review: metrics
  //
  while ( 0 != seq_u ) {
    sec_w++;
    seq_u = seq_u->nex_u;
  }
  res = u3i_list(
    u3_pier_mase("instance", htd_u->sev_l),
    u3_pier_mase("open-slogstreams", u3i_word(sec_w)),
    u3_none);

  while ( 0 != htp_u ) {
    res = u3nc(
      u3_pier_mass(
        u3dc("scot", c3__uv, htp_u->sev_l),
        u3i_list(
          u3_pier_mase("secure",      htp_u->sec),
          u3_pier_mase("loopback",    htp_u->lop),
          u3_pier_mase("live",        htp_u->liv),
          u3_pier_mase("port",        htp_u->por_s),
          u3_pier_mase("connections", htp_u->coq_l),
          u3_none)),
      res);
    htp_u = htp_u->nex_u;
  }
  return u3kb_flop(res);
}

/* _http_io_slog(): print status info.
*/
static void
_http_io_slog(u3_auto* car_u)
{
  u3_httd* htd_u = (u3_httd*)car_u;
  c3_y sec_y = 0;
  u3_hreq* seq_u = htd_u->fig_u.seq_u;
  while ( 0 != seq_u ) {
    sec_y++;
    seq_u = seq_u->nex_u;
  }
  u3l_log("      open slogstreams: %d", sec_y);
}

/* u3_http_io_init(): initialize http I/O.
*/
u3_auto*
u3_http_io_init(u3_pier* pir_u)
{
  u3_httd* htd_u = c3_calloc(sizeof(*htd_u));
  htd_u->sax_p = u3h_new();
  htd_u->nax_p = u3h_new_cache(512);
  htd_u->web_u = 0;
  htd_u->wid_l = 1;

  {
    u3_noun key = u3dt("cat", 3,
      u3i_string("urbauth-"),
      u3dc("scot", 'p', u3i_chubs(2, pir_u->who_d)));
    htd_u->fig_u.ses = u3_nul;
    htd_u->fig_u.key_c = u3r_string(key);
    u3z(key);
  }

  u3_auto* car_u = &htd_u->car_u;
  car_u->nam_m = c3__http;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _http_io_talk;
  car_u->io.info_f = _http_io_info;
  car_u->io.slog_f = _http_io_slog;
  car_u->io.kick_f = _http_io_kick;
  car_u->io.exit_f = _http_io_exit;

  pir_u->sop_p = htd_u;
  pir_u->sog_f = _http_stream_slog;

  uv_timer_t* sit_u = c3_malloc(sizeof(*sit_u));
  sit_u->data = htd_u;
  uv_timer_init(u3L, sit_u);
  uv_timer_start(sit_u, _http_seq_heartbeat_cb, HEARTBEAT_TIMEOUT, 0);
  htd_u->fig_u.sit_u = sit_u;

  {
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now = u3_time_in_tv(&tim_u);
    htd_u->sev_l = u3r_mug(now);
    u3z(now);
  }

  //  XX retry up to N?
  //
  // car_u->ev.bail_f = ...;

  return car_u;
}
