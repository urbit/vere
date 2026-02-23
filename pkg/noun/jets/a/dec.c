/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_inc(u3_atom a)
{
  return u3i_vint(u3k(a));
}

u3_noun
u3qa_dec(u3_atom a)
{
  if ( 0 == a ) {
    return u3m_error("decrement-underflow");
  }
  else {
    return u3qa_sub(a, 1);
  }
}

u3_noun
u3wa_dec(u3_noun cor)
{
  u3_noun a;

  if ( (u3_none == (a = u3r_at(u3x_sam, cor))) ||
       (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_dec(a);
  }
}

u3_noun
u3ka_dec(u3_atom a)
{
  u3_noun b = u3qa_dec(a);
  u3z(a);
  return b;
}
