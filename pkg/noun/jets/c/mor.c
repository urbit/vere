/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_mor(u3_noun a,
           u3_noun b)
  {
    c3_w_tmp c_w = u3r_mug(u3r_mug(a));
    c3_w_tmp d_w = u3r_mug(u3r_mug(b));

    if ( c_w == d_w ) {
      return u3qc_dor(a, b);
    }
    else return (c_w < d_w) ? c3y : c3n;
  }
  u3_noun
  u3wc_mor(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_baad(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_mor(a, b);
    }
  }
