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

    if ( ((u3_none == (a = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
          (u3_none == (b = u3r_head_weak(u3r_tail_weak(u3r_head_weak(u3r_tail(cor)))))) ||
          (u3_none == (c = u3r_tail_weak(u3r_tail_weak(u3r_head_weak(u3r_tail(cor))))))) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_clap(a, b, c);
    }
  }
