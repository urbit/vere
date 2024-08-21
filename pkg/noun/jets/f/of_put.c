/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qfo_put(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  u3_noun fil, kid;
  u3x_cell(a, &fil, &kid);                 
  if ( u3_nul == b ) {
    return u3nc(u3nc(u3_nul, u3k(c)), u3k(kid));
  }
  u3_noun hed, tal;
  u3x_cell(b, &hed, &tal);
  u3_noun nex;
  { 
     u3_noun has = u3qfa_get(kid, hed);
    if ( has == u3_nul )  {
      nex = u3nc(u3_nul, u3_nul);
    } else {
      nex = u3k(u3t(has));
      //u3z(has);
    }
  }
  u3_noun cod = u3qfo_put(nex, tal, c);
  u3_noun kad = u3qfa_put(kid, hed, cod);

  u3_noun res = u3nc(u3k(fil), kad);
  //  u3z(nex);
  return res;
}

u3_noun
u3wfo_put(u3_noun cor)
{
  u3_noun a, b, c;
  u3x_mean(cor, u3x_sam_2, &b, u3x_sam_3, &c, u3x_con_sam, &a, 0);
  return u3qfo_put(a, b, c);
}
