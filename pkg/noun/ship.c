#include "c3/c3.h"
#include "types.h"
#include "imprison.h"
#include "retrieve.h"
#include "vortex.h"
#include "ship.h"

static inline void
_s_chub_to_bytes(c3_y byt_y[8], c3_d num_d)
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

static inline c3_d
_s_bytes_to_chub(c3_y byt_y[8])
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

void
u3_ship_to_bytes(u3_ship who_u, c3_y len_y, c3_y* buf_y)
{
  c3_y sip_y[16] = {0};

  _s_chub_to_bytes(sip_y, who_u[0]);
  _s_chub_to_bytes(sip_y + 8, who_u[1]);

  memcpy(buf_y, sip_y, c3_min(16, len_y));
}

void
u3_ship_of_bytes(u3_ship who_u, c3_y len_y, c3_y* buf_y)
{
  c3_y sip_y[16] = {0};
  memcpy(sip_y, buf_y, c3_min(16, len_y));

  who_u[0] = _s_bytes_to_chub(sip_y);
  who_u[1] = _s_bytes_to_chub(sip_y + 8);
}

u3_atom
u3_ship_to_noun(u3_ship who_u)
{
  return u3i_chubs(2, who_u);
}

c3_c*
u3_ship_to_string(u3_ship who_u)
{
  u3_noun ser = u3dc("scot", c3__p, u3_ship_to_noun(who_u));
  c3_c* who_c = u3r_string(ser);
  u3z(ser);
  return who_c;
}

void
u3_ship_of_noun(u3_ship who_u, u3_noun who)
{
  u3r_chubs(0, 2, who_u, who);
}

c3_o
u3_ships_equal(u3_ship sip_u, u3_ship sap_u)
{
  return __((sip_u[0] == sap_u[0]) && (sip_u[1] == sap_u[1]));
}

void
u3_ship_copy(u3_ship des_u, u3_ship src_u)
{
  des_u[0] = src_u[0];
  des_u[1] = src_u[1];
}

c3_l
u3_ship_rank(u3_ship who_u)
{
  if      ( who_u[1] )       return c3__pawn;
  else if ( who_u[0] >> 32 ) return c3__earl;
  else if ( who_u[0] >> 16 ) return c3__duke;
  else if ( who_u[0] >> 8 )  return c3__king;
  else                       return c3__czar;
}

c3_y
u3_ship_czar(u3_ship who_u) { return who_u[0] & 0xFF; }

c3_s
u3_ship_king(u3_ship who_u) { return who_u[0] & 0xffff; }

c3_w_tmp
u3_ship_duke(u3_ship who_u) { return who_u[0] & 0xffffffff; }

c3_d
u3_ship_earl(u3_ship who_u) { return who_u[0]; }
