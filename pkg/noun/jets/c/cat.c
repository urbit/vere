/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <string.h>

  u3_noun
  u3qc_cat(u3_atom a,
           u3_atom b,
           u3_atom c)
  {
    if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
      return u3m_bail(c3__fail);
    }

    c3_g a_g   = a;
    c3_w lew_w = u3r_met(a_g, b);
    c3_w ler_w = u3r_met(a_g, c);
    c3_w all_w = (lew_w + ler_w);

    if ( 0 == all_w ) {
      return 0;
    }

    //  byte-aligned fast path: mmap each bob input directly.  the
    //  legacy u3i_slab_from + u3r_chop pair would otherwise materialize
    //  either operand if it's a bob (via u3r_words → u3r_blob_load and
    //  u3r_chop → u3r_blob_load respectively).
    //
    if ( a_g >= 3 ) {
      c3_g shf_g = a_g - 3;
      c3_w lew_b = lew_w << shf_g;
      c3_w ler_b = ler_w << shf_g;

      u3i_slab sab_u;
      u3i_slab_init(&sab_u, a_g, all_w);

      u3r_view vb_u, vc_u;
      u3r_view_init(&vb_u, b);
      u3r_view_init(&vc_u, c);

      c3_w cpy_w;
      cpy_w = (vb_u.len_w < lew_b) ? vb_u.len_w : lew_b;
      if ( cpy_w ) {
        memcpy(sab_u.buf_y, vb_u.byt_y, cpy_w);
      }
      cpy_w = (vc_u.len_w < ler_b) ? vc_u.len_w : ler_b;
      if ( cpy_w ) {
        memcpy(sab_u.buf_y + lew_b, vc_u.byt_y, cpy_w);
      }

      u3r_view_done(&vb_u);
      u3r_view_done(&vc_u);
      return u3i_slab_mint(&sab_u);
    }

    //  bit-level fallback — existing path materializes for bobs
    //
    u3i_slab sab_u;
    u3i_slab_from(&sab_u, b, a_g, all_w);
    u3r_chop(a_g, 0, ler_w, lew_w, sab_u.buf_w, c);
    return u3i_slab_mint(&sab_u);
  }

  u3_noun
  u3wc_cat(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( (c3n == u3r_mean(cor, {u3x_sam_2, &a},
                                {u3x_sam_6, &b},
                                {u3x_sam_7, &c})) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_cat(a, b, c);
    }
  }

