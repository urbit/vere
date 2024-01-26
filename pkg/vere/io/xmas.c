/// @file

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"
#include <allocate.h>
#include <defs.h>
#include <error.h>
#include <imprison.h>
#include <jets/q.h>
#include <manage.h>
#include <motes.h>
#include <retrieve.h>
#include <types.h>

#define XMAS_DEBUG     c3y
#define XMAS_VER       1
#define FINE_PAGE      4096             //  packets per page
#define FINE_FRAG      1024             //  bytes per fragment packet
#define FINE_PATH_MAX   384             //  longest allowed scry path
#define HEAD_SIZE         4             //  header size in bytes
#define PACT_SIZE      1472
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"

#define IN_FLIGHT  5

// pending interest sentinels
#define XMAS_ITEM         1  // cached item
#define XMAS_SCRY         2  // waiting on scry

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
#define CHECK_BOUNDS(cur) if ( len_w < cur ) { u3l_log("xmas: failed parse (%i,%i) at line %i", len_w, cur, __LINE__); return 0; }
#define safe_dec(num) num == 0 ? num : num - 1;

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
  ur_cue_test_t*     tes_u;             //  cue-test handle
  u3_cue_xeno*       sil_u;             //  cue handle
  u3p(u3h_root)      her_p;             //
  u3p(u3h_root)      pac_p;             // pending
  c3_w               imp_w[256];
  time_t             imp_t[256];        //  imperial IP timestamps
  c3_o               imp_o[256];        //  imperial print status
  c3_c*              dns_c;             //
  //  jj
  u3p(u3h_root)      req_p;
} u3_xmas;

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
  //  reserved (2 bits)
  c3_y         ran_y; // rank (2 bits)
  c3_y         rif_y; // rift-len (2 bits)
  c3_o         pat_b; // path length length (1bit)
  c3_o         boq_b; // 0b1 if custom, 0b0 implicitly 13 (1bit)
  c3_o         fra_y; // fragment number length (2bit)
} u3_xmas_name_meta;

typedef struct _u3_xmas_name {
  u3_xmas_name_meta  met_u;
  c3_d               her_d[2];
  c3_w               rif_w;
  c3_s               pat_s;
  c3_c*              pat_c;
  c3_y               boq_y;
  c3_w               fra_w;
} u3_xmas_name;

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

typedef struct _u3_xmas_page_meat_meta {
  c3_y             tot_y; // total fragments length (2bit)
  c3_o             aut_b; // authentication len len (1bit)
  c3_y             len_y; // fragment length (5bit)
} u3_xmas_page_meat_meta;

typedef enum _u3_xmas_auth_type {
  AUTH_SIG = 1,
  AUTH_DIG = 2,
  AUTH_HMAC = 3,
} u3_xmas_auth_type;

typedef struct _u3_xmas_page_meat {
  u3_xmas_page_meat_meta  met_u;  // metadata
  c3_w                    tot_w;  // total fragments
  c3_y                    aul_y;  // authentication length
  u3_xmas_auth_type       ayp_y;  // authentication type
  c3_y*                    aut_y; // authentication
  c3_d                    len_d; // fragment length
  c3_y*                   fra_y; // fragment
} u3_xmas_page_meat;

typedef struct _u3_xmas_hop {
  c3_w  len_w;
  c3_y* dat_y;
} u3_xmas_hop;

typedef struct _u3_xmas_hops {
  c3_w len_w;
  u3_xmas_hop** dat_y;
} u3_xmas_hops;

typedef struct _u3_xmas_page_pact {
  u3_xmas_name            nam_u;
  u3_xmas_page_meat       mat_u;
  union {
    c3_y sot_u[6];
    u3_xmas_hop one_u;
    u3_xmas_hops man_u;
  };
} u3_xmas_page_pact;

typedef struct _u3_xmas_poke_pact {
  u3_xmas_peek_pact*     pek_u;
  c3_w                   tot_w;
  c3_y              aut_y[96]; // heap allocate??
  c3_y*             dat_y;
} u3_xmas_poke_pact;

