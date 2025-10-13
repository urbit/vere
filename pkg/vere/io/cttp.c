/// @file

#include "vere.h"

#include "h2o.h"
#include "h2o/websocket.h"
#include "noun.h"
#include "openssl/ssl.h"
#include "openssl/sha.h"
#include "wslay/wslay.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#define U3_WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* u3_csat: client connection state.
*/
  typedef enum {
    u3_csat_init = 0,                   //  initialized
    u3_csat_addr = 1,                   //  address resolution begun
    u3_csat_quit = 2,                   //  cancellation requested
    u3_csat_conn = 3,                   //  sync connect phase
    u3_csat_ripe = 4                    //  passed to libh2o
  } u3_csat;

/* u3_cres: response to http client.
*/
  typedef struct _u3_cres {
    c3_w             sas_w;             //  status code
    u3_noun          hed;               //  headers
    u3_hbod*         bod_u;             //  exit of body queue
    u3_hbod*         dob_u;             //  entry of body queue
  } u3_cres;

typedef enum {
  u3_cws_pending = 0,
  u3_cws_open    = 1,
  u3_cws_closing = 2,
  u3_cws_closed  = 3
} u3_cwsat;

typedef struct _u3_cttp u3_cttp;
typedef struct _u3_cws u3_cws;

/* u3_creq: outgoing http request.
*/
  typedef struct _u3_creq {             //  client request
    c3_l               num_l;           //  request number
    h2o_http1client_t* cli_u;           //  h2o client
    u3_csat            sat_e;           //  connection state
    c3_o               sec;             //  yes == https
    c3_w               ipf_w;           //  IP
    c3_c*              ipf_c;           //  IP (string)
    c3_c*              hot_c;           //  host
    c3_s               por_s;           //  port
    c3_c*              por_c;           //  port (string)
    c3_c*              met_c;           //  method
    c3_c*              url_c;           //  url
    u3_hhed*           hed_u;           //  headers
    u3_hbod*           bod_u;           //  body
    u3_hbod*           rub_u;           //  exit of send queue
    u3_hbod*           bur_u;           //  entry of send queue
    h2o_iovec_t*       vec_u;           //  send-buffer array
    u3_cres*           res_u;           //  nascent response
    u3_cws*            wsu_u;           //  websocket session (optional)
    struct _u3_creq*   nex_u;           //  next in list
    struct _u3_creq*   pre_u;           //  previous in list
    struct _u3_cttp*   ctp_u;           //  cttp backpointer
  } u3_creq;

struct _u3_cws {
  c3_l                 wid_l;          // websocket id
  u3_cwsat             sat_e;          // websocket state
  c3_o                 sec;            // secure (wss)
  c3_c*                hot_c;          // host string (nullable)
  c3_w                 ipf_w;          // ipv4 (numeric)
  c3_c*                ipf_c;          // ipv4 string
  c3_s                 por_s;          // port (numeric)
  c3_c*                por_c;          // port string
  c3_c*                url_c;          // request url/path
  c3_c                 key_c[29];      // sec-websocket-key
  u3_cttp*             ctp_u;          // backpointer
  u3_creq*             ceq_u;          // pending handshake request
  h2o_socket_t*        sok_u;          // underlying socket
  wslay_event_context_ptr wsl_w;       // wslay context
  struct wslay_event_callbacks wcb_u;  // wslay callbacks
  c3_y*                out_y;          // pending write buffer
  struct _u3_cws*      nex_u;          // next in list
  struct _u3_cws*      pre_u;          // prev in list
};

static void _cttp_ws_close(u3_cws* cws_u, c3_o send_event);
static void _cttp_ws_proceed(u3_cws* cws_u);
static c3_o _cttp_ws_start(u3_cttp* ctp_u, c3_l wid_l, u3_atom url);
static c3_o _cttp_ws_send_message(u3_cws* cws_u, u3_noun msg);
static void _cttp_ws_queue_close(u3_cws* cws_u);
static void _cttp_ws_fail_handshake(u3_creq* ceq_u, const c3_c* err_c);
static void _cttp_ws_generate_key(u3_cws* cws_u);
static void _cttp_ws_compute_accept(const c3_c* key_c, c3_c out_c[29]);
static void _cttp_ws_read_cb(h2o_socket_t* sock_u, const c3_c* err_c);
static void _cttp_ws_write_cb(h2o_socket_t* sock_u, const c3_c* err_c);
static ssize_t _cttp_ws_recv_cb(wslay_event_context_ptr ctx, uint8_t* buf_y, size_t len_w, int flags, void* ves_p);
static ssize_t _cttp_ws_send_cb(wslay_event_context_ptr ctx, const uint8_t* dat_y, size_t len_w, int flags, void* ves_p);
static void _cttp_ws_msg_cb(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg* arg, void* ves_p);
static int _cttp_ws_genmask_cb(wslay_event_context_ptr ctx, uint8_t* buf_y, size_t len_w, void* ves_p);
static c3_c* _cttp_ws_origin(u3_atom nor);

static void _cttp_creq_start(u3_creq* ceq_u);
static u3_atom _cttp_ws_normalize_url(u3_atom url);

/* u3_cttp: http client.
*/
  typedef struct _u3_cttp {
    u3_auto          car_u;             //  driver
    c3_l             sev_l;             //  instance number
    u3_creq*         ceq_u;             //  request list
    u3_cws*          cws_u;             //  websocket sessions
    uv_async_t       nop_u;             //  unused handle (async close)
    h2o_timeout_t    tim_u;             //  request timeout
    h2o_http1client_ctx_t               //
                     ctx_u;             //  h2o client ctx
    void*            tls_u;             //  client SSL_CTX*
  } u3_cttp;

