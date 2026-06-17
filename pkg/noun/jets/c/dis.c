/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_dis(u3_atom a,
           u3_atom b)
  {
    c3_d lna_d = u3r_met(5, a);
    c3_d lnb_d = u3r_met(5, b);
    c3_w lna_w, lnb_w;

    if ( UINT32_MAX < lna_d ) {
      u3m_bail(c3__fail);
    }
    if ( UINT32_MAX < lnb_d ) {
      u3m_bail(c3__fail);
    }
    lna_w = (c3_w)lna_d;
    lnb_w = (c3_w)lnb_d;

    if ( (lna_w == 0) && (lnb_w == 0) ) {
      return 0;
    }
    else {
      c3_w     len_w = c3_max(lna_w, lnb_w);
      c3_w       i_w;
      u3i_slab sab_u;
      u3i_slab_from(&sab_u, a, 5, len_w);

      for ( i_w = 0; i_w < len_w; i_w++ ) {
        sab_u.buf_w[i_w] &= u3r_word(i_w, b);
      }

      return u3i_slab_mint(&sab_u);
    }
  }
  u3_noun
  u3wc_dis(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_dis(a, b);
    }
  }

