#include "mesa.h"
#include <log.h>
#include <manage.h>
#include <stdio.h>
// only need for tests, can remove
#include "vere.h"
#include "ivory.h"
#include "ur/ur.h"
#include "ship.h"
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
// endif tests

#define safe_dec(num) (num == 0 ? num : num - 1)
#define _mesa_met3_w(a_w) ((c3_bits_word(a_w) + 0x7) >> 3)

// assertions for roundtrip tests
/* #define MESA_ROUNDTRIP c3y */

#define _assert_eq_f(a, b)                                          \
  if ( a != b ) {                                                   \
    u3l_log("mesa.c:%u  %s != %s", __LINE__,                        \
            __(a)? "&" : "|",                                       \
            __(b)? "&" : "|");                                      \
    u3m_bail(c3__oops);                                             \
  }
#define _assert_eq_udF(a, b)                                        \
  if ( a != b ) {                                                   \
    u3l_log("mesa.c:%u  %u != %u", __LINE__, a, b);                 \
    u3m_bail(c3__oops);                                             \
  }

#define _assert_eq_udG(a, b)                                        \
  if ( a != b ) {                                                   \
    u3l_log("mesa.c:%u  %"PRIu64" != %"PRIu64, __LINE__, a, b);     \
    u3m_bail(c3__oops);                                             \
  }

#define _assert_eq_uxF(a, b)                                        \
  if ( a != b ) {                                                   \
    u3l_log("mesa.c: %u  0x%08x != 0x%08x", __LINE__, a, b);        \
    u3m_bail(c3__oops);                                             \
  }

#define _assert_eq_uxG(a, b)                                        \
  if ( a != b ) {                                                   \
    u3l_log("mesa.c: %u  0x%016llx != 0x%016llx", __LINE__, a, b);  \
    u3m_bail(c3__oops);                                             \
  }

static void
_mesa_check_heads_equal(u3_mesa_head* hed_u, u3_mesa_head* hod_u)
{
  _assert_eq_udF(hed_u->hop_y, hed_u->hop_y);
  _assert_eq_uxF(hed_u->mug_w, hed_u->mug_w);
  _assert_eq_udF(hed_u->nex_y, hed_u->nex_y);
  _assert_eq_udF(hed_u->pro_y, hed_u->pro_y);
  _assert_eq_udF(hed_u->typ_y, hed_u->typ_y);
}

static void
_mesa_check_names_equal(u3_mesa_name* nam_u, u3_mesa_name* nom_u)
{
  u3_assert( __(u3_ships_equal(nam_u->her_u, nom_u->her_u)) );
  _assert_eq_udF(nam_u->rif_w, nom_u->rif_w);
  _assert_eq_udF(nam_u->boq_y, nom_u->boq_y);
  _assert_eq_f(nam_u->nit_o, nom_u->nit_o);
  _assert_eq_f(nam_u->aut_o, nom_u->aut_o);
  _assert_eq_udG(nam_u->fra_d, nom_u->fra_d);
  _assert_eq_udF(nam_u->pat_s, nom_u->pat_s);
  u3_assert( 0 == memcmp(nam_u->pat_c, nom_u->pat_c, nam_u->pat_s + 1) );
}

static void
_mesa_check_auth_datas_equal(u3_auth_data* aut_u, u3_auth_data* aot_u)
{
  _assert_eq_udF(aut_u->typ_e, aot_u->typ_e);
  switch ( aut_u->typ_e ) {
    case AUTH_SIGN: {
      u3_assert( 0 == memcmp(aut_u->sig_y, aot_u->sig_y, 64) );
    } break;
    case AUTH_HMAC: {
      u3_assert( 0 == memcmp(aut_u->mac_y, aot_u->mac_y, 16) );
    } break;
    case AUTH_NONE: {} break;
    case AUTH_PAIR: {
      u3_assert( 0 == memcmp(aut_u->has_y, aot_u->has_y, 64) );
    } break;
    default: u3_assert(!"unreachable");
  }
}

static void
_mesa_check_datas_equal(u3_mesa_data* dat_u, u3_mesa_data* dot_u)
{
  _assert_eq_udG(dat_u->tob_d, dot_u->tob_d);
  _mesa_check_auth_datas_equal(&dat_u->aut_u, &dot_u->aut_u);
  _assert_eq_udF(dat_u->len_w, dot_u->len_w);
  u3_assert( 0 == memcmp(dat_u->fra_y, dot_u->fra_y, dat_u->len_w) );
}

static void
_mesa_check_pacts_equal(u3_mesa_pact* pac_u, u3_mesa_pact* poc_u)
{
  _mesa_check_heads_equal(&poc_u->hed_u, &pac_u->hed_u);

  switch ( poc_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      _mesa_check_names_equal(&poc_u->pek_u.nam_u, &pac_u->pek_u.nam_u);
    } break;
    case PACT_POKE: {
      _mesa_check_names_equal(&poc_u->pok_u.nam_u, &pac_u->pok_u.nam_u);
      _mesa_check_names_equal(&poc_u->pok_u.pay_u, &pac_u->pok_u.pay_u);
      _mesa_check_datas_equal(&poc_u->pok_u.dat_u, &pac_u->pok_u.dat_u);
    } break;
    case PACT_PAGE: {
      _mesa_check_names_equal(&poc_u->pag_u.nam_u, &pac_u->pag_u.nam_u);
      _mesa_check_datas_equal(&poc_u->pag_u.dat_u, &pac_u->pag_u.dat_u);
    } break;
    default: u3_assert(!"unreachable");
  }
}

/* Logging functions
*/

static void
_log_head(u3_mesa_head* hed_u)
{
  u3l_log("-- HEADER --");
  u3l_log("next hop: %u", hed_u->nex_y);
  u3l_log("protocol: %u", hed_u->pro_y);
  u3l_log("packet type: %u", hed_u->typ_y);
  u3l_log("mug: 0x%05x", (hed_u->mug_w & 0xFFFFF));
  u3l_log("hopcount: %u", hed_u->hop_y);
  u3l_log("");
}

