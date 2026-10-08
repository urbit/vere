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
  else {
    c3_h len_h = (c3_h)len;
    c3_d key_d = u3r_met_d(3, key);

    //  NB: this condition is implicit in the pad subtraction
    //
    if ( key_d > len_h ) {
      return u3m_bail(c3__exit);
    }
    else {
      c3_h        key_h = (c3_h)key_d;
      c3_h        sed_h = u3r_half(0, sed);
      c3_o        loc_o = c3n;
      const c3_y* key_y = 0;
      u3r_view    vue_u = {0};
      c3_h        out_h;

      //  if we're hashing more bytes than we have, allocate and copy
      //  to ensure trailing null bytes; otherwise a view borrows the
      //  bytes wherever the atom lives (XX assumes little-endian)
      //
      if ( len_h > key_h ) {
        c3_y* buf_y = u3a_calloc(sizeof(c3_y), len_h);
        u3r_bytes(0, len_h, buf_y, key);
        key_y = buf_y;
        loc_o = c3y;
      }
      else if ( len_h > 0 ) {
        u3r_view_open(&vue_u, key, c3y);
        key_y = u3r_view_bytes(&vue_u);
      }

      MurmurHash3_x86_32(key_y, len_h, sed_h, &out_h);

      if ( c3y == loc_o ) {
        u3a_free((void*)key_y);
      }
      u3r_view_done(&vue_u);

      return u3i_halfs(1, &out_h);
    }
  }
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
