/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "murmur3.h"

// XX: murmur3 only 32 bit lengths...
u3_noun
u3qc_muk(u3_atom sed,
         u3_atom len,
         u3_atom key)
{
  //if ( c3n == u3a_is_cat(len) ) {
  if ( len > u3a_direct_max_h ) {
    return u3m_bail(c3__fail);
  }

  c3_h len_h = (c3_h)len;
  c3_h key_h = u3r_met(3, key);

  //  NB: this condition is implicit in the pad subtraction
  //
  if ( key_h > len_h ) {
    return u3m_bail(c3__exit);
  }

  c3_h sed_h = u3r_half(0, sed);
  c3_h out_h;

  //  u3r_view_padded gives us len_h bytes — mmap-backed for bobs
  //  (previously would have returned seq_w bytes via the direct
  //  buf_w pointer and produced wrong hashes), heap-backed with
  //  zero-padding for atoms shorter than len_h.
  //
  u3r_view vue_u;
  u3r_view_padded(&vue_u, key, len_h);
  MurmurHash3_x86_32((c3_y*)vue_u.byt_y, len_h, sed_h, &out_h);
  u3r_view_done(&vue_u);

  return u3i_halfs(1, &out_h);
}

u3_noun
u3wc_muk(u3_noun cor)
{
  u3_noun sed, len, key;
  u3x_mean(cor, {u3x_sam_2, &sed},
                {u3x_sam_6, &len},
                {u3x_sam_7, &key});

  if (  (c3n == u3ud(sed))
     || (c3n == u3ud(len))
     || (c3n == u3ud(key)) )
  {
    return u3m_bail(c3__exit);
  }
  else {
    return u3qc_muk(sed, len, key);
  }
}