static void
_log_buf(c3_y* buf_y, c3_w len_w)
{
  c3_c *buf_c = c3_malloc(2 * len_w + 1);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    sprintf(buf_c + (i_w*2), "%02x", buf_y[i_w]);
  }
  u3l_log("%s", buf_c);
}

static void
_log_name_meta(u3_mesa_name_meta* met_u)
{
  u3l_log("meta");
  u3l_log("rank: %u", met_u->ran_y);
  u3l_log("rift length: %u", met_u->rif_y);
  u3l_log("nit: %u", met_u->nit_y);
  u3l_log("tau: %u", met_u->tau_y);
  u3l_log("frag num length: %u", met_u->gaf_y);
}

void
log_name(u3_mesa_name* nam_u)
{
  c3_c* her_c;
  {
    u3_noun her = u3dc("scot", c3__p, u3_ship_to_noun(nam_u->her_u));
    her_c = u3r_string(her);
    u3z(her);
  }

  u3l_log("%s: /%.*s", her_c, nam_u->pat_s, nam_u->pat_c);
  u3l_log("  rift: %u  bloq: %u  auth/data: %s init: %s frag: %"PRIu64,
          nam_u->rif_w,
          nam_u->boq_y,
          (c3y == nam_u->aut_o) ? "auth" : "data",
          (c3y == nam_u->nit_o) ? "init" : "nope",
          nam_u->fra_d
  );
  c3_free(her_c);
  fflush(stderr);
}

static void
_log_data(u3_mesa_data* dat_u)
{
  u3l_log("tob_d: %" PRIu64 "  len_w: %u  ",
                  dat_u->tob_d, dat_u->len_w);

  switch ( dat_u->aut_u.typ_e ) {
    case AUTH_SIGN: {
      u3l_log("signature: ");
      _log_buf(dat_u->aut_u.sig_y, 64);
    } break;

    case AUTH_HMAC: {
      u3l_log("hmac: ");
      _log_buf(dat_u->aut_u.mac_y, 16);
    } break;

    case AUTH_NONE: {
      u3l_log("merkle traversal: <skipped>");
    } break;

    case AUTH_PAIR: {
      u3l_log("merkle traversal:");
      _log_buf(dat_u->aut_u.has_y[0], 32);
      _log_buf(dat_u->aut_u.has_y[1], 32);
    } break;
  }
}


static void
_log_peek_pact(u3_mesa_peek_pact* pac_u)
{
  log_name(&pac_u->nam_u);
}

static void
_log_page_pact(u3_mesa_page_pact *pac_u)
{
  log_name(&pac_u->nam_u);
  _log_data(&pac_u->dat_u);
}

static void
_log_poke_pact(u3_mesa_poke_pact *pac_u)
{
  log_name(&pac_u->nam_u);
  log_name(&pac_u->pay_u);
  _log_data(&pac_u->dat_u);
}

void
log_pact(u3_mesa_pact* pac_u)
{
  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      fprintf(stderr, "PEEK ");
      _log_peek_pact(&pac_u->pek_u);
    } break;

    case PACT_PAGE: {
      fprintf(stderr, "PAGE ");
      _log_page_pact(&pac_u->pag_u);
    } break;

    case PACT_POKE: {
      fprintf(stderr, "POKE ");
      _log_poke_pact(&pac_u->pok_u);
    } break;

    default: {
      _log_poke_pact(&pac_u->pok_u);
      break;
    }
  }
}

/* Helper utilities
*/

/* mesa_num_leaves(): calculate how many leaves, given total bytes
*/
c3_d
mesa_num_leaves(c3_d tob_d)
{
  return (tob_d + 1023) / 1024;
}

void
inc_hopcount(u3_mesa_head* hed_u)
{
  hed_u->hop_y = c3_min(hed_u->hop_y+1, 7);
}

static c3_y
_mesa_rank(u3_ship who_u)
{
  switch ( u3_ship_rank(who_u) ) {
    case c3__pawn: return 3;
    case c3__earl: return 2;
    case c3__duke: return 1;
    case c3__king: return 0;
    case c3__czar: return 0;
    default: u3_assert(!"unreachable");
  };
}

/*
** _mesa_make_chub_tag(): make a 2-bit tag for a chub length
*/
static c3_y
_mesa_make_chub_tag(c3_d tot_d)
{
  return  (tot_d <= 0xff)?        0b00 :
          (tot_d <= 0xffff)?      0b01 :
          (tot_d <= 0xffffffff)?  0b10 :
                                  0b11;
}

/*
** _mesa_bytes_of_chub_tag(): how many bytes does a chub tag mean
*/
static c3_y
_mesa_bytes_of_chub_tag(c3_y tot_y)
{
  return 1 << tot_y;
}

/* serialisation
*/

typedef struct _u3_sifter {
  c3_y* buf_y;
  c3_w  rem_w;
  c3_d  bit_d; // for _etch_bits
  c3_y  off_y; // for _etch_bits
  c3_c* err_c;
} u3_sifter;

void
etcher_init(u3_etcher* ech_u, c3_y* buf_y, c3_w cap_w)
{
  ech_u->buf_y = buf_y;
  ech_u->len_w = 0;
  ech_u->cap_w = cap_w;
  ech_u->bit_d = 0;
  ech_u->off_y = 0;
}

static void
sifter_init(u3_sifter* sif_u, c3_y* buf_y, c3_w len_w)
{
  sif_u->buf_y = buf_y;
  sif_u->rem_w = len_w;
  sif_u->bit_d = 0;
  sif_u->off_y = 0;
  sif_u->err_c = NULL;
}

