/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  u3_noun
  u3qb_lien(u3_noun a,
            u3_noun b)
  {
    u3_noun pro = c3n;
    c3_o loz_o;

    if ( u3_nul != a ) {
      u3j_site sit_u;
      u3_noun  i, t;
      u3j_gate_prep(&sit_u, u3k(b));
      u3k(a);

      do {
        i = u3h(a);

        loz_o = u3x_loob(u3j_gate_slam(&sit_u, u3k(i)));
        if ( c3y == loz_o ) {
          pro = c3y;
          break;
        }
        
        t = u3k(u3t(a));
        u3z(a), a = t;
      } while ( u3_nul != a );
      
      u3z(a);
      u3j_gate_lose(&sit_u);
    }

    return pro;
  }

  u3_noun
  u3wb_lien(u3_noun cor)
  {
    u3_noun a, b;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_lien(a, b);
    }
  }
