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
  c3_n  tel_n = u3r_met(3, tail);
  c3_n hed_n;
  if ( c3n == u3r_safe_note(head, &hed_n) ) {
    return u3m_bail(c3__fail);
  }
  c3_y* input;

  if (c3y == u3a_is_cat(tail)) {
    input = (c3_y*)&tail;
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(tail);
    // XX: little endian
    input = (c3_y*)vat_u->buf_n;
  }

  if ( tel_n > hed_n ) {
    return u3m_error("subtract-underflow");
  }
  
  c3_n led_n = hed_n - tel_n;
  c3_n crc_n = 0;

  crc_n = crc32(crc_n, input, tel_n);

  while ( led_n > 0 ) {
    c3_y byt_y = 0;
    crc_n = crc32(crc_n, &byt_y, 1);
    led_n--;
  }

  return u3i_note(crc_n);
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
