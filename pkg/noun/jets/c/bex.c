/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qc_bex(u3_atom a)
{
  c3_d a_d;
  u3i_slab sab_u;

  if ( a < 31 ) {
    return 1U << a;
  }

  if ( c3y == u3a_is_cat(a) ) {
    a_d = a;
  }
  else {
    if ( c3n == u3r_safe_chub(a, &a_d) ) {
      return u3m_bail(c3__fail);
    }

    // We don't currently support atoms 2GB or larger (fails while
    // mugging).  The extra term of 16 is experimentally determined.
    if ( a_d >= ((c3_d)1 << (c3_d)34) - 16 ) {
      u3l_log("bex: overflow");
      return u3m_bail(c3__fail);
    }
  }

  u3i_slab_init(&sab_u, 0, a_d + 1);

  sab_u.buf_w[a_d >> 5] = 1U << (a_d & 31);

  return u3i_slab_moot(&sab_u);
}

u3_noun
u3kc_bex(u3_atom a)
{
  u3_noun b = u3qc_bex(a);
  u3z(a);
  return b;
}

u3_noun
u3wc_bex(u3_noun cor)
{
  u3_noun a = u3x_at(u3x_sam, cor);
  return u3qc_bex(u3x_atom(a));
}
