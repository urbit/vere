/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "softfloat.h"
#include "softblas.h"

#include <assert.h>
#include <math.h>  // for pow()
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
  u3qf_la_add_real(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, x_data);
    uint8_t* y_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a+1, y_bytes, y_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        haxpy(len_a, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 5:
        saxpy(len_a, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 6:
        daxpy(len_a, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 7:
        qaxpy(len_a, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return u3_none;
    }
  }

/* sub - axpy = -1*y+x
*/
  u3_noun
  u3qf_la_sub_real(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, y_data);  // XXX
    uint8_t* y_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a+1, y_bytes, x_data);  // XXX

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        haxpy(len_a, (float16_t){SB_REAL16_NEGONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 5:
        saxpy(len_a, (float32_t){SB_REAL32_NEGONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 6:
        daxpy(len_a, (float64_t){SB_REAL64_NEGONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 7:
        qaxpy(len_a, (float128_t){SB_REAL128L_NEGONE,SB_REAL128U_NEGONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return u3_none;
    }
  }

/* mul - x.*y
   elementwise multiplication
*/
  u3_noun
  u3qf_la_mul_real(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, x_data);
    uint8_t* y_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a+1, y_bytes, y_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        for (uint64_t i = 0; i < len_a; i++) {
          ((float16_t*)y_bytes)[i] = f16_mul(((float16_t*)x_bytes)[i], ((float16_t*)y_bytes)[i]);
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 5:
        for (uint64_t i = 0; i < len_a; i++) {
          ((float32_t*)y_bytes)[i] = f32_mul(((float32_t*)x_bytes)[i], ((float32_t*)y_bytes)[i]);
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 6:
        for (uint64_t i = 0; i < len_a; i++) {
          ((float64_t*)y_bytes)[i] = f64_mul(((float64_t*)x_bytes)[i], ((float64_t*)y_bytes)[i]);
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 7:
        for (uint64_t i = 0; i < len_a; i++) {
          f128M_mul(&(((float128_t*)y_bytes)[i]), &(((float128_t*)x_bytes)[i]), &(((float128_t*)y_bytes)[i]));
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return u3_none;
    }
  }

/* div - x/y
   elementwise division
*/
  u3_noun
  u3qf_la_div_real(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, x_data);
    uint8_t* y_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a+1, y_bytes, y_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        for (uint64_t i = 0; i < len_a; i++) {
          ((float16_t*)y_bytes)[i] = f16_div(((float16_t*)x_bytes)[i], ((float16_t*)y_bytes)[i]);
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 5:
        for (uint64_t i = 0; i < len_a; i++) {
          ((float32_t*)y_bytes)[i] = f32_div(((float32_t*)x_bytes)[i], ((float32_t*)y_bytes)[i]);
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 6:
        for (uint64_t i = 0; i < len_a; i++) {
          ((float64_t*)y_bytes)[i] = f64_div(((float64_t*)x_bytes)[i], ((float64_t*)y_bytes)[i]);
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 7:
        for (uint64_t i = 0; i < len_a; i++) {
          f128M_div(&(((float128_t*)y_bytes)[i]), &(((float128_t*)x_bytes)[i]), &(((float128_t*)y_bytes)[i]));
        }

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return u3_none;
    }
  }

/* adds - axpy = 1*x+n
*/
  u3_noun
  u3qf_la_adds_real(u3_noun x_data,
                    u3_noun shape,
                    u3_noun bloq,
                    u3_noun n)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, x_data);
    uint8_t* y_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    u3_noun r_data;

    //  Switch on the block size.  We assume that n fits in the target block size; Hoon typecheck should prevent.
    switch (bloq) {
      case 4:
        u3r_bytes(0, 2, (uint8_t*)&n16, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float16_t*)y_bytes)[i] = n16;
        }
        y_bytes[siz_a] = 1;  // pin head
        haxpy(len_a, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 5:
        u3r_bytes(0, 4, (uint8_t*)&n32, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float32_t*)y_bytes)[i] = n32;
        }
        y_bytes[siz_a] = 1;  // pin head
        saxpy(len_a, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 6:
        u3r_bytes(0, 8, (uint8_t*)&n64, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float64_t*)y_bytes)[i] = n64;
        }
        y_bytes[siz_a] = 1;  // pin head
        daxpy(len_a, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 7:
        u3r_bytes(0, 16, (uint8_t*)&n128, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float128_t*)y_bytes)[i] = (float128_t){n128.v[0], n128.v[1]};
        }
        y_bytes[siz_a] = 1;  // pin head
        qaxpy(len_a, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), y_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return u3_none;
    }
  }

/* subs - axpy = -1*n+x
*/
  u3_noun
  u3qf_la_subs_real(u3_noun x_data,
                    u3_noun shape,
                    u3_noun bloq,
                    u3_noun n)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a+1, x_bytes, x_data);
    uint8_t* y_bytes = (uint8_t*)u3a_malloc(siz_a*sizeof(uint8_t));

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    u3_noun r_data;

    //  Switch on the block size.  We assume that n fits in the target block size; Hoon typecheck should prevent.
    switch (bloq) {
      case 4:
        u3r_bytes(0, 2, (uint8_t*)&n16, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float16_t*)y_bytes)[i] = n16;
        }
        haxpy(len_a, (float16_t){SB_REAL16_NEGONE}, (float16_t*)y_bytes, 1, (float16_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 5:
        u3r_bytes(0, 4, (uint8_t*)&n32, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float32_t*)y_bytes)[i] = n32;
        }
        saxpy(len_a, (float32_t){SB_REAL32_NEGONE}, (float32_t*)y_bytes, 1, (float32_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 6:
        u3r_bytes(0, 8, (uint8_t*)&n64, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float64_t*)y_bytes)[i] = n64;
        }
        daxpy(len_a, (float64_t){SB_REAL64_NEGONE}, (float64_t*)y_bytes, 1, (float64_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      case 7:
        u3r_bytes(0, 16, (uint8_t*)&n128, n);
        // set y to [n]
        for (uint64_t i = 0; i < len_a; i++) {
          ((float128_t*)y_bytes)[i] = (float128_t){n128.v[0], n128.v[1]};
        }
        qaxpy(len_a, (float128_t){SB_REAL128L_NEGONE,SB_REAL128U_NEGONE}, (float128_t*)y_bytes, 1, (float128_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);

        return u3_none;
    }
  }

/* muls - x.*[n]
   elementwise multiplication
*/
  u3_noun
  u3qf_la_muls_real(u3_noun x_data,
                    u3_noun shape,
                    u3_noun bloq,
                    u3_noun n)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, x_data);
    x_bytes[siz_a] = 1;  // pin head

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        u3r_bytes(0, 2, (uint8_t*)&n16, n);
        hscal(len_a, n16, (float16_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      case 5:
        u3r_bytes(0, 4, (uint8_t*)&n32, n);
        sscal(len_a, n32, (float32_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      case 6:
        u3r_bytes(0, 8, (uint8_t*)&n64, n);
        dscal(len_a, n64, (float64_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      case 7:
        u3r_bytes(0, 16, (uint8_t*)&(n128.v[0]), n);
        qscal(len_a, n128, (float128_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);

        return u3_none;
    }
  }

/* divs - x/[n]
   elementwise multiplication
*/
  u3_noun
  u3qf_la_divs_real(u3_noun x_data,
                    u3_noun shape,
                    u3_noun bloq,
                    u3_noun n)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t len_a = _get_length(shape);
    uint64_t siz_a = len_a * pow(2, bloq - 3);
    uint8_t* x_bytes = (uint8_t*)u3a_malloc((siz_a+1)*sizeof(uint8_t));
    u3r_bytes(0, siz_a, x_bytes, x_data);
    x_bytes[siz_a] = 1;  // pin head

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        u3r_bytes(0, 2, (uint8_t*)&n16, n);
        n16 = f16_div((float16_t){SB_REAL16_ONE}, n16);
        hscal(len_a, n16, (float16_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      case 5:
        u3r_bytes(0, 4, (uint8_t*)&n32, n);
        n32 = f32_div((float32_t){SB_REAL32_ONE}, n32);
        sscal(len_a, n32, (float32_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      case 6:
        u3r_bytes(0, 8, (uint8_t*)&n64, n);
        n64 = f64_div((float64_t){SB_REAL64_ONE}, n64);
        dscal(len_a, n64, (float64_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      case 7:
        // u3r_bytes(0, 16, (uint8_t*)&(n128.v[0]), n);
        u3l_log("divs: n", n);
        u3r_bytes(0, 16, (uint8_t*)&n128, n);
        fprintf(stderr, "n128: %lx %lx\r\n", n128.v[0], n128.v[1]);
        f128M_div(&((float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}), &n128, &n128);
        fprintf(stderr, "one:  %lx %lx\r\n", SB_REAL128L_ONE, SB_REAL128U_ONE);
        fprintf(stderr, "n128: %lx %lx\r\n", n128.v[0], n128.v[1]);
        qscal(len_a, n128, (float128_t*)x_bytes, 1);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes((siz_a+1)*sizeof(uint8_t), x_bytes);

        //  Clean up.
        u3a_free(x_bytes);

        return r_data;

      default:
        u3a_free(x_bytes);

        return u3_none;
    }
  }

/* mmul
*/
  u3_noun
  u3qf_la_mmul_real(u3_noun x_data,
                    u3_noun y_data,
                    u3_noun x_shape,
                    u3_noun y_shape,
                    u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    uint64_t M = u3h(x_shape);
    uint64_t Na = u3h(u3t(x_shape));
    uint64_t Nb = u3h(y_shape);
    uint64_t P = u3h(u3t(y_shape));

    assert(u3_nul == u3t(u3t(x_shape)));
    assert(Na == Nb);
    uint64_t N = Na;
    assert(u3_nul == u3t(u3t(y_shape)));

    uint8_t* x_bytes = (uint8_t*)u3a_malloc((M*N)*sizeof(uint8_t));
    u3r_bytes(0, M*N, x_bytes, x_data);
    uint8_t* y_bytes = (uint8_t*)u3a_malloc((N*P)*sizeof(uint8_t));
    u3r_bytes(0, N*P, y_bytes, y_data);
    uint8_t* c_bytes = (uint8_t*)u3a_malloc((M*P)*sizeof(uint8_t));

    u3_noun r_data;

    //  Switch on the block size.
    switch (bloq) {
      case 4:
        hgemm('N', 'N', M, N, P, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, N, (float16_t*)y_bytes, N, (float16_t){SB_REAL16_ZERO}, (float16_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      case 5:
        sgemm('N', 'N', M, N, P, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, N, (float32_t*)y_bytes, N, (float32_t){SB_REAL32_ZERO}, (float32_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      case 6:
        dgemm('N', 'N', M, N, P, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, N, (float64_t*)y_bytes, N, (float64_t){SB_REAL64_ZERO}, (float64_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      case 7:
        qgemm('N', 'N', M, N, P, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, N, (float128_t*)y_bytes, N, (float128_t){SB_REAL128L_ZERO,SB_REAL128U_ZERO}, (float128_t*)c_bytes, P);

        //  Unpack the result back into a noun.
        r_data = u3i_bytes(M*P, c_bytes);

        //  Clean up.
        u3a_free(x_bytes);
        u3a_free(y_bytes);
        u3a_free(c_bytes);

        return u3nc(u3nq(u3nl(M, P, u3_none), bloq, c3__real, u3_nul), r_data);

      default:
        u3a_free(x_bytes);
        u3a_free(y_bytes);
        u3a_free(c_bytes);
        
        return u3_none;
    }
  }

  u3_noun
  u3wf_la_add(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data,
            b_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &b_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              y_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &y_shape,
                            6, &b_bloq,
                           14, &b_kind,
                           15, &b_fxp,
                            0) ||
           c3n == u3r_sing(x_shape, y_shape) ||
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
            u3_noun r_data = u3qf_la_add_real(x_data, y_data, x_shape, a_bloq);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_sub(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data,
            b_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &b_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              y_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &y_shape,
                            6, &b_bloq,
                           14, &b_kind,
                           15, &b_fxp,
                            0) ||
           c3n == u3r_sing(x_shape, y_shape) ||
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
            u3_noun r_data = u3qf_la_sub_real(x_data, y_data, x_shape, a_bloq);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_mul(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data,
            b_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &b_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              y_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &y_shape,
                            6, &b_bloq,
                           14, &b_kind,
                           15, &b_fxp,
                            0) ||
           c3n == u3r_sing(x_shape, y_shape) ||
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
            u3_noun r_data = u3qf_la_mul_real(x_data, y_data, x_shape, a_bloq);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_div(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data,
            b_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &b_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              y_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &y_shape,
                            6, &b_bloq,
                           14, &b_kind,
                           15, &b_fxp,
                            0) ||
           c3n == u3r_sing(x_shape, y_shape) ||
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
            u3_noun r_data = u3qf_la_div_real(x_data, y_data, x_shape, a_bloq);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_adds(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           //  shape does not matter so no check
           //  bloq does not matter so no check
           //  kind does not matter so no check
           //  fxp does not need to match so no check
           c3n == u3r_mean(cor, 30, &rnd, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            _set_rounding(rnd);
            u3_noun r_data = u3qf_la_adds_real(x_data, x_shape, a_bloq, n);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_subs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           //  shape does not matter so no check
           //  bloq does not matter so no check
           //  kind does not matter so no check
           //  fxp does not need to match so no check
           c3n == u3r_mean(cor, 30, &rnd, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            _set_rounding(rnd);
            u3_noun r_data = u3qf_la_subs_real(x_data, x_shape, a_bloq, n);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_muls(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           //  shape does not matter so no check
           //  bloq does not matter so no check
           //  kind does not matter so no check
           //  fxp does not need to match so no check
           c3n == u3r_mean(cor, 30, &rnd, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            _set_rounding(rnd);
            u3_noun r_data = u3qf_la_muls_real(x_data, x_shape, a_bloq, n);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wf_la_divs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun a_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           //  shape does not matter so no check
           //  bloq does not matter so no check
           //  kind does not matter so no check
           //  fxp does not need to match so no check
           c3n == u3r_mean(cor, 30, &rnd, 0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (a_kind) {
          case c3__real:
            _set_rounding(rnd);
            u3_noun r_data = u3qf_la_divs_real(x_data, x_shape, a_bloq, n);
            return u3nc(u3nq(x_shape, a_bloq, a_kind, a_fxp), r_data);
            break;

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
    u3_noun a_meta, x_data,
            b_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &a_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &b_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, a_bloq, a_kind, a_fxp,
              y_shape, b_bloq, b_kind, b_fxp,
              rnd;
      if ( c3n == u3r_mean(a_meta,
                            2, &x_shape,
                            6, &a_bloq,
                           14, &a_kind,
                           15, &a_fxp,
                            0) ||
           c3n == u3r_mean(b_meta,
                            2, &y_shape,
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
            return u3qf_la_mmul_real(x_data, y_data, x_shape, y_shape, a_bloq);
            break;

          default:
            return u3_none;
        }
      }
    }
  }
