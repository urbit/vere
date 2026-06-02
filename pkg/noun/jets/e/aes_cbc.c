/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

/* All of the CBC hoon truncates its key and prv inputs by passing them to
 * the ECB functions, which truncate them, hence the raw u3r_bytes unpacking.
 */

typedef int (*urcrypt_cbc)(c3_y*, size_t, c3_y*, c3_y*);

  static u3_atom
  _cqea_cbc_help(c3_y* key_y, u3_atom iv, u3_atom msg, urcrypt_cbc low_f)
  {
    c3_y     iv_y[16];
    //  message length in 16-byte (bloq 7) blocks; cbc always processes at least
    //  one block (the hoon pads an empty message to a single zero block)
    c3_d     len_d = c3_max(1, u3r_met(7, msg));
    u3i_slab sab_u;

    u3r_bytes(0, 16, iv_y, iv);

    //  read/write buffer holding [msg] little-endian, zero-padded to a 16-byte
    //  block boundary (bloq 7), passed to urcrypt's unsafe (no realloc)
    //  interface, which operates in place.
    //
    u3i_slab_from(&sab_u, msg, 7, len_d);

    //  the only error is a non-block-aligned length, ruled out by construction
    //
    u3_assert( 0 == (*low_f)(sab_u.buf_y, (c3_z)sab_u.len_w << 2, key_y, iv_y) );

    return u3i_slab_mint(&sab_u);
  }

  static u3_atom
  _cqea_cbca_en(u3_atom key,
                u3_atom iv,
                u3_atom msg)
  {
    c3_y key_y[16];
    u3r_bytes(0, 16, key_y, key);
    return _cqea_cbc_help(key_y, iv, msg, &urcrypt_aes_cbca_en_unsafe);
  }

  u3_noun
  u3wea_cbca_en(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( c3n == u3r_mean(cor, u3x_sam, &c, 60, &a, 61, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("cbca-en", _cqea_cbca_en(a, b, c));
    }
  }

  static u3_atom
  _cqea_cbca_de(u3_atom key,
                u3_atom iv,
                u3_atom msg)
  {
    c3_y key_y[16];
    u3r_bytes(0, 16, key_y, key);
    return _cqea_cbc_help(key_y, iv, msg, &urcrypt_aes_cbca_de_unsafe);
  }

  u3_noun
  u3wea_cbca_de(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( c3n == u3r_mean(cor, u3x_sam, &c, 60, &a, 61, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("cbca-de", _cqea_cbca_de(a, b, c));
    }
  }

  static u3_atom
  _cqea_cbcb_en(u3_atom key,
                u3_atom iv,
                u3_atom msg)
  {
    c3_y key_y[24];
    u3r_bytes(0, 24, key_y, key);
    return _cqea_cbc_help(key_y, iv, msg, &urcrypt_aes_cbcb_en_unsafe);
  }

  u3_noun
  u3wea_cbcb_en(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( c3n == u3r_mean(cor, u3x_sam, &c, 60, &a, 61, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("cbcb-en", _cqea_cbcb_en(a, b, c));
    }
  }

  static u3_atom
  _cqea_cbcb_de(u3_atom key,
                u3_atom iv,
                u3_atom msg)
  {
    c3_y key_y[24];
    u3r_bytes(0, 24, key_y, key);
    return _cqea_cbc_help(key_y, iv, msg, &urcrypt_aes_cbcb_de_unsafe);
  }

  u3_noun
  u3wea_cbcb_de(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( c3n == u3r_mean(cor, u3x_sam, &c, 60, &a, 61, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("cbcb-de", _cqea_cbcb_de(a, b, c));
    }
  }

  static u3_atom
  _cqea_cbcc_en(u3_atom key,
                u3_atom iv,
                u3_atom msg)
  {
    c3_y key_y[32];
    u3r_bytes(0, 32, key_y, key);
    return _cqea_cbc_help(key_y, iv, msg, &urcrypt_aes_cbcc_en_unsafe);
  }

  u3_noun
  u3wea_cbcc_en(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( c3n == u3r_mean(cor, u3x_sam, &c, 60, &a, 61, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("cbcc-en", _cqea_cbcc_en(a, b, c));
    }
  }

  static u3_atom
  _cqea_cbcc_de(u3_atom key,
                u3_atom iv,
                u3_atom msg)
  {
    c3_y key_y[32];
    u3r_bytes(0, 32, key_y, key);
    return _cqea_cbc_help(key_y, iv, msg, &urcrypt_aes_cbcc_de_unsafe);
  }

  u3_noun
  u3wea_cbcc_de(u3_noun cor)
  {
    u3_noun a, b, c;

    if ( c3n == u3r_mean(cor, u3x_sam, &c, 60, &a, 61, &b, 0) ||
         c3n == u3ud(a) ||
         c3n == u3ud(b) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3l_punt("cbcc-de", _cqea_cbcc_de(a, b, c));
    }
  }
