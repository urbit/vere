/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_atom
u3qc_ctz(u3_atom a)
{
  c3_w wor_w, i_w = 0;

  if ( 0 == a ) {
    return 0;
  }

  do {
    wor_w = u3r_word(i_w++, a);
  }
  while ( !wor_w );

  {
    c3_w bit_d = i_w - 1;
    bit_d *= 32;
    bit_d += c3_tz_w(wor_w);
    return u3i_chub(bit_d);
  }
}

u3_noun
u3wc_ctz(u3_noun cor)
{
  return u3qc_ctz(u3x_atom(u3h(u3t(cor))));
}
