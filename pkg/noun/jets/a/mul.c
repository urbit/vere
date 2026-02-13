/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_mul(u3_atom a,
         u3_atom b)
{
#ifndef VERE64
  if ( _(u3a_is_cat(a)) && _(u3a_is_cat(b)) ) {
#else
  c3_g bit_g = c3_bits_chub(a) + c3_bits_chub(b);
  if (bit_g <= 64) {
#endif
    c3_d c = ((c3_d) a) * ((c3_d) b);
#ifdef VERE64
    bit_g = bit_g - c3_bits_chub(c);
    if (1 < bit_g) goto gmp_mul;
#endif
    return u3i_chub(c);
  }
  else if ( 0 == a ) {
    return 0;
  }
  else {
#ifdef VERE64
    gmp_mul:
#endif
    mpz_t a_mp, b_mp;

    u3r_mp(a_mp, a);
    u3r_mp(b_mp, b);

    mpz_mul(a_mp, a_mp, b_mp);
    mpz_clear(b_mp);

    return u3i_mp(a_mp);
  }
}

u3_noun
u3wa_mul(u3_noun cor)
{
  u3_noun a, b;

  if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, u3_nul)) ||
       (c3n == u3ud(a)) ||
       (c3n == u3ud(b)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_mul(a, b);
  }
}

u3_noun
u3ka_mul(u3_noun a,
         u3_noun b)
{
  u3_noun c = u3qa_mul(a, b);

  u3z(a); u3z(b);
  return c;
}