// XX deduplicate with _http_vec_to_atom
/* _cttp_vec_to_atom(): convert h2o_iovec_t to atom (cord)
*/
static u3_noun
_cttp_vec_to_atom(h2o_iovec_t vec_u)
{
  return u3i_bytes(vec_u.len, (const c3_y*)vec_u.base);
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

/* _cttp_bod_new(): create a data buffer
*/
static u3_hbod*
_cttp_bod_new(c3_w len_w, c3_c* hun_c)
{
  u3_hbod* bod_u = c3_malloc(1 + len_w + sizeof(*bod_u));
  bod_u->hun_y[len_w] = 0;
  bod_u->len_w = len_w;
  memcpy(bod_u->hun_y, (const c3_y*)hun_c, len_w);

  bod_u->nex_u = 0;
  return bod_u;
}

/* _cttp_bod_from_hed(): create a data buffer from a header
*/
static u3_hbod*
_cttp_bod_from_hed(u3_hhed* hed_u)
{
  c3_w len_w     = hed_u->nam_w + 2 + hed_u->val_w + 2;
  u3_hbod* bod_u = c3_malloc(1 + len_w + sizeof(*bod_u));
  bod_u->hun_y[len_w] = 0;

  memcpy(bod_u->hun_y, hed_u->nam_c, hed_u->nam_w);
  memcpy(bod_u->hun_y + hed_u->nam_w, ": ", 2);
  memcpy(bod_u->hun_y + hed_u->nam_w + 2, hed_u->val_c, hed_u->val_w);
  memcpy(bod_u->hun_y + hed_u->nam_w + 2 + hed_u->val_w, "\r\n", 2);

  bod_u->len_w = len_w;
  bod_u->nex_u = 0;

  return bod_u;
}

/* _cttp_bods_to_octs: translate body buffer into octet-stream noun.
*/
static u3_noun
_cttp_bods_to_octs(u3_hbod* bod_u)
{
  c3_w    len_w;
  c3_y*   buf_y;
  u3_noun cos;

  {
    u3_hbod* bid_u = bod_u;

    len_w = 0;
    while ( bid_u ) {
      len_w += bid_u->len_w;
      bid_u = bid_u->nex_u;
    }
  }
  buf_y = c3_malloc(1 + len_w);
  buf_y[len_w] = 0;

  {
    c3_y* ptr_y = buf_y;

    while ( bod_u ) {
      memcpy(ptr_y, bod_u->hun_y, bod_u->len_w);
      ptr_y += bod_u->len_w;
      bod_u = bod_u->nex_u;
    }
  }
  cos = u3i_bytes(len_w, buf_y);
  c3_free(buf_y);
  return u3nc(len_w, cos);
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

  if ( 0 == len_w ) {
    *tot_w = len_w;
    return 0;
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

/* websocket session bookkeeping helpers
*/
static void
_cttp_ws_link(u3_cttp* ctp_u, u3_cws* cws_u)
{
  cws_u->ctp_u = ctp_u;
  cws_u->nex_u = ctp_u->cws_u;
  cws_u->pre_u = 0;

  if ( 0 != ctp_u->cws_u ) {
    ctp_u->cws_u->pre_u = cws_u;
  }

  ctp_u->cws_u = cws_u;
}

static void
_cttp_ws_unlink(u3_cws* cws_u)
{
  u3_cttp* ctp_u = cws_u->ctp_u;

  if ( cws_u->pre_u ) {
    cws_u->pre_u->nex_u = cws_u->nex_u;
    if ( 0 != cws_u->nex_u ) {
      cws_u->nex_u->pre_u = cws_u->pre_u;
    }
  }
  else if ( ctp_u->cws_u == cws_u ) {
    ctp_u->cws_u = cws_u->nex_u;
    if ( 0 != cws_u->nex_u ) {
      cws_u->nex_u->pre_u = 0;
    }
  }
}

static u3_cws*
_cttp_ws_find(u3_cttp* ctp_u, c3_l wid_l)
{
  u3_cws* cws_u = ctp_u->cws_u;

  while ( cws_u ) {
    if ( wid_l == cws_u->wid_l ) {
      return cws_u;
    }
    cws_u = cws_u->nex_u;
  }

  return 0;
}

static void
_cttp_ws_plan_event(u3_cws* cws_u, u3_noun event)
{
  u3_cttp* ctp_u = cws_u->ctp_u;
  c3_l wid_l = cws_u->wid_l;
  u3_noun typ = u3h(event);
  if ( c3y == u3a_is_cat(typ) ) {
    c3_c* nam_c = u3r_string(u3k(typ));
    u3l_log("cttp: ws plan wid=%u typ=%s", wid_l, nam_c);
    c3_free(nam_c);
  }
  else {
    u3l_log("cttp: ws plan wid=%u", wid_l);
  }
  u3_noun wir = u3nt(u3i_string("http-client"),
                     u3dc("scot", c3__uv, ctp_u->sev_l),
                     u3_nul);
  u3_noun pay = u3nc(u3i_chub((c3_d)cws_u->wid_l), event);
  u3_noun cad = u3nc(u3i_string("websocket-event"), pay);

  u3_auto_plan(&ctp_u->car_u, u3_ovum_init(0, c3__i, wir, cad));
}

static void
_cttp_base64_encode(c3_c* dst_c, const uint8_t* src_y, size_t len_w)
{
  static const char tab_c[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i_w = 0;
  size_t o_w = 0;

  while ( len_w >= 3 ) {
    dst_c[o_w++] = tab_c[(src_y[i_w] >> 2) & 0x3F];
    dst_c[o_w++] = tab_c[((src_y[i_w] & 0x3) << 4) | (src_y[i_w + 1] >> 4)];
    dst_c[o_w++] = tab_c[((src_y[i_w + 1] & 0xF) << 2) | (src_y[i_w + 2] >> 6)];
    dst_c[o_w++] = tab_c[src_y[i_w + 2] & 0x3F];
    i_w += 3;
    len_w -= 3;
  }

  if ( len_w > 0 ) {
    dst_c[o_w++] = tab_c[(src_y[i_w] >> 2) & 0x3F];
    if ( 1 == len_w ) {
      dst_c[o_w++] = tab_c[(src_y[i_w] & 0x3) << 4];
      dst_c[o_w++] = '=';
      dst_c[o_w++] = '=';
    }
    else { // len_w == 2
      dst_c[o_w++] = tab_c[((src_y[i_w] & 0x3) << 4) | (src_y[i_w + 1] >> 4)];
      dst_c[o_w++] = tab_c[(src_y[i_w + 1] & 0xF) << 2];
      dst_c[o_w++] = '=';
    }
  }

  dst_c[o_w] = 0;
}

static void
_cttp_ws_generate_key(u3_cws* cws_u)
{
  c3_w rad_w[16];
  c3_y raw_y[16];

  c3_rand(rad_w);
  memcpy(raw_y, rad_w, sizeof(raw_y));

  _cttp_base64_encode(cws_u->key_c, raw_y, sizeof(raw_y));
}

static void
_cttp_ws_compute_accept(const c3_c* key_c, c3_c out_c[29])
{
  c3_y sha_y[20];
  c3_c buf_c[24 + sizeof(U3_WS_GUID)];

  memcpy(buf_c, key_c, 24);
  memcpy(buf_c + 24, U3_WS_GUID, sizeof(U3_WS_GUID) - 1);

  SHA1((const unsigned char*)buf_c, 24 + (sizeof(U3_WS_GUID) - 1), sha_y);
  _cttp_base64_encode(out_c, sha_y, sizeof(sha_y));
}

static c3_o
_cttp_header_is(const h2o_header_t* hed_u, const c3_c* name_c)
{
  size_t len = strlen(name_c);

  if ( hed_u->name->len != len ) {
    return c3n;
  }

  for ( size_t i = 0; i < len; i++ ) {
    if ( tolower((unsigned char)hed_u->name->base[i]) != tolower((unsigned char)name_c[i]) ) {
      return c3n;
    }
  }

  return c3y;
}

static h2o_iovec_t*
_cttp_find_header(h2o_header_t* hed_u, size_t hed_t, const c3_c* name_c)
{
  for ( size_t i = 0; i < hed_t; i++ ) {
    if ( c3y == _cttp_header_is(&hed_u[i], name_c) ) {
      return &hed_u[i].value;
    }
  }

  return 0;
}

static c3_c*
_cttp_ws_origin(u3_atom nor)
{
  c3_c* url_c = u3r_string(nor);
  c3_c* scheme_c = strstr(url_c, "://");
  c3_c* start_c = scheme_c ? scheme_c + 3 : url_c;
  c3_c* end_c = strchr(start_c, '/');
  size_t len_w = end_c ? (size_t)(end_c - url_c) : strlen(url_c);

  c3_c* ori_c = c3_malloc(len_w + 1);
  memcpy(ori_c, url_c, len_w);
  ori_c[len_w] = 0;

  c3_free(url_c);
  return ori_c;
}

static u3_atom
_cttp_ws_normalize_url(u3_atom url)
{
  c3_c* url_c = u3r_string(url);
  size_t len_w = strlen(url_c);
  u3_atom ret;

  if ( len_w >= 5 && 0 == strncmp(url_c, "ws://", 5) ) {
    size_t new_len = len_w + 2;
    c3_c* rew_c = c3_malloc(new_len + 1);
    memcpy(rew_c, "http://", 7);
    memcpy(rew_c + 7, url_c + 5, len_w - 5);
    rew_c[new_len] = '\0';
    ret = u3i_string(rew_c);
    c3_free(rew_c);
  }
  else if ( len_w >= 6 && 0 == strncmp(url_c, "wss://", 6) ) {
    size_t new_len = len_w + 2;
    c3_c* rew_c = c3_malloc(new_len + 1);
    memcpy(rew_c, "https://", 8);
    memcpy(rew_c + 8, url_c + 6, len_w - 6);
    rew_c[new_len] = '\0';
    ret = u3i_string(rew_c);
    c3_free(rew_c);
  }
  else {
    ret = u3k(url);
  }

  c3_free(url_c);
  return ret;
}

// XX deduplicate with _http_heds_free
/* _cttp_heds_free(): free header linked list
*/
static void
_cttp_heds_free(u3_hhed* hed_u)
{
  while ( hed_u ) {
    u3_hhed* nex_u = hed_u->nex_u;

    c3_free(hed_u->nam_c);
    c3_free(hed_u->val_c);
    c3_free(hed_u);
    hed_u = nex_u;
  }
}

// XX deduplicate with _http_hed_new
/* _cttp_hed_new(): create u3_hhed from nam/val cords
*/
static u3_hhed*
_cttp_hed_new(u3_atom nam, u3_atom val)
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

static void
_cttp_hed_push(u3_hhed** list_u, const c3_c* nam_c, const c3_c* val_c)
{
  u3_hhed* hed_u = _cttp_hed_new(u3i_string(nam_c), u3i_string(val_c));
  hed_u->nex_u = *list_u;
  *list_u = hed_u;
}

// XX deduplicate with _http_heds_from_noun
/* _cttp_heds_from_noun(): convert (list (pair @t @t)) to u3_hhed
*/
static u3_hhed*
_cttp_heds_from_noun(u3_noun hed)
{
  u3_noun deh = hed;
  u3_noun i_hed;

  u3_hhed* hed_u = 0;

  while ( u3_nul != hed ) {
    i_hed = u3h(hed);
    u3_hhed* nex_u = _cttp_hed_new(u3h(i_hed), u3t(i_hed));
    nex_u->nex_u = hed_u;

    hed_u = nex_u;
    hed = u3t(hed);
  }

  u3z(deh);
  return hed_u;
}

// XX deduplicate with _http_heds_to_noun
/* _cttp_heds_to_noun(): convert h2o_header_t to (list (pair @t @t))
*/
static u3_noun
_cttp_heds_to_noun(h2o_header_t* hed_u, c3_d hed_d)
{
  u3_noun hed = u3_nul;
  c3_d dex_d  = hed_d;

  h2o_header_t deh_u;

  while ( 0 < dex_d ) {
    deh_u = hed_u[--dex_d];
    hed = u3nc(u3nc(_cttp_vec_to_atom(*deh_u.name),
                    _cttp_vec_to_atom(deh_u.value)), hed);
  }

  return hed;
}

/* _cttp_cres_free(): free a u3_cres.
*/
static void
_cttp_cres_free(u3_cres* res_u)
{
  _cttp_bods_free(res_u->bod_u);
  c3_free(res_u);
}

/* _cttp_cres_new(): create a response
*/
static void
_cttp_cres_new(u3_creq* ceq_u, c3_w sas_w)
{
  ceq_u->res_u = c3_calloc(sizeof(*ceq_u->res_u));
  ceq_u->res_u->sas_w = sas_w;
}

/* _cttp_cres_fire_body(): attach response body buffer
*/
static void
_cttp_cres_fire_body(u3_cres* res_u, u3_hbod* bod_u)
{
  u3_assert(!bod_u->nex_u);

  if ( !(res_u->bod_u) ) {
    res_u->bod_u = res_u->dob_u = bod_u;
  }
  else {
    res_u->dob_u->nex_u = bod_u;
    res_u->dob_u = bod_u;
  }
}

/* _cttp_mcut_pork(): measure/cut path/extension.
*/
static c3_w
_cttp_mcut_pork(c3_c* buf_c, c3_w len_w, u3_noun pok)
{
  u3_noun h_pok = u3h(pok);
  u3_noun t_pok = u3t(pok);

  len_w = u3_mcut_path(buf_c, len_w, '/', u3k(t_pok));
  if ( u3_nul != h_pok ) {
    len_w = u3_mcut_char(buf_c, len_w, '.');
    len_w = u3_mcut_cord(buf_c, len_w, u3k(u3t(h_pok)));
  }
  u3z(pok);
  return len_w;
}

/* _cttp_mcut_quay(): measure/cut query.
*/
static c3_w
_cttp_mcut_quay(c3_c* buf_c, c3_w len_w, u3_noun quy)
{
  u3_noun yuq = quy;
  c3_o  fir_o = c3y;

  while ( u3_nul != quy ) {
    if ( c3y == fir_o ) {
      len_w = u3_mcut_char(buf_c, len_w, '?');
      fir_o = c3n;
    }
    else {
      len_w = u3_mcut_char(buf_c, len_w, '&');
    }

    {
      u3_noun i_quy, t_quy;
      u3_noun pi_quy, qi_quy;
      u3x_cell(quy, &i_quy, &t_quy);
      u3x_cell(i_quy, &pi_quy, &qi_quy);

      len_w = u3_mcut_cord(buf_c, len_w, u3k(pi_quy));
      len_w = u3_mcut_char(buf_c, len_w, '=');
      len_w = u3_mcut_cord(buf_c, len_w, u3k(qi_quy));

      quy = t_quy;
    }
  }

  u3z(yuq);
  return len_w;
}

/* _cttp_mcut_url(): measure/cut purl, producing relative URL.
*/
static c3_w
_cttp_mcut_url(c3_c* buf_c, c3_w len_w, u3_noun pul)
{
  u3_noun q_pul = u3h(u3t(pul));
  u3_noun r_pul = u3t(u3t(pul));

  len_w = u3_mcut_char(buf_c, len_w, '/');
  len_w = _cttp_mcut_pork(buf_c, len_w, u3k(q_pul));

  if ( u3_nul != r_pul ) {
    len_w = _cttp_mcut_quay(buf_c, len_w, u3k(r_pul));
  }
  u3z(pul);
  return len_w;
}

/* _cttp_creq_port(): stringify port
*/
static c3_c*
_cttp_creq_port(c3_s por_s)
{
  c3_c* por_c = c3_malloc(8);
  snprintf(por_c, 7, "%d", 0xffff & por_s);
  return por_c;
}

/* _cttp_creq_url(): construct url from noun.
*/
static c3_c*
_cttp_creq_url(u3_noun pul)
{
  c3_w  len_w = _cttp_mcut_url(0, 0, u3k(pul));
  c3_c* url_c = c3_malloc(1 + len_w);

  _cttp_mcut_url(url_c, 0, pul);
  url_c[len_w] = 0;

  return url_c;
}

/* _cttp_creq_host(): construct host from noun.
*/
static c3_c*
_cttp_creq_host(u3_noun hot)
{
  c3_w  len_w = u3_mcut_host(0, 0, u3k(hot));
  c3_c* hot_c = c3_malloc(1 + len_w);

  u3_mcut_host(hot_c, 0, hot);
  hot_c[len_w] = 0;

  return hot_c;
}

/* _cttp_creq_ip(): stringify ip
*/
static c3_c*
_cttp_creq_ip(c3_w ipf_w)
{
  c3_c* ipf_c = c3_malloc(17);
  snprintf(ipf_c, 16, "%d.%d.%d.%d", (ipf_w >> 24),
                                     ((ipf_w >> 16) & 255),
                                     ((ipf_w >> 8) & 255),
                                     (ipf_w & 255));
  return ipf_c;
}

/* _cttp_creq_find(): find a request by number in the client
*/
static u3_creq*
_cttp_creq_find(u3_cttp* ctp_u, c3_l num_l)
{
  u3_creq* ceq_u = ctp_u->ceq_u;

  //  XX glories of linear search
  //
  while ( ceq_u ) {
    if ( num_l == ceq_u->num_l ) {
      return ceq_u;
    }
    ceq_u = ceq_u->nex_u;
  }
  return 0;
}

/* _cttp_creq_link(): link request to client
*/
static void
_cttp_creq_link(u3_cttp* ctp_u, u3_creq* ceq_u)
{
  ceq_u->nex_u = ctp_u->ceq_u;

  if ( 0 != ceq_u->nex_u ) {
    ceq_u->nex_u->pre_u = ceq_u;
  }

  ceq_u->ctp_u = ctp_u;
  ctp_u->ceq_u = ceq_u;
}

/* _cttp_creq_unlink(): unlink request from client
*/
static void
_cttp_creq_unlink(u3_creq* ceq_u)
{
  u3_cttp* ctp_u = ceq_u->ctp_u;

  if ( ceq_u->pre_u ) {
    ceq_u->pre_u->nex_u = ceq_u->nex_u;

    if ( 0 != ceq_u->nex_u ) {
      ceq_u->nex_u->pre_u = ceq_u->pre_u;
    }
  }
  else {
    ctp_u->ceq_u = ceq_u->nex_u;

    if ( 0 != ceq_u->nex_u ) {
      ceq_u->nex_u->pre_u = 0;
    }
  }
}

/* _cttp_creq_free(): free a u3_creq.
*/
static void
_cttp_creq_free(u3_creq* ceq_u)
{
  ceq_u->wsu_u = 0;
  _cttp_creq_unlink(ceq_u);

  _cttp_heds_free(ceq_u->hed_u);
  // Note: ceq_u->bod_u is covered here
  _cttp_bods_free(ceq_u->rub_u);

  if ( ceq_u->res_u ) {
    _cttp_cres_free(ceq_u->res_u);
  }

  c3_free(ceq_u->hot_c);
  c3_free(ceq_u->ipf_c);
  c3_free(ceq_u->por_c);
  c3_free(ceq_u->met_c);
  c3_free(ceq_u->url_c);
  c3_free(ceq_u->vec_u);
  c3_free(ceq_u);
}

/* _cttp_creq_new(): create a u3_creq from an +http-request
 *
 *   If we were rewriting all of this from scratch, this isn't how we'd do it.
 *
 *   We start with the (?? - JB)
 */
static u3_creq*
_cttp_creq_new(u3_cttp* ctp_u, c3_l num_l, u3_noun hes)
{
  u3_creq* ceq_u = c3_calloc(sizeof(*ceq_u));

  u3_noun method, url, headers, body;
  if (c3n == u3r_qual(hes, &method, &url, &headers, &body)) {
    u3z(hes);
    return 0;
  }

  //  parse the url out of the new style url passed to us.
  //
  u3_noun unit_pul = u3do("de-purl:html", u3k(url));

  if ( c3n == u3du(unit_pul) ) {
    c3_c* url_c = u3r_string(url);
    u3l_log("cttp: unable to parse url:\n    %s", url_c);
    c3_free(url_c);
    u3z(hes);
    return 0;
  }

  u3_noun pul = u3t(unit_pul);

  u3_noun hat = u3h(pul);      // +hart
  u3_noun sec = u3h(hat);
  u3_noun por = u3h(u3t(hat));
  u3_noun hot = u3t(u3t(hat)); // +host

  ceq_u->sat_e = u3_csat_init;
  ceq_u->num_l = num_l;
  ceq_u->sec   = sec;

  if ( c3y == u3h(hot) ) {
    ceq_u->hot_c = _cttp_creq_host(u3k(u3t(hot)));
  } else {
    ceq_u->ipf_w = u3r_word(0, u3t(hot));
    ceq_u->ipf_c = _cttp_creq_ip(ceq_u->ipf_w);
  }

  if ( u3_nul != por ) {
    ceq_u->por_s = u3t(por);
    ceq_u->por_c = _cttp_creq_port(ceq_u->por_s);
  }

  //  XX this should be checked against a whitelist
  //
  u3_assert( c3y == u3ud(method) );
  ceq_u->met_c = u3r_string(method);
  ceq_u->url_c = _cttp_creq_url(u3k(pul));

  ceq_u->hed_u = _cttp_heds_from_noun(u3k(headers));

  if ( u3_nul != body ) {
    ceq_u->bod_u = _cttp_bod_from_octs(u3k(u3t(body)));
  }

  _cttp_creq_link(ctp_u, ceq_u);

  u3z(unit_pul);
  u3z(hes);

  return ceq_u;
}

/* _cttp_creq_fire_body(): attach body to request buffers.
*/
static void
_cttp_creq_fire_body(u3_creq* ceq_u, u3_hbod *rub_u)
{
  u3_assert(!rub_u->nex_u);

  if ( !(ceq_u->rub_u) ) {
    ceq_u->rub_u = ceq_u->bur_u = rub_u;
  }
  else {
    ceq_u->bur_u->nex_u = rub_u;
    ceq_u->bur_u = rub_u;
  }
}

/* _cttp_creq_fire_str(): attach string to request buffers.
*/
static void
_cttp_creq_fire_str(u3_creq* ceq_u, c3_c* str_c)
{
  _cttp_creq_fire_body(ceq_u, _cttp_bod_new(strlen(str_c), str_c));
  c3_free(str_c);
}

/* _cttp_creq_fire_heds(): attach output headers.
*/
static void
_cttp_creq_fire_heds(u3_creq* ceq_u, u3_hhed* hed_u)
{
  while ( hed_u ) {
    _cttp_creq_fire_body(ceq_u, _cttp_bod_from_hed(hed_u));
    hed_u = hed_u->nex_u;
  }
}

/* _cttp_creq_fire(): load request data for into buffers.
*/
static void
_cttp_creq_fire(u3_creq* ceq_u)
{
  {
    c3_w  len_w = strlen(ceq_u->met_c) + 1 + strlen(ceq_u->url_c) + 12;
    c3_c* lin_c = c3_malloc(len_w);

    len_w = snprintf(lin_c, len_w, "%s %s HTTP/1.1\r\n",
                                   ceq_u->met_c,
                                   ceq_u->url_c);
    _cttp_creq_fire_str(ceq_u, lin_c);
  }

  {
    c3_c* hot_c = ceq_u->hot_c ? ceq_u->hot_c : ceq_u->ipf_c;
    c3_c* hos_c;
    c3_w  len_w;

    if ( ceq_u->por_c ) {
      len_w = 6 + strlen(hot_c) + 1 + strlen(ceq_u->por_c) + 3;
      hos_c = c3_malloc(len_w);
      len_w = snprintf(hos_c, len_w, "Host: %s:%s\r\n", hot_c, ceq_u->por_c);
    }
    else {
      len_w = 6 + strlen(hot_c) + 3;
      hos_c = c3_malloc(len_w);
      len_w = snprintf(hos_c, len_w, "Host: %s\r\n", hot_c);
    }

    _cttp_creq_fire_body(ceq_u, _cttp_bod_new(len_w, hos_c));
    c3_free(hos_c);
  }

  _cttp_creq_fire_heds(ceq_u, ceq_u->hed_u);

  if ( !ceq_u->bod_u ) {
    _cttp_creq_fire_body(ceq_u, _cttp_bod_new(2, "\r\n"));
  }
  else {
    c3_c len_c[41];
    c3_w len_w = snprintf(len_c, 40, "Content-Length: %u\r\n\r\n",
                                     ceq_u->bod_u->len_w);

    _cttp_creq_fire_body(ceq_u, _cttp_bod_new(len_w, len_c));
    _cttp_creq_fire_body(ceq_u, ceq_u->bod_u);
  }
}

static void
_cttp_ws_close(u3_cws* cws_u, c3_o send_event)
{
  if ( !cws_u ) {
    return;
  }

  if ( u3_cws_closed != cws_u->sat_e ) {
    u3l_log("cttp: ws close wid=%u send=%c", cws_u->wid_l, (c3y==send_event?'y':'n'));
    cws_u->sat_e = u3_cws_closed;

    if ( c3y == send_event ) {
      _cttp_ws_plan_event(cws_u, u3nc(u3i_string("disconnect"), u3_nul));
      send_event = c3n;
    }

    if ( cws_u->wsl_w ) {
      wslay_event_context_free(cws_u->wsl_w);
      cws_u->wsl_w = 0;
    }

    if ( cws_u->sok_u ) {
      h2o_socket_t* sok_u = cws_u->sok_u;
      cws_u->sok_u = 0;
      h2o_socket_close(sok_u);
    }
  }
  else if ( c3y == send_event ) {
    _cttp_ws_plan_event(cws_u, u3nc(u3i_string("disconnect"), u3_nul));
    send_event = c3n;
  }

  if ( cws_u->out_y ) {
    c3_free(cws_u->out_y);
    cws_u->out_y = 0;
  }

  _cttp_ws_unlink(cws_u);
  cws_u->ctp_u = 0;

  c3_free(cws_u->hot_c);
  c3_free(cws_u->ipf_c);
  c3_free(cws_u->por_c);
  c3_free(cws_u->url_c);

  c3_free(cws_u);
}

static ssize_t
_cttp_ws_recv_cb(wslay_event_context_ptr ctx,
                 uint8_t* buf_y,
                 size_t len_w,
                 int flags,
                 void* ves_p)
{
  u3_cws* cws_u = ves_p;

  u3l_log("cttp: ws recv_cb wid=%u want=%zu have=%zu",
          cws_u->wid_l,
          len_w,
          (size_t)cws_u->sok_u->input->size);

  if ( 0 == cws_u->sok_u->input->size ) {
    wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
    u3l_log("cttp: ws recv_cb wid=%u empty", cws_u->wid_l);
    return -1;
  }

  if ( cws_u->sok_u->input->size < len_w ) {
    len_w = cws_u->sok_u->input->size;
  }

  memcpy(buf_y, cws_u->sok_u->input->bytes, len_w);
  h2o_buffer_consume(&cws_u->sok_u->input, len_w);

  return (ssize_t)len_w;
}

static ssize_t
_cttp_ws_send_cb(wslay_event_context_ptr ctx,
                 const uint8_t* dat_y,
                 size_t len_w,
                 int flags,
                 void* ves_p)
{
  u3_cws* cws_u = ves_p;

  u3l_log("cttp: ws send_cb wid=%u len=%zu writing=%c",
          cws_u->wid_l,
          len_w,
          h2o_socket_is_writing(cws_u->sok_u) ? 'y' : 'n');

  if ( h2o_socket_is_writing(cws_u->sok_u) ) {
    wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
    u3l_log("cttp: ws send_cb wid=%u busy", cws_u->wid_l);
    return -1;
  }

  if ( 0 == len_w ) {
    return 0;
  }

  cws_u->out_y = c3_malloc(len_w);
  memcpy(cws_u->out_y, dat_y, len_w);

  h2o_iovec_t buf_u = h2o_iovec_init((char*)cws_u->out_y, len_w);
  h2o_socket_write(cws_u->sok_u, &buf_u, 1, _cttp_ws_write_cb);

  return (ssize_t)len_w;
}

static void
_cttp_ws_msg_cb(wslay_event_context_ptr ctx,
                const struct wslay_event_on_msg_recv_arg* arg,
                void* ves_p)
{
  u3_cws* cws_u = ves_p;

  if ( 0 == arg ) {
    u3l_log("cttp: ws msg close wid=%u", cws_u->wid_l);
    _cttp_ws_close(cws_u, c3y);
    return;
  }

  if ( WSLAY_CONNECTION_CLOSE == arg->opcode ) {
    u3l_log("cttp: ws msg opcode close wid=%u", cws_u->wid_l);
    _cttp_ws_close(cws_u, c3y);
    return;
  }

  u3l_log("cttp: ws msg opcode=%u len=%zu", arg->opcode, arg->msg_length);

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

  _cttp_ws_plan_event(cws_u, event);
}

static int
_cttp_ws_genmask_cb(wslay_event_context_ptr ctx,
                    uint8_t* buf_y,
                    size_t len_w,
                    void* ves_p)
{
  (void)ctx;
  (void)ves_p;

  size_t off_w = 0;

  while ( off_w < len_w ) {
    c3_w rad_w[16];
    size_t rem_w = len_w - off_w;
    size_t chc_w = c3_min(rem_w, sizeof(rad_w));

    c3_rand(rad_w);
    memcpy(buf_y + off_w, (const uint8_t*)rad_w, chc_w);
    off_w += chc_w;
  }

  return 0;
}

static void
_cttp_ws_proceed(u3_cws* cws_u)
{
  if ( 0 == cws_u->sok_u ) {
    return;
  }

  while ( 1 ) {
    c3_o handled = c3n;

    if ( !h2o_socket_is_writing(cws_u->sok_u) &&
         wslay_event_want_write(cws_u->wsl_w) )
    {
      int sas_i = wslay_event_send(cws_u->wsl_w);

      if ( 0 == sas_i ) {
        handled = c3y;
      }
      else {
        u3l_log("cttp: ws send err wid=%u code=%d want-read=%c want-write=%c",
                cws_u->wid_l,
                sas_i,
                wslay_event_want_read(cws_u->wsl_w) ? 'y' : 'n',
                wslay_event_want_write(cws_u->wsl_w) ? 'y' : 'n');

        if ( WSLAY_ERR_WOULDBLOCK != sas_i ) {
          //  any other error is fatal to the websocket session
          _cttp_ws_close(cws_u, c3y);
          return;
        }
      }
    }

    if ( cws_u->sok_u->input->size && wslay_event_want_read(cws_u->wsl_w) ) {
      int ras_i = wslay_event_recv(cws_u->wsl_w);

      if ( 0 == ras_i ) {
        handled = c3y;
      }
      else {
        u3l_log("cttp: ws recv err wid=%u code=%d want-read=%c want-write=%c input=%zu",
                cws_u->wid_l,
                ras_i,
                wslay_event_want_read(cws_u->wsl_w) ? 'y' : 'n',
                wslay_event_want_write(cws_u->wsl_w) ? 'y' : 'n',
                (size_t)cws_u->sok_u->input->size);

        if ( WSLAY_ERR_WOULDBLOCK != ras_i ) {
          //  wslay reports anything else as a hard protocol failure
          _cttp_ws_close(cws_u, c3y);
          return;
        }
      }
    }

    if ( c3n == handled ) {
      break;
    }
  }

  if ( wslay_event_want_read(cws_u->wsl_w) ) {
    u3l_log("cttp: ws proceed wid=%u want-read", cws_u->wid_l);
    h2o_socket_read_start(cws_u->sok_u, _cttp_ws_read_cb);
  }
  else if ( h2o_socket_is_writing(cws_u->sok_u) ||
            wslay_event_want_write(cws_u->wsl_w) )
  {
    u3l_log("cttp: ws proceed wid=%u write-pending", cws_u->wid_l);
    h2o_socket_read_stop(cws_u->sok_u);
  }
  else {
    u3l_log("cttp: ws proceed wid=%u idle", cws_u->wid_l);
    h2o_socket_read_start(cws_u->sok_u, _cttp_ws_read_cb);
  }
}

static void
_cttp_ws_read_cb(h2o_socket_t* sok_u, const c3_c* err_c)
{
  u3_cws* cws_u = sok_u->data;

  if ( err_c ) {
    u3l_log("cttp: ws read err %s", err_c);
    _cttp_ws_close(cws_u, c3y);
    return;
  }

  u3l_log("cttp: ws read wid=%u size=%zu", cws_u->wid_l, sok_u->input->size);

  _cttp_ws_proceed(cws_u);
}

static void
_cttp_ws_write_cb(h2o_socket_t* sok_u, const c3_c* err_c)
{
  u3_cws* cws_u = sok_u->data;

  if ( cws_u->out_y ) {
    c3_free(cws_u->out_y);
    cws_u->out_y = 0;
  }

  if ( err_c ) {
    u3l_log("cttp: ws write err %s", err_c);
    _cttp_ws_close(cws_u, c3y);
    return;
  }

  u3l_log("cttp: ws write done wid=%u", cws_u->wid_l);
  _cttp_ws_proceed(cws_u);
}

static void
_cttp_ws_queue_close(u3_cws* cws_u)
{
  if ( (u3_cws_open != cws_u->sat_e) || (0 == cws_u->wsl_w) ) {
    u3l_log("cttp: ws queue close wid=%u direct", cws_u->wid_l);
    _cttp_ws_close(cws_u, c3y);
    return;
  }

  if ( u3_cws_closing == cws_u->sat_e ) {
    u3l_log("cttp: ws queue close wid=%u already", cws_u->wid_l);
    return;
  }

  u3l_log("cttp: ws queue close wid=%u normal", cws_u->wid_l);
  cws_u->sat_e = u3_cws_closing;
  wslay_event_queue_close(cws_u->wsl_w, WSLAY_CODE_NORMAL_CLOSURE, NULL, 0);
  _cttp_ws_proceed(cws_u);
}

static c3_o
_cttp_ws_send_message(u3_cws* cws_u, u3_noun msg)
{
  if ( (u3_cws_open != cws_u->sat_e) || (0 == cws_u->wsl_w) ) {
    u3z(msg);
    return c3n;
  }

  u3_noun opcode = u3h(msg);
  u3_noun body   = u3t(msg);
  c3_w    opc_w;

  if ( c3n == u3r_safe_word(opcode, &opc_w) ) {
    u3l_log("cttp: websocket invalid opcode");
    u3z(msg);
    return c3n;
  }

  c3_y* buf_y = 0;
  size_t len_w = 0;

  if ( u3_nul != body ) {
    if ( u3_nul != u3h(body) ) {
      u3l_log("cttp: websocket body malformed");
      u3z(msg);
      return c3n;
    }

    u3_noun oct = u3t(body);
    c3_d len_d = u3r_chub(0, u3h(oct));
    if ( len_d > SIZE_MAX ) {
      u3l_log("cttp: websocket body too large");
      u3z(msg);
      return c3n;
    }

    len_w = (size_t)len_d;
    if ( len_w ) {
      buf_y = c3_malloc(len_w);
      u3r_bytes(0, len_w, buf_y, u3t(oct));
    }
  }

  struct wslay_event_msg out = {
    .opcode = (uint8_t)(opc_w & 0xFF),
    .msg = buf_y,
    .msg_length = len_w
  };

  if ( 0 != wslay_event_queue_msg(cws_u->wsl_w, &out) ) {
    if ( buf_y ) {
      c3_free(buf_y);
    }
    u3l_log("cttp: websocket queue failed");
    u3z(msg);
    return c3n;
  }

  if ( buf_y ) {
    c3_free(buf_y);
  }

  u3z(msg);
  _cttp_ws_proceed(cws_u);
  return c3y;
}

static void
_cttp_ws_fail_handshake(u3_creq* ceq_u, const c3_c* err_c)
{
  u3_cws* cws_u = ceq_u->wsu_u;

  if ( 0 == cws_u ) {
    return;
  }

  ceq_u->wsu_u = 0;

  if ( err_c ) {
    u3l_log("cttp: websocket handshake failed (%s)", err_c);
  }
  else {
    u3l_log("cttp: websocket handshake failed (unknown)");
  }

  _cttp_ws_plan_event(cws_u, u3nc(u3i_string("reject"), u3_nul));
  _cttp_ws_close(cws_u, c3n);
}

static c3_o
_cttp_ws_start(u3_cttp* ctp_u, c3_l wid_l, u3_atom url)
{
  u3l_log("cttp: ws start wid=%u", wid_l);
  u3_cws* cws_u = c3_calloc(sizeof(*cws_u));

  cws_u->wid_l = wid_l;
  cws_u->sat_e = u3_cws_pending;

  _cttp_ws_generate_key(cws_u);
  _cttp_ws_link(ctp_u, cws_u);

  u3_atom nor = _cttp_ws_normalize_url(url);

  u3_noun hed = u3_nul;
  hed = u3nc(u3nc(u3i_string("Sec-WebSocket-Key"),
                 u3i_string(cws_u->key_c)),
             hed);
  hed = u3nc(u3nc(u3i_string("Sec-WebSocket-Version"),
                 u3i_string("13")),
             hed);
  hed = u3nc(u3nc(u3i_string("Connection"),
                 u3i_string("Upgrade")),
             hed);
  hed = u3nc(u3nc(u3i_string("Upgrade"),
                 u3i_string("websocket")),
             hed);
  {
    c3_c* ori_c = _cttp_ws_origin(nor);
    hed = u3nc(u3nc(u3i_string("Origin"), u3i_string(ori_c)), hed);
    c3_free(ori_c);
  }

  u3_noun hes = u3nq(u3i_string("GET"), u3k(nor), hed, u3_nul);

  u3_creq* ceq_u = _cttp_creq_new(ctp_u, wid_l, hes);

  if ( 0 == ceq_u ) {
    _cttp_ws_plan_event(cws_u, u3nc(u3i_string("reject"), u3_nul));
    _cttp_ws_close(cws_u, c3n);
    u3z(nor);
    u3z(url);
    return c3y;
  }

  ceq_u->wsu_u = cws_u;
  cws_u->ceq_u = ceq_u;
  cws_u->sec = ceq_u->sec;

  _cttp_creq_start(ceq_u);

  u3z(nor);
  u3z(url);
  return c3y;
}


/* _cttp_creq_quit(): cancel a u3_creq
*/
static void
_cttp_creq_quit(u3_creq* ceq_u)
{
  if ( ceq_u->wsu_u ) {
    _cttp_ws_fail_handshake(ceq_u, "cancel");
    _cttp_creq_free(ceq_u);
    return;
  }

  if ( u3_csat_addr == ceq_u->sat_e ) {
    ceq_u->sat_e = u3_csat_quit;
    return;  // wait to be called again on address resolution
  }

  if ( ceq_u->cli_u ) {
    h2o_http1client_cancel(ceq_u->cli_u);
  }

  _cttp_creq_free(ceq_u);
}

static void
_cttp_http_client_receive(u3_creq* ceq_u, c3_w sas_w, u3_noun mes, u3_noun uct)
{
  u3_cttp* ctp_u = ceq_u->ctp_u;

  //  XX inject partial responses as separate events
  //
  u3_noun wir = u3nt(u3i_string("http-client"),
                     u3dc("scot", c3__uv, ctp_u->sev_l),
                     u3_nul);
  u3_noun cad = u3nt(u3i_string("receive"),
                    ceq_u->num_l,
                    u3nq(u3i_string("start"), u3nc(sas_w, mes), uct, c3y));

  u3_auto_plan(&ctp_u->car_u, u3_ovum_init(0, c3__i, wir, cad));
}

/* _cttp_creq_fail(): dispatch error response
*/
static void
_cttp_creq_fail(u3_creq* ceq_u, const c3_c* err_c)
{
  if ( ceq_u->wsu_u ) {
    _cttp_ws_fail_handshake(ceq_u, err_c ? err_c : "handshake failed");
    _cttp_creq_free(ceq_u);
    return;
  }

  // XX anything other than a 504?
  c3_w cod_w = 504;

  u3l_log("http: fail (%d, %d): %s", ceq_u->num_l, cod_w, err_c);

  // XX include err_c as response body?
  _cttp_http_client_receive(ceq_u, cod_w, u3_nul, u3_nul);
  _cttp_creq_free(ceq_u);
}

/* _cttp_creq_respond(): dispatch response
*/
static void
_cttp_creq_respond(u3_creq* ceq_u)
{
  if ( ceq_u->wsu_u ) {
    _cttp_ws_fail_handshake(ceq_u, "handshake rejected");
    _cttp_creq_free(ceq_u);
    return;
  }

  u3_cres* res_u = ceq_u->res_u;

  _cttp_http_client_receive(ceq_u, res_u->sas_w, res_u->hed,
             ( !res_u->bod_u ) ? u3_nul :
             u3nc(u3_nul, _cttp_bods_to_octs(res_u->bod_u)));

  _cttp_creq_free(ceq_u);
}

// XX research: may be called with closed client?
/* _cttp_creq_on_body(): cb invoked by h2o upon receiving a response body
*/
static c3_i
_cttp_creq_on_body(h2o_http1client_t* cli_u, const c3_c* err_c)
{
  u3_creq* ceq_u = (u3_creq *)cli_u->data;

  if ( ceq_u->wsu_u ) {
    _cttp_ws_fail_handshake(ceq_u, err_c ? err_c : "unexpected body");
    _cttp_creq_free(ceq_u);
    return -1;
  }

  if ( 0 != err_c && h2o_http1client_error_is_eos != err_c ) {
    _cttp_creq_fail(ceq_u, err_c);
    return -1;
  }

  h2o_buffer_t* buf_u = cli_u->sock->input;

  if ( buf_u->size ) {
    _cttp_cres_fire_body(ceq_u->res_u,
                         _cttp_bod_new(buf_u->size, buf_u->bytes));
    h2o_buffer_consume(&cli_u->sock->input, buf_u->size);
  }

  // We're using the end of stream thing here to queue event to urbit. we'll
  // need to separate this into our own timer for partial progress sends.
  if ( h2o_http1client_error_is_eos == err_c ) {
    _cttp_creq_respond(ceq_u);
  }

  return 0;
}

/* _cttp_creq_on_head(): cb invoked by h2o upon receiving response headers
*/
static h2o_http1client_body_cb
_cttp_creq_on_head(h2o_http1client_t* cli_u, const c3_c* err_c, c3_i ver_i,
                   c3_i sas_i, h2o_iovec_t sas_u, h2o_header_t* hed_u,
                   size_t hed_t, c3_i len_i)
{
  u3_creq* ceq_u = (u3_creq *)cli_u->data;

  if ( ceq_u->wsu_u ) {
    u3_cws* cws_u = ceq_u->wsu_u;

    if ( 0 != err_c && h2o_http1client_error_is_eos != err_c ) {
      _cttp_ws_fail_handshake(ceq_u, err_c);
      _cttp_creq_free(ceq_u);
      return 0;
    }

    if ( 101 != sas_i ) {
      _cttp_ws_fail_handshake(ceq_u, "status");
      _cttp_creq_free(ceq_u);
      return 0;
    }

    for ( size_t i = 0; i < hed_t; i++ ) {
      h2o_header_t* hdr = &hed_u[i];
      u3l_log("cttp: ws hdr %.*s: %.*s",
              (int)hdr->name->len, hdr->name->base,
              (int)hdr->value.len, hdr->value.base);
    }

    h2o_iovec_t* acc_u = _cttp_find_header(hed_u, hed_t, "sec-websocket-accept");
    c3_c exp_c[29];

    if ( 0 == acc_u ) {
      _cttp_ws_fail_handshake(ceq_u, "missing accept");
      _cttp_creq_free(ceq_u);
      return 0;
    }

    _cttp_ws_compute_accept(cws_u->key_c, exp_c);

    if ( acc_u->len != 28 || 0 != memcmp(acc_u->base, exp_c, 28) ) {
      _cttp_ws_fail_handshake(ceq_u, "bad accept");
      _cttp_creq_free(ceq_u);
      return 0;
    }

    h2o_socket_t* sok_u = h2o_http1client_steal_socket(cli_u);

    if ( 0 == sok_u ) {
      _cttp_ws_fail_handshake(ceq_u, "steal failure");
      _cttp_creq_free(ceq_u);
      return 0;
    }

    ceq_u->wsu_u = 0;
    ceq_u->cli_u = 0;
    cws_u->ceq_u = 0;

    if ( sok_u->input && sok_u->input->size ) {
      u3l_log("cttp: ws draining leftover handshake wid=%u size=%zu",
              cws_u->wid_l,
              (size_t)sok_u->input->size);
      h2o_buffer_consume(&sok_u->input, sok_u->input->size);
    }

    _cttp_creq_free(ceq_u);

    memset(&cws_u->wcb_u, 0, sizeof(cws_u->wcb_u));
    cws_u->wcb_u.recv_callback = _cttp_ws_recv_cb;
    cws_u->wcb_u.send_callback = _cttp_ws_send_cb;
    cws_u->wcb_u.genmask_callback = _cttp_ws_genmask_cb;
    cws_u->wcb_u.on_msg_recv_callback = _cttp_ws_msg_cb;

    wslay_event_context_client_init(&cws_u->wsl_w, &cws_u->wcb_u, cws_u);

    cws_u->sok_u = sok_u;
    sok_u->data = cws_u;

    cws_u->sat_e = u3_cws_open;

    _cttp_ws_plan_event(cws_u, u3nc(u3i_string("accept"), u3_nul));
    _cttp_ws_proceed(cws_u);
    return 0;
  }

  if ( 0 != err_c && h2o_http1client_error_is_eos != err_c ) {
    _cttp_creq_fail(ceq_u, err_c);
    return 0;
  }

  _cttp_cres_new(ceq_u, (c3_w)sas_i);
  ceq_u->res_u->hed = _cttp_heds_to_noun(hed_u, hed_t);

  if ( h2o_http1client_error_is_eos == err_c ) {
    _cttp_creq_respond(ceq_u);
    return 0;
  }

  return _cttp_creq_on_body;
}

/* _cttp_creq_on_connect(): cb invoked by h2o upon successful connection
*/
static h2o_http1client_head_cb
_cttp_creq_on_connect(h2o_http1client_t* cli_u, const c3_c* err_c,
                      h2o_iovec_t** vec_u, size_t* vec_i, c3_i* hed_i)
{
  u3_creq* ceq_u = (u3_creq *)cli_u->data;

  if ( 0 != err_c ) {
    //  if synchronously connecting, caller will cleanup
    //
    if ( u3_csat_conn == ceq_u->sat_e ) {
      ceq_u->sat_e = u3_csat_quit;
    }
    else {
      u3_assert( u3_csat_ripe == ceq_u->sat_e );
      _cttp_creq_fail(ceq_u, err_c);
    }
    return 0;
  }

  //  serialize request (populate rub_u)
  //
  _cttp_creq_fire(ceq_u);

  {
    c3_w len_w;
    ceq_u->vec_u = _cttp_bods_to_vec(ceq_u->rub_u, &len_w);

    *vec_i = len_w;
    *vec_u = ceq_u->vec_u;
    *hed_i = (0 == strcmp(ceq_u->met_c, "HEAD"));
  }

  return _cttp_creq_on_head;
}

/* _cttp_creq_connect(): establish connection
*/
static void
_cttp_creq_connect(u3_creq* ceq_u)
{
  u3_assert( u3_csat_conn == ceq_u->sat_e );
  u3_assert( ceq_u->ipf_c );

  //  connect by ip/port, avoiding synchronous getaddrinfo()
  //
  {
    h2o_iovec_t ipf_u = h2o_iovec_init(ceq_u->ipf_c, strlen(ceq_u->ipf_c));
    c3_t        tls_t = ( c3y == ceq_u->sec );
    c3_s        por_s = ( ceq_u->por_s )
                        ? ceq_u->por_s
                        : ( tls_t ) ? 443 : 80;

    h2o_http1client_connect(&ceq_u->cli_u, ceq_u, &ceq_u->ctp_u->ctx_u,
                            ipf_u, por_s, tls_t, _cttp_creq_on_connect);
  }

  //  connect() failed, cb invoked synchronously
  //
  if ( u3_csat_conn != ceq_u->sat_e ) {
    u3_assert( u3_csat_quit == ceq_u->sat_e );
    //  only one such failure case
    //
    _cttp_creq_fail(ceq_u, "socket create error");
  }
  else {
    ceq_u->sat_e = u3_csat_ripe;

    //  fixup hostname for TLS handshake
    //
    //    must be synchronous, after successfull connect() call
    //
    if ( ceq_u->hot_c && (c3y == ceq_u->sec) ) {
      u3_assert( ceq_u->cli_u );
      c3_free(ceq_u->cli_u->ssl.server_name);
      ceq_u->cli_u->ssl.server_name = strdup(ceq_u->hot_c);
    }
  }
}

/* _cttp_creq_resolve_cb(): cb upon IP address resolution
*/
static void
_cttp_creq_resolve_cb(uv_getaddrinfo_t* adr_u,
                      c3_i              sas_i,
                      struct addrinfo*  aif_u)
{
  u3_creq* ceq_u = adr_u->data;

  if ( u3_csat_quit == ceq_u->sat_e ) {
    _cttp_creq_quit(ceq_u);;
  }
  else if ( 0 != sas_i ) {
    _cttp_creq_fail(ceq_u, uv_strerror(sas_i));
  }
  else {
    // XX traverse struct a la _ames_czar_cb
    ceq_u->ipf_w = ntohl(((struct sockaddr_in *)aif_u->ai_addr)->sin_addr.s_addr);
    ceq_u->ipf_c = _cttp_creq_ip(ceq_u->ipf_w);

    ceq_u->sat_e = u3_csat_conn;
    _cttp_creq_connect(ceq_u);
  }

  c3_free(adr_u);
  uv_freeaddrinfo(aif_u);
}

/* _cttp_creq_resolve(): resolve hostname to IP address
*/
static void
_cttp_creq_resolve(u3_creq* ceq_u)
{
  u3_assert(u3_csat_addr == ceq_u->sat_e);
  u3_assert(ceq_u->hot_c);

  uv_getaddrinfo_t* adr_u = c3_malloc(sizeof(*adr_u));
  adr_u->data = ceq_u;

  struct addrinfo hin_u;
  memset(&hin_u, 0, sizeof(struct addrinfo));

  hin_u.ai_family = PF_INET;
  hin_u.ai_socktype = SOCK_STREAM;
  hin_u.ai_protocol = IPPROTO_TCP;

  // XX is this necessary?
  c3_c* por_c = ceq_u->por_c ? ceq_u->por_c :
                ( c3y == ceq_u->sec ) ? "443" : "80";

  c3_i sas_i;

  if ( 0 != (sas_i = uv_getaddrinfo(u3L, adr_u, _cttp_creq_resolve_cb,
                                         ceq_u->hot_c, por_c, &hin_u)) ) {
    _cttp_creq_fail(ceq_u, uv_strerror(sas_i));
  }
}

/* _cttp_creq_start(): start a request
*/
static void
_cttp_creq_start(u3_creq* ceq_u)
{
  if ( ceq_u->ipf_c ) {
    ceq_u->sat_e = u3_csat_conn;
    _cttp_creq_connect(ceq_u);
  } else {
    ceq_u->sat_e = u3_csat_addr;
    _cttp_creq_resolve(ceq_u);
  }
}

/* _cttp_init_tls: initialize OpenSSL context
*/
static SSL_CTX*
_cttp_init_tls(void)
{
  // XX require 1.1.0 and use TLS_client_method()
  SSL_CTX* tls_u = SSL_CTX_new(SSLv23_client_method());
  // XX use SSL_CTX_set_max_proto_version() and SSL_CTX_set_min_proto_version()
  SSL_CTX_set_options(tls_u, SSL_OP_NO_SSLv2 |
                             SSL_OP_NO_SSLv3 |
                             // SSL_OP_NO_TLSv1 | // XX test
                             SSL_OP_NO_COMPRESSION);

  u3K.ssl_x509_f(SSL_CTX_get_cert_store(tls_u));
  SSL_CTX_set_verify(tls_u, SSL_VERIFY_PEER, 0);
  SSL_CTX_set_session_cache_mode(tls_u, SSL_SESS_CACHE_OFF);
  SSL_CTX_set_cipher_list(tls_u,
                          "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:"
                          "ECDH+AES128:DH+AES:ECDH+3DES:DH+3DES:RSA+AESGCM:"
                          "RSA+AES:RSA+3DES:!aNULL:!MD5:!DSS");

  return tls_u;
}

/* _cttp_ef_http_client(): send an %http-client (outgoing request) to cttp.
*/
static c3_o
_cttp_ef_http_client(u3_cttp* ctp_u, u3_noun tag, u3_noun dat)
{
  u3_creq* ceq_u;
  c3_o     ret_o;

  if ( c3y == u3r_sing_c("request", tag) ) {
    u3_noun num, req;
    c3_l  num_l;

    if (  (c3n == u3r_cell(dat, &num, &req))
       || (c3n == u3r_safe_word(num, &num_l)) )
    {
      u3l_log("cttp: strange request");
      ret_o = c3n;
    }
    else if ( (ceq_u = _cttp_creq_new(ctp_u, num_l, u3k(req))) ) {
      _cttp_creq_start(ceq_u);
      ret_o = c3y;
    }
    else {
      ret_o = c3n;
    }
  }
  else if ( c3y == u3r_sing_c("cancel-request", tag) ) {
    c3_l num_l;

    if ( c3n == u3r_safe_word(dat, &num_l) ) {
      u3l_log("cttp: strange cancel-request");
      ret_o = c3n;
    }
    else if ( (ceq_u =_cttp_creq_find(ctp_u, num_l)) ) {
      _cttp_creq_quit(ceq_u);
      ret_o = c3y;
    }
    else {
      //  accepted whether or not request exists
      //
      ret_o = c3y;
    }
  }
  else if ( c3y == u3r_sing_c("websocket-handshake", tag) ) {
    u3_noun wid, url;
    c3_l    wid_l;

    if (  (c3n == u3r_cell(dat, &wid, &url))
       || (c3n == u3r_safe_word(wid, &wid_l)) )
    {
      u3l_log("cttp: strange websocket-handshake");
      ret_o = c3n;
    }
    else {
      ret_o = _cttp_ws_start(ctp_u, wid_l, u3k(url));
    }
  }
  else if ( c3y == u3r_sing_c("websocket-response", tag) ) {
    u3_noun wid, evt;
    c3_l    wid_l;

    if (  (c3n == u3r_cell(dat, &wid, &evt))
       || (c3n == u3r_safe_word(wid, &wid_l)) )
    {
      u3l_log("cttp: strange websocket-response");
      ret_o = c3n;
    }
    else {
      u3_cws* cws_u = _cttp_ws_find(ctp_u, wid_l);

      u3_noun typ = u3h(evt);

      if ( 0 == cws_u ) {
        if ( c3y == u3r_sing_c("message", typ) ) {
          u3l_log("cttp: unknown websocket id %u", wid_l);
          ret_o = c3n;
        }
        else {
          ret_o = c3y;
        }
      }
      else {
        if ( c3y == u3r_sing_c("message", typ) ) {
          ret_o = _cttp_ws_send_message(cws_u, u3k(u3t(evt)));
        }
        else if ( c3y == u3r_sing_c("disconnect", typ) ) {
          _cttp_ws_queue_close(cws_u);
          ret_o = c3y;
        }
        else if ( c3y == u3r_sing_c("accept", typ) ) {
          ret_o = c3y;
        }
        else {
          u3l_log("cttp: unexpected websocket response");
          ret_o = c3n;
        }
      }
    }
  }
  else {
    u3l_log("cttp: strange effect (unknown type)");
    ret_o = c3n;
  }

  u3z(tag); u3z(dat);
  return ret_o;
}

/* _cttp_io_talk(): notify that we're live.
*/
static void
_cttp_io_talk(u3_auto* car_u)
{
  u3_cttp* ctp_u = (u3_cttp*)car_u;

  //  XX remove u3A->sen
  //
  u3_noun wir = u3nt(u3i_string("http-client"),
                     u3dc("scot", c3__uv, ctp_u->sev_l),
                     u3_nul);
  u3_noun cad = u3nc(c3__born, u3_nul);

  u3_auto_plan(car_u, u3_ovum_init(0, c3__i, wir, cad));
}

/* _cttp_io_kick(): apply effects
*/
static c3_o
_cttp_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_cttp* ctp_u = (u3_cttp*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &dat))
     || (c3n == u3r_sing_c("http-client", i_wir)) )
  {
    ret_o = c3n;
  }
  else {
    ret_o = _cttp_ef_http_client(ctp_u, u3k(tag), u3k(dat));
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

/* _cttp_io_exit_cb(): free cttp.
*/
static void
_cttp_io_exit_cb(uv_handle_t* han_u)
{
  u3_cttp* ctp_u = han_u->data;

  SSL_CTX_free(ctp_u->tls_u);
  c3_free(ctp_u);
}

/* _cttp_io_exit(): shut down cttp.
*/
static void
_cttp_io_exit(u3_auto* car_u)
{
  u3_cttp* ctp_u = (u3_cttp*)car_u;

  //  close unused handle to free [ctp_u] after h2o is done
  //
  uv_close((uv_handle_t*)&ctp_u->nop_u, _cttp_io_exit_cb);

  {
    u3_cws* cws_u = ctp_u->cws_u;

    while ( cws_u ) {
      u3_cws* nex_u = cws_u->nex_u;
      _cttp_ws_close(cws_u, c3n);
      cws_u = nex_u;
    }
  }

  //  cancel requests
  //
  {
    u3_creq* ceq_u = ctp_u->ceq_u;

    while ( ceq_u ) {
      _cttp_creq_quit(ceq_u);
      ceq_u = ceq_u->nex_u;
    }
  }

  h2o_timeout_dispose(u3L, &ctp_u->tim_u);
}

/* u3_cttp_io_init(): initialize http client I/O.
*/
u3_auto*
u3_cttp_io_init(u3_pier* pir_u)
{
  u3_cttp* ctp_u = c3_calloc(sizeof(*ctp_u));

  //  link to event loop
  //
  ctp_u->ctx_u.loop = u3L;

  //  unused handle for async close
  //
  uv_async_init(u3L, &ctp_u->nop_u, 0);
  ctp_u->nop_u.data = ctp_u;

  //  link to initialized request timeout
  //
  h2o_timeout_init(u3L, &ctp_u->tim_u, 300 * 1000);
  ctp_u->ctx_u.io_timeout = &ctp_u->tim_u;
  ctp_u->ctx_u.websocket_timeout = &ctp_u->tim_u;

  //  link to initialized tls ctx
  //
  ctp_u->tls_u = _cttp_init_tls();
  ctp_u->ctx_u.ssl_ctx = ctp_u->tls_u;

  u3_auto* car_u = &ctp_u->car_u;
  car_u->nam_m = c3__cttp;

  //  XX set in done_cb for %born
  //
  car_u->liv_o = c3y;
  car_u->io.talk_f = _cttp_io_talk;
  car_u->io.kick_f = _cttp_io_kick;
  car_u->io.exit_f = _cttp_io_exit;
  //  XX retry up to N?
  //
  // car_u->ev.bail_f = ...;

  {
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now = u3_time_in_tv(&tim_u);
    ctp_u->sev_l = u3r_mug(now);
    u3z(now);
  }

  return car_u;
}
