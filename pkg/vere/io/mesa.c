/// @file

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"
#include "cubic.h"
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
#include "blake.h"
#include "lss.h"

c3_o dop_o = c3n;

#define MESA_DEBUG     c3y
//#define MESA_TEST
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
#define REORDER_THRESH  5

// logging and debug symbols
#define MESA_SYM_DESC(SYM) MESA_DESC_ ## SYM
#define MESA_SYM_FIELD(SYM) MESA_FIELD_ ## SYM
#ifdef MESA_DEBUG
  #define MESA_LOG(SYM, ...) { sam_u->sat_u.MESA_SYM_FIELD(SYM)++; u3l_log("mesa: (%u) %s", __LINE__, MESA_SYM_DESC(SYM)); }
#else
  #define MESA_LOG(SYM, ...) { sam_u->sat_u.MESA_SYM_FIELD(SYM)++; }
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
  c3_w      num_w;
  lss_pair* par_u;
} u3_misord_buf;

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
  c3_w                   nex_w; // number of the next fragment to be sent
  c3_w                   tot_w; // total number of fragments expected
  uv_timer_t             tim_u; // timehandler
  c3_y*                  dat_y; // ((mop @ud *) lte)
  c3_w                   len_w;
  c3_w                   lef_w; // lowest fragment number currently in flight/pending
  c3_w                   old_w; // frag num of oldest packet sent
  c3_w                   ack_w; // highest acked fragment number
  u3_gage*               gag_u; // congestion control
  u3_vec(u3_buf)         mis_u; // misordered packets
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
  c3_w res_w = ((( fra_w ) / MESA_HUNK) * MESA_HUNK);
  return res_w;
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