typedef struct _u3_xmas_pact {
  u3_xmas_ptag      typ_y;
  uv_udp_send_t     snd_u;
  struct _u3_xmas*  sam_u;
  u3_xmas_rout      rut_u;
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
_log_buf(c3_y* buf_y, c3_w len_w)
{
  c3_w siz_w = 2*len_w + 1;
  c3_c* res_c = c3_calloc(siz_w);
  c3_w cur_w = 0;
  c3_c tmp_c[3];
  for(c3_w idx_w = 0; idx_w < len_w; idx_w++ ) {
    snprintf(res_c + (2*idx_w), siz_w - (2*idx_w), "%02x", buf_y[idx_w]); 
  }
  u3l_log("buffer: %s", res_c);
  free(res_c);
}

static void
_log_name(u3_xmas_name* nam_u)
{
  u3l_log("meta");
  u3l_log("rank: %u", nam_u->met_u.ran_y);
  u3l_log("rift length: %u", nam_u->met_u.rif_y);
  u3l_log("custom bloq: %c", nam_u->met_u.boq_b == 1 ? 'y' : 'n');
  u3l_log("frag num length: %u", nam_u->met_u.fra_y);
  u3l_log("path length length: %u", nam_u->met_u.pat_b);
  u3l_log("publisher: %s", u3r_string(u3dc("scot", c3__p, u3i_chubs(2, nam_u->her_d))));
  u3l_log("rift: %u", nam_u->rif_w);
  u3l_log("path len: %u", nam_u->pat_s);
  u3l_log("path: %s", nam_u->pat_c);
  u3l_log("bloq: %u", nam_u->boq_y);
  u3l_log("frag: %u", nam_u->fra_w);
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
  u3_xmas_page_meat* mat_u = &pac_u->mat_u;
  u3l_log("meta");
  u3l_log("total length: %u", mat_u->met_u.tot_y);
  u3l_log("auth len length: %u", mat_u->met_u.aut_b);
  u3l_log("frag len len: %u", mat_u->met_u.len_y);
  u3l_log("actual:");
  u3l_log("total: %u", mat_u->tot_w);
  u3l_log("auth len: %u", mat_u->aul_y);
  u3l_log("frag len: %u", mat_u->len_d);
}

static void
_log_pact(u3_xmas_pact* pac_u)
{
  switch ( pac_u->typ_y ) {
    case PACT_PEEK: {
      _log_peek_pact(&pac_u->pek_u);
      break;
    };
    case PACT_PAGE: {
      _log_page_pact(&pac_u->pag_u);
      break;

    }
    default: {
      u3l_log("logging not implemented for %i", pac_u->typ_y);
      break;
    }
  }
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


/* u3_xmas_lane_to_chub(): serialize lane to double-word
*/
static c3_d
u3_xmas_lane_to_chub(u3_lane lan) {
  return ((c3_d)lan.por_s << 32) ^ (c3_d)lan.pip_w;
}

/* u3_xmas_encode_lane(): serialize lane to noun
*/
static u3_atom
u3_xmas_encode_lane(u3_lane lan_u) {
  // [%| p=@]
  // [%& p=@pC]
  return u3nt(c3__if, u3i_word(lan_u.pip_w), lan_u.por_s);
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
// refcounted
//
static u3_noun _xmas_path_with_fra(c3_c* pat_c, c3_s* fra_s)
{
  u3_noun pax = u3do("stab", u3i_string(pat_c));
  u3_noun fra = u3dc("slav", c3__ud, u3do("rear", u3k(pax)));
  u3_noun res = u3do("snip", pax);
  *fra_s = u3r_short(0, fra);

  u3z(fra); // pax is unncessary bc moved in snip
  return res;
}

static u3_noun _xmas_get_now() {
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  u3_noun res = u3_time_in_tv(&tim_u);
  return res;
}


// refcounted
static c3_o
_xmas_rout_pact(u3_xmas_pact* pac_u, u3_noun lan) {
  u3_noun tag, val;
  u3x_cell(lan, &tag, &val);

  u3_assert( (c3y == tag) || (c3n == tag) );
  c3_o suc_o = c3y;
  if ( c3y == tag ) {
    u3_assert( c3y == u3a_is_cat(val) );
    u3_assert( val < 256 );
    pac_u->rut_u.typ_y = ROUT_GALAXY;
    pac_u->rut_u.imp_y = val;
  } else {
    u3_lane lan_u = u3_xmas_decode_lane(u3k(val));
    //  if in local-only mode, don't send remote packets
    //
    if ( (c3n == u3_Host.ops_u.net) && (0x7f000001 != lan_u.pip_w) ) {
      suc_o = c3n;
    }
    //  if the lane is uninterpretable, silently drop the packet
    //
    else if ( 0 == lan_u.por_s ) {
      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("ames: inscrutable lane");
      }
      suc_o = c3n;
    } else {
      pac_u->rut_u.typ_y = ROUT_OTHER;
      pac_u->rut_u.lan_u = lan_u;
    }
  }
  u3z(lan);
  return suc_o;
}

u3_noun _xmas_request_key(u3_xmas_name* nam_u)
{
  u3_noun pax = u3do("stab", u3dt("cat", 3, '/', u3i_string(nam_u->pat_c)));
  u3_noun res = u3nt(u3i_chubs(2, nam_u->her_d), u3i_word(nam_u->rif_w), pax);
  u3m_p("key", res);
  return res;
}

static u3_weak
_xmas_get_request(u3_xmas* sam_u, u3_xmas_name* nam_u) {
  u3_noun key = _xmas_request_key(nam_u);

  u3_weak res = u3h_git(sam_u->req_p, key);
  u3z(key);
  return res;
}

/** RETAIN
 */
static void
_xmas_put_request(u3_xmas* sam_u, u3_xmas_name* nam_u, u3_noun val)
{
  u3_noun key = _xmas_request_key(nam_u);

  u3h_put(sam_u->req_p, key, val);

  u3z(key);
}



// refcounted
//  RETAIN
static c3_y
_ship_meta(u3_noun her)
{
  c3_w met_w = u3r_met(3, her);
  c3_y res_y;
  switch ( met_w ) {
    case 0:
    case 1:
    case 2: {
      res_y = 0;
      break;
    }
    case 3:
    case 4: {
      res_y = 1;
      break;
    }
    case 5:
    case 6:
    case 7:
    case 8: {
      res_y = 2;
      break;
    }
    default: {
      res_y = 3;
      break;
    }
  }
  return res_y;
}


static void _xmas_pact_free(u3_xmas_pact* pac_u) {
  // TODO: i'm lazy
}

static u3_xmas_head
_xmas_head_from_pact(u3_xmas_pact* pac_u, u3_noun her)
{
  u3_xmas_head hed_u;
  hed_u.pro_y = XMAS_VER;
  hed_u.typ_y = pac_u->typ_y;
  // ship meta is retain, no u3k necessary
  //hed_u.ran_y = _ship_meta(her);
  hed_u.hop_y = 0;
  hed_u.mug_w = 0;

  u3z(her);
  return hed_u;
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

  return c3y;

  /*if(c3o((hed_u->typ_y == PACT_PEEK), (hed_u->typ_y == PACT_POKE))) {
    hed_u->ran_y = (hed_w >> 30) & 0x3;
  }*/
}

static c3_w
_xmas_sift_name(u3_xmas_name* nam_u, c3_y* buf_y, c3_w len_w)
{
#ifdef XMAS_DEBUG
  u3l_log("xmas: sifting name %i", len_w);
#endif

  c3_w cur_w = 0;
  CHECK_BOUNDS(cur_w + 1);
  c3_y met_y = buf_y[cur_w];
  nam_u->met_u.ran_y = (met_y >> 0) & 0x3;
  nam_u->met_u.rif_y = (met_y >> 2) & 0x3;
  nam_u->met_u.pat_b = (met_y >> 4) & 0x1;
  nam_u->met_u.boq_b = (met_y >> 5) & 0x1;
  nam_u->met_u.fra_y = (met_y >> 6) & 0x3;
  cur_w += 1;

  c3_y her_y = 2 << nam_u->met_u.ran_y;
  CHECK_BOUNDS(cur_w + her_y)
  _ames_ship_to_chubs(nam_u->her_d, her_y, buf_y + cur_w);
  cur_w += her_y;

  c3_y rif_y = nam_u->met_u.rif_y + 1;
  nam_u->rif_w = 0;
  CHECK_BOUNDS(cur_w + rif_y)
  for( int i = 0; i < rif_y; i++ ) {
    nam_u->rif_w |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }
  
  c3_y pat_y = 1 << nam_u->met_u.pat_b;
  CHECK_BOUNDS(cur_w + pat_y)
  nam_u->pat_s = 0;
  for ( int i = 0; i < pat_y; i++ ) {
    nam_u->pat_s |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  nam_u->pat_c = c3_calloc(nam_u->pat_s + 1); // unix string for ease of manipulation
  CHECK_BOUNDS(nam_u->pat_s + cur_w);
  memcpy(nam_u->pat_c, buf_y + cur_w, nam_u->pat_s);
  nam_u->pat_c[nam_u->pat_s] = 0;
  cur_w += nam_u->pat_s;

  if ( 1 == nam_u->met_u.boq_b ) {
    CHECK_BOUNDS(cur_w + 1);
    nam_u->boq_y = buf_y[cur_w];
    cur_w++;
  } else {
    nam_u->boq_y = 13;
  }

  c3_y fra_y = nam_u->met_u.fra_y + 1;
  u3l_log("fra_y: %i", fra_y);
  CHECK_BOUNDS(cur_w + fra_y)
  nam_u->fra_w = 0;
  for( int i = 0; i < fra_y; i++ ) {
    nam_u->fra_w |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  return cur_w;
}

static c3_w
_xmas_sift_hop_long(u3_xmas_hop* hop_u, c3_y* buf_y, c3_w len_w)
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
_xmas_sift_page_pact(u3_xmas_page_pact* pac_u, u3_xmas_head* hed_u, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0;
  c3_w nam_w = _xmas_sift_name(&pac_u->nam_u, buf_y, len_w);

  if( nam_w == 0 ) {
    return 0;
  }
  cur_w += nam_w;

  u3_xmas_page_meat* mat_u = &pac_u->mat_u;

  CHECK_BOUNDS(cur_w + 1);

  c3_y met_y = buf_y[cur_w];
  mat_u->met_u.tot_y = (met_y >> 0) & 0x3;
  mat_u->met_u.aut_b = (met_y >> 2) & 0x1;
  mat_u->met_u.len_y = (met_y >> 3) & 0x1F;
  cur_w += 1;

  c3_y tot_y = pac_u->mat_u.met_u.tot_y + 1;
  CHECK_BOUNDS(cur_w + tot_y);
  mat_u->tot_w = 0;
  for( int i = 0; i < tot_y; i++ ) {
    mat_u->tot_w |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  mat_u->aul_y = 0;
  if ( 1 == mat_u->met_u.aut_b ) {
    CHECK_BOUNDS(cur_w + 1);
    mat_u->aul_y = buf_y[cur_w];
    cur_w++;
  }

  CHECK_BOUNDS(cur_w + mat_u->aul_y);
  for ( int i = 0; i < mat_u->aul_y; i++ ) {
    mat_u->aut_y = c3_calloc(mat_u->aul_y);
    memcpy(mat_u->aut_y, buf_y + cur_w, mat_u->aul_y);
    cur_w += mat_u->aul_y;
  }

  c3_y len_y = mat_u->met_u.len_y;
  CHECK_BOUNDS(cur_w + len_y);
  u3_assert( len_y <= 8 );
  mat_u->len_d = 0;
  for ( int i = 0; i < len_y; i++ ) {
    mat_u->len_d |= (buf_y[cur_w] << (8*i));
    cur_w++;
  }

  CHECK_BOUNDS(cur_w + len_y);
  mat_u->fra_y = c3_calloc(mat_u->len_d);
  memcpy(mat_u->fra_y, buf_y + cur_w, mat_u->len_d);

  switch ( hed_u->nex_y ) {
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

      pac_u->man_u.dat_y = c3_calloc(sizeof(u3_xmas_hop*) * pac_u->man_u.len_w);

      for( int i = 0; i < pac_u->man_u.len_w; i++ ) {
        pac_u->man_u.dat_y[i] = c3_calloc(sizeof(u3_xmas_hop));
        c3_w hop_w = _xmas_sift_hop_long(pac_u->man_u.dat_y[i], buf_y + cur_w ,len_w - cur_w);
        if ( hop_w == 0 ) {
          return 0;
        }
        cur_w += hop_w;
      }
    }
  }

  return cur_w;

  /*// next hop
  memcpy(pac_u->hop_y, buf_y, 6);
  cur_w += 6;

  // path length
  pac_u->pat_s = _ames_sift_short(buf_y + cur_w);
  cur_w += 2;

  // path contents
  pac_u->pat_c = c3_calloc(pac_u->pat_s + 1);
  memcpy(pac_u->pat_c, buf_y + cur_w, pac_u->pat_s);
  pac_u->pat_c[pac_u->pat_s] = '\0';
  cur_w += pac_u->pat_s + 1;

  // total fragments
  pac_u->tot_w = _ames_sift_word(buf_y + cur_w);
  cur_w += 4;

  // Authenticator
  memcpy(pac_u->aut_y, buf_y + cur_w, 96);
  cur_w += 96;

  c3_w dat_w = 1024; // len_w - cur_w;
  pac_u->dat_y = c3_calloc(dat_w);
  memcpy(pac_u->dat_y, buf_y + cur_w, dat_w);
  */
  return 0;
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

  /*if (siz_w > len_w ) {
      u3l_log("xmas: buffer overrun (FIXME)");
      u3m_bail(c3__foul);
  }*/
  return siz_w;
}

static c3_w
_xmas_sift_poke_pact(u3_xmas_poke_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0;
  // Peek portion
  pac_u->pek_u = c3_calloc(sizeof(*(pac_u->pek_u)));
  cur_w += _xmas_sift_peek_pact(pac_u->pek_u, buf_y, len_w);

  // Total fragments
  pac_u->tot_w = _ames_sift_word(buf_y + cur_w);
  cur_w += 4;

  // Authenticator
  memcpy(pac_u->aut_y, buf_y + cur_w, 96);
  cur_w += 96;

  // Datum
  memcpy(pac_u->dat_y, buf_y + cur_w, len_w - cur_w);
  return cur_w;
}

static c3_w
_xmas_sift_pact(u3_xmas_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  u3_xmas_head hed_u;
  if( len_w < 8 ) {
    u3l_log("xmas: attempted to parse overly short packet of size %u", len_w);
  }

  _xmas_sift_head(buf_y, &hed_u);
  pac_u->typ_y = hed_u.typ_y;
  c3_w res_w = 0;
  buf_y += 8;
  len_w -= 8;
  switch ( pac_u->typ_y ) {
    case PACT_PEEK: {
      res_w = _xmas_sift_peek_pact(&pac_u->pek_u, buf_y, len_w);
    } break;
    case PACT_PAGE: {
      res_w = _xmas_sift_page_pact(&pac_u->pag_u, &hed_u, buf_y, len_w);
    } break;
    case PACT_POKE: {
      res_w = _xmas_sift_poke_pact(&pac_u->pok_u, buf_y, len_w);
    } break;
    default: {
      u3l_log("xmas: received unknown packet type");
      break;
    }
  }
  //u3_assert(res_w <= len_w );
  return res_w;
}



static void
_xmas_etch_head(u3_xmas_head* hed_u, c3_y buf_y[8])
{
  if( c3y == XMAS_DEBUG ) {
    if( hed_u->pro_y > 7 ) {
      u3l_log("xmas: bad protocol version");
      return;
    }
  }
  c3_o req_o = c3o((hed_u->typ_y == PACT_PEEK), (hed_u->typ_y == PACT_POKE));
  c3_y siz_y = req_o ? 5 : 7;
  c3_w hed_w = (hed_u->nex_y & 0x3 ) << 2
             ^ (hed_u->pro_y  & 0x7 ) << 4
             ^ ((hed_u->typ_y & 0x3     ) << 7)
             ^ ((hed_u->hop_y & 0x7 ) << 9)
             ^ ((hed_u->mug_w & 0xFFFFFF ) << 12);
             // XX: we don't expand hopcount if no request. Correct?
      //
  /*if ( c3y == req_o ) {
    hed_w = hed_w ^ ((hed_u->ran_y & 0x3) << 30);
  }*/

  _ames_etch_word(buf_y, hed_w);
  memcpy(buf_y + 4, XMAS_COOKIE, XMAS_COOKIE_LEN);
}

static c3_w
_xmas_etch_page_pact(c3_y* buf_y, u3_xmas_page_pact* pac_u, u3_xmas_head* hed_u)
{
  /*c3_w cur_w = 0;
 
  // hops
  memcpy(buf_y, pac_u->hop_y, 6);
  cur_w += 6;

  // path length
  _ames_etch_short(buf_y + cur_w, pac_u->pat_s);
  cur_w += 2;

  // path
  memcpy(buf_y + cur_w, pac_u->pat_c, pac_u->pat_s + 1);
  cur_w += pac_u->pat_s + 1;

  //  total
  _ames_etch_word(buf_y + cur_w, pac_u->tot_w);
  cur_w += 4;

  // auth
  memcpy(buf_y + cur_w, &pac_u->aut_y, 96);
  cur_w += 96;

  // dat
  memcpy(buf_y + cur_w, pac_u->dat_y, 1024);
  cur_w += 1024;

  return cur_w;*/
  return 0;
}

static c3_w
_xmas_etch_peek_pact(c3_y* buf_y, u3_xmas_peek_pact* pac_u, u3_xmas_head* hed_u, c3_o tag_o)
{
#ifdef XMAS_DEBUG
  if ( strlen(pac_u->nam_u.pat_c) != pac_u->nam_u.pat_s ) {
    u3l_log(
      "xmas: packet validation failure, with path %s expected length %i, got %i", 
      pac_u->nam_u.pat_c,
      pac_u->nam_u.pat_s,
      strlen(pac_u->nam_u.pat_c)
    );
    u3m_bail(c3__foul);
  }

#endif 
  c3_w cur_w = 0;
  u3_xmas_name* nam_u = &pac_u->nam_u;
  nam_u->met_u.ran_y = safe_dec(u3r_met(0, u3r_met(4, u3i_chubs(2, nam_u->her_d))));
  nam_u->met_u.rif_y = safe_dec(u3r_met(3, u3i_word(nam_u->rif_w)));
  nam_u->met_u.fra_y = safe_dec(u3r_met(3, u3i_word(nam_u->fra_w)));
  nam_u->met_u.boq_b = nam_u->boq_y == 13 ? 0 : 1;
  nam_u->met_u.pat_b = nam_u->pat_s > 0xFF ? 1 : 0;
  
  // TODO: double check reserved bits
  c3_y met_y = (nam_u->met_u.ran_y & 0x3) >> 0
             ^ (nam_u->met_u.rif_y & 0x3) >> 2
             ^ (nam_u->met_u.pat_b & 0x1) >> 4
             ^ (nam_u->met_u.boq_b & 0x1) >> 5
             ^ (nam_u->met_u.fra_y & 0x3) >> 6;

  buf_y[cur_w] = met_y;


  u3_xmas_name_meta met_u = nam_u->met_u;
  //ship
  cur_w++;
  c3_y her_y = 1 << (met_u.ran_y + 1);
  _ames_ship_of_chubs(nam_u->her_d, her_y, buf_y + cur_w);
  cur_w += her_y;

  // rift
  c3_y rif_y = met_u.rif_y + 1;
  for ( int i = 0; i < rif_y; i++) {
    buf_y[cur_w] = (nam_u->rif_w >> (8*i)) & 0xff;
    cur_w++;
  }

  // path length
  c3_y pat_y = 1 >> met_u.pat_b;
  for ( int i = 0; i < pat_y; i++ ) {
    buf_y[cur_w] = (nam_u->pat_s >> (8*i)) & 0xff;
    cur_w++;
  }

  // path
  memcpy(buf_y + cur_w, nam_u->pat_c, nam_u->pat_s);
  cur_w += nam_u->pat_s;


  // possible bloq size
  if ( 1 == met_u.boq_b ) {
    buf_y[cur_w] = nam_u->boq_y;
    cur_w++;
  }

  c3_y fra_y = met_u.fra_y + 1;
  for( int i = 0; i < fra_y; i++ ) {
    buf_y[cur_w] = (nam_u->fra_w >> (8*i)) & 0xff;
    cur_w++;
  }

  return cur_w;
  //_ames_etch_short(buf_y + cur_w, &met_s);
}

static c3_w
_xmas_etch_poke_pact(c3_y* buf_y, u3_xmas_poke_pact* pac_u, u3_xmas_head* hed_u)
{
  c3_w cur_w = 0;
  cur_w += _xmas_etch_peek_pact(buf_y, pac_u->pek_u, hed_u, c3n);

  //  total
  _ames_etch_word(buf_y + cur_w, pac_u->tot_w);
  cur_w += 4;

  // auth
  memcpy(buf_y + cur_w, pac_u->aut_y, 96);
  cur_w += 96;

  // day
  memcpy(buf_y + cur_w, pac_u->dat_y, PACT_SIZE - cur_w);

  return PACT_SIZE;
}

static c3_w
_xmas_etch_pact(c3_y* buf_y, u3_xmas_pact* pac_u, u3_noun her)
{
  c3_w cur_w = 0;
  u3_xmas_head hed_u = _xmas_head_from_pact(pac_u, her);
  _xmas_etch_head(&hed_u, buf_y + cur_w);
  cur_w += 8;


  switch ( pac_u->typ_y ) {
    case PACT_POKE: {
      cur_w += _xmas_etch_poke_pact(buf_y + cur_w, &pac_u->pok_u, &hed_u);
    } break;
    case PACT_PEEK: {
      cur_w += _xmas_etch_peek_pact(buf_y + cur_w, &pac_u->pek_u, &hed_u, c3y);
    } break;
    case PACT_PAGE: {
      cur_w += _xmas_etch_page_pact(buf_y + cur_w, &pac_u->pag_u, &hed_u);
    } break;

    default: {
      u3l_log("bad pact type");//u3m_bail(c3__bail);
    }
  }
  return cur_w;
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

static void _xmas_send(u3_xmas_pact* pac_u)
{
  u3_xmas* sam_u = pac_u->sam_u;
  u3l_log("xmas sending");


  //u3l_log("_ames_send %s %u", _str_typ(pac_u->typ_y),
  //                              pac_u->rut_u.lan_u.por_s);
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w siz_w;
  {
    u3_noun who = 0;
    
    siz_w = _xmas_etch_pact(buf_y, pac_u, who);
  }

  _xmas_send_buf(sam_u, pac_u->rut_u.lan_u, buf_y, siz_w);
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
  u3l_log("routing bufs");
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
      _xmas_send_buf(sam_u, lan_u, buf_y, len_w);
    }
  }
  u3z(las);
  return suc_o;
}

static c3_o
_xmas_rout_pacs(u3_xmas_pact* pac_u, u3_noun las)
{
  c3_o suc_o = c3n;
  u3_noun lan, t = las;
  while ( t != u3_nul ) {
    u3x_cell(t, &lan, &t);
    // if ( c3n == u3r_cell(t, &lan, &t) ) {
    //   break;
    // }
    if ( c3n == _xmas_rout_pact(pac_u, u3k(lan)) ) {
      u3l_log("xmas: failed to set route");
    } else {
       u3l_log("xmas_send");
      //_xmas_send_buf(pac_u);
      suc_o = c3y;
    }
  }
  u3z(las);
  return suc_o;
}

static void
_xmas_update_req_peek(u3_xmas_pact* pac_u) {
  u3_weak req = _xmas_get_request(pac_u->sam_u, &pac_u->pek_u.nam_u);

  if ( u3_none == req ) {
    u3_assert( 0 == pac_u->pek_u.nam_u.fra_w ); // TODO restart request
    _xmas_put_request(pac_u->sam_u, &pac_u->pek_u.nam_u, u3nq(0, u3_nul, u3_nul, u3nc(0, u3_nul)));
  } else {
    u3_noun tot, wat, mis, nex, dat;
    u3x_quil(req, &tot, &wat, &mis, &nex, &dat);
    u3_noun fra = u3i_word(pac_u->pek_u.nam_u.fra_w);
    if ( c3y == u3r_sing(fra, nex) ) {
      nex = fra;
    }

    u3_noun nat = u3kdi_put(u3k(wat), u3k(fra));

    u3_noun new = u3nq(u3k(tot), nat, u3k(mis), u3nc(u3k(nex), u3k(dat)));
    _xmas_put_request(pac_u->sam_u, &pac_u->pek_u.nam_u, new);
    //u3z(new); u3z(req);
  }
}

static void
_xmas_ef_send(u3_xmas* sam_u, u3_noun las, u3_noun pac)
{
  u3l_log("sending");
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

  if ( pac_u.typ_y == PACT_PEEK ) {
    _xmas_update_req_peek(&pac_u);
  }



  c3_o suc_o = c3n;
  u3_noun lan, t = las;
  _xmas_rout_bufs(sam_u, buf_y, len, las);
  u3z(pac);
}

static void
_xmas_ef_peek(u3_xmas* sam_u, u3_noun las, u3_noun who, u3_noun pat, u3_noun cop) 
{
  u3_noun boq, fra, her, rif;
  u3x_cell(who, &her, &rif);
  u3x_cell(cop, &boq, &fra);
  
  if ( c3n == sam_u->car_u.liv_o ) {
    u3l_log("xmas: not yet live, dropping outbound\r");
    u3z(pat);
  } else {
    u3_xmas_pact* pac_u = c3_calloc(sizeof(*pac_u));
    pac_u->typ_y = PACT_PEEK;
    pac_u->sam_u = sam_u;
    {
      u3_noun pas = u3do("spat", pat);
      pac_u->pek_u.nam_u.pat_c = u3r_string(pas);
      pac_u->pek_u.nam_u.pat_s = strlen(pac_u->pek_u.nam_u.pat_c);
      u3z(pas);
    }
    u3r_chubs(0, 2, pac_u->pek_u.nam_u.her_d, her);
    pac_u->pek_u.nam_u.rif_w = u3r_word(0, rif);
    pac_u->pek_u.nam_u.boq_y = u3r_byte(0, boq);

    if( 384 < pac_u->pek_u.nam_u.pat_s ) {
      u3l_log("xmas: path in peek too long");
      _xmas_pact_free(pac_u);
    } else {
      c3_o rut_o = _xmas_rout_pacs(pac_u, u3k(las));

      if ( c3n == rut_o ) {
        _xmas_pact_free(pac_u);
      }
    }
  }
  // do not need to lose pat because moved in spat call
  u3z(who); u3z(cop);
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

/*
 * RETAIN
 */
static u3_weak
_xmas_get_peer(u3_xmas* sam_u, u3_noun her)
{
  return u3h_git(sam_u->her_p, her);
}

/*
 * RETAIN
 */
static void
_xmas_put_sponsee(u3_xmas* sam_u, u3_noun her, u3_noun las)
{
  u3_noun val = u3nc(u3_nul, u3k(las));
  u3h_put(sam_u->her_p, her, val);
  u3z(val);
}

/*
 * RETAIN
 */
static c3_o
_xmas_add_galaxy_pend(u3_xmas* sam_u, u3_noun her, u3_noun pen)
{
  u3_weak old = u3h_git(sam_u->her_p, her);
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
  u3_noun rif = u3dc("scot", c3__ud, nam_u->rif_w);
  u3_noun boq = u3dc("scot", c3__ud, nam_u->boq_y);
  u3_noun fag = u3dc("scot", c3__ud, nam_u->fra_w);
  u3_noun pax = u3do("stab", u3dt("cat", 3, '/', u3i_string(nam_u->pat_c)));

  return u3dc("weld", u3nc(c3__pact, u3nq(rif, boq, fag, u3nc(c3__data, u3_nul))), pax);
}

/*
 * RETAIN
 */
static u3_weak
_xmas_get_cache(u3_xmas* sam_u, u3_xmas_name* nam_u)
{
  u3_noun pax = _name_to_scry(nam_u);
  u3_weak res = u3h_git(sam_u->pac_p, pax);
  if ( u3_none == res ) {
    u3m_p("miss", u3k(pax));
  } else { 
    u3m_p("hit", u3nc(u3k(pax), u3k(res)));
  }
  return res;
}

static void
_xmas_put_cache(u3_xmas* sam_u, u3_xmas_name* nam_u, u3_noun val)
{
  u3_noun pax = _name_to_scry(nam_u);
  u3m_p("new key", u3k(pax));
  u3m_p("new value", u3k(val));

  u3h_put(sam_u->pac_p, pax, val);
  u3z(pax); // TODO: fix refcount
}

/* _xmas_czar(): add packet to queue, possibly begin DNS resolution
 */
static void
_xmas_czar(u3_xmas_pact* pac_u)
{
  u3_xmas* sam_u = pac_u->sam_u;
#ifdef XMAS_DEBUG
  if ( pac_u->typ_y != PACT_PEEK ) {
    u3l_log("xmas: attempted to resolve galaxy for packet that was not peek");
    u3m_bail(c3__oops);
    return;
  }
#endif

  u3_noun pat = u3i_string(pac_u->pek_u.nam_u.pat_c);

  c3_y her_y = pac_u->rut_u.imp_y;

  u3_weak her = _xmas_get_peer(sam_u, her_y);

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
      if ( XMAS_SCRY == tag ) {
        c3_y* buf_y;
        
        u3m_p("hit", hit);
        u3m_p("dat", dat);
        c3_w len_w = _xmas_respond(pac_u, &buf_y, u3k(hit));
        _xmas_rout_bufs(sam_u, buf_y, len_w, u3k(u3t(dat)));
      }
      _xmas_put_cache(sam_u, &pac_u->pek_u.nam_u, u3nc(XMAS_ITEM, u3k(hit)));
      u3z(old);
    }
    u3z(hit);
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
_xmas_hear_page(u3_xmas_pact* pac_u, c3_y* buf_y, c3_w len_w, u3_lane lan_u)
{
  u3l_log("xmas hear page");
  u3_xmas* sam_u = pac_u->sam_u;
  u3_noun wir = u3nc(c3__xmas, u3_nul);
  c3_s fra_s;
  _log_pact(pac_u);
  
  u3_noun fra = u3i_bytes(pac_u->pag_u.mat_u.len_d, pac_u->pag_u.mat_u.fra_y) ;


  u3_weak req = _xmas_get_request(sam_u, &pac_u->pag_u.nam_u);

  if ( u3_none == req ) {
    u3l_log("xmas: heard page w/ no request, dropping"); // TODO
    return;
  }

  u3_noun tot, wat, mis, nex, dat;
  u3x_quil(req, &tot, &wat, &mis, &nex, &dat);
  if ( tot == 0 ) {
    tot = u3i_word(pac_u->pag_u.mat_u.tot_w);
  }
  c3_w nex_w = u3r_word(0, nex);
  u3_noun num = u3i_word(pac_u->pag_u.nam_u.fra_w);
  u3_noun wut = u3qdi_del(wat, u3k(num));
  c3_w lat_w = u3r_word(0, u3qdi_wyt(wut));

  u3_noun dit = u3qdb_put(dat, u3k(num), fra);
  u3m_p("dit wyt", u3qdb_wyt(dit));


  if ( c3y == u3r_sing(tot, u3qdb_wyt(dit)) ) {
    u3l_log("finished");
    return;
  } else if ( lat_w < IN_FLIGHT ) {
    c3_w sen_w = IN_FLIGHT - lat_w;
    u3l_log("send %u", sen_w);
    for(int i = 0; i < sen_w; i++) {
      u3l_log("sending req fra %u", nex_w + i);
      u3_xmas_pact* nex_u = c3_calloc(sizeof(u3_xmas_pact));
      nex_u->sam_u = sam_u;
      nex_u->typ_y = PACT_PEEK;
      memcpy(&nex_u->pek_u.nam_u, &pac_u->pag_u.nam_u, sizeof(u3_xmas_name));
      nex_u->pek_u.nam_u.fra_w = nex_w + i;
      c3_y* buf_y = c3_calloc(PACT_SIZE);
      c3_w siz_w  =_xmas_etch_pact(buf_y, nex_u, 0);
      if( siz_w == 0 ) {
        u3l_log("failed to etch");
        u3_assert( 0 );

      }
      // TODO: better route management
      _xmas_send_buf(sam_u, lan_u, buf_y, siz_w);
      wut = u3qdi_put(wut, u3i_word(nex_u->pek_u.nam_u.fra_w));
      nex = u3dc("add", 1, nex);
    }
  }
  _xmas_put_request(sam_u, &pac_u->pag_u.nam_u, u3nq(u3k(tot), u3k(wut), u3k(mis), u3nc(u3k(nex), u3k(dit))));
  //u3z(req);

}

static void
_xmas_add_lane_to_cache(u3_xmas* sam_u, u3_xmas_name* nam_u, u3_noun las, u3_lane lan_u)
{
  u3_noun hit = u3nq(XMAS_SCRY,
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
  u3l_log("%hu", lan_u.por_s);
  // u3_assert(pac_u->typ_y == PACT_PEEK);
#endif

  u3_xmas* sam_u = pac_u->sam_u;
  u3_weak hit = _xmas_get_cache(sam_u, &pac_u->pek_u.nam_u);
  if ( u3_none != hit ) {
    u3_noun tag, dat;
    u3x_cell(u3k(hit), &tag, &dat);
    u3m_p("dat", dat);
    if ( tag == XMAS_SCRY ) {
      _xmas_add_lane_to_cache(sam_u, &pac_u->pek_u.nam_u, u3t(dat), lan_u);
    } else if ( tag == XMAS_ITEM ) {
      u3l_log("item hit");
      u3l_log("after dat");
      c3_y* buf_y;

      c3_w len_w = _xmas_respond(pac_u, &buf_y, u3k(dat));
      _xmas_send_buf(pac_u->sam_u, lan_u, buf_y, len_w);
    } else {
      u3l_log("xmas: weird case in cache, dropping");
    }
    u3z(hit);
  } else {
    u3l_log("adding lane to cache");
    _xmas_add_lane_to_cache(sam_u, &pac_u->pek_u.nam_u, u3_nul, lan_u); // TODO: retrieve from namespace
    u3_noun sky = _name_to_scry(&pac_u->pek_u.nam_u); 

    u3_noun our = u3i_chubs(2, sam_u->car_u.pir_u->who_d);
    u3_noun bem = u3nc(u3nt(our, u3_nul, u3nc(c3__ud, 1)), sky);
    u3_pier_peek(sam_u->car_u.pir_u, u3_nul, u3k(u3nq(1, c3__beam, c3__xx, bem)), pac_u, _xmas_page_scry_cb);
    // u3_pier_peek_last(sam_u->car_u.pir_u, u3_nul, c3__xx, u3_nul, sky, pac_u, _xmas_page_scry_cb);
  }
  u3l_log("xmas: finished hear peek");
  // u3z(pax);
}

static void
_xmas_hear(u3_xmas* sam_u,
           u3_lane* lan_u,
           c3_w     len_w,
           c3_y*    hun_y)
{

  u3l_log("xmas_hear");

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
  if( lin_w == 0 ) {
#ifdef XMAS_DEBUG
    u3l_log("xmas: failed to parse packet");
    _log_pact(pac_u);
#endif
    // TODO free everything
    return;


  }
#ifdef XMAS_DEBUG
  _log_peek_pact(&pac_u->pek_u);
  u3l_log("xmas: sifted packet");
#endif 

  switch ( pac_u->typ_y ) {
    case PACT_PEEK: {
      _xmas_hear_peek(pac_u, *lan_u);
    } break;
    case PACT_PAGE: {
      _xmas_hear_page(pac_u, hun_y, len_w, *lan_u);
    } break;
    default: {
      u3l_log("xmas: unimplemented packet type %u", pac_u->typ_y);
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

    u3l_log("test");

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

  u3_assert( !uv_udp_init(u3L, &sam_u->wax_u) );
  sam_u->wax_u.data = sam_u;

  sam_u->sil_u = u3s_cue_xeno_init();
  sam_u->tes_u = ur_cue_test_init();

  //  Disable networking for fake ships
  //
  if ( c3y == sam_u->pir_u->fak_o ) {
    u3_Host.ops_u.net = c3n;
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
_test_sift_peek() 
{
  u3_xmas_pact pac_u;
  memset(&pac_u,0, sizeof(u3_xmas_pact));
  pac_u.typ_y = PACT_PEEK;;
  u3l_log("checking sift/etch idempotent");
  u3_xmas_name* nam_u = &pac_u.pek_u.nam_u;
  u3_noun her = u3v_wish("~hastuc-dibtux");
  u3r_chubs(0, 2, nam_u->her_d, her);
  nam_u->rif_w = 15;
  nam_u->pat_c = "foo/bar";
  nam_u->pat_s = strlen(nam_u->pat_c);
  nam_u->boq_y = 13;
  nam_u->fra_w = 54;

  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w len_w =_xmas_etch_pact(buf_y, &pac_u, her);
  u3_xmas_pact nex_u;
  memset(&nex_u, 0, sizeof(u3_xmas_pact));
  _xmas_sift_pact(&nex_u, buf_y, len_w);

  if ( 0 != memcmp(nex_u.pek_u.nam_u.pat_c, pac_u.pek_u.nam_u.pat_c, pac_u.pek_u.nam_u.pat_s) ) {
    u3l_log(RED_TEXT);
    u3l_log("path mismatch");
    u3l_log("got");
    _log_pact(&nex_u);
    u3l_log("expected");
    _log_pact(&pac_u);
    exit(1);


  }
  nex_u.pek_u.nam_u.pat_c = 0;
  pac_u.pek_u.nam_u.pat_c = 0;
  
  if ( 0 != memcmp(&nex_u, &pac_u, sizeof(u3_xmas_pact)) ) {
    u3l_log(RED_TEXT);
    u3l_log("mismatch");
    u3l_log("got");
    _log_pact(&nex_u);
    u3l_log("expected");
    _log_pact(&pac_u);
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
}

int main()
{

  _setup();

  _test_sift_peek();
  return 0;
}

#endif




