/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_cat(u3_atom a,
           u3_atom b,
           u3_atom c)
  {
    if ( !_(u3a_is_cat(a)) || (a >= 32) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_g   a_g = a;
      c3_d   lew_d = u3r_met(a_g, b);
      c3_d   ler_d = u3r_met(a_g, c);
      c3_d   all_d = (lew_d + ler_d);

      if ( 0 == all_d ) {
        return 0;
      }
      else {
        u3i_slab sab_u;
        u3i_slab_from(&sab_u, b, a_g, all_d);

        u3r_chop(a_g, 0, ler_d, lew_d, sab_u.buf_w, c);

        return u3i_slab_mint(&sab_u);
      }
    }
  }

  u3_noun
  u3wc_cat(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a,
                                u3x_sam_6, &b,
                                u3x_sam_7, &c, 0)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_cat(a, b, c);
    }
  }

