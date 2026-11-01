/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* u3we_monk(): virtual nock, dropping the trace of a crash.
*/
u3_noun
u3we_monk(u3_noun cor)
{
  u3_noun bus, fol, gul, som;

  bus = u3h(u3h(u3h(u3t(cor))));
  fol = u3t(u3h(u3h(u3t(cor))));
  gul = u3t(u3h(u3t(cor)));

  som = u3n_nock_et(u3k(gul), u3k(bus), u3k(fol));

  if ( 2 == u3h(som) ) {
    u3z(som);
    return u3nc(2, u3_nul);
  }

  return som;
}
