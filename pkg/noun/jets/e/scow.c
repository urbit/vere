/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <ctype.h>

u3_atom
u3qe_scow(u3_atom a, u3_atom b)
{
  u3_weak dat = u3qe_scot(a, b);
  u3_weak pro = u3_none;

  if ( u3_none != dat ) {
    pro = u3qc_rip(3, 1, dat);
    u3z(dat);
  }

  return pro;
}

u3_noun
u3we_scow(u3_noun cor)
{
  u3_atom a, b;
  u3x_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, u3_nul);
  return u3qe_scow(u3x_atom(a), u3x_atom(b));
}
