/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "c3/motes.h"

#include "noun.h"
#include "softfloat.h"
#include "softblas.h"

#include <math.h>  // for pow()
#include <stdio.h>

#define f16_ceil(a) f16_roundToInt( a, softfloat_round_max, false )
#define f32_ceil(a) f32_roundToInt( a, softfloat_round_max, false )
#define f64_ceil(a) f64_roundToInt( a, softfloat_round_max, false )
#define f128M_ceil(a, b) f128M_roundToInt( a, softfloat_round_max, false, b )

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
    // you, dear developer, to see it set here.
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

/* length of shape = x * y * z * w * ...
*/
  static inline c3_d _get_length(u3_noun shape)
  {
    c3_d len = 1;
    while (u3_nul != shape) {
      len = len * u3x_atom(u3h(shape));
      shape = u3t(shape);
    }
    return len;
  }

/* get dims from shape as array [x y z w ...]
*/
  static inline c3_d* _get_dims(u3_noun shape)
  {
    u3_atom len = u3qb_lent(shape);
    c3_d len_d = u3r_chub(0, len);
    c3_d* dims = (c3_d*)u3a_malloc(len_d*sizeof(c3_d));
    for (c3_d i = 0; i < len_d; i++) {
      dims[i] = u3r_chub(0, u3x_atom(u3h(shape)));
      shape = u3t(shape);
    }
    u3z(len);
    return dims;
  }

/* check consistency of array shape and bloq size
    |=  =ray
    ^-  ?
    .=  (roll shape.meta.ray ^mul)
    (dec (met bloq.meta.ray data.ray))
*/
  static inline c3_o _check(u3_noun ray)
  {
    //  Calculate expected size.
    u3_atom shp = u3h(u3h(ray));        // (reported) shape of ray, +4
    u3_atom blq = u3h(u3t(u3h(ray)));   // block size of ray, +10
    u3_atom sin = _get_length(shp);     // calculated length of ray

    //  Calculate actual size.
    u3_atom len = u3r_met(blq, u3t(ray));   // length of ray
    u3_atom dex = u3qa_dec(len);            // decrement length b/c of pinned 1

    return __(sin == dex);
  }

