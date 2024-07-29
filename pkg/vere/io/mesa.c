/// @file

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"
#include "ship.h"
#include "mesa/mesa.h"
#include "mesa/bitset.h"
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
#include "lss.h"

c3_o dop_o = c3n;

#define MESA_DEBUG     c3y
//#define MESA_TEST
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
#define REORDER_THRESH  5

#define DIRECT_ROUTE_TIMEOUT_MICROS 5000000
#define DIRECT_ROUTE_RETRY_MICROS   1000000

// logging and debug symbols
#define MESA_SYM_DESC(SYM) MESA_DESC_ ## SYM
#define MESA_SYM_FIELD(SYM) MESA_FIELD_ ## SYM
#ifdef MESA_DEBUG
  #define MESA_LOG(SAM_U, SYM, ...) { SAM_U->sat_u.MESA_SYM_FIELD(SYM)++; u3l_log("mesa: (%u) %s", __LINE__, MESA_SYM_DESC(SYM)); }
#else
  #define MESA_LOG(SAM_U, SYM, ...) { SAM_U->sat_u.MESA_SYM_FIELD(SYM)++; }
#endif

typedef struct _u3_mesa_stat {
  c3_w dop_w;  // dropped for other reasons
  c3_w ser_w;  // dropped for serialisation
  c3_w aut_w;  // droppped for auth
  c3_w apa_w;  // dropped bc no interest
  c3_w inv_w;  // non-fatal invariant violation
  c3_w dup_w;  // duplicates
} u3_mesa_stat;

#define MESA_DESC_STRANGE "dropped strange packet"
#define MESA_FIELD_STRANGE dop_w

#define MESA_DESC_SERIAL "dropped packet (serialisation)"
#define MESA_FIELD_SERIAL ser_w

#define MESA_DESC_AUTH "dropped packet (authentication)"
#define MESA_FIELD_AUTH aut_w

#define MESA_DESC_INVARIANT "invariant violation"
#define MESA_FIELD_INVARIANT inv_w

#define MESA_DESC_APATHY "dropped packet (no interest)"
#define MESA_FIELD_APATHY apa_w

#define MESA_DESC_DUPE "dropped packet (duplicate)"
#define MESA_FIELD_DUPE dup_w

#define IN_FLIGHT  10

// XX
#define MESA_HUNK  16384  //  184

// pending interest sentinels
#define MESA_ITEM         1  // cached item
#define MESA_WAIT         2  // waiting on scry

// routing table sentinels
#define MESA_CZAR         1  // pending dns lookup
#define MESA_ROUT         2  // have route
//
// hop enum

#define SIFT_VAR(dest, src, len) dest = 0; for(int i = 0; i < len; i++ ) { dest |= ((src + i) >> (8*i)); }
#define CHECK_BOUNDS(cur) if ( len_w < cur ) { u3l_log("mesa: failed parse (%u,%u) at line %i", len_w, cur, __LINE__); return 0; }
#define safe_dec(num) (num == 0 ? num : num - 1)
#define _mesa_met3_w(a_w) ((c3_bits_word(a_w) + 0x7) >> 3)

struct _u3_mesa_pact;

typedef struct _u3_pact_stat {
  c3_d  sen_d; // last sent
  c3_y  sip_y; // skips
  c3_y  tie_y; // tries
} u3_pact_stat;

struct _u3_mesa;

typedef struct _u3_misord_buf {
  c3_y*     fra_y;
  c3_w      len_w;
  lss_pair* par_u;
} u3_misord_buf;

typedef struct _u3_gage {
  c3_w     rtt_w;  // rtt
  c3_w     rto_w;  // rto
  c3_w     rtv_w;  // rttvar
  c3_w     wnd_w;  // cwnd
  c3_w     wnf_w;  // cwnd fraction
  c3_w     sst_w;  // ssthresh
  c3_w     con_w;  // counter
  //
  uv_timer_t tim_u;
} u3_gage;

struct _u3_mesa;

typedef struct _u3_mesa_pict {
  uv_udp_send_t      snd_u;
  struct _u3_mesa*   sam_u;
  u3_mesa_pact       pac_u;
} u3_mesa_pict;

typedef struct _u3_czar_info {
  c3_w               pip_w; // IP of galaxy
  c3_y               imp_y; // galaxy number
  struct _u3_mesa*   sam_u; // backpointer
  time_t             tim_t; // time of retrieval
  c3_c*              dns_c; // domain
  u3_noun            pen;   // (list @) of pending packet
} u3_czar_info;

typedef struct _u3_lane_state {
  c3_d  sen_d;  //  last sent date
  c3_d  her_d;  //  last heard date
  c3_w  rtt_w;  //  round-trip time
  c3_w  rtv_w;  //  round-trip time variance
} u3_lane_state;

/* _u3_mesa: next generation networking
 */
typedef struct _u3_mesa {
  u3_auto            car_u;
  u3_pier*           pir_u;
  union {
    uv_udp_t         wax_u;
    uv_handle_t      had_u;
  };
  u3_mesa_stat       sat_u;       //  statistics
  c3_l               sev_l;       //  XX: ??
  c3_o               for_o;       //  is forwarding
  ur_cue_test_t*     tes_u;       //  cue-test handle
  u3_cue_xeno*       sil_u;       //  cue handle
  u3p(u3h_root)      her_p;       //  (map ship u3_peer)
  u3p(u3h_root)      pac_p;       //  packet cache
  u3p(u3h_root)      lan_p;       //  lane cache
  u3p(u3h_root)      pit_p;       //  (map path [our=? las=(set lane)])
  u3_czar_info       imp_u[256];  //  galaxy information
  c3_c*              dns_c;       //  turf (urb.otrg)
  c3_d               tim_d;       //  XX: remove
} u3_mesa;

typedef struct _u3_peer {
  u3_mesa*       sam_u;  //  backpointer
  c3_o           ful_o;  //  has this been initialized?
  u3_lane        dan_u;  //  direct lane (nullable)
  u3_lane_state  dir_u;  //  direct lane state
  c3_y           imp_y;  //  galaxy @p
  u3_lane_state  ind_u;  //  indirect lane state
  u3p(u3h_root)  req_p;  //  (map [rift path] u3_pend_req)
} u3_peer;

typedef struct _u3_pend_req {
  u3_peer*               per_u; // backpointer
  c3_w                   nex_w; // number of the next fragment to be sent
  c3_w                   tot_w; // total number of fragments expected
  u3_auth_data           aum_u; // message authenticator
  uv_timer_t             tim_u; // timehandler
  c3_y*                  dat_y; // ((mop @ud *) lte)
  c3_w                   len_w;
  c3_w                   lef_w; // lowest fragment number currently in flight/pending
  c3_w                   old_w; // frag num of oldest packet sent
  c3_w                   ack_w; // highest acked fragment number
  u3_gage*               gag_u; // congestion control
  u3_misord_buf          mis_u[8]; // misordered packets
  lss_verifier*          los_u; // Lockstep verifier
  u3_mesa_pict*          pic_u; // preallocated request packet
  u3_pact_stat*          wat_u; // ((mop @ud packet-state) lte)
  u3_bitset              was_u; // ((mop @ud ?) lte)
  c3_y                   pad_y[64];
  //  stats TODO: use
  c3_d                   beg_d; // date when request began
} u3_pend_req;

typedef enum _u3_mesa_ctag {
  CACE_WAIT = 1,
  CACE_ITEM = 2,
} u3_mesa_ctag;

/*
 *  typedef u3_mesa_req u3_noun
 *  [tot=@ waiting=(set @ud) missing=(set @ud) nex=@ud dat=(map @ud @)]
 */


typedef struct _u3_cace_enty {
  u3_mesa_ctag      typ_y;
  union {
    // u03_mesa_cace_wait  wat_u;
    // u3_mesa_cace_item  res_u;
  };
} u3_cace_enty;

// Sent request
//   because of lifecycles a u3_mesa_pact may have several libuv
//   callbacks associated with it, so we can't those as callback
//   instead just alloc new buffer and stick here
typedef struct _u3_seal {
  uv_udp_send_t    snd_u;  // udp send request
  u3_mesa*         sam_u;
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
  u3l_log("mesa: lane (%s,%u)", u3r_string(u3dc("scot", c3__if, u3i_word(lan_u->pip_w))), lan_u->por_s);
}

