/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_atom
u3qa_add(u3_atom a,
         u3_atom b)
{
  if ( _(u3a_is_cat(a)) && _(u3a_is_cat(b)) ) {
    c3_w c = a + b;

    return u3i_words(1, &c);
  }
  else if ( 0 == a ) {
    return u3k(b);
  }
  else {
    mpz_t a_mp, b_mp;

    u3r_mp(a_mp, a);
    u3r_mp(b_mp, b);

    mpz_add(a_mp, a_mp, b_mp);
    mpz_clear(b_mp);

    return u3i_mp(a_mp);
  }
}

u3_weak
u3wa_add(u3_noun cor)
{
  u3_noun a, b;

  u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0);
  if ( !_(u3ud(a)) || !_(u3ud(b)) ) {
    return u3_none;
  }
  else {
    return u3qa_add(a, b);
  }
}

u3_noun
u3ka_add(u3_noun a,
         u3_noun b)
{
  u3_noun c = u3qa_add(a, b);

  u3z(a); u3z(b);
  return c;
}
