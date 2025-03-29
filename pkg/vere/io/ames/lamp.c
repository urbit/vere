#include "vere.h"
#include "io/ames/lamp.h"

#define NAME tuf_map
#define KEY_TY u3_ship
#define HASH_FN u3_hash_ship
#define CMPR_FN u3_cmpr_ship
#define VAL_TY c3_y**
#define IMPLEMENTATION_MODE
#include "verstable.h"

/* _ames_czar_port(): udp port for galaxy.
*/
static c3_s
_ames_czar_port(c3_y imp_y)
{
  if ( c3n == u3_Host.ops_u.net ) {
    return htons(31337 + imp_y);
  }
  else {
    return htons(13337 + imp_y);
  }
}

/* _ames_czar_str: galaxy name as c3_c[3]
*/
static void
_ames_czar_str(c3_c zar_c[3], c3_y imp_y)
{
  u3_po_to_suffix(imp_y, (c3_y*)zar_c, (c3_y*)zar_c + 1, (c3_y*)zar_c + 2);
}

/* _ames_etch_czar: galaxy fqdn
*/
static c3_i
_ames_etch_czar(c3_c dns_c[256], const c3_c* dom_c, c3_y imp_y)
{
  c3_c* bas_c = dns_c;
  c3_w  len_w = strlen(dom_c);

  //  name 3, '.' 2, trailing null
  //
  if ( 250 <= len_w ) {
    return -1;
  }

  _ames_czar_str(dns_c, imp_y);
  dns_c   += 3;
  *dns_c++ = '.';

  memcpy(dns_c, dom_c, len_w);
  dns_c   += len_w;
  *dns_c++ = '.';

  memset(dns_c, 0, 256 - (dns_c - bas_c));

  return 0;
}

/* _ames_czar_lane: retrieve lane for galaxy if stored.
*/
c3_o
_ames_czar_lane(u3_ames* sam_u, c3_y imp_y, sockaddr_in* lan_u)
{

  lan_u->sin_family = AF_INET;
  lan_u->sin_port = _ames_czar_port(imp_y);

  if ( c3n == u3_Host.ops_u.net ) {
    lan_u->sin_addr.s_addr = NLOCALHOST;
  }
  else {
    lan_u->sin_addr.s_addr =
      sam_u->zar_u.pip_w[imp_y];

    if ( !lan_u->sin_addr.s_addr ) {
      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("ames: czar not resolved");
      }
      return c3n;
    }
    else if ( _CZAR_GONE == lan_u->sin_addr.s_addr ) {
      //  print only on first send failure
      //
      c3_w blk_w = imp_y >> 5;
      c3_w bit_w = 1 << (imp_y & 31);

      if ( !(sam_u->zar_u.log_w[blk_w] & bit_w) ) {
        c3_c dns_c[256];
        u3_assert ( !_ames_etch_czar(dns_c, sam_u->zar_u.dom_c, imp_y) );
        u3l_log("ames: czar at %s: not found (b)", dns_c);
        sam_u->zar_u.log_w[blk_w] |= bit_w;
      }

      return c3n;
    }
  }

  return c3y;
}

typedef struct _czar_resv {
  uv_getaddrinfo_t adr_u;
  u3_ames*         sam_u;
  c3_y             imp_y;
} _czar_resv;

/* _ames_czar_gone(): galaxy address resolution failed.
*/
static void
_ames_czar_gone(u3_ames* sam_u, c3_y imp_y)
{
  c3_w old_w = sam_u->zar_u.pip_w[imp_y];

  if ( !old_w ) {
    sam_u->zar_u.pip_w[imp_y] = _CZAR_GONE;
  }
}

/* _ames_czar_here(): galaxy address resolution succeeded.
*/
static void
_ames_czar_here(u3_ames* sam_u, c3_y imp_y, sockaddr_in lan_u)
{
  c3_w old_w = sam_u->zar_u.pip_w[imp_y];

  if ( lan_u.sin_addr.s_addr != old_w ) {
    c3_c dns_c[256];
    c3_w nip_w = lan_u.sin_addr.s_addr;
    c3_c nip_c[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &nip_w, nip_c, INET_ADDRSTRLEN);

    u3_assert ( !_ames_etch_czar(dns_c, sam_u->zar_u.dom_c, imp_y) );
    u3l_log("ames: czar %s ip .%s", dns_c, nip_c);
  }

  sam_u->zar_u.pip_w[imp_y] = lan_u.sin_addr.s_addr;

  {
    c3_w blk_w = imp_y >> 5;
    c3_w bit_w = 1 << (imp_y & 31);

    sam_u->zar_u.log_w[blk_w] &= ~bit_w;
  }
}