static void _log_peer(u3_peer* per_u)
{
  if ( per_u == NULL ) {
    u3l_log("NULL peer");
    return;
  }
  u3l_log("dir");
  _log_lane(&per_u->dan_u);
  u3l_log("galaxy: %s", u3r_string(u3dc("scot", 'p', per_u->imp_y)));
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

static void
_log_mesa_data(u3_mesa_data dat_u)
{
  u3l_log("total frag: %u", dat_u.tot_w);
  u3l_log("frag len: %u", dat_u.len_w);
  // u3l_log("frag: %xxx", dat_u.fra_y);
}

/* _mesa_lop(): find beginning of page containing fra_w
*/
static inline c3_w
_mesa_lop(c3_w fra_w)
{
  return 0;
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
  /* u3l_log("clamp rto %llu", rto_d); */
  return c3_min(c3_max(rto_d, 200000), 25000000);
}

static inline c3_o
_mesa_is_lane_zero(u3_lane* lan_u)
{
  return __(0 == lan_u->pip_w && 0 == lan_u->por_s);
}

static c3_o
_mesa_is_direct_mode(u3_peer* per_u)
{
  c3_d now_d = _get_now_micros();
  return __(per_u->dir_u.her_d + DIRECT_ROUTE_TIMEOUT_MICROS > now_d);
}

static inline void
_get_her(u3_mesa_pact* pac_u, c3_d* our_d)
{
  switch ( pac_u->hed_u.typ_y ) {
    default: {
      u3m_bail(c3__foul);
      break;
    }
    case PACT_PAGE: {
      memcpy(our_d, pac_u->pag_u.nam_u.her_u, 16);
      break;
    }
    case PACT_PEEK: {
      memcpy(our_d, pac_u->pek_u.nam_u.her_u, 16);
      break;
    }
    case PACT_POKE: {
      memcpy(our_d, pac_u->pok_u.nam_u.her_u, 16);
      break;
    }
  }
}

static c3_c*
_mesa_czar_dns(c3_y imp_y, c3_c* zar_c)
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

/*  _mesa_encode_path(): produce buf_y as a parsed path
*/
static u3_noun
_mesa_encode_path(c3_w len_w, c3_y* buf_y)
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
_mesa_free_pict(u3_mesa_pict* pic_u)
{
  mesa_free_pact(&pic_u->pac_u);
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

/* _mesa_request_key(): produce key for request hashtable sam_u->req_p from nam_u
*/
u3_noun _mesa_request_key(u3_mesa_name* nam_u)
{
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);
  u3_noun res = u3nc(u3i_word(nam_u->rif_w), pax);
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

/* u3_mesa_encode_lane(): serialize lane to noun
*/
static u3_noun
u3_mesa_encode_lane(u3_lane lan_u) {
  // [%if ip=@ port=@]
  return u3nt(c3__if, u3i_word(lan_u.pip_w), lan_u.por_s);
}

//  lane cache is (map [lane @p] lane-info)
//
static u3_noun
_mesa_lane_key(u3_ship her_u, u3_lane* lan_u)
{
  return u3nc(u3_ship_to_noun(her_u), u3_mesa_encode_lane(*lan_u));
}

// TODO: all the her_p hashtable functions are not refcounted properly
static u3_peer*
_mesa_get_peer_raw(u3_mesa* sam_u, u3_noun her)
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
_mesa_get_peer(u3_mesa* sam_u, u3_ship her_u)
{
  return _mesa_get_peer_raw(sam_u, u3_ship_to_noun(her_u));
}

static void
_mesa_put_peer_raw(u3_mesa* sam_u, u3_noun her, u3_peer* per_u)
{
  u3_peer* old_u = _mesa_get_peer_raw(sam_u, u3k(her));
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
_mesa_put_peer(u3_mesa* sam_u, u3_ship her_u, u3_peer* per_u)
{
  _mesa_put_peer_raw(sam_u, u3_ship_to_noun(her_u), per_u);
}

/* _mesa_get_request(): produce pending request state for nam_u
 *
 *   produces a NULL pointer if no pending request exists
*/
static u3_pend_req*
_mesa_get_request(u3_mesa* sam_u, u3_mesa_name* nam_u) {
  u3_pend_req* ret_u = NULL;

  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_u);
  if ( !per_u ) {
    return ret_u;
  }
  u3_noun key = _mesa_request_key(nam_u);
  u3_weak res = u3h_git(per_u->req_p, key);

  if ( res != u3_none && res != u3_nul ) {
    ret_u = u3to(u3_pend_req, res);
  }

  u3z(key);
  return ret_u;
}

static void
_mesa_del_request(u3_mesa* sam_u, u3_mesa_name* nam_u) {
  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_u);
  if ( !per_u ) {
    return;
  }
  u3_noun key = _mesa_request_key(nam_u);

  u3_weak req = u3h_get(per_u->req_p, key);
  if ( req == u3_none ) {
    u3z(key);
    return;
  }
  u3_pend_req* req_u = u3to(u3_pend_req, req);
    // u3l_log("wat_u %p", req_u->wat_u);
  // u3l_log("was_u buf %p", req_u->was_u.buf_y);
  uv_timer_stop(&req_u->tim_u);
  _mesa_free_pict(req_u->pic_u);
  c3_free(req_u->wat_u);
  c3_free(req_u->dat_y);
  lss_verifier_free(req_u->los_u);
  u3h_del(per_u->req_p, key);
  u3a_free(req_u);
  u3z(key);
}

/* _mesa_put_request(): save new pending request state for nam_u
 *
 *   the memory in the hashtable is allocated once in the lifecycle of the
 *   request. req_u will be copied into the hashtable memory, and so can be
 *   immediately freed
 *
*/
static u3_pend_req*
_mesa_put_request(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_pend_req* req_u) {
  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_u);
  if ( !per_u ) {
    return NULL;
  }
  u3_noun key = _mesa_request_key(nam_u);

  if ( req_u == NULL) {
    u3h_put(per_u->req_p, key, u3_nul);
    u3z(key);
    return req_u;
  }
  u3_pend_req* old_u = _mesa_get_request(sam_u, nam_u);
  u3_pend_req* new_u = req_u;
  if ( old_u == NULL ) {
    new_u = u3a_calloc(1, sizeof(u3_pend_req));
    /* u3l_log("putting fresh req %p", new_u); */
    memcpy(new_u, req_u, sizeof(u3_pend_req));
    new_u->per_u = per_u;
    uv_timer_init(u3L, &new_u->tim_u);
  } else {
    new_u = old_u;
    memcpy(new_u, req_u, sizeof(u3_pend_req));
  }

  u3_noun val = u3of(u3_pend_req, new_u);
  u3h_put(per_u->req_p, key, val);
  u3z(key);
  return new_u;
}

/* _ames_czar_port(): udp port for galaxy.
  XX copied from io/ames.c
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

static u3_lane
_mesa_get_direct_lane_raw(u3_mesa* sam_u, u3_noun her)
{
  if ( (c3y == u3a_is_cat(her)) && (her < 256) ) {
    c3_s por_s = _ames_czar_port(her);
    return (u3_lane){sam_u->imp_u[her].pip_w, por_s};
  }
  u3_peer* per_u = _mesa_get_peer_raw(sam_u, her);
  if ( !per_u ) {
    return (u3_lane){0,0};
  }
  return per_u->dan_u;
}

static c3_o
_mesa_lanes_equal(u3_lane* lan_u, u3_lane* lon_u)
{
  return __((lan_u->pip_w == lon_u->pip_w) && (lan_u->por_s == lon_u->por_s));
}

static u3_lane
_mesa_get_direct_lane(u3_mesa* sam_u, u3_ship her_u)
{
  return _mesa_get_direct_lane_raw(sam_u, u3_ship_to_noun(her_u));
}

static u3_lane
_mesa_get_czar_lane(u3_mesa* sam_u, c3_y imp_y)
{
  c3_s por_s = _ames_czar_port(imp_y);
  return (u3_lane){sam_u->imp_u[imp_y].pip_w, por_s};
}

static u3_lane
_mesa_get_indirect_lane(u3_mesa* sam_u, u3_noun her, u3_noun lan)
{
  if ( (c3y == u3a_is_cat(her)) && (her < 256) ) {
    return _mesa_get_czar_lane(sam_u, her);
  }
  u3_peer* per_u = _mesa_get_peer_raw(sam_u, her);
  if ( !per_u ) {
    return (u3_lane){0,0};
  }
  return _mesa_get_czar_lane(sam_u, per_u->imp_y);
}

/* RETAIN
*/
static u3_gage*
_mesa_get_lane_raw(u3_mesa* sam_u, u3_noun key)
{
  u3_gage* ret_u = NULL;
  u3_weak res = u3h_git(sam_u->lan_p, key);

  if ( res != u3_none && res != u3_nul ) {
    ret_u = u3to(u3_gage, res);
  }

  return ret_u;
}

/* _mesa_get_lane(): get lane
*/
static u3_gage*
_mesa_get_lane(u3_mesa* sam_u, u3_ship her_u, u3_lane* lan_u) {
  u3_noun key =_mesa_lane_key(her_u, lan_u);
  u3_gage* ret_u = _mesa_get_lane_raw(sam_u, key);
  u3z(key);
  return ret_u;
}

