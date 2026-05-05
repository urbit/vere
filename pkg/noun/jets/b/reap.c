/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_reap(u3_atom a,
            u3_noun b)
  {
    if ( !_(u3a_is_cat(a)) ) {
      return u3m_bail(c3__fail);
    }
    else {
      u3_noun acc = u3_nul;
      c3_w i_w = a;

      while ( i_w ) {
        acc = u3nc(u3k(b), acc);
        i_w--;
      }

      return acc;
    }
  }

  u3_noun
  u3wb_reap(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
         (u3_none == (b = u3r_tail_weak(u3r_head_weak(u3r_tail(cor))))) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_reap(a, b);
    }
  }
