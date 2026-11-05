/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"

#define SINGNAN 0x7fc00000

  union sing {
    float32_t s;
    c3_h c;
  };

  static inline c3_t
  _nan_test(float32_t a)
  {
    return !f32_eq(a, a);
  }

  static inline float32_t
  _nan_unify(float32_t a)
  {
    if ( _nan_test(a) )
    {
      *(c3_h*)(&a) = SINGNAN;
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

/* _rs_arr(): defines u3uet_<fun>, array driver of u3wet_<fun>: len sample
** atoms at arg[0..len-1], then the rounding mode of the door if used.
*/
#define _rs_arr(fun, len, ...)                                \
  u3_noun                                                     \
  u3uet_##fun(u3_noun* arg)                                   \
  {                                                           \
    for ( c3_w i_w = 0; i_w < len; i_w++ ) {                  \
      if ( c3n == u3ud(arg[i_w]) ) {                          \
        return u3m_bail(c3__exit);                            \
      }                                                       \
    }                                                         \
    return u3qet_##fun(__VA_ARGS__);                          \
  }

/* add
*/
  u3_noun
  u3qet_add(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union sing c, d, e;
    _set_rounding(r);
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);
    e.s = _nan_unify(f32_add(c.s, d.s));

    return u3i_halfs(1, &e.c);
  }

  u3_noun
  u3wet_add(u3_noun cor)
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
      return u3qet_add(a, b, u3x_at(30, cor));
    }
  }
  _rs_arr(add, 2, arg[0], arg[1], arg[2])

/* sub
*/
  u3_noun
  u3qet_sub(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union sing c, d, e;
    _set_rounding(r);
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);
    e.s = _nan_unify(f32_sub(c.s, d.s));

    return u3i_halfs(1, &e.c);
  }

  u3_noun
  u3wet_sub(u3_noun cor)
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
      return u3qet_sub(a, b, u3x_at(30, cor));
    }
  }
  _rs_arr(sub, 2, arg[0], arg[1], arg[2])

/* mul
*/
  u3_noun
  u3qet_mul(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union sing c, d, e;
    _set_rounding(r);
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);
    e.s = _nan_unify(f32_mul(c.s, d.s));

    return u3i_halfs(1, &e.c);
  }

  u3_noun
  u3wet_mul(u3_noun cor)
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
      return u3qet_mul(a, b, u3x_at(30, cor));
    }
  }
  _rs_arr(mul, 2, arg[0], arg[1], arg[2])

/* div
*/
  u3_noun
  u3qet_div(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union sing c, d, e;
    _set_rounding(r);
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);
    e.s = _nan_unify(f32_div(c.s, d.s));

    return u3i_halfs(1, &e.c);
  }

  u3_noun
  u3wet_div(u3_noun cor)
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
      return u3qet_div(a, b, u3x_at(30, cor));
    }
  }
  _rs_arr(div, 2, arg[0], arg[1], arg[2])

/* sqt
*/
  u3_noun
  u3qet_sqt(u3_atom a,
            u3_atom r)
  {
    union sing c, d;
    _set_rounding(r);
    c.c = u3r_half(0, a);
    d.s = _nan_unify(f32_sqrt(c.s));

    return u3i_halfs(1, &d.c);
  }

  u3_noun
  u3wet_sqt(u3_noun cor)
  {
    u3_weak a;

    if ( u3_none == (a = u3r_at(u3x_sam, cor)) ||
         c3n == u3ud(a) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qet_sqt(a, u3x_at(30, cor));
    }
  }
  _rs_arr(sqt, 1, arg[0], arg[1])

/* fma
*/
  u3_noun
  u3qet_fma(u3_atom a,
            u3_atom b,
            u3_atom c,
            u3_atom r)
  {
    union sing d, e, f, g;
    _set_rounding(r);
    d.c = u3r_half(0, a);
    e.c = u3r_half(0, b);
    f.c = u3r_half(0, c);
    g.s = _nan_unify(f32_mulAdd(d.s, e.s, f.s));

    return u3i_halfs(1, &g.c);
  }

  u3_noun
  u3wet_fma(u3_noun cor)
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
      return u3qet_fma(a, b, c, u3x_at(30, cor));
    }
  }
  _rs_arr(fma, 3, arg[0], arg[1], arg[2], arg[3])

/* lth
*/
  u3_noun
  u3qet_lth(u3_atom a,
            u3_atom b)
  {
    union sing c, d;
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);

    return __(f32_lt(c.s, d.s));
  }

  u3_noun
  u3wet_lth(u3_noun cor)
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
      return u3qet_lth(a, b);
    }
  }
  _rs_arr(lth, 2, arg[0], arg[1])

/* lte
*/
  u3_noun
  u3qet_lte(u3_atom a,
            u3_atom b)
  {
    union sing c, d;
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);

    return __(f32_le(c.s, d.s));
  }

  u3_noun
  u3wet_lte(u3_noun cor)
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
      return u3qet_lte(a, b);
    }
  }
  _rs_arr(lte, 2, arg[0], arg[1])

/* equ
*/
  u3_noun
  u3qet_equ(u3_atom a,
            u3_atom b)
  {
    union sing c, d;
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);

    return __(f32_eq(c.s, d.s));
  }

  u3_noun
  u3wet_equ(u3_noun cor)
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
      return u3qet_equ(a, b);
    }
  }
  _rs_arr(equ, 2, arg[0], arg[1])

/* gte
*/
  u3_noun
  u3qet_gte(u3_atom a,
            u3_atom b)
  {
    union sing c, d;
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);

    return __(f32_le(d.s, c.s));
  }

  u3_noun
  u3wet_gte(u3_noun cor)
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
      return u3qet_gte(a, b);
    }
  }
  _rs_arr(gte, 2, arg[0], arg[1])

/* gth
*/
  u3_noun
  u3qet_gth(u3_atom a,
            u3_atom b)
  {
    union sing c, d;
    c.c = u3r_half(0, a);
    d.c = u3r_half(0, b);

    return __(f32_lt(d.s, c.s));
  }

  u3_noun
  u3wet_gth(u3_noun cor)
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
      return u3qet_gth(a, b);
    }
  }
  _rs_arr(gth, 2, arg[0], arg[1])
