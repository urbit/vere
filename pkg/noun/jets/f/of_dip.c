/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qfo_dip(u3_noun a,
          u3_noun b)
{
  if ( u3_nul == b ) {
    return u3k(a);
  }
  else {
    u3_noun hed, tal; 
    u3x_cell(b, &hed, &tal);

    u3_noun kid = u3qfa_get(u3t(a), hed);
    if ( u3_nul == kid ) {
      return u3nc(u3_nul, u3_nul);
    }
    __attribute__((musttail)) return u3qfo_dip(u3t(kid), tal);
  }
}

u3_noun
u3wfo_dip(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam, &b, u3x_con_sam, &a, 0);
  return u3qfo_dip(a, b);
}
