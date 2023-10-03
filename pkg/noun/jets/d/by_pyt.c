/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

typedef struct {
  c3_o mut;
  u3_noun d;
} pyt_inner_ret;

pyt_inner_ret
u3qdb_pyt_inner(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  if ( u3_nul == a ) {
    return ( pyt_inner_ret ) { c3y, u3nt(u3nc(u3k(b), u3k(c)),
                u3_nul,
                u3_nul) };
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3_noun pn_a, qn_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_cell(n_a, &pn_a, &qn_a);

    if ( c3y == u3r_sing(pn_a, b) ) {
      if ( c3y == u3r_sing(qn_a, c) ) {
        return ( pyt_inner_ret ) { c3n, u3_nul } ;
      }
      else {
        return ( pyt_inner_ret ) { c3y, u3nt(u3nc(u3k(b), u3k(c)),
                    u3k(l_a),
                    u3k(r_a))};
      }
    }
    else {
      pyt_inner_ret d_ret;

      if ( c3y == u3qc_gor(b, pn_a) ) {
        d_ret = u3qdb_pyt_inner(l_a, b, c);

        if ( c3n == d_ret.mut ) {
            return d_ret;
        }

        if ( c3y == u3qc_mor(pn_a, u3h(u3h(d_ret.d))) ) {
          d_ret.d = u3nt(u3k(n_a),
                      d_ret.d,
                      u3k(r_a));
          return d_ret;
        }
        else {
          u3_noun n_d, l_d, r_d;
          u3r_trel(d_ret.d, &n_d, &l_d, &r_d);

          u3_noun e = u3nt(u3k(n_d),
                           u3k(l_d),
                           u3nt(u3k(n_a),
                                u3k(r_d),
                                u3k(r_a)));

          u3z(d_ret.d);
          d_ret.d = e;
          return d_ret;
        }
      }
      else {
        d_ret = u3qdb_pyt_inner(r_a, b, c);

        if ( c3n == d_ret.mut ) {
            return d_ret;
        }

        if ( c3y == u3qc_mor(pn_a, u3h(u3h(d_ret.d))) ) {
            d_ret.d = u3nt(u3k(n_a),
                      u3k(l_a),
                      d_ret.d);
            return d_ret;
        }
        else {
          u3_noun n_d, l_d, r_d;
          u3r_trel(d_ret.d, &n_d, &l_d, &r_d);

          u3_noun e = u3nt(u3k(n_d),
                           u3nt(u3k(n_a),
                                u3k(l_a),
                                u3k(l_d)),
                            u3k(r_d));

          u3z(d_ret.d);
          d_ret.d = e;
          return d_ret;
        }
      }
    }
  }
}

u3_noun
u3qdb_pyt(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  pyt_inner_ret d_ret = u3qdb_pyt_inner(u3k(a), b, c);
  if ( c3y == d_ret.mut ) {
    u3z(a);
    return d_ret.d;
  }
  return a;
}


u3_noun
u3wdb_pyt(u3_noun cor)
{
  u3_noun a, b, c;
  u3x_mean(cor, u3x_sam_2,   &b,
                u3x_sam_3,   &c,
                u3x_con_sam, &a, 0);
  return u3qdb_pyt(a, b, c);
}

u3_noun
u3kdb_pyt(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  u3_noun pro = u3qdb_pyt(a, b, c);
  u3z(a); u3z(b); u3z(c);
  return pro;
}
