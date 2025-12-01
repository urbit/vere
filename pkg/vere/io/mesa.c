/// @file

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur/ur.h"
#include "ship.h"
#include "io/ames/stun.h"
#include "mesa/mesa.h"
#include "mesa/bitset.h"
#include <allocate.h>
#include <error.h>
#include <imprison.h>
#include <inttypes.h>
#include <jets/q.h>
#include <manage.h>
#include <c3/motes.h>
#include <retrieve.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <stdlib.h>
#include "lss.h"
#include "arena.h"

static c3_o dop_o = c3n;

static c3_y are_y[524288];

/* FILE* packs; */
/* static c3_d tim_y[200000] = {0}; */
/* static c3_o done = c3y; */

/* #define PACKET_TEST c3y */

// #define MESA_DEBUG     c3y
//#define MESA_TEST
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
#define REORDER_THRESH  5

#define DIRECT_ROUTE_TIMEOUT_MICROS 5000000
#define DIRECT_ROUTE_RETRY_MICROS   1000000
#define PIT_EXPIRE_MICROS           20000000

#define JUMBO_CACHE_MAX_SIZE 200000000 // 200 mb

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

// routing table sentinels
#define MESA_CZAR         1  // pending dns lookup
#define MESA_ROUT         2  // have route
//
// hop enum

#define _mesa_met3_w(a_w) ((c3_bits_word(a_w) + 0x7) >> 3)

struct _u3_mesa_pact;

typedef struct _u3_pact_stat {
  c3_y  tie_y; // tries
  c3_d  sen_d; // last sent
  c3_y  sip_y; // skips
} u3_pact_stat;

struct _u3_mesa;

typedef struct _u3_gage {
  c3_w     rtt_w;  // rtt
  c3_w     rto_w;  // rto
  c3_w     rtv_w;  // rttvar
  c3_w     wnd_w;  // cwnd
  c3_w     wnf_w;  // cwnd fraction
  c3_w     sst_w;  // ssthresh
  c3_w     con_w;  // counter
  //
} u3_gage;

struct _u3_mesa;

typedef struct _u3_mesa_pict {
  uv_udp_send_t      snd_u;
  struct _u3_mesa*   sam_u;
  u3_mesa_pact       pac_u;
} u3_mesa_pict;

typedef struct _u3_lane_state {
  c3_d  sen_d;  //  last sent date
  c3_d  her_d;  //  last heard date
  c3_w  rtt_w;  //  round-trip time
  c3_w  rtv_w;  //  round-trip time variance
} u3_lane_state;

/* _u3_mesa: next generation networking
 */

typedef struct _u3_pit_addr u3_pit_addr;

typedef struct sockaddr_in sockaddr_in;

typedef struct _u3_pit_addr {
  sockaddr_in sdr_u;
  u3_pit_addr* nex_p;
} u3_pit_addr;

typedef struct _u3_pit_entry {
  u3_pit_addr* adr_u;
  c3_d         tim_d;
  arena        are_u;
} u3_pit_entry;

typedef struct _u3_shap {
  c3_d hed_d;
  c3_d tel_d;
} u3_shap;


static u3_shap u3_ship_to_shap( u3_ship ship )
{
  return (u3_shap){ship[0], ship[1]};
}

static void u3_shap_to_ship( u3_ship ship, u3_shap shap )
{
  ship[0] = shap.hed_d;
  ship[1] = shap.tel_d;
}

static void u3_free_pit( u3_pit_entry* pit_u )
{
  arena_free(&pit_u->are_u);
}

static void u3_free_str( u3_str key )
{
  c3_free(key.str_c);
}

static uint64_t u3_hash_str( u3_str key )
{
  c3_d hash = 0xcbf29ce484222325ull;
  for (c3_w i = 0; i < key.len_w; i++) {
    hash = ( (unsigned char)*(key.str_c)++ ^ hash ) * 0x100000001b3ull;
  }
  return hash;
}

static uint64_t u3_cmpr_str( u3_str key1, u3_str key2 )
{
  return key1.len_w == key2.len_w && memcmp( key1.str_c, key2.str_c, key1.len_w ) == 0;
}

static uint64_t u3_cmpr_shap( u3_shap ship1, u3_shap ship2 )
{
  return ship1.hed_d == ship2.hed_d && ship1.tel_d == ship2.tel_d;
}

static uint64_t u3_hash_shap(u3_shap ship)
{
  uint64_t combined = ship.hed_d ^ (ship.tel_d * 0x9e3779b97f4a7c15ull);
  combined ^= combined >> 23;
  combined *= 0x2127599bf4325c37ull;
  combined ^= combined >> 47;
  return combined;
}

typedef struct _u3_pend_req u3_pend_req;

#define NAME req_map
#define KEY_TY u3_str
#define HASH_FN u3_hash_str
#define CMPR_FN u3_cmpr_str
#define VAL_TY u3_pend_req*
#include "verstable.h"

#define NAME pit_map
#define KEY_TY u3_str
#define HASH_FN u3_hash_str
#define CMPR_FN u3_cmpr_str
#define VAL_TY u3_pit_entry*
#define VAL_DTOR_FN u3_free_pit
#include "verstable.h"

#define NAME gag_map
#define KEY_TY u3_shap
#define HASH_FN u3_hash_shap
#define CMPR_FN u3_cmpr_shap
#define VAL_TY u3_gage*
#include "verstable.h"

typedef enum _u3_mesa_ctag {
  CTAG_WAIT = 1,
  CTAG_BLOCK = 2,
} u3_mesa_ctag;


typedef struct _u3_scry_handle {
  void* vod_p;
  arena are_u;
} u3_scry_handle;

//  jumbo frame cache value
//
typedef struct _u3_mesa_line {
  u3_mesa_name nam_u;  //  full name for data, ready to serialize
  u3_auth_data aut_u;  //  message authenticator
  c3_w         tob_d;  //  number of bytes in whole message
  c3_w         dat_w;  //  size in bytes of dat_y
  c3_w         len_w;  //  total allocated size, in bytes
  c3_y*        tip_y;  //  initial Merkle spine, nullable
  c3_y*        dat_y;  //  fragment data (1024 bytes per fragment)
  c3_y*        haz_y;  //  hash pairs    (64 bytes per fragment)
  arena        are_u;
} u3_mesa_line;


static void u3_free_line( u3_mesa_line* lin_u )
{
  // CTAG_WAIT, CTAG_BLOCK
  if ( lin_u > (u3_mesa_line*)2 ) {
    arena_free(&lin_u->are_u);
  }
}

#define NAME jum_map
#define KEY_TY u3_str
#define KEY_DTOR_FN u3_free_str
#define VAL_DTOR_FN u3_free_line
#define HASH_FN u3_hash_str
#define CMPR_FN u3_cmpr_str
#define VAL_TY u3_mesa_line*
#include "verstable.h"

typedef struct _u3_peer u3_peer;

#define NAME per_map
#define KEY_TY u3_shap
#define HASH_FN u3_hash_shap
#define CMPR_FN u3_cmpr_shap
#define VAL_TY u3_peer*
#include "verstable.h"

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
  per_map            per_u;       //  (map ship u3_peer)
  c3_d               jum_d;       //  bytes in jumbo cache
  jum_map            jum_u;       //  jumbo cache
  gag_map            gag_u;       //  lane cache
  pit_map            pit_u;       //  (map path [our=? las=(set lane)])
  req_map            req_u;       //  (map [rift path] u3_pend_req)
  c3_c*              dns_c;       //  turf (urb.otrg)
  c3_d               tim_d;       //  XX: remove
  arena              are_u;       //  per packet arena
  arena              par_u;       //  permanent arena
  uv_timer_t         tim_u;       //  pit clear timer
} u3_mesa;

typedef struct _u3_peer {
  u3_mesa*       sam_u;  //  backpointer
  u3_ship        her_u;  //  who is this peer
  c3_o           ful_o;  //  has this been initialized?
  sockaddr_in    dan_u;  //  direct lane (nullable)
  u3_lane_state  dir_u;  //  direct lane state
  c3_y           imp_y;  //  galaxy @p
  u3_lane_state  ind_u;  //  indirect lane state
} u3_peer;

typedef struct _u3_pend_req {
  u3_peer*               per_u; // backpointer
  c3_d                   nex_d; // number of the next fragment to be sent
  c3_d                   tof_d; // total number of expected fragments
  c3_d                   tob_d; // total number of expected bytes
  u3_auth_data           aut_u; // message authenticator
  uv_timer_t             tim_u; // timehandler
  c3_y*                  dat_y; // ((mop @ud *) lte)
  c3_d                   hav_d; // how many fragments we've received
  c3_d                   lef_d; // lowest fragment number currently in flight/pending
  c3_d                   out_d; // outstanding fragments in flight
  c3_d                   old_d; // frag num of oldest packet sent
  c3_d                   ack_d; // highest acked fragment number
  u3_gage*               gag_u; // congestion control
  lss_pair*              mis_u; // misordered packets
  lss_verifier*          los_u; // Lockstep verifier
  u3_mesa_pict*          pic_u; // preallocated request packet
  u3_pact_stat*          wat_u; // ((mop @ud packet-state) lte)
  u3_bitset              was_u; // ((mop @ud ?) lte)
  c3_c*                  pek_c;
  c3_w                   pek_w;
  c3_d                   pek_d;
  //  stats TODO: use
  c3_d                   beg_d; // date when request began
  arena                  are_u;
} u3_pend_req;

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
  arena            are_u;
} u3_seal;

typedef struct _u3_mesa_cb_data {
  u3_mesa*     sam_u;
  u3_mesa_name nam_u;
  sockaddr_in  lan_u;
} u3_mesa_cb_data;

static c3_d
get_millis() {
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return ((c3_d)tp.tv_sec * 1000ull) + ((c3_d)tp.tv_usec / 1000ull);
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
  u3l_log("gauge at %p", gag_u);
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
  u3l_log("galaxy: %s", u3r_string(u3dc("scot", 'p', per_u->imp_y)));
}

static void
_log_pend_req(u3_pend_req* req_u)
{
  if( req_u == NULL ) {
    u3l_log("pending request was NULL");
    return;
  }
  u3l_log("have: %"PRIu64, req_u->hav_d);
  u3l_log("next: %"PRIu64, req_u->nex_d);
  u3l_log("total: %" PRIu64, req_u->tof_d);
  u3l_log("gage: %c", req_u->gag_u == NULL ? 'n' : 'y');
  //u3l_log("timer in: %" PRIu64 " ms", uv_timer_get_due_in(&req_u->tim_u));
}

static void
_log_mesa_data(u3_mesa_data dat_u)
{
  u3l_log("total bytes: %" PRIu64, dat_u.tob_d);
  u3l_log("frag len: %u", dat_u.len_w);
  // u3l_log("frag: %xxx", dat_u.fra_y);
}

/* _mesa_lop(): find beginning of page containing fra_d
*/
static inline c3_d
_mesa_lop(c3_d fra_d)
{
  return fra_d & ~((1 << u3_Host.ops_u.jum_y) - 1);
}

static c3_d
_get_now_micros()
{
  struct timeval tim_u;
  gettimeofday(&tim_u, NULL);
	return ((c3_d)tim_u.tv_sec * 1000ull * 1000ull) + (c3_d)tim_u.tv_usec;
}

static c3_d
_abs_dif(c3_d ayy_d, c3_d bee_d)
{
  return ayy_d > bee_d ? ayy_d - bee_d : bee_d - ayy_d;
}

static c3_d
_clamp_rto(c3_d rto_d) {
  return c3_min(c3_max(rto_d, 200 * 1000), 25000 * 1000); // ~s25 max backoff
}

static inline c3_o
_mesa_is_lane_zero(sockaddr_in lan_u)
{
  return __((lan_u.sin_addr.s_addr == 0) && (lan_u.sin_port == 0));
}

