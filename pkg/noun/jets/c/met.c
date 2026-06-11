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
      return u3m_bail(c3__fail);
    }
    //  blob atoms: compute from u3r_blob_met (returns c3_d) to avoid
    //  c3_w truncation in u3r_met on 32-bit builds.
    //
    else if ( (c3y == u3a_is_bob(b)) && (3 <= a) ) {
      c3_d bit_d = u3r_blob_met(b);
      if ( 0 == bit_d ) return (u3_noun)u3m_bail(c3__fail);
      c3_d rnd_d = (c3_d)((1 << a) - 1);
      return u3i_chub((bit_d + rnd_d) >> a);
    }
    else {
      c3_w met_w = u3r_met(a, b);

      if ( !_(u3a_is_cat(met_w)) ) {
        return u3i_word(met_w);
      }
      else return met_w;
    }
  }
  u3_noun
  u3wc_met(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b})) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(a) && 0 != b) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_met(a, b);
    }
  }

