/// @file
///
/// Jets for the numerics `/lib/unum` library (2022 Posit Standard; userspace,
/// registered under the `non` chapter alongside `math` and `lagoon`).  Each jet
/// calls SoftUnum (ext/softunum), the bit-exact C twin of `/lib/unum`, so jet
/// output is identical to the unjetted Hoon.
///
/// `/lib/unum` is one generic posit core `++pp` (`|_ =bloq`) specialized into
/// `rpb`/`rph`/`rps`/`rpd`/`rpq` (posit8/16/32/64/128) via `%*`.  We register a
/// SINGLE `%unum` core and dispatch on `bloq` read from the door sample (the
/// same one-jet-per-op, runtime-dispatch shape `lagoon` uses).  bloq lives in
/// the `pp` door, which is the gate's context: gate axis 7 = door, door axis 6
/// = `=bloq`, so bloq is at gate axis 30.  SoftUnum covers bloq 3/4/5
/// (posit8/16/32); for bloq 6/7 (posit64/128, not yet in SoftUnum) the jet
/// returns u3_none and the pure-Hoon arm runs.
///
/// Marshalling uses chub reads/writes (word-size-agnostic across the 32- and
/// 64-bit runtimes).  Posit bit patterns occupy the low n bits.
///
/// MASTER COPY lives in urbit/numerics libmath/vere/noun/jets/i/unum.c; applied
/// by hand to the vere runtime.  SoftUnum itself is vendored (ext/softunum).

#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"
#include "softunum.h"

//  bloq from the pp door sample: gate axis 7 = door, door axis 6 = =bloq.
#define _UNUM_BLOQ_AXIS 30

//  Read bloq (the door sample) from a gate core; c3n if absent/not-atom.
static inline c3_t
_unum_bloq(u3_noun cor, c3_d* out)
{
  u3_noun b = u3r_at(_UNUM_BLOQ_AXIS, cor);
  if ( u3_none == b || c3n == u3ud(b) ) {
    return c3n;
  }
  *out = u3r_chub(0, b);
  return c3y;
}

//  binary posit -> posit op (add/sub/mul/div), bloq-dispatched.
#define _UNUM_BINOP(nam, f8, f16, f32)                                       \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom a, u3_atom b) {                  \
    c3_d ua = u3r_chub(0, a), ub = u3r_chub(0, b), r;                         \
    switch ( bloq ) {                                                        \
      case 3:  r = f8((posit8_t)ua, (posit8_t)ub);    break;                 \
      case 4:  r = f16((posit16_t)ua, (posit16_t)ub); break;                 \
      case 5:  r = f32((posit32_t)ua, (posit32_t)ub); break;                 \
      default: return u3_none;                                               \
    }                                                                       \
    return u3i_chubs(1, &r);                                                 \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun a, b;  c3_d bloq;                                                \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||            \
         c3n == u3ud(a) || c3n == u3ud(b) ) return u3m_bail(c3__exit);       \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, a, b);                                      \
  }

//  binary posit -> loobean comparison; returns & (c3y) / | (c3n).
#define _UNUM_CMP(nam, f8, f16, f32)                                         \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom a, u3_atom b) {                  \
    c3_d ua = u3r_chub(0, a), ub = u3r_chub(0, b);  c3_t v;                  \
    switch ( bloq ) {                                                        \
      case 3:  v = f8((posit8_t)ua, (posit8_t)ub);    break;                 \
      case 4:  v = f16((posit16_t)ua, (posit16_t)ub); break;                 \
      case 5:  v = f32((posit32_t)ua, (posit32_t)ub); break;                 \
      default: return u3_none;                                               \
    }                                                                       \
    return v ? c3y : c3n;                                                    \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun a, b;  c3_d bloq;                                                \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||            \
         c3n == u3ud(a) || c3n == u3ud(b) ) return u3m_bail(c3__exit);       \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, a, b);                                      \
  }

//  unary posit -> posit op (neg/abs/sgn/sqt), bloq-dispatched.
#define _UNUM_UNOP(nam, f8, f16, f32)                                        \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom a) {                            \
    c3_d ua = u3r_chub(0, a), r;                                             \
    switch ( bloq ) {                                                        \
      case 3:  r = f8((posit8_t)ua);  break;                                 \
      case 4:  r = f16((posit16_t)ua); break;                                \
      case 5:  r = f32((posit32_t)ua); break;                                \
      default: return u3_none;                                               \
    }                                                                       \
    return u3i_chubs(1, &r);                                                 \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun a = u3r_at(u3x_sam, cor);  c3_d bloq;                            \
    if ( u3_none == a || c3n == u3ud(a) ) return u3m_bail(c3__exit);         \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, a);                                         \
  }

