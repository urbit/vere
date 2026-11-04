/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "nock-compile.h"
#include "noun.h"

/* _mice(): virtual nock through fun_f, with the toon of a crash.
*/
static u3_noun
_mice(u3_noun cor, u3_funq fun_f)
{
  u3_noun bus, fol, som;

  bus = u3h(u3h(u3t(cor)));
  fol = u3t(u3h(u3t(cor)));
  som = u3m_soft_cax(fun_f, u3k(bus), u3k(fol));

  if ( 2 == u3h(som) ) {
    u3_noun tan = u3t_mook(u3k(u3t(som)));
    u3z(som);
    return u3nc(2, tan);
  }

  return som;
}

/* u3we_mice(): from u3n, in u3n.
*/
u3_noun
u3we_mice(u3_noun cor)
{
  return _mice(cor, u3n_nock_on);
}

/* u3ue_mice(): from compiled nock, in compiled nock.
*/
u3_noun
u3ue_mice(u3_noun cor)
{
  return _mice(cor, u3nc_nock_on);
}
