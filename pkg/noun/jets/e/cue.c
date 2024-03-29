/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qe_cue(u3_atom a)
{
  return u3s_cue_atom(a);
}

u3_noun
u3we_cue(u3_noun cor)
{
  return u3qe_cue(u3x_at(u3x_sam, cor));
}

u3_noun
u3ke_cue(u3_atom a)
{
  u3_noun b = u3qe_cue(a);
  u3z(a);
  return b;
}
