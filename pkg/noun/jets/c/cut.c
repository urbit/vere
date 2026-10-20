/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_cut(u3_atom a,
           u3_atom b,
           u3_atom c,
           u3_atom d)
  {
    c3_w b_w, c_w;
    if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
      return u3m_bail(c3__fail);
    }

    //  a bob cut on a byte boundary: chub offsets, since a blob can run
    //  past a word of bits, and a copy out of a view
    //
    if ( (a >= 3) && (c3y == u3a_is_bob(d)) ) {
      c3_g shf_g = (c3_g)(a - 3);
      c3_d b_d, c_d;

      if (  (c3n == u3r_safe_chub(b, &b_d))
         || (c3n == u3r_safe_chub(c, &c_d)) )
      {
        return u3m_bail(c3__fail);
      }

      u3r_view vue_u;
      u3r_view_open(&vue_u, d, c3y);

      c3_d max_d = vue_u.byt_d >> shf_g;   //  whole bloqs, a floor
      c3_d off_d = b_d << shf_g;
      c3_d cnt_d;

      if ( !c_d || (b_d > max_d) || (off_d >= vue_u.byt_d) ) {
        u3r_view_done(&vue_u);
        return 0;
      }
      cnt_d = vue_u.byt_d - off_d;
      if ( c_d <= ((vue_u.byt_d - off_d) >> shf_g) ) {
        cnt_d = c_d << shf_g;
      }

      if ( !off_d && (cnt_d == vue_u.byt_d) ) {
        u3r_view_done(&vue_u);
        return u3k(d);
      }
      if ( cnt_d > c3_w_max ) {
        u3r_view_done(&vue_u);
        return u3m_bail(c3__fail);
      }

      {
        u3i_slab sab_u;
        u3i_slab_init(&sab_u, 3, cnt_d);
        u3r_view_copy(&vue_u, off_d, sab_u.buf_y, (c3_z)cnt_d);
        u3r_view_done(&vue_u);
        return u3i_slab_mint(&sab_u);
      }
    }

    if ( !_(u3r_safe_word(b, &b_w)) ) {
      return u3m_bail(c3__fail);
    }
    if ( !_(u3r_safe_word(c, &c_w)) ) {
      return u3m_bail(c3__fail);
    }

    {
      c3_g a_g   = a;
      c3_w len_w = u3r_met(a_g, d);

      if ( (0 == c_w) || (b_w >= len_w) ) {
        return 0;
      }
      if ( b_w + c_w > len_w ) {
        c_w = (len_w - b_w);
      }
      if ( (b_w == 0) && (c_w == len_w) ) {
        return u3k(d);
      }
      else {
        u3i_slab sab_u;
        u3i_slab_init(&sab_u, a_g, c_w);

        u3r_chop(a_g, b_w, c_w, 0, sab_u.buf_w, d);

        return u3i_slab_mint(&sab_u);
      }
    }
  }
  u3_noun
  u3wc_cut(u3_noun cor)
  {
    u3_noun a, b, c, d;

    a = u3h(u3h(u3t(cor)));
    b = u3h(u3h(u3t(u3h(u3t(cor)))));
    c = u3t(u3h(u3t(u3h(u3t(cor)))));
    d = u3t(u3t(u3h(u3t(cor))));

    if ( (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) ||
         (c3n == u3ud(d)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_cut(a, b, c, d);
    }
  }

