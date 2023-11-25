/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qf_cell(u3_noun hed,
            u3_noun tal)
  {
    if ( (c3_tas(void) == hed) || (c3_tas(void) == tal) ) {
      return c3_tas(void);
    } else {
      return u3nt(c3_tas(cell), u3k(hed), u3k(tal));
    }
  }
  u3_noun
  u3wf_cell(u3_noun cor)
  {
    u3_noun hed, tal;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &hed, u3x_sam_3, &tal, 0) ) {
      return u3m_bail(c3_tas(fail));
    } else {
      return u3qf_cell(hed, tal);
    }
  }
