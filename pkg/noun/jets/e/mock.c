/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* u3we_mock(): virtual nock, rendering the trace of a crash.
*/
u3_noun
u3we_mock(u3_noun cor)
{
  u3_noun bus, fol, gul, som;

  bus = u3h(u3h(u3h(u3t(cor))));
  fol = u3t(u3h(u3h(u3t(cor))));
  gul = u3t(u3h(u3t(cor)));

  som = u3n_nock_et(u3k(gul), u3k(bus), u3k(fol));

  if ( 2 == u3h(som) ) {
    u3_noun tan = u3t_mook(u3k(u3t(som)));
    u3z(som);
    return u3nc(2, tan);
  }

  return som;
}
