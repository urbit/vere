#include "xmas.h"
#include <stdio.h>

#define SIFT_VAR(dest, src, len) dest = 0; for(int i = 0; i < len; i++ ) { dest |= ((src + i) >> (8*i)); }
#define CHECK_BOUNDS(cur) if ( len_w < cur ) { u3l_log("xmas: failed parse (%u,%u) at line %i", len_w, cur, __LINE__); return 0; }
#define safe_dec(num) (num == 0 ? num : num - 1)
#define _xmas_met3_w(a_w) ((c3_bits_word(a_w) + 0x7) >> 3)
#define u3_assert(x)                      \
  do {                                    \
    if (!(x)) {                           \
      fflush(stderr);                     \
      fprintf(stderr, "\rAssertion '%s' " \
              "failed in %s:%d\r\n",      \
              #x, __FILE__, __LINE__);    \
      /*u3m_bail(c3__oops); */                \
      /*abort(); */                            \
    }                                     \
  } while(0)

#define u3l_log(...)  fprintf(stderr, __VA_ARGS__)
#define c3_calloc(s) ({                                    \
    void* rut = calloc(1,s);                                \
    if ( 0 == rut ) {                                       \
      fprintf(stderr, "c3_calloc(%" PRIu64 ") failed\r\n",  \
                      (c3_d)s);                             \
      u3_assert(!"memory lost");                            \
    }                                                       \
    rut;})


/* Logging functions
*/

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
    // u3_noun her = u3dc("scot", c3__p, u3i_chubs(2, nam_u->her_d));
    // c3_c* her_c = u3r_string(her);
    // u3l_log("publisher: %s", her_c);
    // c3_free(her_c);
    // u3z(her);
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

/* Helper utilities
*/
static void
_update_hopcount(u3_xmas_head* hed_u)
{
  hed_u->hop_y = c3_max(hed_u->hop_y+1, 7);
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

/* lifecycle
*/
void xmas_free_pact(u3_xmas_pact* pac_u) 
{
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
      break;
    };
  }
  c3_free(pac_u);
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
_xmas_sift_page_pact(u3_xmas_pact* pat_u, c3_y nex_y, c3_y* buf_y, c3_w len_w)
{
  u3_xmas_page_pact* pac_u = &pat_u->pag_u;
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

c3_w
xmas_sift_pact(u3_xmas_pact* pac_u, c3_y* buf_y, c3_w len_w)
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
      res_w = _xmas_sift_page_pact(pac_u, pac_u->hed_u.nex_y, buf_y, len_w);
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

/* serialisation
*/
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



/* sizing
*/
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

c3_w
xmas_etch_pact(c3_y* buf_y, u3_xmas_pact* pac_u)
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