/* _mesa_put_lane(): put lane state in state
 *
 *   uses same copying trick as _mesa_put_request()
*/
static void
_mesa_put_lane(u3_mesa* sam_u, u3_ship her_u, u3_lane* lan_u, u3_gage* gag_u)
{
  u3_noun key = _mesa_lane_key(her_u, lan_u);
  u3_gage* old_u = _mesa_get_lane_raw(sam_u, key);
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
// congestion control update
static void _mesa_handle_ack(u3_gage* gag_u, u3_pact_stat* pat_u)
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

static inline c3_w
_mesa_req_get_remaining(u3_pend_req* req_u)
{
  return req_u->tot_w - req_u->nex_w;
}

/*
 * _mesa_req_get_cwnd(): produce packets to send
 *
 * saves next fragment number and preallocated pact into the passed pointers.
 * Will not do so if returning 0
*/
static c3_w
_mesa_req_get_cwnd(u3_pend_req* req_u)
{
  c3_w liv_w = bitset_wyt(&req_u->was_u);
  c3_w rem_w = _mesa_req_get_remaining(req_u);
  return c3_min(rem_w, req_u->gag_u->wnd_w - liv_w);
}

/* _mesa_req_pact_sent(): mark packet as sent
**
*/
static void
_mesa_req_pact_resent(u3_pend_req* req_u, u3_mesa_name* nam_u)
{
  c3_d now_d = _get_now_micros();
  // if we dont have pending request noop
  if ( NULL == req_u ) {
    return;
  }

  req_u->wat_u[nam_u->fra_w].sen_d = now_d;
  req_u->wat_u[nam_u->fra_w].tie_y++;
}

/* _mesa_req_pact_sent(): mark packet as sent
**   after 1-RT first packet is handled in _mesa_req_pact_init()
*/
static void
_mesa_req_pact_sent(u3_pend_req* req_u, u3_mesa_name* nam_u)
{
  c3_d now_d = _get_now_micros();
  // if we already have pending request
  if ( NULL != req_u ) {
    if( req_u->nex_w == nam_u->fra_w ) {
      req_u->nex_w++;
    }
    // TODO: optional assertions?
    /* req_u->wat_u[nam_u->fra_w] = (u3_pact_stat){now_d, 0, 1, 0 }; */
    req_u->wat_u[nam_u->fra_w].sen_d = now_d;
    req_u->wat_u[nam_u->fra_w].sip_y = 0;
    req_u->wat_u[nam_u->fra_w].tie_y = 1;

    #ifdef MESA_DEBUG
      u3l_log("bitset_put %u", nam_u->fra_w);
    #endif
    bitset_put(&req_u->was_u, nam_u->fra_w);
  } else {
    u3l_log("mesa: no req for sent");
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

/* u3_mesa_decode_lane(): deserialize noun to lane; 0.0.0.0:0 if invalid
*/
static u3_lane
u3_mesa_decode_lane(u3_atom lan) {
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
static void _mesa_free_seal(u3_seal* sel_u)
{
  c3_free(sel_u->buf_y);
  c3_free(sel_u);
}

static u3_noun _mesa_get_now() {
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  u3_noun res = u3_time_in_tv(&tim_u);
  return res;
}

static void
_mesa_send_cb(uv_udp_send_t* req_u, c3_i sas_i)
{
  u3_seal* sel_u = (u3_seal*)req_u;
  u3_mesa* sam_u = sel_u->sam_u;

  if ( sas_i ) {
    u3l_log("mesa: send fail_async: %s", uv_strerror(sas_i));
    //sam_u->fig_u.net_o = c3n;
  }
  else {
    //sam_u->fig_u.net_o = c3y;
  }

  _mesa_free_seal(sel_u);
}

static void _mesa_send_buf(u3_mesa* sam_u, u3_lane lan_u, c3_y* buf_y, c3_w len_w)
{
  u3_seal* sel_u = c3_calloc(sizeof(*sel_u));
  // this is wrong, need to calloc & memcpy
  sel_u->buf_y = buf_y;
  sel_u->len_w = len_w;
  sel_u->sam_u = sam_u;
  struct sockaddr_in add_u;

  memset(&add_u, 0, sizeof(add_u));
  add_u.sin_family = AF_INET;
  c3_w pip_w = c3y == u3_Host.ops_u.net ? lan_u.pip_w : 0x7f000001;
  c3_s por_s = lan_u.por_s;
  add_u.sin_addr.s_addr = htonl(pip_w);
  add_u.sin_port = htons(por_s);

#ifdef MESA_DEBUG
  c3_c* sip_c = inet_ntoa(add_u.sin_addr);
  u3l_log("mesa: sending packet to %s:%u", sip_c, por_s);
#endif

  uv_buf_t buf_u = uv_buf_init((c3_c*)buf_y, len_w);

  c3_i     sas_i = uv_udp_send(&sel_u->snd_u,
                               &u3_Host.wax_u,
                               &buf_u, 1,
                               (const struct sockaddr*)&add_u,
                               _mesa_send_cb);

  if ( sas_i ) {
    u3l_log("ames: send fail_sync: %s", uv_strerror(sas_i));
    /*if ( c3y == sam_u->fig_u.net_o ) {
      //sam_u->fig_u.net_o = c3n;
    }*/
    _mesa_free_seal(sel_u);
  }
}

static void _mesa_send(u3_mesa_pict* pic_u, u3_lane* lan_u)
{
  u3_mesa* sam_u = pic_u->sam_u;

  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w siz_w = mesa_etch_pact(buf_y, &pic_u->pac_u);

  _mesa_send_buf(sam_u, *lan_u, buf_y, siz_w);
}

typedef struct _u3_mesa_request_data {
  u3_mesa*      sam_u;
  u3_ship       her_u;
  u3_mesa_name* nam_u;
  c3_y*         buf_y;
  c3_w          len_w;
  u3_noun       las;
} u3_mesa_request_data;

typedef struct _u3_mesa_resend_data {
  uv_timer_t           tim_u;
  u3_mesa_request_data dat_u;
  c3_y                 ret_y;  //  number of remaining retries
} u3_mesa_resend_data;

static void
_mesa_free_request_data(u3_mesa_request_data* dat_u)
{
  c3_free(dat_u->nam_u->pat_c);
  c3_free(dat_u->nam_u);
  c3_free(dat_u->buf_y);
  u3z(dat_u->las);
  //  does not free dat_u itself, since it could be in a u3_mesa_resend_data
}

static void
_mesa_free_resend_data(u3_mesa_resend_data* res_u)
{
  uv_timer_stop(&res_u->tim_u);
  _mesa_free_request_data(&res_u->dat_u);
  c3_free(res_u);
}

static void
_mesa_send_bufs(u3_mesa* sam_u,
                u3_peer* per_u,
                c3_y* buf_y,
                c3_w len_w,
                u3_noun las);

static void
_mesa_send_modal(u3_peer* per_u, c3_y* buf_y, c3_w len_w)
{
  u3_mesa* sam_u = per_u->sam_u;
  c3_d now_d = _get_now_micros();

  if ( c3y == _mesa_is_direct_mode(per_u) ) {
    u3l_log("mesa: direct");
    _mesa_send_buf(sam_u, per_u->dan_u, buf_y, len_w);
    per_u->dir_u.sen_d = now_d;
  }
  else {
    u3l_log("mesa: indirect to %u", per_u->imp_y);
    //  XX if we have lanes in arvo, send it also there?
    //  otherwise after a peer turns indirect because we haven't contacted them,
    //  we never achieve a direct route since we only send
    //  to the sponsor, and all pages will come forwarded, and per_u->dir_u.her_d
    //  only gets updated when pages come directly, or if it's the first time
    //  after a restart of the driver
    //
    u3_lane imp_u = _mesa_get_czar_lane(sam_u, per_u->imp_y);
    _mesa_send_buf(sam_u, imp_u, buf_y, len_w);
    per_u->ind_u.sen_d = now_d;

    if ( (c3n == _mesa_is_lane_zero(&per_u->dan_u)) &&
        (per_u->dir_u.sen_d + DIRECT_ROUTE_RETRY_MICROS > now_d)) {
      _mesa_send_buf(sam_u, per_u->dan_u, buf_y, len_w);
      per_u->dir_u.sen_d = now_d;
    }
  }
}

static void
_mesa_send_request(u3_mesa_request_data* dat_u)
{
  u3_peer* per_u = _mesa_get_peer(dat_u->sam_u, dat_u->her_u);
  if ( !per_u ) {
    _mesa_send_bufs(dat_u->sam_u,
                    NULL,
                    dat_u->buf_y,
                    dat_u->len_w,
                    u3k(dat_u->las));
  }
  else {
    _mesa_send_modal(per_u, dat_u->buf_y, dat_u->len_w);
  }
  u3z(dat_u->las);
}

static void
_try_resend(u3_pend_req* req_u, c3_w ack_w)
{
  c3_o los_o = c3n;
  c3_d now_d = _get_now_micros();
  u3_mesa_pact *pac_u = &req_u->pic_u->pac_u;

  c3_y* buf_y = c3_calloc(PACT_SIZE);
  for ( int i = req_u->lef_w; i < ack_w; i++ ) {
    //  TODO: make fast recovery different from slow
    //  TODO: track skip count but not dupes, since dupes are meaningless
    if ( (c3y == bitset_has(&req_u->was_u, i)) &&
        (now_d - req_u->wat_u[i].sen_d > req_u->gag_u->rto_w) ) {
      los_o = c3y;

      pac_u->pek_u.nam_u.fra_w = i;
      c3_w siz_w  = mesa_etch_pact(buf_y, pac_u);
      if ( 0 == siz_w ) {
        u3_assert(!"failed to etch");
      }
      _mesa_send_modal(req_u->per_u, buf_y, siz_w);
    }
  }
  c3_free(buf_y);

  if ( c3y == los_o ) {
    req_u->gag_u->sst_w = (req_u->gag_u->wnd_w / 2) + 1;
    req_u->gag_u->wnd_w = req_u->gag_u->sst_w;
    req_u->gag_u->rto_w = _clamp_rto(req_u->gag_u->rto_w * 2);
  }
}

static void
_mesa_packet_timeout(uv_timer_t* tim_u);

//  TODO rename to indicate it sets a timer
static void
_update_resend_timer(u3_pend_req *req_u)
{
  if( req_u->tot_w == 0 || req_u->len_w == req_u->tot_w ) {
    u3l_log("bad condition tot_w: %u  len_w: %u",
            req_u->tot_w, req_u->len_w);
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
#ifdef MESA_DEBUG
    /* u3l_log("failed to find new oldest"); */
#endif
  }
  req_u->old_w = idx_w;
  req_u->tim_u.data = req_u;
  c3_d gap_d = req_u->wat_u[idx_w].sen_d - now_d;
  /* u3l_log("timeout %llu", (gag_u->rto_w - gap_d) / 1000); */
  c3_w dur_w = (req_u->gag_u->rto_w - gap_d) / 1000;
  uv_timer_start(&req_u->tim_u, _mesa_packet_timeout, dur_w, 0);
}

/* _mesa_packet_timeout(): callback for packet timeout
*/
static void
_mesa_packet_timeout(uv_timer_t* tim_u) {
  u3_pend_req* req_u = (u3_pend_req*)tim_u->data;
  /* u3l_log("%u packet timed out", req_u->old_w); */
  _try_resend(req_u, req_u->nex_w);
  _update_resend_timer(req_u);
}

static c3_o
_mesa_burn_misorder_queue(u3_pend_req* req_u)
{
  c3_w num_w;
  c3_w max_w = sizeof(req_u->mis_u) / sizeof(u3_misord_buf);
  c3_o res_o = c3y;
  for ( num_w = 0; num_w < max_w; num_w++ ) {
    u3_misord_buf* buf_u = &req_u->mis_u[num_w];
    if ( buf_u->len_w == 0 ) {
      break;
    }
    if ( c3y != lss_verifier_ingest(req_u->los_u, buf_u->fra_y, buf_u->len_w, buf_u->par_u) ) {
      res_o = c3n;
      break;
    }
  }
  // ratchet forward
  num_w++; // account for the packet processed in _mesa_req_pact_done
  memcpy(req_u->mis_u, req_u->mis_u + num_w, max_w - num_w);
  memset(req_u->mis_u + max_w - num_w, 0, num_w * sizeof(u3_misord_buf));
  return res_o;
}

static void
_init_lane_state(u3_lane_state* sat_u);

/* _mesa_req_pact_done(): mark packet as done
*/
static void
_mesa_req_pact_done(u3_pend_req*  req_u,
                    u3_mesa_name* nam_u,
                    u3_mesa_data* dat_u,
                    c3_y          hop_y,
                    u3_lane       lan_u)
{
  u3_mesa* sam_u = req_u->per_u->sam_u; //  needed for the MESA_LOG macro

  // received past the end of the message
  if ( dat_u->tot_w <= nam_u->fra_w ) {
    u3l_log("strange tot_w %u fra_w %u req_u %u", dat_u->tot_w, nam_u->fra_w, req_u->len_w);
    MESA_LOG(sam_u, STRANGE);
    //  XX: is this sufficient to drop whole request
    return;
  }

  // received duplicate
  if ( c3n == bitset_has(&req_u->was_u, nam_u->fra_w) ) {
    MESA_LOG(sam_u, DUPE);
    return;
  }

  bitset_del(&req_u->was_u, nam_u->fra_w);
  if ( nam_u->fra_w > req_u->ack_w ) {
    req_u->ack_w = nam_u->fra_w;
  }

  #ifdef MESA_DEBUG
    if ( nam_u->fra_w != 0 && req_u->wat_u[nam_u->fra_w].tie_y != 1 ) {
      u3l_log("received retry %u", nam_u->fra_w);
    }
  #endif

  req_u->len_w++;

  #ifdef MESA_DEBUG
    u3l_log("fragment %u len %u", nam_u->fra_w, req_u->len_w);
  #endif
  if ( req_u->lef_w == nam_u->fra_w ) {
    req_u->lef_w++;
  }

  lss_pair* par_u = NULL;
  if ( dat_u->aum_u.typ_e == AUTH_NEXT ) {
    // needs to be heap allocated bc will be saved if misordered
    par_u = c3_calloc(sizeof(lss_pair));
    memcpy((*par_u)[0], dat_u->aup_u.has_y[0], sizeof(lss_hash));
    memcpy((*par_u)[1], dat_u->aup_u.has_y[1], sizeof(lss_hash));
  }

  if ( req_u->los_u->counter != nam_u->fra_w ) {
    if ( nam_u->fra_w < req_u->los_u->counter ) {
      u3l_log("fragment number too low (%u)", nam_u->fra_w);
    } else if ( nam_u->fra_w >= req_u->los_u->counter + (sizeof(req_u->mis_u)/sizeof(u3_misord_buf)) ) {
      u3l_log("fragment number too high (%u)", nam_u->fra_w);
    } else {
      // insert into misordered queue
      u3_misord_buf* buf_u = &req_u->mis_u[nam_u->fra_w - req_u->los_u->counter - 1];
      buf_u->fra_y = c3_calloc(dat_u->len_w);
      buf_u->len_w = dat_u->len_w;
      memcpy(buf_u->fra_y, dat_u->fra_y, dat_u->len_w);
      buf_u->par_u = par_u;
    }
  }
  else if ( c3y != lss_verifier_ingest(req_u->los_u, dat_u->fra_y, dat_u->len_w, par_u) ) {
    c3_free(par_u);
    // TODO: do we drop the whole request on the floor?
    u3l_log("auth fail frag %u", nam_u->fra_w);
    MESA_LOG(sam_u, AUTH);
    return;
  }
  else if ( c3y != _mesa_burn_misorder_queue(req_u) ) {
    c3_free(par_u);
    MESA_LOG(sam_u, AUTH)
    return;
  }
  else {
    c3_free(par_u);
  }

  u3_lane_state* sat_u;
  if ( 0 == hop_y && (c3n == _mesa_lanes_equal(&lan_u, &req_u->per_u->dan_u)) ) {
    req_u->per_u->dan_u = lan_u;
    sat_u = &req_u->per_u->dir_u;
    _init_lane_state(sat_u);
  }
  else {
    sat_u = &req_u->per_u->ind_u;
  }
  sat_u->her_d = _get_now_micros();

  // handle gauge update
  _mesa_handle_ack(req_u->gag_u, &req_u->wat_u[nam_u->fra_w]);

  c3_w siz_w = (1 << (nam_u->boq_y - 3));
  memcpy(req_u->dat_y + (siz_w * nam_u->fra_w), dat_u->fra_y, dat_u->len_w);

  _try_resend(req_u, nam_u->fra_w);
  _update_resend_timer(req_u);
}

static u3_lane
_realise_lane(u3_noun lan) {
  u3_lane lan_u;
  lan_u.por_s = 0;
  lan_u.pip_w = 0;

  if ( c3y == u3a_is_cat(lan)  ) {
    // u3_assert( lan < 256 );
    if ( (c3n == u3_Host.ops_u.net) ) {
      lan_u.pip_w =  0x7f000001 ;
      lan_u.por_s = _ames_czar_port(lan);
    }
  } else {
    u3_noun tag, pip, por;
    u3x_trel(lan, &tag, &pip, &por);
    if ( tag == c3__if ) {
      lan_u.pip_w = u3r_word(0, pip);
      u3_assert( c3y == u3a_is_cat(por) && por <= 0xFFFF);
      lan_u.por_s = por;
    } else {
      u3l_log("mesa: inscrutable lane");
    }
    u3z(lan);
  }
  return lan_u;
}

static void
_mesa_send_bufs(u3_mesa* sam_u,
                u3_peer* per_u, // null for response packets
                c3_y* buf_y,
                c3_w len_w,
                u3_noun las)
{
  u3_noun lan, t = las;
  while ( t != u3_nul ) {
    u3x_cell(t, &lan, &t);
    u3_lane lan_u = _realise_lane(u3k(lan));

    if ( !lan_u.por_s ) {
      u3l_log("mesa: failed to realise lane");
    } else {
      c3_y* sen_y = c3_calloc(len_w);
      memcpy(sen_y, buf_y, len_w);
      _mesa_send_buf(sam_u, lan_u, sen_y, len_w);
      if ( per_u && (c3y == _mesa_lanes_equal(&lan_u, &per_u->dan_u)) ) {
        u3l_log("  direct send");
        per_u->dir_u.sen_d = _get_now_micros();
      }
    }
  }
  u3z(las);
}

static void
_mesa_timer_cb(uv_timer_t* tim_u) {
  u3_pend_req* req_u = tim_u->data;
  _try_resend(req_u, req_u->nex_w);
}

static void
_mesa_czar_here(u3_czar_info* imp_u, time_t now_t, struct sockaddr_in* add_u)
{
  u3_mesa* sam_u = imp_u->sam_u;
  c3_y imp_y = imp_u->imp_y;
  c3_w pip_w = ntohl(add_u->sin_addr.s_addr);

  if ( imp_u->pip_w != pip_w ) {
    u3_noun nam = u3dc("scot", c3__if, u3i_word(pip_w));
    c3_c* nam_c = u3r_string(nam);

    u3l_log("mesa: czar %s: ip %s", imp_u->dns_c, nam_c);

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
    _mesa_send_buf(sam_u, lan_u, buf_y, len_w);
  }
  u3z(imp_u->pen);
  imp_u->pen = u3_nul;
}

static void
_mesa_czar_gone(u3_mesa* sam_u, c3_i sas_i, c3_y imp_y, time_t now_t)
{
  u3_czar_info* imp_u = &sam_u->imp_u[imp_y];
  imp_u->tim_t = now_t;
  u3l_log("mesa: %s", uv_strerror(sas_i));
}

static void
_mesa_czar_cb(uv_getaddrinfo_t* adr_u, c3_i sas_i, struct addrinfo* aif_u)
{
  u3_czar_info* imp_u = (u3_czar_info*)adr_u->data;
  c3_y imp_y = imp_u->imp_y;
  u3_mesa* sam_u = imp_u->sam_u;
  time_t now_t = time(0);

  if ( 0 == sas_i ) {
    // XX: lifetimes for addrinfo, ames does something funny
    _mesa_czar_here(imp_u, now_t, (struct sockaddr_in*)aif_u->ai_addr);
  } else {
    _mesa_czar_gone(sam_u, sas_i, imp_y, now_t);
  }

  c3_free(adr_u);
  uv_freeaddrinfo(aif_u);

}

static void
_mesa_resolve_czar(u3_mesa* sam_u, c3_y imp_y, u3_noun pac)
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
    u3l_log("mesa: no galaxy domain");
    return;
  }
  imp_u->dns_c = _mesa_czar_dns(imp_y, sam_u->dns_c);
  {
    uv_getaddrinfo_t* adr_u = c3_calloc(sizeof(*adr_u));
    c3_i sas_i;
    adr_u->data = imp_u;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if ( 0 != (sas_i = uv_getaddrinfo(u3L, adr_u, _mesa_czar_cb, imp_u->dns_c, 0, &hints))) {
      time_t now_t = time(0);
      _mesa_czar_gone(sam_u, sas_i, imp_y, now_t);
      return;
    }
  }


}

static u3_noun
_mesa_queue_czar(u3_mesa* sam_u, u3_noun las, u3_noun pac)
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
        _mesa_resolve_czar(sam_u, lan, u3k(pac));
      }
    } else {
      res = u3nc(u3k(lan), res);
    }
  }
  u3z(las);
  u3z(pac);
  return res;
}

