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
    else if ( !_(u3a_is_cat(a)) || (a >= u3a_note_bits) ) {
      return 1;
    }
    else {
      c3_n met_n = u3r_met(a, b);

      if ( !_(u3a_is_cat(met_n)) ) {
        return u3i_note(met_n);
      }
      else return met_n;
    }
  }
  u3_noun
  u3wc_met(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, u3_nul)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(a) && 0 != b) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_met(a, b);
    }
  }