static c3_o
_mesa_is_direct_mode(u3_peer* per_u)
{
  c3_d now_d = _get_now_micros();
  return __(per_u->dir_u.her_d + DIRECT_ROUTE_TIMEOUT_MICROS > now_d);
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
_mesa_copy_auth_data(u3_auth_data* des_u, u3_auth_data* src_u)
{
  des_u->typ_e = src_u->typ_e;
  switch ( des_u->typ_e ) {
    case AUTH_HMAC: {
      memcpy(des_u->mac_y, src_u->mac_y, 16);
    } break;
    case AUTH_SIGN: {
      memcpy(des_u->sig_y, src_u->sig_y, 64);
    } break;
    case AUTH_PAIR: {
      memcpy(des_u->has_y, src_u->has_y, 64);
    } break;
    case AUTH_NONE: {
    } break;
    default: u3_assert(!"unreachable");
  }
}

static void
_mesa_copy_name(u3_mesa_name* des_u, u3_mesa_name* src_u, arena* are_u)
{
  memcpy(des_u, src_u, sizeof(u3_mesa_name));
  u3_ship_copy(des_u->her_u, src_u->her_u);
  des_u->str_u.str_c = new(are_u, c3_c, src_u->str_u.len_w);
  memcpy(des_u->str_u.str_c, src_u->str_u.str_c, src_u->str_u.len_w);
  des_u->pat_c = des_u->str_u.str_c + (src_u->pat_c - src_u->str_u.str_c);
}

static u3_mesa_name*
_mesa_copy_name_alloc(u3_mesa_name* src_u, arena* are_u)
{
  u3_mesa_name* des_u = new(are_u, u3_mesa_name, 1);
  _mesa_copy_name(des_u, src_u, are_u);
  return des_u;
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
u3_noun
_mesa_request_key(u3_mesa_name* nam_u)
{
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);
  u3_noun res = u3nc(u3i_word(nam_u->rif_w), pax);
  return res;
}

static void
_init_gage(u3_gage* gag_u)  //  microseconds
{
  gag_u->rto_w = 200 * 1000;  // ~s1
  gag_u->rtt_w = 1000 * 1000;  // ~s1
  gag_u->rtv_w = 1000 * 1000;  // ~s1
  /* gag_u->rto_w = 200 * 1000;  // ~s1 */
  /* gag_u->rtt_w = 82 * 1000;  // ~s1 */
  /* gag_u->rtv_w = 100 * 1000;  // ~s1 */
  gag_u->con_w = 0;
  gag_u->wnd_w = 1;
  gag_u->sst_w = 10000;
}

/* u3_mesa_encode_lane(): serialize lane to noun
*/
static u3_noun
u3_mesa_encode_lane(sockaddr_in lan_u) {
  // [%if ip=@ port=@]
  c3_w pip_w = ntohl(lan_u.sin_addr.s_addr);
  c3_s por_s = ntohs(lan_u.sin_port);
  return u3nt(c3__if, u3i_word(pip_w), por_s);
}

static u3_peer*
_mesa_get_peer(u3_mesa* sam_u, u3_ship her)
{
  per_map_itr itr_u = vt_get(&sam_u->per_u, u3_ship_to_shap(her));
  if ( vt_is_end(itr_u) ) {
    return NULL;
  }
  return itr_u.data->val;
}

static void
_mesa_put_peer(u3_mesa* sam_u, u3_ship her_u, u3_peer* per_u)
{
  u3_shap him_u = u3_ship_to_shap(her_u);
  per_map_itr itr_u = vt_get(&sam_u->per_u, him_u);

  if ( vt_is_end(itr_u) ) {
    itr_u = vt_insert(&sam_u->per_u, him_u, per_u);

    if ( vt_is_end(itr_u) ) {
      fprintf(stderr, "mesa: cannot allocate memory for peer, dying");
      u3_king_bail();
    }
  } else {
    memcpy(itr_u.data->val, per_u, sizeof(u3_peer));
  }
}

/* _mesa_get_request(): produce pending request state for nam_u
 *
 *   produces a NULL pointer if no pending request exists
*/
static u3_pend_req*
_mesa_get_request(u3_mesa* sam_u, u3_mesa_name* nam_u) {
  u3_str key_u = {nam_u->pat_c, nam_u->pat_s};
  req_map_itr itr_u = vt_get(&sam_u->req_u, key_u);
  if ( vt_is_end(itr_u) ) {
    return NULL;
  }
  return itr_u.data->val;
}

static void
_mesa_del_request_cb(uv_handle_t* han_u) {
  u3_pend_req* req_u = han_u->data;
  arena_free(&req_u->are_u);
}

static void
_mesa_del_request(u3_mesa* sam_u, u3_mesa_name* nam_u) {
  u3_str key_u = {nam_u->pat_c, nam_u->pat_s};
  req_map_itr itr_u = vt_get(&sam_u->req_u, key_u);

  if ( vt_is_end(itr_u) ) {
    return;
  }

  u3_pend_req* req_u = itr_u.data->val;
  vt_erase(&sam_u->req_u, key_u);
  if ( (u3_pend_req*)CTAG_WAIT != req_u ) {
    uv_timer_stop(&req_u->tim_u);
    req_u->tim_u.data = req_u;
    uv_close((uv_handle_t*)&req_u->tim_u, _mesa_del_request_cb);
  }
}

/* _mesa_put_request(): save new pending request state for nam_u
*/
static void
_mesa_put_request(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_pend_req* req_u) {
  u3_str key_u = {nam_u->pat_c, nam_u->pat_s};
  req_map_itr itr_u = vt_insert(&sam_u->req_u, key_u, req_u);

  if ( vt_is_end(itr_u) ) {
    fprintf(stderr, "mesa: cannot allocate memory for request, dying");
    u3_king_bail();
  }
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

static sockaddr_in
_mesa_get_direct_lane(u3_mesa* sam_u, u3_ship her_u)
{
  sockaddr_in adr_u = {0};
  adr_u.sin_family = AF_INET;

  if ( c3__czar == u3_ship_rank(her_u) ) {
    c3_s por_s = _ames_czar_port(her_u[0]);
    adr_u.sin_addr.s_addr = htonl(u3_Host.imp_u[her_u[0]]);
    adr_u.sin_port = htons(por_s);
    return adr_u;
  }

  per_map_itr itr_u = vt_get(&sam_u->per_u, u3_ship_to_shap(her_u));
  if ( vt_is_end(itr_u) ) {
    return adr_u;
  }

  adr_u = itr_u.data->val->dan_u;


  return adr_u;
}

static c3_o
_mesa_lanes_equal(sockaddr_in lan_u, sockaddr_in lon_u)
{
  return __((lan_u.sin_addr.s_addr == lon_u.sin_addr.s_addr) && (lan_u.sin_port == lon_u.sin_port));
}

static sockaddr_in
_mesa_get_czar_lane(u3_mesa* sam_u, c3_y imp_y)
{
  c3_s por_s = _ames_czar_port(imp_y);
  sockaddr_in adr_u = {0};
  adr_u.sin_family = AF_INET;
  adr_u.sin_addr.s_addr = htonl(u3_Host.imp_u[imp_y]);
  adr_u.sin_port = htons(por_s);
  return adr_u;
}

/* _mesa_get_lane(): get lane
*/
static u3_gage*
_mesa_get_gage(u3_mesa* sam_u, u3_ship her_u) {
  gag_map_itr itr_u = vt_get(&sam_u->gag_u, u3_ship_to_shap(her_u));
  if ( vt_is_end(itr_u) ) {
    return NULL;
  }
  return itr_u.data->val;
}

/* _mesa_put_gage(): put gage state in state
 *
*/
static void
_mesa_put_gage(u3_mesa* sam_u, u3_ship her_u, u3_gage* gag_u)
{
  gag_map_itr itr_u = vt_insert(&sam_u->gag_u, u3_ship_to_shap(her_u), gag_u);
  if ( vt_is_end(itr_u) ) {
    fprintf(stderr, "mesa: cannot allocate memory for gage, dying");
    u3_king_bail();
  }
}
// congestion control update
static void _mesa_handle_ack(u3_gage* gag_u, u3_pact_stat* pat_u)
{
  /* _log_gage(gag_u); */
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

static inline c3_d
_mesa_req_get_remaining(u3_pend_req* req_u)
{
  return req_u->tof_d - req_u->hav_d;
}

static c3_d
_safe_sub(c3_d a, c3_d b) {
  return a < b ? 0 : a - b;
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
  /* c3_w liv_w = bitset_wyt(&req_u->was_u); */
  c3_w rem_w = _mesa_req_get_remaining(req_u);
  /* u3l_log("rem_w %u wnd_w %u", rem_w, req_u->gag_u->wnd_w); */

  /* u3l_log("rem_w %u", rem_w); */
  /* u3l_log("wnd_w %u", req_u->gag_u->wnd_w); */
  /* u3l_log("out_d %"PRIu64, req_u->out_d); */
  /* return c3_min(rem_w, _safe_sub(3500, req_u->out_d)); */
  c3_d ava_d = _safe_sub((c3_d)req_u->gag_u->wnd_w, req_u->out_d);
  return c3_min(rem_w, ava_d);
  /* return c3_min(rem_w, 5000 - req_u->out_d); */
}

/* _mesa_req_pact_resent(): mark packet as resent
**
*/
static void
_mesa_req_pact_resent(u3_pend_req* req_u, u3_mesa_name* nam_u, c3_d now_d)
{
  req_u->wat_u[nam_u->fra_d].sen_d = now_d;
  req_u->wat_u[nam_u->fra_d].tie_y++;
}

/* _mesa_req_pact_sent(): mark packet as sent
**   after 1-RT first packet is handled in _mesa_req_pact_init()
*/
static void
_mesa_req_pact_sent(u3_pend_req* req_u, c3_d fra_d, c3_d now_d)
{
  if( req_u->nex_d == fra_d ) {
    req_u->nex_d++;
    req_u->out_d++;
  }
  // TODO: optional assertions?
  req_u->wat_u[fra_d].sen_d = now_d;
  req_u->wat_u[fra_d].sip_y = 0;
  req_u->wat_u[fra_d].tie_y++;
}

/* _ames_alloc(): libuv buffer allocator.
*/
static void
_mesa_alloc(uv_handle_t* had_u, size_t len_i, uv_buf_t* buf)
{
  u3_mesa* sam_u = (u3_mesa*)had_u->data;
  sam_u->are_u.beg = (char*)are_y;
  c3_c* ptr_v = new(&sam_u->are_u, c3_c, 400000);
  buf->base = ptr_v;
  buf->len = 400000;
}

/* u3_mesa_decode_lane(): deserialize noun to lane; 0.0.0.0:0 if invalid
*/
static sockaddr_in
u3_mesa_decode_lane(u3_atom lan) {
  sockaddr_in adr_u = {0};
  c3_d lan_d;

  if ( c3n == u3r_safe_chub(lan, &lan_d) || (lan_d >> 48) != 0 ) {
    return adr_u;
  }

  u3z(lan);
  //  convert incoming localhost to outgoing localhost
  //

  adr_u.sin_addr.s_addr = ( u3_Host.ops_u.net == c3y ) ?
                          htonl((c3_w)lan_d) : htonl(0x7f000001);
  adr_u.sin_port = htons((c3_s)(lan_d >> 32));

  return adr_u;
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

static void
_mesa_send_cb2(uv_udp_send_t* req_u, c3_i sas_i)
{
  if ( sas_i ) {
    u3l_log("mesa: send fail_async: %s", uv_strerror(sas_i));
    //sam_u->fig_u.net_o = c3n;
  }
  c3_free(req_u);
}

typedef struct _send_helper {
  uv_udp_send_t snd_u;
  u3_pend_req* req_u;
  c3_d fra_d;
} send_helper;

static void
_mesa_send_cb3(uv_udp_send_t* snt_u, c3_i sas_i)
{
  send_helper* snd_u = (send_helper*)snt_u;
  if ( sas_i ) {
    u3l_log("mesa: send fail_async: %s", uv_strerror(sas_i));
    //sam_u->fig_u.net_o = c3n;
  } else {
    /* _mesa_req_pact_sent(snd_u->req_u, snd_u->fra_d); */
  }
  c3_free(snd_u);
}

static c3_i _mesa_send_buf2(struct sockaddr** ads_u, uv_buf_t** bfs_u, c3_w* int_u, c3_w num_w)
{

  /* add_u.sin_family = AF_INET; */

 /* if ( u3_Host.ops_u.net && (add_u.sin_addr.s_addr == 0 || add_u.sin_addr.s_addr == 0xffffffff) ) { */
  /*   return; */
  /* } */

#ifdef MESA_DEBUG
  /* c3_c* sip_c = inet_ntoa(add_u.sin_addr); */
  // u3l_log("mesa: sending packet to %s:%u", sip_c, por_s);
#endif
  c3_i sas_i = uv_udp_try_send2(&u3_Host.wax_u, num_w, bfs_u, int_u, ads_u, 0);
  if ( sas_i < 0 ) {
    u3l_log("ames: send fail_sync: %s", uv_strerror(sas_i));
    return 0;
  }
  return sas_i;
//  else if ( sas_i < (c3_i)num_w) {
//    for ( c3_i i = sas_i; i < (c3_i)num_w; i++) {
//      uv_udp_send_t* req_u = c3_malloc(sizeof(*req_u));
//      uv_udp_send(req_u, &u3_Host.wax_u, bfs_u[i], 1, ads_u[0], _mesa_send_cb2);
//    }
//  }
}
static void _mesa_send_buf3(sockaddr_in add_u, uv_buf_t buf_u)
{

  add_u.sin_addr.s_addr = ( u3_Host.ops_u.net == c3y ) ? add_u.sin_addr.s_addr  : htonl(0x7f000001);
  /* add_u.sin_family = AF_INET; */

  if ( u3_Host.ops_u.net && (add_u.sin_addr.s_addr == 0 || add_u.sin_addr.s_addr == 0xffffffff) ) {
    return;
  }

#ifdef MESA_DEBUG
  // c3_c* sip_c = inet_ntoa(add_u.sin_addr);
  // u3l_log("mesa: sending packet to %s:%u", sip_c, por_s);
#endif


  uv_udp_send_t* snd_u = c3_malloc(sizeof(*snd_u));

  c3_i     sas_i = uv_udp_send((uv_udp_send_t*)snd_u,
                               &u3_Host.wax_u,
                               &buf_u, 1,
                               (const struct sockaddr*)&add_u,
                               _mesa_send_cb3);

  if ( sas_i ) {
    u3l_log("ames: send fail_sync: %s", uv_strerror(sas_i));
    /*if ( c3y == sam_u->fig_u.net_o ) {
      //sam_u->fig_u.net_o = c3n;
    }*/
  }
}

static void _mesa_send_buf(u3_mesa* sam_u, sockaddr_in add_u, c3_y* buf_y, c3_w len_w)
{

  add_u.sin_addr.s_addr = ( u3_Host.ops_u.net == c3y ) ? add_u.sin_addr.s_addr  : htonl(0x7f000001);
  add_u.sin_family = AF_INET;

  if ( u3_Host.ops_u.net && (add_u.sin_addr.s_addr == 0 || add_u.sin_addr.s_addr == 0xffffffff) ) {
    c3_free(buf_y);
    return;
  }

  u3_seal* sel_u = c3_calloc(sizeof(*sel_u));
  sel_u->buf_y = buf_y;
  sel_u->len_w = len_w;
  sel_u->sam_u = sam_u;



#ifdef MESA_DEBUG
  c3_c* sip_c = inet_ntoa(add_u.sin_addr);
  u3l_log("mesa: sending packet to %s:%u", sip_c, ntohs(add_u.sin_port));
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

static void _mesa_send(u3_mesa_pict* pic_u, sockaddr_in lan_u)
{
  u3_mesa* sam_u = pic_u->sam_u;
  c3_y  *buf_y  = c3_calloc(PACT_SIZE);
  c3_w len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, &pic_u->pac_u);
  _mesa_send_buf(sam_u, lan_u, buf_y, len_w);
}

typedef struct _u3_mesa_request_data {
  u3_mesa*      sam_u;
  u3_ship       her_u;
  u3_mesa_name* nam_u;
  c3_y*         buf_y;
  c3_w          len_w;
  u3_pit_addr*  las_u;
  arena         are_u;
} u3_mesa_request_data;

typedef struct _u3_mesa_resend_data {
  arena                are_u;
  uv_timer_t           tim_u;
  u3_mesa_request_data dat_u;
  c3_y                 ret_y;  //  number of remaining retries
} u3_mesa_resend_data;


static void
_mesa_free_cb(uv_handle_t *han_u)
{
  u3_mesa_resend_data* res_u = han_u->data;
  arena_free(&res_u->are_u);
}

static void
_mesa_free_resend_data(u3_mesa_resend_data* res_u)
{
  uv_timer_stop(&res_u->tim_u);
  res_u->tim_u.data = res_u;
  uv_close((uv_handle_t*)&res_u->tim_u, _mesa_free_cb);
}

static void
_mesa_send_bufs(u3_mesa* sam_u,
                u3_peer* per_u,
                c3_y* buf_y,
                c3_w len_w,
                u3_pit_addr* las_u);

static void
_mesa_send_modal(u3_peer* per_u, uv_buf_t buf_u, u3_pit_addr* las_u)
{
  u3_mesa* sam_u = per_u->sam_u;
  c3_d now_d = _get_now_micros();

  c3_w len_w = buf_u.len;
  c3_y* sen_y = c3_calloc(len_w);
  memcpy(sen_y, buf_u.base, len_w);

  u3_ship gal_u = {0};
  gal_u[0] = per_u->imp_y;
  c3_o our_o = u3_ships_equal(gal_u, sam_u->pir_u->who_d);

  if ( ( c3y == _mesa_is_direct_mode(per_u) ) ||
       // if we are the sponsor of the ship, don't send to ourselves
       (our_o == c3y) )  {
    // u3l_log("mesa: direct");
    _mesa_send_buf(sam_u, per_u->dan_u, sen_y, len_w);
    per_u->dir_u.sen_d = now_d;
  }
  else if ( las_u != NULL ) {
    _mesa_send_bufs(sam_u, per_u, sen_y, len_w, las_u);
  } else {
    #ifdef MESA_DEBUG
      /* c3_c* gal_c = u3_ship_to_string(gal_u); */
      // u3l_log("mesa: sending to %s", gal_c);
      /* c3_free(gal_c); */
    #endif
    //
    sockaddr_in imp_u = _mesa_get_czar_lane(sam_u, per_u->imp_y);
    _mesa_send_buf(sam_u, imp_u, sen_y, len_w);
    per_u->ind_u.sen_d = now_d;

    if ( c3n == _mesa_is_lane_zero(per_u->dan_u) ) {
      c3_y* san_y = c3_calloc(len_w);
      memcpy(san_y, buf_u.base, len_w);
      _mesa_send_buf(sam_u, per_u->dan_u, san_y, len_w);
      per_u->dir_u.sen_d = now_d;
    }
  }
}


static uv_buf_t
_mesa_peek_buf(c3_c* pek_c, c3_d fra_d, c3_w pek_w)
// 43
{
  if (fra_d <= 0xff) {
    return uv_buf_init(pek_c+(fra_d*pek_w), pek_w);
  }
  if (fra_d <= 0xffff) {
    return uv_buf_init(
      pek_c + (0x100 * pek_w) + ((fra_d - 0x100) * (pek_w + 1)),
      pek_w + 1
    );
  }
  if (fra_d <= 0xffffff) {
    return uv_buf_init(
      pek_c + (0x100 * pek_w) +
      ((fra_d - 0x100) * (pek_w + 1)) +
      ((fra_d - 0x10000) * (pek_w + 3)),
      pek_w + 3
    );
  }
  return uv_buf_init(
    pek_c + (0xff * pek_w) +
    ((fra_d - 0x100) * (pek_w + 1)) +
    ((fra_d - 0x10000) * (pek_w + 3)) +
    ((fra_d - 0x1000000ULL) * (pek_w + 7)),
    pek_w + 7
  );
}

static void
_try_resend(u3_pend_req* req_u, c3_d nex_d)
{
  c3_o los_o = c3n;
  c3_d now_d = _get_now_micros();
  u3_mesa_pact *pac_u = &req_u->pic_u->pac_u;

  /* c3_y buf_y[PACT_SIZE]; */
  /* arena scr_u = req_u->are_u; */
  /* uv_buf_t* bfs_u = new(&scr_u, uv_buf_t, 1); */
  // c3_w i_w = 0;
  for ( c3_d i_d = req_u->lef_d; i_d < nex_d; i_d++ ) {
    //  TODO: make fast recovery different from slow
    //  TODO: track skip count but not dupes, since dupes are meaningless
    if ( (c3n == bitset_has(&req_u->was_u, i_d)) &&
        (now_d - req_u->wat_u[i_d].sen_d > req_u->gag_u->rto_w) ) {
      // u3l_log("now_d %"PRIu64, now_d);
      // u3l_log("sen_d %"PRIu64, req_u->wat_u[i_d].sen_d);
      // u3l_log("rto_w %u", req_u->gag_u->rto_w);
      los_o = c3y;
      /* u3l_log("resend fra_w: %llu", i_d); */

      uv_buf_t buf_u = _mesa_peek_buf(req_u->pek_c, i_d, req_u->pek_w);
      /* if (buf_u.base < req_u->pek_c) { */
      /*     u3l_log("peek overflow, dying, fragment %"PRIu64, i_d); */
      /*     abort(); */
      /* } */
      /* new(&scr_u, uv_buf_t, 1); */
      /* bfs_u[i_w] = buf_u; */
      /* c3_w len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, pac_u); */
      /* _mesa_send_buf3(req_u->per_u->dan_u, buf_u, req_u, i_d); */
      _mesa_send_modal(req_u->per_u, buf_u, NULL);
      _mesa_req_pact_resent(req_u, &pac_u->pek_u.nam_u, now_d);
      // i_w++;
    }
  }
  /* c3_w* int_u = new(&scr_u, c3_w, i_w); */
  /* struct sockaddr** ads_u = new(&scr_u, struct sockaddr*, i_w); */
  /* uv_buf_t** bus_u = new(&scr_u, uv_buf_t*, i_w); */
  /* for (c3_w j_w = 0; j_w < i_w; j_w++) { */
  /*   ads_u[j_w] = (struct sockaddr*)&req_u->per_u->dan_u; */
  /*   bus_u[j_w] = &bfs_u[j_w]; */
  /*   int_u[j_w] = 1; */
  /* } */

  if ( c3y == los_o ) {
    /* _mesa_send_buf2(req_u->per_u->sam_u, ads_u, bus_u, int_u, i_w); */
    /* _mesa_send_buf2(req_u->per_u->sam_u, req_u->per_u->dan_u, bfs_u, i_w); */
    req_u->gag_u->sst_w = c3_max(1, req_u->gag_u->wnd_w / 2);
    req_u->gag_u->wnd_w = req_u->gag_u->sst_w;
    req_u->gag_u->rto_w = _clamp_rto(req_u->gag_u->rto_w * 2);
    // u3l_log("loss");
    // u3l_log("resent %u", i_w);
    // u3l_log("counter %u hav_d %"PRIu64 " nex_d %"PRIu64 " ack_d %"PRIu64 " lef_d %"PRIu64 " old_d %"PRIu64, req_u->los_u->counter, req_u->hav_d, req_u->nex_d, req_u->ack_d, req_u->lef_d, req_u->old_d);
    // _log_gage(req_u->gag_u);
  }
}

static void
_mesa_packet_timeout(uv_timer_t* tim_u);

//  TODO rename to indicate it sets a timer
static void
_update_resend_timer(u3_pend_req *req_u)
{
  // scan in flight packets, find oldest
  c3_w idx_d = req_u->lef_d;
  /* c3_d now_d = _get_now_micros(); */
  /* c3_d wen_d = now_d; */
  /* for ( c3_d i = req_u->lef_d; i < req_u->nex_d; i++ ) { */
  /*   // u3l_log("fra %u (%u)", i, __LINE__); */
  /*   if ( c3n == bitset_has(&req_u->was_u, i) && */
	/*        wen_d > req_u->wat_u[i].sen_d */
  /*      ) { */
  /*     wen_d = req_u->wat_u[i].sen_d; */
  /*     idx_d = i; */
  /*   } */
  /* } */
/*   if ( now_d == wen_d ) { */
/* #ifdef MESA_DEBUG */
/*     /\* u3l_log("failed to find new oldest"); *\/ */
/* #endif */
/*   } */
  /* req_u->old_d = idx_d; */
  req_u->tim_u.data = req_u;
  // c3_d gap_d = req_u->wat_u[idx_d].sen_d == 0 ?
  //               0 :
  //               now_d - req_u->wat_u[idx_d].sen_d;
  c3_d next_expiry = req_u->gag_u->rto_w;
  // u3l_log("next_expiry %llu", next_expiry / 1000);
  // u3l_log("DUE %llu", uv_timer_get_due_in(&req_u->tim_u));
  uv_timer_start(&req_u->tim_u, _mesa_packet_timeout, next_expiry / 1000, 0);
}

/* _mesa_packet_timeout(): callback for packet timeout
*/
static void
_mesa_packet_timeout(uv_timer_t* tim_u) {
  u3_pend_req* req_u = (u3_pend_req*)tim_u->data;
  // u3l_log("old %llu nex %llu packet timed out", req_u->old_d, req_u->nex_d);
  _try_resend(req_u, req_u->nex_d);
  _update_resend_timer(req_u);
}

static c3_o
_mesa_burn_misorder_queue(u3_pend_req* req_u, c3_y boq_y, c3_w ack_w)
{
  c3_d num_d;
  c3_d max_d = req_u->tof_d;
  c3_o res_o = c3y;
  for ( num_d = 0; (num_d + ack_w) < max_d; num_d++ ) {
    c3_w siz_w = (1 << (boq_y - 3));  // XX
    c3_y* fra_y = req_u->dat_y + (siz_w * (ack_w + num_d));
    c3_w len_w = (num_d + ack_w) == (max_d - 1) ? req_u->tob_d % 1024 : 1024;
    lss_pair* pur_u =  &req_u->mis_u[ack_w + num_d];
    lss_pair* par_u = (0 == memcmp(pur_u, &(lss_pair){0}, sizeof(lss_pair))) ? NULL : &req_u->mis_u[ack_w + num_d];
    if ( c3n == bitset_has(&req_u->was_u, (num_d + ack_w)) ) {
      break;
    }
    if ( c3y != lss_verifier_ingest(req_u->los_u, fra_y, len_w, par_u) ) {
      res_o = c3n;
      u3l_log("fail to burn %" PRIu64 " %" PRIu64, num_d + ack_w, req_u->tof_d);
      break;
    }
    // u3l_log("size %u counter %u num %u fra %u inx %u lef_d %u", siz_w, req_u->los_u->counter , num_w, fra_d, (req_u->los_u->counter + num_w + 1), lef_d);
  }

  // ratchet forward
  /* u3l_log("burned %llu", num_d); */
  num_d++; // account for the in-ordered packet processed in _mesa_req_pact_done
  req_u->lef_d += num_d;
  req_u->hav_d += num_d;

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
                    sockaddr_in   lan_u)
{
  u3_mesa* sam_u = req_u->per_u->sam_u; //  needed for the MESA_LOG macro

  // received past the end of the message
  if ( mesa_num_leaves(dat_u->tob_d) <= nam_u->fra_d ) {
    u3l_log("strange tob_d %"PRIu64" fra_d %"PRIu64" req_u %"PRIu64,
            dat_u->tob_d, nam_u->fra_d, req_u->hav_d);
    MESA_LOG(sam_u, STRANGE);
    //  XX: is this sufficient to drop whole request
    return;
  }

  // received duplicate
  if ( c3y == bitset_has(&req_u->was_u, nam_u->fra_d) ) {
    // MESA_LOG(sam_u, DUPE);
    /* _update_resend_timer(req_u); */
    return;
  }

  lss_pair* par_u = NULL;

  c3_w siz_w = (1 << (nam_u->boq_y - 3));
  memcpy(req_u->dat_y + (siz_w * nam_u->fra_d), dat_u->fra_y, dat_u->len_w);

  if ( dat_u->aut_u.typ_e == AUTH_PAIR ) {
    par_u = &req_u->mis_u[nam_u->fra_d];
    memcpy((*par_u)[0], dat_u->aut_u.has_y[0], sizeof(lss_hash));
    memcpy((*par_u)[1], dat_u->aut_u.has_y[1], sizeof(lss_hash));
  }

  if ( req_u->los_u->counter != nam_u->fra_d ) {
    // insert into misordered queue
    /* u3l_log("insert into misordered queue fra: %llu [counter %u]", */
    /*         nam_u->fra_d, */
    /*         req_u->los_u->counter); */
    req_u->out_d--;
    bitset_put(&req_u->was_u, nam_u->fra_d);

    _mesa_handle_ack(req_u->gag_u, &req_u->wat_u[nam_u->fra_d]);
    /* _try_resend(req_u, nam_u->fra_d); */
    /* _update_resend_timer(req_u); */
    return;
  }
  else if ( c3y != lss_verifier_ingest(req_u->los_u, dat_u->fra_y, dat_u->len_w, par_u) ) {
    u3l_log("auth fail frag %"PRIu64, nam_u->fra_d);
    u3l_log("nit_o %u", nam_u->nit_o);
    _mesa_del_request(sam_u, nam_u);
    MESA_LOG(sam_u, AUTH);
    return;
  }
  else if ( c3y != _mesa_burn_misorder_queue(req_u, nam_u->boq_y, req_u->los_u->counter) ) {
    MESA_LOG(sam_u, AUTH)
    _mesa_del_request(sam_u, nam_u);
    return;
  }
  else {
    // u3l_log("about to other free");
  }

  if ( nam_u->fra_d > req_u->ack_d ) {
    req_u->ack_d = nam_u->fra_d;
  }

  req_u->out_d--;
  bitset_put(&req_u->was_u, nam_u->fra_d);

  #ifdef MESA_DEBUG
    // u3l_log("fragment %llu counter %u hav_d %llu nex_d %llu ack_d %llu lef_d %llu old_d %llu", nam_u->fra_d, req_u->los_u->counter, req_u->hav_d, req_u->nex_d, req_u->ack_d, req_u->lef_d, req_u->old_d);
  #endif

  u3_lane_state* sat_u;
  if ( 0 == hop_y && (c3n == _mesa_lanes_equal(lan_u, req_u->per_u->dan_u)) ) {
    req_u->per_u->dan_u = lan_u;
    sat_u = &req_u->per_u->dir_u;
    _init_lane_state(sat_u);
  }
  else {
    sat_u = &req_u->per_u->ind_u;
  }
  sat_u->her_d = _get_now_micros();

  // handle gauge update
  _mesa_handle_ack(req_u->gag_u, &req_u->wat_u[nam_u->fra_d]);

  //  XX FIXME?
  /* _try_resend(req_u, nam_u->fra_d); */
  _update_resend_timer(req_u);
}

static sockaddr_in
_realise_lane(u3_noun lan) {
  sockaddr_in lan_u = {0};
  lan_u.sin_family = AF_INET;

  if ( c3y == u3a_is_cat(lan)  ) {
    // u3_assert( lan < 256 );
    if ( (c3n == u3_Host.ops_u.net) ) {
      lan_u.sin_addr.s_addr = htonl(0x7f000001);
      lan_u.sin_port = htons(_ames_czar_port(lan));
    } else {
      lan_u.sin_addr.s_addr = htonl(u3_Host.imp_u[lan]);
      lan_u.sin_port = htons(_ames_czar_port(lan));
    }
  } else {
    u3_noun tag, pip, por;
    u3x_trel(lan, &tag, &pip, &por);
    if ( tag == c3__if ) {
      lan_u.sin_addr.s_addr = htonl(u3r_word(0, pip));
      u3_assert( c3y == u3a_is_cat(por) && por <= 0xFFFF);
      lan_u.sin_port = htons(por);
    } else {
      u3l_log("mesa: inscrutable lane");
    }

  }
  u3z(lan);
  return lan_u;
}

static void
_mesa_send_bufs(u3_mesa* sam_u,
                u3_peer* per_u, // null for response packets
                c3_y* buf_y,
                c3_w len_w,
                u3_pit_addr* las_u)
{

  u3_pit_addr* t = las_u;
  while ( NULL != t ) {
    sockaddr_in lan_u = t->sdr_u;

    if ( !lan_u.sin_port ) {
      u3l_log("mesa: failed to realise lane");
    } else {
      c3_y* sen_y = c3_calloc(len_w);
      memcpy(sen_y, buf_y, len_w);
      _mesa_send_buf(sam_u, lan_u, sen_y, len_w);
      if ( per_u && (c3y == _mesa_lanes_equal(lan_u, per_u->dan_u)) ) {
        per_u->dir_u.sen_d = _get_now_micros();
      }
    }
    t = t->nex_p;
  }
}

static u3_pit_entry*
_mesa_get_pit(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  pit_map_itr itr_u = vt_get(&sam_u->pit_u, nam_u->str_u);
  if ( vt_is_end(itr_u) ) {
    return NULL;
  }
  return itr_u.data->val;
}

static void
_mesa_del_pit(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  /* u3l_log("deleting %llu", nam_u->fra_d); */
  vt_erase(&sam_u->pit_u, nam_u->str_u);
}

static void
_mesa_add_lane_to_pit(u3_mesa* sam_u, u3_mesa_name* nam_u, sockaddr_in lan_u)
{
  pit_map_itr itr_u = vt_get(&sam_u->pit_u, nam_u->str_u);

  c3_d now_d = _get_now_micros();

  if ( vt_is_end(itr_u) ) {
    arena are_u = arena_create(16384);
    u3_pit_entry* ent_u = new(&are_u, u3_pit_entry, 1);
    ent_u->are_u = are_u;
    ent_u->tim_d = now_d;
    u3_pit_addr* adr_u = new(&ent_u->are_u, u3_pit_addr, 1);
    adr_u->nex_p = 0;
    adr_u->sdr_u = lan_u;
    ent_u->adr_u = adr_u;

    c3_c* str_c = new(&ent_u->are_u, c3_c, nam_u->str_u.len_w);
    memcpy(str_c, nam_u->str_u.str_c, nam_u->str_u.len_w);
    u3_str str_u = {str_c, nam_u->str_u.len_w};

    itr_u = vt_insert(&sam_u->pit_u, str_u, ent_u);

    if ( vt_is_end(itr_u) ) {
      fprintf(stderr, "mesa: cannot allocate memory for pit, dying");
      u3_king_bail();
    }

  }
  else {
    u3_pit_entry* ent_u = itr_u.data->val;
    ent_u->tim_d = now_d;
    u3_pit_addr* old_u = ent_u->adr_u;
    while (old_u) {
      if ( c3y == _mesa_lanes_equal(lan_u, old_u->sdr_u) ) {
        return;
      }
      old_u = old_u->nex_p;
    }

    u3_pit_addr* adr_u = new(&ent_u->are_u, u3_pit_addr, 1);
    adr_u->sdr_u = lan_u;
    u3_pit_addr* tmp_u = ent_u->adr_u;
    adr_u->nex_p = tmp_u;
    ent_u->adr_u = adr_u;
  }
  return;
}

static void
_mesa_resend_timer_cb(uv_timer_t* tim_u)
{
  u3_mesa_resend_data* res_u = tim_u->data;
  u3_mesa_request_data* dat_u = &res_u->dat_u;
  res_u->ret_y--;

  u3_pend_req* pit_u = _mesa_get_request(dat_u->sam_u, dat_u->nam_u);
  if ( (u3_pend_req*)CTAG_WAIT != pit_u ) {
    #ifdef MESA_DEBUG
      // u3l_log("mesa: resend PIT entry gone %u", res_u->ret_y);
    #endif
    _mesa_free_resend_data(res_u);
    return;
  }
  else {
    #ifdef MESA_DEBUG
      u3l_log("mesa: resend %u", res_u->ret_y);
    #endif
  }

  _mesa_send_bufs(dat_u->sam_u,
                  NULL,
                  dat_u->buf_y,
                  dat_u->len_w,
                  dat_u->las_u);

  if ( res_u->ret_y ) {
    uv_timer_start(&res_u->tim_u, _mesa_resend_timer_cb, 1000, 0);
  }
  else {
    _mesa_del_request(res_u->dat_u.sam_u, res_u->dat_u.nam_u);
    _mesa_free_resend_data(res_u);
  }
}

static u3_pit_addr*
_mesa_lanes_to_addrs(u3_noun las, arena* are_u) {
  u3_pit_addr* adr_u = NULL;
  u3_noun lan, t = las;
  while ( t != u3_nul ) {
    u3x_cell(t, &lan, &t);
    u3_pit_addr* new_u = new(are_u, u3_pit_addr, 1);
    new_u->sdr_u = _realise_lane(u3k(lan));
    new_u->nex_p = adr_u;
    adr_u = new_u;
  }
  return adr_u;
}

static void
_mesa_hear(u3_mesa* sam_u,
           const struct sockaddr* adr_u,
           c3_w     len_w,
           c3_y*    hun_y);

static void time_elapsed(c3_d fra_d, c3_d total, c3_d begin, c3_d end)
{
  c3_d elapsed = end - begin;
  double percent = 100.0 * ((double)elapsed / (double)total);
  u3l_log("  %"PRIu64": %"PRIu64"(%.2f%%)", fra_d, elapsed, percent);
}

static void
packet_test(u3_mesa* sam_u, c3_c* fil_c) {
  FILE* fd = fopen(fil_c, "rb");
  fseek(fd, 0L, SEEK_END);
  c3_d sz = ftell(fd);
  c3_y* packets = c3_malloc(sz);
  fseek(fd, 0L, SEEK_SET);

  const struct sockaddr adr_u = {
    .sa_family = AF_INET,
    .sa_data = {
      0x00, 0x50,             // Port 80 in network byte order
      0xa7, 0xac, 0xa8, 0x17, // IP 167.172.168.23
      0x00, 0x00, 0x00, 0x00, // Padding
      0x00, 0x00, 0x00, 0x00  // Padding
    }
  };

  fread(packets, 1, sz, fd);
  fclose(fd);

  c3_d now_d = _get_now_micros();
  c3_d tidx = 0;

  for (c3_d i = 0; i < sz;) {
    c3_w len_w = *(c3_w*)(packets+i);
    /* u3l_log("len_w %u i %"PRIu64, len_w, i); */
    i += 4;
    _mesa_hear(sam_u, &adr_u, len_w, packets + i);
    /* tim_y[tidx] = _get_now_micros(); */
    sam_u->are_u.beg = (char*)are_y;
    i += len_w;
    /* tidx++; */
  }
  c3_d end_d = _get_now_micros();
  c3_d total_d = end_d - now_d;
  /* for (c3_d i = 1; i < tidx; i++) { */
  /*   time_elapsed(i, total_d, tim_y[i-1], tim_y[i]); */
  /* } */
  u3l_log("packet_test took %"PRIu64, total_d);
  c3_free(packets);
}

static void
_mesa_ef_send(u3_mesa* sam_u, u3_noun las, u3_noun pac)
{
  c3_w len_w = u3r_met(3, pac);
  arena are_u = arena_create(len_w + 16384);
  c3_y* buf_y = new(&are_u, c3_y, len_w);
  u3r_bytes(0, len_w, buf_y, pac);

  u3_mesa_pact pac_u;
  memset(&pac_u, 0x11, sizeof(pac_u));
  c3_c* err_c = mesa_sift_pact_from_buf(&pac_u, buf_y, len_w);
  if ( err_c ) {
    u3l_log("mesa: ef_send: sift failed: %u %s", len_w, err_c);
    u3z(pac);
    u3z(las);
    arena_free(&are_u);
    return;
  }

  u3_pend_req* old_u = _mesa_get_request(sam_u, &pac_u.pek_u.nam_u);

  if (old_u) {
    u3z(pac);
    u3z(las);
    arena_free(&are_u);
    return;
  }

  if ( PACT_PAGE == pac_u.hed_u.typ_y ) {
    u3_pit_entry* pin_u = _mesa_get_pit(sam_u, &pac_u.pek_u.nam_u);

    if ( NULL == pin_u ) {
        arena_free(&are_u);
        u3z(pac);
        u3z(las);
      return;
    }

    _mesa_send_bufs(sam_u, NULL, buf_y, len_w, pin_u->adr_u);
    _mesa_del_pit(sam_u, &pac_u.pek_u.nam_u);
    arena_free(&are_u);
  }
  else {
    u3_mesa_resend_data* res_u = new(&are_u, u3_mesa_resend_data, 1);
    res_u->are_u = are_u;
    u3_mesa_name* nam_u = _mesa_copy_name_alloc(&pac_u.pek_u.nam_u, &res_u->are_u);
    u3_mesa_request_data* dat_u = &res_u->dat_u;
    {
      dat_u->sam_u = sam_u;
      u3_ship_copy(dat_u->her_u, nam_u->her_u);
      dat_u->nam_u = nam_u;
      dat_u->las_u = _mesa_lanes_to_addrs(las, &res_u->are_u);
      dat_u->buf_y = buf_y;
      dat_u->len_w = len_w;
    }
    {
      res_u->ret_y = 9;
      uv_timer_init(u3L, &res_u->tim_u);
    }
    _mesa_put_request(sam_u, nam_u, (u3_pend_req*)CTAG_WAIT);
    res_u->tim_u.data = res_u;
    #ifdef PACKET_TEST
    packet_test(sam_u, "pages.packs");
    #else

    _mesa_send_bufs(sam_u, NULL, buf_y, len_w, dat_u->las_u);
    uv_timer_start(&res_u->tim_u, _mesa_resend_timer_cb, 1000, 0);
    #endif
  }

  u3z(pac);
  u3z(las);
}

c3_o
_ames_kick_newt(void* sam_u, u3_noun tag, u3_noun dat);

u3_atom
u3_ames_encode_lane(u3_lane lan);

static void _meet_peer(u3_mesa* sam_u, u3_peer* per_u, u3_ship her_u);
static void _init_peer(u3_mesa* sam_u, u3_peer* per_u);

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
        /* u3l_log("mesa: send old %s", tag_c); */
        c3_free(tag_c);
      #endif
      ret_o = _ames_kick_newt(u3_Host.sam_u, u3k(tag), u3k(dat));
    } break;
    case c3__nail: {
      u3_ship who_u;
      u3_ship_of_noun(who_u, u3h(dat));
      u3_peer* per_u = _mesa_get_peer(sam_u, who_u);

      if ( NULL == per_u ) {
        per_u = new(&sam_u->par_u, u3_peer, 1);
        _init_peer(sam_u, per_u);
      }

      // XX the format of the lane %nail gives is (list (each @p address))
      //
      u3_noun las = u3t(dat);

      if ( las == u3_nul ) {
        per_u->dan_u = (sockaddr_in){0};
      }
      else {
        u3_noun lan = u3h(las);
        // we either have a direct route, and a galaxy, or just one lane
        if ( c3n == u3h(lan) ) {
          sockaddr_in lan_u = u3_mesa_decode_lane(u3k(u3t(lan)));
          per_u->dan_u = lan_u;
        } else {
          // delete direct lane if galaxy
          per_u->dan_u = (sockaddr_in){0};
        }
      }

      _meet_peer(sam_u, per_u, who_u);

      ret_o = _ames_kick_newt(u3_Host.sam_u, u3k(tag), u3k(dat));
    } break;
  }

  // technically losing tag is unnecessary as it always should
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
_mesa_exit_cb(uv_handle_t* han_u) {
  u3_mesa* sam_u = (u3_mesa*)han_u->data;
  arena_free(&sam_u->par_u);
}

static void
_mesa_io_exit(u3_auto* car_u)
{
  u3_mesa* sam_u = (u3_mesa*)car_u;
  uv_timer_stop(&sam_u->tim_u);
  sam_u->tim_u.data = sam_u;
  uv_udp_recv_stop(&u3_Host.wax_u);
  uv_close((uv_handle_t*)&sam_u->tim_u, _mesa_exit_cb);
  uv_close((uv_handle_t*)&u3_Host.wax_u, 0);
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
  per_u->dan_u = (sockaddr_in){0};
  _init_lane_state(&per_u->dir_u);
  per_u->imp_y = 0;
  _init_lane_state(&per_u->ind_u);
}

static u3_noun
_name_to_jumbo_scry(u3_mesa_name* nam_u)
{
  u3_noun rif = _dire_etch_ud(nam_u->rif_w);
  u3_noun boq = _dire_etch_ud(31); // XX make configurable
  u3_noun fag = _dire_etch_ud(0); // XX 1
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);
  // u3_noun wer = nam_u->nit_o == c3y
  //   ? u3nc(c3__init, pax)
  //   : u3nt(nam_u->aut_o == c3y ? c3__auth : c3__data, fag, pax);

  //  XX only boq_y of MAX JUMBO allowed
  u3_noun wer = u3nt(c3__data, fag, pax);
  u3_noun res = u3nc(c3__mess, u3nq(rif, c3__pact, boq, u3nc(c3__etch, wer)));

  return res;
}


static c3_w
_name_to_jumbo_str(u3_mesa_name* nam_u, c3_y* buf_y)
{
  u3_mesa_name tmp_u = *nam_u;
  tmp_u.boq_y = 31;
  tmp_u.fra_d = 0;
  tmp_u.nit_o = c3n;

  u3_etcher ech_u;
  etcher_init(&ech_u, buf_y, PACT_SIZE);
  _mesa_etch_name(&ech_u, &tmp_u);
  return ech_u.len_w;
}

static u3_mesa_line*
_mesa_get_jumbo_cache(u3_mesa* sam_u, u3_mesa_name* nam_u)
{
  c3_y buf_y[PACT_SIZE];
  c3_w len_w = _name_to_jumbo_str(nam_u, buf_y);
  u3_str str_u = {(c3_c*)buf_y, len_w};

  jum_map_itr itr_u = vt_get(&sam_u->jum_u, str_u);
  if ( vt_is_end(itr_u) ) {
    return NULL;
  }
  return itr_u.data->val;
}

static void
_mesa_put_jumbo_cache(u3_mesa* sam_u, u3_mesa_name* nam_u, u3_mesa_line* lin_u)
{
  c3_y* buf_y = c3_malloc(PACT_SIZE);

  c3_w len_w = _name_to_jumbo_str(nam_u, buf_y);
  u3_str str_u = {(c3_c*)buf_y, len_w};

  // CTAG_BLOCK, CTAG_WAIT
  if ( lin_u > (u3_mesa_line*)2 ) {
    if (sam_u->jum_d > JUMBO_CACHE_MAX_SIZE) {
      vt_cleanup(&sam_u->jum_u);
      sam_u->jum_d = 0;
    }

    sam_u->jum_d += lin_u->tob_d;
  }

  jum_map_itr itr_u = vt_insert(&sam_u->jum_u, str_u, lin_u);

  if ( vt_is_end(itr_u) ) {
    fprintf(stderr, "mesa: cannot allocate memory for jumbo cache, dying");
    u3_king_bail();
  }
}

static void
_mesa_send_pact_single(u3_mesa*      sam_u,
                       sockaddr_in   adr_u,
                       u3_mesa_pact* pac_u)
{
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, pac_u);
  _mesa_send_buf(sam_u, adr_u, buf_y, len_w);
}