/* add - axpy = 1*x+y
*/
  u3_noun
  u3qi_la_add_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq
                   )
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);
    
    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        haxpy(len_x, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        break;

      case 5:
        saxpy(len_x, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        break;

      case 6:
        daxpy(len_x, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        break;

      case 7:
        qaxpy(len_x, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* sub - axpy = -1*y+x
*/
  u3_noun
  u3qi_la_sub_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq
                   )
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);
    
    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        haxpy(len_x, (float16_t){SB_REAL16_NEGONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        break;

      case 5:
        saxpy(len_x, (float32_t){SB_REAL32_NEGONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        break;

      case 6:
        daxpy(len_x, (float64_t){SB_REAL64_NEGONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        break;

      case 7:
        qaxpy(len_x, (float128_t){SB_REAL128L_NEGONE,SB_REAL128U_NEGONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }


/* mul - x.*y
   elementwise multiplication
*/
  u3_noun
  u3qi_la_mul_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = f16_mul(((float16_t*)x_bytes)[i], ((float16_t*)y_bytes)[i]);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = f32_mul(((float32_t*)x_bytes)[i], ((float32_t*)y_bytes)[i]);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = f64_mul(((float64_t*)x_bytes)[i], ((float64_t*)y_bytes)[i]);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          f128M_mul(&(((float128_t*)y_bytes)[i]), &(((float128_t*)x_bytes)[i]), &(((float128_t*)y_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* div - x/y
   elementwise division
*/
  u3_noun
  u3qi_la_div_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = f16_div(((float16_t*)x_bytes)[i], ((float16_t*)y_bytes)[i]);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = f32_div(((float32_t*)x_bytes)[i], ((float32_t*)y_bytes)[i]);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = f64_div(((float64_t*)x_bytes)[i], ((float64_t*)y_bytes)[i]);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          f128M_div(&(((float128_t*)y_bytes)[i]), &(((float128_t*)x_bytes)[i]), &(((float128_t*)y_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* mod - x % y = x - r*floor(x/r)
   remainder after division
*/
  u3_noun
  u3qi_la_mod_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          // Perform division x/n
          float16_t div_result16 = f16_div(x_val16, y_val16);
          // Compute floor of the division result
          c3_ds floor_result16 = f16_to_i64(div_result16, softfloat_round_minMag, false);
          float16_t floor_float16 = i64_to_f16(floor_result16);
          // Multiply n by floor(x/n)
          float16_t mult_result16 = f16_mul(y_val16, floor_float16);
          // Compute remainder: x - n * floor(x/n)
          ((float16_t*)y_bytes)[i] = f16_sub(x_val16, mult_result16);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          // Perform division x/n
          float32_t div_result32 = f32_div(x_val32, y_val32);
          // Compute floor of the division result
          c3_ds floor_result32 = f32_to_i64(div_result32, softfloat_round_minMag, false);
          float32_t floor_float32 = i64_to_f32(floor_result32);
          // Multiply n by floor(x/n)
          float32_t mult_result32 = f32_mul(y_val32, floor_float32);
          // Compute remainder: x - n * floor(x/n)
          ((float32_t*)y_bytes)[i] = f32_sub(x_val32, mult_result32);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          // Perform division x/n
          float64_t div_result64 = f64_div(x_val64, y_val64);
          // Compute floor of the division result
          c3_ds floor_result64 = f64_to_i64(div_result64, softfloat_round_minMag, false);
          float64_t floor_float64 = i64_to_f64(floor_result64);
          // Multiply n by floor(x/n)
          float64_t mult_result64 = f64_mul(y_val64, floor_float64);
          // Compute remainder: x - n * floor(x/n)
          ((float64_t*)y_bytes)[i] = f64_sub(x_val64, mult_result64);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          // Perform division x/n
          float128_t div_result128;
          f128M_div((float128_t*)&x_val128, (float128_t*)&y_val128, (float128_t*)&div_result128);
          // Compute floor of the division result
          c3_ds floor_result128 = f128M_to_i64(&div_result128, softfloat_round_minMag, false);
          float128_t floor_float128;
          i64_to_f128M(floor_result128, &floor_float128);
          // Multiply n by floor(x/n)
          float128_t mult_result128;
          f128M_mul(((float128_t*)&y_val128), ((float128_t*)&floor_float128), ((float128_t*)&mult_result128));
          // Compute remainder: x - n * floor(x/n)
          f128M_sub(((float128_t*)&x_val128), ((float128_t*)&mult_result128), &(((float128_t*)y_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* cumsum - x[0] + x[1] + ... x[n]
*/
  u3_noun
  u3qi_la_cumsum_i754(u3_noun x_data,
                      u3_noun shape,
                      u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // y_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t sum16[2];
        sum16[0] = (float16_t){SB_REAL16_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          sum16[0] = f16_add(sum16[0], ((float16_t*)x_bytes)[i-1]);
        }
        sum16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)sum16);
        break;}

      case 5: {
        float32_t sum32[2];
        sum32[0] = (float32_t){SB_REAL32_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          sum32[0] = f32_add(sum32[0], ((float32_t*)x_bytes)[i-1]);
        }
        sum32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)sum32);
        break;}

      case 6: {
        float64_t sum64[2];
        sum64[0] = (float64_t){SB_REAL64_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          sum64[0] = f64_add(sum64[0], ((float64_t*)x_bytes)[i-1]);
        }
        sum64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)sum64);
        break;}

      case 7: {
        float128_t sum128[2];
        sum128[0] = (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        for (c3_d i = len_x; i > 0; i--) {
          f128M_add(&(sum128[0]), &(((float128_t*)x_bytes)[i-1]), &(sum128[0]));
        }
        sum128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)sum128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* argmin - argmin(x)
*/
  u3_noun
  u3qi_la_argmin_i754(u3_noun x_data,
                      u3_noun shape,
                      u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1, which doesn't matter here)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    c3_w min_idx = 0;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t min_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f16_lt(((float16_t*)x_bytes)[i], min_val16)) {
             min_val16 = ((float16_t*)x_bytes)[i];
             min_idx = (len_x - i - 1);
           }
        }
        break;}

      case 5: {
        float32_t min_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f32_lt(((float32_t*)x_bytes)[i], min_val32)) {
             min_val32 = ((float32_t*)x_bytes)[i];
             min_idx = (len_x - i - 1);
           }
        }
        break;}

      case 6: {
        float64_t min_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f64_lt(((float64_t*)x_bytes)[i], min_val64)) {
             min_val64 = ((float64_t*)x_bytes)[i];
             min_idx = (len_x - i - 1);
           }
        }
        break;}

      case 7: {
        float128_t min_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f128M_lt(&(((float128_t*)x_bytes)[i]), &min_val128)) {
             min_val128 = *f128M_min(&min_val128, &((float128_t*)x_bytes)[i]);
             min_idx = (len_x - i - 1);
           }
        }
        break;}
    }

    u3_noun r_data = u3i_chub(min_idx);

    return r_data;
  }

/* argmax - argmax(x)
*/
  u3_noun
  u3qi_la_argmax_i754(u3_noun x_data,
                      u3_noun shape,
                      u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1, which doesn't matter here)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    c3_w max_idx = 0;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t max_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f16_gt(((float16_t*)x_bytes)[i], max_val16)) {
             max_val16 = ((float16_t*)x_bytes)[i];
             max_idx = (len_x - i - 1);
           }
        }
        break;}

      case 5: {
        float32_t max_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f32_gt(((float32_t*)x_bytes)[i], max_val32)) {
             max_val32 = ((float32_t*)x_bytes)[i];
             max_idx = (len_x - i - 1);
           }
        }
        break;}

      case 6: {
        float64_t max_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f64_gt(((float64_t*)x_bytes)[i], max_val64)) {
             max_val64 = ((float64_t*)x_bytes)[i];
             max_idx = (len_x - i - 1);
           }
        }
        break;}

      case 7: {
        float128_t max_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
           if(f128M_gt(&(((float128_t*)x_bytes)[i]), &max_val128)) {
             max_val128 = *f128M_max(&max_val128, &((float128_t*)x_bytes)[i]);
             max_idx = (len_x - i - 1);
           }
        }
        break;}
    }

    u3_noun r_data = u3i_chub(max_idx);

    return r_data;
  }

/* ravel - x -> ~[x[0], x[1], ... x[n]]
   entire nd-array busted out as a linear list
*/
  u3_noun
  u3qi_la_ravel_i754(u3_noun x_data,
                     u3_noun shape,
                     u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // r_data is the result noun of [data]
    u3_noun r_data = u3_nul;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          r_data = u3nc(u3i_word(x_val16.v), r_data);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          r_data = u3nc(u3i_word(x_val32.v), r_data);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          r_data = u3nc(u3i_chub(x_val64.v), r_data);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          r_data = u3nc(u3i_chubs(2, (c3_d*)&(x_val128.v)), r_data);
        }
        break;
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* min - min(x,y)
*/
  u3_noun
  u3qi_la_min_i754(u3_noun x_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t min_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val16 = f16_min(min_val16, ((float16_t*)x_bytes)[i]);
        }
        float16_t r16[2];
        r16[0] = min_val16;
        r16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)r16);
        break;}

      case 5: {
        float32_t min_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val32 = f32_min(min_val32, ((float32_t*)x_bytes)[i]);
        }
        float32_t r32[2];
        r32[0] = min_val32;
        r32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)r32);
        break;}

      case 6: {
        float64_t min_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val64 = f64_min(min_val64, ((float64_t*)x_bytes)[i]);
        }
        float64_t r64[2];
        r64[0] = min_val64;
        r64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)r64);
        break;}

      case 7: {
        float128_t min_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          min_val128 = *f128M_min(&min_val128, &((float128_t*)x_bytes)[i]);
        }
        float128_t r128[2];
        r128[0] = min_val128;
        r128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)r128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* max - max(x,y)
