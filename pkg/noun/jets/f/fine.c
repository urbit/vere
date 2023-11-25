/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qf_fine(u3_noun fuv,
            u3_noun lup,
            u3_noun mar)
  {
    if ( (c3_tas(void) == lup) || (c3_tas(void) == mar) ) {
      return c3_tas(void);
    } else {
      return u3nq(c3_tas(fine),
                  u3k(fuv),
                  u3k(lup),
                  u3k(mar));
    }
  }
  u3_noun
  u3wf_fine(u3_noun cor)
  {
    u3_noun fuv, lup, mar;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &fuv,
                              u3x_sam_6, &lup,
                              u3x_sam_7, &mar, 0) ) {
      return u3m_bail(c3_tas(fail));
    } else {
      return u3qf_fine(fuv, lup, mar);
    }
  }