_UNUM_BINOP(add, p8_add, p16_add, p32_add)
_UNUM_BINOP(sub, p8_sub, p16_sub, p32_sub)
_UNUM_BINOP(mul, p8_mul, p16_mul, p32_mul)
_UNUM_BINOP(div, p8_div, p16_div, p32_div)

_UNUM_CMP(lth, p8_lt, p16_lt, p32_lt)
_UNUM_CMP(lte, p8_le, p16_le, p32_le)
_UNUM_CMP(gth, p8_gt, p16_gt, p32_gt)
_UNUM_CMP(gte, p8_ge, p16_ge, p32_ge)
_UNUM_CMP(equ, p8_eq, p16_eq, p32_eq)
_UNUM_CMP(neq, !p8_eq, !p16_eq, !p32_eq)

_UNUM_UNOP(neg, p8_neg, p16_neg, p32_neg)
_UNUM_UNOP(abs, p8_abs, p16_abs, p32_abs)
_UNUM_UNOP(sgn, p8_sgn, p16_sgn, p32_sgn)
_UNUM_UNOP(sqt, p8_sqrt, p16_sqrt, p32_sqrt)

/* ++fma:pp -- fused multiply-add (a*b + c), single rounding.  Ternary gate:
** sample [a b c] = [a [b c]]: a @ sam_2, b @ sam_6, c @ sam_7.
*/
  u3_noun
  u3qi_unum_fma(c3_d bloq, u3_atom a, u3_atom b, u3_atom c)
  {
    c3_d ua = u3r_chub(0, a), ub = u3r_chub(0, b), uc = u3r_chub(0, c), r;
    switch ( bloq ) {
      case 3:  r = p8_fma((posit8_t)ua, (posit8_t)ub, (posit8_t)uc);    break;
      case 4:  r = p16_fma((posit16_t)ua, (posit16_t)ub, (posit16_t)uc); break;
      case 5:  r = p32_fma((posit32_t)ua, (posit32_t)ub, (posit32_t)uc); break;
      default: return u3_none;
    }
    return u3i_chubs(1, &r);
  }

  u3_noun
  u3wi_unum_fma(u3_noun cor)
  {
    u3_noun a, b, c;  c3_d bloq;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_6, &b, u3x_sam_7, &c, 0) ||
         c3n == u3ud(a) || c3n == u3ud(b) || c3n == u3ud(c) ) {
      return u3m_bail(c3__exit);
    }
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_fma(bloq, a, b, c);
  }

//  Elementary / transcendental functions.  Unary ones (incl. log-2/log-10,
//  which map to SoftUnum's base-2/base-10 log fns) reuse the unary template;
//  pow is binary; pow-n is posit ^ @u (the exponent is a raw integer, not a
//  posit).  Constants (pi/e/...) are left to pure Hoon, like /lib/math.
_UNUM_UNOP(exp, p8_exp, p16_exp, p32_exp)
_UNUM_UNOP(sin, p8_sin, p16_sin, p32_sin)
_UNUM_UNOP(cos, p8_cos, p16_cos, p32_cos)
_UNUM_UNOP(tan, p8_tan, p16_tan, p32_tan)
_UNUM_UNOP(log, p8_log, p16_log, p32_log)
_UNUM_UNOP(log2, p8_log2, p16_log2, p32_log2)
_UNUM_UNOP(log10, p8_log10, p16_log10, p32_log10)
_UNUM_UNOP(cbrt, p8_cbrt, p16_cbrt, p32_cbrt)
_UNUM_UNOP(atan, p8_atan, p16_atan, p32_atan)
_UNUM_UNOP(asin, p8_asin, p16_asin, p32_asin)
_UNUM_UNOP(acos, p8_acos, p16_acos, p32_acos)
_UNUM_UNOP(factorial, p8_factorial, p16_factorial, p32_factorial)
_UNUM_BINOP(pow, p8_pow, p16_pow, p32_pow)