/* _ames_czar_cb(): galaxy address resolution callback.
*/
static void
_ames_czar_cb(uv_getaddrinfo_t* adr_u,
               c3_i              sas_i,
               struct addrinfo*  aif_u)
{
  struct addrinfo* rai_u = aif_u;
  _czar_resv*      res_u = (_czar_resv*)adr_u;
  u3_ames*         sam_u = res_u->sam_u;
  c3_y             imp_y = res_u->imp_y;

  while ( rai_u && (AF_INET != rai_u->ai_family) ) {
    rai_u = rai_u->ai_next;
  }

  if ( rai_u && rai_u->ai_addr ) {
    struct sockaddr_in* lan_u = (void*)rai_u->ai_addr;
    _ames_czar_here(sam_u, imp_y, *lan_u);
  }
  else {
    if ( !sas_i ) {
      // XX unpossible
      u3l_log("ames: czar: strange failure, no error");
    }
    else if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("ames: czar fail: %s", uv_strerror(sas_i));
    }

    _ames_czar_gone(sam_u, imp_y);
  }

  sam_u->zar_u.pen_s--;

  uv_freeaddrinfo(aif_u);
  c3_free(res_u);
}

/* _ames_czar(): single galaxy address resolution.
*/
static void
_ames_czar(u3_ames* sam_u, const c3_c* dom_c, c3_y imp_y)
{
  struct addrinfo   hin_u = { .ai_family = AF_INET };
  uv_getaddrinfo_t* adr_u;
  _czar_resv*       res_u;
  c3_c              dns_c[256];
  c3_i              sas_i;

  u3_assert ( !_ames_etch_czar(dns_c, dom_c, imp_y) );

  res_u = c3_malloc(sizeof(*res_u));
  res_u->sam_u = sam_u;
  res_u->imp_y = imp_y;

  adr_u = &(res_u->adr_u);
  sas_i = uv_getaddrinfo(u3L, adr_u, _ames_czar_cb, dns_c, 0, &hin_u);

  if ( sas_i ) {
    _ames_czar_cb(adr_u, sas_i, NULL);
  }
}

/* _ames_czar_all(): galaxy address resolution.
*/
static void
_ames_czar_all(uv_timer_t* tim_u)
{
  u3_ames* sam_u = tim_u->data;

  //  requests still pending
  if ( sam_u->zar_u.pen_s ) {
    uv_timer_start(&sam_u->zar_u.tim_u, _ames_czar_all, 30*1000, 0);
    return;
  }

  sam_u->zar_u.pen_s = 256;

  for ( c3_w i_w = 0; i_w < 256; i_w++ ) {
    _ames_czar(sam_u, sam_u->zar_u.dom_c, (c3_y)i_w);
  }

  uv_timer_start(&sam_u->zar_u.tim_u, _ames_czar_all, 300*1000, 0);
}

/* _ames_ef_turf(): initialize ames I/O on domain(s).
*/
static void
_ames_ef_turf(u3_ames* sam_u, u3_noun tuf)
{
  if ( u3_nul != tuf ) {
    c3_c  dom_c[sizeof(sam_u->zar_u.dom_c)];
    u3_noun hot = u3h(tuf);
    c3_w  len_w = u3_mcut_host(0, 0, u3k(hot));

    if ( len_w >= sizeof(dom_c) ) {  // >250
      //  3 char for the galaxy (e.g. zod) and two dots
      u3l_log("ames: galaxy domain too big (len=%u)", len_w);
      u3m_p("hot", hot);
      u3_pier_bail(u3_king_stub());
    }

    u3_mcut_host(dom_c, 0, u3k(hot));
    memset(dom_c + len_w, 0, sizeof(dom_c) - len_w);

    if ( 0 != memcmp(sam_u->zar_u.dom_c, dom_c, sizeof(dom_c)) ) {
      memcpy(sam_u->zar_u.dom_c, dom_c, sizeof(dom_c));
      memset(sam_u->zar_u.pip_w, 0, sizeof(sam_u->zar_u.pip_w));
      sam_u->zar_u.dom_o = c3y;
      _ames_czar_all(&(sam_u->zar_u.tim_u));
    }

    //  XX save all for fallback, not just first
    //
    if ( u3_nul != u3t(tuf) ) {
      u3l_log("ames: turf: ignoring additional domains");
      u3m_p("second", u3h(u3t(tuf)));

      if ( u3_nul != u3t(u3t(tuf)) ) {
        u3m_p("third", u3h(u3t(u3t(tuf))));
      }
    }

    u3z(tuf);
  }
  else if ( (c3n == sam_u->pir_u->fak_o) && (c3n == sam_u->zar_u.dom_o) ) {
    u3l_log("ames: turf: no domains");
  }

  //  XX is this ever necessary?
  //
  if ( c3n == sam_u->car_u.liv_o ) {
    _ames_io_start(sam_u);
  }
}

