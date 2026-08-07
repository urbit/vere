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
  c3_w  tel_w = u3r_met(3, tail);
  c3_w hed_w;
  if ( c3n == u3r_safe_word(head, &hed_w) ) {
    return u3m_bail(c3__fail);
  }

  if ( tel_w > hed_w ) {
    return u3m_error("subtract-underflow");
  }

  //  zero-copy view of the atom's significant bytes (mmap for bob).
  //  NB: the legacy vat_u->buf_w path returned seq_w for bob atoms;
  //  u3r_view gives the actual file bytes.
  //
  u3r_view vue_u;
  u3r_view_init(&vue_u, tail);

  c3_w led_w = hed_w - tel_w;
  c3_w crc_w = crc32(0, vue_u.byt_y, tel_w);

  u3r_view_done(&vue_u);

  while ( led_w > 0 ) {
    c3_y byt_y = 0;
    crc_w = crc32(crc_w, &byt_y, 1);
    led_w--;
  }

  return u3i_word(crc_w);
}

u3_noun
u3we_crc32(u3_noun cor)
{
  u3_noun a = u3r_at(u3x_sam, cor);

  if ( (u3du(a) == c3y) && (u3ud(u3h(a)) == c3y) && (u3ud(u3t(a)) == c3y) ) {
    return u3qe_crc32(a);
  } else {
    return u3m_bail(c3__exit);
  }
}
