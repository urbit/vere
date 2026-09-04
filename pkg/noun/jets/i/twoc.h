/// @file
///
/// Shared two's-complement integer kernels for the numerics jets.  These are
/// the bit-exact arithmetic primitives validated standalone (see numerics
/// libmath/tools/twoc/), used by BOTH the scalar `/lib/twoc` jet (twoc.c) and
/// the Lagoon `%int2` element-wise array jets (lagoon.c), so there is one
/// validated source of truth.
///
/// Native paths: the op set is generated for uint64 (lanes/values <=64 bits)
/// and unsigned __int128 (<=128) via a type-templated macro.  [msk] is the
/// low-width-bit mask, [sb] the sign-bit position (width-1).  All arithmetic is
/// unsigned with explicit masking, so there is no signed-overflow UB (e.g.
/// div(min,-1) wraps to min, as in the Hoon).  GMP helpers cover widths >128.
///
/// REQUIRES noun.h (c3 types, gmp.h) to be included before this header.

#ifndef _NOUN_JETS_I_TWOC_H
#define _NOUN_JETS_I_TWOC_H

typedef unsigned __int128 _twoc_u128;
typedef          __int128 _twoc_s128;

#define _TWOC_NATIVE(SUF, UT)                                                  \
  static inline UT _tn_neg##SUF(UT a, UT msk) {                                \
    return (UT)(((~a) + 1) & msk);                                             \
  }                                                                            \
  static inline UT _tn_sign##SUF(UT a, c3_d sb) {                              \
    return (UT)((a >> sb) & 1);                                                \
  }                                                                            \
  static inline UT _tn_abs##SUF(UT a, UT msk, c3_d sb) {                       \
    return _tn_sign##SUF(a, sb) ? _tn_neg##SUF(a, msk) : (UT)(a & msk);        \
  }                                                                            \
  static inline UT _tn_add##SUF(UT a, UT b, UT msk) {                          \
    return (UT)((a + b) & msk);                                                \
  }                                                                            \
  static inline UT _tn_sub##SUF(UT a, UT b, UT msk) {                          \
    return (UT)((a + _tn_neg##SUF(b, msk)) & msk);                             \
  }                                                                            \
  static inline UT _tn_mul##SUF(UT a, UT b, UT msk) {                          \
    return (UT)(((a & msk) * (b & msk)) & msk);                                \
  }                                                                            \
  static inline UT _tn_div##SUF(UT a, UT b, UT msk, c3_d sb) {                 \
    UT qa = _tn_abs##SUF(a, msk, sb), qb = _tn_abs##SUF(b, msk, sb);           \
    UT q  = (UT)(qa / qb);                                                     \
    return ( _tn_sign##SUF(a, sb) != _tn_sign##SUF(b, sb) )                    \
           ? _tn_neg##SUF(q, msk) : (UT)(q & msk);                            \
  }                                                                            \
  static inline UT _tn_rem##SUF(UT a, UT b, UT msk, c3_d sb) {                 \
    UT r = (UT)(_tn_abs##SUF(a, msk, sb) % _tn_abs##SUF(b, msk, sb));          \
    return _tn_sign##SUF(a, sb) ? _tn_neg##SUF(r, msk) : (UT)(r & msk);        \
  }                                                                            \
  static inline UT _tn_pow##SUF(UT a, c3_d n, UT msk) {                        \
    UT base = (UT)(a & msk), acc = (UT)(1 & msk);                              \
    while ( n ) {                                                              \
      if ( n & 1 ) acc = (UT)((acc * base) & msk);                            \
      base = (UT)((base * base) & msk);                                        \
      n >>= 1;                                                                 \
    }                                                                          \
    return acc;                                                               \
  }                                                                            \
  /* two's-complement order: negatives below non-negatives. */                \
  static inline c3_t _tn_gth##SUF(UT a, UT b, c3_d sb) {                       \
    UT sa = _tn_sign##SUF(a, sb), sbb = _tn_sign##SUF(b, sb);                  \
    if ( sa != sbb ) return ( 0 == sa ) ? c3y : c3n;                          \
    return ( a > b ) ? c3y : c3n;                                             \
  }

_TWOC_NATIVE(64,  c3_d)
_TWOC_NATIVE(128, _twoc_u128)

//  low-width-bit masks, guarding the width==type-size shift (UB) case.
static inline c3_d
_tn_msk64(c3_d wid)
{
  return ( wid >= 64 ) ? (c3_d)~0ull : (((c3_d)1 << wid) - 1);
}
static inline _twoc_u128
_tn_msk128(c3_d wid)
{
  return ( wid >= 128 ) ? (~(_twoc_u128)0)
                        : ((((_twoc_u128)1) << wid) - 1);
}

/* GMP path (widths >128).  Masking to width bits is a floored mod 2^width
** (mpz_fdiv_r_2exp), the canonical non-negative two's-complement rep; the
** signed value (for div/rem/compare) subtracts 2^width when the sign bit is set.
*/
//  r <- a mod 2^wid (non-negative).
static inline void
_tg_mask(mpz_t r, const mpz_t a, c3_d wid)
{
  mpz_fdiv_r_2exp(r, a, wid);
}
//  is the wid-1 (sign) bit of a masked value set?
static inline c3_t
_tg_sign(const mpz_t m, c3_d wid)
{
  return mpz_tstbit(m, wid - 1) ? c3y : c3n;
}
//  s <- signed value of masked m (subtract 2^wid if negative).
static inline void
_tg_signed(mpz_t s, const mpz_t m, c3_d wid)
{
  if ( _tg_sign(m, wid) == c3y ) {
    mpz_t pow;
    mpz_init(pow);
    mpz_ui_pow_ui(pow, 2, wid);
    mpz_sub(s, m, pow);
    mpz_clear(pow);
  }
  else {
    mpz_set(s, m);
  }
}

#endif /* _NOUN_JETS_I_TWOC_H */
