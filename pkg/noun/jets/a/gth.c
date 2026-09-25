/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

// @Refcount: direct product
u3_noun
u3qa_gth(u3_atom a, u3_atom b)
{
  return __( 1 == u3r_comp(a, b) );
}

u3_noun
u3wa_gth(u3_noun cor)
{
  u3_noun a, b;

  a = u3h(u3h(u3t(cor)));
  b = u3t(u3h(u3t(cor)));

  if (  (c3n == u3ud(b))
     || (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_gth(a, b);
  }
}

// @Refcount: direct product
u3_noun
u3ka_gth(u3_noun a, u3_noun b)
{
  u3_noun c = u3qa_gth(a, b);
  u3z(a); u3z(b);
  return c;
}