static void
_mesa_send_pact(u3_mesa*      sam_u,
                u3_pit_addr*  las_u,
                u3_peer*      per_u, // null for response packets
                u3_mesa_pact* pac_u)
{
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, pac_u);
  _mesa_send_bufs(sam_u, per_u, buf_y, len_w, las_u);
}

static void
_mesa_send_leaf(u3_mesa*      sam_u,
                u3_mesa_line* lin_u,
                u3_mesa_pact* pac_u, // scratchpad
                c3_d          fra_d,
                sockaddr_in   adr_u
                )
{
  u3_mesa_name* nam_u = &pac_u->pag_u.nam_u;
  u3_mesa_data* dat_u = &pac_u->pag_u.dat_u;

  nam_u->fra_d = fra_d;
  c3_d i_d = fra_d - (lin_u->nam_u.fra_d * (1 << u3_Host.ops_u.jum_y));
  c3_w cur_w = i_d * 1024;
  dat_u->fra_y = lin_u->dat_y + cur_w;
  dat_u->len_w = c3_min(lin_u->dat_w - cur_w, 1024);

  lss_pair* pair = ((lss_pair*)lin_u->haz_y) + i_d;

  if ( 0 == nam_u->fra_d ) {  // XX
    _mesa_copy_auth_data(&dat_u->aut_u, &lin_u->aut_u);
  } else if ( 0 == memcmp(pair, &(lss_pair){0}, sizeof(lss_pair)) ) {
    dat_u->aut_u.typ_e = AUTH_NONE;
  } else {
    dat_u->aut_u.typ_e = AUTH_PAIR;
    memcpy(dat_u->aut_u.has_y, pair, sizeof(lss_pair));
  }
#ifdef MESA_DEBUG
  // u3l_log(" sending leaf packet, fra_d: %"PRIu64, nam_u->fra_d);
#endif
  // log_pact(pac_u);
  _mesa_send_pact_single(sam_u, adr_u, pac_u);
}

