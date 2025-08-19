/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_snag(u3_atom a,
            u3_noun b)
  {
    if ( !_(u3a_is_cat(a)) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_n len_n = a;

      while ( len_n ) {
        if ( c3n == u3du(b) ) {
          return u3m_bail(c3__exit);
        }
        b = u3t(b);
        len_n--;
      }
      if ( c3n == u3du(b) ) {
        return u3m_bail(c3__exit);
      }
      return u3k(u3h(b));
    }
  }
  u3_noun
  u3wb_snag(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, u3_nul)) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_snag(a, b);
    }
  }
