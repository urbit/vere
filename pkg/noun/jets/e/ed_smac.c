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

    if ( (0 != u3r_bytes_fit(32, a_y, a)) ||
         (0 != u3r_bytes_fit(32, b_y, b)) ||
         (0 != u3r_bytes_fit(32, c_y, c)) )  {
      // hoon does not check size of inputs
      return u3_none;
    }
    else {
      urcrypt_ed_scalar_muladd(a_y, b_y, c_y, out_y);
      return u3i_bytes(32, out_y);
    }
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
      return u3l_punt("smac", _cqee_smac(a, b, c));
    }
  }
