/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

// @Refcount: transfer arguments, direct product
static u3_noun
_by_any(u3_noun a, u3j_site* sit_u)
{
  if ( u3_nul == a ) {
    return c3n;
  }
  else {
    u3_noun n_a, l_a, r_a, q_n_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_cell(n_a, NULL, &q_n_a);
    u3k(l_a); u3k(r_a); u3k(q_n_a);
    u3z(a);

    switch ( u3j_gate_slam(sit_u, q_n_a) ) {
      case c3y: {
        u3z(l_a); u3z(r_a); 
        return c3y;
      }
      case c3n: break;
      default:  return u3m_bail(c3__exit);
    }

    if ( c3y == _by_any(l_a, sit_u) ) {
      u3z(r_a); 
      return c3y;
    }

    return _by_any(r_a, sit_u);
  }
}

u3_noun
u3qdb_any(u3_noun a, u3_noun b)
{
  u3_noun    pro;
  u3j_site sit_u;

  u3j_gate_prep(&sit_u, u3k(b));
  pro = _by_any(u3k(a), &sit_u);
  u3j_gate_lose(&sit_u);

  return pro;
}

u3_noun
u3wdb_any(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, u3x_sam, &b, u3x_con_sam, &a, 0);
  return u3qdb_any(a, b);
}
