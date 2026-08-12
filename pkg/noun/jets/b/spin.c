/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  u3_noun
  u3kb_spin(u3_noun a,
            u3_noun b,
            u3_noun c)
  {
    if ( u3_nul == a ) {
      u3z(c);
      return u3nc(u3_nul, b);
    }


    u3j_site sit_u;
    u3_noun pro, *lit = &pro, *hed, *tel, i, t;
    u3j_gate_prep(&sit_u, c);

    do {
      u3x_cell(a, &i, &t);
      u3k(i), u3k(t);
      u3z(a);
      a = t;

      u3_noun res = u3j_gate_slam(&sit_u, u3nc(i, b));
      b = u3k(u3t(res));
      *lit = u3i_defcons(&hed, &tel);
      *hed = u3k(u3h(res));
      lit = tel;
      u3z(res);
    } while (u3_nul != a);

    *lit = u3_nul;
    u3j_gate_lose(&sit_u);
    return u3nc(pro, b);
  }

  u3_noun
  u3wb_spin(u3_noun cor)
  {
    u3_noun a, b, c;

    a = u3h(u3h(u3t(cor)));
    b = u3h(u3t(u3h(u3t(cor))));
    c = u3t(u3t(u3h(u3t(cor))));
    return u3kb_spin(u3k(a), u3k(b), u3k(c));
  }

