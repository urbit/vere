/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_gte(u3_noun a, u3_noun b)
{
  if ( _(u3a_is_cat(a)) && _(u3a_is_cat(b)) ) {
    return __(a >= b);
  }
  else if ( 0 == a ) {
    return c3n;
  }
  else if ( 0 == b ) {
    return c3y;
  }
  else if ( !_(u3ud(a)) || !_(u3ud(b)) ) {
    if ( _(u3r_sing(a, b)) ) return c3y;
    else return u3m_bail(c3__fail);
  }
  else {
    c3_w a_w = u3r_met(0, a);
    c3_w b_w = u3r_met(0, b);

    if ( a_w != b_w ) {
      return __(a_w >= b_w);
    }
    else {
      mpz_t   a_mp, b_mp;
      u3_noun cmp;

      u3r_mp(a_mp, a);
      u3r_mp(b_mp, b);

      cmp = (mpz_cmp(a_mp, b_mp) >= 0) ? c3y : c3n;

      mpz_clear(a_mp);
      mpz_clear(b_mp);

      return cmp;
    }
  }
}

u3_noun
u3wa_gte(u3_noun cor)
{
  u3_noun a, b;

  u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0);
  return u3qa_gte(a, b);
}
