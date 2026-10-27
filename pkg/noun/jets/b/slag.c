/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_slag(u3_atom a, u3_noun b)
  {
    if ( u3_nul == b ) {
      return u3_nul;
    }
    else if ( !_(u3a_is_cat(a)) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_w len_w = a;

      while ( len_w ) {
        if ( c3n == u3du(b) ) {
          return u3_nul;
        }
        b = u3t(b);
        len_w--;
      }
      return u3k(b);
    }
  }
  u3_noun
  u3wb_slag(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( (c3n == u3ud(a) && u3_nul != b) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_slag(a, b);
    }
  }
