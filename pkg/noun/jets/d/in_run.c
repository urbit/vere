/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  [a] and [out] are TRANSFERRED
//
static void
_in_run(u3_noun a, u3j_site* sit_u, u3_noun* out)
{
  if ( u3_nul == a ) {
    return;
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3x_trel(a, &n_a, &l_a, &r_a);

    u3k(n_a);
    u3k(l_a);
    u3k(r_a);

    {
      u3_noun new = u3j_gate_slam(sit_u, n_a);
      u3_noun pro = u3qdi_put(*out, new);

      u3z(new);
      u3z(*out);
      *out = pro;
    }

    _in_run(l_a, sit_u, out);
    _in_run(r_a, sit_u, out);

    u3z(a);
  }
}

u3_noun
u3qdi_run(u3_noun a, u3_noun b)
{
  u3_noun    out = u3_nul;
  u3j_site sit_u;

  u3j_gate_prep(&sit_u, u3k(b));
  _in_run(u3k(a), &sit_u, &out);
  u3j_gate_lose(&sit_u);

  return out;
}

u3_noun
u3wdi_run(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam, &b, u3x_con_sam, &a, 0);
  return u3qdi_run(a, b);
}
