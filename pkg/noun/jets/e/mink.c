/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  u3_noun
  u3we_mink(u3_noun cor)
  {
    u3_noun bus, fol, gul;

    if ( ((u3_none == (bus = u3r_head_weak(u3r_head_weak(u3r_head_weak(u3r_tail(cor)))))) ||
          (u3_none == (fol = u3r_tail_weak(u3r_head_weak(u3r_head_weak(u3r_tail(cor)))))) ||
          (u3_none == (gul = u3r_tail_weak(u3r_head_weak(u3r_tail(cor)))))) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      u3_noun som;

      som = u3n_nock_et(u3k(gul), u3k(bus), u3k(fol));

      return som;
    }
  }
