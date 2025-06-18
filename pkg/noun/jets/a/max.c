/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_max(u3_atom a, u3_atom b)
{
  if ( _(u3a_is_cat(a)) || _(u3a_is_cat(b)) )
  {
    return u3k(c3_max(a, b));
  }

  if (a == b) return u3k(a);

  u3a_atom* a_u = u3a_to_ptr(a);
  u3a_atom* b_u = u3a_to_ptr(b);

  if (a_u->len_w != b_u->len_w)
  {
    return (a_u->len_w > b_u->len_w) ? u3k(a) : u3k(b);
  }

  c3_w* a_w = a_u->buf_w;
  c3_w* b_w = b_u->buf_w;
  for (c3_w i_w = a_u->len_w; i_w--;)
  {
    if (a_w[i_w] > b_w[i_w]) return u3k(a);
    if (a_w[i_w] < b_w[i_w]) return u3k(b);
  }

  return u3k(a);
}

u3_noun
u3wa_max(u3_noun cor)
{
  u3_noun a, b;

  if (  (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0))
     || (c3n == u3ud(b))
     || (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_max(a, b);
  }
}

u3_noun
u3ka_max(u3_noun a, u3_noun b)
{
  u3_noun c = u3qa_max(a, b);
  u3z(a); u3z(b);
  return c;
}
