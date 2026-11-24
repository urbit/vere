/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_atom
u3qc_ham(u3_atom a)
{
  c3_w        len_w;
  c3_d        pop_d = 0;
  u3r_view    vue_u;
  const c3_w* a_w;

  //  one view for the loop: a bob maps once, not per word
  //
  u3r_view_open(&vue_u, a, 0, c3y);
  a_w = u3r_view_words(&vue_u, &len_w, c3y);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    pop_d += c3_pc_w(a_w[i_w]);
  }
  u3r_view_done(&vue_u);

  return u3i_chub(pop_d);
}

u3_noun
u3wc_ham(u3_noun cor)
{
  return u3qc_ham(u3x_atom(u3h(u3t(cor))));
}