*/
  u3_noun
  u3qi_la_max_i754(u3_noun x_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t max_val16 = ((float16_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val16 = f16_max(max_val16, ((float16_t*)x_bytes)[i]);
        }
        float16_t r16[2];
        r16[0] = max_val16;
        r16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)r16);
        break;}

      case 5: {
        float32_t max_val32 = ((float32_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val32 = f32_max(max_val32, ((float32_t*)x_bytes)[i]);
        }
        float32_t r32[2];
        r32[0] = max_val32;
        r32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)r32);
        break;}

      case 6: {
        float64_t max_val64 = ((float64_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val64 = f64_max(max_val64, ((float64_t*)x_bytes)[i]);
        }
        float64_t r64[2];
        r64[0] = max_val64;
        r64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)r64);
        break;}

      case 7: {
        float128_t max_val128 = ((float128_t*)x_bytes)[0];
        for (c3_d i = 0; i < len_x; i++) {
          max_val128 = *f128M_max(&max_val128, &((float128_t*)x_bytes)[i]);
        }
        float128_t r128[2];
        r128[0] = max_val128;
        r128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)r128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* abs - |x|
*/
  u3_noun
  u3qi_la_abs_i754(u3_noun x_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by for range)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)x_bytes)[i] = f16_abs(((float16_t*)x_bytes)[i]);
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)x_bytes)[i] = f32_abs(((float32_t*)x_bytes)[i]);
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)x_bytes)[i] = f64_abs(((float64_t*)x_bytes)[i]);
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          ((float128_t*)x_bytes)[i] = f128_abs(((float128_t*)x_bytes)[i]);
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* gth - x > y
*/
  u3_noun
  u3qi_la_gth_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_gt(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_gt(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_gt(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_gt(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* gte - x > y
*/
  u3_noun
  u3qi_la_gte_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_ge(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_ge(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_ge(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_ge(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* lth - x > y
*/
  u3_noun
  u3qi_la_lth_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_lt(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_lt(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_lt(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_lt(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* lte - x > y
*/
  u3_noun
  u3qi_la_lte_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          float16_t y_val16 = ((float16_t*)y_bytes)[i];
          ((float16_t*)y_bytes)[i] = f16_le(x_val16, y_val16) ? (float16_t){SB_REAL16_ONE} : (float16_t){SB_REAL16_ZERO};
        }
        break;

      case 5:
        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          float32_t y_val32 = ((float32_t*)y_bytes)[i];
          ((float32_t*)y_bytes)[i] = f32_le(x_val32, y_val32) ? (float32_t){SB_REAL32_ONE} : (float32_t){SB_REAL32_ZERO};
        }
        break;

      case 6:
        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          float64_t y_val64 = ((float64_t*)y_bytes)[i];
          ((float64_t*)y_bytes)[i] = f64_le(x_val64, y_val64) ? (float64_t){SB_REAL64_ONE} : (float64_t){SB_REAL64_ZERO};
        }
        break;

      case 7:
        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          float128_t y_val128 = ((float128_t*)y_bytes)[i];
          ((float128_t*)y_bytes)[i] = f128M_le(((float128_t*)&x_val128), ((float128_t*)&y_val128)) ? (float128_t){SB_REAL128L_ONE, SB_REAL128U_ONE} : (float128_t){SB_REAL128L_ZERO, SB_REAL128U_ZERO};
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* adds - axpy = 1*x+[n]
*/
  u3_noun
  u3qi_la_adds_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    //  Switch on the block size.  We assume that n fits in the target block size; Hoon typecheck should prevent.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = n16;
        }
        haxpy(len_x, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = n32;
        }
        saxpy(len_x, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = n64;
        }
        daxpy(len_x, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float128_t*)y_bytes)[i] = (float128_t){n128.v[0], n128.v[1]};
        }
        qaxpy(len_x, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    y_bytes[syz_x] = 0x1;  // pin head
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* subs - axpy = -1*[n]+x
*/
  u3_noun
  u3qi_la_subs_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/o leading 0x1)
    c3_y* y_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    //  Switch on the block size.  We assume that n fits in the target block size; Hoon typecheck should prevent.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float16_t*)y_bytes)[i] = n16;
        }
        haxpy(len_x, (float16_t){SB_REAL16_NEGONE}, (float16_t*)y_bytes, 1, (float16_t*)x_bytes, 1);
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float32_t*)y_bytes)[i] = n32;
        }
        saxpy(len_x, (float32_t){SB_REAL32_NEGONE}, (float32_t*)y_bytes, 1, (float32_t*)x_bytes, 1);
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float64_t*)y_bytes)[i] = n64;
        }
        daxpy(len_x, (float64_t){SB_REAL64_NEGONE}, (float64_t*)y_bytes, 1, (float64_t*)x_bytes, 1);
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        // set y to [n]
        for (c3_d i = 0; i < len_x; i++) {
          ((float128_t*)y_bytes)[i] = (float128_t){n128.v[0], n128.v[1]};
        }
        qaxpy(len_x, (float128_t){SB_REAL128L_NEGONE,SB_REAL128U_NEGONE}, (float128_t*)y_bytes, 1, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    x_bytes[syz_x] = 0x1;  // pin head
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* muls - ?scal n * x
   elementwise multiplication
*/
  u3_noun
  u3qi_la_muls_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);
    x_bytes[syz_x] = 0x1;  // pin head

    float16_t n16;
    float32_t n32;
    float64_t n64;
    float128_t n128;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        hscal(len_x, n16, (float16_t*)x_bytes, 1);
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        sscal(len_x, n32, (float32_t*)x_bytes, 1);
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        dscal(len_x, n64, (float64_t*)x_bytes, 1);
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        qscal(len_x, n128, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* divs - ?scal 1/n * x
   elementwise division
*/
  u3_noun
  u3qi_la_divs_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);
    x_bytes[syz_x] = 0x1;  // pin head

    float16_t in16;
    float32_t in32;
    float64_t in64;
    float128_t in128;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        //  XX note that in16 is doing double duty here
        u3r_bytes(0, 2, (c3_y*)&(in16.v), n);
        in16 = f16_div((float16_t){SB_REAL16_ONE}, in16);
        hscal(len_x, in16, (float16_t*)x_bytes, 1);
        break;

      case 5:
        //  XX note that in32 is doing double duty here
        u3r_bytes(0, 4, (c3_y*)&(in32.v), n);
        in32 = f32_div((float32_t){SB_REAL32_ONE}, in32);
        sscal(len_x, in32, (float32_t*)x_bytes, 1);
        break;

      case 6:
        //  XX note that in64 is doing double duty here
        u3r_bytes(0, 8, (c3_y*)&(in64.v), n);
        in64 = f64_div((float64_t){SB_REAL64_ONE}, in64);
        dscal(len_x, in64, (float64_t*)x_bytes, 1);
        break;

      case 7:
        //  XX note that in128 is doing double duty here
        u3r_bytes(0, 16, (c3_y*)&(in128.v[0]), n);
        f128M_div(&((float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}), &in128, &in128);
        qscal(len_x, in128, (float128_t*)x_bytes, 1);
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* mods - x % [n] = x - r*floor(x/r)
   remainder after scalar division
