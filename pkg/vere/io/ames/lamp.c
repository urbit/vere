#include "vere.h"
#include "io/ames/lamp.h"

c3_o
_mesa_is_lane_zero(sockaddr_in lan_u);

/* _ames_czar_port(): udp port for galaxy.
*/
c3_s
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

u3_peer*
_mesa_get_peer(void* sam_u, u3_ship her_u);

u3_peer*
_mesa_gut_peer(void* sam_u, u3_ship her_u);

/* _ames: retrieve lane for galaxy if stored.
*/
c3_o
_ames_lamp_lane(u3_auto* car_u, u3_ship her_u, sockaddr_in* lan_u)
{
  u3_peer* per_u = _mesa_get_peer(car_u, her_u);
  if (NULL == per_u) return c3n;

  if (c3n == per_u->lam_o) {
    u3l_log("fatal: peer is not lamp");
    u3_king_bail();
  }

  if ( c3n == u3_Host.ops_u.net ) {
    lan_u->sin_addr.s_addr = NLOCALHOST;
  }
  else {
    *lan_u = per_u->dan_u;
    if ( c3y == _mesa_is_lane_zero(*lan_u) ) {
      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("ames: lamp not resolved");
      }
      return c3n;
    }
    else if ( _CZAR_GONE == lan_u->sin_addr.s_addr ) {
      //  print only on first send failure
      //

      if ( c3n == per_u->log_o ) {
        // XX
        //u3l_log("ames: lamp at %s: not found (b)", *dns_c);
        per_u->log_o = c3y;
      }

      return c3n;
    }
  }

  return c3y;
}

typedef struct _lamp_resv {
  uv_getaddrinfo_t adr_u;
  u3_lamp_state*         lam_u;
  u3_peer*         per_u;
  c3_c**           dns_c;
} _lamp_resv;

/* _ames_lamp_gone(): galaxy address resolution failed.
*/
static void
_ames_lamp_gone(u3_lamp_state* lam_u, u3_peer* per_u)
{
  c3_w old_w = per_u->dan_u.sin_addr.s_addr;

  if ( !old_w ) {
    per_u->dan_u.sin_addr.s_addr = _CZAR_GONE;
  }
}

/* _ames_lamp_here(): galaxy address resolution succeeded.
*/
static void
_ames_lamp_here(u3_lamp_state* lam_u, u3_peer* per_u, sockaddr_in lan_u, c3_c* dns_c)
{
  c3_w old_w = per_u->dan_u.sin_addr.s_addr;

  if ( lan_u.sin_addr.s_addr != old_w ) {
    c3_w nip_w = lan_u.sin_addr.s_addr;
    c3_c nip_c[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &nip_w, nip_c, INET_ADDRSTRLEN);

    u3l_log("ames: lamp %s ip .%s", dns_c, nip_c);
  }

  per_u->dan_u.sin_addr.s_addr = lan_u.sin_addr.s_addr;
  per_u->log_o = c3n;
}

static void
_ames_lamp(u3_lamp_state* lam_u, u3_peer* per_u, c3_c** dns_c);

/* _ames_lamp_cb(): galaxy address resolution callback.
*/
static void
_ames_lamp_cb(uv_getaddrinfo_t* adr_u,
               c3_i              sas_i,
               struct addrinfo*  aif_u)
{
  struct addrinfo* rai_u = aif_u;
  _lamp_resv*      res_u = (_lamp_resv*)adr_u;
  u3_lamp_state*         lam_u = res_u->lam_u;
  u3_peer*         per_u = res_u->per_u;

  while ( rai_u && (AF_INET != rai_u->ai_family) ) {
    rai_u = rai_u->ai_next;
  }

  if ( rai_u && rai_u->ai_addr ) {
    struct sockaddr_in* lan_u = (void*)rai_u->ai_addr;
    _ames_lamp_here(lam_u, per_u, *lan_u, *res_u->dns_c);
    lam_u->pen_s--;
    uv_freeaddrinfo(aif_u);
    c3_free(res_u);
  }
  else {
    if ( !sas_i ) {
      // XX unpossible
      u3l_log("ames: lamp: strange failure, no error");
      _ames_lamp_gone(lam_u, per_u);
      lam_u->pen_s--;
      uv_freeaddrinfo(aif_u);
      c3_free(res_u);
    }
    else {
      c3_c** dns_c = res_u->dns_c;
      c3_o nex_o = (NULL != *(dns_c + 1)) ? c3y : c3n;
      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("ames: lamp fail: %s", uv_strerror(sas_i));
        //if (nex_o) u3l_log("trying next");
      }
      if (c3y == nex_o)
        _ames_lamp(lam_u, per_u, dns_c + 1);
      else {
        // XX: wait should we do this here?
        _ames_lamp_gone(lam_u, per_u);
        lam_u->pen_s--;
        uv_freeaddrinfo(aif_u);
        c3_free(res_u);
      }
    }
  }
}

