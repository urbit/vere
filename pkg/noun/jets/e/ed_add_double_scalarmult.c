/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_add_double_scalarmult(u3_atom a,
                             u3_atom a_point,
                             u3_atom b,
                             u3_atom b_point)
  {
    c3_y a_y[32], a_point_y[32],
         b_y[32], b_point_y[32],
         out_y[32];
    c3_w met_w;

    met_w = u3r_met(3, a);
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

    met_w = u3r_met(3, b);
    if ( (32 < met_w) ||
         ( (32 == met_w) &&
           (127 < u3r_byte(31, b)) )
        ) {
      u3_noun b_recs = u3qee_recs(b);
      u3r_bytes(0, 32, b_y, b_recs);
      u3z(b_recs);
    } else {
      u3r_bytes(0, 32, b_y, b);
    }

    if ( (0 != u3r_bytes_fit(32, a_point_y, a_point)) ||
         (0 != u3r_bytes_fit(32, b_point_y, b_point)) ||
         (0 != urcrypt_ed_add_double_scalarmult(a_y, a_point_y, b_y, b_point_y, out_y)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return u3i_bytes(32, out_y);
    }
  }

  u3_noun
  u3wee_add_double_scalarmult(u3_noun cor)
  {
    u3_noun a, b, c, d;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a,
                               u3x_sam_6, &b,
                               u3x_sam_14, &c,
                               u3x_sam_15, &d, 0)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) ||
         (c3n == u3ud(d)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqee_add_double_scalarmult(a, b, c, d);
    }
  }
