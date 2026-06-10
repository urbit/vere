/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  RETAINS lit
//
static u3_noun
_malt_fast(u3_noun lit)
{
  if ( u3_nul == lit ) return u3_nul;

  u3_noun root = u3h(lit);
  u3_noun rest = u3t(lit);
  while ( u3_nul != rest ) {
    u3_noun h = u3h(rest);
    rest = u3t(rest);
    //  this will pick last pair, given equal keys
    if ( _(u3qc_mor(u3h(h), u3h(root))) ) {
      root = h;
    }
  }
  u3_noun part_l, part_r;
  u3_noun *cur_l = &part_l, *cur_r = &part_r, *old, *item;
  rest = lit;
  while ( u3_nul != rest ) {
    u3_noun h = u3h(rest);
    rest = u3t(rest);
    if ( c3y == u3r_sing(u3h(h), u3h(root)) ) continue;
    if ( _(u3qc_gor(u3h(h), u3h(root))) ) {
      old = cur_l;
      *old = u3i_defcons(&item, &cur_l);
      *item = u3k(h);
    }
    else {
      old = cur_r;
      *old = u3i_defcons(&item, &cur_r);
      *item = u3k(h);
    }
  }
  *cur_l = u3_nul;
  *cur_r = u3_nul;

  u3_noun l = _malt_fast(part_l);
  u3_noun r = _malt_fast(part_r);
  u3_noun out = u3nt(u3k(root), l, r);
  u3z(part_l); u3z(part_r);
  return out;
}

u3_noun
u3qdb_gas(u3_noun a,
          u3_noun b)
{
  if ( u3_nul == b ) {
    return u3k(a);
  }
  else {
    if ( u3_nul == a ) {
      return _malt_fast(b);
    }
    u3_noun i_b,  t_b,
            pi_b, qi_b;
    u3x_cell(b, &i_b, &t_b);
    u3x_cell(i_b, &pi_b, &qi_b);

    u3_noun c = u3qdb_put(a, pi_b, qi_b);
    u3_noun d = u3qdb_gas(c, t_b);
    u3z(c);
    return d;
  }
}

u3_noun
u3wdb_gas(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam, &b, u3x_con_sam, &a, 0);
  return u3qdb_gas(a, b);
}

u3_noun
u3kdb_gas(u3_noun a,
          u3_noun b)
{
  u3_noun c = u3qdb_gas(a, b);
  u3z(a); u3z(b);
  return c;
}
