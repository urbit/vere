/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"
#include "softblas.h"

#include <assert.h>
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

  //  $?(%n %u %d %z %a)
  static inline void
  _set_rounding(c3_w a)
  {
    // We could use SoftBLAS set_rounding() to set the SoftFloat
    // mode as well, but it's more explicit to do it here since
    // we may use SoftFloat in any given Lagoon jet and we want
    // you, dear developer, to see this set here.
    fprintf(stderr, "%x %c\n", a, a);
    switch ( a )
    {
    default:
      u3m_bail(c3__fail);
      break;
    // %n - near
    case c3__n:
      softfloat_roundingMode = softfloat_round_near_even;
      softblas_roundingMode = 'n';
      break;
    // %z - zero
    case c3__z:
      softfloat_roundingMode = softfloat_round_minMag;
      softblas_roundingMode = 'z';
      break;
    // %u - up
    case c3__u:
      softfloat_roundingMode = softfloat_round_max;
      softblas_roundingMode = 'u';
      break;
    // %d - down
    case c3__d:
      softfloat_roundingMode = softfloat_round_min;
      softblas_roundingMode = 'd';
      break;
    // %a - away
    case c3__a:
      softfloat_roundingMode = softfloat_round_near_maxMag;
      softblas_roundingMode = 'a';
      break;
    }
  }

/* shape
*/
  static inline uint64_t _get_length(u3_noun shape)
  {
    uint64_t len = 1;
    while (u3_nul != shape) {
      len = len * u3h(shape);
      shape = u3t(shape);
    }
    return len;
  }

/* add - axpy = 1*x+y
*/
  u3_noun
  u3qf_la_add_real(u3_noun a_data,
                   u3_noun b_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * bloq;
    uint8_t* a_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, a_bytes, a_data);
    uint8_t* b_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, b_bytes, b_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        haxpy(len_a, (float16_t){SB_REAL16_ONE}, (float16_t*)a_bytes, 1, (float16_t*)b_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(siz_a*sizeof(uint8_t), b_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);

        return r_data;

      case 5:
        saxpy(len_a, (float32_t){SB_REAL32_ONE}, (float32_t*)a_bytes, 1, (float32_t*)b_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(siz_a*sizeof(uint8_t), b_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);

        return r_data;

      case 6:
        daxpy(len_a, (float64_t){SB_REAL64_ONE}, (float64_t*)a_bytes, 1, (float64_t*)b_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(siz_a*sizeof(uint8_t), b_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);

        return r_data;

      case 7:
        qaxpy(len_a, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)a_bytes, 1, (float128_t*)b_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(siz_a*sizeof(uint8_t), b_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);

        return r_data;

      default:
        u3a_free(a_bytes);
        u3a_free(b_bytes);

        return u3_none;
    }
  }

/* mmul
*/
  u3_noun
  u3qf_la_mmul_real(u3_noun a_data,
                    u3_noun b_data,
                    u3_noun a_shape,
                    u3_noun b_shape,
                    u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t M = u3h(a_shape);
    uint64_t Na = u3h(u3t(a_shape));
    uint64_t Nb = u3h(b_shape);
    uint64_t P = u3h(u3t(b_shape));

    assert(u3_nul == u3t(u3t(a_shape)));
    assert(Na == Nb);
    uint64_t N = Na;
    assert(u3_nul == u3t(u3t(b_shape)));

    uint8_t* a_bytes = (uint8_t*)u3a_malloc((M*N)*sizeof(uint8_t));
    u3r_bytes(0, M*N, a_bytes, a_data);
    uint8_t* b_bytes = (uint8_t*)u3a_malloc((N*P)*sizeof(uint8_t));
    u3r_bytes(0, N*P, b_bytes, b_data);
    uint8_t* c_bytes = (uint8_t*)u3a_malloc((M*P)*sizeof(uint8_t));

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        hgemm('N', 'N', M, N, P, (float16_t){SB_REAL16_ONE}, (float16_t*)a_bytes, N, (float16_t*)b_bytes, N, (float16_t){SB_REAL16_ZERO}, (float16_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      case 5:
        sgemm('N', 'N', M, N, P, (float32_t){SB_REAL32_ONE}, (float32_t*)a_bytes, N, (float32_t*)b_bytes, N, (float32_t){SB_REAL32_ZERO}, (float32_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      case 6:
        dgemm('N', 'N', M, N, P, (float64_t){SB_REAL64_ONE}, (float64_t*)a_bytes, N, (float64_t*)b_bytes, N, (float64_t){SB_REAL64_ZERO}, (float64_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      case 7:
        qgemm('N', 'N', M, N, P, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)a_bytes, N, (float128_t*)b_bytes, N, (float128_t){SB_REAL128L_ZERO,SB_REAL128U_ZERO}, (float128_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(a_bytes);
        u3a_free(b_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      default:
        u3a_free(a_bytes);
        u3a_free(b_bytes);
        u3a_free(c_bytes);
        
        return u3_none;
    }
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
      u3_noun a_shape, a_bloq, a_kind, a_fxp,
              b_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &a_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &b_shape,
                            6, &b_bloq,
                           14, &b_kind,
                           15, &b_fxp,
                            0) ||
           c3n == u3r_sing(a_shape, b_shape) ||
           c3n == u3r_sing(a_bloq, b_bloq) ||
           c3n == u3r_sing(a_kind, b_kind) ||
           //  fxp does not need to match so no check
           c3n == u3r_mean(cor, 30, &rnd, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            _set_rounding(rnd);
            u3_noun r_data = u3qf_la_add_real(a_data, b_data, a_shape, a_bloq);
            return u3nc(u3nq(a_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          // case c3__int2:
          //   return u3qf_la_add_int2(a_data, b_data, a_shape, a_bloq);

          // case c3__uint:
          //   return u3qf_la_add_uint(a_data, b_data, a_shape, a_bloq);

          // case c3__cplx:
          //   _set_rounding(rnd);
          //   return u3qf_la_add_cplx(a_data, b_data, a_shape, a_bloq);

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

  u3_noun
  u3wf_la_mmul(u3_noun cor)
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
      u3_noun a_shape, a_bloq, a_kind, a_fxp,
              b_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &a_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &b_shape,
                            6, &b_bloq,
                           14, &b_kind,
                           15, &b_fxp,
                            0) ||
           c3n == u3r_sing(a_bloq, b_bloq) ||
           c3n == u3r_sing(a_kind, b_kind) ||
           //  fxp does not need to match so no check
           c3n == u3r_mean(cor, 30, &rnd, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            _set_rounding(rnd);
            return u3qf_la_mmul_real(a_data, b_data, a_shape, b_shape, a_bloq);
            break;

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
