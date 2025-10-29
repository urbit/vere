/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqe_chacha_crypt(u3_atom rounds, u3_atom key, u3_atom nonce, u3_atom counter, u3_atom wid, u3_atom dat)
  {
    c3_w rounds_w, wid_w;
    c3_d counter_d;
    if ( !u3r_word_fit(&rounds_w, rounds) || !u3r_word_fit(&wid_w, wid) || c3n == u3r_safe_chub(counter, &counter_d) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y key_y[32], nonce_y[8];
      u3r_bytes(0, 32, key_y, key);
      u3r_bytes(0, 8, nonce_y, nonce);
      c3_y *dat_y = u3r_bytes_alloc(0, wid_w, dat);
      urcrypt_chacha_crypt(rounds_w, key_y, nonce_y, counter_d, wid_w, dat_y);
      u3_noun cry = u3i_bytes(wid_w, dat_y);
      u3a_free(dat_y);
      return u3i_cell(wid, cry);
    }
  }

  u3_noun
  u3we_chacha_crypt(u3_noun cor)
  {
    u3_noun sam = u3x_at(u3x_sam, cor);
    u3_noun rounds, key, nonce, counter, msg;
    u3_noun wid, dat;

    if ( u3r_quil(sam, &rounds, &key, &nonce, &counter, &msg) ||
         u3ud(rounds) || u3ud(key) || u3ud(nonce) || u3ud(counter) || u3r_cell(msg, &wid, &dat) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("chacha_crypt", _cqe_chacha_crypt(rounds, key, nonce, counter, wid, dat));
    }
  }


  static u3_noun
  _cqe_chacha_xchacha(u3_atom rounds, u3_atom key, u3_atom nonce)
  {
    c3_w rounds_w;
    if ( !u3r_word_fit(&rounds_w, rounds) ) {
      return u3m_bail(c3__fail);
    }
    c3_y key_y[32], nonce_y[64], xkey_y[32], xnonce_y[8];
    u3r_bytes(0, 32, key_y, key);
    u3r_bytes(0, 24, nonce_y, nonce);
    urcrypt_chacha_xchacha(rounds, key_y, nonce_y, xkey_y, xnonce_y);
    return u3i_cell(u3i_bytes(32, xkey_y), u3i_bytes(8, xnonce_y));
  }

  u3_noun
  u3we_chacha_xchacha(u3_noun cor)
  {
    u3_noun sam = u3x_at(u3x_sam, cor);
    u3_noun rounds, key, nonce;
    if ( c3n == u3r_trel(sam, &rounds, &key, &nonce) ||
         c3n == u3ud(rounds) ||
         c3n == u3ud(key) ||
         c3n == u3ud(nonce) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("chacha_xchacha", _cqe_chacha_xchacha(rounds, key, nonce));
    }
  }