static void
_sift_fail(u3_sifter* sif_u, c3_c* msg_c)
{
  if ( sif_u->err_c ) {
    return;
  }
  sif_u->err_c = msg_c;
  #ifdef MESA_ROUNDTRIP
    u3l_log(RED_TEXT "sift fail: %s" DEF_TEXT, msg_c);
    assert(!"sift fail");
  #endif
}

static c3_y*
_etch_next(u3_etcher* ech_u, c3_w len_w)
{
  assert ( ech_u->off_y == 0 ); // ensure all bits were etched
  assert ( ech_u->len_w + len_w <= ech_u->cap_w ); // ensure buffer is big enough
  c3_y *res_y = ech_u->buf_y + ech_u->len_w;
  ech_u->len_w += len_w;
  return res_y;
}

static c3_y*
_sift_next(u3_sifter* sif_u, c3_w len_w)
{
  assert ( sif_u->off_y == 0 ); // ensure all bits were sifted
  if ( sif_u->err_c ) {
    return NULL;
  }
  else if ( len_w > sif_u->rem_w ) {
    _sift_fail(sif_u, "unexpected eof");
  }
  c3_y *res_y = sif_u->buf_y;
  sif_u->buf_y += len_w;
  sif_u->rem_w -= len_w;
  return res_y;
}

static void
_etch_bytes(u3_etcher* ech_u, c3_y *buf_y, c3_w len_w)
{
  c3_y *res_y = _etch_next(ech_u, len_w);
  memcpy(res_y, buf_y, len_w);
}

static void
_sift_bytes(u3_sifter* sif_u, c3_y *buf_y, c3_w len_w)
{
  c3_y *res_y = _sift_next(sif_u, len_w);
  if ( NULL == res_y ) {
    memset(buf_y, 0, len_w);
    return;
  }
  memcpy(buf_y, res_y, len_w);
}

static void
_etch_byte(u3_etcher* ech_u, c3_y val_y)
{
  _etch_next(ech_u, 1)[0] = val_y;
}

static c3_y
_sift_byte(u3_sifter* sif_u)
{
  c3_y *res_y = _sift_next(sif_u, 1);
  return ( NULL == res_y ) ? 0 : res_y[0];
}

static void
_etch_short(u3_etcher* ech_u, c3_s val_s)
{
  c3_etch_short(_etch_next(ech_u, 2), val_s);
}

static c3_s
_sift_short(u3_sifter* sif_u)
{
  c3_y *res_y = _sift_next(sif_u, 2);
  return ( NULL == res_y ) ? 0 : c3_sift_short(res_y);
}

static void
_etch_word(u3_etcher* ech_u, c3_w val_w)
{
  c3_etch_word(_etch_next(ech_u, 4), val_w);
}

static c3_w
_sift_word(u3_sifter* sif_u)
{
  c3_y *res_y = _sift_next(sif_u, 4);
  return ( NULL == res_y ) ? 0 : c3_sift_word(res_y);
}

static void
_etch_chub(u3_etcher* ech_u, c3_d val_d)
{
  c3_etch_chub(_etch_next(ech_u, 8), val_d);
}

static c3_d
_sift_chub(u3_sifter* sif_u)
{
  c3_y *res_y = _sift_next(sif_u, 8);
  return ( NULL == res_y ) ? 0 : c3_sift_chub(res_y);
}

static void
_etch_var_word(u3_etcher* ech_u, c3_w val_w, c3_w len_w)
{
  assert ( len_w <= 4 );
  c3_y *buf_y = _etch_next(ech_u, len_w);
  for ( c3_w i = 0; i < len_w; i++ ) {
    buf_y[i] = (val_w >> (8*i)) & 0xFF;
  }
}

static c3_w
_sift_var_word(u3_sifter* sif_u, c3_w len_w)
{
  assert ( len_w <= 4 );
  c3_y *res_y = _sift_next(sif_u, len_w);
  if ( NULL == res_y ) {
    return 0;
  }
  c3_w val_w = 0;
  for ( c3_w i = 0; i < len_w; i++ ) {
    val_w |= (res_y[i] << (8*i));
  }
  return val_w;
}

static void
_etch_var_chub(u3_etcher* ech_u, c3_d val_d, c3_w len_w)
{
  assert ( len_w <= 8 );
  c3_y *buf_y = _etch_next(ech_u, len_w);
  for ( int i = 0; i < len_w; i++ ) {
    buf_y[i] = (val_d >> (8*i)) & 0xFF;
  }
}

static c3_d
_sift_var_chub(u3_sifter* sif_u, c3_w len_w)
{
  assert ( len_w <= 8 );
  c3_y *res_y = _sift_next(sif_u, len_w);
  if ( NULL == res_y ) {
    return 0;
  }
  c3_d val_d = 0;
  for ( c3_d i = 0; i < len_w; i++ ) {
    val_d |= ((c3_d)res_y[i] << (8*i));
  }
  return val_d;
}

static void
_etch_bits(u3_etcher* ech_u, c3_w wid_w, c3_d val_d)
{
  assert ( ech_u->off_y + wid_w <= 64 );
  ech_u->bit_d |= ((val_d&((1 << wid_w) - 1)) << ech_u->off_y);
  ech_u->off_y += wid_w;
  while ( ech_u->off_y >= 8 ) {
    ech_u->buf_y[ech_u->len_w] = ech_u->bit_d & 0xFF;
    ech_u->bit_d >>= 8;
    ech_u->len_w += 1;
    ech_u->off_y -= 8;
  }
}

static c3_d
_sift_bits(u3_sifter* sif_u, c3_w wid_w)
{
  assert ( wid_w <= 64 );
  while ( sif_u->off_y < wid_w ) {
    c3_d byt_d = (sif_u->rem_w > 0) ? sif_u->buf_y[0] : 0;
    sif_u->buf_y += 1;
    sif_u->rem_w -= 1;
    sif_u->bit_d |= (byt_d << sif_u->off_y);
    sif_u->off_y += 8;
  }
  c3_d res_d = sif_u->bit_d & ((1 << wid_w) - 1);
  sif_u->bit_d >>= wid_w;
  sif_u->off_y -= wid_w;
  return res_d;
}

