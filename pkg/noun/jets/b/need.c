/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qb_need(u3_noun a)
  {
    if ( 0 == a ) {
      return u3m_bail(c3__exit);
    }
    else {
      return u3k(u3t(a));
    }
  }
  u3_noun
  u3wb_need(u3_noun cor)
  {
    u3_noun a;

    if ( u3_none == (a = u3r_at(u3x_sam, cor)) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qb_need(a);
    }
  }

/* u3ub_need(): array driver of u3wb_need.
*/
u3_noun
u3ub_need(u3_noun* arg)
{
  return u3qb_need(arg[0]);
}

