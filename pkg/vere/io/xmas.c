/// @file

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"
#include "cubic.h"
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

//#define XMAS_DEBUG     c3y
//#define XMAS_TEST
#define XMAS_VER       1
#define FINE_PAGE      4096             //  packets per page
#define FINE_FRAG      1024             //  bytes per fragment packet
#define FINE_PATH_MAX   384             //  longest allowed scry path
#define HEAD_SIZE         4             //  header size in bytes
#define PACT_SIZE      1472
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
#define REORDER_THRESH  5

#define IN_FLIGHT  10

// pending interest sentinels
#define XMAS_ITEM         1  // cached item
#define XMAS_WAIT         2  // waiting on scry

// routing table sentinels
#define XMAS_CZAR         1  // pending dns lookup
#define XMAS_ROUT         2  // have route
//
// hop enum
#define HOP_NONE 0b0
#define HOP_SHORT 0b1
#define HOP_LONG 0b10
#define HOP_MANY 0b11

#define XMAS_COOKIE_LEN   4
static c3_y XMAS_COOKIE[4] = { 0x5e, 0x1d, 0xad, 0x51 };


#define SIFT_VAR(dest, src, len) dest = 0; for(int i = 0; i < len; i++ ) { dest |= ((src + i) >> (8*i)); }
#define CHECK_BOUNDS(cur) if ( len_w < cur ) { u3l_log("xmas: failed parse (%u,%u) at line %i", len_w, cur, __LINE__); return 0; }
#define safe_dec(num) (num == 0 ? num : num - 1)
#define _xmas_met3_w(a_w) ((c3_bits_word(a_w) + 0x7) >> 3)

typedef struct _u3_bitset {
  c3_w  len_w;
  c3_y* buf_y;
} u3_bitset;



/** typedef u3_xmas_pack_stat u3_noun
 *  [last=@da tries=@ud]
 */
struct _u3_xmas_pact;

typedef struct _u3_pact_stat {
  c3_d  sen_d; // last sent
  c3_y  sip_y; // skips 
  c3_y  tie_y; // tries
  c3_y  dup_y; // dupes
} u3_pact_stat;

struct _u3_xmas;

typedef struct _u3_peer_last {
  c3_d acc_d; // time of last access
  c3_d son_d; // time of sponsor check
} u3_peer_last;

typedef struct _u3_buf {
  c3_y* fra_y;
  c3_w  fra_w;
} u3_buf;

typedef struct _u3_pend_req {
  c3_w                   nex_w;
  c3_w                   tot_w;
  uv_timer_t             tim_u;
  struct _u3_xmas_pact*  pac_u;
  u3_pact_stat*          wat_u; // ((mop @ud packet-state) lte)
  u3_bitset              was_u; // ((mop @ud packet-state) lte)
  c3_y*                  dat_y; // ((mop @ud *) lte)
  c3_w                   len_w;
  c3_w                   lef_w; // lowest fragment number currently in flight/pending
  c3_w                   old_w; // frag num of oldest packet sent
  c3_w                   ack_w; // highest acked fragment number
  u3_lane                lan_u; // last lane heard
  u3_gage*               gag_u;
  u3_vec(u3_buf)         mis_u; // misordered blake hash
  blake_bao*             bao_u; // blake verifier
} u3_pend_req;

typedef struct _u3_czar_info {
  c3_w               pip_w;
  time_t             tim_t;
  u3_noun            pen;
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
  c3_l               sev_l;
  c3_o               for_o;             //  is forwarding
  ur_cue_test_t*     tes_u;             //  cue-test handle
  u3_cue_xeno*       sil_u;             //  cue handle
  u3p(u3h_root)      her_p;             //
  u3p(u3h_root)      pac_p;             // pending
  u3p(u3h_root)      lan_p;             // lanes
  u3_czar_info       imp_u[256];
  u3p(u3h_root)      req_p;            // hashtable of u3_pend_req
  c3_c*              dns_c;
  c3_d              tim_d;
} u3_xmas;

typedef struct _u3_peer {
  u3_peer_last   las_u; // last check timestamps
  u3_lane        dir_u; // direct lane (if any)
  u3_lane        ind_u; // indirect lane (if any)
  c3_s           imp_s; // galaxy
  u3_xmas*       sam_u; // backpointer
} u3_peer;


typedef enum _u3_xmas_ptag {
  PACT_RESV = 0,
  PACT_PAGE = 1,
  PACT_PEEK = 2,
  PACT_POKE = 3,
} u3_xmas_ptag;

typedef enum _u3_xmas_rout_tag {
  ROUT_GALAXY = 0,
  ROUT_OTHER = 1
} u3_xmas_rout_tag;

typedef enum _u3_xmas_nexh {
  NEXH_NONE = 0,
  NEXH_SBYT = 1,
  NEXH_ONLY = 2,
  NEXH_MANY = 3
} u3_xmas_nexh;

typedef struct _u3_xmas_name_meta {
  c3_y         ran_y;  // rank (2 bits)
  c3_y         rif_y;  // rift-len (2 bits)
  c3_y         nit_y;  // initial overlay (1 bit)
  c3_y         tau_y;  // %data (0) or %auth (1), 0 if !nit_o (1 bit)
  c3_y         gaf_y;  // fragment number length (2bit)
} u3_xmas_name_meta;

typedef struct _u3_xmas_name {
  // u3_xmas_name_meta  met_u;
  c3_d               her_d[2];
  c3_w               rif_w;
  c3_y               boq_y;
  c3_o               nit_o;
  c3_o               aut_o;
  c3_w               fra_w;
  c3_s               pat_s;
  c3_c*              pat_c;
} u3_xmas_name;

typedef struct _u3_xmas_data_meta {
  c3_y         bot_y;  // total-fragments len (2 bits)
  c3_y         aul_y;  // auth-left (message) type (2 bits)
  c3_y         aur_y;  // auth-right (packet) type (2 bits)
  c3_y         men_y;  // fragment length/type (2 bits)
} u3_xmas_data_meta;

typedef enum  {
  AUTH_NONE = 0,
  AUTH_NEXT = 1,  // %1, must be two hash
  AUTH_SIGN = 2,  // %0, hashes are optional depending on num frag
  AUTH_HMAC = 3
} u3_xmas_auth_type;

typedef struct _u3_xmas_data {
  // u3_xmas_data_meta   met_u;
  c3_w                tot_w;  // total fragments
  struct {
    u3_xmas_auth_type typ_e;  // none, traversal (none), sig, or hmac
    union {                   //
      c3_y        sig_y[64];  // signature
      c3_y        mac_y[32];  // hmac
    };
  } aum_u;
  struct {
    c3_y       len_y;         //  number of hashes (0, 1, or 2)
    c3_y       has_y[2][32];  //  hashes
  } aup_u;
  c3_w                len_w;  // fragment length
  c3_y*               fra_y;  // fragment
} u3_xmas_data;

typedef struct _u3_xmas_rout {
  u3_xmas_rout_tag  typ_y;  // type tag
  union {
    c3_y            imp_y;
    u3_lane         lan_u;
  };
} u3_xmas_rout;

typedef struct _u3_xmas_head {
  u3_xmas_nexh     nex_y;  // next-hop
  c3_y             pro_y;  // protocol version
  u3_xmas_ptag     typ_y;  // packet type
  c3_y             hop_y;  // hopcount
  c3_w             mug_w; // mug checksum
} u3_xmas_head;

// 
// +$  cache
//   [%rout lanes=(list lanes)]
//   [%pending pacs=(list pact)]


typedef struct _u3_xmas_peek_pact {
  u3_xmas_name     nam_u;
} u3_xmas_peek_pact;

typedef struct _u3_xmas_hop {
  c3_w  len_w;
  c3_y* dat_y;
} u3_xmas_hop_once;

typedef struct _u3_xmas_hop_more {
  c3_w len_w;
  u3_xmas_hop_once* dat_y;
} u3_xmas_hop_more;

typedef union {
  c3_y sot_u[6];
  u3_xmas_hop_once one_u;
  u3_xmas_hop_more man_u;
} u3_xmas_hop;


typedef struct _u3_xmas_page_pact {
  u3_xmas_name            nam_u;
  u3_xmas_data            dat_u;
  union {
    c3_y sot_u[6];
    u3_xmas_hop_once one_u;
    u3_xmas_hop_more man_u;
  };
} u3_xmas_page_pact;

typedef struct _u3_xmas_poke_pact {
  u3_xmas_name            nam_u;
  u3_xmas_name            pay_u;
  u3_xmas_data            dat_u;
} u3_xmas_poke_pact;

typedef struct _u3_xmas_pact {
  u3_xmas_head      hed_u;
  uv_udp_send_t     snd_u;
  struct _u3_xmas*  sam_u;
  union {
    u3_xmas_poke_pact  pok_u;
    u3_xmas_page_pact  pag_u;
    u3_xmas_peek_pact  pek_u;
  };
} u3_xmas_pact;

typedef enum _u3_xmas_ctag {
  CACE_WAIT = 1,
  CACE_ITEM = 2,
} u3_xmas_ctag;

/*
 *  typedef u3_xmas_req u3_noun
 *  [tot=@ waiting=(set @ud) missing=(set @ud) nex=@ud dat=(map @ud @)]
 */


typedef struct _u3_cace_enty {
  u3_xmas_ctag      typ_y;
  union {
    // u03_xmas_cace_wait  wat_u;
    // u3_xmas_cace_item  res_u;
  };
} u3_cace_enty;

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
_log_head(u3_xmas_head* hed_u)
{
  u3l_log("-- HEADER --");
  u3l_log("next hop: %u", hed_u->nex_y);
  u3l_log("protocol: %u", hed_u->pro_y);
  u3l_log("packet type: %u", hed_u->typ_y);
  u3l_log("mug: 0x%05x", (hed_u->mug_w & 0xfffff));
  u3l_log("hopcount: %u", hed_u->hop_y);
  u3l_log("");
}