static void
_mesa_send_piece(u3_mesa* sam_u, u3_mesa_line* lin_u, u3_mesa_name* nam_u, c3_d fra_d, sockaddr_in adr_u) {
  u3_mesa_pact pac_u = {0};
  u3_mesa_head* hed_u = &pac_u.hed_u;
  {
    hed_u->nex_y = HOP_NONE;
    hed_u->pro_y = 1;
    hed_u->typ_y = PACT_PAGE;
    hed_u->hop_y = 0;
    //  mug_w varies by fragment
  }
  pac_u.pag_u.nam_u = *nam_u;

  u3_mesa_data* dat_u = &pac_u.pag_u.dat_u;
  {
    dat_u->tob_d = lin_u->tob_d;
    dat_u->aut_u = lin_u->aut_u;
    //  aut_u, len_w, and fra_y vary by fragment
  }

  c3_d mev_d = mesa_num_leaves(dat_u->tob_d);
  c3_w pro_w = lss_proof_size(mev_d);
  if ( 0 == nam_u->fra_d && c3y == nam_u->nit_o ) {
    if ( pro_w > 1 ) {
      dat_u->len_w = pro_w * sizeof(lss_hash);
      c3_y* pro_y = c3_malloc(dat_u->len_w);
      memcpy(pro_y, lin_u->tip_y, dat_u->len_w);
      dat_u->fra_y = pro_y;
      _mesa_send_pact_single(sam_u, adr_u, &pac_u);
      c3_free(pro_y);
    }

    //  single-fragment message; just send the one data fragment
    else if ( 0 == fra_d ) {
      _mesa_send_leaf(sam_u, lin_u, &pac_u, 0, adr_u);
    }
    else {
      u3l_log("mesa: weird fragment number %"PRIu64, fra_d);
    }

  } else {
    _mesa_send_leaf(sam_u, lin_u, &pac_u, fra_d, adr_u);
  }
}

