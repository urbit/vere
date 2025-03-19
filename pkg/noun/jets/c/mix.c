/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_mix(u3_atom a,
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

      //  XX use u3r_chop for XOR?
      //
      for ( i_w = 0; i_w < lnb_w; i_w++ ) {
        sab_u.buf_n[i_w] ^= u3r_note(i_w, b);
      }

      return u3i_slab_mint(&sab_u);
    }
  }
  u3_noun
  u3wc_mix(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_baad(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_mix(a, b);
    }
  }
  u3_noun
  u3kc_mix(u3_atom a,
           u3_atom b)
  {
    u3_noun res = u3qc_mix(a, b);
    u3z(a); u3z(b);
    return res;
  }
