/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_smac(u3_atom a,
             u3_atom b,
             u3_atom c)
  {
    c3_y a_y[32], b_y[32], c_y[32], out_y[32];
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

    met_w = u3r_met(3, c);
    if ( (32 < met_w) ||
         ( (32 == met_w) &&
           (127 < u3r_byte(31, c)) )
        ) {
      u3_noun c_recs = u3qee_recs(c);
      u3r_bytes(0, 32, c_y, c_recs);
      u3z(c_recs);
    } else {
      u3r_bytes(0, 32, c_y, c);
    }

    urcrypt_ed_scalar_muladd(a_y, b_y, c_y, out_y);
    return u3i_bytes(32, out_y);
  }

  u3_noun
  u3wee_smac(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &a,
                               u3x_sam_6, &b,
                               u3x_sam_7, &c, 0)) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqee_smac(a, b, c);
    }
  }
