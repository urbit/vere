#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_peg(u3_atom a, u3_atom b)
{
  if ( (0 == a) || (0 == b) ) {
    return u3m_bail(c3__exit);
  }
  else if ( 1 == b ) {
    return u3k(a);
  }

  c3_d a_d, b_d;
  c3_w c_w;

  if ( (c3y == u3a_is_cat(a)) && (c3y == u3a_is_cat(b)) ) {
    c_w = c3_bits_word(b) - 1;
    a_d = a;
    b_d = b;
  }
  else {
    c3_w d_w = u3r_met(0, a);
    c3_d e_d;

    c_w = u3r_met(0, b) - 1;
    e_d = (c3_d)c_w + d_w;

    if ( 64 <= e_d ) {
      u3i_slab sab_u;
      u3i_slab_init(&sab_u, 0, e_d);

      u3r_chop(0, 0, c_w,   0, sab_u.buf_w, b);
      u3r_chop(0, 0, d_w, c_w, sab_u.buf_w, a);

      return u3i_slab_moot(&sab_u);
    }

    a_d = u3r_chub(0, a);
    b_d = u3r_chub(0, b);
  }

  b_d  &= ((c3_d)1 << c_w) - 1;
  a_d <<= c_w;
  a_d  ^= b_d;

  return u3i_chub(a_d);
}

u3_noun
u3wc_peg(u3_noun cor)
{
  u3_noun a, b;

  if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ||
       (c3n == u3ud(b)) ||
       (c3n == u3ud(a) && b != 1) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qc_peg(a, b);
  }
}
