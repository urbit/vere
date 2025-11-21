#include "c3/c3.h"
#include "types.h"
#include "imprison.h"
#include "retrieve.h"
#include "vortex.h"
#include "ship.h"

void
u3_ship_to_bytes(u3_ship who_u, c3_y len_y, c3_y* buf_y)
{
  c3_y sip_y[16] = {0};

  c3_etch_chub(sip_y, who_u.hed_d);
  c3_etch_chub(sip_y + 8, who_u.tel_d);

  memcpy(buf_y, sip_y, c3_min(16, len_y));
}

u3_ship
u3_ship_of_bytes(c3_y len_y, c3_y* buf_y)
{
  u3_ship who_u;
  c3_y sip_y[16] = {0};
  memcpy(sip_y, buf_y, c3_min(16, len_y));

  who_u.hed_d = c3_sift_chub(sip_y);
  who_u.tel_d = c3_sift_chub(sip_y + 8);
  return who_u;
}

u3_atom
u3_ship_to_noun(u3_ship who_u)
{
  return u3i_chubs(2, &who_u.hed_d);
}

c3_c*
u3_ship_to_string(u3_ship who_u)
{
  u3_noun ser = u3dc("scot", c3__p, u3_ship_to_noun(who_u));
  c3_c* who_c = u3r_string(ser);
  u3z(ser);
  return who_c;
}

u3_ship
u3_ship_of_noun(u3_noun who)
{
  u3_ship who_u;
  u3r_chubs(0, 2, &who_u.hed_d, who);
  return who_u;
}

c3_o
u3_ships_equal(u3_ship sip_u, u3_ship sap_u)
{
  return __((sip_u.hed_d == sap_u.hed_d) && (sip_u.tel_d == sap_u.tel_d));
}

void
u3_ship_copy(u3_ship des_u, u3_ship src_u)
{
  des_u.hed_d = src_u.hed_d;
  des_u.tel_d = src_u.tel_d;
}

c3_l
u3_ship_rank(u3_ship who_u)
{
  if      ( who_u.tel_d )       return c3__pawn;
  else if ( who_u.hed_d >> 32 ) return c3__earl;
  else if ( who_u.hed_d >> 16 ) return c3__duke;
  else if ( who_u.hed_d >> 8 )  return c3__king;
  else                       return c3__czar;
}

c3_y
u3_ship_czar(u3_ship who_u) { return who_u.hed_d & 0xFF; }

c3_s
u3_ship_king(u3_ship who_u) { return who_u.hed_d & 0xffff; }

c3_w
u3_ship_duke(u3_ship who_u) { return who_u.hed_d & 0xffffffff; }

c3_d
u3_ship_earl(u3_ship who_u) { return who_u.hed_d; }
