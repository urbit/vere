/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"


  static u3_atom
  _cqee_point_neg(u3_atom a)
  {
    c3_y a_y[32];

    if ( (0 != u3r_bytes_fit(32, a_y, a)) ||
         (0 != urcrypt_ed_point_neg(a_y)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return u3i_bytes(32, a_y);
    }
  }

  u3_noun
  u3wee_point_neg(u3_noun cor)
  {

    u3_noun a;

    if ( (u3_none == (a = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqee_point_neg(a);
    }
  }
