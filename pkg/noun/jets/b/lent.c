/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

STATIC_ASSERT( (UINT32_MAX > u3a_cells),
               "length precision" );

u3_noun
u3qb_lent(u3_noun a)
{
  c3_n len_w = 0;

  while ( u3_nul != a ) {
    a = u3t(a);
    len_w++;
  }

  return u3i_note(len_w);
}

u3_noun
u3wb_lent(u3_noun cor)
{
  return u3qb_lent(u3x_at(u3x_sam, cor));
}

u3_noun
u3kb_lent(u3_noun a)
{
  u3_noun b = u3qb_lent(a);
  u3z(a);
  return b;
}