static void _mesa_add_our_to_pit(u3_mesa*, u3_mesa_name*);

static u3_noun
_name_to_scry(u3_mesa_name* nam_u)
{
  u3_noun rif = _dire_etch_ud(nam_u->rif_w);
  u3_noun boq = _dire_etch_ud(nam_u->boq_y);
  u3_noun fag = _dire_etch_ud(nam_u->fra_w);
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);

  u3_noun wer = nam_u->nit_o == c3y
    ? u3nc(c3__init, pax)
    : u3nt(nam_u->aut_o == c3y ? c3__auth : c3__data, fag, pax);

  u3_noun res = u3nc(c3__mess, u3nq(rif, c3__pact, boq, u3nc(c3__etch, wer)));

  return res;
}

/*
 * RETAIN */
static u3_weak
_mesa_get_pit(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  return u3h_get(sam_u->pit_p, _name_to_scry((nam_u)));
}

static void
_mesa_put_pit(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_noun val)
{
  u3_noun pax = _name_to_scry(nam_u);
  u3h_put(sam_u->pit_p, pax, u3k(val));
  #ifdef MESA_DEBUG
    c3_c* our_c = (c3y == u3h(val))? "&" : "|";
    c3_c* las_c = (u3_nul == u3t(val))? "~" : "...";
    u3l_log("mesa: put_pit(our %s, las %s)", our_c, las_c);
    log_name(nam_u);
  #endif
  u3z(pax);
  u3z(val);
}

static void
_mesa_del_pit(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  u3_noun pax = _name_to_scry(nam_u);
  u3h_del(sam_u->pit_p, pax);
  u3z(pax);
}

static void
_mesa_add_lane_to_pit(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_lane lan_u)
{
  // TODO: prevent duplicate lane from being added
  u3_noun lan = u3_mesa_encode_lane(lan_u);
  u3_weak pin = _mesa_get_pit(sam_u, nam_u);
  if ( u3_none == pin ) {
    pin = u3nt(c3n, u3k(lan), u3_nul);
  }
  else {
    pin = u3nt(u3k(u3h(pin)), u3k(lan), u3k(u3t(pin)));
  }
  _mesa_put_pit(sam_u, nam_u, u3k(pin));
  u3z(lan); u3z(pin);
  return;
}

static void
_mesa_add_our_to_pit(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  u3_weak pin = _mesa_get_pit(sam_u, nam_u);
  if ( u3_none == pin ) {
    pin = u3nc(c3y, u3_nul);
  }
  else {
    pin = u3nc(c3y, u3k(u3t(pin)));
  }
  _mesa_put_pit(sam_u, nam_u, u3k(pin));
  u3z(pin);
  return;
}

static void
_mesa_resend_timer_cb(uv_timer_t* tim_u)
{
  u3_mesa_resend_data* res_u = (u3_mesa_resend_data*)tim_u;
  u3_mesa_request_data* dat_u = &res_u->dat_u;
  res_u->ret_y--;

  u3_weak pit = _mesa_get_pit(dat_u->sam_u, dat_u->nam_u);
  if ( u3_none == pit ) {
    #ifdef MESA_DEBUG
      u3l_log("mesa: resend PIT entry gone %u", res_u->ret_y);
    #endif
    _mesa_free_resend_data(res_u);
    return;
  }
  else {
    #ifdef MESA_DEBUG
      u3l_log("mesa: resend %u", res_u->ret_y);
    #endif
  }
  u3z(pit);
  
  _mesa_send_request(dat_u);

  if ( res_u->ret_y ) {
    uv_timer_start(&res_u->tim_u, _mesa_resend_timer_cb, 1000, 0);
  }
  else {
    _mesa_free_resend_data(res_u);
  }
}

