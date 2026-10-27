/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_clap(u3_noun a,
            u3_noun b,
            u3_noun c)
  {
    if ( 0 == a ) {
      return u3k(b);
    }
    else if ( 0 == b ) {
      return u3k(a);
    }
    else {
      return u3nc(0, u3n_slam_on(u3k(c), u3nc(u3k(u3t(a)), u3k(u3t(b)))));
    }
  }
  u3_noun
  u3wb_clap(u3_noun cor)
  {
    u3_noun a, b, c;

    a = u3h(u3h(u3t(cor)));
    b = u3h(u3t(u3h(u3t(cor))));
    c = u3t(u3t(u3h(u3t(cor))));
    return u3qb_clap(a, b, c);
  }
