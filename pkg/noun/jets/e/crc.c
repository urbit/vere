/// @file

#include <stdio.h>
#include <allocate.h>
#include "zlib.h"

#include "jets/w.h"

#include "noun.h"

u3_noun
u3qe_crc32(u3_noun input_octs)
{
  u3_atom head = u3h(input_octs);
  u3_atom tail = u3t(input_octs);

  c3_w hed_w;
  if ( c3n == u3r_safe_word(head, &hed_w) ) {
    return u3m_bail(c3__fail);
  }

  //  a view borrows the bytes wherever the atom lives; read-only
  //
  u3r_view vue_u;
  u3r_view_open(&vue_u, tail, c3y);
  if ( vue_u.byt_d > c3_w_max ) {
    return u3m_bail(c3__fail);
  }
  c3_w        tel_w = (c3_w)vue_u.byt_d;
  const c3_y* input = u3r_view_bytes(&vue_u);

  if ( tel_w > hed_w ) {
    u3r_view_done(&vue_u);
    return u3m_error("subtract-underflow");
  }

  c3_w led_w = hed_w - tel_w;
  c3_w crc_w = 0;

  //  zlib takes a uInt length: feed the bytes in pieces it can count
  //
  for ( c3_w off_w = 0; off_w < tel_w; ) {
    c3_w cnk_w = c3_min(tel_w - off_w, (c3_w)1 << 30);
    crc_w  = crc32(crc_w, input + off_w, (uInt)cnk_w);
    off_w += cnk_w;
  }

  while ( led_w > 0 ) {
    c3_y byt_y = 0;
    crc_w = crc32(crc_w, &byt_y, 1);
    led_w--;
  }

  u3r_view_done(&vue_u);
  return u3i_word(crc_w);
}

u3_noun
u3we_crc32(u3_noun cor)
{
  u3_noun a = u3r_at(u3x_sam, cor);

  if ( (u3_none != a)
  && (u3du(a) == c3y)
  && (u3ud(u3h(a)) == c3y)
  && (u3ud(u3t(a)) == c3y) ) {
    return u3qe_crc32(a);
  } else {
    return u3m_bail(c3__exit);
  }
}
