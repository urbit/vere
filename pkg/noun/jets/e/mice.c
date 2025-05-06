/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* variant of u3we_mink() / u3m_soft_run(). caching, no scry.
*/
u3_noun
u3we_mice(u3_noun cor) {
  u3_noun bus, fol;

  if ( c3n == u3r_mean(cor, u3x_sam_2, &bus,
                            u3x_sam_3, &fol,
                            0) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3m_soft_cax(u3n_nock_on, u3k(bus), u3k(fol));
  }
}