static void
_mesa_page_scry_jumbo_cb(void* vod_p, u3_noun res)
{
  u3_scry_handle* han_u = (u3_scry_handle*)vod_p;
  u3_mesa_cb_data* dat_u = (u3_mesa_cb_data*)han_u->vod_p;
  u3_mesa* sam_u = dat_u->sam_u;
  u3_mesa_name* nam_u = &dat_u->nam_u;

  u3_weak pag = u3r_at(7, res);

  if ( u3_none == pag ) {
    // TODO: mark as dead
    u3_mesa_line* lin_u = _mesa_get_jumbo_cache(sam_u, nam_u);
    u3z(res);
    if ( NULL == lin_u ) {
      arena_free(&han_u->are_u);
      return;
    }
    lin_u = (u3_mesa_line*)CTAG_BLOCK;
    _mesa_put_jumbo_cache(sam_u, nam_u, lin_u);
    arena_free(&han_u->are_u);
    return;
  }
  u3_noun pac, pas, pof;
  if ( c3n == u3r_trel(pag, &pac, &pas, &pof) ||
       c3n == u3a_is_pug(pac) ) {
    u3l_log("mesa: jumbo frame misshapen");
    u3z(res);
    arena_free(&han_u->are_u);
    return;
  }

  u3_mesa_line* lin_u;
  {
    c3_w jumbo_w = u3r_met(3, pac);
    c3_y* jumbo_y = c3_calloc(jumbo_w);
    u3r_bytes(0, jumbo_w, jumbo_y, pac);

    u3_mesa_pact jum_u;
    c3_c* err_c = mesa_sift_pact_from_buf(&jum_u, jumbo_y, jumbo_w);
    if ( err_c ) {
      u3l_log("mesa: jumbo frame parse failure: %s", err_c);
      arena_free(&han_u->are_u);
      u3z(res);
      return;
    }
    u3_mesa_data* dat_u = &jum_u.pag_u.dat_u;

    c3_d mev_d = mesa_num_leaves(dat_u->tob_d); // leaves in message
    c3_w tip_w = // bytes in Merkle spine
      (mev_d > 1 && jum_u.pag_u.nam_u.fra_d == 0)?
      lss_proof_size(mev_d) * sizeof(lss_hash):
      0;
    c3_w dat_w = dat_u->len_w; // bytes in fragment data in this jumbo frame
    c3_w lev_w = mesa_num_leaves(dat_w); // number of leaves in this frame
    c3_w haz_w = lev_w * sizeof(lss_pair); // bytes in hash pairs
    c3_w len_w = tip_w + dat_w + haz_w;

    arena are_u = arena_create(sizeof(u3_mesa_line) + len_w + 2048);

    lin_u = new(&are_u, u3_mesa_line, 1);
    lin_u->are_u = are_u;
    _mesa_copy_name(&lin_u->nam_u, &jum_u.pek_u.nam_u, &lin_u->are_u);
    lin_u->aut_u = dat_u->aut_u;
    lin_u->tob_d = dat_u->tob_d;
    lin_u->dat_w = dat_w;
    lin_u->len_w = len_w;
    lin_u->tip_y = new(&lin_u->are_u, c3_y, len_w);
    lin_u->dat_y = lin_u->tip_y + tip_w;
    lin_u->haz_y = lin_u->dat_y + dat_w;
    memcpy(lin_u->dat_y, dat_u->fra_y, dat_u->len_w);

    c3_y* haz_y = lin_u->haz_y;
    while ( pas != u3_nul ) {
      u3r_bytes(0, 64, haz_y, u3h(pas));
      haz_y += 64;
      pas = u3t(pas);
    }

    u3r_bytes(0, tip_w, lin_u->tip_y, pof);
    c3_free(jumbo_y);

  }

  _mesa_put_jumbo_cache(sam_u, nam_u, lin_u);

  u3_pit_entry* ent_u = _mesa_get_pit(sam_u, nam_u);
  if ( NULL != ent_u ) {
    u3_pit_addr* adr_u = ent_u->adr_u;
    while (adr_u) {
      _mesa_send_piece(sam_u, lin_u, nam_u, 0, adr_u->sdr_u);
      adr_u = adr_u->nex_p;
    }
    _mesa_del_pit(sam_u, nam_u);
  }


  /* _mesa_send_jumbo_pieces(sam_u, lin_u, NULL); */

  u3z(res);
  arena_free(&han_u->are_u);
  return;
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
    u3_noun her = u3h(sax);
    u3_ship her_u;
    u3_ship_of_noun(her_u, her);
    u3_peer* new_u = _mesa_get_peer(per_u->sam_u, her_u);
    if ( new_u != NULL ) {
      per_u = new_u;
    } else {
      _mesa_put_peer(per_u->sam_u, her_u, per_u);
    }
    u3_mesa* sam_u = per_u->sam_u;
    u3_noun gal = u3do("rear", u3k(sax));
    u3_assert( c3y == u3a_is_cat(gal) && gal < 256 );
    // both atoms guaranteed to be cats, bc we don't call unless forwarding
    per_u->ful_o = c3y;
    per_u->imp_y = gal;
  }

  u3z(nun);
}