static void
_mesa_ef_send(u3_mesa* sam_u, u3_noun las, u3_noun pac)
{
  las = _mesa_queue_czar(sam_u, las, u3k(pac));
  c3_w len_w = u3r_met(3, pac);
  c3_y* buf_y = c3_calloc(len_w);
  u3r_bytes(0, len_w, buf_y, pac);

  u3_mesa_head hed_u;
  mesa_sift_head(buf_y, &hed_u);

  #ifdef MESA_DEBUG
    c3_c* typ_c = (PACT_PEEK == hed_u.typ_y) ? "PEEK" :
                  (PACT_POKE == hed_u.typ_y) ? "POKE" :
                  (PACT_PAGE == hed_u.typ_y) ? "PAGE" : "RESV";
    u3l_log("mesa: ef_send() %s", typ_c);
  #endif

  u3_mesa_pact pac_u;
  c3_w         res_w = mesa_sift_pact(&pac_u, buf_y, len_w);

  if ( PACT_PAGE == hed_u.typ_y ) {
    u3_weak pin = _mesa_get_pit(sam_u, &pac_u.pek_u.nam_u);

    if ( u3_none == pin ) {
      #ifdef MESA_DEBUG
        u3l_log(" no PIT entry");
      #endif
      return;
    }

    u3_noun our, las;
    u3x_cell(pin, &our, &las);
    u3m_p("lane", las);
    if ( u3_nul != las ) {
      _mesa_send_bufs(sam_u, NULL, buf_y, len_w, u3k(las));
      _mesa_del_pit(sam_u, &pac_u.pek_u.nam_u);
      u3z(pin);
    }
  }
  else {
    u3_mesa_name* nam_u = c3_malloc(sizeof(u3_mesa_name));
    memcpy(nam_u, &pac_u.pek_u.nam_u, sizeof(u3_mesa_name));
    u3_mesa_resend_data* res_u = c3_malloc(sizeof(u3_mesa_resend_data));
    u3_mesa_request_data* dat_u = &res_u->dat_u;
    {
      dat_u->sam_u = sam_u;
      _get_her(&pac_u, dat_u->her_u);
      dat_u->nam_u = nam_u;
    }
    {
      res_u->ret_y = 4;
      uv_timer_init(u3L, &res_u->tim_u);
    }
    _mesa_add_our_to_pit(sam_u, nam_u);
    _mesa_send_request(dat_u);
    uv_timer_start(&res_u->tim_u, _mesa_resend_timer_cb, 1000, 0);
  }

  sam_u->tim_d = _get_now_micros();

  //  TODO: free pac_u or any fields in pac_u?
  u3z(pac);
  u3z(las);
}

c3_o
_ames_kick_newt(void* sam_u, u3_noun tag, u3_noun dat);

static void _meet_peer(u3_mesa* sam_u, u3_peer* per_u, u3_ship her_u);

static c3_o _mesa_kick(u3_mesa* sam_u, u3_noun tag, u3_noun dat)
{
  c3_o ret_o;
  switch ( tag ) {
    default: {
      ret_o = c3n;
     } break;
    case c3__push: {
      u3_noun las, pac;
      if ( c3n == u3r_cell(dat, &las, &pac) ) {
        // u3l_log(" mesa: send old");
        ret_o = c3n;
      } else {
        // u3l_log(" mesa: send new");
        _mesa_ef_send(sam_u, u3k(las), u3k(pac));
        ret_o = c3y;
      }
    } break;
    case c3__send:
    case c3__turf:
    case c3__saxo: {
      #ifdef MESA_DEBUG
        c3_c* tag_c = u3r_string(tag);
        u3l_log("mesa: send old %s", tag_c);
        c3_free(tag_c);
      #endif
      ret_o = _ames_kick_newt(u3_Host.sam_u, u3k(tag), u3k(dat));
    } break;
    case c3__nail: {
      u3m_p("data", dat);
      u3_noun who = u3k(u3h(dat));
      u3_peer* per_u = _mesa_get_peer_raw(sam_u, who);
      c3_o new_o = c3n;

      u3_ship who_u;
      u3_ship_of_noun(who_u ,who);
      _meet_peer(sam_u, per_u, who_u);

      ret_o = _ames_kick_newt(u3_Host.sam_u, u3k(tag), u3k(dat));
    } break;
  }

  // technically losing tag is unncessary as it always should
  // be a direct atom, but better to be strict
  u3z(dat); u3z(tag);
  return ret_o;
}

