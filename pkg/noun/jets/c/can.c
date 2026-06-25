/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <string.h>

  u3_noun
  u3qc_can(u3_atom a,
           u3_noun b)
  {
    if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_g       a_g = a;
      c3_w     tot_w = 0;
      u3i_slab sab_u;

      /* Measure and validate the slab required.
      */
      {
        u3_noun cab = b;

        while ( 1 ) {
          u3_noun i_cab, pi_cab, qi_cab;

          if ( 0 == cab ) {
            break;
          }
          if ( c3n == u3du(cab) ) return u3m_bail(c3__fail);
          i_cab = u3h(cab);
          if ( c3n == u3du(i_cab) ) return u3m_bail(c3__fail);
          pi_cab = u3h(i_cab);
          qi_cab = u3t(i_cab);
          if ( c3n == u3a_is_cat(pi_cab) ) return u3m_bail(c3__fail);
          if ( c3n == u3ud(qi_cab) )  return u3m_bail(c3__fail);
          if ( (tot_w + pi_cab) < tot_w ) return u3m_bail(c3__fail);

          tot_w += pi_cab;
          cab = u3t(cab);
        }

        if ( 0 == tot_w ) {
          return 0;
        }

        u3i_slab_init(&sab_u, a_g, tot_w);
      }

      /* Chop the list atoms in.  For byte-aligned bloqs and bob
         atoms, mmap the blob and memcpy directly — avoids a full
         u3r_blob_load per item.
      */
      {
        u3_noun cab = b;
        c3_w    pos_w = 0;
        c3_g    shf_g = (a_g >= 3) ? (a_g - 3) : 0;

        while ( 0 != cab ) {
          u3_noun i_cab  = u3h(cab);
          u3_atom pi_cab = u3h(i_cab);
          u3_atom qi_cab = u3t(i_cab);

          if ( a_g >= 3 ) {
            c3_w pos_b = pos_w << shf_g;
            c3_w len_b = ((c3_w)pi_cab) << shf_g;

            u3r_view vue_u;
            u3r_view_init(&vue_u, qi_cab);
            c3_w cpy_w = (vue_u.len_w < len_b) ? vue_u.len_w : len_b;
            if ( cpy_w ) {
              memcpy(sab_u.buf_y + pos_b, vue_u.byt_y, cpy_w);
            }
            u3r_view_done(&vue_u);
          }
          else {
            u3r_chop(a_g, 0, pi_cab, pos_w, sab_u.buf_w, qi_cab);
          }

          pos_w += pi_cab;
          cab = u3t(cab);
        }
      }

      return u3i_slab_mint(&sab_u);
    }
  }
  u3_noun
  u3wc_can(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b})) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__fail);
    } else {
      return u3qc_can(a, b);
    }
  }

