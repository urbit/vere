/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_pow(u3_atom a,
           u3_atom b)
  {
    if ( !_(u3a_is_cat(b)) ) {
      return u3m_bail(c3__fail);
    }
    else {
      mpz_t a_mp;

      u3r_mp(a_mp, a);
      mpz_pow_ui(a_mp, a_mp, b);

      return u3i_mp(a_mp);
    }
  }
  u3_noun
  u3wc_pow(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_pow(a, b);
    }
  }

