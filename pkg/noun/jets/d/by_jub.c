/* j/4/by_jub.c
**
*/
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3qdb_jub(u3_noun a,
          u3_noun key,
          u3_noun fun)
{
  if ( u3_nul == a ) {
    u3_noun val = u3n_slam_on(u3k(fun), u3_nul);

    if ( u3_nul != val ) {
      u3_noun b = u3nt(u3nc(u3k(key), u3k(u3t(val))), u3_nul, u3_nul);

      u3z(val);
      return b;
    }
    else {
      return u3_nul;
    }
  }

  else {
    u3_noun n_a, lr_a;
    u3_noun pn_a, qn_a;
    u3x_cell(a, &n_a, &lr_a);
    u3x_cell(n_a, &pn_a, &qn_a);

    if ( c3y == u3r_sing(pn_a, key) ) {
      u3_noun val = u3n_slam_on(u3k(fun), u3nc(u3_nul, u3k(qn_a)));

      if ( u3_nul != val ) {
        u3_noun u_val = u3t(val);

        if ( c3y == u3r_sing(qn_a, u_val) ) {

          u3z(val);
          return u3k(a);
        }
        else {
          u3_noun b = u3nc(u3nc(u3k(pn_a), u3k(u_val)), u3k(lr_a));

          u3z(val);
          return b;
        }
      }
      else {
        u3_noun b = u3qdb_del(a, key);

        return b;
      }
    }
    else {
      u3_noun l_a, r_a;
      u3x_cell(lr_a, &l_a, &r_a);
      u3_noun d;

      if ( c3y == u3qc_gor(key, pn_a) ) {
        d = u3qdb_jub(l_a, key, fun);

        if ( u3_nul == d ) {
          return u3qdb_put(u3k(r_a), u3k(pn_a), u3k(qn_a));
        }

        if ( c3y == u3qc_mor(pn_a, u3h(u3h(d))) ) {
          return u3nt(u3k(n_a),
                      d,
                      u3k(r_a));
        }
        else {
          u3_noun n_d, l_d, r_d;

          u3r_trel(d, &n_d, &l_d, &r_d);

          u3_noun e = u3nt(u3k(n_d),
                           u3k(l_d),
                           u3nt(u3k(n_a),
                                u3k(r_d),
                                u3k(r_a)));

          u3z(d);
          return e;
        }
      }
      else {
        d = u3qdb_jub(r_a, key, fun);

        if ( u3_nul == d ) {
          return u3qdb_put(u3k(l_a), u3k(pn_a), u3k(qn_a));
        }

        if ( c3y == u3qc_mor(pn_a, u3h(u3h(d))) ) {
          return u3nt(u3k(n_a),
                      u3k(l_a),
                      d);
        }
        else {
          u3_noun n_d, l_d, r_d;
          u3r_trel(d, &n_d, &l_d, &r_d);

          u3_noun e = u3nt(u3k(n_d),
                           u3nt(u3k(n_a),
                                u3k(l_a),
                                u3k(l_d)),
                            u3k(r_d));

          u3z(d);
          return e;
        }
      }
    }
  }
}

u3_noun
u3wdb_jub(u3_noun cor)
{
  u3_noun a, key, fun;
  u3x_mean(cor, u3x_sam_2,   &key,
                u3x_sam_3,   &fun,
                u3x_con_sam, &a, 0);

  return u3qdb_jub(a, key, fun);
}
