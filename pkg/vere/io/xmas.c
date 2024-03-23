/// @file

#include "vere.h"
#include "ivory.h"


#include "noun.h"
#include "ur.h"
#include "cubic.h"
#include "xmas/xmas.h"
#include "xmas/bitset.h"
#include <allocate.h>
#include <defs.h>
#include <error.h>
#include <imprison.h>
#include <jets/q.h>
#include <manage.h>
#include <motes.h>
#include <retrieve.h>
#include <types.h>
#include <stdlib.h>
#include "blake.h"

c3_o dop_o = c3n;

// #define XMAS_DEBUG     c3y
//#define XMAS_TEST
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
#define REORDER_THRESH  5

// logging and debug symbols
#define XMAS_SYM_DESC(SYM) XMAS_DESC_ ## SYM
#define XMAS_SYM_FIELD(SYM) XMAS_FIELD_ ## SYM
#ifdef XMAS_DEBUG
  #define XMAS_LOG(SYM, ...) { sam_u->sat_u.XMAS_SYM_FIELD(SYM)++; u3l_log("xmas: (%u) %s", __LINE__, XMAS_SYM_DESC(SYM)); }
#else
  #define XMAS_LOG(SYM, ...) { sam_u->sat_u.XMAS_SYM_FIELD(SYM)++; }
#endif

typedef struct _u3_xmas_stat {
  c3_w dop_w;  // dropped for other reasons
  c3_w ser_w;  // dropped for serialisation
  c3_w aut_w;  // droppped for auth
  c3_w apa_w;  // dropped bc no interest
  c3_w inv_w;  // non-fatal invariant violation
  c3_w dup_w;  // duplicates
} u3_xmas_stat;

#define XMAS_DESC_STRANGE "dropped strange packet"
#define XMAS_FIELD_STRANGE dop_w

#define XMAS_DESC_SERIAL "dropped packet (serialisation)"
#define XMAS_FIELD_SERIAL ser_w

#define XMAS_DESC_AUTH "dropped packet (authentication)"
#define XMAS_FIELD_AUTH aut_w

#define XMAS_DESC_INVARIANT "invariant violation"
#define XMAS_FIELD_INVARIANT inv_w

#define XMAS_DESC_APATHY "dropped packet (no interest)"
#define XMAS_FIELD_APATHY apa_w

#define XMAS_DESC_DUPE "dropped packet (duplicate)"
#define XMAS_FIELD_DUPE dup_w

// routing table sentinels
#define XMAS_CZAR         1  // pending dns lookup
#define XMAS_ROUT         2  // have route

/** typedef u3_xmas_pack_stat u3_noun
 *  [last=@da tries=@ud]
 */
typedef struct _u3_pact_stat {
  c3_d  sen_d; // last sent
  c3_y  sip_y; // skips 
  c3_y  tie_y; // tries
  c3_y  dup_y; // dupes
} u3_pact_stat;

typedef struct _u3_peer_last {
  c3_d acc_d; // time of last access
  c3_d son_d; // time of sponsor check
} u3_peer_last;

typedef struct _u3_misord_buf {
  c3_y* fra_y;
  c3_w  len_w;
  c3_w  num_w;
  blake_pair* par_u;
} u3_misord_buf;

typedef struct _u3_xmas_pict {
  uv_udp_send_t      snd_u;
  struct _u3_xmas*   sam_u;
  u3_xmas_pact       pac_u;
} u3_xmas_pict;

typedef struct _u3_pend_req {
  c3_w                   nex_w; // number of the next fragment to be sent
  c3_w                   tot_w; // total number of fragments expected
  uv_timer_t             tim_u; // timehandler
  c3_y*                  dat_y; // ((mop @ud *) lte)
  c3_w                   len_w;
  c3_w                   lef_w; // lowest fragment number currently in flight/pending
  c3_w                   old_w; // frag num of oldest packet sent
  c3_w                   ack_w; // highest acked fragment number
  u3_lane                lan_u; // last lane heard
  u3_xmas_auth           aum_u;
  u3_gage*               gag_u; // congestion control
  u3_vec(u3_misord_buf)  mis_u; // misordered blake hash
  blake_bao*             bao_u; // blake verifier
  u3_xmas_pict*          pic_u; // preallocated request packet
  u3_pact_stat*          wat_u; // ((mop @ud packet-state) lte)
  u3_bitset              was_u; // ((mop @ud packet-state) lte)
  c3_y                  pad_y[64];
} u3_pend_req;

typedef struct _u3_czar_info {
  c3_w               pip_w; // IP of galaxy
  c3_y               imp_y; // galaxy number
  struct _u3_xmas*   sam_u; // backpointer
  time_t             tim_t; // time of retrieval
  c3_c*              dns_c; // domain
  u3_noun            pen;   // (list @) of pending packet
} u3_czar_info;

/* _u3_xmas: next generation networking
 */
typedef struct _u3_xmas {
  u3_auto            car_u;
  u3_pier*           pir_u;
  union {
    uv_udp_t         wax_u;
    uv_handle_t      had_u;
  };
  u3_xmas_stat       sat_u;             // statistics
  c3_l               sev_l;             // XX: ??
  c3_o               for_o;             //  is forwarding
  u3p(u3h_root)      her_p;             //  (map ship u3_peer)
  u3p(u3h_root)      lan_p;             //  (map [lane ship] u3_gage)
  u3p(u3h_root)      req_p;             //  (map [ship rift path] u3_pend_req)
  u3p(u3h_root)      cac_p;             //  (map name packet=@)  // response cache
  u3p(u3h_root)      pit_p;             //  (map name [first=@ last=@ our=? for=(set lane)])
  u3p(u3h_root)      our_p;             //  (map name [packet=@ time=@])  // request set
  u3_czar_info       imp_u[256];        // galaxy information
  c3_c*              dns_c;             // turf (urbit.org)
  c3_d               tim_d;             // XX: move to u3_pend_req
} u3_xmas;


typedef struct _u3_peer {
  u3_peer_last   las_u; // last check timestamps
  u3_lane        dir_u; // direct lane (if any)
  u3_lane        ind_u; // indirect lane (if any)
  c3_s           imp_s; // galaxy
  u3_xmas*       sam_u; // backpointer
} u3_peer;

// Sent request
//   because of lifecycles a u3_xmas_pact may have several libuv
//   callbacks associated with it, so we can't those as callback
//   instead just alloc new buffer and stick here
typedef struct _u3_seal {
  uv_udp_send_t    snd_u;  // udp send request
  u3_xmas*         sam_u;
  c3_w             len_w;
  c3_y*            buf_y;
} u3_seal;

static c3_d
get_millis() {
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
	// Convert the seconds to milliseconds by multiplying by 1000
	// Convert the microseconds to milliseconds by dividing by 1000
}

static void
_log_buf(c3_y* buf_y, c3_w len_w)
{
  for( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    fprintf(stderr, "%02x", buf_y[i_w]);
  }
  fprintf(stderr, "\r\n");
}


static void
_log_gage(u3_gage* gag_u)
{
  u3l_log("gauge");
  u3l_log("rtt: %f", ((double)gag_u->rtt_w / 1000));
  u3l_log("rto: %f", ((double)gag_u->rto_w / 1000));
  u3l_log("rttvar: %f", ((double)gag_u->rtv_w / 1000));
  u3l_log("cwnd: %u", gag_u->wnd_w);
  u3l_log("cwnd fraction: %f", gag_u->wnf_w / (float)gag_u->wnd_w );
  u3l_log("ssthresh: %u", gag_u->sst_w);
  u3l_log("counter: %u", gag_u->con_w);
  //u3l_log("algorithm: %s", gag_u->alg_c);
}

static void
_log_lane(u3_lane* lan_u)
{
  u3l_log("xmas: lane (%s,%u)", u3r_string(u3dc("scot", c3__if, u3i_word(lan_u->pip_w))), lan_u->por_s);
}


static void _log_peer(u3_peer* per_u)
{
  if ( per_u == NULL ) {
    u3l_log("NULL peer");
    return;
  }
  u3l_log("dir");
  _log_lane(&per_u->dir_u);
  u3l_log("ind");
  _log_lane(&per_u->ind_u);
  u3l_log("galaxy: %s", u3r_string(u3dc("scot", 'p', per_u->imp_s)));
}

static void
_log_czar_info(u3_czar_info* zar_u)
{
  {
    u3_noun nam = u3dc("scot", 'p', zar_u->imp_y);
    u3l_log("czar: %s", u3r_string(nam));
    u3_noun pip = u3dc("scot", c3__if, zar_u->pip_w);
    u3l_log("IP: %s", u3r_string(pip));
    u3l_log("time: %" PRIu64, (c3_d)zar_u->tim_t);
    u3l_log("dns: %s", zar_u->dns_c != NULL ? zar_u->dns_c : "NO DNS");
    u3l_log("pending: %u", u3r_word(0, u3do("lent", zar_u->pen)));
  }
}

static void
_log_pend_req(u3_pend_req* req_u)
{
  if( req_u == NULL ) {
    u3l_log("pending request was NULL");
    return;
  }
  u3l_log("have: %u", req_u->len_w);
  u3l_log("next: %u", req_u->nex_w);
  u3l_log("total: %u", req_u->tot_w);
  u3l_log("gage: %c", req_u->gag_u == NULL ? 'n' : 'y');
  //u3l_log("timer in: %" PRIu64 " ms", uv_timer_get_due_in(&req_u->tim_u));
}

static c3_d
_get_now_micros()
{
  struct timeval tim_u;
  gettimeofday(&tim_u, NULL);
  return (tim_u.tv_sec * 1000000) + tim_u.tv_usec;
}

static c3_d 
_abs_dif(c3_d ayy_d, c3_d bee_d)
{
  return ayy_d > bee_d ? ayy_d - bee_d : bee_d - ayy_d;
}

static c3_d
_clamp_rto(c3_d rto_d) {
  return c3_min(c3_max(rto_d, 200000), 25000000);
}

static c3_c*
_xmas_czar_dns(c3_y imp_y, c3_c* zar_c)
{
  u3_noun nam = u3dc("scot", 'p', imp_y);
  c3_c* nam_c = u3r_string(nam);
  c3_w len_w = 3 + strlen(nam_c) + strlen(zar_c);
  u3_assert(len_w <= 256);
  c3_c* dns_c = c3_calloc(len_w);

  c3_i sas_i = snprintf(dns_c, len_w, "%s.%s.", nam_c + 1, zar_c);
  u3_assert(sas_i <= 255);

  c3_free(nam_c);
  u3z(nam);

  return dns_c;
}


