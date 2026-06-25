/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <string.h>

u3_noun
u3qc_rsh(u3_atom a,
         u3_atom b,
         u3_atom c)
{
  if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
    return u3m_bail(c3__fail);
  }
  else if ( !_(u3a_is_cat(b)) ) {
    return 0;
  }

  c3_g a_g   = a;
  c3_w b_w   = b;
  c3_w len_w = u3r_met(a_g, c);

  if ( b_w >= len_w ) {
    return 0;
  }

  c3_w wid_w = len_w - b_w;

  //  bob-aware fast path for byte-aligned suffixes: mmap the blob
  //  and memcpy only the requested bytes starting at offset b_w.
  //  equivalent to cut(a_g, b_w, wid_w, c).
  //
  if ( (a_g >= 3) && (c3y == u3a_is_bob(c)) ) {
    c3_d        map_d = 0;
    const c3_y* map_y = u3r_blob_map(c, &map_d);

    if ( map_y ) {
      c3_g shf_g = a_g - 3;
      c3_d off_d = (c3_d)b_w  << shf_g;
      c3_d byt_d = (c3_d)wid_w << shf_g;

      c3_d cpy_d = byt_d;
      if ( off_d >= map_d ) {
        cpy_d = 0;
      }
      else if ( off_d + cpy_d > map_d ) {
        cpy_d = map_d - off_d;
      }

      u3i_slab sab_u;
      u3i_slab_init(&sab_u, a_g, wid_w);

      if ( cpy_d ) {
        memcpy(sab_u.buf_y, map_y + off_d, (size_t)cpy_d);
      }

      u3r_blob_unmap(map_y, map_d);
      return u3i_slab_mint(&sab_u);
    }
    //  mmap failed — fall through
  }

  u3i_slab sab_u;
  u3i_slab_init(&sab_u, a_g, wid_w);
  u3r_chop(a_g, b_w, wid_w, 0, sab_u.buf_w, c);
  return u3i_slab_mint(&sab_u);
}

u3_noun
u3wc_rsh(u3_noun cor)
{
  u3_atom bloq, step;
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a},
                {u3x_sam_3, &b});
  u3x_bite(a, &bloq, &step);

  return u3qc_rsh(bloq, step, u3x_atom(b));
}

u3_noun
u3kc_rsh(u3_noun a,
         u3_noun b,
         u3_noun c)
{
  u3_noun d = u3qc_rsh(a, b, c);

  u3z(a); u3z(b); u3z(c);
  return d;
}
