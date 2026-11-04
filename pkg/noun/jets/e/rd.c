/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"

#define DOUBNAN 0x7ff8000000000000

  union doub {
    float64_t d;
    c3_d c;
  };

  static inline c3_t
  _nan_test(float64_t a)
  {
    return !f64_eq(a, a);
  }

  static inline float64_t
  _nan_unify(float64_t a)
  {
    if ( _nan_test(a) )
    {
      *(c3_d*)(&a) = DOUBNAN;
    }
    return a;
  }

  static inline void
  _set_rounding(u3_atom a)
  {
    switch ( a )
    {
    default:
      u3m_bail(c3__fail);
      break;
    case c3__n:
      softfloat_roundingMode = softfloat_round_near_even;
      break;
    case c3__z:
      softfloat_roundingMode = softfloat_round_minMag;
      break;
    case c3__u:
      softfloat_roundingMode = softfloat_round_max;
      break;
    case c3__d:
      softfloat_roundingMode = softfloat_round_min;
      break;
    }
  }

/* _rd_arr(): defines u3uer_<fun>, array driver of u3wer_<fun>: len sample
** atoms at arg[0..len-1], then the rounding mode of the door if used.
*/
#define _rd_arr(fun, len, ...)                                \
  u3_noun                                                     \
  u3uer_##fun(u3_noun* arg)                                   \
  {                                                           \
    for ( c3_w i_w = 0; i_w < len; i_w++ ) {                  \
      if ( c3n == u3ud(arg[i_w]) ) {                          \
        return u3m_bail(c3__exit);                            \
      }                                                       \
    }                                                         \
    return u3qer_##fun(__VA_ARGS__);                          \
  }

/* add
*/
  u3_noun
  u3qer_add(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union doub c, d, e;
    _set_rounding(r);
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);
    e.d = _nan_unify(f64_add(c.d, d.d));

    return u3i_chubs(1, &e.c);
  }

  u3_noun
  u3wer_add(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_add(a, b, u3x_at(30, cor));
    }
  }
  _rd_arr(add, 2, arg[0], arg[1], arg[2])

/* sub
*/
  u3_noun
  u3qer_sub(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union doub c, d, e;
    _set_rounding(r);
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);
    e.d = _nan_unify(f64_sub(c.d, d.d));

    return u3i_chubs(1, &e.c);
  }

  u3_noun
  u3wer_sub(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_sub(a, b, u3x_at(30, cor));
    }
  }
  _rd_arr(sub, 2, arg[0], arg[1], arg[2])

/* mul
*/
  u3_noun
  u3qer_mul(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union doub c, d, e;
    _set_rounding(r);
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);
    e.d = _nan_unify(f64_mul(c.d, d.d));

    return u3i_chubs(1, &e.c);
  }

  u3_noun
  u3wer_mul(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_mul(a, b, u3x_at(30, cor));
    }
  }
  _rd_arr(mul, 2, arg[0], arg[1], arg[2])

/* div
*/
  u3_noun
  u3qer_div(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union doub c, d, e;
    _set_rounding(r);
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);
    e.d = _nan_unify(f64_div(c.d, d.d));

    return u3i_chubs(1, &e.c);
  }

  u3_noun
  u3wer_div(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_div(a, b, u3x_at(30, cor));
    }
  }
  _rd_arr(div, 2, arg[0], arg[1], arg[2])

/* sqt
*/
  u3_noun
  u3qer_sqt(u3_atom a,
            u3_atom r)
  {
    union doub c, d;
    _set_rounding(r);
    c.c = u3r_chub(0, a);
    d.d = _nan_unify(f64_sqrt(c.d));

    return u3i_chubs(1, &d.c);
  }

  u3_noun
  u3wer_sqt(u3_noun cor)
  {
    u3_weak a;

    if ( u3_none == (a = u3r_at(u3x_sam, cor)) ||
         c3n == u3ud(a) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_sqt(a, u3x_at(30, cor));
    }
  }
  _rd_arr(sqt, 1, arg[0], arg[1])

/* fma
*/
  u3_noun
  u3qer_fma(u3_atom a,
            u3_atom b,
            u3_atom c,
            u3_atom r)
  {
    union doub d, e, f, g;
    _set_rounding(r);
    d.c = u3r_chub(0, a);
    e.c = u3r_chub(0, b);
    f.c = u3r_chub(0, c);
    g.d = _nan_unify(f64_mulAdd(d.d, e.d, f.d));

    return u3i_chubs(1, &g.c);
  }

  u3_noun
  u3wer_fma(u3_noun cor)
  {
    u3_noun a, b, c;

    a = u3h(u3h(u3t(cor)));
    b = u3h(u3t(u3h(u3t(cor))));
    c = u3t(u3t(u3h(u3t(cor))));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) ||
         c3n == u3ud(c) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_fma(a, b, c, u3x_at(30, cor));
    }
  }
  _rd_arr(fma, 3, arg[0], arg[1], arg[2], arg[3])

/* lth
*/
  u3_noun
  u3qer_lth(u3_atom a,
            u3_atom b)
  {
    union doub c, d;
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);

    return __(f64_lt(c.d, d.d));
  }

  u3_noun
  u3wer_lth(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_lth(a, b);
    }
  }
  _rd_arr(lth, 2, arg[0], arg[1])

/* lte
*/
  u3_noun
  u3qer_lte(u3_atom a,
            u3_atom b)
  {
    union doub c, d;
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);

    return __(f64_le(c.d, d.d));
  }

  u3_noun
  u3wer_lte(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_lte(a, b);
    }
  }
  _rd_arr(lte, 2, arg[0], arg[1])

/* equ
*/
  u3_noun
  u3qer_equ(u3_atom a,
            u3_atom b)
  {
    union doub c, d;
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);

    return __(f64_eq(c.d, d.d));
  }

  u3_noun
  u3wer_equ(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_equ(a, b);
    }
  }
  _rd_arr(equ, 2, arg[0], arg[1])

/* gte
*/
  u3_noun
  u3qer_gte(u3_atom a,
            u3_atom b)
  {
    union doub c, d;
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);

    return __(f64_le(d.d, c.d));
  }

  u3_noun
  u3wer_gte(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_gte(a, b);
    }
  }
  _rd_arr(gte, 2, arg[0], arg[1])

/* gth
*/
  u3_noun
  u3qer_gth(u3_atom a,
            u3_atom b)
  {
    union doub c, d;
    c.c = u3r_chub(0, a);
    d.c = u3r_chub(0, b);

    return __(f64_lt(d.d, c.d));
  }

  u3_noun
  u3wer_gth(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( c3n == u3ud(a) ||
         c3n == u3ud(b) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qer_gth(a, b);
    }
  }
  _rd_arr(gth, 2, arg[0], arg[1])