/*  _xmas_encode_path(): produce buf_y as a parsed path
*/
static u3_noun
_xmas_encode_path(c3_w len_w, c3_y* buf_y)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  {
    u3_noun*   hed;
    u3_noun*   tel;
    c3_y*    fub_y = buf_y;
    c3_y     car_y;
    c3_w     tem_w;
    u3i_slab sab_u;

    while ( len_w-- ) {
      car_y = *buf_y++;
      if ( len_w == 0 ) {
        buf_y++;
        car_y = 47;
      }

      if ( 47 == car_y ) {
        tem_w = buf_y - fub_y - 1;
        u3i_slab_bare(&sab_u, 3, tem_w);
        sab_u.buf_w[sab_u.len_w - 1] = 0;
        memcpy(sab_u.buf_y, fub_y, tem_w);

        *lit  = u3i_defcons(&hed, &tel);
        *hed  = u3i_slab_moot(&sab_u);
        lit   = tel;
        fub_y = buf_y;
      }
    }
  }

  *lit = u3_nul;

  return pro;
}

static void
_xmas_free_pict(u3_xmas_pict* pic_u)
{
  xmas_free_pact(&pic_u->pac_u);
  c3_free(pic_u);
}

static u3_atom
_dire_etch_ud(c3_d num_d)
{
  c3_y  hun_y[26];
  c3_y* buf_y = u3s_etch_ud_smol(num_d, hun_y);
  c3_w  dif_w = (c3_p)buf_y - (c3_p)hun_y;
  return u3i_bytes(26 - dif_w, buf_y); // XX known-non-null
}

/* _xmas_request_key(): produce key for request hashtable sam_u->req_p from nam_u
*/
u3_noun _xmas_request_key(u3_xmas_name* nam_u)
{
  u3_noun pax = _xmas_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);
  u3_noun res = u3nt(u3i_chubs(2, nam_u->her_d), u3i_word(nam_u->rif_w), pax);
  return res;
}

static void _init_gage(u3_gage* gag_u)
{
  gag_u->rto_w = 1000000;
  gag_u->rtt_w = 1000000;
  gag_u->rtv_w = 1000000;
  gag_u->con_w = 0;
  gag_u->wnd_w = 1;
  gag_u->sst_w = 10000;
}


/* u3_xmas_encode_lane(): serialize lane to noun
*/
static u3_noun
u3_xmas_encode_lane(u3_lane lan_u) {
  // [%if ip=@ port=@]
  return u3nt(c3__if, u3i_word(lan_u.pip_w), lan_u.por_s);
}

//  lane cache is (map [lane @p] lane-info)
//  
static u3_noun
_xmas_lane_key(c3_d her_d[2], u3_lane* lan_u)
{
  return u3nc(u3i_chubs(2,her_d), u3_xmas_encode_lane(*lan_u));
}

/* RETAIN
*/
static u3_gage*
_xmas_get_lane_raw(u3_xmas* sam_u, u3_noun key)
{
  u3_gage* ret_u = NULL;
  u3_weak res = u3h_git(sam_u->lan_p, key);

  if ( res != u3_none && res != u3_nul ) {
    ret_u = u3to(u3_gage, res);
  }

  return ret_u;
}

/* _xmas_get_lane(): get lane
*/
static u3_gage*
_xmas_get_lane(u3_xmas* sam_u, c3_d her_d[2], u3_lane* lan_u) {
  u3_noun key =_xmas_lane_key(her_d, lan_u);
  u3_gage* ret_u = _xmas_get_lane_raw(sam_u, key);
  u3z(key);
  return ret_u;
}

/* _xmas_put_lane(): put lane state in state
 *
 *   uses same copying trick as _xmas_put_request()
*/
static void
_xmas_put_lane(u3_xmas* sam_u, c3_d her_d[2], u3_lane* lan_u, u3_gage* gag_u)
{
  u3_noun key = _xmas_lane_key(her_d, lan_u);
  u3_gage* old_u = _xmas_get_lane_raw(sam_u, key);
  u3_gage* new_u = gag_u;

  if ( old_u == NULL ) {
    new_u = u3a_calloc(sizeof(u3_gage),1);
    memcpy(new_u, gag_u, sizeof(u3_gage));
  } else {
    new_u = old_u;
    memcpy(new_u, gag_u, sizeof(u3_gage));
  }

  u3_noun val = u3of(u3_gage, new_u);
  u3h_put(sam_u->lan_p, key, val);
  u3z(key);
}

static void
_xmas_del_request(u3_xmas* sam_u, u3_xmas_name* nam_u) {
  u3_noun key = _xmas_request_key(nam_u);

  u3_weak req = u3h_get(sam_u->req_p, key);
  if ( req == u3_none ) {
    u3z(key);
    return;
  }
  u3_pend_req* req_u = u3to(u3_pend_req, req);
    // u3l_log("wat_u %p", req_u->wat_u);
  // u3l_log("was_u buf %p", req_u->was_u.buf_y);
  uv_timer_stop(&req_u->tim_u);
  _xmas_free_pict(req_u->pic_u);
  c3_free(req_u->wat_u);
  vec_free(&req_u->mis_u);
  c3_free(req_u->dat_y);
  blake_bao_free(req_u->bao_u);
  u3h_del(sam_u->req_p, key);
  u3a_free(req_u);
  u3z(key);
}

// congestion control update
static void _xmas_handle_ack(u3_gage* gag_u, u3_pact_stat* pat_u)
{

  gag_u->con_w++;

  c3_d now_d = _get_now_micros();
  c3_d rtt_d = now_d < pat_u->sen_d ? 0 : now_d - pat_u->sen_d;

  c3_d err_d = _abs_dif(rtt_d, gag_u->rtt_w);

  gag_u->rtt_w = (rtt_d + (gag_u->rtt_w * 7)) >> 3;
  gag_u->rtv_w = (err_d + (gag_u->rtv_w * 7)) >> 3;
  gag_u->rto_w = _clamp_rto(gag_u->rtt_w + (4*gag_u->rtv_w));

  if ( gag_u->wnd_w < gag_u->sst_w ) {
    gag_u->wnd_w++;
  } else if ( gag_u->wnd_w <= ++gag_u->wnf_w ) {
    gag_u->wnd_w++;
    gag_u->wnf_w = 0;
  }
}

/*
 * _xmas_req_get_cwnd(): produce packets to send
 *
 * saves next fragment number and preallocated pact into the passed pointers.
 * Will not do so if returning 0
*/
static c3_w
_xmas_req_get_cwnd(u3_pend_req* req_u)
{
  c3_w res_w = 0;

  if ( req_u->tot_w == 0 || req_u->gag_u == NULL ) {
    u3l_log("shouldn't happen");
    _log_pend_req(req_u);
    u3_assert(0);
    return 1;
  }

  c3_w liv_w = bitset_wyt(&req_u->was_u);
  if ( req_u->nex_w == req_u->tot_w ) {
    return 0;
  }

  c3_w rem_w = req_u->tot_w - req_u->nex_w + 1;
  return c3_min(rem_w, req_u->gag_u->wnd_w - liv_w);
}

/* _xmas_req_pact_sent(): mark packet as sent
**
*/
static void
_xmas_req_pact_resent(u3_pend_req* req_u, u3_xmas_name* nam_u)
{
  c3_d now_d = _get_now_micros();
  // if we dont have pending request noop
  if ( NULL == req_u ) {
    return;
  }

  req_u->wat_u[nam_u->fra_w].sen_d = now_d;
  req_u->wat_u[nam_u->fra_w].tie_y++;
}

/* _xmas_req_pact_sent(): mark packet as sent
**   after 1-RT first packet is handled in _xmas_req_pact_init()
*/
static void
_xmas_req_pact_sent(u3_pend_req* req_u, u3_xmas_name* nam_u)
{
  c3_d now_d = _get_now_micros();
  // if we already have pending request
  if ( NULL != req_u ) {
    if( req_u->nex_w == nam_u->fra_w ) {
      req_u->nex_w++;
    }
    // TODO: optional assertions?
    req_u->wat_u[nam_u->fra_w] = (u3_pact_stat){now_d, 0, 1, 0 };
    bitset_put(&req_u->was_u, nam_u->fra_w);
  } else {
    u3l_log("xmas: no req for sent");
    return;
  }

  if ( req_u->lef_w != 0 && c3n == bitset_has(&req_u->was_u, req_u->lef_w) ) {
    while ( req_u->lef_w++ < req_u->tot_w ) {
      if ( c3y == bitset_has(&req_u->was_u, req_u->lef_w) ) {
        break;
      }
    }
  }
}


/* _ames_czar_port(): udp port for galaxy.
*/
static c3_s
_ames_czar_port(c3_y imp_y)
{
  if ( c3n == u3_Host.ops_u.net ) {
    return 31337 + imp_y;
  }
  else {
    return 13337 + imp_y;
  }
}


/* _ames_alloc(): libuv buffer allocator.
*/
static void
_ames_alloc(uv_handle_t* had_u,
            size_t len_i,
            uv_buf_t* buf
            )
{
  //  we allocate 2K, which gives us plenty of space
  //  for a single ames packet (max size 1060 bytes)
  //
  void* ptr_v = c3_malloc(4096);
  *buf = uv_buf_init(ptr_v, 4096);
}

/* u3_xmas_decode_lane(): deserialize noun to lane; 0.0.0.0:0 if invalid
*/
static u3_lane
u3_xmas_decode_lane(u3_atom lan) {
  u3_lane lan_u;
  c3_d lan_d;

  if ( c3n == u3r_safe_chub(lan, &lan_d) || (lan_d >> 48) != 0 ) {
    return (u3_lane){0, 0};
  }

  u3z(lan);

  lan_u.pip_w = (c3_w)lan_d;
  lan_u.por_s = (c3_s)(lan_d >> 32);
  //  convert incoming localhost to outgoing localhost
  //
  lan_u.pip_w = ( 0 == lan_u.pip_w ) ? 0x7f000001 : lan_u.pip_w;

  return lan_u;
}

//  END plagariasm zone
//
//
//
//
//
//
static void _xmas_free_seal(u3_seal* sel_u)
{
  c3_free(sel_u->buf_y);
  c3_free(sel_u);
}

