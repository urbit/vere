/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <ctype.h>

u3_atom
u3qe_scot(u3_atom a, u3_atom b)
{
  switch (a) {
    case c3_tas(tas): return u3k(b);
    case c3_tas(ud):  return u3s_etch_ud(b);
    case c3_tas(ux):  return u3s_etch_ux(b);
    case c3_tas(uv):  return u3s_etch_uv(b);
    case c3_tas(uw):  return u3s_etch_uw(b);
    default:      return u3_none;
  }
}

u3_noun
u3we_scot(u3_noun cor)
{
  u3_atom a, b;
  u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0);
  return u3qe_scot(u3x_atom(a), u3x_atom(b));
}
