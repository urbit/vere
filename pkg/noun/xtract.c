/// @file

#include "xtract.h"

#include "manage.h"
#include "retrieve.h"

u3_atom
u3x_atom(u3_noun a);
u3_noun
u3x_good(u3_weak som);

/* u3x_mean():
**
**   Attempt to deconstruct `a` by axis, noun pairs; 0 terminates.
**   Axes must be sorted in tree order.
*/
void
u3x_mean(u3_noun som, ...)
{
  c3_o    ret_o;
  va_list ap;

  va_start(ap, som);
  ret_o = u3r_vmean(som, ap);
  va_end(ap);

  if ( c3n == ret_o ) {
    u3m_bail(c3__exit);
  }
}
