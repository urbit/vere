/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "nock-compile.h"
#include "noun.h"

/* _monk(): virtual nock through fun_f, with the raw trace of a crash.
*/
static u3_noun
_monk(u3_noun cor, u3_funq fun_f)
{
  u3_noun bus, fol, gul, som;

  bus = u3h(u3h(u3h(u3t(cor))));
  fol = u3t(u3h(u3h(u3t(cor))));
  gul = u3t(u3h(u3t(cor)));

  som = u3m_soft_run(u3k(gul), fun_f, u3k(bus), u3k(fol));

  if ( 2 == u3h(som) ) {
    u3z(som);
    return u3nc(2, u3_nul);
  }

  return som;
}

/* u3we_monk(): from u3n, in u3n.
*/
u3_noun
u3we_monk(u3_noun cor)
{
  return _monk(cor, u3n_nock_on);
}

/* u3ue_monk(): from compiled nock, in compiled nock.
*/
u3_noun
u3ue_monk(u3_noun cor)
{
  return _monk(cor, u3nc_nock_on);
}
