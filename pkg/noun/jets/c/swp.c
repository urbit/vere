/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_swp(u3_atom a,
         u3_atom b)
{
  c3_w len_w = u3r_met(a, b);
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, a, len_w);

  for (c3_w i = 0; i < len_w; i++) {
    u3r_chop(a, i, 1, len_w - i - 1, sab_u.buf_w, b);
  }
         
  return u3i_slab_mint(&sab_u);
}

u3_noun
u3wc_swp(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0);

  if (  (c3n == u3ud(a))
     || (c3n == u3ud(b)) )
  {
    return u3m_bail(c3__exit);
  }

  return u3qc_swp(a, b);
 }

u3_noun
u3kc_swp(u3_atom a,
         u3_atom b)
{
  u3_noun pro = u3qc_swp(a, b);
  u3z(a); u3z(b);
  return pro;
}