static u3_noun _xmas_get_now() {
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  u3_noun res = u3_time_in_tv(&tim_u);
  return res;
}




static void
_xmas_send_cb(uv_udp_send_t* req_u, c3_i sas_i)
{
  u3_seal* sel_u = (u3_seal*)req_u;
  u3_xmas* sam_u = sel_u->sam_u;

  if ( sas_i ) {
    u3l_log("xmas: send fail_async: %s", uv_strerror(sas_i));
    //sam_u->fig_u.net_o = c3n;
  }
  else {
    //sam_u->fig_u.net_o = c3y;
  }

  _xmas_free_seal(sel_u);
}

static void _xmas_send_buf(u3_xmas* sam_u, u3_lane lan_u, c3_y* buf_y, c3_w len_w)
{
  u3_seal* sel_u = c3_calloc(sizeof(*sel_u));
  // this is wrong, need to calloc & memcpy
  sel_u->buf_y = buf_y;
  sel_u->len_w = len_w;
  sel_u->sam_u = sam_u;
  struct sockaddr_in add_u;

  memset(&add_u, 0, sizeof(add_u));
  add_u.sin_family = AF_INET;
  c3_w pip_w = c3n == u3_Host.ops_u.net ? lan_u.pip_w : 0x7f000001;
  c3_s por_s = lan_u.por_s;

  add_u.sin_addr.s_addr = htonl(pip_w);
  add_u.sin_port = htons(por_s);

#ifdef XMAS_DEBUG
  c3_c* sip_c = inet_ntoa(add_u.sin_addr);
  // u3l_log("xmas: sending packet (%s,%u)", sip_c, por_s);
#endif

  uv_buf_t buf_u = uv_buf_init((c3_c*)buf_y, len_w);

  c3_i     sas_i = uv_udp_send(&sel_u->snd_u,
                               &sam_u->wax_u,
                               &buf_u, 1,
                               (const struct sockaddr*)&add_u,
                               _xmas_send_cb);

  if ( sas_i ) {
    u3l_log("ames: send fail_sync: %s", uv_strerror(sas_i));
    /*if ( c3y == sam_u->fig_u.net_o ) {
      //sam_u->fig_u.net_o = c3n;
    }*/
    _xmas_free_seal(sel_u);
  }
}



static void _xmas_send(u3_xmas_pict* pic_u, u3_lane* lan_u)
{
  u3_xmas* sam_u = pic_u->sam_u;

  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w siz_w = xmas_etch_pact(buf_y, &pic_u->pac_u);

  _xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
}


static void
_try_resend(u3_pend_req* req_u)
{
  u3_xmas* sam_u = req_u->pic_u->sam_u;
  u3_lane* lan_u = &req_u->lan_u;
  c3_o los_o = c3n;
  if ( req_u->tot_w == 0 || req_u->ack_w <= REORDER_THRESH ) {
    return;
  }
  c3_w ack_w = req_u->ack_w - REORDER_THRESH;
  c3_d now_d = _get_now_micros();
  for ( int i = req_u->lef_w; i < ack_w; i++ ) {
    if ( c3y == bitset_has(&req_u->was_u, i) ) {
      req_u->pic_u->pac_u.pek_u.nam_u.fra_w = i;
      if ( req_u->wat_u[i].tie_y == 1 ) {
        u3l_log("fast resend %u", i);
        los_o = c3y;
        c3_y* buf_y = c3_calloc(PACT_SIZE);
        req_u->pic_u->pac_u.pek_u.nam_u.fra_w = i;
        c3_w siz_w  = xmas_etch_pact(buf_y, &req_u->pic_u->pac_u);
        if ( siz_w == 0 ) {
          u3l_log("failed to etch");
          u3_assert( 0 );
        }
        // TODO: better route management
        _xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
        _xmas_req_pact_resent(req_u, &req_u->pic_u->pac_u.pek_u.nam_u);

      } else if ( (now_d - req_u->wat_u[i].sen_d) > req_u->gag_u->rto_w ) {
        los_o = c3y;
        c3_y* buf_y = c3_calloc(PACT_SIZE);
        req_u->pic_u->pac_u.pek_u.nam_u.fra_w = i;
        u3l_log("slow resending %u ", i);
        c3_w siz_w  = xmas_etch_pact(buf_y, &req_u->pic_u->pac_u);
        if( siz_w == 0 ) {
          u3l_log("failed to etch");
          u3_assert( 0 );
        }
        // TODO: better route management
        _xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
        _xmas_req_pact_resent(req_u, &req_u->pic_u->pac_u.pek_u.nam_u);
            }
          }
        }
      
  if ( c3y == los_o ) {
    req_u->gag_u->sst_w = (req_u->gag_u->wnd_w / 2) + 1;
    req_u->gag_u->wnd_w = req_u->gag_u->sst_w;
    req_u->gag_u->rto_w = _clamp_rto(req_u->gag_u->rto_w * 2);
  }
}

/* _xmas_packet_timeout(): callback for packet timeout
*/
static void
_xmas_packet_timeout(uv_timer_t* tim_u) {
  u3_pend_req* req_u = (u3_pend_req*)tim_u->data;
  u3l_log("%u packet timed out", req_u->old_w);
  _try_resend(req_u);
}

static void
_update_oldest_req(u3_pend_req *req_u, u3_gage* gag_u)
{
  if( req_u->tot_w == 0 || req_u->len_w == req_u->tot_w ) {
    return;
  }
  // scan in flight packets, find oldest
  c3_w idx_w = req_u->lef_w;
  c3_d now_d = _get_now_micros();
  c3_d wen_d = now_d;
  for ( c3_w i = req_u->lef_w; i < req_u->nex_w; i++ ) {
    // u3l_log("fra %u (%u)", i, __LINE__);
    if ( c3y == bitset_has(&req_u->was_u, i) &&
	 wen_d > req_u->wat_u[i].sen_d
    ) {
      wen_d = req_u->wat_u[i].sen_d;
      idx_w = i;
    }
  }
  if ( now_d == wen_d ) {
#ifdef XMAS_DEBUG
    u3l_log("failed to find new oldest");
#endif
  }
  req_u->old_w = idx_w;
  req_u->tim_u.data = req_u;
  c3_d gap_d = now_d -req_u->wat_u[idx_w].sen_d;
  uv_timer_start(&req_u->tim_u, _xmas_packet_timeout, (gag_u->rto_w - gap_d) / 1000, 0);
}

static void _xmas_free_misord_buf(u3_misord_buf* buf_u)
{
  c3_free(buf_u->fra_y);
  if ( buf_u->par_u != NULL ) {
    c3_free(buf_u->par_u);
  }
  c3_free(buf_u);
}

static bao_ingest_result 
_xmas_burn_misorder_queue(u3_pend_req* req_u)
{
  c3_w wan_w = req_u->bao_u->con_w;
  c3_w len_w;
  c3_o fon_o;
  bao_ingest_result res_y = BAO_GOOD;
  while ( (len_w = vec_len(&req_u->mis_u)) != 0 ) {
    fon_o = c3n;
    for (int i = 0; i < len_w; i++) {
      u3_misord_buf* buf_u = (u3_misord_buf*)req_u->mis_u.vod_p[i];
      if ( buf_u->num_w == wan_w ) {
        fon_o = c3y;
        if ( BAO_GOOD != (res_y = blake_bao_verify(req_u->bao_u, buf_u->fra_y, buf_u->len_w, buf_u->par_u))) {
          return res_y;
        } else { 
          _xmas_free_misord_buf((u3_misord_buf*)vec_pop(&req_u->mis_u, i));
          break;
        }
      }
    }
    wan_w++;
    if ( c3n == fon_o ) {
      break;
    }
  }
  return res_y; 
}

/* _xmas_req_pact_done(): mark packet as done, returning if we should continue
*/
static u3_pend_req*
_xmas_req_pact_done(u3_xmas*      sam_u,
                    u3_pend_req*  req_u,
                    u3_xmas_name *nam_u,
                    u3_xmas_data* dat_u,
                    u3_lane*      lan_u)
{
  c3_d now_d = _get_now_micros(); // XX use?

  u3_gage* gag_u = _xmas_get_lane(sam_u, nam_u->her_d, lan_u);
  req_u->gag_u = gag_u;

  // first we hear from lane
  if ( gag_u == NULL ) {
    gag_u = alloca(sizeof(u3_gage));
    memset(gag_u, 0, sizeof(u3_gage));
    _init_gage(gag_u);
  }

  req_u->lan_u = *lan_u;

  c3_w siz_w = (1 << (nam_u->boq_y - 3));
  // First packet received, instantiate request fully

  if ( dat_u->tot_w <= nam_u->fra_w ) {
    XMAS_LOG(STRANGE);
    //  XX: is this sufficient to drop whole request
    return NULL;
  }

  // received duplicate
  if ( nam_u->fra_w != 0 && c3n == bitset_has(&req_u->was_u, nam_u->fra_w) ) {
    XMAS_LOG(DUPE);
    req_u->wat_u[nam_u->fra_w].dup_y++;
    return NULL;
  }

  bitset_del(&req_u->was_u, nam_u->fra_w);
  if ( nam_u->fra_w > req_u->ack_w ) {
    req_u->ack_w = nam_u->fra_w;
  }
  if ( nam_u->fra_w != 0 && req_u->wat_u[nam_u->fra_w].tie_y != 1 ) {
#ifdef XMAS_DEBUG
    u3l_log("received retry %u", nam_u->fra_w);
#endif
  }

  req_u->len_w++;
  if ( req_u->lef_w == nam_u->fra_w ) {
    req_u->lef_w++;
  }

  blake_pair* par_u = NULL;
  if ( dat_u->aum_u.typ_e == AUTH_NEXT ) {
    // needs to be heap allocated bc will be saved if misordered
    par_u = c3_calloc(sizeof(blake_pair));
    memcpy(par_u->sin_y, dat_u->aup_u.has_y[0], BLAKE3_OUT_LEN);
    memcpy(par_u->dex_y, dat_u->aup_u.has_y[1], BLAKE3_OUT_LEN);
  }

  c3_y ver_y;
  // TODO: move to bottom
  if ( req_u->bao_u->con_w != nam_u->fra_w ) { 
    // TODO: queue packet
    u3_misord_buf* buf_u = c3_calloc(sizeof(u3_misord_buf));
    buf_u->fra_y = c3_calloc(dat_u->len_w);
    buf_u->len_w = dat_u->len_w;
    memcpy(buf_u->fra_y, dat_u->fra_y, dat_u->len_w);
    buf_u->par_u = par_u;
    buf_u->num_w = nam_u->fra_w;
    vec_append(&req_u->mis_u, buf_u);
  } else if ( BAO_GOOD != (ver_y = blake_bao_verify(req_u->bao_u, dat_u->fra_y, dat_u->len_w, par_u)) ) {
    c3_free(par_u);
    // TODO: do we drop the whole request on the floor?
    XMAS_LOG(AUTH);
    return NULL;
  } else if ( vec_len(&req_u->mis_u) != 0 
            && BAO_GOOD != (ver_y = _xmas_burn_misorder_queue(req_u))) {
    c3_free(par_u);
    XMAS_LOG(AUTH)
    return NULL;
  } else {
    c3_free(par_u);
  }

  // handle gauge update
  _xmas_handle_ack(gag_u, &req_u->wat_u[nam_u->fra_w]);


  memcpy(req_u->dat_y + (siz_w * nam_u->fra_w), dat_u->fra_y, dat_u->len_w);

  _try_resend(req_u);

  _update_oldest_req(req_u, gag_u);

  // _xmas_put_request(sam_u, nam_u, req_u);
  // _xmas_put_lane(sam_u, nam_u->her_d, lan_u, gag_u);
  return req_u;
}




