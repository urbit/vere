/// @file

#include "xtract.h"

#include "manage.h"
#include "retrieve.h"

u3_atom
u3x_atom(u3_noun a);
u3_noun
u3x_good(u3_weak som);
c3_o
u3x_loob(u3_noun a);

/* u3x_mean():
**
**   Attempt to deconstruct `a` by axis, noun pairs.
**   Axes must be sorted in tree order.
*/
void
u3x_vmean(u3_noun a, u3r_mean_pair pairs[], c3_z len_z)
{
  if ( c3n == u3r_vmean(a, pairs, len_z) ) {
    u3m_bail(c3__exit);
  }
}
