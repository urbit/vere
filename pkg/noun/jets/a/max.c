/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qa_max(u3_atom a, u3_atom b)
{
  if ( _(u3a_is_cat(a)) && _(u3a_is_cat(b)) ) {
    return u3k(c3_max(a, b));
  }
  else {
    c3_w a_w = u3r_met(0, a);
    c3_w b_w = u3r_met(0, b);

    if ( a_w != b_w ) {
      return u3k((a_w > b_w) ? a : b);
    }
    else {
      mpz_t   a_mp, b_mp;
      u3_noun max;

      u3r_mp(a_mp, a);
      u3r_mp(b_mp, b);

      max = (mpz_cmp(a_mp, b_mp) > 0) ? a : b;

      mpz_clear(a_mp);
      mpz_clear(b_mp);

      return u3k(max);
    }
  }
}

u3_noun
u3wa_max(u3_noun cor)
{
  u3_noun a, b;

  if (  (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0))
     || (c3n == u3ud(b) && 0 != a)
     || (c3n == u3ud(a) && 0 != b) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qa_max(a, b);
  }
}

u3_noun
u3ka_max(u3_noun a, u3_noun b)
{
  u3_noun c = u3qa_max(a, b);
  u3z(a); u3z(b);
  return c;
}
