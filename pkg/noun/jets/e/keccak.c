/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

//  the message is read through a view: borrowed, not copied, unless the
//  width runs past the atom
//
#define defw(bits,byts) \
  u3_atom \
  _kecc_##bits(c3_d len_d, u3_atom a) \
  { \
    c3_y        out[byts]; \
    c3_y*       cop_y = 0; \
    const c3_y* dat_y; \
    u3r_view    vue_u; \
  \
    u3r_view_open(&vue_u, a, c3y); \
    if ( len_d <= vue_u.byt_d ) { \
      dat_y = u3r_view_bytes(&vue_u); \
    } \
    else if ( len_d <= c3_w_max ) { \
      dat_y = cop_y = u3r_bytes_alloc(0, (c3_w)len_d, a); \
    } \
    else { \
      u3r_view_done(&vue_u); \
      return u3m_bail(c3__fail); \
    } \
  \
    c3_i ret_i = urcrypt_keccak_##bits(dat_y, len_d, out); \
    if ( cop_y ) { \
      u3a_free(cop_y); \
    } \
    u3r_view_done(&vue_u); \
    if ( 0 != ret_i ) { \
      /* urcrypt_keccac_##bits always succeeds when called correctly */ \
      return u3m_bail(c3__oops); \
    } \
    else { \
      return u3i_bytes(byts, out); \
    } \
  } \
  \
  u3_weak \
  u3we_kecc##bits(u3_noun cor) \
  { \
    c3_d    len_d; \
    u3_noun len, tom; \
    u3x_mean(cor, {u3x_sam_2, &len}, {u3x_sam_3, &tom}); \
    return ( (c3n == u3ud(len)) || (c3n == u3ud(tom)) ) \
      ? u3m_bail(c3__exit) \
      : (c3n == u3r_safe_chub(len, &len_d)) \
      ? u3m_bail(c3__fail) \
      : _kecc_##bits(len_d, tom); \
  }

defw(224, 28)
defw(256, 32)
defw(384, 48)
defw(512, 64)