/* ++pow-n:pp -- posit ^ @u (integer power); the exponent is a raw unsigned
** integer, not a posit, so it is read as a plain chub.
*/
  u3_noun
  u3qi_unum_pow_n(c3_d bloq, u3_atom x, u3_atom p)
  {
    c3_d ux = u3r_chub(0, x), up = u3r_chub(0, p), r;
    switch ( bloq ) {
      case 3:  r = p8_pow_n((posit8_t)ux, up);  break;
      case 4:  r = p16_pow_n((posit16_t)ux, up); break;
      case 5:  r = p32_pow_n((posit32_t)ux, up); break;
      default: return u3_none;
    }
    return u3i_chubs(1, &r);
  }

  u3_noun
  u3wi_unum_pow_n(u3_noun cor)
  {
    u3_noun x, p;  c3_d bloq;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &x, u3x_sam_3, &p, 0) ||
         c3n == u3ud(x) || c3n == u3ud(p) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_pow_n(bloq, x, p);
  }

//  Rounding to integral value.  The Hoon rnd/flr/cel are eta-expanded into
//  unary gates wrapping +round, so they jet via the unary template.
_UNUM_UNOP(rnd, p8_nearest_int, p16_nearest_int, p32_nearest_int)
_UNUM_UNOP(flr, p8_floor, p16_floor, p32_floor)
_UNUM_UNOP(cel, p8_ceil, p16_ceil, p32_ceil)

/* ++sun:pp -- @u -> posit.  The argument is a raw unsigned integer (read as a
** full chub), NOT a posit pattern, so it is not masked to the posit width.
*/
  u3_noun
  u3qi_unum_sun(c3_d bloq, u3_atom v)
  {
    c3_d uv = u3r_chub(0, v), r;
    switch ( bloq ) {
      case 3:  r = p8_from_u64(uv);  break;
      case 4:  r = p16_from_u64(uv); break;
      case 5:  r = p32_from_u64(uv); break;
      default: return u3_none;
    }
    return u3i_chubs(1, &r);
  }
  u3_noun
  u3wi_unum_sun(u3_noun cor)
  {
    u3_noun v = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == v || c3n == u3ud(v) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_sun(bloq, v);
  }

/* ++san:pp -- @s -> posit.  Decode the Hoon signed atom (even 2m -> +m,
** odd 2m-1 -> -m) to a C int64, then encode.
*/
  u3_noun
  u3qi_unum_san(c3_d bloq, u3_atom v)
  {
    c3_d  uv = u3r_chub(0, v);
    c3_ds sv = (uv & 1) ? -(c3_ds)((uv + 1) >> 1) : (c3_ds)(uv >> 1);
    c3_d  r;
    switch ( bloq ) {
      case 3:  r = p8_from_i64(sv);  break;
      case 4:  r = p16_from_i64(sv); break;
      case 5:  r = p32_from_i64(sv); break;
      default: return u3_none;
    }
    return u3i_chubs(1, &r);
  }
  u3_noun
  u3wi_unum_san(u3_noun cor)
  {
    u3_noun v = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == v || c3n == u3ud(v) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_san(bloq, v);
  }

/* ++toi:pp -- posit -> (unit @s).  NaR -> ~ (none); else [~ @s] with the
** integer re-encoded into a Hoon signed atom (+m -> 2m, -m -> 2m-1).
*/
  u3_noun
  u3qi_unum_toi(c3_d bloq, u3_atom p)
  {
    c3_d  up = u3r_chub(0, p);
    c3_ds out;  c3_t ok;
    switch ( bloq ) {
      case 3:  ok = p8_to_i64((posit8_t)up, (int64_t*)&out);  break;
      case 4:  ok = p16_to_i64((posit16_t)up, (int64_t*)&out); break;
      case 5:  ok = p32_to_i64((posit32_t)up, (int64_t*)&out); break;
      default: return u3_none;
    }
    if ( !ok ) return u3_nul;
    c3_d sa = (out >= 0) ? ((c3_d)out << 1) : (((c3_d)(-out) << 1) - 1);
    return u3nc(u3_nul, u3i_chubs(1, &sa));
  }
  u3_noun
  u3wi_unum_toi(u3_noun cor)
  {
    u3_noun p = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == p || c3n == u3ud(p) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_toi(bloq, p);
  }

