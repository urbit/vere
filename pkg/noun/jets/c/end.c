/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <string.h>

u3_noun
u3qc_end(u3_atom a,
         u3_atom b,
         u3_atom c)
{
  if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
    return u3m_bail(c3__fail);
  }
  else if ( !_(u3a_is_cat(b)) ) {
    return u3k(c);
  }

  c3_g a_g   = a;
  c3_w b_w   = b;
  c3_w len_w = u3r_met(a_g, c);

  if ( 0 == b_w ) {
    return 0;
  }
  if ( b_w >= len_w ) {
    return u3k(c);
  }

  //  bob: read only the requested leading bytes, straight from the blob
  //  into the result.  equivalent to cut(a_g, 0, b_w, c).
  //
  if ( (a_g >= 3) && (c3y == u3a_is_bob(c)) ) {
    u3_weak pro = u3r_blob_cut(a_g, 0, b_w, c);
    if ( u3_none != pro ) {
      return pro;
    }
    //  open failed — fall through
  }

  u3i_slab sab_u;
  u3i_slab_init(&sab_u, a_g, b_w);
  u3r_chop(a_g, 0, b_w, 0, sab_u.buf_w, c);
  return u3i_slab_mint(&sab_u);
}

u3_noun
u3wc_end(u3_noun cor)
{
  u3_atom bloq, step;
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a},
                {u3x_sam_3, &b});
  u3x_bite(a, &bloq, &step);

  return u3qc_end(bloq, step, u3x_atom(b));
}
