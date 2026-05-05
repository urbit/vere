/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_met(u3_atom a,
           u3_atom b)
  {
    if ( 0 == b ) {
      return 0;
    }
    else if ( !_(u3a_is_cat(a)) || (a >= 32) ) {
      return u3m_bail(c3__fail);;
    }
    else {
      c3_w met_w = u3r_met(a, b);

      if ( !_(u3a_is_cat(met_w)) ) {
        return u3i_words(1, &met_w);
      }
      else return met_w;
    }
  }
  u3_noun
  u3wc_met(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
         (u3_none == (b = u3r_tail_weak(u3r_head_weak(u3r_tail(cor))))) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(a) && 0 != b) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_met(a, b);
    }
  }