static void
_etch_ship(u3_etcher* ech_u, u3_ship who_u, c3_y len_y)
{
  assert ( len_y <= 16 );
  u3_ship_to_bytes(who_u, len_y, _etch_next(ech_u, len_y));
}

static void
_sift_ship(u3_sifter* sif_u, u3_ship who_u, c3_y len_y)
{
  assert ( len_y <= 16 );
  c3_y *res_y = _sift_next(sif_u, len_y);
  if ( NULL == res_y ) {
    who_u[0] = who_u[1] = 0;
    return;
  }
  u3_ship_of_bytes(who_u, len_y, res_y);
}

static void
_mesa_etch_head(u3_etcher* ech_u, u3_mesa_head* hed_u)
{
  if ( 1 != hed_u->pro_y ) {
    u3l_log("etching bad head");
  }
  _etch_bits(ech_u, 2, 0); // unused
  _etch_bits(ech_u, 2, hed_u->nex_y);
  _etch_bits(ech_u, 3, hed_u->pro_y);
  _etch_bits(ech_u, 2, hed_u->typ_y);
  _etch_bits(ech_u, 3, hed_u->hop_y);
  _etch_bits(ech_u, 20, hed_u->mug_w);
  _etch_word(ech_u, MESA_COOKIE);
}

c3_o
mesa_is_new_pact(c3_y* buf_y, c3_w len_w)
{
  return __((len_w >= 8) && c3_sift_word(buf_y + 4) == MESA_COOKIE);
}

void
mesa_sift_head(u3_sifter* sif_u, u3_mesa_head* hed_u)
{
                 _sift_bits(sif_u, 2); // unused
  hed_u->nex_y = _sift_bits(sif_u, 2);
  hed_u->pro_y = _sift_bits(sif_u, 3);
  hed_u->typ_y = _sift_bits(sif_u, 2);
  hed_u->hop_y = _sift_bits(sif_u, 3);
  hed_u->mug_w = _sift_bits(sif_u, 20);
  if ( 1 != hed_u->pro_y ) {
    _sift_fail(sif_u, "bad protocol");
  }
  if ( _sift_word(sif_u) != MESA_COOKIE ) {
    _sift_fail(sif_u, "bad cookie");
  }
}

void
_mesa_etch_name(u3_etcher *ech_u, u3_mesa_name* nam_u)
{
  u3_mesa_name_meta met_u = {0};
  met_u.ran_y = _mesa_rank(nam_u->her_u);
  met_u.rif_y = safe_dec(_mesa_met3_w(nam_u->rif_w));

  if ( c3y == nam_u->nit_o ) {
    met_u.nit_y = 1;
    met_u.tau_y = 0;
    met_u.gaf_y = 0;
  }
  else {
    met_u.nit_y = 0;
    met_u.tau_y = (c3y == nam_u->aut_o) ? 1 : 0;
    met_u.gaf_y = _mesa_make_chub_tag(nam_u->fra_d);
  }

  _etch_bits(ech_u, 2, met_u.ran_y);
  _etch_bits(ech_u, 2, met_u.rif_y);
  _etch_bits(ech_u, 1, met_u.nit_y);
  _etch_bits(ech_u, 1, met_u.tau_y);
  _etch_bits(ech_u, 2, met_u.gaf_y);
  c3_y her_y = 2 << met_u.ran_y; // XX confirm
  _etch_ship(ech_u, nam_u->her_u, her_y);
  // log_name(nam_u);
  _etch_var_word(ech_u, nam_u->rif_w, met_u.rif_y + 1);
  _etch_byte(ech_u, nam_u->boq_y);

  if ( met_u.nit_y ) {
    // init packet
  }
  else {
    // log_name(nam_u);
    _etch_var_chub(ech_u, nam_u->fra_d, 1 << met_u.gaf_y);
  }

  _etch_short(ech_u, nam_u->pat_s);
  _etch_bytes(ech_u, (c3_y*)nam_u->pat_c, nam_u->pat_s);
}

static void
_mesa_sift_name(u3_sifter* sif_u, u3_mesa_name* nam_u)
{
  nam_u->str_u.str_c = (c3_c*)sif_u->buf_y;
  c3_w rem_w = sif_u->rem_w;

  u3_mesa_name_meta met_u = {0};
  met_u.ran_y = _sift_bits(sif_u, 2);
  met_u.rif_y = _sift_bits(sif_u, 2);
  met_u.nit_y = _sift_bits(sif_u, 1);
  met_u.tau_y = _sift_bits(sif_u, 1);
  met_u.gaf_y = _sift_bits(sif_u, 2);

  nam_u->nit_o = __( met_u.nit_y == 1 );
  nam_u->aut_o = __( met_u.tau_y == 1 );

  _sift_ship(sif_u, nam_u->her_u, 2 << met_u.ran_y);
  nam_u->rif_w = _sift_var_word(sif_u, met_u.rif_y + 1);
  nam_u->boq_y = _sift_byte(sif_u);

  if ( met_u.nit_y ) {
    assert( !met_u.tau_y );
    assert( !met_u.gaf_y );
    // XX init packet
    nam_u->fra_d = 0;
  }
  else {
    nam_u->fra_d = _sift_var_chub(sif_u, 1 << met_u.gaf_y);
  }

  nam_u->pat_s = _sift_short(sif_u);

  nam_u->pat_c = (c3_c*)sif_u->buf_y;
  /* nam_u->pat_c = c3_calloc(nam_u->pat_s + 1); */
  /* _sift_bytes(sif_u, (c3_y*)nam_u->pat_c, nam_u->pat_s); */

  sif_u->buf_y += nam_u->pat_s;
  sif_u->rem_w -= nam_u->pat_s;

  nam_u->str_u.len_w = rem_w - sif_u->rem_w;

  /* nam_u->pat_c[nam_u->pat_s] = 0; */
}