/* ++is-close:pp -- |a - b| <= tol, a loobean.  Ternary sample [a b tol].
*/
  u3_noun
  u3qi_unum_is_close(c3_d bloq, u3_atom a, u3_atom b, u3_atom tol)
  {
    c3_d ua = u3r_chub(0, a), ub = u3r_chub(0, b), ut = u3r_chub(0, tol);  c3_t v;
    switch ( bloq ) {
      case 3:  v = p8_is_close((posit8_t)ua, (posit8_t)ub, (posit8_t)ut);    break;
      case 4:  v = p16_is_close((posit16_t)ua, (posit16_t)ub, (posit16_t)ut); break;
      case 5:  v = p32_is_close((posit32_t)ua, (posit32_t)ub, (posit32_t)ut); break;
      default: return u3_none;
    }
    return v ? c3y : c3n;
  }
  u3_noun
  u3wi_unum_is_close(u3_noun cor)
  {
    u3_noun a, b, tol;  c3_d bloq;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_6, &b, u3x_sam_7, &tol, 0) ||
         c3n == u3ud(a) || c3n == u3ud(b) || c3n == u3ud(tol) ) {
      return u3m_bail(c3__exit);
    }
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_is_close(bloq, a, b, tol);
  }

//  IEEE-754 conversion (value-based).  The posit width is bloq; the float width
//  is fixed by the arm.  to-rh/rs/rd and from-rh/rs/rd are 1-chub each side;
//  to-rq / from-rq use the 128-bit (2-chub) binary128 pattern.
#define _UNUM_TO(nam, f8, f16, f32)                                          \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom p) {                            \
    c3_d up = u3r_chub(0, p), r;                                             \
    switch ( bloq ) {                                                       \
      case 3:  r = f8((posit8_t)up);  break;                                 \
      case 4:  r = f16((posit16_t)up); break;                                \
      case 5:  r = f32((posit32_t)up); break;                                \
      default: return u3_none;                                               \
    }                                                                       \
    return u3i_chubs(1, &r);                                                 \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun p = u3r_at(u3x_sam, cor);  c3_d bloq;                            \
    if ( u3_none == p || c3n == u3ud(p) ) return u3m_bail(c3__exit);         \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, p);                                         \
  }

#define _UNUM_FROM(nam, f8, f16, f32)                                        \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom r) {                            \
    c3_d ur = u3r_chub(0, r), v;                                             \
    switch ( bloq ) {                                                       \
      case 3:  v = f8(ur);  break;                                           \
      case 4:  v = f16(ur); break;                                           \
      case 5:  v = f32(ur); break;                                           \
      default: return u3_none;                                               \
    }                                                                       \
    return u3i_chubs(1, &v);                                                 \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun r = u3r_at(u3x_sam, cor);  c3_d bloq;                            \
    if ( u3_none == r || c3n == u3ud(r) ) return u3m_bail(c3__exit);         \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, r);                                         \
  }

_UNUM_TO(to_rh, p8_to_rh, p16_to_rh, p32_to_rh)
_UNUM_TO(to_rs, p8_to_rs, p16_to_rs, p32_to_rs)
_UNUM_TO(to_rd, p8_to_rd, p16_to_rd, p32_to_rd)
_UNUM_FROM(from_rh, p8_from_rh, p16_from_rh, p32_from_rh)
_UNUM_FROM(from_rs, p8_from_rs, p16_from_rs, p32_from_rs)
_UNUM_FROM(from_rd, p8_from_rd, p16_from_rd, p32_from_rd)

/* ++to-rq:pp -- posit -> binary128 (2-chub result).
*/
  u3_noun
  u3qi_unum_to_rq(c3_d bloq, u3_atom p)
  {
    c3_d up = u3r_chub(0, p), out[2];
    switch ( bloq ) {
      case 3:  p8_to_rq((posit8_t)up, out);  break;
      case 4:  p16_to_rq((posit16_t)up, out); break;
      case 5:  p32_to_rq((posit32_t)up, out); break;
      default: return u3_none;
    }
    return u3i_chubs(2, out);
  }
  u3_noun
  u3wi_unum_to_rq(u3_noun cor)
  {
    u3_noun p = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == p || c3n == u3ud(p) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_to_rq(bloq, p);
  }

/* ++from-rq:pp -- binary128 (2-chub input) -> posit.
*/
  u3_noun
  u3qi_unum_from_rq(c3_d bloq, u3_atom r)
  {
    c3_d in[2], v;
    in[0] = u3r_chub(0, r);
    in[1] = u3r_chub(1, r);
    switch ( bloq ) {
      case 3:  v = p8_from_rq(in);  break;
      case 4:  v = p16_from_rq(in); break;
      case 5:  v = p32_from_rq(in); break;
      default: return u3_none;
    }
    return u3i_chubs(1, &v);
  }
  u3_noun
  u3wi_unum_from_rq(u3_noun cor)
  {
    u3_noun r = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == r || c3n == u3ud(r) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_from_rq(bloq, r);
  }

