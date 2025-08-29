/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qb_skim(u3_noun a, u3_noun b)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  if ( u3_nul != a) {
    u3_noun   i, t;
    u3_noun*   hed;
    u3_noun*   tel;
    u3j_site sit_u;
    u3j_gate_prep(&sit_u, u3k(b));
    u3k(a);

    do {
      i = u3h(a);

      switch ( u3j_gate_slam(&sit_u, u3k(i)) ) {
        case c3y: {
          *lit = u3i_defcons(&hed, &tel);
          *hed = u3k(i);
          lit  = tel;
        } break;

        case c3n: break;

        default: u3m_bail(c3__exit);
      }

      t = u3k(u3t(a));
      u3z(a), a = t;
    }
    while ( u3_nul != a );

    u3j_gate_lose(&sit_u);
  }

  *lit = u3_nul;

  return pro;
}

u3_noun
u3wb_skim(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0);
  return u3qb_skim(a, b);
}
