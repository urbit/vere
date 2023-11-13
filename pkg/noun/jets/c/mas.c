#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_mas(u3_atom a)
{
  c3_w b_w;

  if ( c3y == u3a_is_cat(a) ) {
    b_w = c3_bits_word(a);

    if ( 2 > b_w ) {
      return u3m_bail(c3__exit);
    }
    else {
      a  &= ~((c3_w)1 << (b_w - 1));
      a  |=  ((c3_w)1 << (b_w - 2));
      return a;
    }
  }
  else {
    b_w = u3r_met(0, a);

    if ( 64 > b_w ) {
      c3_d a_d = u3r_chub(0, a);
      a_d  &= ~((c3_d)1 << (b_w - 1));
      a_d  |=  ((c3_d)1 << (b_w - 2));
      return u3i_chub(a_d);
    }
    else {
      u3i_slab sab_u;
      u3i_slab_from(&sab_u, a, 0, b_w - 1);

      b_w -= 2;
      sab_u.buf_w[(b_w >> 5)] |= ((c3_w)1 << (b_w & 31));

      return u3i_slab_mint(&sab_u);
    }
  }
}

u3_noun
u3wc_mas(u3_noun cor)
{
  return u3qc_mas(u3x_atom(u3x_at(u3x_sam, cor)));
}