//  Quire: a 16n-bit exact accumulator, marshalled to/from SoftUnum's uint64
//  word array (QW = n/4 words: 2/4/8 for posit8/16/32).  The accumulate ops
//  mutate the buffer in place; we read QW chubs in and write QW chubs out.
#define _UNUM_QW(bloq) (1 << ((bloq) - 2))

  static void
  _unum_qload(u3_atom q, c3_d *buf, int qw)
  {
    for ( int i = 0; i < qw; i++ ) buf[i] = u3r_chub(i, q);
  }

  u3_noun
  u3qi_unum_p_to_q(c3_d bloq, u3_atom p)
  {
    c3_d buf[8] = {0}, up = u3r_chub(0, p);  int qw = _UNUM_QW(bloq);
    switch ( bloq ) {
      case 3:  p8_p_to_q((posit8_t)up, buf);  break;
      case 4:  p16_p_to_q((posit16_t)up, buf); break;
      case 5:  p32_p_to_q((posit32_t)up, buf); break;
      default: return u3_none;
    }
    return u3i_chubs(qw, buf);
  }
  u3_noun
  u3wi_unum_p_to_q(u3_noun cor)
  {
    u3_noun p = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == p || c3n == u3ud(p) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_p_to_q(bloq, p);
  }

  u3_noun
  u3qi_unum_q_to_p(c3_d bloq, u3_atom q)
  {
    c3_d buf[8] = {0}, r;
    if ( bloq < 3 || bloq > 5 ) return u3_none;
    _unum_qload(q, buf, _UNUM_QW(bloq));
    switch ( bloq ) {
      case 3:  r = p8_q_to_p(buf);  break;
      case 4:  r = p16_q_to_p(buf); break;
      default: r = p32_q_to_p(buf); break;
    }
    return u3i_chubs(1, &r);
  }
  u3_noun
  u3wi_unum_q_to_p(u3_noun cor)
  {
    u3_noun q = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == q || c3n == u3ud(q) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_q_to_p(bloq, q);
  }

  u3_noun
  u3qi_unum_q_negate(c3_d bloq, u3_atom q)
  {
    c3_d buf[8] = {0};  int qw = _UNUM_QW(bloq);
    if ( bloq < 3 || bloq > 5 ) return u3_none;
    _unum_qload(q, buf, qw);
    switch ( bloq ) {
      case 3:  p8_q_negate(buf);  break;
      case 4:  p16_q_negate(buf); break;
      default: p32_q_negate(buf); break;
    }
    return u3i_chubs(qw, buf);
  }
  u3_noun
  u3wi_unum_q_negate(u3_noun cor)
  {
    u3_noun q = u3r_at(u3x_sam, cor);  c3_d bloq;
    if ( u3_none == q || c3n == u3ud(q) ) return u3m_bail(c3__exit);
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_q_negate(bloq, q);
  }

//  q-mul-add / q-mul-sub: (quire, posit, posit) -> quire.
#define _UNUM_QMA(nam, f8, f16, f32)                                         \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom q, u3_atom a, u3_atom b) {       \
    c3_d buf[8] = {0}, ua = u3r_chub(0, a), ub = u3r_chub(0, b);             \
    int qw = _UNUM_QW(bloq);                                                 \
    _unum_qload(q, buf, qw);                                                 \
    switch ( bloq ) {                                                       \
      case 3:  f8(buf, (posit8_t)ua, (posit8_t)ub);    break;                \
      case 4:  f16(buf, (posit16_t)ua, (posit16_t)ub); break;               \
      case 5:  f32(buf, (posit32_t)ua, (posit32_t)ub); break;               \
      default: return u3_none;                                               \
    }                                                                       \
    return u3i_chubs(qw, buf);                                               \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun q, a, b;  c3_d bloq;                                             \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &q, u3x_sam_6, &a, u3x_sam_7, &b, 0) || \
         c3n == u3ud(q) || c3n == u3ud(a) || c3n == u3ud(b) )               \
      return u3m_bail(c3__exit);                                             \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, q, a, b);                                   \
  }

_UNUM_QMA(q_mul_add, p8_q_mul_add, p16_q_mul_add, p32_q_mul_add)
_UNUM_QMA(q_mul_sub, p8_q_mul_sub, p16_q_mul_sub, p32_q_mul_sub)

