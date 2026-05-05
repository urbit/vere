/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qf_help(u3_noun sag,
            u3_noun tip)
  {
    if ( c3__void == tip ) {
      return c3__void;
    }
    else return u3nt(c3__help,
                     u3k(sag),
                     u3k(tip));
  }
  u3_noun
  u3wf_help(u3_noun cor)
  {
    u3_noun sag, tip;

    if ( ((u3_none == (sag = u3r_head_weak(u3r_head_weak(u3r_tail(cor))))) ||
          (u3_none == (tip = u3r_tail_weak(u3r_head_weak(u3r_tail(cor)))))) ) {
      return u3m_bail(c3__fail);
    } else {
      return u3qf_help(sag, tip);
    }
  }