static c3_o _mesa_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_mesa* sam_u = (u3_mesa*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;
  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3__ames != i_wir)
     || (c3n == u3r_cell(cad, &tag, &dat)) )
  {
    ret_o = c3n;
  }
  else {
    ret_o = _mesa_kick(sam_u, u3k(tag), u3k(dat));
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

static u3_noun
_mesa_io_info(u3_auto* car_u)
{

  return u3_nul;
}

static void
_mesa_io_slog(u3_auto* car_u) {
  u3l_log("mesa is online");
}

static void
_mesa_free_peer(u3_noun per)
{
  u3_peer* per_u = u3to(u3_peer, per);
  // u3h_free(per_u->req_p);  XX FIXME; refcounts wrong
}

static void
_mesa_exit_cb(uv_handle_t* had_u)
{
  u3_mesa* sam_u = had_u->data;

  u3s_cue_xeno_done(sam_u->sil_u);
  ur_cue_test_done(sam_u->tes_u);
  u3h_walk(sam_u->her_p, _mesa_free_peer);
  u3h_free(sam_u->her_p);

  c3_free(sam_u);
}

static void
_mesa_io_exit(u3_auto* car_u)
{
  u3_mesa* sam_u = (u3_mesa*)car_u;
  uv_close(&sam_u->had_u, _mesa_exit_cb);
}

static void
_init_lane_state(u3_lane_state* sat_u)
{
  sat_u->sen_d = 0;
  sat_u->her_d = 0;
  sat_u->rtt_w = 1000000;
  sat_u->rtv_w = 1000000;
}

static void
_init_peer(u3_mesa* sam_u, u3_peer* per_u)
{
  per_u->sam_u = sam_u;
  per_u->ful_o = c3n;
  per_u->dan_u = (u3_lane){0,0};
  _init_lane_state(&per_u->dir_u);
  per_u->imp_y = 0;
  _init_lane_state(&per_u->ind_u);
  per_u->req_p = u3h_new();
}

/*
 * RETAIN
 */
static c3_o
_mesa_add_galaxy_pend(u3_mesa* sam_u, u3_noun her, u3_noun pen)
{
  u3_weak old = u3h_get(sam_u->her_p, her);
  u3_noun pes = u3_nul;
  u3_noun wat;
  if ( u3_none != old ) {
    if ( u3h(old) == MESA_CZAR ) {
      u3x_cell(u3t(old), &pes, &wat);
      u3z(old);
    } else {
      u3l_log("mesa: attempted to resolve resolved czar");
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
_name_to_jumbo_scry(u3_mesa_name* nam_u)
{
  u3_noun rif = _dire_etch_ud(nam_u->rif_w);
  u3_noun boq = _dire_etch_ud(31);
  u3_noun fag = _dire_etch_ud(0); // XX 1
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);

  u3_noun wer = nam_u->nit_o == c3y
    ? u3nc(c3__init, pax)
    : u3nt(nam_u->aut_o == c3y ? c3__auth : c3__data, fag, pax);

  u3_noun res = u3nc(c3__mess, u3nq(rif, c3__pact, boq, u3nc(c3__etch, wer)));

  return res;
}

/*
 * RETAIN */
static u3_weak
_mesa_get_jumbo_cache(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  u3_noun pax = _name_to_jumbo_scry(nam_u);
  u3_weak res = u3h_get(sam_u->pac_p, pax);
  #ifdef MESA_DEBUG
    if ( u3_none == res ) {
      u3m_p("miss", pax);
    } else {
      u3_noun kev = u3nc(u3k(pax), u3k(res));
      u3m_p("hit", kev);
      u3z(kev);
    }
    u3z(pax);
  #endif
  return res;
}

static void
_mesa_put_jumbo_cache(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_noun val)
{
  u3_noun pax = _name_to_jumbo_scry(nam_u);
  u3h_put(sam_u->pac_p, pax, u3k(val));
  u3z(pax); // TODO: fix refcount
}

static void
_mesa_send_pact(u3_mesa*      sam_u,
                u3_noun       las,
                u3_peer*      per_u, // null for response packets
                u3_mesa_pact* tac_u)
{
  c3_y buf_y[PACT_SIZE];
  c3_w len_w = mesa_etch_pact(buf_y, tac_u);
  _mesa_send_bufs(sam_u, per_u, buf_y, len_w, u3k(las));
  u3z(las);
}

static void
_mesa_send_jumbo_pieces(u3_mesa* sam_u, u3_noun pag)
{
  #ifdef MESA_DEBUG
    u3l_log("mesa: send_jumbo_pieces()");
  #endif
  // parse packet
  u3_mesa_pact tac_u = {0};
  {
    c3_w jumbo_w = u3r_met(3, pag);
    c3_y* jumbo_y = c3_calloc(jumbo_w);
    u3r_bytes(0, jumbo_w, jumbo_y, pag);
    u3z(pag);
    mesa_sift_pact(&tac_u, jumbo_y, jumbo_w);
  }
  c3_w jumbo_pact_w = tac_u.pag_u.dat_u.len_w;
  c3_y* jumbo_pact_y = tac_u.pag_u.dat_u.fra_y;

  // compute LSS data
  //
  // TODO: this assumes we have the entire message. Should be switched to use
  // lss_builder_transceive instead.
  c3_w leaves_w = (jumbo_pact_w + 1023) / 1024;
  lss_builder bil_u;
  lss_builder_init(&bil_u, leaves_w);
  for ( c3_w i = 0; i < leaves_w; i++ ) {
    c3_y* leaf_y = jumbo_pact_y + (i*1024);
    c3_w leaf_w = (i < leaves_w - 1) ? 1024 : jumbo_pact_w % 1024;
    lss_builder_ingest(&bil_u, leaf_y, leaf_w);
  }
  lss_hash* proof = lss_builder_finalize(&bil_u);
  c3_w proof_len = lss_proof_size(leaves_w);


  // send packets
  u3_mesa_name* nam_u = &tac_u.pag_u.nam_u;
  u3_mesa_data* dat_u = &tac_u.pag_u.dat_u;
  nam_u->boq_y = 13;
  dat_u->tot_w = leaves_w;

  if ( c3y == nam_u->nit_o && leaves_w > 4) {
    u3_weak pin = _mesa_get_pit(sam_u, nam_u);
    if ( u3_none != pin ) {
        #ifdef MESA_DEBUG
          u3l_log(" sending proof packet");
        #endif
        dat_u->len_w = proof_len*sizeof(lss_hash);
        c3_y* proof_y = c3_malloc(dat_u->len_w);
        memcpy(proof_y, proof, dat_u->len_w);
        dat_u->fra_y = proof_y;
       _mesa_send_pact(sam_u, u3k(u3t(pin)), NULL, &tac_u);
       _mesa_del_pit(sam_u, nam_u);
       u3z(pin);
    }
  }

  // send leaf packets
  for (c3_w i = 0; i < leaves_w; i++) {
    nam_u->nit_o = __((i == 0) && (leaves_w <= 4));
    nam_u->fra_w = i;
    dat_u->fra_y = jumbo_pact_y + (i*1024);
    dat_u->len_w = c3_min(jumbo_pact_w - (i*1024), 1024);

    if ( c3y == nam_u->nit_o ) {
      // inline proof; omit first leaf
      proof_len--;
      proof++;
      dat_u->aup_u.len_y = proof_len;
      memcpy(dat_u->aup_u.has_y, proof, proof_len*sizeof(lss_hash));
    } else {
      lss_pair* pair = lss_builder_pair(&bil_u, i);
      if ( NULL == pair ) {
        dat_u->aup_u.len_y = 0;
        dat_u->aum_u.typ_e = AUTH_NONE;
      } else {
        dat_u->aup_u.len_y = 2;
        dat_u->aum_u.typ_e = AUTH_NEXT;
        memcpy(dat_u->aup_u.has_y[0], (*pair)[0], sizeof(lss_hash));
        memcpy(dat_u->aup_u.has_y[1], (*pair)[1], sizeof(lss_hash));
      }
    }
    u3_weak pin = _mesa_get_pit(sam_u, nam_u);
    if ( u3_none != pin) {
      #ifdef MESA_DEBUG
        u3l_log(" sending leaf packet, fra_w: %u", nam_u->fra_w);
      #endif
      _mesa_send_pact(sam_u, u3k(u3t(pin)), NULL, &tac_u);
      _mesa_del_pit(sam_u, nam_u);
      u3k(pin);
    }
  }
}

static void
_mesa_page_scry_jumbo_cb(void* vod_p, u3_noun res)
{
  u3_mesa_pict* pic_u = vod_p;
  u3_mesa*      sam_u = pic_u->sam_u;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa_name* nam_u = &pac_u->pag_u.nam_u;

  u3_weak pag = u3r_at(7, res);

  if ( u3_none == pag ) {
    // TODO: mark as dead
    u3z(res);
    u3l_log("mesa: jumbo frame missing");
    log_pact(pac_u);
    return;
  }
  #ifdef MESA_DEBUG
    u3l_log("mesa: scry_jumbo_cb()");
    log_pact(pac_u);
  #endif
  _mesa_put_jumbo_cache(sam_u, nam_u, u3nc(MESA_ITEM, u3k(pag)));
  _mesa_send_jumbo_pieces(sam_u, u3k(pag));
  u3z(pag);
}

static void
_mesa_hear_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3l_log("mesa: hear bail");
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
    u3_peer* new_u = _mesa_get_peer_raw(per_u->sam_u, u3k(her));
    if ( new_u != NULL ) {
      per_u = new_u;
    }
    u3_mesa* sam_u = per_u->sam_u;
    u3_noun gal = u3do("rear", u3k(sax));
    u3_assert( c3y == u3a_is_cat(gal) && gal < 256 );
    // both atoms guaranteed to be cats, bc we don't call unless forwarding
    per_u->ful_o = c3y;
    per_u->imp_y = gal;
    _mesa_put_peer_raw(per_u->sam_u, her, per_u);
  }

  u3z(nun);
}

static void
_meet_peer(u3_mesa* sam_u, u3_peer* per_u, u3_ship her_u)
{
  u3_noun her = u3_ship_to_noun(her_u);
  u3_noun gan = u3nc(u3_nul, u3_nul);
  u3_noun pax = u3nc(u3dc("scot", c3__p, her), u3_nul);
  u3_pier_peek_last(sam_u->pir_u, gan, c3__j, c3__saxo, pax, per_u, _saxo_cb);
}

static void
_hear_peer(u3_mesa* sam_u, u3_peer* per_u, u3_lane lan_u, c3_o dir_o)
{
  if ( c3y == dir_o ) {
    per_u->dan_u = lan_u;
    per_u->dir_u.her_d = _get_now_micros();
  } else {
    per_u->ind_u.her_d = _get_now_micros();
  }
}

static void
_mesa_request_next_fragments(u3_mesa* sam_u,
                             u3_pend_req* req_u,
                             u3_lane lan_u)
{
  c3_w win_w = _mesa_req_get_cwnd(req_u);
  u3_mesa_pict* nex_u = req_u->pic_u;
  c3_w nex_w = req_u->nex_w;
  for ( int i = 0; i < win_w; i++ ) {
    c3_w fra_w = nex_w + i;
    if ( fra_w >= req_u->tot_w ) {
      break;
    }
    nex_u->pac_u.pek_u.nam_u.fra_w = nex_w + i;
    _mesa_add_our_to_pit(sam_u, &nex_u->pac_u.pek_u.nam_u);
    _mesa_send(nex_u, &lan_u);
    _mesa_req_pact_sent(req_u, &nex_u->pac_u.pek_u.nam_u);
  }
}

typedef struct _u3_mesa_veri_cb_data {
  u3_mesa*     sam_u;
  u3_mesa_name nam_u;
  u3_lane      lan_u;
} u3_mesa_veri_cb_data;

static void
_mesa_veri_scry_cb(void* vod_p, u3_noun nun)
{
  u3_mesa_veri_cb_data* ver_u = vod_p;
  u3_pend_req* req_u = _mesa_get_request(ver_u->sam_u, &ver_u->nam_u);
  if ( !req_u ) {
    return;
  }
  else if ( c3y == nun ) {
    _mesa_request_next_fragments(ver_u->sam_u, req_u, ver_u->lan_u);
  }
  else if ( c3n == nun ) {
    u3l_log("mesa: packet auth failed verification");
    // TODO: wipe request state? (If this was an imposter,
    // we don't want to punish the real peer.)
  }
  else {
    u3l_log("mesa: %%veri returned strange value");
  }
}

static void
_mesa_req_pact_init(u3_mesa* sam_u, u3_mesa_pict* pic_u, u3_lane* lan_u)
{
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa_name* nam_u = &pac_u->pag_u.nam_u;
  u3_mesa_data* dat_u = &pac_u->pag_u.dat_u;

  u3_gage* gag_u = _mesa_get_lane(sam_u, nam_u->her_u, lan_u);
  if ( gag_u == NULL ) {
    gag_u = alloca(sizeof(u3_gage));
    _init_gage(gag_u);
    // save and re-retrieve so we have persistent pointer
    _mesa_put_lane(sam_u, nam_u->her_u, lan_u, gag_u);
    gag_u = _mesa_get_lane(sam_u, nam_u->her_u, lan_u);
    u3_assert( gag_u != NULL );
  }

  u3_pend_req* req_u = alloca(sizeof(u3_pend_req));
  memset(req_u, 0, sizeof(u3_pend_req));
  req_u->pic_u = c3_calloc(sizeof(u3_mesa_pict));
  req_u->pic_u->sam_u = sam_u;
  req_u->pic_u->pac_u.hed_u.typ_y = PACT_PEEK;
  req_u->pic_u->pac_u.hed_u.pro_y = MESA_VER;
  memcpy(&req_u->pic_u->pac_u.pek_u.nam_u, nam_u, sizeof(u3_mesa_name));
  req_u->pic_u->pac_u.pek_u.nam_u.aut_o = c3n;
  req_u->pic_u->pac_u.pek_u.nam_u.nit_o = c3n;
  req_u->aum_u = dat_u->aum_u;

  c3_w siz_w = 1 << (pac_u->pag_u.nam_u.boq_y - 3);
  u3_assert( siz_w == 1024 ); // boq_y == 13
  req_u->gag_u = gag_u;
  req_u->dat_y = c3_calloc(siz_w * dat_u->tot_w);
  req_u->wat_u = c3_calloc(sizeof(u3_pact_stat) * dat_u->tot_w + 2 );
  req_u->tot_w = dat_u->tot_w;
  bitset_init(&req_u->was_u, dat_u->tot_w);

  // TODO: handle restart
  // u3_assert( nam_u->fra_w == 0 );

  c3_o lin_o = dat_u->tot_w <= 4 ? c3y : c3n;
  req_u->nex_w = (c3y == lin_o) ? 1 : 0;
  req_u->len_w = (c3y == lin_o) ? 1 : 0;
  req_u->lef_w = 0;
  req_u->old_w = 0;
  req_u->ack_w = 0;

  c3_w pof_w = lss_proof_size(req_u->tot_w);
  lss_hash* pof_u = c3_calloc(pof_w * sizeof(lss_hash));
  if ( c3y == lin_o ) {
    if ( pof_w != (dat_u->aup_u.len_y + 1) ) {
      return; // TODO: handle like other auth failures
    }
    for ( int i = 1; i < pof_w; i++ ) {
      memcpy(pof_u[i], dat_u->aup_u.has_y[i-1], sizeof(lss_hash));
    }
    // complete the proof by computing the first leaf hash
    lss_complete_inline_proof(pof_u, dat_u->fra_y, dat_u->len_w);
  } else {
    if ( dat_u->len_w != pof_w*sizeof(lss_hash) ) {
      return; // TODO: handle like other auth failures
    }
    for ( int i = 0; i < pof_w; i++ ) {
      memcpy(pof_u[i], dat_u->fra_y + (i * sizeof(lss_hash)), sizeof(lss_hash));
    }
  }
  lss_hash root;
  lss_root(root, pof_u, pof_w);
  req_u->los_u = c3_calloc(sizeof(lss_verifier));
  lss_verifier_init(req_u->los_u, 0, req_u->tot_w, pof_u);
  c3_free(pof_u);

  if ( c3y == lin_o ) {
    if ( c3y != lss_verifier_ingest(req_u->los_u, dat_u->fra_y, dat_u->len_w, NULL) ) {
      return; // TODO: handle like other auth failures
    }
    memcpy(req_u->dat_y, dat_u->fra_y, dat_u->len_w);
  }

  req_u = _mesa_put_request(sam_u, nam_u, req_u);
  _update_resend_timer(req_u);

  // scry to verify auth
  u3_noun typ, aut;
  switch ( dat_u->aum_u.typ_e ) {
  case AUTH_SIGN:
    typ = c3__sign;
    aut = u3dc("scot", c3__uv, u3i_bytes(64, dat_u->aum_u.sig_y));
    break;
  case AUTH_HMAC:
    typ = c3__hmac;
    aut = u3dc("scot", c3__uv, u3i_bytes(32, dat_u->aum_u.mac_y));
    break;
  default:
    return; // TODO: handle like other auth failures
  }
  u3_noun her = u3dc("scot", c3__p, u3_ship_to_noun(nam_u->her_u));
  u3_noun rut = u3dc("scot", c3__uv, u3i_bytes(32, root));
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);
  u3_noun sky = u3i_list(typ, her, aut, rut, pax, u3_none);
  u3_mesa_veri_cb_data* ver_u = c3_malloc(sizeof(u3_mesa_veri_cb_data));
  ver_u->sam_u = sam_u;
  memcpy(&ver_u->nam_u, nam_u, sizeof(u3_mesa_name));
  ver_u->lan_u = *lan_u;
  u3_pier_peek_last(sam_u->pir_u, u3_nul, c3__a, c3__veri, sky, ver_u, _mesa_veri_scry_cb);
}

typedef struct _u3_mesa_lane_cb_data {
  u3_ship  her_u;
  u3_lane  lan_u;
  u3_peer* per_u;
} u3_mesa_lane_cb_data;

static void
_mesa_page_news_cb(u3_ovum* egg_u, u3_ovum_news new_e)
{
  if ( u3_ovum_done != new_e ) {
    #ifdef MESA_DEBUG
      u3l_log("mesa: arvo page event was not a success");
    #endif
    return;
  }
  u3_mesa_lane_cb_data* dat_u = egg_u->ptr_v;
  u3_peer* per_u = dat_u->per_u;

  #ifdef MESA_DEBUG
    c3_c* her_c = u3_ship_to_string(dat_u->her_u);
    u3l_log("mesa: %%dear %s", her_c);
    _log_lane(&dat_u->lan_u);
    c3_free(her_c);
  #endif

  c3_free(dat_u);
}