static void
_mesa_etch_data(u3_etcher* ech_u, u3_mesa_data* dat_u)
{
  u3_mesa_data_meta met_u = {0};
  met_u.bot_y = _mesa_make_chub_tag(dat_u->tob_d);
  met_u.aut_o = __(dat_u->aut_u.typ_e == AUTH_SIGN || dat_u->aut_u.typ_e == AUTH_HMAC);
  met_u.auv_o = __(dat_u->aut_u.typ_e == AUTH_SIGN || dat_u->aut_u.typ_e == AUTH_NONE);
  c3_y nel_y = _mesa_met3_w(dat_u->len_w);
  met_u.men_y = c3_min(nel_y, 3);
  _etch_bits(ech_u, 2, met_u.bot_y);
  _etch_bits(ech_u, 1, met_u.aut_o);
  _etch_bits(ech_u, 1, met_u.auv_o);
  _etch_bits(ech_u, 2, 0); // unused
  _etch_bits(ech_u, 2, met_u.men_y);
  _etch_var_chub(ech_u, dat_u->tob_d, 1 << met_u.bot_y);
  switch ( dat_u->aut_u.typ_e ) {
    case AUTH_SIGN: {
      _etch_bytes(ech_u, dat_u->aut_u.sig_y, 64);
    } break;
    case AUTH_HMAC: {
      _etch_bytes(ech_u, dat_u->aut_u.mac_y, 16);
    } break;
    case AUTH_NONE: {
    } break;
    case AUTH_PAIR: {
      _etch_bytes(ech_u, dat_u->aut_u.has_y[0], 32);
      _etch_bytes(ech_u, dat_u->aut_u.has_y[1], 32);
    } break;
  }

  if ( 3 == met_u.men_y ) {
    _etch_byte(ech_u, nel_y);
  }
  _etch_var_word(ech_u, dat_u->len_w, nel_y);
  _etch_bytes(ech_u, dat_u->fra_y, dat_u->len_w);
}

static void
_mesa_sift_data(u3_sifter* sif_u, u3_mesa_data* dat_u)
{
  u3_mesa_data_meta met_u = {0};
  met_u.bot_y = _sift_bits(sif_u, 2);
  met_u.aut_o = _sift_bits(sif_u, 1);
  met_u.auv_o = _sift_bits(sif_u, 1);
                _sift_bits(sif_u, 2); // unused
  met_u.men_y = _sift_bits(sif_u, 2);

  dat_u->tob_d = _sift_var_chub(sif_u, 1<<met_u.bot_y);

  if ( c3y == met_u.aut_o ) {
    if ( c3y == met_u.auv_o ) {
      _sift_bytes(sif_u, dat_u->aut_u.sig_y, 64);
      dat_u->aut_u.typ_e = AUTH_SIGN;
    }
    else {
      _sift_bytes(sif_u, dat_u->aut_u.mac_y, 16);
      dat_u->aut_u.typ_e = AUTH_HMAC;
    }
  }
  else {
    if ( c3y == met_u.auv_o ) {
      dat_u->aut_u.typ_e = AUTH_NONE;
    } else {
      _sift_bytes(sif_u, dat_u->aut_u.has_y[0], 32);
      _sift_bytes(sif_u, dat_u->aut_u.has_y[1], 32);
      dat_u->aut_u.typ_e = AUTH_PAIR;
    }
  }

  c3_y nel_y = met_u.men_y;
  if ( 3 == met_u.men_y ) {
    nel_y = _sift_byte(sif_u);
  }
  dat_u->len_w = _sift_var_word(sif_u, nel_y);
  dat_u->fra_y = _sift_next(sif_u, dat_u->len_w);
}

static void
_mesa_etch_hop_long(u3_etcher* ech_u, u3_mesa_hop_once* hop_u)
{
  _etch_byte(ech_u, hop_u->len_w);
  _etch_bytes(ech_u, hop_u->dat_y, hop_u->len_w);
}

static void
_mesa_etch_page_pact(u3_etcher* ech_u, u3_mesa_page_pact* pag_u, u3_mesa_head* hed_u)
{
  _mesa_etch_name(ech_u, &pag_u->nam_u);
  _mesa_etch_data(ech_u, &pag_u->dat_u);

  switch ( hed_u->nex_y ) {
    case HOP_NONE: {
    } break;
    case HOP_SHORT: {
      _etch_bytes(ech_u, pag_u->sot_u, 6);
    } break;
    case HOP_LONG: {
      _mesa_etch_hop_long(ech_u, &pag_u->one_u);
    } break;
    case HOP_MANY: {
      _etch_byte(ech_u, pag_u->man_u.len_w);
      for ( c3_w i = 0; i < pag_u->man_u.len_w; i++ ) {
        _mesa_etch_hop_long(ech_u, &pag_u->man_u.dat_y[i]);
      }
    } break;
    default: {
      return;
    } break;
  }
}

static void
_mesa_sift_page_pact(u3_sifter* sif_u, u3_mesa_page_pact* pag_u, c3_y nex_y)
{
  _mesa_sift_name(sif_u, &pag_u->nam_u);
  _mesa_sift_data(sif_u, &pag_u->dat_u);

  switch ( nex_y ) {
    default: {
      u3_assert(!"mesa: sift invalid hop type");
    }
    case HOP_NONE: return;
    case HOP_SHORT: {
      _sift_bytes(sif_u, pag_u->sot_u, 6);
      return;
    }
    case HOP_LONG: {
      _sift_fail(sif_u, "mesa: sift invalid hop long");
      return;
    }
    case HOP_MANY: {
      _sift_fail(sif_u, "mesa: sift invalid hop many");
      return;
    }
  }
}

static void
_mesa_etch_peek_pact(u3_etcher* ech_u, u3_mesa_peek_pact* pac_u)
{
  _mesa_etch_name(ech_u, &pac_u->nam_u);
}

