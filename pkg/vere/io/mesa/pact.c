#include "mesa.h"
#include <stdio.h>
// only need for tests, can remove
#include "vere.h"
#include "ivory.h"
#include "ur.h"
#define RED_TEXT    "\033[0;31m"
#define DEF_TEXT    "\033[0m"
// endif tests

#define SIFT_VAR(dest, src, len) dest = 0; for(int i = 0; i < len; i++ ) { dest |= ((src + i) >> (8*i)); }
#define CHECK_BOUNDS(cur) if ( len_w < cur ) { u3l_log("mesa: failed parse (%u,%u) at line %i", len_w, cur, __LINE__); return 0; }
#define safe_dec(num) (num == 0 ? num : num - 1)
#define _mesa_met3_w(a_w) ((c3_bits_word(a_w) + 0x7) >> 3)


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
  for( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    fprintf(stderr, "%02x", buf_y[i_w]);
  }
  fprintf(stderr, "\r\n");
}

void
log_name(u3_mesa_name* nam_u)
{
  // u3l_log("meta");
  // u3l_log("rank: %u", nam_u->met_u.ran_y);
  // u3l_log("rift length: %u", nam_u->met_u.rif_y);
  // u3l_log("nit: %u", nam_u->met_u.nit_y);
  // u3l_log("tau: %u", nam_u->met_u.tau_y);
  // u3l_log("frag num length: %u", nam_u->met_u.gaf_y);

  c3_c* her_c;
  {
    u3_noun her = u3dc("scot", c3__p, u3i_chubs(2, nam_u->her_d));
    her_c = u3r_string(her);
    u3z(her);
  }

  u3l_log("%s: /%s", her_c, nam_u->pat_c);
  u3l_log("  rift: %u  bloq: %u  init: %s  auth: %s  frag: %u",
          nam_u->rif_w,
          nam_u->boq_y,
          (c3y == nam_u->nit_o) ? "&" : "|",
          (c3y == nam_u->aut_o) ? "&" : "|",
          nam_u->fra_w
  );
  c3_free(her_c);
}

