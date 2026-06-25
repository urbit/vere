/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qf_fine(u3_noun fuv,
            u3_noun lup,
            u3_noun mar)
  {
    if ( (c3__void == lup) || (c3__void == mar) ) {
      return c3__void;
    } else {
      return u3nq(c3__fine,
                  u3k(fuv),
                  u3k(lup),
                  u3k(mar));
    }
  }
  u3_noun
  u3wf_fine(u3_noun cor)
  {
    u3_noun fuv, lup, mar;

    if ( ((u3_none == (fuv = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
          (u3_none == (lup = u3r_head_weak(u3r_tail_weak(u3r_head_weak(u3r_tail(cor)))))) ||
          (u3_none == (mar = u3r_tail_weak(u3r_tail_weak(u3r_head_weak(u3r_tail(cor))))))) ) {
      return u3m_bail(c3__fail);
    } else {
      return u3qf_fine(fuv, lup, mar);
    }
  }