*/
  u3_noun
  u3qi_la_mods_i754(u3_noun x_data,
                    u3_noun n,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    // we reuse it for results for parsimony
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);

    float16_t n16, in16;
    float32_t n32, in32;
    float64_t n64, in64;
    float128_t n128, in128;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        u3r_bytes(0, 2, (c3_y*)&(n16.v), n);
        in16 = f16_div((float16_t){SB_REAL16_ONE}, n16);

        for (c3_d i = 0; i < len_x; i++) {
          float16_t x_val16 = ((float16_t*)x_bytes)[i];
          // Perform division x/n
          float16_t div_result16 = f16_mul(in16, x_val16);
          // Compute floor of the division result
          c3_ds floor_result16 = f16_to_i64(div_result16, softfloat_round_minMag, false);
          float16_t floor_float16 = i64_to_f16(floor_result16);
          // Multiply n by floor(x/n)
          float16_t mult_result16 = f16_mul(n16, floor_float16);
          // Compute remainder: x - n * floor(x/n)
          ((float16_t*)x_bytes)[i] = f16_sub(x_val16, mult_result16);
        }
        break;

      case 5:
        u3r_bytes(0, 4, (c3_y*)&(n32.v), n);
        in32 = f32_div((float32_t){SB_REAL32_ONE}, n32);

        for (c3_d i = 0; i < len_x; i++) {
          float32_t x_val32 = ((float32_t*)x_bytes)[i];
          // Perform division x/n
          float32_t div_result32 = f32_mul(in32, x_val32);
          // Compute floor of the division result
          c3_ds floor_result32 = f32_to_i64(div_result32, softfloat_round_minMag, false);
          float32_t floor_float32 = i64_to_f32(floor_result32);
          // Multiply n by floor(x/n)
          float32_t mult_result32 = f32_mul(n32, floor_float32);
          // Compute remainder: x - n * floor(x/n)
          ((float32_t*)x_bytes)[i] = f32_sub(x_val32, mult_result32);
        }
        break;

      case 6:
        u3r_bytes(0, 8, (c3_y*)&(n64.v), n);
        in64 = f64_div((float64_t){SB_REAL64_ONE}, n64);

        for (c3_d i = 0; i < len_x; i++) {
          float64_t x_val64 = ((float64_t*)x_bytes)[i];
          // Perform division x/n
          float64_t div_result64 = f64_mul(in64, x_val64);
          // Compute floor of the division result
          c3_ds floor_result64 = f64_to_i64(div_result64, softfloat_round_minMag, false);
          float64_t floor_float64 = i64_to_f64(floor_result64);
          // Multiply n by floor(x/n)
          float64_t mult_result64 = f64_mul(n64, floor_float64);
          // Compute remainder: x - n * floor(x/n)
          ((float64_t*)x_bytes)[i] = f64_sub(x_val64, mult_result64);
        }
        break;

      case 7:
        u3r_bytes(0, 16, (c3_y*)&(n128.v[0]), n);
        f128M_div(&((float128_t){SB_REAL128L_ONE,SB_REAL128U_ZERO}), &n128, &in128);

        for (c3_d i = 0; i < len_x; i++) {
          float128_t x_val128 = ((float128_t*)x_bytes)[i];
          // Perform division x/n
          float128_t div_result128;
          f128M_mul((float128_t*)&in128, (float128_t*)&x_val128, (float128_t*)&div_result128);
          // Compute floor of the division result
          c3_ds floor_result128 = f128M_to_i64(&div_result128, softfloat_round_minMag, false);
          float128_t floor_float128;
          i64_to_f128M(floor_result128, &floor_float128);
          // Multiply n by floor(x/n)
          float128_t mult_result128;
          f128M_mul(((float128_t*)&n128), ((float128_t*)&floor_float128), ((float128_t*)&mult_result128));
          // Compute remainder: x - n * floor(x/n)
          f128M_sub(((float128_t*)&x_val128), ((float128_t*)&mult_result128), &(((float128_t*)x_bytes)[i]));
        }
        break;
    }

    // r_data is the result noun of [data]
    u3_noun r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), x_bytes);

    //  Clean up and return.
    u3a_free(x_bytes);

    return r_data;
  }

/* dot - ?dot = x · y
*/
  u3_noun
  u3qi_la_dot_i754(u3_noun x_data,
                   u3_noun y_data,
                   u3_noun shape,
                   u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(shape);

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // y_bytes is the data array (w/ leading 0x1, skipped by ?axpy)
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, y_bytes, y_data);

    u3_noun r_data;

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t r16[2];
        r16[0] = hdot(len_x, (float16_t*)x_bytes, 1, (float16_t*)y_bytes, 1);
        r16[1].v = 0x1;
        r_data = u3i_bytes((2+1)*sizeof(c3_y), (c3_y*)r16);
        break;}

      case 5: {
        float32_t r32[2];
        r32[0] = sdot(len_x, (float32_t*)x_bytes, 1, (float32_t*)y_bytes, 1);
        r32[1].v = 0x1;
        r_data = u3i_bytes((4+1)*sizeof(c3_y), (c3_y*)r32);
        break;}

      case 6: {
        float64_t r64[2];
        r64[0] = ddot(len_x, (float64_t*)x_bytes, 1, (float64_t*)y_bytes, 1);
        r64[1].v = 0x1;
        r_data = u3i_bytes((8+1)*sizeof(c3_y), (c3_y*)r64);
        break;}

      case 7: {
        float128_t r128[2];
        r128[0] = qdot(len_x, (float128_t*)x_bytes, 1, (float128_t*)y_bytes, 1);
        r128[1] = (float128_t){0x1, 0x0};
        r_data = u3i_bytes((16+1)*sizeof(c3_y), (c3_y*)r128);
        break;}
    }

    //  Clean up and return.
    u3a_free(x_bytes);
    u3a_free(y_bytes);

    return r_data;
  }

