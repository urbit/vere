/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_gth(u3_atom a, u3_atom b)
{
  if (c3y == u3a_is_cat(a) || c3y == u3a_is_cat(b))
  {
    return __( a > b );
  }

  if (a == b) return c3n;

  u3a_atom* a_u = u3a_to_ptr(a);
  u3a_atom* b_u = u3a_to_ptr(b);

  if (a_u->len_n != b_u->len_n)
  {
    return __( a_u->len_n > b_u->len_n );
  }

  c3_w_tmp* a_w = a_u->buf_w;
  c3_w_tmp* b_w = b_u->buf_w;
  for (c3_w_tmp i_w = a_u->len_n; i_w--;)
  {
    if (a_w[i_w] > b_w[i_w]) return c3y;
    if (a_w[i_w] < b_w[i_w]) return c3n;
  }

  return c3n;
}

u3_noun
u3wa_gth(u3_noun cor)
{
  u3_noun a, b;

  if (  (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0))
     || (c3n == u3ud(b))
     || (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_gth(a, b);
  }
}

u3_noun
u3ka_gth(u3_noun a, u3_noun b)
{
  u3_noun c = u3qa_gth(a, b);
  u3z(a); u3z(b);
  return c;
}
