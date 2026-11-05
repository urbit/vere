/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

#define defw(bits,byts) \
  u3_atom \
  _kecc_##bits(c3_w len_w, u3_atom a) \
  { \
    c3_y  out[byts]; \
    c3_y* buf_y = u3r_bytes_alloc(0, len_w, a); \
    if ( 0 != urcrypt_keccak_##bits(buf_y, len_w, out) ) { \
      /* urcrypt_keccac_##bits always succeeds when called correctly */ \
      return u3m_bail(c3__oops); \
    } \
    else { \
      u3_atom pro = u3i_bytes(byts, out); \
      u3a_free(buf_y); \
      return pro; \
    } \
  } \
  \
  u3_weak \
  u3we_kecc##bits(u3_noun cor) \
  { \
    c3_w    len_w; \
    u3_noun len, tom; \
    u3x_mean(cor, {u3x_sam_2, &len}, {u3x_sam_3, &tom}); \
    return ( (c3n == u3ud(len)) || (c3n == u3ud(tom)) ) \
      ? u3m_bail(c3__exit) \
      : (!u3r_word_fit(&len_w, len)) \
      ? u3m_bail(c3__fail) \
      : _kecc_##bits(len_w, tom); \
  } \
  \
  /* u3ue_kecc##bits(): array driver of u3we_kecc##bits. */ \
  u3_noun \
  u3ue_kecc##bits(u3_noun* arg) \
  { \
    c3_w len_w; \
    return ( (c3n == u3ud(arg[0])) || (c3n == u3ud(arg[1])) ) \
      ? u3m_bail(c3__exit) \
      : (!u3r_word_fit(&len_w, arg[0])) \
      ? u3m_bail(c3__fail) \
      : _kecc_##bits(len_w, arg[1]); \
  }

defw(224, 28)
defw(256, 32)
defw(384, 48)
defw(512, 64)