static inline void
_get_her(u3_mesa_pact* pac_u, c3_d* our_d)
{
  switch ( pac_u->hed_u.typ_y ) {
    default: {
      u3m_bail(c3__foul);
      break;
    }
    case PACT_PAGE: {
      memcpy(our_d, pac_u->pag_u.nam_u.her_d,2);
      break;
    }
    case PACT_PEEK: {
      memcpy(our_d, pac_u->pek_u.nam_u.her_d,2);
      break;
    }
    case PACT_POKE: {
      memcpy(our_d, pac_u->pok_u.pay_u.her_d, 2);
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
_mesa_lane_key(c3_d her_d[2], u3_lane* lan_u)
{
  return u3nc(u3i_chubs(2,her_d), u3_mesa_encode_lane(*lan_u));
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
_mesa_get_peer(u3_mesa* sam_u, c3_d her_d[2])
{
  return _mesa_get_peer_raw(sam_u, u3i_chubs(2, her_d));
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
_mesa_put_peer(u3_mesa* sam_u, c3_d her_d[2], u3_peer* per_u)
{
  _mesa_put_peer_raw(sam_u, u3i_chubs(2, her_d), per_u);
}

/* _mesa_get_request(): produce pending request state for nam_u
 *
 *   produces a NULL pointer if no pending request exists
*/
static u3_pend_req*
_mesa_get_request(u3_mesa* sam_u, u3_mesa_name* nam_u) {
  u3_pend_req* ret_u = NULL;

  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_d);
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
  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_d);
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
  vec_free(&req_u->mis_u);
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
  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_d);
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

static u3_lane
_mesa_get_direct_lane(u3_mesa* sam_u, c3_d her_d[2])
{
  return _mesa_get_direct_lane_raw(sam_u, u3i_chubs(2, her_d));
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
_mesa_get_lane(u3_mesa* sam_u, c3_d her_d[2], u3_lane* lan_u) {
  u3_noun key =_mesa_lane_key(her_d, lan_u);
  u3_gage* ret_u = _mesa_get_lane_raw(sam_u, key);
  u3z(key);
  return ret_u;
}

/* _mesa_put_lane(): put lane state in state
 *
 *   uses same copying trick as _mesa_put_request()
*/
static void
_mesa_put_lane(u3_mesa* sam_u, c3_d her_d[2], u3_lane* lan_u, u3_gage* gag_u)
{
  u3_noun key = _mesa_lane_key(her_d, lan_u);
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

/*
 * _mesa_req_get_cwnd(): produce packets to send
 *
 * saves next fragment number and preallocated pact into the passed pointers.
 * Will not do so if returning 0
*/
static c3_w
_mesa_req_get_cwnd(u3_pend_req* req_u)
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
  /* u3l_log("rem_w: %u", rem_w); */
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

    /* u3l_log("bitset_put %u", nam_u->fra_w); */
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
  /* c3_c* sip_c = inet_ntoa(add_u.sin_addr); */
  /* u3l_log("mesa: sending packet (%s,%u)", sip_c, por_s); */
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

//  TODO: check whether direct, decide what to send
static void
_try_resend(u3_pend_req* req_u)
{
  c3_o los_o = c3n;
  c3_d now_d = _get_now_micros();
  u3_mesa_pact *pac_u = &req_u->pic_u->pac_u;

  for ( int i = req_u->lef_w; i < req_u->nex_w; i++ ) {
    //  TODO: make fast recovery different from slow
    //  TODO: track skip count but not dupes, since dupes are meaningless
    if ( ( c3y == bitset_has(&req_u->was_u, i) ) &&
       ( (now_d - req_u->wat_u[i].sen_d) > req_u->gag_u->rto_w ) ) {
      los_o = c3y;

      pac_u->pek_u.nam_u.fra_w = i;
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w siz_w  = mesa_etch_pact(buf_y, pac_u);
      if ( 0 == siz_w ) {
        u3l_log("failed to etch");
        u3_assert( 0 );
      }
      // TODO: better route management
      // it needs to be more ergonomic to access the u3_peer
      //    could do a backpointer to the u3_peer from the u3_pend_req
      _mesa_send_buf(req_u->pic_u->sam_u, req_u->lan_u, buf_y, siz_w);
      _mesa_req_pact_resent(req_u, &pac_u->pek_u.nam_u);
    }
  }

  if ( c3y == los_o ) {
    req_u->gag_u->sst_w = (req_u->gag_u->wnd_w / 2) + 1;
    req_u->gag_u->wnd_w = req_u->gag_u->sst_w;
    req_u->gag_u->rto_w = _clamp_rto(req_u->gag_u->rto_w * 2);
  }
}

static void
_mesa_packet_timeout(uv_timer_t* tim_u);

static void
_update_oldest_req(u3_pend_req *req_u, u3_gage* gag_u)
{
  if( req_u->tot_w == 0 || req_u->len_w == req_u->tot_w ) {
    /* u3l_log("bad condition"); */
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
  uv_timer_start(&req_u->tim_u, _mesa_packet_timeout, (gag_u->rto_w - gap_d) / 1000, 0);
}

/* _mesa_packet_timeout(): callback for packet timeout
*/
static void
_mesa_packet_timeout(uv_timer_t* tim_u) {
  u3_pend_req* req_u = (u3_pend_req*)tim_u->data;
  /* u3l_log("%u packet timed out", req_u->old_w); */
  _try_resend(req_u);
  _update_oldest_req(req_u, req_u->gag_u);
}

static void _mesa_free_misord_buf(u3_misord_buf* buf_u)
{
  c3_free(buf_u->fra_y);
  if ( buf_u->par_u != NULL ) {
    c3_free(buf_u->par_u);
  }
  c3_free(buf_u);
}

static c3_o
_mesa_burn_misorder_queue(u3_pend_req* req_u)
{
  c3_w wan_w = req_u->los_u->counter;
  c3_w len_w;
  c3_o fon_o;
  c3_o res_y = c3y;
  while ( (len_w = vec_len(&req_u->mis_u)) != 0 ) {
    fon_o = c3n;
    for (int i = 0; i < len_w; i++) {
      u3_misord_buf* buf_u = (u3_misord_buf*)req_u->mis_u.vod_p[i];
      if ( buf_u->num_w == wan_w ) {
        fon_o = c3y;
        if ( c3y != (res_y = lss_verifier_ingest(req_u->los_u, buf_u->fra_y, buf_u->len_w, buf_u->par_u))) {
          return res_y;
        } else {
          /* u3l_log("burn %u", buf_u->num_w); */
          /* memcpy(req_u->dat_y + (1024 * buf_u->num_w), buf_u->fra_y, buf_u->len_w); */
          _mesa_free_misord_buf((u3_misord_buf*)vec_pop(&req_u->mis_u, i));
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

/* _mesa_req_pact_done(): mark packet as done, returning if we should continue
*/
static u3_pend_req*
_mesa_req_pact_done(u3_mesa* sam_u, u3_mesa_name *nam_u, u3_mesa_data* dat_u, u3_lane* lan_u)
{
  u3_weak ret = u3_none;
  c3_d now_d = _get_now_micros();
  u3_pend_req* req_u = _mesa_get_request(sam_u, nam_u);

  if ( NULL == req_u ) {
    /* MESA_LOG(APATHY); */
    return NULL;
  }

  u3_gage* gag_u = _mesa_get_lane(sam_u, nam_u->her_d, lan_u);
  req_u->gag_u = gag_u;

  // first we hear from lane
  if ( gag_u == NULL ) {
    gag_u = alloca(sizeof(u3_gage));
    memset(gag_u, 0, sizeof(u3_gage));
    _init_gage(gag_u);
  }

  //  TODO: fix this
  req_u->lan_u = *lan_u;

  c3_w siz_w = (1 << (nam_u->boq_y - 3));
  // First packet received, instantiate request fully

  if ( dat_u->tot_w <= nam_u->fra_w ) {
    MESA_LOG(STRANGE);
    //  XX: is this sufficient to drop whole request
    return req_u;
  }

  // received duplicate
  if ( c3n == bitset_has(&req_u->was_u, nam_u->fra_w) ) {
    /* MESA_LOG(DUPE); */
    return req_u;
  }

  /* u3l_log("bitset_del %u", nam_u->fra_w); */
  bitset_del(&req_u->was_u, nam_u->fra_w);
  if ( nam_u->fra_w > req_u->ack_w ) {
    req_u->ack_w = nam_u->fra_w;
  }
  if ( nam_u->fra_w != 0 && req_u->wat_u[nam_u->fra_w].tie_y != 1 ) {
#ifdef MESA_DEBUG
    /* u3l_log("received retry %u", nam_u->fra_w); */
#endif
  }

  req_u->len_w++;
  /* u3l_log("fragment %u len %u", nam_u->fra_w, req_u->len_w); */
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

  c3_y ver_y;
  // TODO: move to bottom

  c3_y buf_y[1024];
  memcpy(buf_y, dat_u->fra_y, dat_u->len_w);
  memset(buf_y + dat_u->len_w, 0, 1024 - dat_u->len_w);
  c3_w len_w = (nam_u->fra_w + 1 == dat_u->tot_w) ? dat_u->len_w : 1024;

  if ( req_u->los_u->counter != nam_u->fra_w ) {
    // TODO: queue packet
    u3_misord_buf* buf_u = c3_calloc(sizeof(u3_misord_buf));
    buf_u->fra_y = c3_calloc(len_w);
    buf_u->len_w = len_w;
    memcpy(buf_u->fra_y, buf_y, len_w);
    buf_u->par_u = par_u;
    buf_u->num_w = nam_u->fra_w;
    vec_append(&req_u->mis_u, buf_u);
  }
  else if ( c3y != (ver_y = lss_verifier_ingest(req_u->los_u, buf_y, len_w, par_u)) ) {
    c3_free(par_u);
    // TODO: do we drop the whole request on the floor?
    u3l_log("auth fail frag %u", nam_u->fra_w);
    MESA_LOG(AUTH);
    return req_u;
  }
  else if ( vec_len(&req_u->mis_u) != 0
            && c3y != (ver_y = _mesa_burn_misorder_queue(req_u))) {
    c3_free(par_u);
    MESA_LOG(AUTH)
    return req_u;
  }
  else {
    c3_free(par_u);
  }

  // handle gauge update
  _mesa_handle_ack(gag_u, &req_u->wat_u[nam_u->fra_w]);


  memcpy(req_u->dat_y + (siz_w * nam_u->fra_w), dat_u->fra_y, dat_u->len_w);

  _try_resend(req_u);

  _update_oldest_req(req_u, gag_u);

  // _mesa_put_request(sam_u, nam_u, req_u);
  // _mesa_put_lane(sam_u, nam_u->her_d, lan_u, gag_u);
  return req_u;
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

static c3_o
_mesa_rout_bufs(u3_mesa* sam_u, c3_y* buf_y, c3_w len_w, u3_noun las)
{
  c3_o suc_o = c3n;
  u3_noun lan, t = las;
  // u3l_log("sending to ip: %x, port: %u", lan_u.pip_w, lan_u.por_s);
  while ( t != u3_nul ) {
    u3x_cell(t, &lan, &t);
    u3_lane lan_u = _realise_lane(u3k(lan));

    #ifdef MESA_DEBUG
     /* u3l_log("sending to ip: %x, port: %u", lan_u.pip_w, lan_u.por_s); */

    #endif /* ifdef MESA_DEBUG
        u3l_log("sending to ip: %x, port: %u", lan_u.pip_w, lan_u.por_s); */
    if ( lan_u.por_s == 0 ) {
      u3l_log("mesa: failed to realise lane");
    } else {
      c3_y* sen_y = c3_calloc(len_w);
      memcpy(sen_y, buf_y, len_w);
      _mesa_send_buf(sam_u, lan_u, sen_y, len_w);
    }
  }
  // u3z(las);
  return suc_o;
}

static void
_mesa_timer_cb(uv_timer_t* tim_u) {
  u3_pend_req* req_u = tim_u->data;
  _try_resend(req_u);
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

static void
_mesa_ef_send(u3_mesa* sam_u, u3_noun las, u3_noun pac)
{
  // u3m_p("pac", pac);
  /* u3m_p("las", las); */
  las = _mesa_queue_czar(sam_u, las, u3k(pac));
  /* u3m_p("las", las); */
  c3_w len_w = u3r_met(3, pac);
  c3_y* buf_y = c3_calloc(len_w);
  u3r_bytes(0, len_w, buf_y, pac);
  sam_u->tim_d = _get_now_micros();

  c3_o suc_o = c3n;
  _mesa_rout_bufs(sam_u, buf_y, len_w, las);

  c3_free(buf_y);
  u3z(pac);
}

c3_o
_ames_kick_newt(void* sam_u, u3_noun tag, u3_noun dat);

static c3_o _mesa_kick(u3_mesa* sam_u, u3_noun tag, u3_noun dat)
{
  c3_o ret_o;
  /* u3l_log("blabla"); */
  switch ( tag ) {
    default: {
      ret_o = c3n;
     } break;
    case c3__push: {
      u3_noun las, pac;
      if ( c3n == u3r_cell(dat, &las, &pac) ) {
        u3l_log(" mesa: send no");
        ret_o = c3n;
      } else {
        u3l_log(" mesa: send yes");
        _mesa_ef_send(sam_u, u3k(las), u3k(pac));
        ret_o = c3y;
      }
    } break;
    case c3__send:
    case c3__turf:
    case c3__saxo:
    case c3__nail: {
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
  u3h_free(per_u->req_p);
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

static u3_lane_state
_init_lane_state()
{
  u3_lane_state sat_u;
  sat_u.sen_d = 0;
  sat_u.her_d = 0;
  sat_u.rtt_w = 1000000;
  sat_u.rtv_w = 1000000;
  return sat_u;
}

static void
_init_peer(u3_mesa* sam_u, u3_peer* per_u)
{
  per_u->sam_u = sam_u;
  per_u->ful_o = c3n;
  per_u->dan_u = (u3_lane){0,0};
  per_u->dir_u = _init_lane_state();
  per_u->imp_y = 0;
  per_u->ind_u = _init_lane_state();
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

static u3_noun
_name_to_batch_scry(u3_mesa_name* nam_u, c3_w lop_w, c3_w len_w)
{
  u3_noun rif = _dire_etch_ud(nam_u->rif_w);
  u3_noun boq = _dire_etch_ud(nam_u->boq_y);
  u3_noun fag = _dire_etch_ud(nam_u->fra_w);
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);

  u3_noun lop = _dire_etch_ud(lop_w);
  u3_noun len = _dire_etch_ud(len_w);

  u3_noun wer = nam_u->nit_o == c3y
    ? u3nc(c3__init, pax)
    : u3nt(nam_u->aut_o == c3y ? c3__auth : c3__data, fag, pax);

  u3_noun res = u3nc(c3__mess, u3nq(rif, c3__pact, boq, u3nc(c3__etch, wer)));
  // [%hunk lop=@t len=@t pat=*]
  u3_noun bat = u3nq(c3__hunk, lop, len, res);

  return bat;
}

static u3_noun
_name_to_jumbo_scry(u3_mesa_name* nam_u, c3_w lop_w, c3_w len_w)
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
 * RETAIN
 */
static u3_weak
_mesa_get_cache(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  u3_noun pax = _name_to_scry(nam_u);
  u3_weak res = u3h_get(sam_u->pac_p, pax);
  if ( u3_none == res ) {
    //u3m_p("miss", u3k(pax));
  } else {
    //u3m_p("hit", u3nc(u3k(pax), u3k(res)));
  }
  return res;
}

static void
_mesa_put_cache(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_noun val)
{
  u3_noun pax = _name_to_scry(nam_u);
  u3h_put(sam_u->pac_p, pax, u3k(val));
  u3z(pax); // TODO: fix refcount
}

static void
_mesa_try_forward(u3_mesa_pict* pic_u, u3_noun fra, u3_noun hit)
{
  u3l_log("");
  u3l_log("stubbed forwarding");
}

static c3_w
_mesa_respond(u3_mesa_pict* req_u, c3_y** buf_y, u3_noun hit)
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
_mesa_page_scry_cb(void* vod_p, u3_noun nun)
{
  u3_mesa_pict* pic_u = vod_p;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  //u3_noun pax = _mesa_path_with_fra(pac_u->pek_u.nam_u.pat_c, &fra_s);

  u3_weak hit = u3r_at(7, nun);
  if ( u3_none == hit ) {
    // TODO: mark as dead
    //u3z(nun);
    u3l_log("unbound");

  } else {
    u3_weak old = _mesa_get_cache(sam_u, &pac_u->pag_u.nam_u);
    if ( old == u3_none ) {
      u3l_log("bad");
      MESA_LOG(APATHY);
    } else {
      u3_noun tag;
      u3_noun dat;
      u3x_cell(u3k(old), &tag, &dat);
      if ( MESA_WAIT == tag ) {
        c3_y* buf_y;

        c3_w len_w = _mesa_respond(pic_u, &buf_y, u3k(hit));
        _mesa_rout_bufs(sam_u, buf_y, len_w, u3k(u3t(dat)));
      }
      _mesa_put_cache(sam_u, &pac_u->pek_u.nam_u, u3nc(MESA_ITEM, u3k(hit)));
      // u3z(old);
    }
    // u3z(hit);
  }
  // u3z(pax);
}

/*
 */
static void
_mesa_page_scry_hunk_cb(void* vod_p, u3_noun nun)
{
  u3_mesa_pict* pic_u = vod_p;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  //u3_noun pax = _mesa_path_with_fra(pac_u->pek_u.nam_u.pat_c, &fra_s);

  u3_weak hit = u3r_at(7, nun);
  if ( u3_none == hit ) {
    // TODO: mark as dead
    //u3z(nun);
    u3l_log("unbound");
  } else {
    c3_w fra_w = pac_u->pek_u.nam_u.fra_w;
    c3_w  bat_w = _mesa_lop(fra_w);
    pac_u->pek_u.nam_u.fra_w = bat_w;
    u3_weak old = _mesa_get_cache(sam_u, &pac_u->pag_u.nam_u);
    if ( old == u3_none ) {
      u3l_log("bad");
      MESA_LOG(APATHY);
    } else {
      u3_noun tag;
      u3_noun dat;
      u3x_cell(u3k(old), &tag, &dat);
      c3_y* buf_y;
      // u3m_p("hit", u3a_is_cell(hit));

      c3_w len_w = bat_w;
      /* u3l_log("path %s", pac_u->pek_u.nam_u.pat_c); */
      c3_w i = 0;
      while ( u3_nul != hit ) {
        // u3_noun key = u3nc(u3k(pax), u3i_word(lop_w));
        // u3h_put(sam_u->fin_s.sac_p, key, u3k(u3h(lis)));

        pac_u->pek_u.nam_u.fra_w = len_w;

        if ( (bat_w == 0) && (i == 0) ) {
          pac_u->pek_u.nam_u.nit_o = c3y;
          /* pac_u->pek_u.nam_u.aut_o = c3y; */
        } else {
          pac_u->pek_u.nam_u.nit_o = c3n;
          pac_u->pek_u.nam_u.aut_o = c3n;
        }

        /* if (len_w == 0) { */
        /*   c3_w lun_w = _mesa_respond(pic_u, &buf_y, u3k(u3h(hit))); */
        /*   /\* pac_u->pek_u.nam_u.fra_w = fra_w; *\/ */
        /*   _mesa_rout_bufs(sam_u, buf_y, lun_w, u3k(u3t(dat))); */
        /* } */

        c3_w lun_w = _mesa_respond(pic_u, &buf_y, u3k(u3h(hit)));
        _mesa_rout_bufs(sam_u, buf_y, lun_w, u3k(u3t(dat)));

        /* u3l_log("putting %u", pac_u->pek_u.nam_u.fra_w); */
        /* _log_pact(pac_u); */

        _mesa_put_cache(sam_u, &pac_u->pek_u.nam_u, u3nc(MESA_ITEM, u3k(u3h(hit))));
        // u3z(key);

        hit = u3t(hit);

        if (!( (bat_w == 0) && (i == 0) )) {
          len_w++;
        }

        i++;
      }
      u3l_log("i %u", i);
      // u3z(old);
    }
  }
    // u3z(hit);
  // u3z(pax);
}

static void
_mesa_page_scry_jumbo_cb(void* vod_p, u3_noun nun)
{
  u3_mesa_pict* pic_u = vod_p;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  //u3_noun pax = _mesa_path_with_fra(pac_u->pek_u.nam_u.pat_c, &fra_s);

  u3_weak hit = u3r_at(7, nun);
  if ( u3_none == hit ) {
    // TODO: mark as dead
    //u3z(nun);
    u3l_log("unbound");
  } else {
    c3_w fra_w = pac_u->pek_u.nam_u.fra_w;
    u3_weak old = _mesa_get_cache(sam_u, &pac_u->pag_u.nam_u);
    if ( old == u3_none ) {
      u3l_log("bad");
      MESA_LOG(APATHY);
    } else {
      u3_noun tag;
      u3_noun dat;
      u3x_cell(u3k(old), &tag, &dat);
      c3_w siz = u3r_met(13, hit);
      c3_y* buf_y;

      /* u3l_log("path %s", pac_u->pek_u.nam_u.pat_c); */
      u3_mesa_pact tac_u;

      c3_w len_w = u3r_met(3, hit);
      u3l_log("len_w %u", len_w);
      c3_y* puf_y = c3_calloc(len_w);
      u3r_bytes(0, len_w, puf_y, hit);

      mesa_sift_pact(&tac_u, puf_y, len_w);
      _log_pact(&tac_u);

      for (c3_w i=0; i < siz; i++) {
        // (cut 3 [wid 1] dat.byts))
        u3_atom lin = u3qc_cut(13, i, 1, hit);

        pac_u->pek_u.nam_u.fra_w = i;

        _mesa_put_cache(sam_u, &pac_u->pek_u.nam_u, u3nc(MESA_ITEM, u3k(lin)));

        c3_w lun_w = _mesa_respond(pic_u, &buf_y, u3k(lin));
        _mesa_rout_bufs(sam_u, buf_y, lun_w, u3k(u3t(dat)));

        /* u3l_log("putting %u", pac_u->pek_u.nam_u.fra_w); */
        /* _log_pact(pac_u); */

      }

      // u3z(old);
    }
  }
    // u3z(hit);
  // u3z(pax);
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
_meet_peer(u3_mesa* sam_u, u3_peer* per_u, c3_d her_d[2])
{
  u3_noun her = u3i_chubs(2, her_d);
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
_mesa_forward(u3_mesa_pict* pic_u, u3_lane lan_u)
{
  u3l_log("forwarding");
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  c3_d her_d[2];
  _get_her(pac_u, her_d); // XX wrong
  // XX: revive
  //_update_hopcount(&pac_u->hed_u);

  if ( pac_u->hed_u.typ_y == PACT_PAGE ) {
    //u3l_log("should update next hop");
    u3_weak hit = _mesa_get_cache(sam_u, &pac_u->pag_u.nam_u);
    if ( u3_none == hit ) {
      MESA_LOG(APATHY);
      return;
    }
    u3_noun tag, dat;
    u3x_cell(u3k(hit), &tag, &dat);
    if ( tag == MESA_WAIT ) {
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w len_w = mesa_etch_pact(buf_y, pac_u);
      _mesa_rout_bufs(sam_u, buf_y, len_w, u3k(u3t(dat)));
    } else {
      u3l_log("mesa: weird pending interest");
    }
  } else {
    u3_peer* per_u = _mesa_get_peer(sam_u, her_d);
    if ( NULL != per_u ) {
      u3_lane lin_u = _mesa_get_direct_lane(sam_u, her_d);
      if ( lin_u.pip_w != 0 ) {
        _mesa_send(pic_u, &lin_u);
      }
    }
  }
  mesa_free_pact(pac_u);
}

static u3_pend_req*
_mesa_req_pact_init(u3_mesa* sam_u, u3_mesa_pict* pic_u, u3_lane* lan_u)
{
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa_name* nam_u = &pac_u->pag_u.nam_u;
  u3_mesa_data* dat_u = &pac_u->pag_u.dat_u;
  c3_o lin_o = dat_u->tot_w <= 4 ? c3y : c3n;

  u3_pend_req* req_u = _mesa_get_request(sam_u, nam_u);
  if ( NULL != req_u ) {
    // duplicate
    if ( req_u->tot_w <= 4 ) {
      return NULL;
    }
    return NULL;
  } else {
    req_u = alloca(sizeof(u3_pend_req));
    memset(req_u, 0, sizeof(u3_pend_req));
  }

  u3_gage* gag_u = _mesa_get_lane(sam_u, nam_u->her_d, lan_u);

  if ( gag_u == NULL ) {
    gag_u = alloca(sizeof(u3_gage));
    _init_gage(gag_u);
    // save and re-retrieve so we have persistent pointer
    _mesa_put_lane(sam_u, nam_u->her_d, lan_u, gag_u);
    gag_u = _mesa_get_lane(sam_u, nam_u->her_d, lan_u);
    u3_assert( gag_u != NULL );
  }

  req_u->pic_u = c3_calloc(sizeof(u3_mesa_pict));
  req_u->pic_u->sam_u = sam_u;
  req_u->pic_u->pac_u.hed_u.typ_y = PACT_PEEK;
  req_u->pic_u->pac_u.hed_u.pro_y = MESA_VER;
  memcpy(&req_u->pic_u->pac_u.pek_u.nam_u, nam_u, sizeof(u3_mesa_name));
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
  req_u->lef_w = 0;
  req_u->old_w = 0;
  req_u->ack_w = 0;

  if ( c3y == lin_o ) {
    // complete the proof by computing the first leaf hash
    c3_w pof_w = pac_u->pag_u.dat_u.aup_u.len_y + 1;
    if ( pof_w != lss_proof_size(req_u->tot_w) ) {
      return NULL; // XX ???
    }
    lss_hash* pof_u = c3_calloc(pof_w * sizeof(lss_hash));
    for ( int i = 1; i < pof_w; i++ ) {
      memcpy(pof_u[i], pac_u->pag_u.dat_u.aup_u.has_y[i-1], sizeof(lss_hash));
    }
    lss_complete_inline_proof(pof_u, dat_u->fra_y, dat_u->len_w);
    // TODO: authenticate root
    // lss_hash root;
    // lss_root(root, pof_u, pof_w);

    req_u->los_u = c3_calloc(sizeof(lss_verifier));
    if ( c3y != lss_verifier_init(req_u->los_u, req_u->tot_w, pof_u) ) {
      return NULL; // XX ???
    }
    c3_free(pof_u);
    if ( c3y != lss_verifier_ingest(req_u->los_u, dat_u->fra_y, dat_u->len_w, NULL) ) {
      return NULL; // XX ???
    }
    memcpy(req_u->dat_y, dat_u->fra_y, dat_u->len_w);
  } else {
    if ( dat_u->len_w != lss_proof_size(req_u->tot_w) ) {
      return NULL; // XX ???
    }
    // TODO: cast directly instead of copying?
    lss_hash* pof_u = c3_calloc(dat_u->len_w * sizeof(lss_hash));
    for ( int i = 0; i < dat_u->len_w; i++ ) {
      memcpy(pof_u[i], dat_u->fra_y + (i * sizeof(lss_hash)), sizeof(lss_hash));
    }
    // TODO: authenticate root
    // lss_hash root;
    // lss_root(root, pof_u, dat_u->len_w/32);

    req_u->los_u = c3_calloc(sizeof(lss_verifier));
    lss_verifier_init(req_u->los_u, req_u->tot_w, pof_u);
    c3_free(pof_u);
  }
  vec_init(&req_u->mis_u, 8);

  req_u = _mesa_put_request(sam_u, nam_u, req_u);
  _update_oldest_req(req_u, gag_u);
  return req_u;
}

static void
_mesa_hear_page(u3_mesa_pict* pic_u, u3_lane lan_u)
{
#ifdef MESA_DEBUG
   /* u3l_log("mesa hear page %u", pic_u->pac_u.pag_u.nam_u.fra_w); */
#endif
  u3_mesa* sam_u = pic_u->sam_u;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  c3_s fra_s;


  c3_d* her_d = pac_u->pek_u.nam_u.her_d;
  c3_o  our_o = __( 0 == memcmp(her_d, sam_u->pir_u->who_d, sizeof(*her_d) * 2) );

  //  forwarding wrong, need a PIT entry
  // if ( c3n == our_o ) {
  //   _mesa_forward(pic_u, lan_u);
  //   _mesa_free_pict(pic_u);
  //   return;
  // }

  u3_peer* per_u = _mesa_get_peer(sam_u, pac_u->pag_u.nam_u.her_d);
  c3_o new_o = c3n;
  if ( NULL == per_u ) {
    new_o = c3y;
    per_u = c3_calloc(sizeof(u3_peer));
    _init_peer(sam_u, per_u);
    _meet_peer(sam_u, per_u, pac_u->pag_u.nam_u.her_d);
  }

  c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
  if ( pac_u->hed_u.hop_y == 0 ) {
    _hear_peer(sam_u, per_u, lan_u, dir_o);
  } else {
    u3l_log("received forwarded page");
  }
  if ( new_o == c3y ) {
    //u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    //_log_lane(&lan_u);
  }

  _mesa_put_peer(sam_u, pac_u->pag_u.nam_u.her_d, per_u);

  u3_weak hit = _mesa_get_cache(sam_u, &pac_u->pag_u.nam_u);

  //  XX  better check to forward pages?
  if ( u3_none != hit ) {
    _mesa_forward(pic_u, lan_u);
    _mesa_free_pict(pic_u);
    u3z(hit);
    return;
  }

  if ( 1 == pac_u->pag_u.dat_u.tot_w ) {
    u3_noun wir = u3nc(c3__ames, u3_nul);
    u3_noun aut, cad;

    switch ( pac_u->pag_u.dat_u.aum_u.typ_e ) {
      case AUTH_SIGN: {
        aut = u3nc(c3y, u3i_bytes(64, pac_u->pag_u.dat_u.aum_u.sig_y));
      } break;

      case AUTH_HMAC: {
        aut = u3nc(c3n, u3i_bytes(32, pac_u->pag_u.dat_u.aum_u.mac_y));
      } break;

      default: {
        u3l_log("page: strange auth");
        _mesa_free_pict(pic_u);
        return;
      }
    }

    {
      // u3_noun pax = _mesa_encode_path(pac_u->pag_u.nam_u.pat_s,
      //                         (c3_y*)(pac_u->pag_u.nam_u.pat_c));
      // u3_noun par = u3nc(u3i_chubs(2, pac_u->pag_u.nam_u.her_d), pax);
      // u3_noun lan = u3nc(u3_nul, u3_mesa_encode_lane(lan_u));
      // u3_noun dat = u3i_bytes(pac_u->pag_u.dat_u.len_w,
      //                         pac_u->pag_u.dat_u.fra_y);

      // cad = u3nt(c3__mess_ser, lan,
      //            u3nq(c3__page, par, aut, dat));

      u3_noun lan = u3_mesa_encode_lane(lan_u);
      u3i_slab sab_u;
      u3i_slab_init(&sab_u, 3, PACT_SIZE);

      //  XX should just preserve input buffer
      c3_w cur_w  = mesa_etch_pact(sab_u.buf_y, pac_u);
      // _log_buf(sab_u.buf_y, cur_w);
      cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));

    }

    //  XX should put in cache on success
    u3_auto_plan(&sam_u->car_u,
                 u3_ovum_init(0, c3__ames, wir, cad));
    _mesa_free_pict(pic_u);
    return;
  }

  // u3_noun fra = u3i_bytes(pac_u->pag_u.dat_u.len_w, pac_u->pag_u.dat_u.fra_y) ;
  /*if ( dop_o == c3n && pac_u->pag_u.nam_u.fra_w == 150) {
    dop_o = c3y;
    u3l_log("simulating dropped packet");
    return;
  }*/
  u3_pend_req* req_u;

  /* req_u = _mesa_req_pact_init(sam_u, pic_u, &lan_u); */
  /* if ( req_u == NULL ) { */
  /*   req_u = _mesa_req_pact_done(sam_u, &pac_u->pag_u.nam_u, &pac_u->pag_u.dat_u, &lan_u); */
  /*   if ( req_u == NULL ) { */
  /*     // cleanup */
  /*     /\* u3l_log("wrong"); *\/ */
  /*     _log_pact(pac_u); */
  /*     _mesa_free_pict(pic_u); */
  /*     return; */
  /*   } */
  /* } */

  if ( pac_u->pek_u.nam_u.nit_o == c3y ) {
    /* u3l_log("_mesa_req_pact_init NIT"); */
    req_u = _mesa_req_pact_init(sam_u, pic_u, &lan_u);
    if ( req_u == NULL ) {
      _mesa_free_pict(pic_u);
      return;
    }
  } else {
    req_u = _mesa_req_pact_done(sam_u, &pac_u->pag_u.nam_u, &pac_u->pag_u.dat_u, &lan_u);
    if ( req_u == NULL ) {
      // cleanup
      /* u3l_log("wrong"); */
      /* _log_pact(pac_u); */
      _mesa_free_pict(pic_u);
      return;
    }
    /* u3l_log("right"); */
    /* _log_pact(pac_u); */
  }

  if ( req_u == NULL ) {
    u3_assert(!"invalid");
    return;
  }
  c3_w win_w = _mesa_req_get_cwnd(req_u);
  /* c3_w win_w = 100000; */
  u3_mesa_pict* nex_u = req_u->pic_u;
  c3_w nex_w = req_u->nex_w;
  if ( win_w != 0 ) {
#ifdef MESA_DEBUG
    /* u3l_log("continuing flow nex: %u, win: %u", nex_w, win_w); */
    /* u3l_log("in flight %u", bitset_wyt(&req_u->was_u)); */
#endif
    for ( int i = 0; i < win_w; i++ ) {
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w fra_w = nex_w + i;
      if ( fra_w >= req_u->tot_w ) {
        break;
      }
      nex_u->pac_u.pek_u.nam_u.fra_w = nex_w + i;
      c3_w siz_w  = mesa_etch_pact(buf_y, &nex_u->pac_u);
      // _log_buf(buf_y, siz_w);
      if ( siz_w == 0 ) {
        u3l_log("failed to etch");
        u3_assert( 0 );
      }
      // TODO: better route management
      _mesa_send_buf(sam_u, lan_u, buf_y, siz_w);
      _mesa_req_pact_sent(req_u, &nex_u->pac_u.pek_u.nam_u);
    }
  }
  if ( req_u->len_w == req_u->tot_w ) {
    // fprintf(stderr, "finished");
    // u3l_log("queue size %u", req_u->mis_u.len_w);
    c3_d now_d = _get_now_micros();
    u3l_log("%u kilobytes took %f ms", req_u->tot_w, (now_d - sam_u->tim_d)/1000.0);
    c3_w siz_w = (1 << (pac_u->pag_u.nam_u.boq_y - 3));
    u3_noun dat = u3i_bytes((siz_w * req_u->tot_w), req_u->dat_y);

    u3_noun wir = u3nc(c3__ames, u3_nul);
    u3_noun cad;
    {
      // u3_noun pax = _mesa_encode_path(pac_u->pag_u.nam_u.pat_s,
      //                         (c3_y*)(pac_u->pag_u.nam_u.pat_c));
      // u3_noun par = u3nc(u3i_chubs(2, pac_u->pag_u.nam_u.her_d), pax);
      // u3_noun lan = u3_nul;
      // u3_noun aut = u3nc(c3y, 0); // XX s/b saved in request state

      // cad = u3nt(c3__mess_ser, lan,
      //            u3nq(c3__page, par, aut, dat));
      u3_noun  lan = u3_mesa_encode_lane(lan_u);
      u3i_slab sab_u;
      // u3i_slab_init(&sab_u, 3, PACT_SIZE);
      // u3i_slab_init(&sab_u, 3, (siz_w * req_u->tot_w) + 135);
      /* u3l_log("slab size %u", (PACT_SIZE - pac_u->pag_u.dat_u.len_w) + (siz_w * req_u->tot_w)); */
      u3i_slab_init(&sab_u, 3, (PACT_SIZE - pac_u->pag_u.dat_u.len_w) + (siz_w * req_u->tot_w));

      pac_u->pag_u.dat_u.len_w = (siz_w * req_u->tot_w);
      pac_u->pag_u.dat_u.fra_y = c3_realloc(pac_u->pag_u.dat_u.fra_y, pac_u->pag_u.dat_u.len_w);
      memcpy(pac_u->pag_u.dat_u.fra_y, req_u->dat_y, pac_u->pag_u.dat_u.len_w);


      /* u3l_log("last frag %u", pic_u->pac_u.pag_u.nam_u.fra_w); */
      // this could be a retry, so we just rewrite the fragment to be the last one
      //
      pic_u->pac_u.pag_u.nam_u.fra_w = req_u->tot_w - 1;
      //  XX should just preserve input buffer
      c3_w res = mesa_etch_pact(sab_u.buf_y, pac_u);
      /* u3l_log("slab len_w %u mesa_etch_pact %u", sab_u.len_w, res); */

      cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));
    }

    _mesa_del_request(sam_u, &pac_u->pag_u.nam_u);

    u3_auto_plan(&sam_u->car_u,
                 u3_ovum_init(0, c3__ames, wir, cad));
  }

  _mesa_free_pict(pic_u);
}

static void
_mesa_add_lane_to_cache(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_noun las, u3_lane lan_u)
{
  u3_noun hit = u3nq(MESA_WAIT,
                     _mesa_get_now(),
                     u3_mesa_encode_lane(lan_u),
                     u3k(las));
  _mesa_put_cache(sam_u, nam_u, hit);
  u3z(las);
}

static void
_mesa_hear_peek(u3_mesa_pict* pic_u, u3_lane lan_u)
{
#ifdef MESA_DEBUG
  // u3l_log("mesa: hear peek");
  // u3_assert(pac_u->hed_u.typ_y == PACT_PEEK);
#endif
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  c3_d* her_d = pac_u->pek_u.nam_u.her_d;
  c3_o  our_o = __( 0 == memcmp(her_d, sam_u->pir_u->who_d, sizeof(*her_d) * 2) );

  //  XX forwarding wrong, need a PIT entry
  if ( c3n == our_o ) {
    u3_peer* per_u = _mesa_get_peer(sam_u, her_d);
    if ( per_u == NULL ) {
      //  XX leaks
      u3l_log("mesa: alien forward for %s", u3r_string(u3dc("scot", c3__p, u3i_chubs(2, her_d))));
      _mesa_free_pict(pic_u);
      return;
    }
    if ( c3y == sam_u->for_o && sam_u->pir_u->who_d[0] == per_u->imp_y ) {
//#ifdef MESA_DEBUG
      u3_lane lin_u = _mesa_get_direct_lane(sam_u, her_d);
//#endif
      //_update_hopcount(&pac_u->hed_u);
       u3l_log("sending peek %u", pac_u->pek_u.nam_u.fra_w);
      _mesa_send(pic_u, &lin_u);
    }
    _mesa_free_pict(pic_u);
    return;
  }
  c3_w  fra_w = pac_u->pek_u.nam_u.fra_w;
  c3_w  bat_w = _mesa_lop(fra_w);

  pac_u->pek_u.nam_u.fra_w = bat_w;
  /* _log_pact(pac_u); */
  /* u3l_log("_mesa_hear_peek %s", pac_u->pek_u.nam_u.pat_c); */
  u3_weak hit = _mesa_get_cache(sam_u, &pac_u->pek_u.nam_u);

  /* u3l_log("peek fra %u hit %u", fra_w, hit != u3_none); */
  /* u3l_log("peek fra %u bat %u hit %u", fra_w, bat_w, hit != u3_none); */

  if ( u3_none != hit ) {
    u3_noun tag, dat;
    u3x_cell(u3k(hit), &tag, &dat);
    if ( tag == MESA_WAIT ) {
      /* u3l_log("MESA_WAIT for %u", bat_w); */
      _mesa_add_lane_to_cache(sam_u, &pac_u->pek_u.nam_u, u3k(u3t(dat)), lan_u);
    } else if ( c3y == our_o && tag == MESA_ITEM ) { // XX our_o redundant
      pac_u->pek_u.nam_u.fra_w = fra_w;
      c3_y* buf_y;
      u3_weak hit_2 = _mesa_get_cache(sam_u, &pac_u->pek_u.nam_u);
      u3_noun tag_2, dat_2;
      u3x_cell(hit_2, &tag_2, &dat_2);
      /* u3l_log("cache hit %u bat %u", fra_w, bat_w); */
      c3_w len_w = _mesa_respond(pic_u, &buf_y, u3k(dat_2));
      // _log_buf(buf_y, len_w);
      _mesa_send_buf(sam_u, lan_u, buf_y, len_w);
      u3z(hit_2);
    } else {
      u3l_log("mesa: weird case in cache, dropping");
    }
    _mesa_free_pict(pic_u);
    u3z(hit);
    return;
  } else {
    _mesa_add_lane_to_cache(sam_u, &pac_u->pek_u.nam_u, u3_nul, lan_u); // TODO: retrieve from namespace
    if ( c3y == our_o ) {
      // u3_noun sky = _name_to_batch_scry(&pac_u->pek_u.nam_u,
      //                                   bat_w,
      //                                   bat_w + MESA_HUNK);

      u3_noun sky = _name_to_jumbo_scry(&pac_u->pek_u.nam_u,
                                        bat_w,
                                        bat_w + MESA_HUNK);

      pac_u->pek_u.nam_u.fra_w = fra_w;

      u3_noun our = u3i_chubs(2, sam_u->car_u.pir_u->who_d);
      u3_noun bem = u3nc(u3nt(our, u3_nul, u3nc(c3__ud, 1)), sky);
      // only branch where we do not free pic_u
      u3_pier_peek(sam_u->car_u.pir_u, u3_nul, u3k(u3nq(1, c3__beam, c3__ax, bem)), pic_u, _mesa_page_scry_jumbo_cb);
    } else {
      // XX unpossible
      _mesa_free_pict(pic_u);
    }
  }
  // u3z(pax);
}

static void
_mesa_poke_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_mesa_pict* pic_u = egg_u->ptr_v;

  if ( u3_ovum_done == new_e ) {
    // XX success stuff here
    /* u3l_log("mesa: poke success"); */
  }
}

static void
_mesa_poke_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3_mesa_pict* pic_u = egg_u->ptr_v;
  // XX failure stuff here
  u3l_log("mesa: poke failure");
}

// xx: should inject event directly, but vane does not work
// so we just hack it to get
static void
_mesa_hear_poke(u3_mesa_pict* pic_u, u3_lane* lan_u)
{

  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  c3_d* her_d = pac_u->pek_u.nam_u.her_d;
  c3_o  our_o = __( 0 == memcmp(her_d, sam_u->pir_u->who_d, sizeof(*her_d) * 2) );

#ifdef MESA_DEBUG
  // u3l_log("mesa: hear poke");
  u3_assert(pac_u->hed_u.typ_y == PACT_POKE);
#endif

  //  XX forwarding wrong, need a PIT entry
  if ( c3n == our_o ) {
    u3_peer* per_u = _mesa_get_peer(sam_u, her_d);
    if ( per_u == NULL ) {
      //  XX leaks
      u3l_log("mesa: alien forward for %s", u3r_string(u3dc("scot", c3__p, u3i_chubs(2, her_d))));
      _mesa_free_pict(pic_u);
      return;
    }
    if ( c3y == sam_u->for_o && sam_u->pir_u->who_d[0] == per_u->imp_y ) {
//#ifdef MESA_DEBUG
      u3_lane lin_u = _mesa_get_direct_lane(sam_u, her_d);
//#endif
       u3l_log("sending peek %u", pac_u->pek_u.nam_u.fra_w);
      //_update_hopcount(&pac_u->hed_u);
      _mesa_send(pic_u, &lin_u);
    }
    _mesa_free_pict(pic_u);
    return;
  }

  //  XX if this lane management stuff is necessary
  // it should be deferred to after successful event processing
  u3_peer* per_u = _mesa_get_peer(sam_u, pac_u->pok_u.pay_u.her_d);
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
    // u3l_log("learnt lane");
  } else {
    // u3l_log("received forwarded poke");
  }
  if ( new_o == c3y ) {
    // u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    // _log_lane(lan_u);
  }
  _mesa_put_peer(sam_u, pac_u->pok_u.pay_u.her_d, per_u);

  //  XX could check cache for ack (completed duplicate)

  u3_ovum_peer nes_f;
  u3_ovum_bail bal_f;
  void*        ptr_v;

  //  XX create PIT entry for ack

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

  if ( 1 == pac_u->pok_u.dat_u.tot_w ) {
    nes_f = bal_f = ptr_v = NULL;
    _mesa_free_pict(pic_u);
  }
  else {
    assert(pac_u->pok_u.dat_u.tot_w);
    //  XX check request state for *payload* (in-progress duplicate)
    nes_f = _mesa_poke_news;
    bal_f = _mesa_poke_bail;
    ptr_v = pic_u;
  }

  u3_auto_peer(
    u3_auto_plan(&sam_u->car_u,
                 u3_ovum_init(0, c3__ames, wir, cad)),
    ptr_v, nes_f, bal_f);
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
    // MESA_LOG(SERIAL)
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

static void
_mesa_recv_cb(uv_udp_t*        wax_u,
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
  u3_mesa* sam_u  = c3_calloc(sizeof(*sam_u));
  sam_u->pir_u    = pir_u;

  //  XX config
  sam_u->her_p = u3h_new_cache(100000);
  sam_u->lan_p = u3h_new_cache(100000);
  sam_u->pac_p = u3h_new_cache(100000);

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

