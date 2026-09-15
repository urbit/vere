/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qb_skid(u3_noun a, u3_noun b)
{
  u3_noun l, r;
  u3_noun* lef = &l;
  u3_noun* rig = &r;

  if ( u3_nul != a) {
    u3_noun   i, t;
    u3_noun*   hed;
    u3_noun*   tel;
    u3j_site sit_u;
    u3j_gate_prep(&sit_u, u3k(b));
    u3k(a);

    do {
      i = u3k(u3k(u3h(a)));

      if ( c3y == u3x_loob(u3j_gate_slam(&sit_u, i)) ) {
        *lef = u3i_defcons(&hed, &tel);
        *hed = i;
        lef  = tel;
      }
      else {
        *rig = u3i_defcons(&hed, &tel);
        *hed = i;
        rig  = tel;
      }
      
      t = u3k(u3t(a));
      u3z(a), a = t;
    }
    while ( u3_nul != a );

    u3j_gate_lose(&sit_u);
  }

  *lef = u3_nul;
  *rig = u3_nul;

  return u3nc(l, r);
}

u3_noun
u3wb_skid(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});
  return u3qb_skid(a, b);
}
