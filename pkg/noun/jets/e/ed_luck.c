/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_luck(u3_atom sed)
  {
    c3_y sed_y[32];

    if ( 0 != u3r_bytes_fit(32, sed_y, sed) ) {
      // hoon explicitly crashes on mis-size
      return u3m_bail(c3__exit);
    }
    else {
      c3_y pub_y[32];
      c3_y sec_y[64];
      urcrypt_ed_luck(sed_y, pub_y, sec_y);
      return u3nc(u3i_bytes(32, pub_y), u3i_bytes(64, sec_y));
    }
  }

  u3_noun
  u3wee_luck(u3_noun cor)
  {
    u3_noun a = u3r_at(u3x_sam, cor);

    if ( (u3_none == a) || (c3n == u3ud(a)) ) {
      return u3m_bail(c3__exit);
    }
    else {
      return _cqee_luck(a);
    }
  }
