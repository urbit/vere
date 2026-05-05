/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qf_cell(u3_noun hed,
            u3_noun tal)
  {
    if ( (c3__void == hed) || (c3__void == tal) ) {
      return c3__void;
    } else {
      return u3nt(c3__cell, u3k(hed), u3k(tal));
    }
  }
  u3_noun
  u3wf_cell(u3_noun cor)
  {
    u3_noun hed, tal;

    if ( ((u3_none == (hed = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
          (u3_none == (tal = u3r_tail_weak(u3r_head_weak(u3r_tail(cor)))))) ) {
      return u3m_bail(c3__fail);
    } else {
      return u3qf_cell(hed, tal);
    }
  }