static void
_mesa_sift_peek_pact(u3_sifter* sif_u, u3_mesa_peek_pact* pac_u)
{
  _mesa_sift_name(sif_u, &pac_u->nam_u);
}

static void
_mesa_etch_poke_pact(u3_etcher* ech_u, u3_mesa_poke_pact* pac_u)
{
  _mesa_etch_name(ech_u, &pac_u->nam_u);
  _mesa_etch_name(ech_u, &pac_u->pay_u);
  _mesa_etch_data(ech_u, &pac_u->dat_u);
}

static void
_mesa_sift_poke_pact(u3_sifter* sif_u, u3_mesa_poke_pact* pac_u)
{
  _mesa_sift_name(sif_u, &pac_u->nam_u);
  _mesa_sift_name(sif_u, &pac_u->pay_u);
  _mesa_sift_data(sif_u, &pac_u->dat_u);
}

static void
_mesa_etch_pact(u3_etcher* ech_u, u3_mesa_pact* pac_u)
{
  // use a separate etcher to make computing the mug easier
  u3_etcher pec_u;
  etcher_init(&pec_u, ech_u->buf_y + ech_u->len_w + 8, ech_u->cap_w - ech_u->len_w - 8);

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_POKE: {
      _mesa_etch_poke_pact(&pec_u, &pac_u->pok_u);
    } break;
    case PACT_PAGE: {
      _mesa_etch_page_pact(&pec_u, &pac_u->pag_u, &pac_u->hed_u);
    } break;
    case PACT_PEEK: {
      _mesa_etch_peek_pact(&pec_u, &pac_u->pek_u);
    } break;
    default: {
      u3l_log("bad pact type %u", pac_u->hed_u.typ_y);
    }
  }
  // now we can compute the mug and write the header
  pac_u->hed_u.mug_w = u3r_mug_bytes(pec_u.buf_y, pec_u.len_w)
                     & 0xFFFFF;
  _mesa_etch_head(ech_u, &pac_u->hed_u);

  // the payload is already written into the correct spot, so we just need to
  // adjust the length
  ech_u->len_w += pec_u.len_w;
}

static void
_mesa_sift_pact(u3_sifter* sif_u, u3_mesa_pact* pac_u)
{
  mesa_sift_head(sif_u, &pac_u->hed_u);

  // for mug, later
  c3_y *mug_y = sif_u->buf_y;
  c3_w pre_w = sif_u->rem_w;

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      _mesa_sift_peek_pact(sif_u, &pac_u->pek_u);
    } break;
    case PACT_PAGE: {
      _mesa_sift_page_pact(sif_u, &pac_u->pag_u, pac_u->hed_u.nex_y);
    } break;
    case PACT_POKE: {
      _mesa_sift_poke_pact(sif_u, &pac_u->pok_u);
    } break;
    default: {
      /* u3l_log("mesa: received unknown packet type"); */
      break;
    }
  }

  {
    c3_w mug_w = u3r_mug_bytes(mug_y, pre_w - sif_u->rem_w)
               & 0xFFFFF;
    if ( mug_w != pac_u->hed_u.mug_w ) {
      _sift_fail(sif_u, "bad mug");
      return;
    }
  }
}

/* packet etch/sift, with roundtrip tests */

c3_w
mesa_etch_pact_to_buf(c3_y* buf_y, c3_w cap_w, u3_mesa_pact *pac_u) {
  u3_etcher ech_u;
  etcher_init(&ech_u, buf_y, cap_w);
  _mesa_etch_pact(&ech_u, pac_u);

  #ifdef MESA_ROUNDTRIP
    u3_mesa_pact poc_u;
    u3_sifter sif_u;
    sifter_init(&sif_u, ech_u.buf_y, ech_u.len_w);
    _mesa_sift_pact(&sif_u, &poc_u);
    if ( sif_u.rem_w && !sif_u.err_c ) {
      u3l_log("mesa: etch roundtrip failed: %u trailing bytes", sif_u.rem_w);
      _sift_fail(&sif_u, "trailing bytes");
    }
    if ( sif_u.err_c ) {
      u3l_log("mesa: roundtrip failed: %s", sif_u.err_c);
      assert(!"roundtrip failed");
    }
    _mesa_check_pacts_equal(&poc_u, pac_u);
    mesa_free_pact(&poc_u);
  #endif

  return ech_u.len_w;
}

c3_c*
mesa_sift_pact_from_buf(u3_mesa_pact *pac_u, c3_y* buf_y, c3_w len_w) {
  u3_sifter sif_u;
  sifter_init(&sif_u, buf_y, len_w);
  _mesa_sift_pact(&sif_u, pac_u);
  if ( sif_u.rem_w && !sif_u.err_c ) {
    _sift_fail(&sif_u, "trailing bytes");
  }

  #ifdef MESA_ROUNDTRIP
    c3_y* bof_y = c3_calloc(len_w);
    u3_etcher ech_u;
    etcher_init(&ech_u, bof_y, len_w);
    _mesa_etch_pact(&ech_u, pac_u);
    u3_assert( 0 == memcmp(bof_y, buf_y, len_w) );
    c3_free(bof_y);
  #endif

  return sif_u.err_c;
}

/* sizing
*/
static c3_w
_mesa_size_name(u3_mesa_name* nam_u)
{
  c3_w siz_w = 1;
  u3_mesa_name_meta met_u;

  met_u.ran_y = _mesa_rank(nam_u->her_u);
  met_u.rif_y = safe_dec(_mesa_met3_w(nam_u->rif_w));

  siz_w += 2 << met_u.ran_y;
  siz_w += met_u.rif_y + 1;
  siz_w++;  // bloq

  if ( nam_u->nit_o == c3n ) {
    met_u.gaf_y = _mesa_make_chub_tag(nam_u->fra_d);
    siz_w += _mesa_bytes_of_chub_tag(met_u.gaf_y);
  }

  siz_w += 2;  // path-length
  siz_w += nam_u->pat_s;

  return siz_w;
}