/* diag - diag(x)
*/
  u3_noun
  u3qi_la_diag(u3_noun x_data,
               u3_noun shape,
               u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }
    //  Assert length of dims is 2.
    if (u3qb_lent(shape) != 2) {
      return u3m_bail(c3__exit);
    }
    //  Unpack shape into an array of dimensions.
    c3_d *dims = _get_dims(shape);
    if (dims[0] != dims[1]) {
      return u3m_bail(c3__exit);
    }

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    c3_d len_x = _get_length(shape);
    c3_d syz_x = len_x * pow(2, bloq - 3);
    c3_d wyd = pow(2, bloq - 3);
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);
    c3_d syz_y = wyd * dims[1];
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_y+1)*sizeof(c3_y));

    u3_noun r_data;

    // Grab the index at i*n_x+j in bytes; put it at j.
    for (c3_d i = 0; i < dims[1]; i++) {
      // Scan across whole field width.
      for (c3_y k = 0; k < wyd; k++) {
        y_bytes[i*wyd+k] = x_bytes[(i*dims[0]+i)*wyd+k];
      }
    }
    y_bytes[syz_y] = 0x1;  // pin head

    //  Unpack the result back into a noun.
    r_data = u3i_bytes((syz_y+1)*sizeof(c3_y), y_bytes);
    
    u3a_free(x_bytes);
    u3a_free(y_bytes);
    u3a_free(dims);

    return r_data;
  }

/* transpose - x'
*/
  u3_noun
  u3qi_la_transpose(u3_noun x_data,
                    u3_noun shape,
                    u3_noun bloq)
  {
    //  Assert length of dims is 2.
    if (u3qb_lent(shape) != 2) {
      return u3m_bail(c3__exit);
    }
    //  Unpack shape into an array of dimensions.
    c3_d *dims = _get_dims(shape);

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    c3_d len_x = _get_length(shape);
    c3_d syz_x = len_x * pow(2, bloq - 3);
    c3_d wyd = pow(2, bloq - 3);
    c3_y* x_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));
    u3r_bytes(0, syz_x+1, x_bytes, x_data);
    c3_y* y_bytes = (c3_y*)u3a_malloc((syz_x+1)*sizeof(c3_y));

    u3_noun r_data;

    // Grab the index at i*n_x+j in bytes; put it at j.
    for (c3_d i = 0; i < dims[1]; i++) {
      for (c3_d j = 0; j < dims[0]; j++) {
        // Scan across whole field width.
        for (c3_y k = 0; k < wyd; k++) {
          y_bytes[(j*dims[1]+i)*wyd+k] = x_bytes[(i*dims[0]+j)*wyd+k];
        }
      }
    }
    y_bytes[syz_x] = 0x1;  // pin head

    //  Unpack the result back into a noun.
    r_data = u3i_bytes((syz_x+1)*sizeof(c3_y), y_bytes);

    u3a_free(x_bytes);
    u3a_free(y_bytes);
    u3a_free(dims);

    return r_data;
  }

/* linspace - [a a+(b-a)/n ... b]
*/
  u3_noun
  u3qi_la_linspace_i754(u3_noun a,
                        u3_noun b,
                        u3_noun n,
                        u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    u3_noun r_data;

    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t a16, b16;
        u3r_bytes(0, 2, (c3_y*)&(a16.v), a);
        u3r_bytes(0, 2, (c3_y*)&(b16.v), b);
        float16_t span16 = f16_sub(b16, a16);
        float16_t interval16 = f16_div(span16, i32_to_f16(n-1));
        c3_y* x_bytes16 = (c3_y*)u3a_malloc((n*2+1)*sizeof(c3_y));
        for (c3_d i = 1; i < n-1; i++) {
          ((float16_t*)x_bytes16)[i] = f16_add(a16, f16_mul(i32_to_f16(i), interval16));
        }
        //  Assign in reverse order so that n=1 case is correctly left-hand bound.
        ((float16_t*)x_bytes16)[n-1] = b16;
        ((float16_t*)x_bytes16)[0] = a16;
        x_bytes16[n*2] = 0x1;  // pin head
        r_data = u3i_bytes((n*2+1)*sizeof(c3_y), x_bytes16);
        u3a_free(x_bytes16);
        break;}
      
      case 5: {
        float32_t a32, b32;
        u3r_bytes(0, 4, (c3_y*)&(a32.v), a);
        u3r_bytes(0, 4, (c3_y*)&(b32.v), b);
        float32_t span32 = f32_sub(b32, a32);
        float32_t interval32 = f32_div(span32, i32_to_f32(n-1));
        c3_y* x_bytes32 = (c3_y*)u3a_malloc((n*4+1)*sizeof(c3_y));
        for (c3_d i = 1; i < n-1; i++) {
          ((float32_t*)x_bytes32)[i] = f32_add(a32, f32_mul(i32_to_f32(i), interval32));
        }
        ((float32_t*)x_bytes32)[n-1] = b32;
        ((float32_t*)x_bytes32)[0] = a32;
        x_bytes32[n*4] = 0x1;  // pin head
        r_data = u3i_bytes((n*4+1)*sizeof(c3_y), x_bytes32);
        u3a_free(x_bytes32);
        break;}

      case 6: {
        float64_t a64, b64;
        u3r_bytes(0, 8, (c3_y*)&(a64.v), a);
        u3r_bytes(0, 8, (c3_y*)&(b64.v), b);
        float64_t span64 = f64_sub(b64, a64);
        float64_t interval64 = f64_div(span64, i32_to_f64(n-1));
        c3_y* x_bytes64 = (c3_y*)u3a_malloc((n*8+1)*sizeof(c3_y));
        for (c3_d i = 1; i < n-1; i++) {
          ((float64_t*)x_bytes64)[i] = f64_add(a64, f64_mul(i32_to_f64(i), interval64));
        }
        ((float64_t*)x_bytes64)[n-1] = b64;
        ((float64_t*)x_bytes64)[0] = a64;
        x_bytes64[n*8] = 0x1;  // pin head
        r_data = u3i_bytes((n*8+1)*sizeof(c3_y), x_bytes64);
        u3a_free(x_bytes64);
        break;}
      
      case 7: {
        float128_t a128, b128;
        u3r_bytes(0, 16, (c3_y*)&(a128.v[0]), a);
        u3r_bytes(0, 16, (c3_y*)&(b128.v[0]), b);
        float128_t span128;
        f128M_sub(&b128, &a128, &span128);
        float128_t interval128;
        float128_t n128;
        i32_to_f128M(n-1, &n128);
        f128M_div(&span128, &n128, &interval128);
        c3_y* x_bytes128 = (c3_y*)u3a_malloc((n*16+1)*sizeof(c3_y));
        float128_t i128;
        for (c3_d i = 1; i < n-1; i++) {
          i32_to_f128M(i, &i128);
          f128M_mul(&i128, &interval128, &((float128_t*)x_bytes128)[i]);
          f128M_add(&a128, &((float128_t*)x_bytes128)[i], &((float128_t*)x_bytes128)[i]);
        }
        ((float128_t*)x_bytes128)[n-1] = b128;
        ((float128_t*)x_bytes128)[0] = a128;
        x_bytes128[n*16] = 0x1;  // pin head
        r_data = u3i_bytes((n*16+1)*sizeof(c3_y), x_bytes128);
        u3a_free(x_bytes128);
        break;}
    }

    return r_data;
  }