static void
_log_lane(u3_lane* lan_u)
{
  u3l_log("xmas: lane (%s,%u)", u3r_string(u3dc("scot", c3__if, u3i_word(lan_u->pip_w))), lan_u->por_s);
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
_log_name(u3_xmas_name* nam_u)
{
  // u3l_log("meta");
  // u3l_log("rank: %u", nam_u->met_u.ran_y);
  // u3l_log("rift length: %u", nam_u->met_u.rif_y);
  // u3l_log("nit: %u", nam_u->met_u.nit_y);
  // u3l_log("tau: %u", nam_u->met_u.tau_y);
  // u3l_log("frag num length: %u", nam_u->met_u.gaf_y);

  {
    u3_noun her = u3dc("scot", c3__p, u3i_chubs(2, nam_u->her_d));
    c3_c* her_c = u3r_string(her);
    u3l_log("publisher: %s", her_c);
    c3_free(her_c);
    u3z(her);
  }

  u3l_log("rift: %u", nam_u->rif_w);
  u3l_log("bloq: %u", nam_u->boq_y);
  u3l_log("init: %s", (c3y == nam_u->nit_o) ? "&" : "|");
  u3l_log("auth: %s", (c3y == nam_u->aut_o) ? "&" : "|");
  u3l_log("frag: %u", nam_u->fra_w);
  u3l_log("path len: %u", nam_u->pat_s);
  u3l_log("path: %s", nam_u->pat_c);
}

static void
_log_data(u3_xmas_data* dat_u)
{
  u3l_log("total fragments: %u", dat_u->tot_w);

  switch ( dat_u->aum_u.typ_e ) {
    case AUTH_NONE: {
      if ( dat_u->aup_u.len_y ) {
        u3l_log("strange no auth");
      }
      else {
        u3l_log("no auth");
      }
    } break;

    case AUTH_NEXT: {
      if ( 2 != dat_u->aup_u.len_y ) {
        u3l_log("bad merkle traversal");
      }
      else {
        u3l_log("merkle traversal:");
        _log_buf(dat_u->aup_u.has_y[0], 32);
        _log_buf(dat_u->aup_u.has_y[1], 32);
      }
    } break;

    case AUTH_SIGN: {
      u3l_log("signature:");
      _log_buf(dat_u->aum_u.sig_y, 64);
    } break;

    case AUTH_HMAC: {
      u3l_log("hmac:");
      _log_buf(dat_u->aum_u.mac_y, 32);
    } break;
  }

  switch ( dat_u->aum_u.typ_e ) {
    case AUTH_SIGN:
    case AUTH_HMAC: {
      if ( !dat_u->aup_u.len_y ) {
        break;
      }

      if ( 4 < dat_u->tot_w ) {
        u3l_log("strange inline proof");
      }
      else {
        u3l_log("inline proof");
      }

      for ( int i = 0; i < dat_u->aup_u.len_y; i++ ) {
        _log_buf(dat_u->aup_u.has_y[i], 32);
      }
    } break;

    default: break;
  }

  u3l_log("frag len: %u", dat_u->len_w);
}

static void
_log_peek_pact(u3_xmas_peek_pact* pac_u)
{
  _log_name(&pac_u->nam_u);
}

static void
_log_page_pact(u3_xmas_page_pact *pac_u)
{
  _log_name(&pac_u->nam_u);
  _log_data(&pac_u->dat_u);
}

static void
_log_poke_pact(u3_xmas_poke_pact *pac_u)
{
  _log_name(&pac_u->nam_u);
  _log_name(&pac_u->pay_u);
  _log_data(&pac_u->dat_u);
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
_log_pend_req(u3_pend_req* req_u)
{
  if( req_u == NULL ) {
    u3l_log("pending request was NULL");
    return;
  }
  u3l_log("next: %u", req_u->nex_w);
  u3l_log("total: %u", req_u->tot_w);
  //u3l_log("timer in: %" PRIu64 " ms", uv_timer_get_due_in(&req_u->tim_u));
}

static void
_log_pact(u3_xmas_pact* pac_u)
{
  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      _log_peek_pact(&pac_u->pek_u);
    } break;

    case PACT_PAGE: {
      _log_page_pact(&pac_u->pag_u);
    } break;

    case PACT_POKE: {
      _log_poke_pact(&pac_u->pok_u);
    } break;

    default: {
      _log_poke_pact(&pac_u->pok_u);
      break;
    }
  }
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

static void
_update_hopcount(u3_xmas_head* hed_u)
{
  hed_u->hop_y = c3_max(hed_u->hop_y+1, 7);
}

static inline void
_get_her(u3_xmas_pact* pac_u, c3_d* our_d)
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




static void
_init_bitset(u3_bitset* bit_u, c3_w len_w)
{
  bit_u->buf_y = c3_calloc(len_w >> 3);
  bit_u->len_w = len_w;
}

static void
_free_bitset(u3_bitset* bit_u)
{
  c3_free(bit_u->buf_y);
}

static c3_w
_wyt_bitset(u3_bitset* bit_u)
{
  c3_w ret_w = 0;
  for(int i = 0; i < (bit_u->len_w >> 3); i++ ) {
    ret_w += __builtin_popcount(bit_u->buf_y[i]);
  }
  return ret_w;
}

static void
_put_bitset(u3_bitset* bit_u, c3_w mem_w)
{
  if (( mem_w > bit_u->len_w )) {
    u3l_log("overrun %u, %u", mem_w, bit_u->len_w);
    return;
  }
  c3_w idx_w = mem_w >> 3;
  c3_w byt_y = bit_u->buf_y[idx_w];
  c3_y rem_y = mem_w & 0x7;
  c3_y mas_y = (1 << rem_y);
  bit_u->buf_y[idx_w] = byt_y | mas_y;
}

static c3_o
_has_bitset(u3_bitset* bit_u, c3_w mem_w) {
  if (( mem_w > bit_u->len_w )) {
    u3l_log("overrun %u, %u", mem_w, bit_u->len_w);
    return c3n;
  }

  u3_assert( mem_w < bit_u->len_w );
  c3_w idx_w = mem_w >> 3;
  c3_y rem_y = mem_w & 0x7;
  return __( (bit_u->buf_y[idx_w] >> rem_y) & 0x1);
}

static void
_del_bitset(u3_bitset* bit_u, c3_w mem_w)
{
  u3_assert( mem_w < bit_u->len_w );
  c3_w idx_w = mem_w >> 3;
  c3_w byt_y = bit_u->buf_y[idx_w];
  c3_y rem_y = mem_w & 0x7;
  c3_y mas_y = ~(1 << rem_y);
  bit_u->buf_y[idx_w] &= mas_y;
}


static void
_log_bitset(u3_bitset* bit_u)
{
  c3_w cur_w = 0;
  u3l_log("logging bitset");
  while( cur_w < bit_u->len_w ) {
    if ( c3y == _has_bitset(bit_u, cur_w) ) {
      u3l_log("%u", cur_w);
    }
    cur_w++;
  }
  u3l_log("finished");
}

// TODO: fix with scry into jael??
// remove rear call to enable star routing
static u3_noun
_xmas_get_sponsor_list(c3_d her_d[2])
{
  u3_noun her = u3i_chubs(2, her_d);
  u3_noun lis = u3do("rear", u3do("^saxo:title", her));
  return lis;
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

/* _ames_chub_bytes(): c3_y[8] to c3_d
** XX factor out, deduplicate with other conversions
*/
static inline c3_d
_ames_chub_bytes(c3_y byt_y[8])
{
  return (c3_d)byt_y[0]
       | (c3_d)byt_y[1] << 8
       | (c3_d)byt_y[2] << 16
       | (c3_d)byt_y[3] << 24
       | (c3_d)byt_y[4] << 32
       | (c3_d)byt_y[5] << 40
       | (c3_d)byt_y[6] << 48
       | (c3_d)byt_y[7] << 56;
}

/* _ames_ship_to_chubs(): pack [len_y] bytes into c3_d[2]
*/
static inline void
_ames_ship_to_chubs(c3_d sip_d[2], c3_y len_y, c3_y* buf_y)
{
  c3_y sip_y[16] = {0};
  memcpy(sip_y, buf_y, c3_min(16, len_y));

  sip_d[0] = _ames_chub_bytes(sip_y);
  sip_d[1] = _ames_chub_bytes(sip_y + 8);
}

/* _ames_chub_bytes(): c3_d to c3_y[8]
** XX factor out, deduplicate with other conversions
*/
static inline void
_ames_bytes_chub(c3_y byt_y[8], c3_d num_d)
{
  byt_y[0] = num_d & 0xff;
  byt_y[1] = (num_d >>  8) & 0xff;
  byt_y[2] = (num_d >> 16) & 0xff;
  byt_y[3] = (num_d >> 24) & 0xff;
  byt_y[4] = (num_d >> 32) & 0xff;
  byt_y[5] = (num_d >> 40) & 0xff;
  byt_y[6] = (num_d >> 48) & 0xff;
  byt_y[7] = (num_d >> 56) & 0xff;
}

static inline void
_ames_ship_of_chubs(c3_d sip_d[2], c3_y len_y, c3_y* buf_y)
{
  c3_y sip_y[16] = {0};

  _ames_bytes_chub(sip_y, sip_d[0]);
  _ames_bytes_chub(sip_y + 8, sip_d[1]);

  memcpy(buf_y, sip_y, c3_min(16, len_y));
}


static inline c3_s
_ames_sift_short(c3_y buf_y[2])
{
  return (buf_y[1] << 8 | buf_y[0]);
}

static inline c3_w
_ames_sift_word(c3_y buf_y[4])
{
  return (buf_y[3] << 24 | buf_y[2] << 16 | buf_y[1] << 8 | buf_y[0]);
}


static c3_o
_xmas_sift_head(c3_y buf_y[8], u3_xmas_head* hed_u)
{
  if ( memcmp(buf_y + 4, &XMAS_COOKIE, XMAS_COOKIE_LEN) ) {
    return c3n;

  }
  c3_w hed_w = _ames_sift_word(buf_y);

  hed_u->nex_y = (hed_w >> 2)  & 0x3;
  hed_u->pro_y = (hed_w >> 4)  & 0x7;
  hed_u->typ_y = (hed_w >> 7)  & 0x3;
  hed_u->hop_y = (hed_w >> 9)  & 0x7;
  hed_u->mug_w = (hed_w >> 12) & 0xFFFFFF;

  assert( 1 == hed_u->pro_y );

  return c3y;

  /*if(c3o((hed_u->typ_y == PACT_PEEK), (hed_u->typ_y == PACT_POKE))) {
    hed_u->ran_y = (hed_w >> 30) & 0x3;
  }*/
}

static c3_w
_xmas_sift_name(u3_xmas_name* nam_u, c3_y* buf_y, c3_w len_w)
{
#ifdef XMAS_DEBUG
  //u3l_log("xmas: sifting name %i", len_w);
#endif

  c3_w cur_w = 0;
  u3_xmas_name_meta met_u;

  CHECK_BOUNDS(cur_w + 1);
  c3_y met_y = buf_y[cur_w];
  met_u.ran_y = (met_y >> 0) & 0x3;
  met_u.rif_y = (met_y >> 2) & 0x3;
  met_u.nit_y = (met_y >> 4) & 0x1;
  met_u.tau_y = (met_y >> 5) & 0x1;
  met_u.gaf_y = (met_y >> 6) & 0x3;
  cur_w += 1;

  c3_y her_y = 2 << met_u.ran_y;
  CHECK_BOUNDS(cur_w + her_y)
  _ames_ship_to_chubs(nam_u->her_d, her_y, buf_y + cur_w);
  cur_w += her_y;

  c3_y rif_y = met_u.rif_y + 1;
  nam_u->rif_w = 0;
  CHECK_BOUNDS(cur_w + rif_y)
  for( int i = 0; i < rif_y; i++ ) {
    nam_u->rif_w |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  CHECK_BOUNDS(cur_w + 1);
  nam_u->boq_y = buf_y[cur_w];
  cur_w++;

  if ( met_u.nit_y ) {
    assert( !met_u.tau_y );
    // XX init packet
    nam_u->fra_w = 0;
  }
  else {
    c3_y fag_y = met_u.gaf_y + 1;
    CHECK_BOUNDS(cur_w + fag_y);
    for ( int i = 0; i < fag_y; i++ ) {
      nam_u->fra_w |= (buf_y[cur_w] << (8*i));
      cur_w++;
    }
  }

  nam_u->nit_o = ( met_u.nit_y ) ? c3y : c3n;
  // XX ?:(=(1 tau.c) %auth %data)
  nam_u->aut_o = ( met_u.tau_y ) ? c3y : c3n;

  CHECK_BOUNDS(cur_w + 2)
  nam_u->pat_s = buf_y[cur_w]
               | (buf_y[cur_w + 1] << 8);
  cur_w += 2;

  nam_u->pat_c = c3_calloc(nam_u->pat_s + 1); // unix string for ease of manipulation
  CHECK_BOUNDS(cur_w + nam_u->pat_s);
  memcpy(nam_u->pat_c, buf_y + cur_w, nam_u->pat_s);
  nam_u->pat_c[nam_u->pat_s] = 0;
  cur_w += nam_u->pat_s;

  return cur_w;
}

static c3_w
_xmas_sift_data(u3_xmas_data* dat_u, c3_y* buf_y, c3_w len_w)
{
#ifdef XMAS_DEBUG
  //u3l_log("xmas: sifting data %i", len_w);
#endif

  c3_w cur_w = 0;
  u3_xmas_data_meta met_u;

  CHECK_BOUNDS(cur_w + 1);
  c3_y met_y = buf_y[cur_w];
  met_u.bot_y = (met_y >> 0) & 0x3;
  met_u.aul_y = (met_y >> 2) & 0x3;
  met_u.aur_y = (met_y >> 4) & 0x3;
  met_u.men_y = (met_y >> 6) & 0x3;
  cur_w += 1;

  c3_y tot_y = met_u.bot_y + 1;
  CHECK_BOUNDS(cur_w + tot_y);
  dat_u->tot_w = 0;
  for( int i = 0; i < tot_y; i++ ) {
    dat_u->tot_w |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  c3_y aum_y = ( 2 == met_u.aul_y ) ? 64 :
               ( 3 == met_u.aul_y ) ? 32 : 0;
  CHECK_BOUNDS(cur_w + aum_y);
  memcpy(dat_u->aum_u.sig_y, buf_y + cur_w, aum_y);
  cur_w += aum_y;

  dat_u->aum_u.typ_e = met_u.aul_y; // XX

  assert( 3 > met_u.aur_y );

  CHECK_BOUNDS(cur_w + (met_u.aur_y * 32));
  dat_u->aup_u.len_y = met_u.aur_y;
  for( int i = 0; i < met_u.aur_y; i++ ) {
    memcpy(dat_u->aup_u.has_y[i], buf_y + cur_w, 32);
    cur_w += 32;
  }

  c3_y nel_y = met_u.men_y;

  if ( 3 == nel_y ) {
    CHECK_BOUNDS(cur_w + 1);
    nel_y = buf_y[cur_w];
    cur_w++;
  }

  CHECK_BOUNDS(cur_w + nel_y);
  dat_u->len_w = 0;
  for ( int i = 0; i < nel_y; i++ ) {
    dat_u->len_w |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  CHECK_BOUNDS(cur_w + dat_u->len_w);
  dat_u->fra_y = c3_calloc(dat_u->len_w);
  memcpy(dat_u->fra_y, buf_y + cur_w, dat_u->len_w);
  cur_w += dat_u->len_w;

  return cur_w;
}

static c3_w
_xmas_sift_hop_long(u3_xmas_hop_once* hop_u, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0;
  CHECK_BOUNDS(cur_w + 1);
  hop_u->len_w = buf_y[cur_w];
  cur_w++;
  CHECK_BOUNDS(cur_w + hop_u->len_w);
  hop_u->dat_y = c3_calloc(hop_u->len_w);
  memcpy(hop_u->dat_y, buf_y + cur_w, hop_u->len_w);

  return cur_w;
}


static c3_w
_xmas_sift_page_pact(u3_xmas_page_pact* pac_u, c3_y nex_y, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0, nex_w;

  if ( !(nex_w = _xmas_sift_name(&pac_u->nam_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _xmas_sift_data(&pac_u->dat_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  switch ( nex_y ) {
    default: {
      u3l_log("xmas: bad hopcount");
      return 0;
    }
    case HOP_NONE: break;
    case HOP_SHORT: {
      CHECK_BOUNDS(cur_w + 6);
      memcpy(pac_u->sot_u, buf_y + cur_w, 6);
      cur_w += 6;
    } break;
    case HOP_LONG: {
      c3_w hop_w = _xmas_sift_hop_long(&pac_u->one_u, buf_y + cur_w, len_w - cur_w);
      if( hop_w == 0 ) {
        return 0;
      }
      cur_w += hop_w;
    } break;
    case HOP_MANY: {
      CHECK_BOUNDS(cur_w + 1);
      pac_u->man_u.len_w = buf_y[cur_w];
      cur_w++;

      pac_u->man_u.dat_y = c3_calloc(sizeof(u3_xmas_hop_once) * pac_u->man_u.len_w);

      for( int i = 0; i < pac_u->man_u.len_w; i++ ) {
        c3_w hop_w = _xmas_sift_hop_long(&pac_u->man_u.dat_y[i], buf_y + cur_w ,len_w - cur_w);
        if ( hop_w == 0 ) {
          return 0;
        }
        cur_w += hop_w;
      }
    }
  }

  return cur_w;
}


static c3_w
_xmas_sift_peek_pact(u3_xmas_peek_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w siz_w = _xmas_sift_name(&pac_u->nam_u, buf_y, len_w);
  if ( siz_w < len_w ) {
    u3l_log("xmas: failed to consume entire packet");
    _log_buf(buf_y + siz_w, len_w - siz_w);
    return 0;
  }

  return siz_w;
}

static c3_w
_xmas_sift_poke_pact(u3_xmas_poke_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0, nex_w;
  //  ack path
  if ( !(nex_w = _xmas_sift_name(&pac_u->nam_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  //  payload path
  if ( !(nex_w = _xmas_sift_name(&pac_u->pay_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  //  payload
  if ( !(nex_w = _xmas_sift_data(&pac_u->dat_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  return cur_w;
}

static c3_w
_xmas_sift_pact(u3_xmas_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w res_w = 0;

  if ( len_w < 8 ) {
    u3l_log("xmas: attempted to parse overly short packet of size %u", len_w);
  }

  _xmas_sift_head(buf_y, &pac_u->hed_u);
  buf_y += 8;
  len_w -= 8;

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      res_w = _xmas_sift_peek_pact(&pac_u->pek_u, buf_y, len_w);
    } break;
    case PACT_PAGE: {
      res_w = _xmas_sift_page_pact(&pac_u->pag_u, pac_u->hed_u.nex_y, buf_y, len_w);
    } break;
    case PACT_POKE: {
      res_w = _xmas_sift_poke_pact(&pac_u->pok_u, buf_y, len_w);
    } break;
    default: {
      u3l_log("xmas: received unknown packet type");
      _log_buf(buf_y, len_w);
      break;
    }
  }
  //u3_assert(res_w <= len_w );
  return res_w + 8;
}

// cut and pasted from ames.c
//
static void
_ames_etch_short(c3_y buf_y[2], c3_s sot_s)
{
  buf_y[0] = sot_s         & 0xff;
  buf_y[1] = (sot_s >>  8) & 0xff;
}

static void
_ames_etch_word(c3_y buf_y[4], c3_w wod_w)
{
  buf_y[0] = wod_w         & 0xff;
  buf_y[1] = (wod_w >>  8) & 0xff;
  buf_y[2] = (wod_w >> 16) & 0xff;
  buf_y[3] = (wod_w >> 24) & 0xff;
}


static u3_atom
_dire_etch_ud(c3_d num_d)
{
  c3_y  hun_y[26];
  c3_y* buf_y = u3s_etch_ud_smol(num_d, hun_y);
  c3_w  dif_w = (c3_p)buf_y - (c3_p)hun_y;
  return u3i_bytes(26 - dif_w, buf_y); // XX known-non-null
}

static void
_xmas_etch_head(u3_xmas_head* hed_u, c3_y buf_y[8])
{
#ifdef XMAS_DEBUG
  if( c3y == XMAS_DEBUG ) {
    if( hed_u->pro_y > 7 ) {
      u3l_log("xmas: bad protocol version");
      return;
    }
  }
#endif

  assert( 1 == hed_u->pro_y );

  // c3_o req_o = c3o((hed_u->typ_y == PACT_PEEK), (hed_u->typ_y == PACT_POKE));
  // c3_y siz_y = req_o ? 5 : 7;
  c3_w hed_w = (hed_u->nex_y & 0x3) << 2
             ^ (hed_u->pro_y & 0x7) << 4  // XX constant, 1
             ^ (hed_u->typ_y & 0x3) << 7
             ^ (hed_u->hop_y & 0x7) << 9
             ^ (hed_u->mug_w & 0xFFFFFF) << 12;
             // XX: we don't expand hopcount if no request. Correct?
      //
  /*if ( c3y == req_o ) {
    hed_w = hed_w ^ ((hed_u->ran_y & 0x3) << 30);
  }*/

  _ames_etch_word(buf_y, hed_w);
  memcpy(buf_y + 4, XMAS_COOKIE, XMAS_COOKIE_LEN);
}

static c3_y
_xmas_rank(c3_d who_d[2])
{
  if ( who_d[1] ) {
    return 3;
  }
  else if ( who_d[0] >> 32 ) {
    return 2;
  }
  else if ( who_d[0] >> 16 ) {
    return 1;
  }
  else {
    return 0;
  }
}

static c3_w
_xmas_etch_name(c3_y* buf_y, u3_xmas_name* nam_u)
{
#ifdef XMAS_DEBUG

#endif 
  c3_w cur_w = 0;
  u3_xmas_name_meta met_u;

  met_u.ran_y = _xmas_rank(nam_u->her_d);
  met_u.rif_y = safe_dec(_xmas_met3_w(nam_u->rif_w));

  if ( c3y == nam_u->nit_o ) {
    assert( c3n == nam_u->aut_o ); // XX
    met_u.nit_y = 1;
    met_u.tau_y = 0;
    met_u.gaf_y = 0;
  }
  else {
    met_u.nit_y = 0;
    met_u.tau_y = (c3y == nam_u->aut_o) ? 1 : 0;
    met_u.gaf_y = safe_dec(_xmas_met3_w(nam_u->fra_w));
  }

  c3_y met_y = (met_u.ran_y & 0x3) << 0
             ^ (met_u.rif_y & 0x3) << 2
             ^ (met_u.nit_y & 0x1) << 4
             ^ (met_u.tau_y & 0x1) << 5
             ^ (met_u.gaf_y & 0x3) << 6;

  buf_y[cur_w] = met_y;

  //ship
  cur_w++;
  c3_y her_y = 2 << met_u.ran_y; // XX confirm
  _ames_ship_of_chubs(nam_u->her_d, her_y, buf_y + cur_w);
  cur_w += her_y;

  // rift
  c3_y rif_y = met_u.rif_y + 1;
  for ( int i = 0; i < rif_y; i++) {
    buf_y[cur_w] = (nam_u->rif_w >> (8*i)) & 0xff;
    cur_w++;
  }

  buf_y[cur_w] = nam_u->boq_y;
  cur_w++;

  c3_y fra_y = (c3y == nam_u->nit_o) ? 0 : met_u.gaf_y + 1;
  for( int i = 0; i < fra_y; i++ ) {
    buf_y[cur_w] = (nam_u->fra_w >> (8*i)) & 0xff;
    cur_w++;
  }

  // path length
  c3_y pat_y = 2;
  for ( int i = 0; i < pat_y; i++ ) {
    buf_y[cur_w] = (nam_u->pat_s >> (8*i)) & 0xff;
    cur_w++;
  }

  // path
  memcpy(buf_y + cur_w, nam_u->pat_c, nam_u->pat_s);
  cur_w += nam_u->pat_s;

  return cur_w;
}

static c3_w
_xmas_etch_data(c3_y* buf_y, u3_xmas_data* dat_u)
{
#ifdef XMAS_DEBUG

#endif
  c3_w cur_w = 0;
  u3_xmas_data_meta met_u;

  met_u.bot_y = safe_dec(_xmas_met3_w(dat_u->tot_w));

  // XX
  met_u.aul_y = dat_u->aum_u.typ_e;
  met_u.aur_y = dat_u->aup_u.len_y;

  c3_y nel_y = _xmas_met3_w(dat_u->len_w);
  met_u.men_y = (3 >= nel_y) ? nel_y : 3;

  c3_y met_y = (met_u.bot_y & 0x3) << 0
             ^ (met_u.aul_y & 0x3) << 2
             ^ (met_u.aur_y & 0x3) << 4
             ^ (met_u.men_y & 0x3) << 6;
  buf_y[cur_w] = met_y;
  cur_w++;

  c3_y tot_y = met_u.bot_y + 1;
  for (int i = 0; i < tot_y; i++ ) {
    buf_y[cur_w] = (dat_u->tot_w >> (8 * i)) & 0xFF;
    cur_w++;
  }

  switch ( dat_u->aum_u.typ_e ) {
    case AUTH_SIGN: {
      memcpy(buf_y + cur_w, dat_u->aum_u.sig_y, 64);
      cur_w += 64;
    } break;

    case AUTH_HMAC: {
      memcpy(buf_y + cur_w, dat_u->aum_u.mac_y, 32);
      cur_w += 32;
    } break;

    default: break;
  }

  for ( int i = 0; i < dat_u->aup_u.len_y; i++ ) {
    memcpy(buf_y + cur_w, dat_u->aup_u.has_y[i], 32);
    cur_w += 32;
  }

  if ( 3 == met_u.men_y ) {
    buf_y[cur_w] = nel_y;
    cur_w++;
  }

  memcpy(buf_y + cur_w, (c3_y*)&dat_u->len_w, nel_y);
  cur_w += nel_y;

  memcpy(buf_y + cur_w, dat_u->fra_y, dat_u->len_w);
  cur_w += dat_u->len_w;

  return cur_w;
}

static c3_w
_xmas_etch_page_pact(c3_y* buf_y, u3_xmas_page_pact* pac_u, u3_xmas_head* hed_u)
{
  c3_w cur_w = 0, nex_w;

  if ( !(nex_w = _xmas_etch_name(buf_y + cur_w, &pac_u->nam_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _xmas_etch_data(buf_y + cur_w, &pac_u->dat_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  // XX hops

  return cur_w;
}

static c3_w
_xmas_etch_poke_pact(c3_y* buf_y, u3_xmas_poke_pact* pac_u, u3_xmas_head* hed_u)
{
  c3_w cur_w = 0, nex_w;

  if ( !(nex_w = _xmas_etch_name(buf_y + cur_w, &pac_u->nam_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _xmas_etch_name(buf_y + cur_w, &pac_u->pay_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _xmas_etch_data(buf_y + cur_w, &pac_u->dat_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  return cur_w;
}

static c3_w
_xmas_size_name(u3_xmas_name* nam_u)
{
  c3_w siz_w = 1;
  u3_xmas_name_meta met_u;

  met_u.ran_y = _xmas_rank(nam_u->her_d);
  met_u.rif_y = safe_dec(_xmas_met3_w(nam_u->rif_w));

  siz_w += 2 << met_u.ran_y;
  siz_w += met_u.rif_y + 1;
  siz_w++;  // bloq

  if (c3n == nam_u->nit_o ) {
    met_u.gaf_y = safe_dec(_xmas_met3_w(nam_u->fra_w));
    siz_w += met_u.gaf_y + 1;
  }

  siz_w += 2;  // path-length
  siz_w += nam_u->pat_s;

  return siz_w;
}

static c3_w
_xmas_size_data(u3_xmas_data* dat_u)
{
  c3_w siz_w = 1;
  u3_xmas_data_meta met_u;

  met_u.bot_y = safe_dec(_xmas_met3_w(dat_u->tot_w));

  siz_w += met_u.bot_y + 1;

  switch ( dat_u->aum_u.typ_e ) {
    case AUTH_SIGN: {
      siz_w += 64;
    } break;

    case AUTH_HMAC: {
      siz_w += 32;
    } break;

    default: break;
  }

  siz_w += 32 * dat_u->aup_u.len_y;

  c3_y nel_y = _xmas_met3_w(dat_u->len_w);
  met_u.men_y = (3 >= nel_y) ? nel_y : 3;

  if ( 3 == met_u.men_y ) {
    siz_w++;
  }

  siz_w += nel_y;
  siz_w += dat_u->len_w;

  return siz_w;
}

static c3_w
_xmas_size_pact(u3_xmas_pact* pac_u)
{
  c3_w siz_w = 8; // header + cookie;

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      siz_w += _xmas_size_name(&pac_u->pek_u.nam_u);
    } break;

    case PACT_PAGE: {
      siz_w += _xmas_size_name(&pac_u->pag_u.nam_u);
      siz_w += _xmas_size_data(&pac_u->pag_u.dat_u);
      // XX hops
    } break;

    case PACT_POKE: {
      siz_w += _xmas_size_name(&pac_u->pok_u.nam_u);
      siz_w += _xmas_size_name(&pac_u->pok_u.pay_u);
      siz_w += _xmas_size_data(&pac_u->pok_u.dat_u);
    } break;

    default: {
      u3l_log("bad pact type %u", pac_u->hed_u.typ_y);//u3m_bail(c3__bail);
      return 0;
    }
  }

  return siz_w;
}

static c3_w
_xmas_etch_pact(c3_y* buf_y, u3_xmas_pact* pac_u)
{
  c3_w siz_w = _xmas_size_pact(pac_u);
  if ( siz_w > PACT_SIZE ) {
    fprintf(stderr, "etch: would overflow %u\r\n", siz_w);
    return 0;
  }

  c3_w cur_w = 0, nex_w;
  u3_xmas_head* hed_u = &pac_u->hed_u;
  _xmas_etch_head(hed_u, buf_y + cur_w);
  cur_w += 8;

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_POKE: {
      if ( !(nex_w = _xmas_etch_poke_pact(buf_y + cur_w, &pac_u->pok_u, hed_u)) ) {
        return 0;
      }
    } break;

    case PACT_PEEK: {
      if ( !(nex_w = _xmas_etch_name(buf_y + cur_w, &pac_u->pek_u.nam_u)) ) {
        return 0;
      }
    } break;

    case PACT_PAGE: {
      if ( !(nex_w = _xmas_etch_page_pact(buf_y + cur_w, &pac_u->pag_u, hed_u)) ) {
        return 0;
      }
    } break;

    default: {
      u3l_log("bad pact type %u", pac_u->hed_u.typ_y);//u3m_bail(c3__bail);
      return 0;
    }
  }

  cur_w += nex_w;

  assert( siz_w == cur_w );

  return cur_w;
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

// xx: imcomplete
static void _xmas_pact_free(u3_xmas_pact* pac_u) {
  switch ( pac_u->hed_u.typ_y ) {
    default: {
      u3m_bail(c3__foul);
    };
    case PACT_PEEK: {
      break;
    };
    case PACT_PAGE: {
      c3_free(pac_u->pag_u.dat_u.fra_y);
      break;
    };
    case PACT_POKE: {
      break;
    };
  }
  c3_free(pac_u);
}


/* u3_xmas_encode_lane(): serialize lane to noun
*/
static u3_noun
u3_xmas_encode_lane(u3_lane lan_u) {
  // [%if ip=@ port=@]
  return u3nt(c3__if, u3i_word(lan_u.pip_w), lan_u.por_s);
}

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

/* _xmas_get_request(): produce pending request state for nam_u
 *
 *   produces a NULL pointer if no pending request exists
*/
static u3_pend_req*
_xmas_get_request(u3_xmas* sam_u, u3_xmas_name* nam_u) {
  u3_pend_req* ret_u = NULL;
  u3_noun key = _xmas_request_key(nam_u);

  u3_weak res = u3h_git(sam_u->req_p, key);
  if ( res != u3_none && res != u3_nul ) {
    ret_u = u3to(u3_pend_req, res);
  }
  if ( ret_u != NULL && ret_u->tot_w != 0 && ret_u->tot_w == ret_u->len_w ) {
    ret_u = NULL;
  }

  u3z(key);
  return ret_u;
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
  _xmas_pact_free(req_u->pac_u);
  _free_bitset(&req_u->was_u);
  c3_free(req_u->pac_u);
  c3_free(req_u->dat_y);
  u3a_free(req_u);
  u3h_del(sam_u->req_p, key);
  u3z(key);
}

/* _xmas_put_request(): save new pending request state for nam_u
 *
 *   the memory in the hashtable is allocated once in the lifecycle of the 
 *   request. req_u will be copied into the hashtable memory, and so can be
 *   immediately freed
 *
*/
static void
_xmas_put_request(u3_xmas* sam_u, u3_xmas_name* nam_u, u3_pend_req* req_u) {
  u3_noun key = _xmas_request_key(nam_u);

  if ( req_u == NULL) {
    u3h_put(sam_u->req_p, key, u3_nul);
    u3z(key);
    return;
  }
  u3_pend_req* old_u = _xmas_get_request(sam_u, nam_u);
  u3_pend_req* new_u = req_u;
  if ( old_u == NULL ) {
    new_u = u3a_calloc(sizeof(u3_pend_req),1);
    memcpy(new_u, req_u, sizeof(u3_pend_req));
  } else {
    new_u = old_u;
    memcpy(new_u, req_u, sizeof(u3_pend_req));
  }


  u3_noun val = u3of(u3_pend_req, new_u);
  u3h_put(sam_u->req_p, key, val);
  u3z(key);
}

/* _xmas_packet_timeout(): callback for packet timeout
*/
static void
_xmas_packet_timeout(uv_timer_t* tim_u) {
  u3_pend_req* req_u = (u3_pend_req*)tim_u->data;
  u3l_log("%u packet timed out", req_u->old_w);
}

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
 * _xmas_req_get_cwnd(): produce current state of packet pump
 *
 * saves next fragment number and preallocated pact into the passed pointers.
 * Will not do so if returning 0
*/
static c3_w
_xmas_req_get_cwnd_nex(u3_xmas* sam_u, u3_xmas_name* nam_u, c3_w* nex_w, u3_xmas_pact** nex_u)
{
  c3_w res_w = 0;
  u3_pend_req* req_u = _xmas_get_request(sam_u, nam_u);
  if ( NULL == req_u ) {
    return 0;
  }
  *nex_u = req_u->pac_u;
  if ( req_u->tot_w == 0 || req_u->gag_u == NULL ) {
    u3l_log("shouldn't happen");
    *nex_w = 0;
    return 1;
  }

  *nex_w = req_u->nex_w;

  c3_w liv_w = _wyt_bitset(&req_u->was_u);

  c3_w rem_w = req_u->tot_w - req_u->nex_w + 1;
  return c3_min(rem_w, req_u->gag_u->wnd_w - liv_w);
}

/* _xmas_req_pact_sent(): mark packet as sent
**
*/
static void
_xmas_req_pact_resent(u3_xmas* sam_u, u3_xmas_name* nam_u)
{
  c3_d now_d = _get_now_micros();
  u3_pend_req* req_u = _xmas_get_request(sam_u, nam_u);
  // if we dont have pending request noop
  if ( NULL == req_u ) {
    return;
  }

  req_u->wat_u[nam_u->fra_w].sen_d = now_d;
  req_u->wat_u[nam_u->fra_w].tie_y++;
}

/* _xmas_req_pact_sent(): mark packet as sent
**
*/
static void
_xmas_req_pact_sent(u3_xmas* sam_u, u3_xmas_name* nam_u)
{
  c3_d now_d = _get_now_micros();
  u3_pend_req* req_u = _xmas_get_request(sam_u, nam_u);

  // if we already have pending request
  if ( NULL != req_u ) {
    if( req_u->nex_w == nam_u->fra_w ) {
      req_u->nex_w++;
    }
    // TODO: optional assertions?
    req_u->wat_u[nam_u->fra_w] = (u3_pact_stat){now_d, 0, 1, 0 };
    _put_bitset(&req_u->was_u, nam_u->fra_w);
    if ( nam_u->fra_w == 1 ) {
      req_u->lef_w = 1;
    }

  } else {
    // instantiate pending request
    // stack allocating is ok, because _xmas_put_pend will copy it out anyway
    req_u = alloca(sizeof(u3_pend_req));
    memset(req_u, 0, sizeof(u3_pend_req));
    req_u->pac_u = c3_calloc(sizeof(u3_xmas_pact));
    req_u->pac_u->sam_u = sam_u;
    req_u->pac_u->hed_u.typ_y = PACT_PEEK;
    req_u->gag_u = NULL;
    memcpy(&req_u->pac_u->pek_u.nam_u, nam_u, sizeof(u3_xmas_name));
    // TODO: handle restart
    // u3_assert( nam_u->fra_w == 0 ); 
    req_u->nex_w = 1;
    req_u->tot_w = 0;
    req_u->len_w = 0;
    req_u->dat_y = NULL;
    uv_timer_init(u3L, &req_u->tim_u);
    req_u->wat_u = NULL;
    req_u->was_u.buf_y = NULL;
    req_u->lef_w = 0;
    req_u->old_w = 1;
    req_u->ack_w = 0;
  }

  if ( req_u->lef_w != 0 && c3n == _has_bitset(&req_u->was_u, req_u->lef_w) ) {
    while ( req_u->lef_w++ < req_u->tot_w ) {
      if ( c3y == _has_bitset(&req_u->was_u, req_u->lef_w) ) {
	break;
      }
    }
  }
  _xmas_put_request(sam_u, nam_u, req_u);
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

/* u3_xmas_lane_to_chub(): serialize lane to double-word
*/
static c3_d
u3_xmas_lane_to_chub(u3_lane lan) {
  return ((c3_d)lan.por_s << 32) ^ (c3_d)lan.pip_w;
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
  //c3_free(sel_u->buf_y);
  //c3_free(sel_u);
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

static void
_update_oldest_req(u3_pend_req *req_u, u3_gage* gag_u)
{
  if( req_u->tot_w == 0 ) {
    return;
  }
  // scan in flight packets, find oldest
  c3_w idx_w = req_u->lef_w;
  c3_d now_d = _get_now_micros();
  c3_d wen_d = now_d;
  for ( c3_w i = req_u->lef_w; i < req_u->nex_w; i++ ) {
    if ( c3y == _has_bitset(&req_u->was_u, i) &&
	 wen_d > req_u->wat_u[i].sen_d
    ) {
      wen_d = req_u->wat_u[i].sen_d;
      idx_w = i;
    }
  }
  if ( now_d == wen_d ) {
#ifdef XMAS_DEBUG
    u3l_log("failed to find new oldest");
    _log_bitset(&req_u->was_u);
#endif
  }
  req_u->old_w = idx_w;
  req_u->tim_u.data = req_u;
  uv_timer_start(&req_u->tim_u, _xmas_packet_timeout, ((req_u->wat_u[req_u->old_w].sen_d + gag_u->rto_w) / 1000), 0);
}

static void _xmas_send(u3_xmas_pact* pac_u, u3_lane* lan_u)
{
  u3_xmas* sam_u = pac_u->sam_u;

  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w siz_w = _xmas_etch_pact(buf_y, pac_u);

  _xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
}


static void
_try_resend(u3_pend_req* req_u)
{
  u3_xmas* sam_u = req_u->pac_u->sam_u;
  u3_lane* lan_u = &req_u->lan_u;
  c3_o los_o = c3n;
  if ( req_u->tot_w == 0 || req_u->ack_w <= REORDER_THRESH ) {
    return;
  }
  c3_w ack_w = req_u->ack_w - REORDER_THRESH;
  c3_d now_d = _get_now_micros();
  for( int i = req_u->lef_w; i < ack_w; i++ ) {
    if ( c3y == _has_bitset(&req_u->was_u, i) ) {
      req_u->pac_u->pek_u.nam_u.fra_w = i;
      if ( req_u->wat_u[i].tie_y == 1 ) {
	los_o = c3y;
	c3_y* buf_y = c3_calloc(PACT_SIZE);
	req_u->pac_u->pek_u.nam_u.fra_w = i;
	c3_w siz_w  = _xmas_etch_pact(buf_y, req_u->pac_u);
	if( siz_w == 0 ) {
	  u3l_log("failed to etch");
	  u3_assert( 0 );
	}
	// TODO: better route management
	_xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
	_xmas_req_pact_resent(sam_u, &req_u->pac_u->pek_u.nam_u);

      } else if ( (now_d - req_u->wat_u[i].sen_d) > req_u->gag_u->rto_w ) {
	los_o = c3y;
	c3_y* buf_y = c3_calloc(PACT_SIZE);
	req_u->pac_u->pek_u.nam_u.fra_w = i;
	u3l_log("slow resending %u ", i);
	c3_w siz_w  = _xmas_etch_pact(buf_y, req_u->pac_u);
	if( siz_w == 0 ) {
	  u3l_log("failed to etch");
	  u3_assert( 0 );
	}
	// TODO: better route management
	_xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
	_xmas_req_pact_resent(sam_u, &req_u->pac_u->pek_u.nam_u);
      }
    }
  }
  
  if ( c3y == los_o ) {
    req_u->gag_u->sst_w = (req_u->gag_u->wnd_w / 2) + 1;
    req_u->gag_u->wnd_w = req_u->gag_u->sst_w;
    req_u->gag_u->rto_w = _clamp_rto(req_u->gag_u->rto_w * 2);
  }
}

static u3_vec(c3_y[BLAKE3_OUT_LEN])*
_xmas_get_proof_vec(u3_xmas_data* dat_u)
{
  u3_assert( dat_u->aum_u.typ_e == AUTH_SIGN );
  u3_assert( dat_u->aup_u.len_y != 0 );
  u3_vec(c3_y[BLAKE3_OUT_LEN])* pof_u = vec_make(4);
  {
    c3_y* lef_y = c3_calloc(BLAKE3_OUT_LEN);
    blake_node* nod_u = blake_leaf_hash(dat_u->fra_y, dat_u->len_w, 0);
    memcpy(lef_y, nod_u->cev_y, BLAKE3_OUT_LEN);
    c3_free(nod_u);
    vec_append(pof_u, lef_y);
  }
  
  for ( c3_w i_w = 0; i_w < dat_u->aup_u.len_y; i_w++ ) {
    c3_y* has_y = c3_calloc(BLAKE3_OUT_LEN);
    memcpy(has_y, dat_u->aup_u.has_y[i_w], BLAKE3_OUT_LEN);
    vec_append(pof_u, has_y);
  }
  return pof_u;
}

static c3_y
_xmas_verify_data(u3_xmas_data* dat_u, blake_bao* bao_u)
{
  blake_pair* par_u = NULL;
  return blake_bao_verify(bao_u, dat_u->fra_y, dat_u->len_w, par_u);
}

/* _xmas_req_pact_done(): mark packet as done, possibly producing the result 
*/
static u3_weak
_xmas_req_pact_done(u3_xmas* sam_u, u3_xmas_name *nam_u, u3_xmas_data* dat_u, u3_lane* lan_u)
{
  u3_weak ret = u3_none;
  c3_d now_d = _get_now_micros();
  u3_pend_req* req_u = _xmas_get_request(sam_u, nam_u);
  
  if ( NULL == req_u ) {
#ifdef XMAS_DEBUG
    u3l_log("xmas: attempting to mark packet done, no request"); 
#endif
    return u3_none;
  }

  u3_gage* gag_u = _xmas_get_lane(sam_u, nam_u->her_d, lan_u);
  req_u->gag_u = gag_u;

  // first we hear from lane
  if ( gag_u == NULL ) {
    gag_u = alloca(sizeof(u3_gage));
    memset(gag_u, 0, sizeof(u3_gage));
    _init_gage(gag_u);
  }
  //if ( gag_u->wnd_w > 20 ) { }



  req_u->lan_u = *lan_u;

  c3_w siz_w = (1 << (nam_u->boq_y - 3));
  // First packet received, instantiate request fully
  if ( req_u->tot_w == 0 ) {
    u3_assert( siz_w == 1024 ); // boq_y == 13
    req_u->dat_y = c3_calloc(siz_w * dat_u->tot_w);
    req_u->wat_u = c3_calloc(sizeof(u3_pact_stat) * dat_u->tot_w);
    req_u->tot_w = dat_u->tot_w;
    _init_bitset(&req_u->was_u, dat_u->tot_w + 1);
    req_u->lef_w = 2;
    u3_vec(c3_y[BLAKE3_OUT_LEN])* pof_u = _xmas_get_proof_vec(dat_u);
    req_u->bao_u = blake_bao_make(req_u->tot_w, pof_u);
  } else {
    //blake_bao_verify(req_u->bao_u);

  }

  if ( dat_u->tot_w <= nam_u->fra_w ) {
    u3l_log("xmas: invalid packet (exceeded bounds)");
    return u3_none;
  }

  // received duplicate
  if ( nam_u->fra_w != 0 && c3n == _has_bitset(&req_u->was_u, nam_u->fra_w) ) {
#ifdef XMAS_DEBUG
      u3l_log("duplicate page");
#endif 
 
    req_u->wat_u[nam_u->fra_w].dup_y++;
    u3l_log("duplicate %u", req_u->wat_u[nam_u->fra_w].dup_y);

    return u3_none;
  } 

  _del_bitset(&req_u->was_u, nam_u->fra_w);
  if ( nam_u->fra_w > req_u->ack_w ) {
    req_u->ack_w = nam_u->fra_w;
  }
  if ( nam_u->fra_w != 0 && req_u->wat_u[nam_u->fra_w].tie_y != 1 ) {
    u3l_log("received retry %u", nam_u->fra_w);
  }

  req_u->len_w++;
  if ( req_u->lef_w == nam_u->fra_w ) {
    req_u->lef_w++;
  }

  if ( nam_u->fra_w != 0 ) {
    // handle gauge update
    _xmas_handle_ack(gag_u, &req_u->wat_u[nam_u->fra_w]);
    if ( (nam_u->fra_w % 10) == 0 ) {
    }
  }


  memcpy(req_u->dat_y + (siz_w * nam_u->fra_w), dat_u->fra_y, dat_u->len_w);
  c3_d wen_d = _get_now_micros();

  _try_resend(req_u);
  /*
   * // in-order
  if ( nam_u->fra_w != 0 && ((nam_u->fra_w - req_u->lef_w) > 5) ) {
    u3l_log("diff: %u", _abs(nam_u->fra_w - req_u->old_w);
    c3_y* buf_y = c3_calloc(PACT_SIZE);
    req_u->pac_u->pek_u.nam_u.fra_w = req_u->old_w;
    u3l_log("resending %u on num %u", req_u->old_w, nam_u->fra_w);
    c3_w siz_w  = _xmas_etch_pact(buf_y, req_u->pac_u);
    if( siz_w == 0 ) {
      u3l_log("failed to etch");
      u3_assert( 0 );
    }
    // TODO: better route management
    _xmas_send_buf(sam_u, *lan_u, buf_y, siz_w);
    _xmas_req_pact_resent(sam_u, &req_u->pac_u->pek_u.nam_u);
  }*/
  _update_oldest_req(req_u, gag_u);


  if ( req_u->len_w == req_u->tot_w ) {
    uv_timer_stop(&req_u->tim_u);
    _xmas_del_request(sam_u, nam_u);
  //u3dc("rap", 13, u3dc("turn", u3n_slam_on(u3k(sam_u->mop_u.tap), req_u->dat), u3v_wish("tail")));
    //_xmas_put_request(sam_u, nam_u, u3_none);
    return u3i_word(req_u->tot_w);
  } else {
    _xmas_put_request(sam_u, nam_u, req_u);
    _xmas_put_lane(sam_u, nam_u->her_d, lan_u, gag_u);
  }
  return u3_none;
}




static u3_lane
_realise_lane(u3_noun lan) {
  u3_lane lan_u;
  lan_u.por_s = 0;
  lan_u.pip_w = 0;

  if ( c3y == u3a_is_cat(lan)  ) {
    u3_assert( lan < 256 );
    if ( (c3n == u3_Host.ops_u.net) ) {
      lan_u.pip_w =  0x7f000001 ;
      lan_u.por_s = _ames_czar_port(lan);
    }
  } else {
    u3_noun tag, pip, por;
    u3x_trel(lan, &tag, &pip, &por);
    if ( tag == c3__if ) {
      lan_u.pip_w = u3i_word(pip);
      u3_assert( c3y == u3a_is_cat(por) && por <= 0xFFFF);
      lan_u.por_s = por;
    } else {
      u3l_log("xmas: inscrutable lane");
      u3m_p("lane", lan);
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
_xmas_update_req_peek(u3_xmas_pact* pac_u) 
{
  u3_xmas* sam_u = pac_u->sam_u;
  _xmas_req_pact_sent(sam_u, &pac_u->pek_u.nam_u);

  // TODO: set timers
}

static void
_xmas_ef_send(u3_xmas* sam_u, u3_noun las, u3_noun pac)
{
  u3_noun len, dat;
  u3x_cell(pac, &len, &dat);
  c3_w len_w = u3r_met(3, dat);
  if ( len_w > len ) {
#ifdef XMAS_DEBUG
    u3l_log("xmas: vane lying about length %i", len_w);
    return;
#endif
  }
  c3_y* buf_y = c3_calloc(len);
  u3r_bytes(0, len, buf_y, dat);
  u3_xmas_pact pac_u;
  pac_u.sam_u = sam_u;
  _xmas_sift_pact(&pac_u, buf_y, len);

  if ( pac_u.hed_u.typ_y == PACT_PEEK ) {
    sam_u->tim_d = _get_now_micros();
    _xmas_update_req_peek(&pac_u);
  }

  c3_o suc_o = c3n;
  u3_noun lan, t = las;
  _xmas_rout_bufs(sam_u, buf_y, len, las);
  u3z(pac);
}



static c3_o _xmas_kick(u3_xmas* sam_u, u3_noun tag, u3_noun dat)
{
  c3_o ret_o;

  switch ( tag ) {
    default: {
      ret_o = c3n;
     } break;
    case c3__sent: {
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

static c3_o _xmas_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_xmas* sam_u = (u3_xmas*)car_u;

  u3_noun tag, dat;
  c3_o ret_o;

  if( c3n == u3r_cell(cad, &tag, &dat) )
  {
    ret_o = c3n;
  } else {
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
_xmas_exit_cb(uv_handle_t* had_u)
{
  u3_xmas* sam_u = had_u->data;

  u3s_cue_xeno_done(sam_u->sil_u);
  ur_cue_test_done(sam_u->tes_u);
  u3h_free(sam_u->her_p);
  u3h_free(sam_u->req_p);

  c3_free(sam_u);
}

static void 
_xmas_io_exit(u3_auto* car_u)
{
  u3_xmas* sam_u = (u3_xmas*)car_u;
  uv_close(&sam_u->had_u, _xmas_exit_cb);
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

  u3_noun res = u3kb_weld(u3nc(c3__pact, u3nq(rif, boq, fag, u3nc(c3__data, u3_nul))), pax);

  return res;
}

/*
 * RETAIN
 */
static u3_weak
_xmas_get_cache(u3_xmas* sam_u, u3_xmas_name* nam_u)
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
_xmas_put_cache(u3_xmas* sam_u, u3_xmas_name* nam_u, u3_noun val)
{
  u3_noun pax = _name_to_scry(nam_u);

  u3h_put(sam_u->pac_p, pax, u3k(val));
  u3z(pax); // TODO: fix refcount
}

/* _xmas_czar(): add packet to queue, possibly begin DNS resolution
 */
static void
_xmas_czar(u3_xmas_pact* pac_u, c3_y her_y)
{
  u3_xmas* sam_u = pac_u->sam_u;
#ifdef XMAS_DEBUG
  if ( pac_u->hed_u.typ_y != PACT_PEEK ) {
    u3l_log("xmas: attempted to resolve galaxy for packet that was not peek");
    u3m_bail(c3__oops);
    return;
  }
#endif
  u3_czar_info* her_u = &sam_u->imp_u[her_y];
  if ( her_u->pip_w == 0 ) {
    c3_y* buf_y = c3_calloc(PACT_SIZE);
    c3_w siz_w = _xmas_etch_pact(buf_y, pac_u);
    her_u->pen = u3nc(u3i_bytes(siz_w, buf_y), her_u->pen);
  }

  u3_noun pat = u3i_string(pac_u->pek_u.nam_u.pat_c);

  //u3_weak her = _xmas_get_peer(sam_u, her_y);

  c3_o lok_o = _xmas_add_galaxy_pend(sam_u, her_y, pat);

  // TODO: actually resolve DNS
  u3z(pat);
}


static void
_xmas_try_forward(u3_xmas_pact* pac_u, u3_noun fra, u3_noun hit)
{
  u3l_log("");
  u3l_log("stubbed forwarding");
}

static c3_w
_xmas_respond(u3_xmas_pact* req_u, c3_y** buf_y, u3_noun hit)
{
  u3_noun len, dat;
  u3x_cell(hit, &len, &dat);


  *buf_y = c3_calloc(len);
  u3r_bytes(0, len, *buf_y, dat);

  u3z(hit);
  return len;
}

static void
_xmas_serve_cache_hit(u3_xmas_pact* req_u, u3_lane lan_u, u3_noun hit) 
{
  u3l_log("serving cache hit");
  c3_d her_d[2];
  memcpy(her_d, req_u->pek_u.nam_u.her_d, 2);
  c3_d our_d[2];
  memcpy(our_d, req_u->sam_u->pir_u->who_d, 2);
  if (  (her_d[0] != our_d[0])
    ||  (her_d[1] != our_d[1]) ) 
  {
    u3l_log("publisher is not ours");
    if (  (256 > our_d[0])
       && (0  == our_d[1]) )
    {
      // Forward only if requested datum is not ours, and we are a galaxy
      //_xmas_try_forward(req_u, fra_s, hit);
    } else {
      u3l_log("no forward, we are not a galaxy");
    }
  } else {
    //u3l_log("first: %x", u3r_string(u3dc("scot", c3__uv, u3k(u3h(u3t(hit))))));
    c3_y* buf_y;

    c3_w len_w = _xmas_respond(req_u, &buf_y, hit);
    _xmas_send_buf(req_u->sam_u, lan_u, buf_y, len_w);
  }
  // no lose needed because all nouns are moved in both
  // branches of the conditional
}

/* 
 */
static void
_xmas_page_scry_cb(void* vod_p, u3_noun nun)
{
  u3_xmas_pact* pac_u = vod_p;
  u3_xmas* sam_u = pac_u->sam_u;
  //u3_noun pax = _xmas_path_with_fra(pac_u->pek_u.nam_u.pat_c, &fra_s);

  u3_weak hit = u3r_at(7, nun);
  if ( u3_none == hit ) {
    // TODO: mark as dead
    //u3z(nun);
    u3l_log("unbound");

  } else {
    u3_weak old = _xmas_get_cache(sam_u, &pac_u->pag_u.nam_u);
    if ( old == u3_none ) {
      u3l_log("bad");
    } else {
      u3_noun tag;
      u3_noun dat;
      u3x_cell(u3k(old), &tag, &dat);
      if ( XMAS_WAIT == tag ) {
        c3_y* buf_y;
        
        c3_w len_w = _xmas_respond(pac_u, &buf_y, u3k(hit));
        _xmas_rout_bufs(sam_u, buf_y, len_w, u3k(u3t(dat)));
      }
      _xmas_put_cache(sam_u, &pac_u->pek_u.nam_u, u3nc(XMAS_ITEM, u3k(hit)));
      // u3z(old);
    }
    // u3z(hit);
  }
  // u3z(pax);
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
  u3m_p("meet peer", pax);
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
_xmas_forward(u3_xmas_pact* pac_u, u3_lane lan_u)
{
  u3_xmas* sam_u = pac_u->sam_u;
  c3_d her_d[2];
  _get_her(pac_u, her_d);
  _update_hopcount(&pac_u->hed_u);

  if ( pac_u->hed_u.typ_y == PACT_PAGE ) {
    //u3l_log("should update next hop");
    u3_weak hit = _xmas_get_cache(sam_u, &pac_u->pag_u.nam_u);
    if ( u3_none == hit ) {
      u3l_log("xmas: got forward, no interest");
      return;
    }
    u3_noun tag, dat;
    u3x_cell(u3k(hit), &tag, &dat);
    if ( tag == XMAS_WAIT ) {
      u3m_p("lanes", u3t(dat));
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w len_w = _xmas_etch_pact(buf_y, pac_u);
      _xmas_rout_bufs(sam_u, buf_y, len_w, u3k(u3t(dat)));
    } else {
      u3l_log("xmas: weird pending interest");
    }
  } else {
    u3_peer* per_u = _xmas_get_peer(sam_u, her_d);
    if ( NULL != per_u ) {
      u3_lane lin_u = _xmas_get_direct_lane(sam_u, her_d);
      if ( lin_u.pip_w != 0 ) {
        _xmas_send(pac_u, &lin_u);
      }
    }
  }
  _xmas_pact_free(pac_u);
}



static void
_xmas_hear_page(u3_xmas_pact* pac_u, c3_y* buf_y, c3_w len_w, u3_lane lan_u)
{
#ifdef XMAS_DEBUG
  u3l_log("xmas hear page");
#endif
  u3_xmas* sam_u = pac_u->sam_u;
  u3_noun wir = u3nc(c3__xmas, u3_nul);
  c3_s fra_s;

  u3_peer* per_u = _xmas_get_peer(sam_u, pac_u->pag_u.nam_u.her_d);
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
    u3l_log("received forwarded peek");
  }
  if ( new_o == c3y ) {
    u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    _log_lane(&lan_u);
  }

  _xmas_put_peer(sam_u, pac_u->pag_u.nam_u.her_d, per_u);
  u3_weak hit = _xmas_get_cache(sam_u, &pac_u->pag_u.nam_u);

  if ( u3_none != hit ) {
    _xmas_forward(pac_u, lan_u);
    return;
  }
  
  u3_noun fra = u3i_bytes(pac_u->pag_u.dat_u.len_w, pac_u->pag_u.dat_u.fra_y) ;
  /*if ( dop_o == c3n && pac_u->pag_u.nam_u.fra_w == 150) {
    dop_o = c3y;
    u3l_log("simulating dropped packet");
    return;
  }*/

  u3_weak res = _xmas_req_pact_done(sam_u, &pac_u->pag_u.nam_u, &pac_u->pag_u.dat_u, &lan_u);
  u3l_log("done");

  if ( u3_none != res ) {
    u3l_log("finished");

    c3_d now_d = _get_now_micros();
    u3l_log("took: %f ms", (now_d - sam_u->tim_d)/1000.0f);
    return;
  }

  u3_xmas_pact* nex_u;

  c3_w nex_w;
  c3_w win_w = _xmas_req_get_cwnd_nex(sam_u, &pac_u->pag_u.nam_u, &nex_w, &nex_u);
  if ( win_w != 0 ) {
#ifdef XMAS_DEBUG
    u3l_log("continuing flow nex: %u, win: %u", nex_w, win_w);
#endif
    c3_y* buf_y = c3_calloc(PACT_SIZE * win_w);
    c3_y* cur_y = buf_y;
    for(int i = 0; i < win_w; i++) {
      nex_u->pek_u.nam_u.fra_w = nex_w + i;
      c3_w siz_w  =_xmas_etch_pact(cur_y, nex_u);
      if( siz_w == 0 ) {
        u3l_log("failed to etch");
        u3_assert( 0 );
      }
      // TODO: better route management
      _xmas_send_buf(sam_u, lan_u, cur_y, siz_w);
      _xmas_req_pact_sent(sam_u, &nex_u->pek_u.nam_u);
      cur_y += siz_w;
    }
  }
}

static void
_xmas_add_lane_to_cache(u3_xmas* sam_u, u3_xmas_name* nam_u, u3_noun las, u3_lane lan_u)
{
  u3_noun hit = u3nq(XMAS_WAIT,
                     _xmas_get_now(), 
                     u3_xmas_encode_lane(lan_u),
                     u3k(las));
  _xmas_put_cache(sam_u, nam_u, hit);
  u3z(las);
}



static void
_xmas_hear_peek(u3_xmas_pact* pac_u, u3_lane lan_u)
{
#ifdef XMAS_DEBUG
  u3l_log("xmas: hear peek");
  // u3_assert(pac_u->hed_u.typ_y == PACT_PEEK);
#endif
  u3_xmas* sam_u = pac_u->sam_u;
  c3_d her_d[2];
  her_d[0] = 0;
  her_d[1] = 0;
  memcpy(her_d, pac_u->pek_u.nam_u.her_d, 2);
  c3_o our_o = __( 0 == memcmp(her_d, sam_u->pir_u->who_d, 2) );


  if ( c3n == our_o ) {
    u3_peer* per_u = _xmas_get_peer(sam_u, her_d);
    if ( per_u == NULL ) {
      u3l_log("xmas: alien forward for %s", u3r_string(u3dc("scot", c3__p, u3i_chubs(2, her_d))));
      _xmas_pact_free(pac_u);
      return;
    }
    if ( c3y == sam_u->for_o && sam_u->pir_u->who_d[0] == per_u->imp_s ) {
//#ifdef XMAS_DEBUG
      u3_lane lin_u = _xmas_get_direct_lane(sam_u, her_d);
//#endif
      _update_hopcount(&pac_u->hed_u);
       u3l_log("sending peek %u", pac_u->pek_u.nam_u.fra_w);
      _xmas_send(pac_u, &lin_u);
    }
  }

  u3_weak hit = _xmas_get_cache(sam_u, &pac_u->pek_u.nam_u);

  if ( u3_none != hit ) {
    u3_noun tag, dat;
    u3x_cell(u3k(hit), &tag, &dat);
    if ( tag == XMAS_WAIT ) {
      _xmas_add_lane_to_cache(sam_u, &pac_u->pek_u.nam_u, u3k(u3t(dat)), lan_u);
    } else if ( c3y == our_o && tag == XMAS_ITEM ) {
      c3_y* buf_y;
//      if ( rand() % 20 == 0) {
//	u3l_log("dropping packet %u", pac_u->pek_u.nam_u.fra_w);
//	return;
//      }
      c3_w len_w = _xmas_respond(pac_u, &buf_y, u3k(dat));
      u3l_log("sending page %u", pac_u->pek_u.nam_u.fra_w);
      _xmas_send_buf(pac_u->sam_u, lan_u, buf_y, len_w);
    } else {
      u3l_log("xmas: weird case in cache, dropping");
    }
    _xmas_pact_free(pac_u);
    // u3z(hit);
  } else {
    _xmas_add_lane_to_cache(sam_u, &pac_u->pek_u.nam_u, u3_nul, lan_u); // TODO: retrieve from namespace
    if ( c3y == our_o ) {
      u3_noun sky = _name_to_scry(&pac_u->pek_u.nam_u); 

      u3_noun our = u3i_chubs(2, sam_u->car_u.pir_u->who_d);
      u3_noun bem = u3nc(u3nt(our, u3_nul, u3nc(c3__ud, 1)), sky);
      u3_pier_peek(sam_u->car_u.pir_u, u3_nul, u3k(u3nq(1, c3__beam, c3__xx, bem)), pac_u, _xmas_page_scry_cb);
    } else {
      _xmas_pact_free(pac_u);
    }
  }
  // u3z(pax);
}

// xx: should inject event directly, but vane does not work
// so we just hack it to get 
static void
_xmas_hear_poke(u3_xmas_pact* pac_u, u3_lane* lan_u)
{
  u3_xmas* sam_u = pac_u->sam_u;
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
    u3l_log("received forwarded peek");
  }
  if ( new_o == c3y ) {
    u3l_log("new lane is direct %c", c3y == dir_o ? 'y' : 'n');
    _log_lane(lan_u);
  }
  _xmas_put_peer(sam_u, pac_u->pok_u.pay_u.her_d, per_u);
}

static void
_xmas_hear(u3_xmas* sam_u,
           u3_lane* lan_u,
           c3_w     len_w,
           c3_y*    hun_y)
{
#ifdef XMAS_DEBUG
  u3l_log("xmas_hear");
#endif

  u3_xmas_pact* pac_u;
  c3_w pre_w;
  c3_y* cur_y = hun_y;
  if ( HEAD_SIZE > len_w ) {
    c3_free(hun_y);
    return;
  }

  pac_u = c3_calloc(sizeof(*pac_u));
  pac_u->sam_u = sam_u;
  c3_w lin_w = _xmas_sift_pact(pac_u, hun_y, len_w);
  if ( lin_w == 0 ) {
#ifdef XMAS_DEBUG
    u3l_log("xmas: failed to parse packet");
    //_log_pact(pac_u);
#endif
    // TODO free everything
    return;


  }
#ifdef XMAS_DEBUG
  //_log_peek_pact(&pac_u->pek_u);
  u3l_log("xmas: sifted packet");
#endif

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      _xmas_hear_peek(pac_u, *lan_u);
    } break;
    case PACT_PAGE: {
      _xmas_hear_page(pac_u, hun_y, len_w, *lan_u);
    } break;
    default: {
      _xmas_hear_poke(pac_u, lan_u);
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

    u3_auto_plan(car_u, u3_ovum_init(0, c3__x, wir, cad));
  }
  //_xmas_io_start(sam_u);
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

/* _xmas_io_init(): initialize ames I/O.
*/
u3_auto*
u3_xmas_io_init(u3_pier* pir_u)
{
  u3_xmas* sam_u  = c3_calloc(sizeof(*sam_u));
  sam_u->pir_u    = pir_u;

  sam_u->her_p = u3h_new_cache(100000);
  sam_u->req_p = u3h_new_cache(100000);
  sam_u->lan_p = u3h_new_cache(100000);

  u3_assert( !uv_udp_init(u3L, &sam_u->wax_u) );
  sam_u->wax_u.data = sam_u;

  sam_u->sil_u = u3s_cue_xeno_init();
  sam_u->tes_u = ur_cue_test_init();


  for ( c3_w i_w = 0; i_w < 5000; i_w++ ) {
    u3_noun key = u3i_word(i_w);
    u3_noun val = u3k(val);

    u3h_put(sam_u->req_p, key, val);
  }

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
      //sam_u.imp_u[i].tim = 0;
      if ( u3_Host.ops_u.net == c3y ) {
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

#ifdef XMAS_TEST 

static void
_test_bitset()
{
  u3_bitset bit_u;
  _init_bitset(&bit_u, 500);
  
  _put_bitset(&bit_u, 5);
  _put_bitset(&bit_u, 50);
  _put_bitset(&bit_u, 100);

  c3_w wyt_w = _wyt_bitset(&bit_u);
  if ( 3 != wyt_w ) {
    u3l_log("wyt failed have %u expect %u", wyt_w, 3);
    exit(1);
  }

  if ( c3y == _has_bitset(&bit_u, 3) ) {
    u3l_log("false positive for has_bitset");
    exit(1);
  }

  if ( c3n == _has_bitset(&bit_u, 50) ) {
    u3l_log("false negative for has_bitset");
    exit(1);
  }

  _del_bitset(&bit_u, 50);

  if ( c3y == _has_bitset(&bit_u, 50) ) {
    u3l_log("false positive for has_bitset");
    exit(1);
  }

  wyt_w = _wyt_bitset(&bit_u);

  if ( 2 != wyt_w ) {
    u3l_log("wyt failed have %u expect %u", wyt_w, 2);
    exit(1);
  }
}

#define cmp_scalar(nam, str, fmt)                       \
  if ( hav_u->nam != ned_u->nam ) {                     \
    fprintf(stderr, "xmas test cmp " str " differ:\r\n" \
                    "    have: " fmt "\r\n"             \
                    "    need: " fmt "\r\n",            \
                    hav_u->nam, ned_u->nam);            \
    ret_i = 1;                                          \
  }

#define cmp_string(nam, siz, str)                       \
  if ( memcmp(hav_u->nam, ned_u->nam, siz) ) {          \
    fprintf(stderr, "xmas test cmp " str " differ:\r\n" \
                    "    have: %*.s\r\n"                \
                    "    need: %*.s\r\n",               \
                    siz, hav_u->nam, siz, ned_u->nam);  \
    ret_i = 1;                                          \
  }

#define cmp_buffer(nam, siz, str)                 \
  if ( memcmp(hav_u->nam, ned_u->nam, siz) ) {    \
    fprintf(stderr, "xmas test cmp " str "\r\n"); \
    ret_i = 1;                                    \
  }

static c3_i
_test_cmp_head(u3_xmas_head* hav_u, u3_xmas_head* ned_u)
{
  c3_i ret_i = 0;

  cmp_scalar(nex_y, "head: next", "%u");
  cmp_scalar(pro_y, "head: protocol", "%u");
  cmp_scalar(typ_y, "head: type", "%u");
  cmp_scalar(hop_y, "head: hop", "%u");
  cmp_scalar(mug_w, "head: mug", "%u");

  return ret_i;
}

static c3_i
_test_cmp_name(u3_xmas_name* hav_u, u3_xmas_name* ned_u)
{
  c3_i ret_i = 0;

  cmp_buffer(her_d, sizeof(ned_u->her_d), "name: ships differ");

  cmp_scalar(rif_w, "name: rifts", "%u");
  cmp_scalar(boq_y, "name: bloqs", "%u");
  cmp_scalar(nit_o, "name: inits", "%u");
  cmp_scalar(aut_o, "name: auths", "%u");
  cmp_scalar(fra_w, "name: fragments", "%u");
  cmp_scalar(pat_s, "name: path-lengths", "%u");

  cmp_string(pat_c, ned_u->pat_s, "name: paths");

  return ret_i;
}

static c3_i
_test_cmp_data(u3_xmas_data* hav_u, u3_xmas_data* ned_u)
{
  c3_i ret_i = 0;

  cmp_scalar(tot_w, "data: total packets", "%u");

  cmp_scalar(aum_u.typ_e, "data: auth-types", "%u");
  cmp_buffer(aum_u.sig_y, 64, "data: sig|hmac|null");

  cmp_scalar(aup_u.len_y, "data: hash-lengths", "%u");
  cmp_buffer(aup_u.has_y, ned_u->aup_u.len_y, "data: hashes");

  cmp_scalar(len_w, "data: fragments-lengths", "%u");
  cmp_buffer(fra_y, ned_u->len_w, "data: fragments");

  return ret_i;
}

static c3_i
_test_pact(u3_xmas_pact* pac_u)
{
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w  len_w =_xmas_etch_pact(buf_y, pac_u);
  c3_i  ret_i = 0;
  c3_i  bot_i = 0;
  c3_w  sif_w;

  u3_xmas_pact nex_u;
  memset(&nex_u, 0, sizeof(u3_xmas_pact));

  if ( !len_w ) {
    fprintf(stderr, "pact: etch failed\r\n");
    ret_i = 1; goto done;
  }
  else if ( len_w > PACT_SIZE ) {
    fprintf(stderr, "pact: etch overflowed: %u\r\n", len_w);
    ret_i = 1; goto done;
  }

  if ( len_w != (sif_w = _xmas_sift_pact(&nex_u, buf_y, len_w)) ) {
    fprintf(stderr, "pact: sift failed len=%u sif=%u\r\n", len_w, sif_w);
    _log_buf(buf_y, len_w);
    ret_i = 1; goto done;
  }

  bot_i = 1;

  if ( _test_cmp_head(&nex_u.hed_u, &pac_u->hed_u) ) {
    fprintf(stderr, "pact: head cmp fail\r\n");
    ret_i = 1;
  }

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      if ( _test_cmp_name(&nex_u.pek_u.nam_u, &pac_u->pek_u.nam_u) ) {
        fprintf(stderr, "%%peek name cmp fail\r\n");
        ret_i = 1;
      }
    } break;

    case PACT_PAGE: {
      if ( _test_cmp_name(&nex_u.pag_u.nam_u, &pac_u->pag_u.nam_u) ) {
        fprintf(stderr, "%%page name cmp fail\r\n");
        ret_i = 1;
      }
      else if ( _test_cmp_data(&nex_u.pag_u.dat_u, &pac_u->pag_u.dat_u) ) {
        fprintf(stderr, "%%page data cmp fail\r\n");
        ret_i = 1;
      }
    } break;

    case PACT_POKE: {
      if ( _test_cmp_name(&nex_u.pok_u.nam_u, &pac_u->pok_u.nam_u) ) {
        fprintf(stderr, "%%poke name cmp fail\r\n");
        ret_i = 1;
      }
      else if ( _test_cmp_name(&nex_u.pok_u.pay_u, &pac_u->pok_u.pay_u) ) {
        fprintf(stderr, "%%poke pay-name cmp fail\r\n");
        ret_i = 1;
      }
      else if ( _test_cmp_data(&nex_u.pok_u.dat_u, &pac_u->pok_u.dat_u) ) {
        fprintf(stderr, "%%poke data cmp fail\r\n");
        ret_i = 1;
      }
    } break;

    default: {
      fprintf(stderr, "test: strange packet\r\n");
      ret_i = 1;
    }
  }

done:
  if ( ret_i ) {
    _log_head(&pac_u->hed_u);
    _log_pact(pac_u);
    _log_buf(buf_y, len_w);

    if ( bot_i ) {
      u3l_log(RED_TEXT);
      _log_head(&nex_u.hed_u);
      _log_pact(&nex_u);
      u3l_log(DEF_TEXT);
    }
  }

  c3_free(buf_y);

  return ret_i;
}

static c3_y
_test_rand_bit(void* ptr_v)
{
  return rand() & 1;
}

static c3_y
_test_rand_bits(void* ptr_v, c3_y len_y)
{
  assert( 8 >= len_y );
  return rand() & ((1 << len_y) - 1);
}

static c3_w
_test_rand_word(void* ptr_v)
{
  c3_w low_w = rand();
  c3_w hig_w = rand();
  return (hig_w << 16) ^ (low_w & ((1 << 16) - 1));
}

static c3_y
_test_rand_gulf_y(void* ptr_v, c3_y top_y)
{
  c3_y bit_y = c3_bits_word(top_y);
  c3_y res_y = 0;

  if ( !bit_y ) return res_y;

  while ( 1 ) {
    res_y = _test_rand_bits(ptr_v, bit_y);

    if ( res_y < top_y ) {
      return res_y;
    }
  }
}

static c3_w
_test_rand_gulf_w(void* ptr_v, c3_w top_w)
{
  c3_w bit_w = c3_bits_word(top_w);
  c3_w res_w = 0;

  if ( !bit_w ) return res_w;

  while ( 1 ) {
    res_w  = _test_rand_word(ptr_v);
    res_w &= (1 << bit_w) - 1;

    if ( res_w < top_w ) {
      return res_w;
    }
  }
}

static void
_test_rand_bytes(void* ptr_v, c3_w len_w, c3_y* buf_y)
{
  c3_w max_w = len_w / 2;

  while ( max_w-- ) {
    c3_w wor_w = rand();
    *buf_y++ = (wor_w >> 0) & 0xff;
    *buf_y++ = (wor_w >> 8) & 0xff;
  }

  if ( len_w & 1 ) {
    *buf_y = rand() & 0xff;
  }
}

const char* ta_c = "-~_.0123456789abcdefghijklmnopqrstuvwxyz";

static void
_test_rand_knot(void* ptr_v, c3_w len_w, c3_c* not_c)
{
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    *not_c++ = ta_c[_test_rand_gulf_y(ptr_v, 40)];
  }
}

static void
_test_rand_path(void* ptr_v, c3_s len_s, c3_c* pat_c)
{
  c3_s not_s = 0;

  while ( len_s ) {
    not_s = _test_rand_gulf_w(ptr_v, len_s);
    _test_rand_knot(ptr_v, not_s, pat_c);
    pat_c[not_s] = '/';
    pat_c += not_s + 1;
    len_s -= not_s + 1;
  }
}

static void
_test_make_head(void* ptr_v, u3_xmas_head* hed_u)
{
  hed_u->nex_y = NEXH_NONE; // XX
  hed_u->pro_y = 1;
  hed_u->typ_y = _test_rand_gulf_y(ptr_v, 3) + 1;
  hed_u->hop_y = _test_rand_gulf_y(ptr_v, 8);
  hed_u->mug_w = 0;
  // XX set mug_w in etch?
}

static void
_test_make_name(void* ptr_v, c3_s pat_s, u3_xmas_name* nam_u)
{
  _test_rand_bytes(ptr_v, 16, (c3_y*)nam_u->her_d);
  nam_u->rif_w = _test_rand_word(ptr_v);

  nam_u->pat_s = _test_rand_gulf_w(ptr_v, pat_s);
  nam_u->pat_c = c3_malloc(nam_u->pat_s + 1);
  _test_rand_path(ptr_v, nam_u->pat_s, nam_u->pat_c);
  nam_u->pat_c[nam_u->pat_s] = 0;

  nam_u->boq_y = _test_rand_bits(ptr_v, 8);
  nam_u->nit_o = _test_rand_bits(ptr_v, 1);

  if ( c3y == nam_u->nit_o ) {
    nam_u->aut_o = c3n;
    nam_u->fra_w = 0;
  }
  else {
    nam_u->aut_o = _test_rand_bits(ptr_v, 1);
    nam_u->fra_w = _test_rand_word(ptr_v);
  }
}

static void
_test_make_data(void* ptr_v, u3_xmas_data* dat_u)
{
  dat_u->tot_w = _test_rand_word(ptr_v);

  memset(dat_u->aum_u.sig_y, 0, 64);
  dat_u->aup_u.len_y = 0;
  memset(dat_u->aup_u.has_y, 0, sizeof(dat_u->aup_u.has_y));

  switch ( dat_u->aum_u.typ_e = _test_rand_bits(ptr_v, 2) ) {
    case AUTH_NEXT: {
      dat_u->aup_u.len_y = 2;
    } break;

    case AUTH_SIGN: {
      dat_u->aup_u.len_y = _test_rand_gulf_y(ptr_v, 3);
      _test_rand_bytes(ptr_v, 64, dat_u->aum_u.sig_y);
    } break;

    case AUTH_HMAC: {
      dat_u->aup_u.len_y = _test_rand_gulf_y(ptr_v, 3);
      _test_rand_bytes(ptr_v, 32, dat_u->aum_u.mac_y);
    } break;

    default: break;
  }

  for ( c3_w i_w = 0; i_w < dat_u->aup_u.len_y; i_w++ ) {
    _test_rand_bytes(ptr_v, 32, dat_u->aup_u.has_y[i_w]);
  }

  dat_u->len_w = _test_rand_gulf_w(ptr_v, 1024);
  dat_u->fra_y = c3_calloc(dat_u->len_w);
  _test_rand_bytes(ptr_v, dat_u->len_w, dat_u->fra_y);
}

static void
_test_make_pact(void* ptr_v, u3_xmas_pact* pac_u)
{
  _test_make_head(ptr_v, &pac_u->hed_u);

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      _test_make_name(ptr_v, 277, &pac_u->pek_u.nam_u);
    } break;

    case PACT_PAGE: {
      _test_make_name(ptr_v, 277, &pac_u->pag_u.nam_u);
      _test_make_data(ptr_v, &pac_u->pag_u.dat_u);
    } break;

    case PACT_POKE: {
      _test_make_name(ptr_v, 124, &pac_u->pok_u.nam_u);
      _test_make_name(ptr_v, 124, &pac_u->pok_u.pay_u);
      _test_make_data(ptr_v, &pac_u->pok_u.dat_u);
    } break;

    default: break; // XX
  }
}

static c3_i
_test_rand_pact(c3_w bat_w)
{
  u3_xmas_pact pac_u;
  void*        ptr_v = 0;

  fprintf(stderr, "pact: test roundtrip %u random packets\r\n", bat_w);

  for ( c3_w i_w = 0; i_w < bat_w; i_w++ ) {
    _test_make_pact(ptr_v, &pac_u);

    if ( _test_pact(&pac_u) ) {
      fprintf(stderr, RED_TEXT "pact: roundtrip attempt %u failed\r\n", i_w);
      exit(1);
    }

    // XX free pat_s / buf_y
  }

  return 0;
}

static void
_test_sift_page()
{
  u3_xmas_pact pac_u;
  memset(&pac_u,0, sizeof(u3_xmas_pact));
  pac_u.hed_u.typ_y = PACT_PAGE;
  pac_u.hed_u.pro_y = 1;
  u3l_log("%%page checking sift/etch idempotent");
  u3_xmas_name* nam_u = &pac_u.pag_u.nam_u;

  {
    u3_noun her = u3v_wish("~hastuc-dibtux");
    u3r_chubs(0, 2, nam_u->her_d, her);
    u3z(her);
  }
  nam_u->rif_w = 15;
  nam_u->pat_c = "foo/bar";
  nam_u->pat_s = strlen(nam_u->pat_c);
  nam_u->boq_y = 13;
  nam_u->fra_w = 54;
  nam_u->nit_o = c3n;

  u3_xmas_data* dat_u = &pac_u.pag_u.dat_u;
  dat_u->aum_u.typ_e = AUTH_NONE;
  dat_u->tot_w = 1000;
  dat_u->len_w = 1024;
  dat_u->fra_y = c3_calloc(1024);
  // dat_u->fra_y[1023] = 1;

  if ( _test_pact(&pac_u) ) {
    fprintf(stderr, RED_TEXT "%%page failed\r\n");
    exit(1);
  }
}

static void
_test_sift_peek() 
{
  u3_xmas_pact pac_u;
  memset(&pac_u,0, sizeof(u3_xmas_pact));
  pac_u.hed_u.typ_y = PACT_PEEK;
  pac_u.hed_u.pro_y = 1;
  u3l_log("%%peek checking sift/etch idempotent");
  u3_xmas_name* nam_u = &pac_u.pek_u.nam_u;
  {
    u3_noun her = u3v_wish("~hastuc-dibtux");
    u3r_chubs(0, 2, nam_u->her_d, her);
    u3z(her);
  }
  nam_u->rif_w = 15;
  nam_u->pat_c = "foo/bar";
  nam_u->pat_s = strlen(nam_u->pat_c);
  nam_u->boq_y = 13;
  nam_u->fra_w = 511;
  nam_u->nit_o = c3n;
  nam_u->aut_o = c3n;

  if ( _test_pact(&pac_u) ) {
    fprintf(stderr, RED_TEXT "%%page failed\r\n");
    exit(1);
  }
}

static void
_test_encode_path(c3_c* pat_c)
{
  u3_noun wan = u3do("stab", u3dt("cat", 3, '/', u3i_string(pat_c)));
  u3_noun hav = _xmas_encode_path(strlen(pat_c), (c3_y*)pat_c);

  if ( c3n == u3r_sing(wan, hav) ) {
    u3l_log(RED_TEXT);
    u3l_log("path encoding mismatch");
    u3m_p("want", wan);
    u3m_p("have", hav);
    exit(1);
  }
}


static void
_setup()
{
  c3_d          len_d = u3_Ivory_pill_len;
  c3_y*         byt_y = u3_Ivory_pill;
  u3_cue_xeno*  sil_u;
  u3_weak       pil;

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) {
    printf("*** fail _setup 2\n");
    exit(1);
  }

  {
    c3_w pid_w = getpid();
    srand(pid_w);
    fprintf(stderr, "test: seeding rand() with pid %u\r\n", pid_w);
  }
}

int main()
{
  _setup();
  _test_bitset();

  _test_sift_peek();
  _test_sift_page();

  _test_rand_pact(100000);

  _test_encode_path("foo/bar/baz");
  _test_encode_path("publ/0/xx//1/foo/g");
  return 0;
}

#endif




