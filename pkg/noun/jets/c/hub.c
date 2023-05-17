#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_hub(u3_atom a, u3_atom b)
{
  c3_w a_w, b_w, c_w;

  if ( 1 == a ) return u3k(b);

  a_w = u3r_met(0, a);
  b_w = u3r_met(0, b);

  if ( b_w < a_w ) return u3m_bail(c3__exit);

  c_w = b_w - a_w;

  if ( (c3y == u3a_is_cat(a)) && (c3y == u3a_is_cat(b)) ) {
    if ( a != (b >> c_w) ) return u3m_bail(c3__exit);

    return b & ((1 << c_w) - 1);
  }
  else {
    u3_atom c, d, e, f, g;

    c = u3i_word(b_w - a_w);
    d = u3qc_rsh(0, c, b);

    if ( c3n == u3r_sing(a, d) ) return u3m_bail(c3__exit);

    e = u3qc_bex(c);
    f = u3qa_dec(e);
    g = u3qc_dis(b, f);

    u3z(c);
    u3z(d);
    u3z(e);
    u3z(f);

    return g;
  }
}

u3_noun
u3wc_hub(u3_noun cor)
{
  u3_noun a, b;
  if (  (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0))
     || (0 == a)
     || (0 == b)
     || (c3n == u3ud(b))
     || (c3n == u3ud(a) && b != 1) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qc_hub(a, b);
  }
}