static void
_mesa_page_bail_cb(u3_ovum* egg_u, u3_ovum_news new_e)
{
  #ifdef MESA_DEBUG
    u3l_log("mesa: arvo page event failed");
  #endif
  c3_free(egg_u->ptr_v);
}

static void
_mesa_add_hop(c3_y hop_y, u3_mesa_head* hed_u, u3_mesa_page_pact* pag_u, u3_lane lan_u)
{
  if ( 1 == hop_y ) {
    c3_etch_word(pag_u->sot_u, lan_u.pip_w);
    c3_etch_short(pag_u->sot_u + 4, lan_u.por_s);
    hed_u->nex_y = HOP_SHORT;
    return;
  }

  hed_u->nex_y = HOP_MANY;

  u3_mesa_hop_once* lan_y = c3_calloc(sizeof(u3_mesa_hop_once));

  c3_etch_word(lan_y->dat_y, lan_u.pip_w);
  c3_etch_short(lan_y->dat_y, lan_u.por_s);

  lan_y->len_w = 6;

  c3_realloc(&pag_u->man_u, pag_u->man_u.len_w + 8);
  pag_u->man_u.dat_y[pag_u->man_u.len_w] = *lan_y;

  pag_u->man_u.len_w++;

}

static void
_mesa_hear_page(u3_mesa_pict* pic_u, u3_lane lan_u)
{
  #ifdef MESA_DEBUG
    u3l_log("mesa: hear_page()");
    log_pact(&pic_u->pac_u);
    u3_assert( PACT_PAGE == pic_u->pac_u.hed_u.typ_y );
  #endif

  u3_mesa* sam_u = pic_u->sam_u;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa_name* nam_u = &pac_u->pek_u.nam_u;
  c3_s fra_s;

  c3_d* her_d = nam_u->her_u;
  c3_o  our_o = __( 0 == memcmp(her_d, sam_u->pir_u->who_d, sizeof(*her_d) * 2) );

  //  forwarding wrong, need a PIT entry
  // if ( c3n == our_o ) {
  //   _mesa_forward_response(pic_u, lan_u);
  //   _mesa_free_pict(pic_u);
  //   return;
  // }

  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_u);
  c3_o new_o = c3n;
  if ( NULL == per_u ) {
    new_o = c3y;
    per_u = c3_calloc(sizeof(u3_peer));
    _init_peer(sam_u, per_u);
    _meet_peer(sam_u, per_u, nam_u->her_u);
  }

  c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
  if ( pac_u->hed_u.hop_y == 0 ) {
    _hear_peer(sam_u, per_u, lan_u, dir_o);
  } else {
    u3l_log(" received forwarded page");
  }
  if ( new_o == c3y ) {
    //u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    //_log_lane(&lan_u);
  }

  _mesa_put_peer(sam_u, nam_u->her_u, per_u);

  u3_weak pin = _mesa_get_pit(sam_u, nam_u);

  if ( u3_none == pin ) {
    #ifdef MESA_DEBUG
      u3l_log(" no PIT entry");
    #endif
    return;
  }
  u3_noun our, las;
  u3x_cell(pin, &our, &las);
  if ( u3_nul != las ) {
    #ifdef MESA_DEBUG
      u3l_log(" forwarding");
    #endif

    inc_hopcount(&pac_u->hed_u);
    c3_etch_word(pac_u->pag_u.sot_u, lan_u.pip_w);
    c3_etch_short(pac_u->pag_u.sot_u + 4, lan_u.por_s);

    //  stick next hop in packet
    _mesa_add_hop(pac_u->hed_u.hop_y, &pac_u->hed_u ,&pac_u->pag_u, lan_u);

    _mesa_send_pact(sam_u, u3k(las), per_u, pac_u);
    _mesa_del_pit(sam_u, nam_u);
    _mesa_free_pict(pic_u);
    u3z(pin);
    return;
  }
  if ( c3n == our ) {
    // TODO: free pact and pict
    u3z(pin);
    return;
  }

  // process incoming response to ourselves
  // TODO: memory management, maybe free pict and pact

  // if single-fragment message, inject directly into Arvo
  if ( 1 == pac_u->pag_u.dat_u.tot_w ) {
    u3_noun cad;
    {
      u3_noun lan = u3_mesa_encode_lane(lan_u);

      //  XX should just preserve input buffer
      u3i_slab sab_u;
      u3i_slab_init(&sab_u, 3, PACT_SIZE);
      c3_w cur_w  = mesa_etch_pact(sab_u.buf_y, pac_u);

      cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));
    }

    u3_noun wir = u3nc(c3__ames, u3_nul);

    u3_ovum* ovo = u3_ovum_init(0, c3__ames, wir, cad);
             ovo = u3_auto_plan(&sam_u->car_u, ovo);

    //  XX only inject dear if we hear a different, direct lane
    if ( pac_u->hed_u.hop_y == 0 && _mesa_lanes_equal(&per_u->dan_u, &lan_u) == c3n) {
      //  XX should put in cache on success
      u3_mesa_lane_cb_data* dat_u = c3_malloc(sizeof(u3_mesa_lane_cb_data));
      {
        memcpy(dat_u->her_u, nam_u->her_u, 16);
        dat_u->lan_u.pip_w = lan_u.pip_w;
        dat_u->lan_u.por_s = lan_u.por_s;
        dat_u->per_u = per_u;
      }
      u3_auto_peer(ovo, dat_u, _mesa_page_news_cb, _mesa_page_bail_cb);
    }

    _mesa_free_pict(pic_u);
    u3z(pin);
    return;
  }

  u3_pend_req* req_u = _mesa_get_request(sam_u, nam_u);
  if ( !req_u ) {
    if ( c3y == nam_u->nit_o ) {
      _mesa_req_pact_init(sam_u, pic_u, &lan_u);
    }
    _mesa_free_pict(pic_u);
    // TODO free pin, other things too?
    return;
  }

  u3_lane lon_u;
  if ( HOP_SHORT == pac_u->hed_u.nex_y ) {
    lon_u.pip_w = c3_sift_word(pac_u->pag_u.sot_u);
    lon_u.por_s = c3_sift_short(pac_u->pag_u.sot_u + 4);
  }
  else {
    lon_u = lan_u;
  }
  _mesa_req_pact_done(req_u,
                      nam_u,
                      &pac_u->pag_u.dat_u,
                      pac_u->hed_u.hop_y,
                      lon_u);
  //  TODO: check return value before continuing?

  c3_o done_with_jumbo_frame = __(req_u->len_w == req_u->tot_w); // TODO: fix for non-message-sized jumbo frames
  if ( c3y == done_with_jumbo_frame ) {
    u3_noun cad;
    {
      // construct jumbo frame
      pac_u->pag_u.nam_u.boq_y = 31; // TODO: use actual jumbo bloq
      pac_u->pag_u.dat_u.tot_w = 1;
      pac_u->pag_u.nam_u.fra_w = 0;
      c3_w jumbo_len_w = (1024 * (req_u->tot_w - 1)) + pac_u->pag_u.dat_u.len_w;
      pac_u->pag_u.dat_u.len_w = jumbo_len_w;
      pac_u->pag_u.dat_u.fra_y = req_u->dat_y;
      pac_u->pag_u.dat_u.aum_u = req_u->aum_u;

      u3_noun lan = u3_mesa_encode_lane(lan_u);

      c3_y* buf_y = c3_calloc(mesa_size_pact(pac_u));
      c3_w res_w = mesa_etch_pact(buf_y, pac_u);
      cad = u3nt(c3__heer, lan, u3i_bytes(res_w, buf_y));
      c3_free(buf_y);
    }

    _mesa_del_request(sam_u, &pac_u->pag_u.nam_u);

    u3_auto_plan(&sam_u->car_u,
                 u3_ovum_init(0, c3__ames, u3nc(c3__ames, u3_nul), cad));

    u3l_log(" received last packet, tot_w: %u", req_u->tot_w);
    c3_d now_d = _get_now_micros();
    u3l_log("%u kilobytes took %f ms", req_u->tot_w, (now_d - sam_u->tim_d)/1000.0);
  }

  if ( req_u->len_w < req_u->tot_w ) {
    _mesa_request_next_fragments(sam_u, req_u, lan_u);
  }

}

static void
_mesa_forward_request(u3_mesa* sam_u, u3_mesa_pict* pic_u, u3_lane lan_u)
{
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_peer* per_u = _mesa_get_peer(sam_u, pac_u->pek_u.nam_u.her_u);
  if ( !per_u ) {
    #ifdef MESA_DEBUG
      c3_c* mes = u3_ship_to_string(pac_u->pek_u.nam_u.her_u);
      u3l_log("mesa: alien forward for %s", mes);
      c3_free(mes);
    #endif
    _mesa_free_pict(pic_u);
    return;
  }
  if ( c3y == sam_u->for_o && sam_u->pir_u->who_d[0] == per_u->imp_y ) {
    u3_lane lin_u = _mesa_get_direct_lane(sam_u, pac_u->pek_u.nam_u.her_u);
    u3_lane zer_u = {0, 0};
    if ( _mesa_lanes_equal(&zer_u, &lin_u) == c3y) {
      _mesa_free_pict(pic_u);
      return;
    }
    inc_hopcount(&pac_u->hed_u);
    #ifdef MESA_DEBUG
      u3l_log("mesa: forward_request()");
      log_pact(pac_u);
    #endif
    _mesa_add_lane_to_pit(sam_u, &pac_u->pek_u.nam_u, lan_u);
    _mesa_send(pic_u, &lin_u);
  }
  _mesa_free_pict(pic_u);
}

static void
_mesa_hear_peek(u3_mesa_pict* pic_u, u3_lane lan_u)
{
  #ifdef MESA_DEBUG
    u3l_log("mesa: hear_peek()\r\n\r\n\r\n\r\n\r\n");
    u3_assert( PACT_PEEK == pic_u->pac_u.hed_u.typ_y );
  #endif

  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  c3_o our_o = u3_ships_equal(pac_u->pek_u.nam_u.her_u, sam_u->pir_u->who_d);

  if ( c3n == our_o ) {
    u3l_log(" forwarding\r\n");
    _mesa_forward_request(sam_u, pic_u, lan_u);
    return;
  }
  // record interest
  _mesa_add_lane_to_pit(sam_u, &pac_u->pek_u.nam_u, lan_u);

  c3_w  fra_w = pac_u->pek_u.nam_u.fra_w;
  c3_w  bat_w = _mesa_lop(fra_w);

  pac_u->pek_u.nam_u.fra_w = bat_w;
  // XX HACK: shouldn't be necessary to change data 0 to init, but
  // for some reason it's changing the data returned by the scry
  if ( pac_u->pek_u.nam_u.fra_w == 0 ) {
    pac_u->pek_u.nam_u.nit_o = c3y;
  }

  // if we have the page, send it
  u3_weak hit = _mesa_get_jumbo_cache(sam_u, &pac_u->pek_u.nam_u);
  if ( u3_none != hit ) {
    u3_noun tag, dat;
    u3x_cell(hit, &tag, &dat);
    if ( MESA_ITEM == tag ) {
      _mesa_send_jumbo_pieces(sam_u, u3k(dat));
      _mesa_free_pict(pic_u);
    }
    u3z(hit);
    return;
  }
  // otherwise, scry
  _mesa_put_jumbo_cache(sam_u, &pac_u->pek_u.nam_u, u3nc(MESA_WAIT, 0));
  u3_noun sky = _name_to_jumbo_scry(&pac_u->pek_u.nam_u);
  u3_noun our = u3i_chubs(2, sam_u->car_u.pir_u->who_d);
  u3_noun bem = u3nc(u3nt(our, u3_nul, u3nc(c3__ud, 1)), sky);
  // NOTE: pic_u not freed
  u3_pier_peek(sam_u->car_u.pir_u, u3_nul, u3k(u3nq(1, c3__beam, c3__ax, bem)), pic_u, _mesa_page_scry_jumbo_cb);
  u3z(hit);
}

