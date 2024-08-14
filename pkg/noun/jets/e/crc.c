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
    input = (c3_y*)&tail;
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(tail);
    input = (c3_y*)vat_u->buf_w;
  }

  c3_w p_octs_w;

  if (c3n == u3r_safe_word(u3h(input_octs), &p_octs_w)) {
    return u3_none;
  }

  c3_w lead_w = p_octs_w - len_w;
  c3_w crc = 0L;

  crc = crc32(crc, input, len_w);

  c3_w zero_w = 0;
  while (lead_w-- > 0) {
    crc = crc32(crc, &zero_w, 1);
  }

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
