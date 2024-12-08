/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_scalarmult(u3_atom a,
                  u3_atom b)
  {
    c3_y a_y[32], b_y[32], out_y[32];
    if (0 != u3r_bytes_fit(32, b_y, b)) {
      return u3m_bail(c3__exit);
    }

    c3_w met_w = u3r_met(3, a);
    // scalarmult expects a_y[31] <= 127
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

    if ( (0 != urcrypt_ed_scalarmult(a_y, b_y, out_y)) ) {
      // at this point, will only fail if b is bad point
      return u3m_bail(c3__exit);
    }
    else {
      return u3i_bytes(32, out_y);
    }
  }

  u3_noun
  u3wee_scalarmult(u3_noun cor)
  {
    u3_noun a, b;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a,
                               u3x_sam_3, &b, 0)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqee_scalarmult(a, b);
    }
  }
