#ifndef VERE_MESA_H
#define VERE_MESA_H

#include "c3/c3.h"
#include "ship.h"

#define MESA_VER       1
#define FINE_PAGE      4096             //  packets per page
#define FINE_FRAG      1024             //  bytes per fragment packet
#define FINE_PATH_MAX   384             //  longest allowed scry path
#define HEAD_SIZE         4             //  header size in bytes
#define PACT_SIZE      1472

static c3_w MESA_COOKIE = 0x67e00200;

typedef enum _u3_mesa_ptag {
  PACT_RESV = 0,
  PACT_PAGE = 1,
  PACT_PEEK = 2,
  PACT_POKE = 3,
} u3_mesa_ptag;

typedef enum _u3_mesa_rout_tag {
  ROUT_GALAXY = 0,
  ROUT_OTHER = 1
} u3_mesa_rout_tag;

typedef enum _u3_mesa_hop_type {
  HOP_NONE  = 0,
  HOP_SHORT = 1,
  HOP_LONG  = 2,
  HOP_MANY  = 3
} u3_mesa_hop_type;

typedef struct _u3_str {
  c3_c* str_c;
  c3_w  len_w;
} u3_str;

typedef struct _u3_mesa_name_meta {
  c3_y         ran_y;  // rank (2 bits)
  c3_y         rif_y;  // rift-len (2 bits)
  c3_y         nit_y;  // initial overlay (1 bit)
  c3_y         tau_y;  // %data (0) or %auth (1), 0 if !nit_o (1 bit)
  c3_y         gaf_y;  // fragment number length (2bit)
} u3_mesa_name_meta;

typedef struct _u3_mesa_name {
  // u3_mesa_name_meta  met_u;
  u3_ship            her_u;
  c3_w               rif_w;
  c3_y               boq_y;
  c3_o               nit_o;
  c3_o               aut_o;
  c3_d               fra_d;
  c3_s               pat_s;
  c3_c*              pat_c;
  u3_str             str_u;
} u3_mesa_name;

typedef struct _u3_mesa_data_meta {
  c3_y         bot_y;  // total-fragments len (2 bits)
  c3_o         aut_o;  // auth tag (c3y for message, c3n for pair)
  c3_o         auv_o;  // auth value (c3y for sig/no-pair, c3n for hmac/pair)
  c3_y         men_y;  // fragment length/type (2 bits)
} u3_mesa_data_meta;

typedef enum  {
  AUTH_SIGN = 0,
  AUTH_NONE = 1,
  AUTH_HMAC = 2,
  AUTH_PAIR = 3,
} u3_mesa_auth_type;

typedef struct _u3_auth_data {
  u3_mesa_auth_type typ_e;     // none, traversal (none), sig, or hmac
  union {                      //
    c3_y        sig_y[64];     // signature
    c3_y        mac_y[16];     // hmac
    c3_y        has_y[2][32];  // hashes
  };
} u3_auth_data;

typedef struct _u3_mesa_data {
  c3_d                tob_d;  // total bytes in message
  u3_auth_data        aut_u;  // authentication
  c3_w                len_w;  // fragment length
  c3_y*               fra_y;  // fragment
} u3_mesa_data;


typedef struct _u3_mesa_head {
  u3_mesa_hop_type nex_y;  // next-hop
  c3_y             pro_y;  // protocol version
  u3_mesa_ptag     typ_y;  // packet type
  c3_y             hop_y;  // hopcount
  c3_w             mug_w;  // truncated mug checksum
} u3_mesa_head;

//
// +$  cache
//   [%rout lanes=(list lanes)]
//   [%pending pacs=(list pact)]


typedef struct _u3_mesa_peek_pact {
  u3_mesa_name     nam_u;
} u3_mesa_peek_pact;

typedef struct _u3_mesa_hop_once {
  c3_w  len_w;
  c3_y* dat_y;
} u3_mesa_hop_once;

typedef struct _u3_mesa_hop_more {
  c3_w len_w;
  u3_mesa_hop_once* dat_y;
} u3_mesa_hop_more;

typedef union {
  c3_y sot_u[6];
  u3_mesa_hop_once one_u;
  u3_mesa_hop_more man_u;
} u3_mesa_hop;

typedef struct _u3_mesa_page_pact {
  u3_mesa_name            nam_u;
  u3_mesa_data            dat_u;
  union {
    c3_y sot_u[6];
    u3_mesa_hop_once one_u;
    u3_mesa_hop_more man_u;
  };
} u3_mesa_page_pact;

typedef struct _u3_mesa_poke_pact {
  u3_mesa_name            nam_u;
  u3_mesa_name            pay_u;
  u3_mesa_data            dat_u;
} u3_mesa_poke_pact;

typedef struct _u3_mesa_pact {
  u3_mesa_head      hed_u;
  union {
    u3_mesa_poke_pact  pok_u;
    u3_mesa_page_pact  pag_u;
    u3_mesa_peek_pact  pek_u;
  };
} u3_mesa_pact;

typedef struct _u3_etcher {
  c3_y* buf_y;
  c3_w  len_w;
  c3_w  cap_w;
  c3_d  bit_d; // for _etch_bits
  c3_y  off_y; // for _etch_bits
} u3_etcher;

c3_d mesa_num_leaves(c3_d tot_d);
c3_w mesa_size_pact(u3_mesa_pact* pac_u);
c3_o mesa_is_new_pact(c3_y* buf_y, c3_w len_w);

void mesa_free_pact(u3_mesa_pact* pac_u);

c3_w mesa_etch_pact_to_buf(c3_y* buf_y, c3_w cap_w, u3_mesa_pact *pac_u);
void etcher_init(u3_etcher* ech_u, c3_y* buf_y, c3_w cap_w);
void _mesa_etch_name(u3_etcher *ech_u, u3_mesa_name* nam_u);
c3_c* mesa_sift_pact_from_buf(u3_mesa_pact *pac_u, c3_y* buf_y, c3_w len_w);

void inc_hopcount(u3_mesa_head*);

void log_pact(u3_mesa_pact* pac_u);
void log_name(u3_mesa_name* nam_u);

#endif
