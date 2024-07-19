/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"
#include <retrieve.h>
#include <types.h>

  static u3_atom
  _cqee_sign_octs(u3_noun len, u3_noun dat, u3_noun key)
  {
    c3_y key_y[32];
    c3_w len_w;
    if ( 0 != u3r_bytes_fit(32, key_y, key) ) {
      // hoon calls suck, which calls puck, which crashes
      return u3m_bail(c3__exit);
    }
    else if ( !u3r_word_fit(&len_w, len) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y  sig_y[64];
      c3_y* dat_y = u3r_bytes_alloc(0, len_w, dat);
      urcrypt_ed_sign(dat_y, len_w, key_y, sig_y);
      u3a_free(dat_y);
      return u3i_bytes(64, sig_y);
    }
  }

  u3_noun
  u3wee_sign_octs(u3_noun cor)
  {
    u3_noun msg, key;
    u3_noun len, dat;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &msg, u3x_sam_3, &key, 0) ||
         c3n == u3r_cell(msg, &len, &dat) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_sign_octs(len, dat, key);
    }
  }
