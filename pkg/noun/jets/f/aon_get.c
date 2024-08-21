/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qfa_get(u3_noun a,
          u3_noun b)
{
  if ( u3_nul == a ) {
    return u3_nul;
  }
  else {
    u3_noun n_a, lr_a;
    u3_noun keyn_a, valn_a;
    u3x_cell(a, &n_a, &lr_a);
    u3x_cell(n_a, &keyn_a, &valn_a);

    if ( (c3y == u3r_sing(b, keyn_a)) ) {
      return u3nc(u3_nul, u3k(valn_a));
    } else if ( c3y == u3qf_lte_iota(b, keyn_a) ) {
      __attribute__((musttail)) return u3qfa_get(u3h(lr_a), b) ;
    } else {
      __attribute__((musttail)) return u3qfa_get(u3t(lr_a), b);
    }
  }
}

u3_noun
u3wfa_get(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam, &b, u3x_con_sam, &a, 0);
  return u3qfa_get(a, b);
}

u3_weak
u3kfa_get(u3_noun a,
          u3_noun b)
{
  u3_noun c = u3qfa_get(a, b);
  u3z(a); u3z(b);

  if ( c3n == u3du(c) ) {
    u3z(c);
    return u3_none;
  }
  else {
    u3_noun pro = u3k(u3t(c));
    u3z(c);
    return pro;
  }
}

