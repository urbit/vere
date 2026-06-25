/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_mor(u3_noun a,
           u3_noun b)
  {
    c3_w c_w = u3r_mug(u3r_mug(a));
    c3_w d_w = u3r_mug(u3r_mug(b));

    if ( c_w == d_w ) {
      return u3qc_dor(a, b);
    }
    else return (c_w < d_w) ? c3y : c3n;
  }
  u3_noun
  u3wc_mor(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
         (u3_none == (b = u3r_tail_weak(u3r_head_weak(u3r_tail(cor))))) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_mor(a, b);
    }
  }
