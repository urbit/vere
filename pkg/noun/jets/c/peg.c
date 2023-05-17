#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_peg(u3_atom a, u3_atom b)
{
  if ( 1 == b ) {
    return u3k(a);
  }

  c3_w  c_w = u3r_met(0, b) - 1;

  if ( (c3y == u3a_is_cat(a) && (c3y == u3a_is_cat(b))) ) {
    c3_w d_w = b - (1 << c_w);
    c3_d e_d = (c3_d)a << c_w;
    return u3i_chub(d_w + e_d);
  }
  else {
    u3_atom d, e, f, g, h;

    d = u3i_word(c_w);
    e = u3qc_lsh(0, d, 1);
    f = u3qa_sub(b, e);
    g = u3qc_lsh(0, d, a);
    h = u3qa_add(f, g);

    u3z(d);
    u3z(e);
    u3z(f);
    u3z(g);

    return h;
  }
}

u3_noun
u3wc_peg(u3_noun cor)
{
  u3_noun a, b;

  if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ||
       (0 == a) ||
       (0 == b) ||
       (c3n == u3ud(b)) ||
       (c3n == u3ud(a) && b != 1) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qc_peg(a, b);
  }
}
