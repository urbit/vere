/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static u3_noun
_tap_in(u3_noun a,
        u3_noun b)
{
  if ( u3_nul == a ) {
    return b;
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3x_trel(a, &n_a, &l_a, &r_a);

    __attribute__((musttail)) return _tap_in(r_a,
                   u3nc(u3k(n_a),
                   _tap_in(l_a, b)));
  }
}

u3_noun
u3qfa_tap(u3_noun a)
{
  return _tap_in(a, u3_nul);
}

u3_noun
u3wfa_tap(u3_noun cor)
{
  u3_noun a;
  u3x_mean(cor, u3x_con_sam, &a, 0);
  return u3qfa_tap(a);
}

u3_noun
u3kfa_tap(u3_noun a)
{
  u3_noun b = u3qfa_tap(a);
  u3z(a);
  return b;
}
