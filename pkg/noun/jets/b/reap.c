/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_reap(u3_atom a,
            u3_noun b)
  {
    if ( !_(u3a_is_cat(a)) ) {
      return u3m_bail(c3__fail);
    }
    else {
      u3_noun acc = u3_nul;
      c3_w i_w = a;

      while ( i_w ) {
        acc = u3nc(u3k(b), acc);
        i_w--;
      }

      return acc;
    }
  }

  u3_noun
  u3wb_reap(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_reap(a, b);
    }
  }

/* u3ub_reap(): array driver of u3wb_reap.
*/
u3_noun
u3ub_reap(u3_noun* arg)
{
  if ( (c3n == u3ud(arg[0])) ) {
    return u3m_bail(c3__exit);
  }
  return u3qb_reap(arg[0], arg[1]);
}
