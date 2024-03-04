#ifndef VERE_XMAS_H
#define VERE_XMAS_H

#include "c3.h"

#define XMAS_VER       1
#define FINE_PAGE      4096             //  packets per page
#define FINE_FRAG      1024             //  bytes per fragment packet
#define FINE_PATH_MAX   384             //  longest allowed scry path
#define HEAD_SIZE         4             //  header size in bytes
#define PACT_SIZE      1472

#define HOP_NONE 0b0
#define HOP_SHORT 0b1
#define HOP_LONG 0b10
#define HOP_MANY 0b11

#define XMAS_COOKIE_LEN   4
static c3_y XMAS_COOKIE[4] = { 0x5e, 0x1d, 0xad, 0x51 };


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
  union {
    u3_xmas_poke_pact  pok_u;
    u3_xmas_page_pact  pag_u;
    u3_xmas_peek_pact  pek_u;
  };
} u3_xmas_pact;

c3_w xmas_sift_pact(u3_xmas_pact* pac_u, c3_y* buf_y, c3_w len_w);
c3_w xmas_etch_pact(c3_y* buf_y, u3_xmas_pact* pac_u);

void xmas_free_pact(u3_xmas_pact* pac_u);

void log_pact(u3_xmas_pact* pac_u);

#endif