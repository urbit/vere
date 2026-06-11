/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

#define defw(bits,byts) \
  u3_atom \
  _kecc_##bits(c3_w len_w, u3_atom a) \
  { \
    c3_y  out[byts]; \
    u3r_view vu_u; \
    u3r_view_padded(&vu_u, a, len_w); \
    if ( 0 != urcrypt_keccak_##bits((c3_y*)vu_u.byt_y, len_w, out) ) { \
      /* urcrypt_keccak_##bits always succeeds when called correctly */ \
      u3r_view_done(&vu_u); \
      return u3m_bail(c3__oops); \
    } \
    u3_atom pro = u3i_bytes(byts, out); \
    u3r_view_done(&vu_u); \
    return pro; \
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
  }

defw(224, 28)
defw(256, 32)
defw(384, 48)
defw(512, 64)