static void
_log_data(u3_mesa_data* dat_u)
{
  fprintf(stderr, "tot_w: %u  len_w: %u  ", dat_u->tot_w, dat_u->len_w);

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
      fprintf(stderr, "signature: ");
      _log_buf(dat_u->aum_u.sig_y, 64);
    } break;

    case AUTH_HMAC: {
      fprintf(stderr, "hmac: ");
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
  fprintf(stderr, "\r\n");
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

/* Helper utilities
*/
void
update_hopcount(u3_mesa_head* hed_u) //  TODO rename, _inc_hopcount()?
{
  hed_u->hop_y = c3_max(hed_u->hop_y+1, 7);
}

static c3_y
_mesa_rank(c3_d who_d[2])
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

/* lifecycle
*/

/* mesa_free_pact(): free contents of packet.
*    Does *not* free pac_u itself
*/
void mesa_free_pact(u3_mesa_pact* pac_u)
{
  //  TODO free pat_c in the name
  switch ( pac_u->hed_u.typ_y ) {
    default: {
      break;
    };
    case PACT_PEEK: {
      break;
    };
    case PACT_PAGE: {
      c3_free(pac_u->pag_u.dat_u.fra_y);
      break;
    };
    case PACT_POKE: {
      c3_free(pac_u->pok_u.dat_u.fra_y);
      break;
    };
  }
}

/* deserialisation
*/

static void
_ames_etch_word(c3_y buf_y[4], c3_w wod_w)
{
  buf_y[0] = wod_w         & 0xff;
  buf_y[1] = (wod_w >>  8) & 0xff;
  buf_y[2] = (wod_w >> 16) & 0xff;
  buf_y[3] = (wod_w >> 24) & 0xff;
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

static inline c3_w
_ames_sift_word(c3_y buf_y[4])
{
  return (buf_y[3] << 24 | buf_y[2] << 16 | buf_y[1] << 8 | buf_y[0]);
}


static inline void
_ames_ship_to_chubs(c3_d sip_d[2], c3_y len_y, c3_y* buf_y)
{
  c3_y sip_y[16] = {0};
  memcpy(sip_y, buf_y, c3_min(16, len_y));

  sip_d[0] = _ames_chub_bytes(sip_y);
  sip_d[1] = _ames_chub_bytes(sip_y + 8);
}

c3_o
mesa_sift_head(c3_y buf_y[8], u3_mesa_head* hed_u)
{
  if ( memcmp(buf_y + 4, &MESA_COOKIE, MESA_COOKIE_LEN) ) {
    return c3n;
  }
  c3_w hed_w = _ames_sift_word(buf_y);

  hed_u->nex_y = (hed_w >> 2)  & 0x3;
  hed_u->pro_y = (hed_w >> 4)  & 0x7;
  hed_u->typ_y = (hed_w >> 7)  & 0x3;
  hed_u->hop_y = (hed_w >> 9)  & 0x7;
  hed_u->mug_w = (hed_w >> 12) & 0xFFFFF;

  assert( 1 == hed_u->pro_y );

  return c3y;

  /*if(c3o((hed_u->typ_y == PACT_PEEK), (hed_u->typ_y == PACT_POKE))) {
    hed_u->ran_y = (hed_w >> 30) & 0x3;
  }*/
}

static c3_w
_mesa_sift_name(u3_mesa_name* nam_u, c3_y* buf_y, c3_w len_w)
{
#ifdef MESA_DEBUG
  //u3l_log("mesa: sifting name %i", len_w);
#endif

  c3_w cur_w = 0;
  u3_mesa_name_meta met_u;

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
_mesa_sift_data(u3_mesa_data* dat_u, c3_y* buf_y, c3_w len_w)
{
// #ifdef MESA_DEBUG
  // u3l_log("mesa: sifting data %i", len_w);
// #endif

  c3_w cur_w = 0;
  u3_mesa_data_meta met_u;

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
_mesa_sift_hop_long(u3_mesa_hop_once* hop_u, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0;
  CHECK_BOUNDS(cur_w + 1);
  hop_u->len_w = buf_y[cur_w];
  cur_w++;
  CHECK_BOUNDS(cur_w + hop_u->len_w);
  hop_u->dat_y = c3_calloc(hop_u->len_w);
  memcpy(hop_u->dat_y, buf_y + cur_w, hop_u->len_w);

  return cur_w + hop_u->len_w;
}

static c3_w
_mesa_sift_hops(u3_mesa_page_pact* pac_u, c3_y nex_y, c3_y* buf_y, c3_w len_w)
{
  switch ( nex_y ) {
    default: {
      u3_assert(!"mesa: sift invalid hop type");
    }
    case HOP_NONE: return 0;
    case HOP_SHORT: {
      CHECK_BOUNDS(6);
      memcpy(pac_u->sot_u, buf_y, 6);
      return 6;
    }
    case HOP_LONG: {
      return _mesa_sift_hop_long(&pac_u->one_u, buf_y, len_w);
    }
    case HOP_MANY: {
      c3_w siz_w = 0;
      CHECK_BOUNDS(siz_w + 1);
      pac_u->man_u.len_w = buf_y[0];
      siz_w++;

      pac_u->man_u.dat_y = c3_calloc(sizeof(u3_mesa_hop_once) * pac_u->man_u.len_w);

      for( c3_w i = 0; i < pac_u->man_u.len_w; i++ ) {
        siz_w += _mesa_sift_hop_long(&pac_u->man_u.dat_y[i],
                                     buf_y + siz_w,
                                     len_w - siz_w);
      }
      return siz_w;
    }
  }
}

static c3_w
_mesa_sift_page_pact(u3_mesa_pact* pat_u, c3_y nex_y, c3_y* buf_y, c3_w len_w)
{
  u3_mesa_page_pact* pac_u = &pat_u->pag_u;
  c3_w cur_w = 0, nex_w;

  if ( !(nex_w = _mesa_sift_name(&pac_u->nam_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _mesa_sift_data(&pac_u->dat_u, buf_y + cur_w, len_w)) ) {
    return 0;
  }
  cur_w += nex_w;

  cur_w += _mesa_sift_hops(pac_u, nex_y, buf_y + cur_w, len_w);

  return cur_w;
}

static c3_w
_mesa_sift_peek_pact(u3_mesa_peek_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w siz_w = _mesa_sift_name(&pac_u->nam_u, buf_y, len_w);
  if ( siz_w < len_w ) {
    u3l_log("mesa: failed to consume entire packet");
    _log_buf(buf_y + siz_w, len_w - siz_w);
    return 0;
  }

  return siz_w;
}

static c3_w
_mesa_sift_poke_pact(u3_mesa_poke_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w cur_w = 0, nex_w;
  //  ack path
  if ( !(nex_w = _mesa_sift_name(&pac_u->nam_u, buf_y + cur_w, len_w)) ) {
    // u3l_log("_mesa_sift_poke_pact nex_w %u", nex_w);
    return 0;
  }
  cur_w += nex_w;

  //  payload path
  if ( !(nex_w = _mesa_sift_name(&pac_u->pay_u, buf_y + cur_w, len_w)) ) {
    // u3l_log("_mesa_sift_poke_pact nex_w %u", nex_w);
    return 0;
  }
  cur_w += nex_w;

  //  payload
  if ( !(nex_w = _mesa_sift_data(&pac_u->dat_u, buf_y + cur_w, len_w)) ) {
    // u3l_log("_mesa_sift_poke_pact nex_w %u", nex_w);
    return 0;
  }
  cur_w += nex_w;

  // u3l_log("_mesa_sift_poke_pact cur_w %u", cur_w);

  return cur_w;
}

c3_w
mesa_sift_pact(u3_mesa_pact* pac_u, c3_y* buf_y, c3_w len_w)
{
  c3_w res_w = 0;

  if ( len_w < 8 ) {
    u3l_log("mesa: attempted to parse overly short packet of size %u", len_w);
  }

  mesa_sift_head(buf_y, &pac_u->hed_u);
  buf_y += 8;
  len_w -= 8;

  // u3l_log("pac_u->hed_u.typ_y typ_y %u", pac_u->hed_u.typ_y);

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      res_w = _mesa_sift_peek_pact(&pac_u->pek_u, buf_y, len_w);
    } break;
    case PACT_PAGE: {
      res_w = _mesa_sift_page_pact(pac_u, pac_u->hed_u.nex_y, buf_y, len_w);
    } break;
    case PACT_POKE: {
      res_w = _mesa_sift_poke_pact(&pac_u->pok_u, buf_y, len_w);
    } break;
    default: {
      /* u3l_log("mesa: received unknown packet type"); */
      /* _log_buf(buf_y, len_w); */
      break;
    }
  }

  {
    c3_w mug_w = u3r_mug_bytes(buf_y, res_w);
    mug_w &= 0xFFFFF;

    if ( mug_w != pac_u->hed_u.mug_w ) {
      /* u3l_log("mesa: failed mug"); */
      /* _log_buf(buf_y, res_w); */
      /* u3l_log("res_w %u", res_w); */
      /* u3l_log("frag %u", pac_u->pag_u.nam_u.fra_w); */
      return 0;
    }
  }

  //u3_assert(res_w <= len_w );
  return res_w + 8;
}

/* serialisation
*/
static void
_mesa_etch_head(u3_mesa_head* hed_u, c3_y buf_y[8])
{
  if ( 1 != hed_u->pro_y ) {
    u3l_log("etching bad head");
  }

  // c3_o req_o = c3o((hed_u->typ_y == PACT_PEEK), (hed_u->typ_y == PACT_POKE));
  // c3_y siz_y = req_o ? 5 : 7;
  c3_w hed_w = (hed_u->nex_y & 0x3) << 2
             ^ (hed_u->pro_y & 0x7) << 4  // XX constant, 1
             ^ (hed_u->typ_y & 0x3) << 7
             ^ (hed_u->hop_y & 0x7) << 9
             ^ (hed_u->mug_w & 0xFFFFF) << 12;
             // XX: we don't expand hopcount if no request. Correct?
      //
  /*if ( c3y == req_o ) {
    hed_w = hed_w ^ ((hed_u->ran_y & 0x3) << 30);
  }*/

  _ames_etch_word(buf_y, hed_w);
  memcpy(buf_y + 4, MESA_COOKIE, MESA_COOKIE_LEN);
}

static c3_w
_mesa_etch_name(c3_y* buf_y, u3_mesa_name* nam_u)
{
#ifdef MESA_DEBUG

#endif
  c3_w cur_w = 0;
  u3_mesa_name_meta met_u;

  met_u.ran_y = _mesa_rank(nam_u->her_d);
  met_u.rif_y = safe_dec(_mesa_met3_w(nam_u->rif_w));

  if ( c3y == nam_u->nit_o ) {
    assert( c3n == nam_u->aut_o ); // XX
    met_u.nit_y = 1;
    met_u.tau_y = 0;
    met_u.gaf_y = 0;
  }
  else {
    met_u.nit_y = 0;
    met_u.tau_y = (c3y == nam_u->aut_o) ? 1 : 0;
    met_u.gaf_y = safe_dec(_mesa_met3_w(nam_u->fra_w));
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
_mesa_etch_data(c3_y* buf_y, u3_mesa_data* dat_u)
{
#ifdef MESA_DEBUG

#endif
  c3_w cur_w = 0;
  u3_mesa_data_meta met_u;

  met_u.bot_y = safe_dec(_mesa_met3_w(dat_u->tot_w));
  // XX
  met_u.aul_y = dat_u->aum_u.typ_e;
  met_u.aur_y = dat_u->aup_u.len_y;
  c3_y nel_y = _mesa_met3_w(dat_u->len_w);
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
_mesa_etch_page_pact(c3_y* buf_y, u3_mesa_page_pact* pac_u, u3_mesa_head* hed_u)
{
  c3_w cur_w = 0, nex_w;

  if ( !(nex_w = _mesa_etch_name(buf_y + cur_w, &pac_u->nam_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _mesa_etch_data(buf_y + cur_w, &pac_u->dat_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  // XX hops

  return cur_w;
}

static c3_w
_mesa_etch_poke_pact(c3_y* buf_y, u3_mesa_poke_pact* pac_u, u3_mesa_head* hed_u)
{
  c3_w cur_w = 0, nex_w;

  if ( !(nex_w = _mesa_etch_name(buf_y + cur_w, &pac_u->nam_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _mesa_etch_name(buf_y + cur_w, &pac_u->pay_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  if ( !(nex_w = _mesa_etch_data(buf_y + cur_w, &pac_u->dat_u)) ) {
    return 0;
  }
  cur_w += nex_w;

  return cur_w;
}



/* sizing
*/
static c3_w
_mesa_size_name(u3_mesa_name* nam_u)
{
  c3_w siz_w = 1;
  u3_mesa_name_meta met_u;

  met_u.ran_y = _mesa_rank(nam_u->her_d);
  met_u.rif_y = safe_dec(_mesa_met3_w(nam_u->rif_w));

  siz_w += 2 << met_u.ran_y;
  siz_w += met_u.rif_y + 1;
  siz_w++;  // bloq

  if (c3n == nam_u->nit_o ) {
    met_u.gaf_y = safe_dec(_mesa_met3_w(nam_u->fra_w));
    siz_w += met_u.gaf_y + 1;
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

  met_u.bot_y = safe_dec(_mesa_met3_w(dat_u->tot_w));

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
      for( c3_w i = 0; i < pac_u->pag_u.man_u.len_w; i++ ) {
        siz_w += 1 + pac_u->pag_u.man_u.dat_y[i].len_w;
      }
      return siz_w;
    }
    default: u3_assert(!"mesa: invalid hop type");
  }
}

static c3_w
_mesa_size_pact(u3_mesa_pact* pac_u)
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

c3_w
mesa_etch_pact(c3_y* buf_y, u3_mesa_pact* pac_u)
{
  c3_w siz_w = _mesa_size_pact(pac_u);

  // if ( siz_w > PACT_SIZE ) {
  //   fprintf(stderr, "etch: would overflow %u\r\n", siz_w);
  //   return 0;
  // }

  u3_mesa_head* hed_u = &pac_u->hed_u;

  c3_w nex_w, cur_w = 8; // space for header + cookie

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_POKE: {
      if ( !(nex_w = _mesa_etch_poke_pact(buf_y + cur_w, &pac_u->pok_u, hed_u)) ) {
        return 0;
      }
    } break;

    case PACT_PEEK: {
      if ( !(nex_w = _mesa_etch_name(buf_y + cur_w, &pac_u->pek_u.nam_u)) ) {
        return 0;
      }
    } break;

    case PACT_PAGE: {
      if ( !(nex_w = _mesa_etch_page_pact(buf_y + cur_w, &pac_u->pag_u, hed_u)) ) {
        return 0;
      }
    } break;

    default: {
      u3l_log("bad pact type %u", pac_u->hed_u.typ_y);//u3m_bail(c3__bail);
      return 0;
    }
  }

  hed_u->mug_w  = u3r_mug_bytes(buf_y + cur_w, nex_w);
  hed_u->mug_w &= 0xFFFFF;

  _mesa_etch_head(hed_u, buf_y);

  cur_w += nex_w;

  assert( siz_w == cur_w );

  return cur_w;
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
_test_cmp_data(u3_mesa_data* hav_u, u3_mesa_data* ned_u)
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
_test_pact(u3_mesa_pact* pac_u)
{
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w  len_w = mesa_etch_pact(buf_y, pac_u);
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

  if ( len_w != (sif_w = mesa_sift_pact(&nex_u, buf_y, len_w)) ) {
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
  hed_u->nex_y = NEXH_NONE; // XX
  hed_u->pro_y = 1;
  hed_u->typ_y = _test_rand_gulf_y(ptr_v, 3) + 1;
  hed_u->hop_y = _test_rand_gulf_y(ptr_v, 8);
  hed_u->mug_w = 0;
  // XX set mug_w in etch?
}

static void
_test_make_name(void* ptr_v, c3_s pat_s, u3_mesa_name* nam_u)
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
_test_make_data(void* ptr_v, u3_mesa_data* dat_u)
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
    u3r_chubs(0, 2, nam_u->her_d, her);
    u3z(her);
  }
  nam_u->rif_w = 15;
  nam_u->pat_c = "foo/bar";
  nam_u->pat_s = strlen(nam_u->pat_c);
  nam_u->boq_y = 13;
  nam_u->fra_w = 54;
  nam_u->nit_o = c3n;

  u3_mesa_data* dat_u = &pac_u.pag_u.dat_u;
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
_test_encode_path(c3_c* pat_c)
{
  u3_noun wan = u3do("stab", u3dt("cat", 3, '/', u3i_string(pat_c)));
  u3_noun hav = _mesa_encode_path(strlen(pat_c), (c3_y*)pat_c);

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