static u3_lane
_realise_lane(u3_noun lan) {
  u3_lane lan_u = {0};

  if ( c3y == u3a_is_cat(lan)  ) {
    //  XX groace and wrong
    //
    if ( lan >= 256 ) {
      u3m_p("big lane", lan);
      lan &= 0xff;
    }

    if ( (c3n == u3_Host.ops_u.net) ) {
      lan_u.pip_w =  0x7f000001 ;
      lan_u.por_s = _ames_czar_port(lan);
    }
  }
  else {
    u3_noun pip, por;

    if (  (c3y == u3r_pq(lan, c3__if, &pip, &por))
       && (c3y == u3r_safe_word(pip, &lan_u.pip_w))
       && (c3y == u3a_is_cat(por))
       && (por <= 0xFFFF) )
    {
      lan_u.por_s = por;
    }
    else {
      u3m_p("inscrutable lane", lan);
    }

    u3z(lan);
  }

  return lan_u;
}

static c3_o
_xmas_rout_bufs(u3_xmas* sam_u, c3_y* buf_y, c3_w len_w, u3_noun las)
{
  c3_o suc_o = c3n;
  u3_noun lan, t = las;
  while ( t != u3_nul ) {
    u3x_cell(t, &lan, &t);
    u3_lane lan_u = _realise_lane(u3k(lan));
    #ifdef XMAS_DEBUG
     u3l_log("sending to ip: %x, port: %u", lan_u.pip_w, lan_u.por_s);
    
    #endif /* ifdef XMAS_DEBUG
        u3l_log("sending to ip: %x, port: %u", lan_u.pip_w, lan_u.por_s); */
    if ( lan_u.por_s == 0 ) {
      u3l_log("xmas: failed to realise lane");
    } else { 
      c3_y* sen_y = c3_calloc(len_w);
      memcpy(sen_y, buf_y, len_w);
      _xmas_send_buf(sam_u, lan_u, sen_y, len_w);
    }
  }
  // u3z(las);
  return suc_o;
}

static void
_xmas_timer_cb(uv_timer_t* tim_u) {
  u3_pend_req* req_u = tim_u->data;
  _try_resend(req_u);
}

static void
_xmas_czar_here(u3_czar_info* imp_u, time_t now_t, struct sockaddr_in* add_u)
{
  u3_xmas* sam_u = imp_u->sam_u;
  c3_y imp_y = imp_u->imp_y;
  c3_w pip_w = ntohl(add_u->sin_addr.s_addr);

  if ( imp_u->pip_w != pip_w ) {
    u3_noun nam = u3dc("scot", c3__if, u3i_word(pip_w));
    c3_c* nam_c = u3r_string(nam);

    u3l_log("xmas: czar %s: ip %s", imp_u->dns_c, nam_c);

    c3_free(nam_c);
    u3z(nam);
  }
  imp_u->pip_w = pip_w;
  imp_u->tim_t = now_t;

  u3_noun pac, t = imp_u->pen;

  while ( t != u3_nul ) {
    u3x_cell(t, &pac, &t);
    c3_w len_w = u3r_met(3,pac);
    c3_y* buf_y = c3_calloc(len_w);
    u3r_bytes(0, len_w, buf_y, pac);
    u3_lane lan_u = (u3_lane){pip_w, _ames_czar_port(imp_y)};
    _xmas_send_buf(sam_u, lan_u, buf_y, len_w);
  }
  u3z(imp_u->pen);
  imp_u->pen = u3_nul;
}

static void
_xmas_czar_gone(u3_xmas* sam_u, c3_i sas_i, c3_y imp_y, time_t now_t)
{
  u3_czar_info* imp_u = &sam_u->imp_u[imp_y];
  imp_u->tim_t = now_t;
  u3l_log("xmas: %s", uv_strerror(sas_i));
}

static void
_xmas_czar_cb(uv_getaddrinfo_t* adr_u, c3_i sas_i, struct addrinfo* aif_u)
{
  u3_czar_info* imp_u = (u3_czar_info*)adr_u->data;
  c3_y imp_y = imp_u->imp_y;
  u3_xmas* sam_u = imp_u->sam_u;
  time_t now_t = time(0);

  if ( 0 == sas_i ) {
    // XX: lifetimes for addrinfo, ames does something funny
    _xmas_czar_here(imp_u, now_t, (struct sockaddr_in*)aif_u->ai_addr);
  } else {
    _xmas_czar_gone(sam_u, sas_i, imp_y, now_t);
  }

  c3_free(adr_u);
  uv_freeaddrinfo(aif_u);

}



static void
_xmas_resolve_czar(u3_xmas* sam_u, c3_y imp_y, u3_noun pac)
{
  u3_assert( c3y == u3_Host.ops_u.net );
  u3_czar_info* imp_u = &sam_u->imp_u[imp_y];
  time_t now_t = time(0);
  time_t wen_t = imp_u->tim_t;
  if ( ((now_t - wen_t) < 300) ) {
    // XX: confirm validity of drop
    return;
  }
  if ( pac != u3_nul && u3_nul != imp_u->pen ) {
    // already pending, add to queue
    imp_u->pen = u3nc(pac, imp_u->pen);
    return;
  }
  if ( pac != u3_nul ) {
    imp_u->pen = u3nc(pac, imp_u->pen);
  }

  if ( !sam_u->dns_c ) {
    u3l_log("xmas: no galaxy domain");
    return;
  }
  imp_u->dns_c = _xmas_czar_dns(imp_y, sam_u->dns_c);
  {
    uv_getaddrinfo_t* adr_u = c3_calloc(sizeof(*adr_u));
    c3_i sas_i;
    adr_u->data = imp_u;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if ( 0 != (sas_i = uv_getaddrinfo(u3L, adr_u, _xmas_czar_cb, imp_u->dns_c, 0, &hints))) {
      time_t now_t = time(0);
      _xmas_czar_gone(sam_u, sas_i, imp_y, now_t);
      return;
    }
  }


}

static u3_noun
_xmas_queue_czar(u3_xmas* sam_u, u3_noun las, u3_noun pac)
{
  las = u3do("flop", las);
  u3_noun lan, t = las;
  u3_noun res = u3_nul;
  time_t now_t = time(0);

  while ( t != u3_nul ) {
    u3x_cell(t, &lan, &t);
    if ( (c3y == u3a_is_cat(lan) && lan < 256 ) ) {
      u3_czar_info* imp_u = &sam_u->imp_u[lan];

      if ( 0 != imp_u->pip_w ) {
        res = u3nc(u3nt(c3__if, u3i_word(imp_u->pip_w), _ames_czar_port(lan)), res);
      } 
      if ( c3y == u3_Host.ops_u.net ) {
        _xmas_resolve_czar(sam_u, lan, u3k(pac));
      }
    } else {
      res = u3nc(u3k(lan), res);
    }
  }
  u3z(las);
  u3z(pac);
  return res;
}

static void
_init_peer(u3_xmas* sam_u, u3_peer* per_u)
{
  per_u->sam_u = sam_u;

  per_u->imp_s = 256;
  per_u->dir_u = (u3_lane){0,0};
  per_u->ind_u = (u3_lane){0,0};
  per_u->las_u = (u3_peer_last){0,0};
}

// TODO: all the her_p hashtable functions are not refcounted properly

static u3_peer*
_xmas_get_peer_raw(u3_xmas* sam_u, u3_noun her)
{
  u3_peer* ret_u = NULL;
  u3_weak res = u3h_git(sam_u->her_p, her);

  if ( res != u3_none && res != u3_nul ) {
    ret_u = u3to(u3_peer, res);
  }

  u3z(her);
  return ret_u;

}
/*
 * RETAIN
 */
static u3_peer*
_xmas_get_peer(u3_xmas* sam_u, c3_d her_d[2])
{
  return _xmas_get_peer_raw(sam_u, u3i_chubs(2, her_d));
}



/*
 */
static void
_xmas_put_peer_raw(u3_xmas* sam_u, u3_noun her, u3_peer* per_u)
{
  u3_peer* old_u = _xmas_get_peer_raw(sam_u, u3k(her));
  u3_peer* new_u = NULL;

  if ( old_u == NULL ) {
    new_u = u3a_calloc(sizeof(u3_peer),1);
    memcpy(new_u, per_u, sizeof(u3_peer));
  } else if ( new_u != old_u ) {
    new_u = old_u;
    memcpy(new_u, per_u, sizeof(u3_peer));
  }

  u3_noun val = u3of(u3_peer, new_u);
  u3h_put(sam_u->her_p, her, val);
  u3z(her);
}

static void
_xmas_put_peer(u3_xmas* sam_u, c3_d her_d[2], u3_peer* per_u)
{
  _xmas_put_peer_raw(sam_u, u3i_chubs(2, her_d), per_u);
}


static u3_lane
_xmas_get_direct_lane_raw(u3_xmas* sam_u, u3_noun her)
{
  if ( c3y == u3a_is_cat(her) && her < 256 ) {
    c3_s por_s = _ames_czar_port(her);
    return (u3_lane){sam_u->imp_u[her].pip_w, por_s};
  }
  u3_peer* per_u = _xmas_get_peer_raw(sam_u, her);
  if ( NULL == per_u ) {
    return (u3_lane){0,0};
  }
  return per_u->dir_u;
}

