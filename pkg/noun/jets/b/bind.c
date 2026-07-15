/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_bind(u3_noun a,
            u3_noun b)
  {
    if ( 0 == a ) {
      return 0;
    } else {
      return u3nc(0, u3n_slam_on(u3k(b), u3k(u3t(a))));
    }
  }
  u3_noun
  u3wb_bind(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));
    return u3qb_bind(a, b);
  }

