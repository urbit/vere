/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  RETAINS lit
//
static u3_noun
_silt_fast(u3_noun lit)
{
  if ( u3_nul == lit ) return u3_nul;

  u3_noun root = u3h(lit);
  u3_noun rest = u3t(lit);
  while ( u3_nul != rest ) {
    u3_noun h = u3h(rest);
    rest = u3t(rest);
    if ( !_(u3qc_mor(root, h)) ) {
      root = h;
    }
  }
  u3_noun part_l = u3_nul, part_r = u3_nul;
  rest = lit;
  while ( u3_nul != rest ) {
    u3_noun h = u3h(rest);
    rest = u3t(rest);
    if ( c3y == u3r_sing(h, root) ) continue;
    if ( _(u3qc_gor(h, root)) ) {
      part_l = u3nc(u3k(h), part_l);
    }
    else {
      part_r = u3nc(u3k(h), part_r);
    }
  }
  u3_noun l = _silt_fast(part_l);
  u3_noun r = _silt_fast(part_r);
  u3_noun out = u3nt(u3k(root), l, r);
  u3z(part_l); u3z(part_r);
  return out;
}

u3_noun
u3qdi_gas(u3_noun a,
          u3_noun b)
{
  if ( u3_nul == b ) {
    return u3k(a);
  }
  else {
    if ( u3_nul == a ) {
      return _silt_fast(b);
    }
    u3_noun i_b, t_b;
    u3x_cell(b, &i_b, &t_b);

    u3_noun c = u3qdi_put(a, i_b);
    u3_noun d = u3qdi_gas(c, t_b);
    u3z(c);
    return d;
  }
}

u3_noun
u3wdi_gas(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam, &b, u3x_con_sam, &a, 0);
  return u3qdi_gas(a, b);
}

u3_noun
u3kdi_gas(u3_noun a,
          u3_noun b)
{
  u3_noun c = u3qdi_gas(a, b);
  u3z(a); u3z(b);
  return c;
}