static u3_lane
_xmas_get_direct_lane(u3_xmas* sam_u, c3_d her_d[2])
{
  return _xmas_get_direct_lane_raw(sam_u, u3i_chubs(2, her_d));
}


static u3_lane
_xmas_get_indirect_lane(u3_xmas* sam_u, u3_noun her, u3_noun lan)
{
  if ( c3y == u3a_is_cat(her) && her < 256 ) {
    c3_s por_s = _ames_czar_port(her);
    return (u3_lane){sam_u->imp_u[her].pip_w, por_s};
  }
  u3_peer* per_u = _xmas_get_peer_raw(sam_u, her);
  if ( NULL == per_u ) {
    return (u3_lane){0,0};
  }
  return per_u->ind_u;
}


/*
 * RETAIN
 */
static c3_o
_xmas_add_galaxy_pend(u3_xmas* sam_u, u3_noun her, u3_noun pen)
{
  u3_weak old = u3h_get(sam_u->her_p, her);
  u3_noun pes = u3_nul;
  u3_noun wat;
  if ( u3_none != old ) {
    if ( u3h(old) == XMAS_CZAR ) {
      u3x_cell(u3t(old), &pes, &wat);
      u3z(old);
    } else { 
      u3l_log("xmas: attempted to resolve resolved czar");
      u3z(old);
      return c3n;
    }
  }
  u3_noun val = u3nc(u3nc(u3k(pen), u3k(pes)), c3y);
  u3h_put(sam_u->her_p, her, val);
  u3z(val);
  return _(wat);
}

static u3_noun
_name_to_scry(u3_xmas_name* nam_u)
{
  u3_noun rif = _dire_etch_ud(nam_u->rif_w);
  u3_noun boq = _dire_etch_ud(nam_u->boq_y);
  u3_noun fag = _dire_etch_ud(nam_u->fra_w);
  u3_noun pax = _xmas_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);

  u3_noun wer = nam_u->nit_o == c3y 
    ? u3nc(c3__init, pax)
    : u3nt(nam_u->aut_o == c3y ? c3__auth : c3__data, fag, pax);

  u3_noun res = u3nc(c3__mess, u3nq(rif, c3__pact, boq, u3nc(c3__etch, wer)));

  return res;
}

/*
 * RETAIN: feat, cash, and pint
 */

typedef struct _feat_key {
  u3_noun key;
} _feat_key;

static _feat_key
_xmas_feat_key(u3_xmas_name* nam_u)
{
  return (_feat_key){ _xmas_request_key(nam_u) };
}

static u3_pend_req*
_xmas_feat_git(u3_xmas* sam_u, _feat_key fek_u)
{
  u3_weak res = u3h_git(sam_u->req_p, fek_u.key);

  if ( u3_none == res ) {
    return NULL;
  }
  else {
    return u3to(u3_pend_req, res);
  }
}

static void
_xmas_feat_put(u3_xmas* sam_u, _feat_key fek_u, u3_pend_req* req_u)
{
  //  XX assert pointer in loom
  //
  u3_assert(req_u);
  u3_noun val = u3of(u3_pend_req, req_u);
  u3h_put(sam_u->req_p, fek_u.key, val);
}

typedef struct _pact_key {
  u3_noun key;
} _pact_key;

static _pact_key
_xmas_pact_key(u3_xmas_name* nam_u)
{
  return (_pact_key){
    .key = u3nc(u3i_chubs(2, nam_u->her_d),
                _name_to_scry(nam_u))
  };
}

static u3_weak
_xmas_cash_git(u3_xmas* sam_u, _pact_key pek_u)
{
  return u3h_git(sam_u->cac_p, pek_u.key);
}

static void
_xmas_cash_put(u3_xmas* sam_u, _pact_key pek_u, u3_atom pac)
{
  return u3h_put(sam_u->cac_p, pek_u.key, pac);
}

static void
_xmas_cash_put_safe(u3_xmas*  sam_u,
                    _pact_key pek_u,
                    u3_atom     pac,
                    u3_weak     val)
{
  if (  (u3_none != val )
     && (c3n == u3r_sing(pac, val)) )
  {
    u3l_log("xmas: cache collision old=0x%x, new=0x%x",
            u3r_mug(val), u3r_mug(val));
    //  XX keep old?
  }

  return _xmas_cash_put(sam_u, pek_u, pac);
}

static u3_weak
_xmas_pint_git(u3_xmas* sam_u, _pact_key pek_u)
{
  return u3h_git(sam_u->pit_p, pek_u.key);
}

static void
_xmas_pint_put(u3_xmas* sam_u, _pact_key pek_u, u3_noun val)
{
  return u3h_put(sam_u->pit_p, pek_u.key, val);
}

static void
_xmas_pint_del(u3_xmas* sam_u, _pact_key pek_u)
{
  u3h_del(sam_u->pit_p, pek_u.key);
}

static void
_xmas_pint_add(u3_xmas*  sam_u,
               _pact_key pek_u,
               c3_o      our_o,
               u3_weak     lan,
               u3_weak     val)
{
  u3_atom tim = u3i_chub(_get_now_micros());
  u3_noun new;

  //  [first=@da last=@da our=? for=(set lane)]
  //
  if ( u3_none == val ) {
    lan = ( u3_none == lan ) ? u3_nul : u3nt(lan, u3_nul, u3_nul);
    new = u3nq(u3k(tim), tim, our_o, lan);
  }
  else {
    u3_noun tel = u3t(u3t(val));
    u3_noun lis = u3t(tel);
    c3_o  old_o = u3h(tel);

    our_o = ( c3y == our_o ) ? our_o : old_o;
    lis   = ( u3_none == lan )? u3k(lis) : u3qdi_put(lis, lan);

    new = u3nq(u3k(u3h(val)), tim, our_o, lis);
  }

  return _xmas_pint_put(sam_u, pek_u, new);
}

static c3_w
_xmas_respond(u3_xmas_pict* req_u, c3_y** buf_y, u3_noun hit)
{
  c3_w len_w = u3r_met(3, hit);


  *buf_y = c3_calloc(len_w);
  u3r_bytes(0, len_w, *buf_y, hit);

  //u3z(hit);
  return len_w;
}

/* 
 */
static void
_xmas_page_scry_cb(void* vod_p, u3_noun nun)
{
  u3_xmas_pict* pic_u = vod_p;
  u3_xmas_pact* pac_u = &pic_u->pac_u;
  u3_xmas_name* nam_u = &pac_u->pek_u.nam_u; // NB: works for all
  u3_xmas*      sam_u = pic_u->sam_u;

  u3_weak val = u3r_at(7, nun);
  if ( u3_none == val ) {
    // TODO: mark as dead
    //u3z(nun);
    u3l_log("xmas: path unbound");
  }
  else {
    _pact_key pek_u = _xmas_pact_key(nam_u);
    u3_weak     hit = _xmas_cash_git(sam_u, pek_u);

    if ( u3_none != hit ) {
      u3l_log("xmas: page already cached");
    }

    _xmas_cash_put_safe(sam_u, pek_u, u3k(val), hit);

    hit = _xmas_pint_git(sam_u, pek_u);

    if ( u3_none == hit ) {
      u3l_log("xmas: pit entry missing");
    }
    else {
      u3_noun tel = u3t(u3t(hit));
      u3_noun lis = u3t(tel);
      c3_o  our_o = u3h(tel);
      u3_noun lan = u3qdi_tap(lis);

      if ( c3y == our_o ) {
        u3l_log("xmas: strange self request");
      }

      c3_y* buf_y;
      c3_w  len_w = _xmas_respond(pic_u, &buf_y, u3k(val));
      _xmas_rout_bufs(sam_u, buf_y, len_w, lan);

      _xmas_pint_del(sam_u, pek_u);
    }

    u3z(pek_u.key);
  }

  u3z(nun);
}

static void
_xmas_hear_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3l_log("xmas: hear bail");
  c3_w len_w = u3qb_lent(lud);
  u3l_log("len_w: %i", len_w);
  if( len_w == 2 ) {
    u3_pier_punt_goof("hear", u3k(u3h(lud)));
    u3_pier_punt_goof("crud", u3k(u3h(u3t(lud))));
  }
  u3_ovum_free(egg_u);
}


static void
_saxo_cb(void* vod_p, u3_noun nun)
{
  u3_peer* per_u = vod_p;
  u3_weak sax    = u3r_at(7, nun);

  if ( sax != u3_none ) {
    u3_noun her = u3do("head", u3k(sax));
    u3_peer* new_u = _xmas_get_peer_raw(per_u->sam_u, u3k(her));
    if ( new_u != NULL ) {
      per_u = new_u;
    }
    u3_xmas* sam_u = per_u->sam_u;
    u3_noun gal = u3do("rear", u3k(sax));
    u3_assert( c3y == u3a_is_cat(gal) && gal < 256 );
    // both atoms guaranteed to be cats, bc we don't call unless forwarding
    per_u->imp_s = gal;
    _xmas_put_peer_raw(per_u->sam_u, her, per_u);
  }

  u3z(nun);
}

static void
_meet_peer(u3_xmas* sam_u, u3_peer* per_u, c3_d her_d[2])
{
  c3_d now_d = _get_now_micros();
  per_u->las_u.son_d = now_d;

  u3_noun her = u3i_chubs(2, her_d);
  u3_noun gan = u3nc(u3_nul, u3_nul);
  u3_noun pax = u3nc(u3dc("scot", c3__p, her), u3_nul);
  u3_pier_peek_last(sam_u->pir_u, gan, c3__j, c3__saxo, pax, per_u, _saxo_cb);
}

static void
_hear_peer(u3_xmas* sam_u, u3_peer* per_u, u3_lane lan_u, c3_o dir_o)
{
  c3_d now_d = _get_now_micros();
  per_u->las_u.acc_d = now_d;
  if ( c3y == dir_o ) {
    per_u->dir_u = lan_u;
  } else {
    per_u->ind_u = lan_u;
  }
}

