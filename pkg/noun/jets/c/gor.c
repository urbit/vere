/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_gor(u3_noun a,
           u3_noun b)
  {
    c3_h c_h = u3r_mug(a);
    c3_h d_h = u3r_mug(b);

    if ( c_h == d_h ) {
      return u3qc_dor(a, b);
    }
    else return (c_h < d_h) ? c3y : c3n;
  }
  u3_noun
  u3wc_gor(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));
    return u3qc_gor(a, b);
  }

