/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_shar(u3_atom pub, u3_atom sed)
  {
    c3_y pub_y[32], sed_y[32];

    if ( 0 != u3r_bytes_fit(32, pub_y, pub) ) {
      return u3m_bail(c3__exit);
    }
    else if ( 0 != u3r_bytes_fit(32, sed_y, sed) ) {
      // hoon calls luck, which crashes
      return u3m_bail(c3__exit);
    }
    else {
      c3_y shr_y[32];
      urcrypt_ed_shar(pub_y, sed_y, shr_y);
      return u3i_bytes(32, shr_y);
    }
  }

  u3_noun
  u3wee_shar(u3_noun cor)
  {
    u3_noun pub, sed;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &pub, u3x_sam_3, &sed, 0)) ||
         (c3n == u3ud(pub)) ||
         (c3n == u3ud(sed)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqee_shar(pub, sed);
    }
  }

  static u3_atom
  _cqee_slar(u3_atom pub, u3_atom sek)
  {
    c3_y pub_y[32], sek_y[64];

    if ( 0 != u3r_bytes_fit(32, pub_y, pub) ) {
      return u3m_bail(c3__exit);
    }
    else if ( 0 != u3r_bytes_fit(64, sek_y, sek) ) {
      return u3m_bail(c3__exit);
    }
    else {
      c3_y shr_y[32];
      urcrypt_ed_slar(pub_y, sek_y, shr_y);
      return u3i_bytes(32, shr_y);
    }
  }

  u3_noun
  u3wee_slar(u3_noun cor)
  {
    u3_noun pub, sek;

    if ( (c3n == u3r_mean(cor, u3x_sam_2, &pub, u3x_sam_3, &sek, 0)) ||
         (c3n == u3ud(pub)) ||
         (c3n == u3ud(sek)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqee_slar(pub, sek);
    }
  }