static void
_forward_lanes_cb(void* vod_p, u3_noun nun)
{
  u3_peer* per_u = vod_p;
  u3_mesa* sam_u = per_u->sam_u;

  u3_weak las    = u3r_at(7, nun);
  // u3m_p("_forward_lanes_cb", las);

  if ( las != u3_none ) {
    u3_peer* new_u = _mesa_get_peer(per_u->sam_u, per_u->her_u);
    if ( new_u != NULL ) {
      per_u = new_u;
    }
    u3_noun gal = u3h(las);
    u3_assert( c3y == u3a_is_cat(gal) && gal < 256 );
    // both atoms guaranteed to be cats, bc we don't call unless forwarding
    per_u->ful_o = c3y;
    per_u->imp_y = gal;
    u3_noun sal = u3k(u3t(las));
    u3_noun lan;
    while ( sal != u3_nul ) {
      u3x_cell(sal, &lan, &sal);

      if ( c3n == u3a_is_cat(lan) ) {
        // there should be only one lane that is not a direct atom
        //
        sockaddr_in lan_u = _realise_lane(u3k(lan));
        per_u->dan_u = lan_u;
      }
    }
    u3z(sal);
    _mesa_put_peer(per_u->sam_u, per_u->her_u, per_u);
  }

  u3z(nun);

}

static void
_meet_peer(u3_mesa* sam_u, u3_peer* per_u, u3_ship her_u)
{
  u3_noun her = u3_ship_to_noun(her_u);
  u3_noun gan = u3nc(u3_nul, u3_nul);

  per_u->her_u[0] = her_u[0];
  per_u->her_u[1] = her_u[1];

  u3_noun pax = u3nc(u3dc("scot", c3__p, her), u3_nul);
  u3_pier_peek_last(sam_u->pir_u, gan, c3__j, c3__saxo, pax, per_u, _saxo_cb);

}

static void
_get_peer_lanes(u3_mesa* sam_u, u3_peer* per_u)
{
  u3_noun her = u3_ship_to_noun(per_u->her_u);
  u3_noun gan = u3nc(u3_nul, u3_nul);
  u3_noun pax = u3nq(u3i_string("chums"),
                  u3dc("scot", 'p', her),
                  u3i_string("lanes"),
                  u3_nul);
  u3m_p("pax", pax);
  u3_pier_peek_last(sam_u->pir_u, gan, c3__ax, u3_nul, pax, per_u, _forward_lanes_cb);
}

static void
_hear_peer(u3_mesa* sam_u, u3_peer* per_u, sockaddr_in lan_u, c3_o dir_o)
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
                             sockaddr_in lan_u)
{

  lan_u.sin_addr.s_addr = ( u3_Host.ops_u.net == c3y ) ? lan_u.sin_addr.s_addr : htonl(0x7f000001);

  c3_w win_w = _mesa_req_get_cwnd(req_u);
  u3_mesa_pict* nex_u = req_u->pic_u;
  c3_w nex_d = req_u->nex_d;
  /* arena scr_u = req_u->are_u; */
  /* uv_buf_t* bfs_u = new(&scr_u, uv_buf_t, win_w); */
  /* uv_buf_t** bus_u = new(&scr_u, uv_buf_t*, win_w); */
  /* struct sockaddr** ads_u = new(&scr_u, struct sockaddr*, win_w); */
  /* c3_w* int_u = new(&scr_u, c3_w, win_w); */
  c3_d now_d = _get_now_micros();
  for ( c3_w i = 0; i < win_w; i++ ) {
    c3_w fra_w = nex_d + i;
    if ( fra_w >= req_u->tof_d ) {
      break;
    }
    // u3l_log("next fra_w: %u", fra_w);
    nex_u->pac_u.pek_u.nam_u.fra_d = fra_w;
    uv_buf_t buf_u = _mesa_peek_buf(req_u->pek_c, nex_d+i, req_u->pek_w);
    if (buf_u.base < req_u->pek_c) {
        u3l_log("peek overflow, dying, fragment %u", nex_d+i);
        abort();
    }
    mesa_etch_pact_to_buf((c3_y*)buf_u.base, buf_u.len, &nex_u->pac_u);
    /* bfs_u[i] = buf_u; */
    /* bus_u[i] = &bfs_u[i]; */
    /* ads_u[i] = (struct sockaddr*)&lan_u; */
    /* int_u[i] = 1; */
    _mesa_req_pact_sent(req_u, fra_w, now_d);

    /* _mesa_send_buf3(req_u->per_u->dan_u, buf_u, req_u, fra_w); */
    _mesa_send_modal(req_u->per_u, buf_u, NULL);
    /* _mesa_send(nex_u, lan_u); */
  }
  /* if ( i > 0 ) { */
  /*   c3_i sen_i = _mesa_send_buf2(ads_u, bus_u, int_u, i); */
  /*   for (c3_w i = 0; i < sen_i; i++) { */
  /*     c3_w fra_w = nex_d + i; */
  /*     _mesa_req_pact_sent(req_u, fra_w, now_d); */
  /*   } */
  /* } */
}