static void
_xmas_req_pact_next(u3_xmas* sam_u, u3_pend_req* req_u, u3_lane* lan_u)
{
  u3_assert(req_u);

  c3_w win_w = _xmas_req_get_cwnd(req_u);
  u3_xmas_pict* nex_u = req_u->pic_u;
  c3_w nex_w = req_u->nex_w;
  if ( win_w != 0 ) {
#ifdef XMAS_DEBUG
    u3l_log("continuing flow nex: %u, win: %u", nex_w, win_w);
    u3l_log("in flight %u", bitset_wyt(&req_u->was_u));
#endif
    for ( int i = 0; i < win_w; i++ ) {
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w fra_w = nex_w + i;
      if ( fra_w >= req_u->tot_w ) {
        break;
      }
      nex_u->pac_u.pek_u.nam_u.fra_w = nex_w + i;
      c3_w siz_w  = xmas_etch_pact(buf_y, &nex_u->pac_u);
      if ( siz_w == 0 ) {
        u3l_log("failed to etch");
        u3_assert( 0 );
      }
      // TODO: better route management
      _xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
      _xmas_req_pact_sent(req_u, &nex_u->pac_u.pek_u.nam_u);
    }
  }
}

static u3_pend_req*
_xmas_req_pact_init(u3_xmas* sam_u, u3_xmas_pict* pic_u, u3_lane* lan_u)
{
  u3_xmas_pact* pac_u = &pic_u->pac_u;
  u3_xmas_name* nam_u = &pac_u->pag_u.nam_u;
  u3_xmas_data* dat_u = &pac_u->pag_u.dat_u;
  c3_o lin_o = dat_u->tot_w <= 4 ? c3y : c3n;

  u3_pend_req* req_u = u3a_calloc(1, sizeof(u3_pend_req));
  u3_gage*     gag_u = _xmas_get_lane(sam_u, nam_u->her_d, lan_u);
  
  if ( gag_u == NULL ) {
    gag_u = alloca(sizeof(u3_gage));
    _init_gage(gag_u);
    // save and re-retrieve so we have persistent pointer
    _xmas_put_lane(sam_u, nam_u->her_d, lan_u, gag_u);
    gag_u = _xmas_get_lane(sam_u, nam_u->her_d, lan_u);
    u3_assert( gag_u != NULL );
  }

  req_u->pic_u = c3_calloc(sizeof(u3_xmas_pict));
  req_u->pic_u->sam_u = sam_u;
  req_u->pic_u->pac_u.hed_u.typ_y = PACT_PEEK;
  req_u->pic_u->pac_u.hed_u.pro_y = XMAS_VER;
  memcpy(&req_u->pic_u->pac_u.pek_u.nam_u, nam_u, sizeof(u3_xmas_name));
  req_u->pic_u->pac_u.pek_u.nam_u.aut_o = c3n;
  req_u->pic_u->pac_u.pek_u.nam_u.nit_o = c3n;

  c3_w siz_w = 1 << (pac_u->pag_u.nam_u.boq_y - 3);
  u3_assert( siz_w == 1024 ); // boq_y == 13
  req_u->gag_u = gag_u;
  req_u->dat_y = c3_calloc(siz_w * dat_u->tot_w);
  req_u->wat_u = c3_calloc(sizeof(u3_pact_stat) * dat_u->tot_w + 2 );
  req_u->tot_w = dat_u->tot_w;
  bitset_init(&req_u->was_u, dat_u->tot_w);

  // TODO: handle restart
  // u3_assert( nam_u->fra_w == 0 ); 

  req_u->nex_w = (c3y == lin_o) ? 1 : 0;
  req_u->len_w = (c3y == lin_o) ? 1 : 0;
  uv_timer_init(u3L, &req_u->tim_u);
  req_u->lef_w = 0;
  req_u->old_w = 0;
  req_u->ack_w = 0;

  memcpy(&(req_u->aum_u), &(dat_u->aum_u), sizeof(req_u->aum_u));

  if ( c3y == lin_o ) {
    u3_vec(c3_y[BLAKE3_OUT_LEN])* pof_u = vec_make(8);
    {
      blake_node* nod_u = blake_leaf_hash(dat_u->fra_y, dat_u->len_w, 0);
      c3_y* lef_y = c3_calloc(BLAKE3_OUT_LEN);
      make_chain_value(lef_y, nod_u);
      c3_free(nod_u);
      vec_append(pof_u, lef_y);
    }

    for ( int i = 0; i < pac_u->pag_u.dat_u.aup_u.len_y; i++ ) {
      c3_y* pof_y = c3_calloc(BLAKE3_OUT_LEN);
      memcpy(pof_y, pac_u->pag_u.dat_u.aup_u.has_y[i], BLAKE3_OUT_LEN);
      vec_append(pof_u, pof_y);
    }

    req_u->bao_u = blake_bao_make(req_u->tot_w, pof_u);
     // XX unchecked, failure here needs to cause a packet drop
    //
    blake_bao_verify(req_u->bao_u, dat_u->fra_y, dat_u->len_w, NULL);
    memcpy(req_u->dat_y, dat_u->fra_y, dat_u->len_w);
  } else {
    c3_w len_w = dat_u->len_w / BLAKE3_OUT_LEN;
    u3_vec(c3_y[BLAKE3_OUT_LEN])* pof_u = vec_make(len_w);
    for ( int i = 0; i < len_w; i++ ) {
      c3_y* pof_y = c3_calloc(BLAKE3_OUT_LEN);
      memcpy(pof_y, dat_u->fra_y + (BLAKE3_OUT_LEN*i), BLAKE3_OUT_LEN);
      vec_append(pof_u, pof_y);
    }
    req_u->bao_u = blake_bao_make(req_u->tot_w, pof_u);
  }
  vec_init(&req_u->mis_u, 8);

  // req_u = _xmas_put_request(sam_u, nam_u, req_u);
  return req_u;
}

/* _xmas_plan_mess(): construct and enqueue [%mess %page] ovum.
*/
static u3_ovum*
_xmas_plan_mess(u3_xmas*      sam_u,
                u3_xmas_auth* aum_u,
                u3_xmas_name* nam_u,
                u3_lane*      lan_u,
                c3_w          len_w,
                c3_y*         buf_y)
{
  u3_noun wir = u3nc(c3__xmas, u3_nul);
  u3_noun aut, cad;

  switch ( aum_u->typ_e ) {
    case AUTH_SIGN: {
      aut = u3nc(c3y, u3i_bytes(64, aum_u->sig_y));
    } break;

    case AUTH_HMAC: {
      aut = u3nc(c3n, u3i_bytes(32, aum_u->mac_y));
    } break;

    default: u3_assert(0);
  }

  {
    u3_noun pax = _xmas_encode_path(nam_u->pat_s,
                            (c3_y*)(nam_u->pat_c));
    u3_noun par = u3nc(u3i_chubs(2, nam_u->her_d), pax);
    u3_noun lan = u3nc(u3_nul, u3_xmas_encode_lane(*lan_u));
    u3_noun dat = u3i_bytes(len_w, buf_y);

    cad = u3nt(c3__mess, lan,
               u3nq(c3__page, par, aut, dat));
  }
  return u3_auto_plan(&sam_u->car_u,
                      u3_ovum_init(0, c3__m, wir, cad));
}

static void
_xmas_hear_page(u3_xmas_pict* pic_u, u3_lane* lan_u)
{
#ifdef XMAS_DEBUG
  u3l_log("xmas hear %%page %u", pic_u->pac_u.pag_u.nam_u.fra_w);
#endif
  u3_xmas* sam_u = pic_u->sam_u;
  u3_xmas_pact* pac_u = &pic_u->pac_u;
  u3_xmas_name *nam_u = &pac_u->pag_u.nam_u;
  u3_xmas_data* dat_u = &pac_u->pag_u.dat_u;

  // XX move, more validation response before anything else (bloq size)
  //
  if ( 13 != nam_u->boq_y ) {
    u3l_log("page: strange bloq size %u", nam_u->boq_y);
    _xmas_free_pict(pic_u);
    return;
  }
  if ( c3y == nam_u->nit_o ) {
    switch ( dat_u->aum_u.typ_e ) {
      case AUTH_SIGN:
      case AUTH_HMAC: break;

      default: {
        u3l_log("page: strange auth on first packet");
        _xmas_free_pict(pic_u);
        return;
      }
    }
  }
  else if ( c3y == nam_u->aut_o ) {
    u3l_log("page: strange auth packet");
    _xmas_free_pict(pic_u);
    return;
  }

  //  XX this peer discovery is completely fake
  //
  {
    u3_peer* per_u = _xmas_get_peer(sam_u, nam_u->her_d);
    c3_o new_o = c3n;
    if ( NULL == per_u ) {
      new_o = c3y;
      per_u = c3_calloc(sizeof(u3_peer));
      _init_peer(sam_u, per_u);
      _meet_peer(sam_u, per_u, nam_u->her_d);
    }

    c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
    if ( pac_u->hed_u.hop_y == 0 ) {
      _hear_peer(sam_u, per_u, *lan_u, dir_o);
    } else {
      u3l_log("received forwarded page");
    }
    if ( new_o == c3y ) {
      //u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
      //_log_lane(&lan_u);
    }

    _xmas_put_peer(sam_u, nam_u->her_d, per_u);
  }

  // _pact_key pek_u = _xmas_pact_key(nam_u);
  // u3m_p("page", pek_u.key);

  // XX if response to our request
  //      req_plan_init/req_plan_done
  //    if request in PIT, send response to all lanes

  _feat_key fek_u = _xmas_feat_key(nam_u);
  u3_pend_req* req_u = _xmas_feat_git(sam_u, fek_u);

  //  we're in the middle of this message
  //
  if ( req_u && (c3n == nam_u->nit_o) ) {
    //  push packet into feat
    //  if fails to validate, drop it
    //
    //    XX cache if it "validates"?
    //
    req_u = _xmas_req_pact_done(sam_u, req_u, nam_u, dat_u, lan_u);
    if ( req_u == NULL ) {
      u3z(fek_u.key);
      _xmas_free_pict(pic_u);
      return;
    }

    _xmas_req_pact_next(sam_u, req_u, lan_u);

    if ( req_u->len_w == req_u->tot_w ) {
      // fprintf(stderr, "finished");
      // u3l_log("queue size %u", req_u->mis_u.len_w);
      c3_d now_d = _get_now_micros();
      u3l_log("%u kilobytes took %f ms", req_u->tot_w, (now_d - sam_u->tim_d)/1000.0);
      c3_w siz_w = (1 << (nam_u->boq_y - 3));

      _xmas_plan_mess(sam_u, &req_u->aum_u, nam_u,
                      lan_u, (siz_w * req_u->tot_w), req_u->dat_y);

      // XX shouldn't delete request till success
      _xmas_del_request(sam_u, nam_u);
    }
  }

  _pact_key pek_u = _xmas_pact_key(nam_u);
  u3_weak     hit = _xmas_pint_git(sam_u, pek_u);

  if ( u3_none != hit ) {
    u3_noun tel = u3t(u3t(hit));
    u3_noun lis = u3t(tel);
    c3_o  our_o = u3h(tel);

    if ( c3y == our_o ) {
      if ( c3y == nam_u->nit_o ) {
        if ( 1 == dat_u->tot_w ) {
          //  XX should put in cache on success
          //
          _xmas_plan_mess(sam_u, &dat_u->aum_u, nam_u,
                          lan_u, dat_u->len_w, dat_u->fra_y);
        }
        else if ( !req_u ) {
          // XX init request
          req_u = _xmas_req_pact_init(sam_u, pic_u, lan_u);
          if ( req_u == NULL ) {
            u3z(fek_u.key);
            u3z(pek_u.key);
            _xmas_free_pict(pic_u);
            return;
          }

          _xmas_feat_put(sam_u, fek_u, req_u);

          _xmas_req_pact_next(sam_u, req_u, lan_u);
        }
      }
      else if ( !req_u ) {
        u3l_log("xmas: strange PIT entry for non init");
        // XX wat do?
      }
    }

    //  XX reuse buffer, update hopcount, append lane
    {
      u3_noun lan = u3qdi_tap(lis);
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w  len_w = xmas_etch_pact(buf_y, &pic_u->pac_u);

      u3_assert( len_w );

      _xmas_rout_bufs(sam_u, buf_y, len_w, lan);
    }

    _xmas_pint_del(sam_u, pek_u);
  }


  u3z(fek_u.key);
  u3z(pek_u.key);
  _xmas_free_pict(pic_u);
}