static c3_w
_mesa_size_data(u3_mesa_data* dat_u)
{
  c3_w siz_w = 1;
  u3_mesa_data_meta met_u;

  met_u.bot_y = _mesa_make_chub_tag(dat_u->tob_d);
  siz_w += _mesa_bytes_of_chub_tag(met_u.bot_y);

  switch ( dat_u->aut_u.typ_e ) {
    case AUTH_SIGN: {
      siz_w += 64;
    } break;

    case AUTH_HMAC: {
      siz_w += 16;
    } break;

    case AUTH_NONE: {
    } break;

    case AUTH_PAIR: {
      siz_w += 64;
    } break;
  }

  c3_y nel_y = _mesa_met3_w(dat_u->len_w);
  met_u.men_y = (3 >= nel_y) ? nel_y : 3;

  if ( 3 == met_u.men_y ) {
    siz_w++;
  }

  siz_w += nel_y;
  siz_w += dat_u->len_w;

  return siz_w;
}

static c3_w
_mesa_size_hops(u3_mesa_pact* pac_u)
{
  if ( PACT_PAGE != pac_u->hed_u.typ_y ) {
    return 0;
  }

  switch ( pac_u->hed_u.nex_y ) {
    case HOP_NONE:  return 0;
    case HOP_SHORT: return 6;
    case HOP_LONG:  return 1 + pac_u->pag_u.one_u.len_w;
    case HOP_MANY: {
      c3_w siz_w = 0;
      for ( c3_w i = 0; i < pac_u->pag_u.man_u.len_w; i++ ) {
        siz_w += 1 + pac_u->pag_u.man_u.dat_y[i].len_w;
      }
      return siz_w;
    }
    default: u3_assert(!"mesa: invalid hop type");
  }
}

c3_w
mesa_size_pact(u3_mesa_pact* pac_u)
{
  c3_w siz_w = 8; // header + cookie;

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      siz_w += _mesa_size_name(&pac_u->pek_u.nam_u);
    } break;

    case PACT_PAGE: {
      siz_w += _mesa_size_name(&pac_u->pag_u.nam_u);
      siz_w += _mesa_size_data(&pac_u->pag_u.dat_u);
      siz_w += _mesa_size_hops(pac_u);
    } break;

    case PACT_POKE: {
      siz_w += _mesa_size_name(&pac_u->pok_u.nam_u);
      siz_w += _mesa_size_name(&pac_u->pok_u.pay_u);
      siz_w += _mesa_size_data(&pac_u->pok_u.dat_u);
    } break;

    default: {
      u3l_log("bad pact type %u", pac_u->hed_u.typ_y);//u3m_bail(c3__bail);
      return 0;
    }
  }

  return siz_w;
}

#ifdef PACT_TEST

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

#define cmp_scalar(nam, str, fmt)                       \
  if ( hav_u->nam != ned_u->nam ) {                     \
    fprintf(stderr, "mesa test cmp " str " differ:\r\n" \
                    "    have: " fmt "\r\n"             \
                    "    need: " fmt "\r\n",            \
                    hav_u->nam, ned_u->nam);            \
    ret_i = 1;                                          \
  }

#define cmp_string(nam, siz, str)                       \
  if ( memcmp(hav_u->nam, ned_u->nam, siz) ) {          \
    fprintf(stderr, "mesa test cmp " str " differ:\r\n" \
                    "    have: %*.s\r\n"                \
                    "    need: %*.s\r\n",               \
                    siz, hav_u->nam, siz, ned_u->nam);  \
    ret_i = 1;                                          \
  }

#define cmp_buffer(nam, siz, str)                 \
  if ( memcmp(hav_u->nam, ned_u->nam, siz) ) {    \
    fprintf(stderr, "mesa test cmp " str "\r\n"); \
    ret_i = 1;                                    \
  }

static c3_i
_test_cmp_head(u3_mesa_head* hav_u, u3_mesa_head* ned_u)
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
_test_cmp_name(u3_mesa_name* hav_u, u3_mesa_name* ned_u)
{
  c3_i ret_i = 0;

  cmp_buffer(her_u, sizeof(ned_u->her_u), "name: ships differ");

  cmp_scalar(rif_w, "name: rifts", "%u");
  cmp_scalar(boq_y, "name: bloqs", "%u");
  cmp_scalar(nit_o, "name: inits", "%u");
  cmp_scalar(aut_o, "name: auths", "%u");
  cmp_scalar(fra_d, "name: fragments", "%" PRIu64);
  cmp_scalar(pat_s, "name: path-lengths", "%u");

  cmp_string(pat_c, ned_u->pat_s, "name: paths");

  return ret_i;
}

static c3_i
_test_cmp_data(u3_mesa_data* hav_u, u3_mesa_data* ned_u)
{
  c3_i ret_i = 0;

  cmp_scalar(tob_d, "data: total packets", "%" PRIu64);

  cmp_scalar(aut_u.typ_e, "data: auth-types", "%u");
  cmp_buffer(aut_u.sig_y, 64, "data: sig|hmac|null");

  // cmp_scalar(aut_u.len_y, "data: hash-lengths", "%u");

  if ( AUTH_PAIR == ned_u->aut_u.typ_e ) {
    cmp_buffer(aut_u.has_y, 2, "data: hashes");
  }

  cmp_scalar(len_w, "data: fragments-lengths", "%u");
  cmp_buffer(fra_y, ned_u->len_w, "data: fragments");

  return ret_i;
}

