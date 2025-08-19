/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_cap(u3_atom a)
{
  c3_n met_n = u3r_met(0, a);

  if ( 2 > met_n ) {
    return u3m_bail(c3__exit);
  }
  else {
    return 2 + u3r_bit((met_n - 2), a);
  }
}

u3_noun
u3wc_cap(u3_noun cor)
{
  return u3qc_cap(u3x_atom(u3x_at(u3x_sam, cor)));
}