/* _xmas_plan_peek(): scry for a packet.
*/
static void
_xmas_plan_peek(u3_xmas_pict* pic_u)
{
  u3_xmas*      sam_u = pic_u->sam_u;
  u3_pier*      pir_u = sam_u->car_u.pir_u;
  u3_xmas_name* nam_u = &pic_u->pac_u.pek_u.nam_u;

  u3_noun bem = u3nc(u3nt(u3i_chubs(2, nam_u->her_d),
                          u3_nul,
                          u3nc(c3__ud, 1)),
                     _name_to_scry(nam_u));
  u3_noun sam = u3nq(c3n, c3__beam, c3__mx, bem);

  u3_pier_peek(pir_u, u3_nul, sam, pic_u, _xmas_page_scry_cb);
}

static void
_xmas_poke_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_xmas_pict* pic_u = egg_u->ptr_v;

  if ( u3_ovum_done == new_e ) {
    // XX update peer state
    // XX initiate payload request if >1 fragment
    u3l_log("xmas: poke success");
  }
}

static void
_xmas_poke_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3_xmas_pict* pic_u = egg_u->ptr_v;
  // XX failure stuff here
  u3l_log("xmas: poke failure");
}


/* _xmas_plan_poke(): queue an ovum for a %poke a packet.
*/
static void
_xmas_plan_poke(u3_xmas_pict* pic_u, u3_lane* lan_u)
{
  u3_xmas*      sam_u = pic_u->sam_u;
  u3_xmas_pact* pac_u = &pic_u->pac_u;

  u3_noun wir = u3nc(c3__xmas, u3_nul);
  u3_noun cad;
  {
    u3_noun    lan = u3_xmas_encode_lane(*lan_u);
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, 3, PACT_SIZE);

    //  XX should just preserve input buffer
    xmas_etch_pact(sab_u.buf_y, pac_u);

    cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));
  }

  u3_ovum_peer nes_f;
  u3_ovum_bail bal_f;
  void*        ptr_v;

  if ( 1 == pac_u->pok_u.dat_u.tot_w ) {
    nes_f = bal_f = ptr_v = NULL;
    _xmas_free_pict(pic_u);
  }
  else {
    assert(pac_u->pok_u.dat_u.tot_w);
    //  XX check request state for *payload* (in-progress duplicate)
    nes_f = _xmas_poke_news;
    bal_f = _xmas_poke_bail;
    ptr_v = pic_u;
  }

  u3_auto_peer(
    u3_auto_plan(&sam_u->car_u,
                 u3_ovum_init(0, c3__m, wir, cad)),
    ptr_v, nes_f, bal_f);

  //  XX defer lane management stuff to success callback (once vane works)
  //
  {
    u3_peer* per_u = _xmas_get_peer(sam_u, pac_u->pok_u.pay_u.her_d);
    c3_o new_o = c3n;
    if ( NULL == per_u ) {
      new_o = c3y;
      per_u = c3_calloc(sizeof(u3_peer));
      _init_peer(sam_u, per_u);
      _meet_peer(sam_u, per_u, pac_u->pok_u.pay_u.her_d);
    }

    c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
    if ( pac_u->hed_u.hop_y == 0 ) {
      new_o = c3y;
      _hear_peer(sam_u, per_u, *lan_u, dir_o);
      u3l_log("learnt lane");
    } else {
      u3l_log("received forwarded poke");
    }
    if ( new_o == c3y ) {
      u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
      _log_lane(lan_u);
    }
    _xmas_put_peer(sam_u, pac_u->pok_u.pay_u.her_d, per_u);
  }
}

/* _xmas_hear_want(): handle a request (%peek or %poke) packet.
*/
static void
_xmas_hear_want(u3_xmas_pict* pic_u, u3_lane* lan_u)
{
#ifdef XMAS_DEBUG
  u3l_log("xmas: hear %%%s",
          (PACT_PEEK == pic_u->pac_u.hed_u.typ_y) ? "peek" : "poke");
#endif

  u3_xmas*      sam_u = pic_u->sam_u;
  u3_xmas_pact* pac_u = &pic_u->pac_u;
  u3_xmas_name* nam_u = &pac_u->pek_u.nam_u;  // NB: works for both types

  c3_d* her_d = nam_u->her_d;
  c3_o  our_o = __( 0 == memcmp(her_d, sam_u->pir_u->who_d, sizeof(*her_d) * 2) );

  //  XX validate request before anything else (bloq size)
  //  - bloq = 13
  //  - nit_o || !aut_o

  _pact_key pek_u = _xmas_pact_key(nam_u);
  u3_weak     hit = _xmas_cash_git(sam_u, pek_u);

  // u3m_p("want", pek_u.key);

  //  if cached, give response
  //
  if ( u3_none != hit ) {
    // XX send from cache
    // XX respond synchronously, reusing pact/lane
    // NB: hopcount should already be correct in cache
#if XMAS_DEBUG
    u3l_log("xmas: hit cache mug=%x", u3r_mug(pek_u.key));
#endif
    c3_y* buf_y;
    c3_w len_w = _xmas_respond(pic_u, &buf_y, hit);
    _xmas_send_buf(sam_u, *lan_u, buf_y, len_w);
  }
  else {
    hit = _xmas_pint_git(sam_u, pek_u);

    //  if ours, track requester lane
    //
    if ( c3y == our_o ) {
      _xmas_pint_add(sam_u, pek_u, c3n, u3_xmas_encode_lane(*lan_u), hit);

      //  suppress duplicates
      //
      if ( u3_none == hit ) {
#if XMAS_DEBUG
        u3l_log("xmas: plan mug=%x", u3r_mug(pek_u.key));
#endif
        if ( PACT_PEEK == pic_u->pac_u.hed_u.typ_y ) {
          _xmas_plan_peek(pic_u);
        }
        else {
          _xmas_plan_poke(pic_u, lan_u);
        }

        u3z(pek_u.key);
        return;  // retain pic_u;
      }
      else {
#if XMAS_DEBUG
        u3l_log("xmas: add to PIT mug=%x", u3r_mug(pek_u.key));
#endif
      }
    }
    //  if we are relaying and have a route, track requester lane
    //
    //    NB: duplicate requests *not* suppressed
    //
    else if ( c3y == sam_u->for_o ) {
      // XX can't presume we have a route
      // XX reuse buf and forward synchronously
      //
      u3_lane lin_u = _xmas_get_direct_lane(sam_u, her_d);
      //_update_hopcount(&pac_u->hed_u);
#ifdef XMAS_DEBUG
      u3l_log("xmas: forwarding mug=%x to=%u.%u.%u.%u:%u",
              u3r_mug(pek_u.key),
              (lin_u.pip_w >> 24) & 0xff,
              (lin_u.pip_w >> 16) & 0xff,
              (lin_u.pip_w >>  8) & 0xff,
              (lin_u.pip_w >>  0) & 0xff,
              lin_u.pip_w);
#endif
      _xmas_send(pic_u, &lin_u);

      _xmas_pint_add(sam_u, pek_u, c3n, u3_xmas_encode_lane(*lan_u), hit);
    }
  }

  u3z(pek_u.key);
  _xmas_free_pict(pic_u);
}

static void
_xmas_hear(u3_xmas* sam_u,
           u3_lane* lan_u,
           c3_w     len_w,
           c3_y*    hun_y)
{
  u3_xmas_pict* pic_u;
  c3_w pre_w;
  c3_y* cur_y = hun_y;
  if ( HEAD_SIZE > len_w ) {
    c3_free(hun_y);
    return;
  }

  pic_u = c3_calloc(sizeof(u3_xmas_pict));
  pic_u->sam_u = sam_u;
  c3_w lin_w = xmas_sift_pact(&pic_u->pac_u, hun_y, len_w);
  c3_free(hun_y);
  if ( lin_w == 0 ) {
    XMAS_LOG(SERIAL)
    // XX revisit
    // xmas_free_pact(&pic_u->pac_u);
    c3_free(pic_u);
    return;
  }

  switch ( pic_u->pac_u.hed_u.typ_y ) {
    case PACT_PAGE: {
      _xmas_hear_page(pic_u, lan_u);
    } break;

    default: {
      _xmas_hear_want(pic_u, lan_u);
    } break;
  }
}

