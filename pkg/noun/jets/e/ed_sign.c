/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"
#include <retrieve.h>
#include <types.h>

  static u3_atom
  _cqee_sign_octs(u3_noun len, u3_noun dat, u3_noun sed)
  {
    c3_y sed_y[32];
    c3_w len_w;
    if ( 0 != u3r_bytes_fit(32, sed_y, sed) ) {
      // hoon calls luck, which crashes
      return u3m_bail(c3__exit);
    }
    else if ( !u3r_word_fit(&len_w, len) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y  sig_y[64];
      c3_y* dat_y = u3r_bytes_alloc(0, len_w, dat);
      urcrypt_ed_sign(dat_y, len_w, sed_y, sig_y);
      u3a_free(dat_y);
      return u3i_bytes(64, sig_y);
    }
  }

  u3_noun
  u3wee_sign_octs(u3_noun cor)
  {
    u3_noun msg, sed;
    u3_noun len, dat;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &msg, u3x_sam_3, &sed, 0) ||
         c3n == u3r_cell(msg, &len, &dat) ||
         c3n == u3ud(sed) ||
         c3n == u3ud(len) ||
         c3n == u3ud(dat) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_sign_octs(len, dat, sed);
    }
  }

  static u3_atom
  _cqee_sign_octs_raw(u3_noun len, u3_noun dat, u3_noun pub, u3_noun sek)
  {
    c3_y pub_y[32], sek_y[64];
    c3_w len_w;
    if ( 0 != u3r_bytes_fit(32, pub_y, pub) ) {
      // hoon asserts size
      return u3m_bail(c3__exit);
    }
    if ( 0 != u3r_bytes_fit(64, sek_y, sek) ) {
      // hoon asserts size
      return u3m_bail(c3__exit);
    }
    else if ( !u3r_word_fit(&len_w, len) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y  sig_y[64];
      c3_y* dat_y = u3r_bytes_alloc(0, len_w, dat);
      urcrypt_ed_sign_raw(dat_y, len_w, pub_y, sek_y, sig_y);
      u3a_free(dat_y);
      return u3i_bytes(64, sig_y);
    }
  }

  u3_noun
  u3wee_sign_octs_raw(u3_noun cor)
  {
    u3_noun msg, pub, sek;
    u3_noun len, dat;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &msg, u3x_sam_6, &pub, u3x_sam_7, &sek, 0) ||
         c3n == u3r_cell(msg, &len, &dat) ||
         c3n == u3ud(pub) ||
         c3n == u3ud(sek) ||
         c3n == u3ud(len) ||
         c3n == u3ud(dat) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_sign_octs_raw(len, dat, pub, sek);
    }
  }

  static u3_atom
  _cqee_sign(u3_noun msg,
             u3_noun sed)
  {
    c3_y sed_y[32];

    if ( 0 != u3r_bytes_fit(32, sed_y, sed) ) {
      // hoon calls luck, which crashes
      return u3m_bail(c3__exit);
    }
    else {
      c3_y  sig_y[64];
      c3_w  met_w;
      c3_y* msg_y = u3r_bytes_all(&met_w, msg);

      urcrypt_ed_sign(msg_y, met_w, sed_y, sig_y);
      u3a_free(msg_y);

      return u3i_bytes(64, sig_y);
    }
  }

  u3_noun
  u3wee_sign(u3_noun cor)
  {
    u3_noun msg, sed;
    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &msg, u3x_sam_3, &sed, 0) ||
         c3n == u3ud(msg) ||
         c3n == u3ud(sed) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_sign(msg, sed);
    }
  }

  static u3_atom
  _cqee_sign_raw(u3_noun msg,
             u3_noun pub,
             u3_noun sek)
  {
    c3_y pub_y[32], sek_y[64];

    if ( 0 != u3r_bytes_fit(32, pub_y, pub) ) {
      // hoon asserts size
      return u3m_bail(c3__exit);
    }
    if ( 0 != u3r_bytes_fit(64, sek_y, sek) ) {
      // hoon asserts size
      return u3m_bail(c3__exit);
    }
    else {
      c3_y  sig_y[64];
      c3_w  met_w;
      c3_y* msg_y = u3r_bytes_all(&met_w, msg);

      urcrypt_ed_sign_raw(msg_y, met_w, pub_y, sek_y, sig_y);
      u3a_free(msg_y);

      return u3i_bytes(64, sig_y);
    }
  }

  u3_noun
  u3wee_sign_raw(u3_noun cor)
  {
    u3_noun msg, pub, sek;
    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &msg, u3x_sam_6, &pub, u3x_sam_7, &sek, 0) ||
         c3n == u3ud(msg) ||
         c3n == u3ud(pub) ||
         c3n == u3ud(sek) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_sign_raw(msg, pub, sek);
    }
  }
