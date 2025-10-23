/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3we_mole_pre_136(u3_noun cor)
{
  u3_noun gul = u3nt(
    //  battery
    //
    u3nq(
      u3nc(1, 0),                                             //  :-  ~
      u3nc(1, 0),                                             //  :-  ~
      2,                                                      //  .*
      u3nq(u3nc(0, 6),                                        //    +<
        1, 12, u3nt(u3nc(0, 2), 0, 3))                        //  [12 [0 2] 0 3]
    ),
    //  sample
    //
    u3nc(0, 0),
    //  context
    //
    0
  );

  u3_noun pro = u3n_nock_et_pre_136(gul,
    u3k(u3x_at(u3x_sam, cor)),
    u3nq(9, 2, 0, 1));
  
  c3_dessert(c3y == u3du(pro));

  switch (u3h(pro)) {
    case 0: {
      u3_noun out = u3nc(u3_nul, u3k(u3t(pro)));
      u3z(pro);
      return out;
    } break;

    case 1:
    case 2: {
      u3z(pro);
      return u3_nul;
    } break;

    default: {
      return u3m_bail(c3__fail);
    } break;
  }
}
