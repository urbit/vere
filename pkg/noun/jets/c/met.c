/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_met(u3_atom a,
           u3_atom b)
  {
    if ( 0 == b ) {
      return 0;
    }
    else if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
      return u3m_bail(c3__fail);;
    }
    else {
      return u3i_chub(u3r_met(a, b));
    }
  }
  u3_noun
  u3wc_met(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( (c3n == u3ud(b)) ||
         (c3n == u3ud(a) && 0 != b) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_met(a, b);
    }
  }

