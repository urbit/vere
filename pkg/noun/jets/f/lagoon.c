/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"
#include "softblas.h"

#include <stdio.h>

  union half {
    float16_t h;
    c3_w c;
  };

  union sing {
    float32_t s;
    c3_w c;
  };

  union doub {
    float64_t d;
    c3_d c;
  };

  union quad {
    float128_t q;
    c3_d c[2];
  };

  static inline void
  _set_rounding(c3_w a)
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

/* add
*/
  u3_noun
  u3qf_la_add_real(u3_noun a_data,
                   u3_noun b_data,
                   u3_noun shape,
                   u3_noun bloq,
                   u3_noun rnd)
  {

    fprintf(stderr, ">>  u3qf_la_add_real\n");

    // SoftBLAS needs to be used here.
    return u3_none;

  //   // Split a into component atoms.
  //   //  (roll shape mul) => 2 x 3 = 6
  //   c3_w size = 1;
  //   u3_atom shp = shape;
  //   while (u3_nul != shp) {
  //     shp = u3t(shp);
  //     size *= shp;
  //   }




  // return u3i_word(len_w);


  //   union sing c, d, e;
  //   _set_rounding(r);
  //   c.c = u3r_word(0, a);
  //   d.c = u3r_word(0, b);
  //   e.s = _nan_unify_s(f32_add(c.s, d.s));

  //   return u3i_words(1, &e.c);
  }

  u3_noun
  u3wf_la_add(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, a_data,
            b_meta, b_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &a_data,
                         u3x_sam_6, &b_meta,
                         u3x_sam_7, &b_data,
                         0) ||
         c3n == u3ud(a_data) ||
         c3n == u3ud(b_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun a_shape, a_bloq, a_kind,
              b_shape, b_bloq, b_kind,
              rnd, fxp;
      if ( c3n == u3r_mean(a_meta,
                           2, &a_shape,
                           6, &a_bloq,
                           7, &a_kind,
                           0) ||
           c3n == u3r_mean(b_meta,
                           2, &b_shape,
                           6, &b_bloq,
                           7, &b_kind,
                           0) ||
           c3n == u3r_sing(a_shape, b_shape) ||
           c3n == u3r_sing(a_bloq, b_bloq) ||
           c3n == u3r_sing(a_kind, b_kind) ||
           c3n == u3r_mean(cor, 60, &rnd, 61, &fxp, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            return u3qf_la_add_real(a_data, b_data, a_shape, a_bloq, rnd);

          // case c3__int2:
          //   return u3qf_la_add_int2(a_data, b_data, a_shape, a_bloq);

          // case c3__uint:
          //   return u3qf_la_add_uint(a_data, b_data, a_shape, a_bloq);

          // case c3__cplx:
          //   return u3qf_la_add_cplx(a_data, b_data, a_shape, a_bloq, rnd);

          // case c3__unum:
          //   return u3qf_la_add_unum(a_data, b_data, a_shape, a_bloq);

          // case c3__fixp:
          //   return u3qf_la_add_fixp(a_data, b_data, a_shape, a_bloq);

          default:
            return u3_none;
        }
      }
    }
  }