static void
_xmas_recv_cb(uv_udp_t*        wax_u,
              ssize_t          nrd_i,
              const uv_buf_t * buf_u,
              const struct sockaddr* adr_u,
              unsigned         flg_i)
{
  if ( 0 > nrd_i ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("xmas: recv: fail: %s", uv_strerror(nrd_i));
    }
    c3_free(buf_u->base);
  }
  else if ( 0 == nrd_i ) {
    c3_free(buf_u->base);
  }
  else if ( flg_i & UV_UDP_PARTIAL ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("xmas: recv: fail: message truncated");
    }
    c3_free(buf_u->base);
  }
  else {
    u3_xmas*            sam_u = wax_u->data;
    struct sockaddr_in* add_u = (struct sockaddr_in*)adr_u;
    u3_lane             lan_u;


    lan_u.por_s = ntohs(add_u->sin_port);
   // u3l_log("port: %s", lan_u.por_s);
    lan_u.pip_w = ntohl(add_u->sin_addr.s_addr);
  //  u3l_log("IP: %x", lan_u.pip_w);
    //  NB: [nrd_i] will never exceed max length from _ames_alloc()
    //
    _xmas_hear(sam_u, &lan_u, (c3_w)nrd_i, (c3_y*)buf_u->base);
  }
}



static void
_xmas_io_talk(u3_auto* car_u)
{
  u3_xmas* sam_u = (u3_xmas*)car_u;
  sam_u->dns_c = "urbit.org"; // TODO: receive turf
  {
    //  XX remove [sev_l]
    //
    u3_noun wir = u3nt(c3__xmas,
                       u3dc("scot", c3__uv, sam_u->sev_l),
                       u3_nul);
    u3_noun cad = u3nc(c3__born, u3_nul);

    u3_auto_plan(car_u, u3_ovum_init(0, c3__m, wir, cad));
  }
  u3_noun    who = u3i_chubs(2, sam_u->pir_u->who_d);
  u3_noun    rac = u3do("clan:title", u3k(who));
  c3_s     por_s = sam_u->pir_u->por_s;
  c3_i     ret_i;
  if ( c3__czar == rac ) {
    c3_y num_y = (c3_y)sam_u->pir_u->who_d[0];
    c3_s zar_s = _ames_czar_port(num_y);

    if ( 0 == por_s ) {
      por_s = zar_s;
    }
    else if ( por_s != zar_s ) {
      u3l_log("ames: czar: overriding port %d with -p %d", zar_s, por_s);
      u3l_log("ames: czar: WARNING: %d required for discoverability", zar_s);
    }
  }


  //  Bind and stuff.
  {
    struct sockaddr_in add_u;
    c3_i               add_i = sizeof(add_u);

    memset(&add_u, 0, sizeof(add_u));
    add_u.sin_family = AF_INET;
    add_u.sin_addr.s_addr = _(u3_Host.ops_u.net) ?
                              htonl(INADDR_ANY) :
                              htonl(INADDR_LOOPBACK);
    add_u.sin_port = htons(por_s);

    if ( (ret_i = uv_udp_bind(&sam_u->wax_u,
                              (const struct sockaddr*)&add_u, 0)) != 0 )
    {
      u3l_log("xmas: bind: %s", uv_strerror(ret_i));

      /*if ( (c3__czar == rac) &&
           (UV_EADDRINUSE == ret_i) )
      {
        u3l_log("    ...perhaps you've got two copies of vere running?");
      }*/

      //  XX revise
      //
      u3_pier_bail(u3_king_stub());
    }

    uv_udp_getsockname(&sam_u->wax_u, (struct sockaddr *)&add_u, &add_i);
    u3_assert(add_u.sin_port);

    sam_u->pir_u->por_s = ntohs(add_u.sin_port);
  }
  if ( c3y == u3_Host.ops_u.net ) {
    u3l_log("xmas: live on %d", sam_u->pir_u->por_s);
  }
  else {
    u3l_log("xmas: live on %d (localhost only)", sam_u->pir_u->por_s);
  }

  uv_udp_recv_start(&sam_u->wax_u, _ames_alloc, _xmas_recv_cb);

  sam_u->car_u.liv_o = c3y;
  //u3z(rac); u3z(who);
}

static void
_xmas_ef_send(u3_xmas* sam_u, u3_noun las, u3_noun pac)
{
  las = _xmas_queue_czar(sam_u, las, u3k(pac));
  c3_w  len_w = u3r_met(3, pac);
  c3_y* buf_y = c3_calloc(len_w);
  u3r_bytes(0, len_w, buf_y, pac);
  sam_u->tim_d = _get_now_micros();

  {
    u3_xmas_pact pac_u;
    u3_xmas_name* nam_u = &pac_u.pek_u.nam_u; // NB: works all types

    if ( 0 == xmas_sift_pact(&pac_u, buf_y, len_w) ) {
      u3l_log("xmas: strange send");
    }
    else {
      _pact_key pek_u = _xmas_pact_key(nam_u);
      u3_weak     hit = _xmas_pint_git(sam_u, pek_u);

      switch ( pac_u.hed_u.typ_y ) {
        case PACT_PAGE: {
#if XMAS_DEBUG
          u3l_log("xmas: send %%page");
#endif

          if ( u3_none != hit ) {
            u3_noun tel = u3t(u3t(hit));
            u3_noun lis = u3t(tel);
            c3_o  our_o = u3h(tel);

            if ( c3y == our_o ) {
              u3l_log("xmas: strange self request (gift)");
            }

            //  unify (list lane) from effect with (set lane) in PIT
            //
            {
              u3_noun lus = u3qdi_gas(lis, las);;
              u3z(las);
              las = u3kdi_tap(lus);
            }
          }

          _xmas_pint_del(sam_u, pek_u);
          _xmas_cash_put(sam_u, pek_u, u3k(pac));
        } break;

        default: {
#if XMAS_DEBUG
          u3l_log("xmas: send %%%s",
                  (PACT_PEEK == pac_u.hed_u.typ_y) ? "peek" : "poke");
#endif
          _xmas_pint_add(sam_u, pek_u, c3y, u3_none, hit);
        } break;
      }

      u3z(pek_u.key);
      xmas_free_pact(&pac_u);
    }
  }

  c3_o suc_o = c3n;
  _xmas_rout_bufs(sam_u, buf_y, len_w, las);

  c3_free(buf_y);
  u3z(pac);
}

static c3_o
_xmas_kick(u3_xmas* sam_u, u3_noun tag, u3_noun dat)
{
  c3_o ret_o;

  switch ( tag ) {
    default: {
      ret_o = c3n;
     } break;
    case c3__send: {
      u3_noun las, pac;
      if ( c3n == u3r_cell(dat, &las, &pac) ) {
        ret_o = c3n;
      } else {
        _xmas_ef_send(sam_u, u3k(las), u3k(pac));
        ret_o = c3y;
      }
    } break;
  }

  // technically losing tag is unncessary as it always should
  // be a direct atom, but better to be strict
  u3z(dat); u3z(tag);
  return ret_o;
}

static c3_o
_xmas_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_xmas* sam_u = (u3_xmas*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3__xmas != i_wir)
     || (c3n == u3r_cell(cad, &tag, &dat)) )
  {
    ret_o = c3n;
  }
  else {
    ret_o = _xmas_kick(sam_u, u3k(tag), u3k(dat));
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

static u3_noun
_xmas_io_info(u3_auto* car_u)
{

  return u3_nul;
}

static void
_xmas_io_slog(u3_auto* car_u) {
  u3l_log("xmas is online");
}

static void
_xmas_exit_free(u3_noun kev)
{
  u3_post val = u3t(kev);

  if ( val ) {
    u3a_free(u3a_into(val));
  }
}

static void
_xmas_exit_cb(uv_handle_t* had_u)
{
  u3_xmas* sam_u = had_u->data;

  u3h_free(sam_u->cac_p);
  u3h_free(sam_u->pit_p);

  u3h_walk(sam_u->her_p, _xmas_exit_free);
  u3h_free(sam_u->her_p);
  u3h_walk(sam_u->lan_p, _xmas_exit_free);
  u3h_free(sam_u->lan_p);
  //  XX refactor _xmas_del_request(), walk
  u3h_free(sam_u->req_p);

  c3_free(sam_u);
}

static void
_xmas_io_exit(u3_auto* car_u)
{
  u3_xmas* sam_u = (u3_xmas*)car_u;
  uv_close(&sam_u->had_u, _xmas_exit_cb);
}

/* _xmas_io_init(): initialize ames I/O.
*/
u3_auto*
u3_xmas_io_init(u3_pier* pir_u)
{
  u3_xmas* sam_u  = c3_calloc(sizeof(*sam_u));
  sam_u->pir_u    = pir_u;

  //  these store pointers, not cap-able
  sam_u->her_p = u3h_new();
  sam_u->req_p = u3h_new();
  sam_u->lan_p = u3h_new();

  //  XX config for max entries
  //
  sam_u->cac_p = u3h_new_cache(100000);
  sam_u->pit_p = u3h_new_cache(10000);

  u3_assert( !uv_udp_init(u3L, &sam_u->wax_u) );
  sam_u->wax_u.data = sam_u;

  //  Disable networking for fake ships
  //
  if ( c3y == sam_u->pir_u->fak_o ) {
    u3_Host.ops_u.net = c3n;
  }

  sam_u->for_o = c3n;
  {
    u3_noun her = u3i_chubs(2, pir_u->who_d);
    for (int i = 0; i < 256; i++) {
      sam_u->imp_u[i].pen = u3_nul;
      sam_u->imp_u[i].sam_u = sam_u;
      sam_u->imp_u[i].imp_y = i;
      //sam_u.imp_u[i].tim = 0;
      if ( u3_Host.ops_u.net == c3n ) {
        sam_u->imp_u[i].pip_w = 0x7f000001;
      } else {
        sam_u->imp_u[i].pip_w = 0;
      }
    }

    if ( c3y == u3a_is_cat(her) && her < 256 ) {
      u3l_log("xmas: forwarding enabled");
      sam_u->for_o = c3y;
    }
    u3z(her);
  }


  u3_auto* car_u = &sam_u->car_u;
  car_u->nam_m = c3__xmas;
  car_u->liv_o = c3y;
  car_u->io.talk_f = _xmas_io_talk;
  car_u->io.info_f = _xmas_io_info;
  car_u->io.slog_f = _xmas_io_slog;
  car_u->io.kick_f = _xmas_io_kick;
  car_u->io.exit_f = _xmas_io_exit;



  /*{
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now = u3_time_in_tv(&tim_u);
    //sam_u->sev_l = u3r_mug(now);
    u3z(now);
  }*/

  return car_u;
}