//  q-add-p / q-sub-p: (quire, posit) -> quire.
#define _UNUM_QAP(nam, f8, f16, f32)                                         \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom q, u3_atom p) {                 \
    c3_d buf[8] = {0}, up = u3r_chub(0, p);  int qw = _UNUM_QW(bloq);        \
    _unum_qload(q, buf, qw);                                                 \
    switch ( bloq ) {                                                       \
      case 3:  f8(buf, (posit8_t)up);  break;                                \
      case 4:  f16(buf, (posit16_t)up); break;                               \
      case 5:  f32(buf, (posit32_t)up); break;                               \
      default: return u3_none;                                               \
    }                                                                       \
    return u3i_chubs(qw, buf);                                               \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun q, p;  c3_d bloq;                                                \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &q, u3x_sam_3, &p, 0) ||            \
         c3n == u3ud(q) || c3n == u3ud(p) ) return u3m_bail(c3__exit);       \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, q, p);                                      \
  }

_UNUM_QAP(q_add_p, p8_q_add_p, p16_q_add_p, p32_q_add_p)
_UNUM_QAP(q_sub_p, p8_q_sub_p, p16_q_sub_p, p32_q_sub_p)

//  q-add-q / q-sub-q: (quire, quire) -> quire.
#define _UNUM_QAQ(nam, f8, f16, f32)                                         \
  u3_noun u3qi_unum_##nam(c3_d bloq, u3_atom x, u3_atom y) {                 \
    c3_d xb[8] = {0}, yb[8] = {0};  int qw = _UNUM_QW(bloq);                 \
    if ( bloq < 3 || bloq > 5 ) return u3_none;                             \
    _unum_qload(x, xb, qw);  _unum_qload(y, yb, qw);                         \
    switch ( bloq ) {                                                       \
      case 3:  f8(xb, yb);  break;                                           \
      case 4:  f16(xb, yb); break;                                           \
      default: f32(xb, yb); break;                                           \
    }                                                                       \
    return u3i_chubs(qw, xb);                                                \
  }                                                                          \
  u3_noun u3wi_unum_##nam(u3_noun cor) {                                     \
    u3_noun x, y;  c3_d bloq;                                                \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &x, u3x_sam_3, &y, 0) ||            \
         c3n == u3ud(x) || c3n == u3ud(y) ) return u3m_bail(c3__exit);       \
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;                     \
    return u3qi_unum_##nam(bloq, x, y);                                      \
  }

_UNUM_QAQ(q_add_q, p8_q_add_q, p16_q_add_q, p32_q_add_q)
_UNUM_QAQ(q_sub_q, p8_q_sub_q, p16_q_sub_q, p32_q_sub_q)

/* ++fdp:pp -- fused dot product of two posit lists (single rounding).  We
** accumulate directly through a quire buffer (q-mul-add per element, q-to-p
** once), zipping to the shorter list -- matching the Hoon.
*/
  u3_noun
  u3qi_unum_fdp(c3_d bloq, u3_noun av, u3_noun bv)
  {
    c3_d buf[8] = {0}, r;
    if ( bloq < 3 || bloq > 5 ) return u3_none;
    u3_noun ta = av, tb = bv;
    while ( (c3y == u3du(ta)) && (c3y == u3du(tb)) ) {
      c3_d a = u3r_chub(0, u3h(ta)), b = u3r_chub(0, u3h(tb));
      switch ( bloq ) {
        case 3:  p8_q_mul_add(buf, (posit8_t)a, (posit8_t)b);    break;
        case 4:  p16_q_mul_add(buf, (posit16_t)a, (posit16_t)b); break;
        default: p32_q_mul_add(buf, (posit32_t)a, (posit32_t)b); break;
      }
      ta = u3t(ta);  tb = u3t(tb);
    }
    switch ( bloq ) {
      case 3:  r = p8_q_to_p(buf);  break;
      case 4:  r = p16_q_to_p(buf); break;
      default: r = p32_q_to_p(buf); break;
    }
    return u3i_chubs(1, &r);
  }
  u3_noun
  u3wi_unum_fdp(u3_noun cor)
  {
    u3_noun av, bv;  c3_d bloq;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &av, u3x_sam_3, &bv, 0) ) {
      return u3m_bail(c3__exit);
    }
    if ( c3n == _unum_bloq(cor, &bloq) ) return u3_none;
    return u3qi_unum_fdp(bloq, av, bv);
  }
