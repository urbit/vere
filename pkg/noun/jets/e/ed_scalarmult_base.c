/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_scalarmult_base(u3_atom a)
  {
    c3_y a_y[32], out_y[32];
    c3_w met_w = u3r_met(3, a);
    // scalarmult_base expects a_y[31] <= 127
    if ( (32 < met_w) ||
         ( (32 == met_w) &&
           (127 < u3r_byte(31, a)) )
        ) {
      u3_noun a_recs = u3qee_recs(a);
      u3r_bytes(0, 32, a_y, a_recs);
      u3z(a_recs);
    } else {
      u3r_bytes(0, 32, a_y, a);
    }

    if (0 != urcrypt_ed_scalarmult_base(a_y, out_y))  {
      // should be unreachable, as scalar already reduced
      return u3m_bail(c3__exit);
    }
    else {
      return u3i_bytes(32, out_y);
    }
  }

  u3_noun
  u3wee_scalarmult_base(u3_noun cor)
  {
    u3_noun a = u3r_at(u3x_sam, cor);

    if ( (u3_none == a) || (c3n == u3ud(a)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return _cqee_scalarmult_base(a);
    }
  }
