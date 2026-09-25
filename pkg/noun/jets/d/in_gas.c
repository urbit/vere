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

  u3_noun root = u3k(u3h(lit));
  u3_noun i, temp, rest = u3k(u3t(lit));
  while ( u3_nul != rest ) {
    i    = u3k(u3h(rest));

    temp = u3k(u3t(rest));
    u3z(rest);
    rest = temp;

    if ( !_(u3qc_mor(root, i)) ) {
      u3z(root);
      root = i;
    }
    else {
      u3z(i);
    }
  }

  u3_noun part_l = u3_nul, part_r = u3_nul;
  rest = u3k(lit);
  while ( u3_nul != rest ) {
    i    = u3k(u3h(rest));

    temp = u3k(u3t(rest));
    u3z(rest);
    rest = temp;
    
    if ( c3y == u3r_sing(i, root) ) {
      u3z(i);
    }
    else if ( _(u3qc_gor(i, root)) ) {
      part_l = u3nc(i, part_l);
    }
    else {
      part_r = u3nc(i, part_r);
    }
  }
  u3_noun l = _silt_fast(part_l);
  u3_noun r = _silt_fast(part_r);
  u3_noun out = u3nt(root, l, r);
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
  u3x_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a});
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
