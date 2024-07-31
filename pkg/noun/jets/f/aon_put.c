/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


u3_noun
u3qfa_put(u3_noun a,
          u3_noun b,
          u3_noun c)
{
  if ( u3_nul == a ) {
    return u3nt(u3nc(u3k(b), u3k(c)), u3_nul, u3_nul);
  }
  else {
    u3_noun n_a, l_a, r_a;
    u3_noun keyn_a, valn_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    u3x_cell(n_a, &keyn_a, &valn_a);

    if ( (c3y == u3r_sing(b, keyn_a)) ) {
      if ( c3y == u3r_sing(c, valn_a) ) {
        return u3k(a);
      } else {
        return u3nt(u3nc(u3k(b), u3k(c)),
                    u3k(l_a),
                    u3k(r_a));
      }
    } else if ( u3qf_lte_iota(b, keyn_a) ) {
      u3_noun d = u3qfa_put(l_a, b, c);
      if ( c3y == u3qc_mor(keyn_a, u3h(u3h(d))) ) {
        return u3nt(u3k(n_a),
                    d,
                    u3k(r_a));
      } else {
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
    } else {
      u3_noun d = u3qfa_put(r_a, b, c);
      if ( c3y == u3qc_mor(keyn_a, u3h(u3h(d))) ) {
        return u3nt(u3k(n_a),
                    u3k(l_a),
                    d);
      } else {
        u3_noun n_d, l_d, r_d;
        u3r_trel(d, &n_d, &l_d, &r_d);
        u3_noun e = u3nt(u3k(n_d),
                         u3nt(u3k(n_a),
                              u3k(l_d),
                              u3k(l_a)),
                         u3k(r_d));
        u3z(d);
        return e;
      }
    }
  }
}

u3_noun
u3wfa_put(u3_noun cor)
{
  u3_noun a, b, c;
  u3x_mean(cor,
           u3x_con_sam, &a,
           u3x_sam_2, &b,
           u3x_sam_3, &c, 0);
  return u3qfa_put(a, b, c);
}