static void
_mesa_veri_scry_cb(void* vod_p, u3_noun nun)
{
  u3_mesa_cb_data* ver_u = vod_p;
  #ifndef PACKET_TEST
  u3_pend_req* req_u = _mesa_get_request(ver_u->sam_u, &ver_u->nam_u);
  if ( !req_u ) {
    return;
  }
  else if ( c3y == nun ) {  // XX
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
  #endif
  c3_free(ver_u);
}

static void
_mesa_req_pact_init(u3_mesa* sam_u, u3_mesa_pict* pic_u, sockaddr_in lan_u, u3_peer* per_u)
{
  sam_u->tim_d = _get_now_micros();
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa_name* nam_u = &pac_u->pag_u.nam_u;
  u3_mesa_data* dat_u = &pac_u->pag_u.dat_u;

  u3_gage* gag_u = _mesa_get_gage(sam_u, nam_u->her_u);
  if ( gag_u == NULL ) {
    gag_u = new(&sam_u->par_u, u3_gage, 1);
    _init_gage(gag_u);
    _mesa_put_gage(sam_u, nam_u->her_u, gag_u);
    u3_assert( gag_u != NULL );
  }

  // _log_gage(gag_u);
  u3_mesa_pact exa_u;
  exa_u.hed_u.typ_y = PACT_PEEK;
  exa_u.pek_u.nam_u = *nam_u;
  exa_u.pek_u.nam_u.fra_d = 0;
  exa_u.pek_u.nam_u.nit_o = c3n;
  exa_u.pek_u.nam_u.aut_o = c3n;
  c3_w pek_w = mesa_size_pact(&exa_u);
  c3_d tof_d = mesa_num_leaves(dat_u->tob_d);
  c3_w pof_w = lss_proof_size(tof_d);
  c3_w pairs_w = c3_bits_word(pof_w);
  c3_d pek_d = dat_u->tob_d;
  arena are_u = arena_create(5*dat_u->tob_d);
  u3_pend_req* req_u = new(&are_u, u3_pend_req, 1);
  req_u->are_u = are_u;
  req_u->pic_u = new(&req_u->are_u, u3_mesa_pict, 1);
  req_u->pic_u->sam_u = sam_u;
  req_u->pic_u->pac_u.hed_u.typ_y = PACT_PEEK;
  req_u->pic_u->pac_u.hed_u.pro_y = MESA_VER;
  req_u->pic_u->pac_u.pek_u.nam_u = *(_mesa_copy_name_alloc(nam_u, &req_u->are_u));
  req_u->pic_u->pac_u.pek_u.nam_u.aut_o = c3n;
  req_u->pic_u->pac_u.pek_u.nam_u.nit_o = c3n;
  req_u->aut_u = dat_u->aut_u;

  uv_timer_init(u3L, &req_u->tim_u);

  u3_assert( pac_u->pag_u.nam_u.boq_y == 13 );
  req_u->gag_u = gag_u;
  req_u->tob_d = dat_u->tob_d;
  /* req_u->out_d = 4000; */
  req_u->out_d = 0;
  req_u->tof_d = mesa_num_leaves(dat_u->tob_d); // NOTE: only correct for bloq 13!
  assert( req_u->tof_d != 1 ); // these should be injected directly by _mesa_hear_page
  req_u->dat_y = new(&req_u->are_u, c3_y, dat_u->tob_d);
  req_u->mis_u = new(&req_u->are_u, lss_pair, tof_d);
  memset(req_u->mis_u, 0, sizeof(lss_pair)*tof_d);
  req_u->wat_u = new(&req_u->are_u, u3_pact_stat, req_u->tof_d + 2);
  bitset_init(&req_u->was_u, req_u->tof_d, &req_u->are_u);

  // TODO: handle restart
  // u3_assert( nam_u->fra_w == 0 );

  req_u->nex_d = 0;
  req_u->hav_d = 0;
  req_u->lef_d = 0;
  req_u->old_d = 0;
  req_u->ack_d = 0;

  lss_hash* pof_u = new(&req_u->are_u, lss_hash, pof_w);
  if ( dat_u->len_w != pof_w*sizeof(lss_hash) ) {
    return; // TODO: handle like other auth failures
  }
  for ( int i = 0; i < pof_w; i++ ) {
    memcpy(pof_u[i], dat_u->fra_y + (i * sizeof(lss_hash)), sizeof(lss_hash));
  }
  lss_hash root;
  lss_root(root, pof_u, pof_w);
  req_u->los_u = new(&req_u->are_u, lss_verifier, 1);
  lss_verifier_init(req_u->los_u, 0, req_u->tof_d, pof_u, &req_u->are_u);

  req_u->pek_w = pek_w;
  req_u->pek_d = pek_d;
  req_u->pek_c = new(&req_u->are_u, c3_c, pek_d);

  req_u->per_u = per_u;

  _mesa_put_request(sam_u, &req_u->pic_u->pac_u.pek_u.nam_u, req_u);
  _update_resend_timer(req_u);

  // scry to verify auth
  u3_noun typ, aut;
  switch ( dat_u->aut_u.typ_e ) {
  case AUTH_SIGN:
    typ = c3__sign;
    aut = u3dc("scot", c3__uv, u3i_bytes(64, dat_u->aut_u.sig_y));
    break;
  case AUTH_HMAC:
    typ = c3__hmac;
    aut = u3dc("scot", c3__uv, u3i_bytes(16, dat_u->aut_u.mac_y));
    break;
  default:
    return; // TODO: handle like other auth failures
  }
  u3_noun her = u3dc("scot", c3__p, u3_ship_to_noun(nam_u->her_u));
  u3_noun rut = u3dc("scot", c3__uv, u3i_bytes(32, root));
  u3_noun pax = _mesa_encode_path(nam_u->pat_s, (c3_y*)nam_u->pat_c);
  u3_noun sky = u3i_list(typ, her, aut, rut, pax, u3_none);
  u3_mesa_cb_data* ver_u = c3_malloc(sizeof(u3_mesa_cb_data));
  ver_u->sam_u = sam_u;
  ver_u->nam_u = req_u->pic_u->pac_u.pek_u.nam_u;
  ver_u->lan_u = lan_u;
  u3_pier_peek_last(sam_u->pir_u, u3_nul, c3__a, c3__veri, sky, ver_u, _mesa_veri_scry_cb);
}

typedef struct _u3_mesa_lane_cb_data {
  u3_lane       lan_u;
  u3_peer*      per_u;
} u3_mesa_lane_cb_data;

static void
_mesa_page_bail_cb(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3l_log("mesa: arvo page event failed");
}

static void
_mesa_add_hop(c3_y hop_y, u3_mesa_head* hed_u, u3_mesa_page_pact* pag_u, sockaddr_in lan_u)
{
  c3_w pip_w = ntohl(lan_u.sin_addr.s_addr);
  c3_s por_s = ntohs(lan_u.sin_port);
  c3_etch_word(pag_u->sot_u, pip_w);
  c3_etch_short(pag_u->sot_u + 4, por_s);
  hed_u->nex_y = HOP_SHORT;
}

/* static c3_d avg_time() { */
/*   c3_d sum = 0; */
/*   c3_w i; */
/*   for (i = 0; tim_y[i] != 0; i++) { */
/*     if (tim_y[i] > 1000) { */
/*       u3l_log("dingding fra %u time %"PRIu64, i, tim_y[i]); */
/*     } */
/*     sum += tim_y[i]; */
/*   } */
/*  return sum / i; */
/* } */

static void
_mesa_forward_request(u3_mesa* sam_u, u3_mesa_pict* pic_u, sockaddr_in lan_u)
{
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_peer* per_u = _mesa_get_peer(sam_u, pac_u->pek_u.nam_u.her_u);
  if ( !per_u ) {
    #ifdef MESA_DEBUG
      c3_c* mes = u3_ship_to_string(pac_u->pek_u.nam_u.her_u);
      u3l_log("mesa: alien forward for %s; meeting ship", mes);
      c3_free(mes);
    #endif
    per_u = new(&sam_u->par_u, u3_peer, 1);
    _init_peer(sam_u, per_u);
    per_u->her_u[0] = pac_u->pek_u.nam_u.her_u[0];
    per_u->her_u[1] = pac_u->pek_u.nam_u.her_u[1];

    _get_peer_lanes(sam_u, per_u); // forward-lanes
    return;
  }
  if ( c3y == sam_u->for_o && sam_u->pir_u->who_d[0] == per_u->imp_y ) {
  // if ( c3y == sam_u->for_o ) {
    sockaddr_in lin_u = _mesa_get_direct_lane(sam_u, pac_u->pek_u.nam_u.her_u);
    if ( _mesa_is_lane_zero(lin_u) == c3y) {
      c3_c* shp_c = u3_ship_to_string(pac_u->pek_u.nam_u.her_u);
      u3l_log("zero lane for %s", shp_c);
      c3_free(shp_c);
      return;
    }
    inc_hopcount(&pac_u->hed_u);
    #ifdef MESA_DEBUG
      u3l_log("mesa: forward_request()");
      // _log_lane(&lan_u);
    #endif

    #ifdef MESA_DEBUG
      c3_c* sip_c = inet_ntoa(lin_u.sin_addr);
      u3l_log("mesa: sending packet to %s:%u", sip_c, ntohs(lin_u.sin_port));
    #endif

    _mesa_add_lane_to_pit(sam_u, &pac_u->pek_u.nam_u, lan_u);
    _mesa_send(pic_u, lin_u);
  }
}

static void
_mesa_hear_page(u3_mesa_pict* pic_u, sockaddr_in lan_u)
{
  #ifdef MESA_DEBUG
    u3l_log("mesa: hear_page()");
    // log_pact(&pic_u->pac_u);
    u3_assert( PACT_PAGE == pic_u->pac_u.hed_u.typ_y );
  #endif

  u3_mesa* sam_u = pic_u->sam_u;
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa_name* nam_u = &pac_u->pag_u.nam_u;

  c3_o our_o = u3_ships_equal(nam_u->her_u, sam_u->pir_u->who_d);

  u3_peer* per_u = _mesa_get_peer(sam_u, nam_u->her_u);
  c3_o new_o = c3n;
  if ( NULL == per_u ) {
    new_o = c3y;
    per_u = new(&sam_u->par_u, u3_peer, 1);
    _init_peer(sam_u, per_u);
    _meet_peer(sam_u, per_u, nam_u->her_u);
  }

  c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
  // if ( pac_u->hed_u.hop_y == 0 ) {
  //   u3l_log(" received direct page");
  // } else {
  //   u3l_log(" received forwarded page");
  // }
  _hear_peer(sam_u, per_u, lan_u, dir_o);

  if ( new_o == c3y ) {
    //u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    //_log_lane(&lan_u);
  }

  _mesa_put_peer(sam_u, nam_u->her_u, per_u);

  u3_pit_entry* pin_u = _mesa_get_pit(sam_u, nam_u);

  if ( NULL != pin_u ) {
    #ifdef MESA_DEBUG
      u3l_log(" forwarding page");
    #endif

    inc_hopcount(&pac_u->hed_u);
    c3_etch_word(pac_u->pag_u.sot_u, ntohl(lan_u.sin_addr.s_addr));
    c3_etch_short(pac_u->pag_u.sot_u + 4, ntohs(lan_u.sin_port));

    //  stick next hop in packet

    _mesa_add_hop(pac_u->hed_u.hop_y, &pac_u->hed_u, &pac_u->pag_u, lan_u);

    _mesa_send_pact(sam_u, pin_u->adr_u, NULL, pac_u);
    _mesa_del_pit(sam_u, nam_u);
  }

  c3_d lev_d = mesa_num_leaves(pac_u->pag_u.dat_u.tob_d);
  u3_pend_req* req_u = _mesa_get_request(sam_u, nam_u);
  if ( !req_u ) {
    return;
  }
  if ( (u3_pend_req*)CTAG_WAIT == req_u ) {
    if (0 != nam_u->fra_d) {
      return;
    }
    // process incoming response to ourselves
    // if single-leaf message, inject directly into Arvo
    c3_d lev_d = mesa_num_leaves(pac_u->pag_u.dat_u.tob_d);
    if ( 1 == lev_d ) {
      u3_noun cad;
      {
        u3_noun lan = u3_mesa_encode_lane(lan_u);

        //  XX should just preserve input buffer
        u3i_slab sab_u;
        u3i_slab_init(&sab_u, 3, PACT_SIZE);
        mesa_etch_pact_to_buf(sab_u.buf_y, PACT_SIZE, pac_u);
        cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));
      }

      u3_noun wir = u3nc(c3__ames, u3_nul);

      u3_ovum* ovo = u3_ovum_init(0, c3__ames, wir, cad);
      ovo = u3_auto_plan(&sam_u->car_u, ovo);

      // early deletion, to avoid injecting retries,
      //    (in case of failure, the retry timer will add it again to the PIT)
      //
      _mesa_del_request(sam_u, nam_u);
      u3_auto_peer(ovo, 0, 0, _mesa_page_bail_cb);
      return;
    }


    c3_d now_d = _get_now_micros();
    _mesa_req_pact_init(sam_u, pic_u, lan_u, per_u);
    return;
  }

  if ( c3y == nam_u->nit_o ) {
    /* u3l_log("dupe init"); */
    return;
  }

  sockaddr_in lon_u = {0};
  if ( HOP_SHORT == pac_u->hed_u.nex_y ) {
    lon_u.sin_family = AF_INET;
    lon_u.sin_addr.s_addr = htonl(c3_sift_word(pac_u->pag_u.sot_u));
    lon_u.sin_port = htons(c3_sift_short(pac_u->pag_u.sot_u + 4));
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

  c3_y boq_y = 31;
  // c3_o done_with_jumbo_frame = __(0 == req_u->hav_d % boq_y);
  c3_o done_with_jumbo_frame = __(req_u->hav_d == req_u->tof_d); // TODO: fix for non-message-sized jumbo frames
  if ( c3y == done_with_jumbo_frame ) {
    u3_noun cad;

    // u3l_log(" received last packet, tof_d: %" PRIu64 " tob_d: %" PRIu64,
    //         req_u->tof_d,
    //         req_u->tob_d);

    /* c3_d now_d = _get_now_micros(); */
    /* u3l_log("%" PRIu64 " kilobytes took %f ms", */
    /*         req_u->tof_d, */
    /*         (now_d - sam_u->tim_d)/1000.0); */
    // u3l_log("page handling took %"PRIu64, avg_time());
    //done = c3y;

    #ifndef PACKET_TEST
    {
      // construct jumbo frame
      u3_noun lan = u3_mesa_encode_lane(lan_u);
      u3_noun pac;
      {
        pac_u->pag_u.nam_u.boq_y = boq_y;
        pac_u->pag_u.dat_u.tob_d = req_u->tob_d;
        pac_u->pag_u.nam_u.fra_d = (req_u->hav_d >> boq_y);
        pac_u->pag_u.dat_u.len_w = req_u->tob_d;
        pac_u->pag_u.dat_u.fra_y = req_u->dat_y;
        pac_u->pag_u.dat_u.aut_u = req_u->aut_u;

        c3_y* buf_y = c3_calloc(mesa_size_pact(pac_u));
        c3_w res_w = mesa_etch_pact_to_buf(buf_y, mesa_size_pact(pac_u), pac_u);
        pac = u3i_bytes(res_w, buf_y);
        c3_free(buf_y);
      }
      cad = u3nt(c3__heer, lan, pac);
    }


    _mesa_del_request(sam_u, &pac_u->pag_u.nam_u);

    u3_auto_plan(&sam_u->car_u,
                 u3_ovum_init(0, c3__ames, u3nc(c3__ames, u3_nul), cad));
   #else
    _mesa_del_request(sam_u, &pac_u->pag_u.nam_u);
   #endif
  } else if ( req_u->hav_d < lev_d ) {
    _mesa_request_next_fragments(sam_u, req_u, lan_u);
  }
}