/* range - [a a+d ... b]
*/
  u3_noun
  u3qi_la_range_i754(u3_noun a,
                     u3_noun b,
                     u3_noun d,
                     u3_noun bloq)
  {
    //  Fence on valid bloq size.
    if (bloq < 4 || bloq > 7) {
      return u3_none;
    }

    u3_noun r_data;

    switch (u3x_atom(bloq)) {
      case 4: {
        float16_t a16, b16, interval16;
        u3r_bytes(0, 2, (c3_y*)&(a16.v), a);
        u3r_bytes(0, 2, (c3_y*)&(b16.v), b);
        u3r_bytes(0, 2, (c3_y*)&(interval16.v), d);
        c3_d n16 = f16_to_i64(f16_ceil(f16_div(f16_sub(b16, a16), interval16)), softfloat_round_minMag, false);
        c3_y* x_bytes16 = (c3_y*)u3a_malloc(((n16+1)*2)*sizeof(c3_y));
        ((float16_t*)x_bytes16)[0] = a16;
        for (c3_d i = 1; i < n16; i++) {
          ((float16_t*)x_bytes16)[i] = f16_add(a16, f16_mul(i32_to_f16(i), interval16));
        }
        ((float16_t*)x_bytes16)[n16].v = 0x1;  // pin head
        r_data = u3i_bytes(((n16+1)*2)*sizeof(c3_y), x_bytes16);
        u3a_free(x_bytes16);
        break;}
      
      case 5: {
        float32_t a32, b32, interval32;
        u3r_bytes(0, 4, (c3_y*)&(a32.v), a);
        u3r_bytes(0, 4, (c3_y*)&(b32.v), b);
        u3r_bytes(0, 4, (c3_y*)&(interval32.v), d);
        c3_d n32 = f32_to_i64(f32_ceil(f32_div(f32_sub(b32, a32), interval32)), softfloat_round_minMag, false);
        c3_y* x_bytes32 = (c3_y*)u3a_malloc(((n32+1)*4)*sizeof(c3_y));
        ((float32_t*)x_bytes32)[0] = a32;
        for (c3_d i = 1; i < n32; i++) {
          ((float32_t*)x_bytes32)[i] = f32_add(a32, f32_mul(i32_to_f32(i), interval32));
        }
        ((float32_t*)x_bytes32)[n32].v = 0x1;  // pin head
        r_data = u3i_bytes(((n32+1)*4)*sizeof(c3_y), x_bytes32);
        u3a_free(x_bytes32);
        break;}

      case 6: {
        float64_t a64, b64, interval64;
        u3r_bytes(0, 8, (c3_y*)&(a64.v), a);
        u3r_bytes(0, 8, (c3_y*)&(b64.v), b);
        u3r_bytes(0, 8, (c3_y*)&(interval64.v), d);
        c3_d n64 = f64_to_i64(f64_ceil(f64_div(f64_sub(b64, a64), interval64)), softfloat_round_minMag, false);
        c3_y* x_bytes64 = (c3_y*)u3a_malloc(((n64+1)*8)*sizeof(c3_y));
        ((float64_t*)x_bytes64)[0] = a64;
        for (c3_d i = 1; i < n64; i++) {
          ((float64_t*)x_bytes64)[i] = f64_add(a64, f64_mul(i32_to_f64(i), interval64));
        }
        ((float64_t*)x_bytes64)[n64].v = 0x1;  // pin head
        r_data = u3i_bytes(((n64+1)*8)*sizeof(c3_y), x_bytes64);
        u3a_free(x_bytes64);
        break;}
      
      case 7: {
        float128_t a128, b128, interval128;
        u3r_bytes(0, 16, (c3_y*)&(a128.v[0]), a);
        u3r_bytes(0, 16, (c3_y*)&(b128.v[0]), b);
        u3r_bytes(0, 16, (c3_y*)&(interval128.v[0]), d);
        float128_t tmp;
        f128M_sub(&b128, &a128, &tmp);
        f128M_div(&tmp, &interval128, &tmp);
        f128M_ceil(&tmp, &tmp);
        c3_d n128 = f128M_to_i64(&tmp, softfloat_round_minMag, false);
        c3_y* x_bytes128 = (c3_y*)u3a_malloc(((n128+1)*16)*sizeof(c3_y));
        float128_t i128;
        ((float128_t*)x_bytes128)[0] = a128;
        for (c3_d i = 1; i < n128; i++) {
          i32_to_f128M(i, &i128);
          f128M_mul(&i128, &interval128, &((float128_t*)x_bytes128)[i]);
          f128M_add(&a128, &((float128_t*)x_bytes128)[i], &((float128_t*)x_bytes128)[i]);
        }
        ((float128_t*)x_bytes128)[n128].v[0] = 0x1;  // pin head
        ((float128_t*)x_bytes128)[n128].v[1] = 0x0;  // pin head
        r_data = u3i_bytes(((n128+1)*16)*sizeof(c3_y), x_bytes128);
        u3a_free(x_bytes128);
        break;}
    }

    return r_data;
  }

