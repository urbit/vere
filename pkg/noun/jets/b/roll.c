/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  u3_noun
  u3qb_roll(u3_noun a,
            u3_noun b)
  {
    u3_noun pro = u3k(u3x_at(u3x_sam_3, b));

    if ( u3_nul != a ) {
      u3j_site sit_u;
      u3_noun  i, t;
      u3j_gate_prep(&sit_u, u3k(b));
      u3k(a);

      do {
        i = u3h(a);

        pro = u3j_gate_slam(&sit_u, u3nc(u3k(i), pro));
        
        t = u3k(u3t(a));
        u3z(a), a = t;
      } while ( u3_nul != a );
      u3j_gate_lose(&sit_u);
    }

    return pro;
  }
  u3_noun
  u3wb_roll(u3_noun cor)
  {
    u3_noun a, b;

    if ( ((u3_none == (a = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
          (u3_none == (b = u3r_tail_weak(u3r_head_weak(u3r_tail(cor)))))) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_roll(a, b);
    }
  }

