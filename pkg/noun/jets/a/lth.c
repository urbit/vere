/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_lth(u3_atom a, u3_atom b)
{
  return __( -1 == u3r_comp(a, b) );
}

u3_noun
u3wa_lth(u3_noun cor)
{
  u3_noun a, b;

  if (  (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0))
     || (c3n == u3ud(b))
     || (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_lth(a, b);
  }
}
