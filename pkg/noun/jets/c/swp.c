/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <string.h>

u3_noun
u3qc_swp(u3_atom a,
         u3_atom b)
{
  if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
    return u3m_bail(c3__fail);
  }
  c3_g a_g   = a;
  c3_w len_w = u3r_met(a_g, b);
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, a_g, len_w);

  //  byte-aligned fast path: mmap once, reverse bloqs via memcpy.
  //  the generic path below would invoke u3r_chop per bloq, each of
  //  which calls u3r_blob_load — an O(len_w * bob_size) disaster on
  //  large bob atoms.
  //
  if ( a_g >= 3 ) {
    c3_g shf_g = a_g - 3;
    c3_w blq_b = (c3_w)1 << shf_g;         //  bytes per bloq

    u3r_view vu_u;
    u3r_view_init(&vu_u, b);

    for ( c3_w i = 0; i < len_w; i++ ) {
      c3_w src_b = i * blq_b;
      c3_w dst_b = (len_w - i - 1) * blq_b;
      c3_w cpy_b = blq_b;

      if ( src_b >= vu_u.len_w ) {
        //  past the view — dest bytes stay zero (slab_init)
        continue;
      }
      if ( src_b + cpy_b > vu_u.len_w ) {
        cpy_b = vu_u.len_w - src_b;
      }
      memcpy(sab_u.buf_y + dst_b, vu_u.byt_y + src_b, cpy_b);
    }

    u3r_view_done(&vu_u);
    return u3i_slab_mint(&sab_u);
  }

  //  bit-level fallback — slow for bobs (u3r_chop materializes per
  //  iteration) but correctness is preserved.
  //
  for (c3_w i = 0; i < len_w; i++) {
    u3r_chop(a_g, i, 1, len_w - i - 1, sab_u.buf_w, b);
  }

  return u3i_slab_mint(&sab_u);
}

u3_noun
u3wc_swp(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});

  if (  (c3n == u3ud(a))
     || (c3n == u3ud(b)) )
  {
    return u3m_bail(c3__exit);
  }

  return u3qc_swp(a, b);
 }

u3_noun
u3kc_swp(u3_atom a,
         u3_atom b)
{
  u3_noun pro = u3qc_swp(a, b);
  u3z(a); u3z(b);
  return pro;
}