/* trace - tr(x)
*/
  u3_noun
  u3qi_la_trace_i754(u3_noun x_data,
                     u3_noun shape,
                     u3_noun bloq)
  {
    u3_noun d_data = u3qi_la_diag(x_data, shape, bloq);
    c3_d len_x0 = _get_dims(shape)[0];
    u3_noun r_data = u3qi_la_dot_i754(d_data, d_data, u3nt(len_x0, 0x1, u3_nul), u3k(bloq));
    return r_data;
  }

/* mmul
*/
  u3_noun
  u3qi_la_mmul_i754(u3_noun x_data,
                    u3_noun y_data,
                    u3_noun x_shape,
                    u3_noun y_shape,
                    u3_noun bloq)
  {
    //  Unpack the data as a byte array.  We assume total length < 2**64.
    c3_d M = u3x_atom(u3h(x_shape));
    c3_d Na= u3x_atom(u3h(u3t(x_shape)));
    c3_d Nb= u3x_atom(u3h(y_shape));
    c3_d P = u3x_atom(u3h(u3t(y_shape)));

    if ((u3_nul != u3t(u3t(x_shape))) ||
        (u3_nul != u3t(u3t(y_shape))) ||
        (Na != Nb)) {
      return u3m_bail(c3__exit);
    }
    c3_d N = Na;

    //  Unpack the data as a byte array.  We assume total length < 2**64.
    // len_x is length in base units
    c3_d len_x = _get_length(x_shape);    // M*N

    // syz_x is length in bytes
    c3_d syz_x = len_x * pow(2, bloq-3);  // M*N

    // x_bytes is the data array (w/o leading 0x1)
    c3_y* x_bytes = (c3_y*)u3a_malloc(syz_x*sizeof(c3_y));
    u3r_bytes(0, syz_x, x_bytes, x_data);

    // len_x is length in base units
    c3_d len_y = _get_length(y_shape);    // N*P

    // syz_x is length in bytes
    c3_d syz_y = len_y * pow(2, bloq-3);  // N*P

    // y_bytes is the data array (w/o leading 0x1)
    c3_y* y_bytes = (c3_y*)u3a_malloc(syz_y*sizeof(c3_y));
    u3r_bytes(0, syz_y, y_bytes, y_data);
    
    // len_r is length in base units
    c3_d len_r = M*P;                     // M*P

    // syz_r is length in bytes
    c3_d syz_r = len_r * pow(2, bloq-3);  // M*P

    // r_bytes is the result array
    c3_y* r_bytes = (c3_y*)u3a_malloc((syz_r+1)*sizeof(c3_y));
    r_bytes[syz_r] = 0x1;  // pin head
    // initialize with 0x0s
    for (c3_d i = 0; i < syz_r; i++) {
      r_bytes[i] = 0x0;
    }

    //  Switch on the block size.
    switch (u3x_atom(bloq)) {
      case 4:
        hgemm('N', 'N', M, N, P, (float16_t){SB_REAL16_ONE}, (float16_t*)x_bytes, N, (float16_t*)y_bytes, P, (float16_t){SB_REAL16_ZERO}, (float16_t*)r_bytes, P);
        break;

      case 5:
        sgemm('N', 'N', M, N, P, (float32_t){SB_REAL32_ONE}, (float32_t*)x_bytes, N, (float32_t*)y_bytes, P, (float32_t){SB_REAL32_ZERO}, (float32_t*)r_bytes, P);
        break;

      case 6:
        dgemm('N', 'N', M, N, P, (float64_t){SB_REAL64_ONE}, (float64_t*)x_bytes, N, (float64_t*)y_bytes, P, (float64_t){SB_REAL64_ZERO}, (float64_t*)r_bytes, P);
        break;

      case 7:
        qgemm('N', 'N', M, N, P, (float128_t){SB_REAL128L_ONE,SB_REAL128U_ONE}, (float128_t*)x_bytes, N, (float128_t*)y_bytes, P, (float128_t){SB_REAL128L_ZERO,SB_REAL128U_ZERO}, (float128_t*)r_bytes, P);
        break;
    }

    //  Unpack the result back into a noun.
    u3_noun r_data = u3i_bytes(syz_r+1, r_bytes);
    u3_noun M_ = u3i_chub(M);
    u3_noun P_ = u3i_chub(P);

    u3a_free(x_bytes);
    u3a_free(y_bytes);
    u3a_free(r_bytes);

    return u3nc(u3nq(u3nt(M_, P_, u3_nul), u3k(bloq), c3__i754, u3_nul), r_data);
  }

  u3_noun
  u3wi_la_add(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_add_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_sub(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_sub_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_mul(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_mul_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_div(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_div_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_mod(u3_noun cor)
  {
      // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(rnd)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_mod_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_cumsum(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_cumsum_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nc(0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_argmin(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_argmin_i754(x_data, x_shape, x_bloq);
            // bare atom (@ index)
            return r_data;}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_ravel(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_ravel_i754(x_data, x_shape, x_bloq);
            // (list @)
            return r_data;}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_argmax(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_argmax_i754(x_data, x_shape, x_bloq);
            // bare atom (@ index)
            return r_data;}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_min(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_min_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nt(0x1, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_max(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(u3nc(x_meta, x_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_max_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nt(0x1, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_abs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_abs_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_gth(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_gth_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_gte(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_gte_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_lth(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_lth_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_lte(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_lte_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3k(x_meta), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_adds(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_adds_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_subs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_subs_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_muls(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
            x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_muls_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_divs(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
            x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_divs_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_mods(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data, n;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_3, &n,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(n) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
            x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      switch (x_kind) {
        case c3__i754:
          _set_rounding(rnd);
          u3_noun r_data = u3qi_la_mods_i754(x_data, n, x_shape, x_bloq);
          if (r_data == u3_none) { return u3_none; }
          return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

        default:
          return u3_none;
      }
    }
  }

  u3_noun
  u3wi_la_dot(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3r_sing(x_meta, y_meta) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_dot_i754(x_data, y_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            c3_d len_x0 = _get_dims(x_shape)[0];
            return u3nc(u3nq(u3nt(len_x0, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_transpose(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(cor)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        u3_noun r_data = u3qi_la_transpose(x_data, x_shape, x_bloq);
        if (r_data == u3_none) { return u3_none; }
        return u3nc(u3nq(u3nt(u3k(u3h(x_shape)), u3k(u3h(u3t(x_shape))), u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);
      }
    }
  }

  u3_noun
  u3wi_la_linspace(u3_noun cor)
  {
    u3_noun x_meta, a, b, n, rnd;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_12, &a,
                         u3x_sam_13, &b,
                         u3x_sam_7, &n,
                         0))
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == u3ud(n) ||
           (n < 1)                    // crash on zero size
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_linspace_i754(a, b, n, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            x_shape = u3nc(u3x_atom(n), u3_nul);
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_range(u3_noun cor)
  {
    u3_noun x_meta, a, b, d, rnd;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_12, &a,
                         u3x_sam_13, &b,
                         u3x_sam_7, &d,
                         0))
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_range_i754(a, b, d, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            c3_d a_, b_, d_;
            c3_ds n_;
            switch (x_bloq) {
              case 4:
                u3r_bytes(0, 2, (c3_y*)&a_, a);
                u3r_bytes(0, 2, (c3_y*)&b_, b);
                u3r_bytes(0, 2, (c3_y*)&d_, d);
                n_ = f16_to_i64(f16_ceil(f16_div(f16_sub((float16_t){b_}, (float16_t){a_}), (float16_t){d_})), softfloat_round_minMag, false) - 1;
                break;
              case 5:
                u3r_bytes(0, 4, (c3_y*)&a_, a);
                u3r_bytes(0, 4, (c3_y*)&b_, b);
                u3r_bytes(0, 4, (c3_y*)&d_, d);
                n_ = f32_to_i64(f32_ceil(f32_div(f32_sub((float32_t){b_}, (float32_t){a_}), (float32_t){d_})), softfloat_round_minMag, false) - 1;
                break;
              case 6:
                u3r_bytes(0, 8, (c3_y*)&a_, a);
                u3r_bytes(0, 8, (c3_y*)&b_, b);
                u3r_bytes(0, 8, (c3_y*)&d_, d);
                n_ = f64_to_i64(f64_ceil(f64_div(f64_sub((float64_t){b_}, (float64_t){a_}), (float64_t){d_})), softfloat_round_minMag, false) - 1;
                break;
              case 7: {
                c3_d a__[2], b__[2], d__[2];
                u3r_bytes(0, 16, (c3_y*)&a__, a);
                u3r_bytes(0, 16, (c3_y*)&b__, b);
                u3r_bytes(0, 16, (c3_y*)&d__, d);
                float128_t tmp;
                f128M_sub((float128_t*)&b__, (float128_t*)&a__, &tmp);
                f128M_div(&tmp, (float128_t*)&d__, &tmp);
                f128M_ceil(&tmp, &tmp);
                n_ = f128M_to_i64(&tmp, softfloat_round_minMag, false) - 1;
                break;}
            }
            u3_noun n = u3i_chub(n_+1);
            x_shape = u3nc(u3k(n), u3_nul);
            return u3nc(u3nq(u3k(x_shape), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_diag(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      x_tail = u3t(u3t(u3t(x_meta))); // 15
      if ( c3n == u3ud(x_bloq) ||
           c3n == u3ud(x_kind) ||
           c3n == _check(cor)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        u3_noun r_data = u3qi_la_diag(x_data, x_shape, x_bloq);
        if (r_data == u3_none) { return u3_none; }
        c3_d len_x0 = _get_dims(x_shape)[0];
        return u3nc(u3nq(u3nt(len_x0, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);
      }
    }
  }

  u3_noun
  u3wi_la_trace(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_2, &x_meta,
                         u3x_sam_3, &x_data,
                         0) ||
         c3n == u3ud(x_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind, x_tail;
      if ( c3n == u3r_mean(x_meta,
                            2, &x_shape,
                            6, &x_bloq,
                           14, &x_kind,
                           15, &x_tail,
                            0)
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754: {
            u3_noun r_data = u3qi_la_trace_i754(x_data, x_shape, x_bloq);
            if (r_data == u3_none) { return u3_none; }
            return u3nc(u3nq(u3nt(0x1, 0x1, u3_nul), u3k(x_bloq), u3k(x_kind), u3k(x_tail)), r_data);}

          default:
            return u3_none;
        }
      }
    }
  }

  u3_noun
  u3wi_la_mmul(u3_noun cor)
  {
    // Each argument is a ray, [=meta data=@ux]
    u3_noun x_meta, x_data,
            y_meta, y_data;

    if ( c3n == u3r_mean(cor,
                         u3x_sam_4, &x_meta,
                         u3x_sam_5, &x_data,
                         u3x_sam_6, &y_meta,
                         u3x_sam_7, &y_data,
                         0) ||
         c3n == u3ud(x_data) ||
         c3n == u3ud(y_data) )
    {
      return u3m_bail(c3__exit);
    } else {
      u3_noun x_shape, x_bloq, x_kind,
              y_shape,
              rnd;
      x_shape = u3h(x_meta);          //  2
      x_bloq = u3h(u3t(x_meta));      //  6
      x_kind = u3h(u3t(u3t(x_meta))); // 14
      y_shape = u3h(y_meta);          //  2
      rnd = u3h(u3t(u3t(u3t(cor))));  // 30
      if ( c3n == _check(u3nc(x_meta, x_data)) ||
           c3n == _check(u3nc(y_meta, y_data))
         )
      {
        return u3m_bail(c3__exit);
      } else {
        switch (x_kind) {
          case c3__i754:
            _set_rounding(rnd);
            u3_noun r_data = u3qi_la_mmul_i754(x_data, y_data, x_shape, y_shape, x_bloq);
            // result is already [meta data]
            return r_data;

          default:
            return u3_none;
        }
      }
    }
  }
