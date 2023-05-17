#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_pin(u3_atom a, u3_atom b)
{
  c3_w a_w, b_w, c_w;

  if ( 1 == a ) return c3y;
  if ( 1 == b ) return c3n;

  a_w = u3r_met(0, a);
  b_w = u3r_met(0, b);

  if ( b_w < a_w ) return c3n;

  c_w = b_w - a_w;

  if ( (c3y == u3a_is_cat(a)) && (c3y == u3a_is_cat(b)) ) {
    return __(a == (b >> (b_w - a_w)));
  }
  else {
    u3_atom c, d, e;

    c = u3i_word(b_w - a_w);
    d = u3qc_rsh(0, c, b);
    e = u3r_sing(a, d);

    u3z(c);
    u3z(d);

    return e;
  }
}

u3_noun
u3wc_pin(u3_noun cor)
{
  u3_noun a, b;

  if (  (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0))
     || (0 == a)
     || (0 == b)
     || (c3n == u3ud(b))
     || (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qc_pin(a, b);
  }
}
