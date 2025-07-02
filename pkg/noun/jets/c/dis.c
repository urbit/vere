/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_dis(u3_atom a,
           u3_atom b)
  {
    c3_n lna_w = u3r_met(u3a_note_bits_log, a);
    c3_n lnb_w = u3r_met(u3a_note_bits_log, b);

    if ( (lna_w == 0) && (lnb_w == 0) ) {
      return 0;
    }
    else {
      c3_n     len_w = c3_max(lna_w, lnb_w);
      c3_n       i_w;
      u3i_slab sab_u;
      u3i_slab_from(&sab_u, a, u3a_note_bits_log, len_w);

      for ( i_w = 0; i_w < len_w; i_w++ ) {
        sab_u.buf_n[i_w] &= u3r_note(i_w, b);
      }

      return u3i_slab_mint(&sab_u);
    }
  }
  u3_noun
  u3wc_dis(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, u3_nul)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_dis(a, b);
    }
  }

