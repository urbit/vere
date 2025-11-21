#include "nat.h"

static void
natpmp_cb(uv_poll_t* handle,
          c3_i        status,
          c3_i        events)
{

  if (status != 0) {
    return;
  }

  u3_natpmp_state* nat_u = handle->data;

  natpmpresp_t response;
  c3_i err_i = readnatpmpresponseorretry(&nat_u->req_u, &response);
  if ( NATPMP_TRYAGAIN == err_i ) {
    return;
  }

  uv_poll_stop(handle);

  if ( 0 != err_i ) {
    u3l_log("ames: natpmp error %i", err_i);
    uv_poll_stop(&nat_u->pol_u);
    closenatpmp(&nat_u->req_u);
    return;
  }

  u3l_log("ames: mapped public port %hu to localport %hu lifetime %u",
         response.pnu.newportmapping.mappedpublicport,
         response.pnu.newportmapping.privateport,
         response.pnu.newportmapping.lifetime);

  closenatpmp(&nat_u->req_u);
  nat_u->tim_u.data = nat_u;
  uv_timer_start(&nat_u->tim_u, natpmp_init, 7200000, 0);
}

void
natpmp_init(uv_timer_t *handle)
{
  u3_natpmp_state* nat_u = handle->data;
  c3_s por_s = nat_u->car_u->pir_u->por_s;

  c3_i err_i = initnatpmp(&nat_u->req_u, 0, 0);

  if (err_i != 0) {
    return;
  }

  err_i = uv_poll_init(u3L, &nat_u->pol_u, nat_u->req_u.s);

  if (err_i != 0) {
    return;
  }

  sendnewportmappingrequest(&nat_u->req_u, NATPMP_PROTOCOL_UDP, por_s, por_s, 7200);

  nat_u->pol_u.data = nat_u;
  uv_poll_start(&nat_u->pol_u, UV_READABLE, natpmp_cb);
}