static void
_mesa_hear_peek(u3_mesa_pict* pic_u, sockaddr_in lan_u)
{
  #ifdef MESA_DEBUG
    u3l_log("mesa: hear_peek()");
    u3_assert( PACT_PEEK == pic_u->pac_u.hed_u.typ_y );
  #endif

  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;
  c3_o our_o = u3_ships_equal(pac_u->pek_u.nam_u.her_u, sam_u->pir_u->who_d);

  if ( c3n == our_o ) {
    _mesa_forward_request(sam_u, pic_u, lan_u);
    return;
  }

  c3_d  fra_d = pac_u->pek_u.nam_u.fra_d;
  c3_d  bat_d = _mesa_lop(fra_d);

  pac_u->pek_u.nam_u.fra_d = bat_d;

  /* u3l_log("hear peek fra %llu", fra_d); */

  // if we have the page, send it
  u3_mesa_line* lin_u = _mesa_get_jumbo_cache(sam_u, &pac_u->pek_u.nam_u);

  if ( ( (u3_mesa_line*)CTAG_WAIT == lin_u )) {
    return;
  }

  if ( ( NULL != lin_u && (u3_mesa_line*)CTAG_BLOCK != lin_u)) {
    _mesa_send_piece(sam_u, lin_u, &pac_u->pek_u.nam_u, fra_d, lan_u);
    return;
  }


  // record interest
  _mesa_add_lane_to_pit(sam_u, &pac_u->pek_u.nam_u, lan_u);

  // otherwise, if blocked or NULL scry
  lin_u = (u3_mesa_line*)CTAG_WAIT;
  /* _mesa_copy_name(&lin_u->nam_u, &pac_u->pek_u.nam_u);  // XX */

  _mesa_put_jumbo_cache(sam_u, &pac_u->pek_u.nam_u, lin_u);
  u3_noun sky = _name_to_jumbo_scry(&pac_u->pek_u.nam_u);
  u3_noun our = u3i_chubs(2, sam_u->car_u.pir_u->who_d);
  u3_noun bem = u3nc(u3nt(our, u3_nul, u3nc(c3__ud, 1)), sky);

  arena are_u = arena_create(sizeof(u3_mesa_cb_data) + 1024);
  u3_scry_handle* han_u = new(&are_u, u3_scry_handle, 1);
  han_u->are_u = are_u;
  u3_mesa_cb_data* dat_u = new(&han_u->are_u, u3_mesa_cb_data, 1);
  han_u->vod_p = dat_u;
  dat_u->sam_u = sam_u;
  _mesa_copy_name(&dat_u->nam_u, &pac_u->pek_u.nam_u, &han_u->are_u);

  u3_pier_peek(sam_u->car_u.pir_u, u3_nul, u3k(u3nq(1, c3__beam, c3__ax, bem)), han_u, _mesa_page_scry_jumbo_cb);
}

static void
_mesa_poke_bail_cb(u3_ovum* egg_u, u3_noun lud)
{
  // XX failure stuff here
  u3l_log("mesa: poke failure");
}

static void
_mesa_hear_poke(u3_mesa_pict* pic_u, sockaddr_in lan_u)
{
  u3_mesa_pact* pac_u = &pic_u->pac_u;
  u3_mesa* sam_u = pic_u->sam_u;

  #ifdef MESA_DEBUG
    u3l_log("mesa: hear_poke()");
    u3_assert( PACT_POKE == pac_u->hed_u.typ_y );
  #endif

  c3_o our_o = u3_ships_equal(pac_u->pek_u.nam_u.her_u, sam_u->pir_u->who_d);

  if ( c3n == our_o ) {
    _mesa_forward_request(sam_u, pic_u, lan_u);
    return;
  }
  //  TODO check if lane already in pit, drop dupes
  _mesa_add_lane_to_pit(sam_u, &pac_u->pek_u.nam_u, lan_u);

  //  XX if this lane management stuff is necessary
  // it should be deferred to after successful event processing
  u3_peer* per_u = _mesa_get_peer(sam_u, pac_u->pok_u.pay_u.her_u);
  c3_o new_o = c3n;
  if ( NULL == per_u ) {
    new_o = c3y;
    per_u = new(&sam_u->par_u, u3_peer, 1);
    _init_peer(sam_u, per_u);
    _meet_peer(sam_u, per_u, pac_u->pok_u.pay_u.her_u);
  }

  c3_o dir_o = __(pac_u->hed_u.hop_y == 0);
  if ( pac_u->hed_u.hop_y == 0 ) {
    new_o = c3y;
    _hear_peer(sam_u, per_u, lan_u, dir_o);
    // u3l_log("learnt lane");
  } else {
    // u3l_log("received forwarded poke");
  }
  if ( new_o == c3y ) {
    // u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    // _log_lane(lan_u);
  }
  //  XX _meet_peer, in the _saxo_cb, is already putting the peer in her_p
  //
  _mesa_put_peer(sam_u, pac_u->pok_u.pay_u.her_u, per_u);

  u3_pend_req* req_u = _mesa_get_request(sam_u, &pac_u->pok_u.pay_u);
  if ( req_u != NULL) {
    // u3l_log("req pending");
    return;
  }

  u3_ovum_peer nes_f;
  u3_ovum_bail bal_f;
  void*        ptr_v;

  u3_noun wir = u3nc(c3__ames, u3_nul);
  u3_noun cad;
  {
    u3_noun    lan = u3_mesa_encode_lane(lan_u);
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, 3, PACT_SIZE);

    //  XX should just preserve input buffer
    mesa_etch_pact_to_buf(sab_u.buf_y, PACT_SIZE, pac_u);
    cad = u3nt(c3__heer, lan, u3i_slab_mint(&sab_u));
  }

  u3_ovum* ovo = u3_ovum_init(0, c3__ames, wir, cad);
           ovo = u3_auto_plan(&sam_u->car_u, ovo);

  //  XX check request state for *payload* (in-progress duplicate)
  assert(pac_u->pok_u.dat_u.tob_d);

  u3_auto_peer(ovo, 0, 0, _mesa_poke_bail_cb);
}

void
_ames_hear(void*    sam_u,
           const struct sockaddr* adr_u,
           c3_w     len_w,
           c3_y*    hun_y);

static void
_mesa_hear(u3_mesa* sam_u,
           const struct sockaddr* adr_u,
           c3_w     len_w,
           c3_y*    hun_y)
{
  /* fwrite(&len_w, 4, 1, packs); */
  /* fwrite(hun_y, 1, len_w, packs); */
  // c3_d now_d = _get_now_micros();
  if ( c3n == mesa_is_new_pact(hun_y, len_w) ) {
    c3_y* han_y = c3_malloc(len_w);
    memcpy(han_y, hun_y, len_w);
    _ames_hear(u3_Host.sam_u, adr_u, len_w, han_y);
    return;
  }

  u3_mesa_pict* pic_u = new(&sam_u->are_u, u3_mesa_pict, 1);
  pic_u->sam_u = sam_u;
  c3_c* err_c = mesa_sift_pact_from_buf(&pic_u->pac_u, hun_y, len_w);
  if ( err_c ) {
    u3l_log("mesa: hear: sift failed: %s", err_c);
    return;
  }

  sockaddr_in sdr_u = *((sockaddr_in*)adr_u);

  switch ( pic_u->pac_u.hed_u.typ_y ) {
    case PACT_PEEK: {
      _mesa_hear_peek(pic_u, sdr_u);
    } break;
    case PACT_PAGE: {
      _mesa_hear_page(pic_u, sdr_u);
    } break;
    default: {
      _mesa_hear_poke(pic_u, sdr_u);
    } break;
  }
  // if (done == c3n) {
    // if (tidx < 200000) {
      // tim_y[tidx] = _get_now_micros() - now_d;
      // tidx++;
    // } else {
      // u3l_log("peek handling took %"PRIu64, avg_time());
      // tidx = 0;
    // }
  // } else {
    // tidx = 0;
    // done = c3n;
  // }
}

static void _mesa_recv_cb(uv_udp_t*        wax_u,
              ssize_t          nrd_i,
              const uv_buf_t * buf_u,
              const struct sockaddr* adr_u,
              unsigned         flg_i)
{
  u3_mesa* sam_u = (u3_mesa*)wax_u->data;
  if ( 0 > nrd_i ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("mesa: recv: fail: %s", uv_strerror(nrd_i));
    }
  }
  else if ( 0 == nrd_i ) {
  }
  else if ( flg_i & UV_UDP_PARTIAL ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("mesa: recv: fail: message truncated");
    }
  }
  else {
    _mesa_hear(wax_u->data, adr_u, (c3_w)nrd_i, (c3_y*)buf_u->base);
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
    u3_noun wir = u3nc(c3__ames, u3_nul);
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
  uv_udp_recv_start(&u3_Host.wax_u, _mesa_alloc, _mesa_recv_cb);

  c3_i rec_i = 2 * 1024 * 1024;
  uv_recv_buffer_size((uv_handle_t*)&u3_Host.wax_u, &rec_i);

  uv_send_buffer_size((uv_handle_t*)&u3_Host.wax_u, &rec_i);

  sam_u->car_u.liv_o = c3y;
  //u3z(rac); u3z(who);
}

static void _mesa_clear_pit(uv_timer_t *tim_u)
{
  u3_mesa* sam_u = (u3_mesa*)tim_u->data;
  pit_map_itr itr_u = vt_first( &sam_u->pit_u );

  c3_d now_d = _get_now_micros();

  while (!vt_is_end(itr_u)) {
    u3_pit_entry* pit_u = itr_u.data->val;
    if ( (now_d - pit_u->tim_d) > PIT_EXPIRE_MICROS) {
      itr_u = vt_erase_itr(&sam_u->pit_u, itr_u);
    } else {
      itr_u = vt_next(itr_u);
    }
  }
}

/* _mesa_io_init(): initialize ames I/O.
*/
u3_auto*
u3_mesa_io_init(u3_pier* pir_u)
{
  u3l_log("mesa: INIT");
  /* packs = fopen("/home/ec2-user/pages.packs", "rb"); */
  arena par_u     = arena_create(67108864);
  u3_mesa* sam_u  = new(&par_u, u3_mesa, 1);
  sam_u->par_u    = par_u;
  sam_u->pir_u    = pir_u;

  arena are_u;
  are_u.dat = (char*)are_y;
  are_u.beg = (char*)are_y;
  are_u.end = (char*)(are_y + 524288);
  sam_u->are_u = are_u;

  sam_u->jum_d = 0;

  uv_timer_init(u3L, &sam_u->tim_u);

  // clear pit every 30 seconds
  sam_u->tim_u.data = sam_u;
  uv_timer_start(&sam_u->tim_u, _mesa_clear_pit, 30000, 30000);

  vt_init(&sam_u->pit_u);
  vt_init(&sam_u->per_u);
  vt_init(&sam_u->gag_u);
  vt_init(&sam_u->jum_u);
  vt_init(&sam_u->req_u);

  //  Disable networking for fake ships
  //
  if ( c3y == sam_u->pir_u->fak_o ) {
    u3_Host.ops_u.net = c3n;
  }

  sam_u->for_o = c3n;
  {
    u3_noun her = u3i_chubs(2, pir_u->who_d);
    if ( c3y == u3a_is_cat(her) && her < 256 ) {
      u3l_log("mesa: forwarding enabled");
      sam_u->for_o = c3y;
    }
    u3z(her);
  }


  u3_auto* car_u = &sam_u->car_u;
  memset(car_u, 0, sizeof(*car_u));
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

    now = u3m_time_in_tv(&tim_u);
    //sam_u->sev_l = u3r_mug(now);
    u3z(now);
  }*/

  return car_u;
}