/* _ames_lamp(): single galaxy address resolution.
*/
static void
_ames_lamp(u3_lamp_state* lam_u, u3_peer* per_u, c3_c** dns_c)
{
  struct addrinfo   hin_u = { .ai_family = AF_INET };
  uv_getaddrinfo_t* adr_u;
  _lamp_resv*       res_u;
  c3_i              sas_i;

  res_u = c3_malloc(sizeof(*res_u));
  res_u->lam_u = lam_u;
  res_u->per_u = per_u;
  res_u->dns_c = dns_c;

  adr_u = &(res_u->adr_u);
  sas_i = uv_getaddrinfo(u3L, adr_u, _ames_lamp_cb, *dns_c, 0, &hin_u);

  if ( sas_i ) {
    _ames_lamp_cb(adr_u, sas_i, NULL);
  }
}

static void
_ames_etch_czars(u3_lamp_state* lam_u) {
  for (c3_w i = 0; i < 256; i++) {
    u3_ship who_u = u3_ship_of_noun(i);
    u3_peer* per_u = _mesa_gut_peer(lam_u->car_u, who_u);
    per_u->lam_o = c3y;
    per_u->dan_u.sin_family = AF_INET;
    per_u->dan_u.sin_port = _ames_czar_port(i);
    c3_c* who_c = u3_ship_to_string(who_u);
    c3_w len_w = u3_mpef_turfs(NULL, 0, who_c + 1, lam_u->dns_c);
    if (per_u->dns_c) c3_free(per_u->dns_c);
    c3_c** dns_c = per_u->dns_c = c3_malloc(len_w);
    u3_mpef_turfs((c3_c*)dns_c, 0, who_c + 1, lam_u->dns_c);
    c3_free(who_c);
  }
}

/* _ames_lamp_all(): galaxy address resolution.
*/
void
_ames_lamp_all(uv_timer_t* tim_u)
{
  u3_lamp_state* lam_u = tim_u->data;

  //  requests still pending
  if ( lam_u->pen_s ) {
    uv_timer_start(&lam_u->tim_u, _ames_lamp_all, 30*1000, 0);
    return;
  }

  lam_u->pen_s = 0;
  //c3_w i = 0;
  for(
    per_map_itr itr =
      per_map_first( lam_u->per_u );
    !per_map_is_end( itr );
    itr = per_map_next( itr ) ) {
    u3_peer* per_u = itr.data->val;
    //c3_c* who_c = u3_ship_to_string(per_u->her_u);
    //u3l_log("czar %u %s", i, who_c);
    //c3_free(who_c);
    //i++;
    if (per_u->dns_c) {
      lam_u->pen_s++;
      _ames_lamp(lam_u, per_u, per_u->dns_c);
    }
  }

  uv_timer_start(&lam_u->tim_u, _ames_lamp_all, 300*1000, 0);
}

static c3_o
_ames_cmp_turfs(c3_c** a, c3_c** b) {
  while (a != NULL || b != NULL) {
    if (a == NULL) {
      if (b == NULL) return c3y;
      return c3n;
    }
    if (b == NULL) {
      return c3n;
    }
    if (0 != strcmp(*a, *b)) return 0;
    a++;
    b++;
  }
  return c3y;
}

/* _ames_ef_turf(): initialize ames I/O on domain(s).
*/
void
_ames_ef_turf(u3_lamp_state* lam_u, u3_noun tuf)
{
  if ( u3_nul != tuf) {
    c3_w len_w = u3_mcut_hosts(NULL, 0, u3k(tuf));
    if (len_w == 0) {
      // todo: clear?
      return;
    }
    c3_c** dns_c = c3_malloc(len_w);
    u3_mcut_hosts((c3_c*)dns_c, 0, u3k(tuf));
    _ames_cmp_turfs(dns_c, lam_u->dns_c);
    if ( c3n == _ames_cmp_turfs(dns_c, lam_u->dns_c) ) {
      c3_free(lam_u->dns_c);
      lam_u->dns_c = dns_c;
      lam_u->dom_o = c3y;
      _ames_etch_czars(lam_u);
      _ames_lamp_all(&(lam_u->tim_u));
    }
  }
  else if ( (c3n == lam_u->car_u->pir_u->fak_o) && (c3n == lam_u->dom_o) ) {
    u3l_log("ames: turf: no domains");
  }

  ////  XX is this ever necessary?
  ////
  //if ( c3n == lam_u->car_u.liv_o ) {
  //  _ames_io_start(lam_u);
  //}
}
