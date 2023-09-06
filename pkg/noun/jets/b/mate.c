/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_mate(u3_noun a,
            u3_noun b)
  {
    if ( u3_nul == b ) {
      return u3k(a);
    } else if ( u3_nul == a ) {
      return u3k(b);
    } else if ( c3y == u3r_sing(u3t(a), u3t(b)) ) {
      return u3k(a);
    } else {
      return u3m_error("mate");
    }
  }
  u3_noun
  u3wb_mate(u3_noun cor)
  {
    u3_noun a, b;
    u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0);
    return u3qb_mate(a, b);
  }

