/// @file

#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"

#define QUADNAN 0x7fff800000000000

  union quad {
    float128_t* q;
    c3_w* c;
  };

#ifdef VERE64
  static const c3_w n = 2;
#else
  static const c3_w n = 4;
#endif

  static inline c3_t
  _nan_test(float128_t* a)
  {
    return !f128M_eq(a, a);
  }

  static inline void
  _nan_unify(float128_t* a)
  {
    if ( _nan_test(a) )
    {
      *( (c3_d*)a)    = 0;
      *(((c3_d*)a)+1) = QUADNAN;
    }
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

/* _rq_arr(): defines u3ueq_<fun>, array driver of u3weq_<fun>: len sample
** atoms at arg[0..len-1], then the rounding mode of the door if used.
*/
#define _rq_arr(fun, len, ...)                                \
  u3_noun                                                     \
  u3ueq_##fun(u3_noun* arg)                                   \
  {                                                           \
    for ( c3_w i_w = 0; i_w < len; i_w++ ) {                  \
      if ( c3n == u3ud(arg[i_w]) ) {                          \
        return u3m_bail(c3__exit);                            \
      }                                                       \
    }                                                         \
    return u3qeq_##fun(__VA_ARGS__);                          \
  }

/* add
*/
  u3_noun
  u3qeq_add(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union quad c, d, e;
    _set_rounding(r);
    c.c = alloca(16);
    d.c = alloca(16);
    e.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    f128M_add(c.q, d.q, e.q);
    _nan_unify(e.q);

    u3_atom f = u3i_words(n, e.c);
    return f;
  }

  u3_noun
  u3weq_add(u3_noun cor)
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
      return u3qeq_add(a, b, u3x_at(30, cor));
    }
  }
  _rq_arr(add, 2, arg[0], arg[1], arg[2])

/* sub
*/
  u3_noun
  u3qeq_sub(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union quad c, d, e;
    _set_rounding(r);
    c.c = alloca(16);
    d.c = alloca(16);
    e.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    f128M_sub(c.q, d.q, e.q);
    _nan_unify(e.q);

    u3_atom f = u3i_words(n, e.c);
    return f;
  }

  u3_noun
  u3weq_sub(u3_noun cor)
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
      return u3qeq_sub(a, b, u3x_at(30, cor));
    }
  }
  _rq_arr(sub, 2, arg[0], arg[1], arg[2])

/* mul
*/
  u3_noun
  u3qeq_mul(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union quad c, d, e;
    _set_rounding(r);
    c.c = alloca(16);
    d.c = alloca(16);
    e.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    f128M_mul(c.q, d.q, e.q);
    _nan_unify(e.q);

    u3_atom f = u3i_words(n, e.c);
    return f;
  }

  u3_noun
  u3weq_mul(u3_noun cor)
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
      return u3qeq_mul(a, b, u3x_at(30, cor));
    }
  }
  _rq_arr(mul, 2, arg[0], arg[1], arg[2])

/* div
*/
  u3_noun
  u3qeq_div(u3_atom a,
            u3_atom b,
            u3_atom r)
  {
    union quad c, d, e;
    _set_rounding(r);
    c.c = alloca(16);
    d.c = alloca(16);
    e.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    f128M_div(c.q, d.q, e.q);
    _nan_unify(e.q);

    u3_atom f = u3i_words(n, e.c);
    return f;
  }

  u3_noun
  u3weq_div(u3_noun cor)
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
      return u3qeq_div(a, b, u3x_at(30, cor));
    }
  }
  _rq_arr(div, 2, arg[0], arg[1], arg[2])

/* sqt
*/
  u3_noun
  u3qeq_sqt(u3_atom a,
            u3_atom r)
  {
    union quad c, d;
    _set_rounding(r);
    c.c = alloca(16);
    d.c = alloca(16);

    u3r_words(0, n, c.c, a);
    f128M_sqrt(c.q, d.q);
    _nan_unify(d.q);

    u3_atom e = u3i_words(n, d.c);
    return e;
  }

  u3_noun
  u3weq_sqt(u3_noun cor)
  {
    u3_weak a;

    if ( u3_none == (a = u3r_at(u3x_sam, cor)) ||
         c3n == u3ud(a) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qeq_sqt(a, u3x_at(30, cor));
    }
  }
  _rq_arr(sqt, 1, arg[0], arg[1])

/* fma
*/
  u3_noun
  u3qeq_fma(u3_atom a,
            u3_atom b,
            u3_atom c,
            u3_atom r)
  {
    union quad d, e, f, g;
    _set_rounding(r);
    d.c = alloca(16);
    e.c = alloca(16);
    f.c = alloca(16);
    g.c = alloca(16);

    u3r_words(0, n, d.c, a);
    u3r_words(0, n, e.c, b);
    u3r_words(0, n, f.c, c);
    f128M_mulAdd(d.q, e.q, f.q, g.q);
    _nan_unify(g.q);

    u3_atom h = u3i_words(n, g.c);
    return h;
  }

  u3_noun
  u3weq_fma(u3_noun cor)
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
      return u3qeq_fma(a, b, c, u3x_at(30, cor));
    }
  }
  _rq_arr(fma, 3, arg[0], arg[1], arg[2], arg[3])

/* lth
*/
  u3_noun
  u3qeq_lth(u3_atom a,
            u3_atom b)
  {
    union quad c, d;
    c.c = alloca(16);
    d.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    c3_o e = __(f128M_lt(c.q, d.q));

    return e;
  }

  u3_noun
  u3weq_lth(u3_noun cor)
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
      return u3qeq_lth(a, b);
    }
  }
  _rq_arr(lth, 2, arg[0], arg[1])

/* lte
*/
  u3_noun
  u3qeq_lte(u3_atom a,
            u3_atom b)
  {
    union quad c, d;
    c.c = alloca(16);
    d.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    c3_o e = __(f128M_le(c.q, d.q));

    return e;
  }

  u3_noun
  u3weq_lte(u3_noun cor)
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
      return u3qeq_lte(a, b);
    }
  }
  _rq_arr(lte, 2, arg[0], arg[1])

/* equ
*/
  u3_noun
  u3qeq_equ(u3_atom a,
            u3_atom b)
  {
    union quad c, d;
    c.c = alloca(16);
    d.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    c3_o e = __(f128M_eq(c.q, d.q));

    return e;
  }

  u3_noun
  u3weq_equ(u3_noun cor)
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
      return u3qeq_equ(a, b);
    }
  }
  _rq_arr(equ, 2, arg[0], arg[1])

/* gte
*/
  u3_noun
  u3qeq_gte(u3_atom a,
            u3_atom b)
  {
    union quad c, d;
    c.c = alloca(16);
    d.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    c3_o e = __(f128M_le(d.q, c.q));

    return e;
  }

  u3_noun
  u3weq_gte(u3_noun cor)
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
      return u3qeq_gte(a, b);
    }
  }
  _rq_arr(gte, 2, arg[0], arg[1])

/* gth
*/
  u3_noun
  u3qeq_gth(u3_atom a,
            u3_atom b)
  {
    union quad c, d;
    c.c = alloca(16);
    d.c = alloca(16);

    u3r_words(0, n, c.c, a);
    u3r_words(0, n, d.c, b);
    c3_o e = __(f128M_lt(d.q, c.q));

    return e;
  }

  u3_noun
  u3weq_gth(u3_noun cor)
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
      return u3qeq_gth(a, b);
    }
  }
  _rq_arr(gth, 2, arg[0], arg[1])