static void
_mesa_poke_news_cb(u3_ovum* egg_u, u3_ovum_news new_e)
{

  if ( u3_ovum_done != new_e ) {
    #ifdef MESA_DEBUG
      u3l_log("mesa: arvo poke event was not a success");
    #endif
    return;
  }

  u3_mesa_lane_cb_data* dat_u = egg_u->ptr_v;
  u3_peer* per_u = dat_u->per_u;

  #ifdef MESA_DEBUG
    c3_c* her_c = u3_ship_to_string(dat_u->her_u);
    u3l_log("mesa: %%dear %s", her_c);
    _log_lane(&dat_u->lan_u);
    c3_free(her_c);
  #endif

  c3_free(dat_u);
}

static void
_mesa_poke_bail_cb(u3_ovum* egg_u, u3_noun lud)
{
  u3_mesa_pict* pic_u = egg_u->ptr_v;
  // XX failure stuff here
  u3l_log("mesa: poke failure");
}

static void
_mesa_hear_poke(u3_mesa_pict* pic_u, u3_lane* lan_u)
{
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;

  #ifdef MESA_DEBUG
    u3l_log("mesa: hear_poke()");
    u3_assert( PACT_POKE == pac_u->hed_u.typ_y );
  #endif

  c3_o our_o = u3_ships_equal(pac_u->pek_u.nam_u.her_u, sam_u->pir_u->who_d);

  if ( c3n == our_o ) {
    _mesa_forward_request(sam_u, pic_u, *lan_u);
    return;
  }

  _mesa_add_lane_to_pit(sam_u, &pac_u->pek_u.nam_u, *lan_u);

  //  XX if this lane management stuff is necessary
  // it should be deferred to after successful event processing
  u3_peer* per_u = _mesa_get_peer(sam_u, pac_u->pok_u.pay_u.her_u);
  c3_o new_o = c3n;
  if ( NULL == per_u ) {
    new_o = c3y;
    per_u = c3_calloc(sizeof(u3_peer));
    _init_peer(sam_u, per_u);
    _meet_peer(sam_u, per_u, pac_u->pok_u.pay_u.her_u);
  }

  c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
  if ( pac_u->hed_u.hop_y == 0 ) {
    new_o = c3y;
    _hear_peer(sam_u, per_u, *lan_u, dir_o);
    // u3l_log("learnt lane");
  } else {
    // u3l_log("received forwarded poke");
  }
  if ( new_o == c3y ) {
    // u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    // _log_lane(lan_u);
  }
  _mesa_put_peer(sam_u, pac_u->pok_u.pay_u.her_u, per_u);

  u3_ovum_peer nes_f;
  u3_ovum_bail bal_f;
  void*        ptr_v;

  u3_noun wir = u3nc(c3__ames, u3_nul);
  u3_noun cad;
  {
    u3_noun    lan = u3_mesa_encode_lane(*lan_u);
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, 3, PACT_SIZE);

    //  XX should just preserve input buffer
    mesa_etch_pact(sab_u.buf_y, pac_u);

    cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));
  }

  u3_ovum* ovo = u3_ovum_init(0, c3__ames, wir, cad);
           ovo = u3_auto_plan(&sam_u->car_u, ovo);

  // if ( 1 == pac_u->pok_u.dat_u.tot_w ) {
  //   u3l_log("free poke");
  //   _mesa_free_pict(pic_u);
  // }
  // else {
  //   u3l_log("inject poke");

    //  XX check request state for *payload* (in-progress duplicate)
    assert(pac_u->pok_u.dat_u.tot_w);
    u3_mesa_lane_cb_data* dat_u = c3_malloc(sizeof(u3_mesa_lane_cb_data));
    {
      memcpy(dat_u->her_u, pac_u->pok_u.pay_u.her_u, 16);
      dat_u->lan_u.pip_w = lan_u->pip_w;
      dat_u->lan_u.por_s = lan_u->por_s;
      dat_u->per_u = per_u;
    }
    u3_auto_peer(ovo, dat_u, _mesa_poke_news_cb, _mesa_poke_bail_cb);
  // }
}

void
_ames_hear(void*    sam_u,
           u3_lane* lan_u,
           c3_w     len_w,
           c3_y*    hun_y);

static void
_mesa_hear(u3_mesa* sam_u,
           u3_lane* lan_u,
           c3_w     len_w,
           c3_y*    hun_y)
{
  u3_mesa_pict* pic_u;
  c3_w pre_w;
  c3_y* cur_y = hun_y;
  if ( HEAD_SIZE > len_w ) {
    c3_free(hun_y);
    return;
  }

  pic_u = c3_calloc(sizeof(u3_mesa_pict));
  pic_u->sam_u = sam_u;

  c3_w lin_w = mesa_sift_pact(&pic_u->pac_u, hun_y, len_w);

  if ( lin_w == 0 ) {
    // MESA_LOG(sam_u, SERIAL)
    // c3_free(hun_y);
    mesa_free_pact(&pic_u->pac_u);
    _ames_hear(u3_Host.sam_u, lan_u, len_w, hun_y);
    return;
  }

  c3_free(hun_y);

  switch ( pic_u->pac_u.hed_u.typ_y ) {
    case PACT_PEEK: {
      _mesa_hear_peek(pic_u, *lan_u);
    } break;
    case PACT_PAGE: {
      _mesa_hear_page(pic_u, *lan_u);
    } break;
    default: {
      _mesa_hear_poke(pic_u, lan_u);
    } break;
  }
}

static void _mesa_recv_cb(uv_udp_t*        wax_u,
              ssize_t          nrd_i,
              const uv_buf_t * buf_u,
              const struct sockaddr* adr_u,
              unsigned         flg_i)
{
  if ( 0 > nrd_i ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("mesa: recv: fail: %s", uv_strerror(nrd_i));
    }
    c3_free(buf_u->base);
  }
  else if ( 0 == nrd_i ) {
    c3_free(buf_u->base);
  }
  else if ( flg_i & UV_UDP_PARTIAL ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("mesa: recv: fail: message truncated");
    }
    c3_free(buf_u->base);
  }
  else {
    u3_mesa*            sam_u = wax_u->data;
    struct sockaddr_in* add_u = (struct sockaddr_in*)adr_u;
    u3_lane             lan_u;


    lan_u.por_s = ntohs(add_u->sin_port);
   // u3l_log("port: %s", lan_u.por_s);
    lan_u.pip_w = ntohl(add_u->sin_addr.s_addr);
  //  u3l_log("IP: %x", lan_u.pip_w);
    //  NB: [nrd_i] will never exceed max length from _ames_alloc()
    //
    _mesa_hear(sam_u, &lan_u, (c3_w)nrd_i, (c3_y*)buf_u->base);
  }
}

static void
_mesa_io_talk(u3_auto* car_u)
{
  u3_mesa* sam_u = (u3_mesa*)car_u;
  sam_u->dns_c = "urbit.org"; // TODO: receive turf
  {
    //  XX remove [sev_l]
    //
    u3_noun wir = u3nt(c3__ames,
                       u3dc("scot", c3__uv, sam_u->sev_l),
                       u3_nul);
    u3_noun cad = u3nc(c3__born, u3_nul);

    u3_auto_plan(car_u, u3_ovum_init(0, c3__a, wir, cad));
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

    if ( (ret_i = uv_udp_bind(&u3_Host.wax_u,
                              (const struct sockaddr*)&add_u, 0)) != 0 )
    {
      u3l_log("mesa: bind: %s", uv_strerror(ret_i));

      /*if ( (c3__czar == rac) &&
           (UV_EADDRINUSE == ret_i) )
      {
        u3l_log("    ...perhaps you've got two copies of vere running?");
      }*/

      //  XX revise
      //
      u3_pier_bail(u3_king_stub());
    }

    uv_udp_getsockname(&u3_Host.wax_u, (struct sockaddr *)&add_u, &add_i);
    u3_assert(add_u.sin_port);

    sam_u->pir_u->por_s = ntohs(add_u.sin_port);
  }
  if ( c3y == u3_Host.ops_u.net ) {
    u3l_log("mesa: live on %d", sam_u->pir_u->por_s);
  }
  else {
    u3l_log("mesa: live on %d (localhost only)", sam_u->pir_u->por_s);
  }

  u3_Host.wax_u.data = sam_u;
  uv_udp_recv_start(&u3_Host.wax_u, _ames_alloc, _mesa_recv_cb);

  sam_u->car_u.liv_o = c3y;
  //u3z(rac); u3z(who);
}

/* _mesa_io_init(): initialize ames I/O.
*/
u3_auto*
u3_mesa_io_init(u3_pier* pir_u)
{
  u3l_log("mesa: INIT");
  u3_mesa* sam_u  = c3_calloc(sizeof(*sam_u));
  sam_u->pir_u    = pir_u;

  //  XX tune cache sizes
  sam_u->her_p = u3h_new_cache(100000);
  sam_u->lan_p = u3h_new_cache(100000);
  sam_u->pac_p = u3h_new_cache(300000);
  sam_u->pit_p = u3h_new_cache(10000);

  u3_assert( !uv_udp_init(u3L, &sam_u->wax_u) );
  sam_u->wax_u.data = sam_u;

  sam_u->sil_u = u3s_cue_xeno_init();
  sam_u->tes_u = ur_cue_test_init();

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
      u3l_log("mesa: forwarding enabled");
      sam_u->for_o = c3y;
    }
    u3z(her);
  }


  u3_auto* car_u = &sam_u->car_u;
  car_u->nam_m = c3__ames;
  car_u->liv_o = c3y;
  car_u->io.talk_f = _mesa_io_talk;
  car_u->io.info_f = _mesa_io_info;
  car_u->io.slog_f = _mesa_io_slog;
  car_u->io.kick_f = _mesa_io_kick;
  car_u->io.exit_f = _mesa_io_exit;



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

