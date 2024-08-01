/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include <stdio.h>

u3_weak 
u3qf_lte_dime(u3_noun a, u3_noun b)
{
  if ( c3n == u3r_sing(u3h(a), u3h(b)) ) {
    return u3qc_aor(u3h(a), u3h(b));
  }
  if ( (u3h(a) == c3__rd) || (u3h(a) == c3__rs) || (u3h(a) == c3__rq) || (u3h(a) == c3__rh) ) {
    return u3l_punt("lte-dime", u3_none);
  }
  if ( (u3h(a) == 't') || (u3h(a) == c3__ta) || (u3h(a) == c3__tas) ) {
    return u3qc_aor(u3t(a), u3t(b));
  }
  return u3qa_lte(u3t(a), u3t(b));
}

u3_weak u3qf_lte_iota(u3_noun a, u3_noun b)
{
  if ( c3y == u3r_sing(a, b) ) {
    return c3y;
  }
  u3_noun d_a = c3y == u3ud(a) ? u3nc(c3__tas, u3k(a)) : a;
  u3_noun d_b = c3y == u3ud(b) ? u3nc(c3__tas, u3k(b)) : b;
  u3_noun res = u3qf_lte_dime(d_a, d_b);
  if ( c3y == u3ud(a) ) { u3z(d_a); }
  if ( c3y == u3ud(b) ) { u3z(d_b); }
  return res;
}


