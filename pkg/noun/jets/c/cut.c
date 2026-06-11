/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <string.h>

  u3_noun
  u3qc_cut(u3_atom a,
           u3_atom b,
           u3_atom c,
           u3_atom d)
  {
    if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
      return u3m_bail(c3__fail);
    }

    c3_g a_g = a;

    //  blob fast path: uses c3_d offsets so files > 4GB work on 32-bit.
    //  must come before u3r_safe_word which would bail on large offsets.
    //
    if ( (a_g >= 3) && (c3y == u3a_is_bob(d)) ) {
      c3_d b_d, c_d;
      if ( c3n == u3r_safe_chub(b, &b_d) ) return u3m_bail(c3__fail);
      if ( c3n == u3r_safe_chub(c, &c_d) ) return u3m_bail(c3__fail);
      if ( 0 == c_d ) return 0;

      c3_d        map_d = 0;
      const c3_y* map_y = u3r_blob_map(d, &map_d);

      if ( map_y ) {
        c3_g shf_g = a_g - 3;
        c3_d off_d = b_d << shf_g;
        c3_d byt_d = c_d << shf_g;

        c3_d cpy_d = byt_d;
        if ( off_d >= map_d ) {
          cpy_d = 0;
        }
        else if ( off_d + cpy_d > map_d ) {
          cpy_d = map_d - off_d;
        }

        //  c_d must fit in c3_w for slab_init (max 4GB slab per cut)
        //
        if ( c_d > (c3_d)c3_w_max ) {
          u3r_blob_unmap(map_y, map_d);
          return u3m_bail(c3__fail);
        }

        u3i_slab sab_u;
        u3i_slab_init(&sab_u, a_g, (c3_w)c_d);

        if ( cpy_d ) {
          memcpy(sab_u.buf_y, map_y + off_d, (size_t)cpy_d);
        }

        u3r_blob_unmap(map_y, map_d);
        return u3i_slab_mint(&sab_u);
      }
      //  mmap failed — fall through to generic path
    }

    //  non-blob path: uses c3_w (offsets must fit in 32 bits)
    //
    {
      c3_w b_w, c_w;
      if ( !_(u3r_safe_word(b, &b_w)) ) {
        return u3m_bail(c3__fail);
      }
      if ( !_(u3r_safe_word(c, &c_w)) ) {
        return u3m_bail(c3__fail);
      }

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

      u3i_slab sab_u;
      u3i_slab_init(&sab_u, a_g, c_w);

      u3r_chop(a_g, b_w, c_w, 0, sab_u.buf_w, d);

      return u3i_slab_mint(&sab_u);
    }
  }
  u3_noun
  u3wc_cut(u3_noun cor)
  {
    u3_noun a, b, c, d;

    if ( (c3n == u3r_mean(cor, {u3x_sam_2,  &a},
                                {u3x_sam_12, &b},
                                {u3x_sam_13, &c},
                                {u3x_sam_7,  &d})) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) ||
         (c3n == u3ud(d)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_cut(a, b, c, d);
    }
  }

