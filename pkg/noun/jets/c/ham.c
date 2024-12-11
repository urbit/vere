/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_atom
u3qc_ham(u3_atom a)
{
  c3_w len_w = u3r_met(5, a);
  c3_d pop_d = 0;
  c3_w wor_w;

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    wor_w  = u3r_word(i_w, a);
    pop_d += c3_pc_w(wor_w);
  }

  return u3i_chub(pop_d);
}

u3_noun
u3wc_ham(u3_noun cor)
{
  return u3qc_ham(u3x_atom(u3h(u3t(cor))));
}