static c3_i
_test_pact(u3_mesa_pact* pac_u)
{
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w  len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, pac_u);
  c3_i  ret_i = 0;
  c3_i  bot_i = 0;
  c3_w  sif_w;

  u3_mesa_pact nex_u;
  memset(&nex_u, 0, sizeof(u3_mesa_pact));

  if ( !len_w ) {
    fprintf(stderr, "pact: etch failed\r\n");
    ret_i = 1; goto done;
  }
  else if ( len_w > PACT_SIZE ) {
    fprintf(stderr, "pact: etch overflowed: %u\r\n", len_w);
    ret_i = 1; goto done;
  }

  u3_sifter sif_u;
  sifter_init(&sif_u, buf_y, len_w);
  _mesa_sift_pact(&sif_u, &nex_u);

  if ( sif_u.rem_w && !sif_u.err_c ) {
    fprintf(stderr, "pact: sift failed len=%u sif=%u\r\n", len_w, sif_u.rem_w);
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
    log_pact(pac_u);
    _log_buf(buf_y, len_w);

    if ( bot_i ) {
      u3l_log(RED_TEXT);
      _log_head(&nex_u.hed_u);
      log_pact(&nex_u);
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

static c3_d
_test_rand_chub(void* ptr_v)
{
  c3_w low_w = _test_rand_word(ptr_v);
  c3_w hig_w = _test_rand_word(ptr_v);
  return ((c3_d)hig_w << 32) ^ low_w;
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
_test_make_head(void* ptr_v, u3_mesa_head* hed_u)
{
  hed_u->nex_y = HOP_NONE; // XX
  hed_u->pro_y = 1;
  hed_u->typ_y = _test_rand_gulf_y(ptr_v, 3) + 1;
  hed_u->hop_y = _test_rand_gulf_y(ptr_v, 8);
  hed_u->mug_w = 0;
  // XX set mug_w in etch?
}

static void
_test_make_name(void* ptr_v, c3_s pat_s, u3_mesa_name* nam_u)
{
  _test_rand_bytes(ptr_v, 16, (c3_y*)nam_u->her_u);
  nam_u->rif_w = _test_rand_word(ptr_v);

  nam_u->pat_s = _test_rand_gulf_w(ptr_v, pat_s);
  nam_u->pat_c = c3_malloc(nam_u->pat_s + 1);
  _test_rand_path(ptr_v, nam_u->pat_s, nam_u->pat_c);
  nam_u->pat_c[nam_u->pat_s] = 0;

  nam_u->boq_y = _test_rand_bits(ptr_v, 8);
  nam_u->nit_o = _test_rand_bits(ptr_v, 1);

  if ( c3y == nam_u->nit_o ) {
    nam_u->aut_o = c3n;
    nam_u->fra_d = 0;
  }
  else {
    nam_u->aut_o = _test_rand_bits(ptr_v, 1);
    nam_u->fra_d = _test_rand_chub(ptr_v);
  }
}

static void
_test_make_data(void* ptr_v, u3_mesa_data* dat_u)
{
  dat_u->tob_d = _test_rand_chub(ptr_v);

  memset(dat_u->aut_u.sig_y, 0, 64);
  // dat_u->aut_u.len_y = 0;
  memset(dat_u->aut_u.has_y, 0, sizeof(dat_u->aut_u.has_y));

  switch ( dat_u->aut_u.typ_e = _test_rand_bits(ptr_v, 2) ) {

    case AUTH_SIGN: {
      _test_rand_bytes(ptr_v, 64, dat_u->aut_u.sig_y);
    } break;

    case AUTH_HMAC: {
      _test_rand_bytes(ptr_v, 16, dat_u->aut_u.mac_y);
    } break;

    case AUTH_NONE: {

    } break;

    case AUTH_PAIR: {
      _test_rand_bytes(ptr_v, 32, dat_u->aut_u.has_y[0]);
      _test_rand_bytes(ptr_v, 32, dat_u->aut_u.has_y[1]);
    } break;

    default: break;
  }

  dat_u->len_w = _test_rand_gulf_w(ptr_v, 1024);
  dat_u->fra_y = c3_calloc(dat_u->len_w);
  _test_rand_bytes(ptr_v, dat_u->len_w, dat_u->fra_y);
}

static void
_test_make_pact(void* ptr_v, u3_mesa_pact* pac_u)
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
  u3_mesa_pact pac_u;
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
  u3_mesa_pact pac_u;
  memset(&pac_u,0, sizeof(u3_mesa_pact));
  pac_u.hed_u.typ_y = PACT_PAGE;
  pac_u.hed_u.pro_y = 1;
  u3l_log("%%page checking sift/etch idempotent");
  u3_mesa_name* nam_u = &pac_u.pag_u.nam_u;

  {
    u3_noun her = u3v_wish("~hastuc-dibtux");
    u3r_chubs(0, 2, nam_u->her_u, her);
    u3z(her);
  }
  nam_u->rif_w = 15;
  nam_u->pat_c = "foo/bar";
  nam_u->pat_s = strlen(nam_u->pat_c);
  nam_u->boq_y = 13;
  nam_u->fra_d = 54;
  nam_u->nit_o = c3n;

  u3_mesa_data* dat_u = &pac_u.pag_u.dat_u;
  dat_u->aut_u.typ_e = AUTH_NONE;
  dat_u->tob_d = 1000;
  dat_u->len_w = 1024;
  dat_u->fra_y = c3_calloc(1024);
  // dat_u->fra_y[1023] = 1;

  if ( _test_pact(&pac_u) ) {
    fprintf(stderr, RED_TEXT "%%page failed\r\n");
    exit(1);
  }
}


static void
_test_encode_path(c3_c* pat_c)
{
  u3_noun wan = u3do("stab", u3dt("cat", 3, '/', u3i_string(pat_c)));
  u3_noun hav = _mesa_encode_path(strlen(pat_c), (c3_y*)pat_c);
  // u3m_p("hav", hav);
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

  _test_rand_pact(100000);

  _test_encode_path("foo/bar/baz");
  _test_encode_path("publ/0/xx//1/foo/g");
  return 0;
}


#endif
