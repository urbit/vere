/// @file

#include <stdio.h>
#include <allocate.h>
#include "zlib.h"

#include "jets/w.h"

#include "noun.h"

u3_noun
u3qe_crc32(u3_noun input_octs)
{
  u3_atom tail = u3t(input_octs);
  c3_w  len_w = u3r_met(3, tail);
  c3_y* input;

  if (c3y == u3a_is_cat(tail)) {
    input = &tail;
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(tail);
    input = (c3_y*)vat_u->buf_w;
  }

  u3_atom head = u3h(input_octs);
  c3_w leading_zeros = head - len_w;
  c3_w crc = 0L;

  while (leading_zeros > 0) {
    c3_y lz_input = 0;
    crc = crc32(crc, &lz_input, 1);
    leading_zeros--;
  }

  crc = crc32(crc, input, len_w);
  return u3i_word(crc);
}

u3_noun
u3we_crc32(u3_noun cor)
{
  u3_noun a = u3r_at(u3x_sam, cor);

  if ( (u3du(a) == c3y) && (u3ud(u3h(a))) == c3y && (u3ud(u3t(a))) == c3y) {
    return u3qe_crc32(a);
  }
  else {
    return u3m_bail(c3__exit);
  }
}
