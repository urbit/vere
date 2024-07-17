/// @file

#ifndef C3_DEFS_H
#define C3_DEFS_H

#include "portable.h"
#include "types.h"

#include <errno.h>

  /** Loobeans - inverse booleans to match nock.
  **/
#     define c3y      0
#     define c3n      1

#     define _(x)        (c3y == (x))
#     define __(x)       ((x) ? c3y : c3n)
#     define c3a(x, y)   __(_(x) && _(y))
#     define c3o(x, y)   __(_(x) || _(y))


  /** Random useful C macros.
  **/
    /* Dessert. Debug assert. If a debugger is attached, it will break in and
       execution can be allowed to proceed without aborting the process.
       Otherwise, the unhandled SIGTRAP will dump core.
    */
#ifdef C3DBG
  #if defined(__i386__) || defined(__x86_64__)
    #define c3_dessert(x) do { if(!(x)) __asm__ volatile("int $3"); } while (0)
  #elif defined(__thumb__)
    #define c3_dessert(x) do { if(!(x)) __asm__ volatile(".inst 0xde01"); } while (0)
  #elif defined(__aarch64__)
    #define c3_dessert(x) do { if(!(x)) __asm__ volatile(".inst 0xd4200000"); } while (0)
  #elif defined(__arm__)
    #define c3_dessert(x) do { if(!(x)) __asm__ volatile(".inst 0xe7f001f0"); } while (0)
  #else
    STATIC_ASSERT(0, "debugger break instruction unimplemented");
  #endif
#else
  #define c3_dessert(x) ((void)(0))
#endif

    /* Stub.
    */
#     define c3_stub       u3_assert(!"stub")

    /* Size in words.
    */
#     define c3_wiseof(x)  (((sizeof (x)) + 3) >> 2)

    /* Bit counting.
    */
#     define c3_bits_word(w) ((w) ? (32 - __builtin_clz(w)) : 0)

    /* Min and max.
    */
#     define c3_max(x, y) ( ((x) > (y)) ? (x) : (y) )
#     define c3_min(x, y) ( ((x) < (y)) ? (x) : (y) )


//! Round up/down (respectively).
//!
//! @param[in] x  Integer to round.
//! @param[in] n  Multiple to round to. Must be power of 2.
//!
//! @return  `x` rounded to the nearest multiple of `n`.
#     define c3_rop(x, n) (((x) + ((n) - 1)) & (~((n) - 1)))
#     define c3_rod(x, n) ((x) & ~((n) - 1))

    /* Rotate.
    */
#     define c3_rotw(r, x)  ( ((x) << (r)) | ((x) >> (32 - (r))) )

    /* Fill 16 words (64 bytes) with high-quality entropy.
    */
      void
      c3_rand(c3_w* rad_w);

    /* Short integers.
    */
#     define c3_s1(a)          ( (a) )
#     define c3_s2(a, b)       ( ((b) << 8) | c3_s1(a) )
#     define c3_s3(a, b, c)    ( ((c) << 16) | c3_s2(a, b) )
#     define c3_s4(a, b, c, d) ( ((d) << 24) | c3_s3(a, b, c) )

#     define c3_s5(a, b, c, d, e) \
        ( ((uint64_t)c3_s1(e) << 32ULL) | c3_s4(a, b, c, d) )
#     define c3_s6(a, b, c, d, e, f) \
        ( ((uint64_t)c3_s2(e, f) << 32ULL) | c3_s4(a, b, c, d) )
#     define c3_s7(a, b, c, d, e, f, g) \
        ( ((uint64_t)c3_s3(e, f, g) << 32ULL) | c3_s4(a, b, c, d) )
#     define c3_s8(a, b, c, d, e, f, g, h) \
        ( ((uint64_t)c3_s4(e, f, g, h) << 32ULL) | c3_s4(a, b, c, d) )

    /* Byte-order twiddling.
    */
#     define c3_flip32(w) \
        ( (((w) >> 24) & 0xff) \
        | (((w) >> 16) & 0xff) << 8 \
        | (((w) >>  8) & 0xff) << 16 \
        | ( (w)        & 0xff) << 24 )

      inline c3_s
      c3_sift_short(c3_y buf_y[2])
      {
        return (buf_y[1] << 8 | buf_y[0]);
      }

      inline c3_w
      c3_sift_word(c3_y buf_y[4])
      {
        return (buf_y[3] << 24 | buf_y[2] << 16 | buf_y[1] << 8 | buf_y[0]);
      }

      inline c3_d
      c3_sift_chub(c3_y byt_y[8])
      {
        return (c3_d)byt_y[0]
             | (c3_d)byt_y[1] << 8
             | (c3_d)byt_y[2] << 16
             | (c3_d)byt_y[3] << 24
             | (c3_d)byt_y[4] << 32
             | (c3_d)byt_y[5] << 40
             | (c3_d)byt_y[6] << 48
             | (c3_d)byt_y[7] << 56;
      }

      inline void
      c3_etch_short(c3_y buf_y[2], c3_s sot_s)
      {
        buf_y[0] = sot_s         & 0xff;
        buf_y[1] = (sot_s >>  8) & 0xff;
      }

      inline void
      c3_etch_word(c3_y buf_y[4], c3_w wod_w)
      {
        buf_y[0] = wod_w         & 0xff;
        buf_y[1] = (wod_w >>  8) & 0xff;
        buf_y[2] = (wod_w >> 16) & 0xff;
        buf_y[3] = (wod_w >> 24) & 0xff;
      }

      inline void
      c3_etch_chub(c3_y byt_y[8], c3_d num_d)
      {
        byt_y[0] = num_d & 0xff;
        byt_y[1] = (num_d >>  8) & 0xff;
        byt_y[2] = (num_d >> 16) & 0xff;
        byt_y[3] = (num_d >> 24) & 0xff;
        byt_y[4] = (num_d >> 32) & 0xff;
        byt_y[5] = (num_d >> 40) & 0xff;
        byt_y[6] = (num_d >> 48) & 0xff;
        byt_y[7] = (num_d >> 56) & 0xff;
      }

    /* Asserting allocators.
    */
#     define c3_free(s) free(s)
#     define c3_malloc(s) ({                                    \
        void* rut = malloc(s);                                  \
        if ( 0 == rut ) {                                       \
          fprintf(stderr, "c3_malloc(%" PRIu64 ") failed\r\n",  \
                          (c3_d)s);                             \
          u3_assert(!"memory lost");                            \
        }                                                       \
        rut;})
#     define c3_calloc(s) ({                                    \
        void* rut = calloc(1,s);                                \
        if ( 0 == rut ) {                                       \
          fprintf(stderr, "c3_calloc(%" PRIu64 ") failed\r\n",  \
                          (c3_d)s);                             \
          u3_assert(!"memory lost");                            \
        }                                                       \
        rut;})
#     define c3_realloc(a, b) ({                                \
        void* rut = realloc(a, b);                              \
        if ( 0 == rut ) {                                       \
          fprintf(stderr, "c3_realloc(%" PRIu64 ") failed\r\n", \
                          (c3_d)b);                             \
          u3_assert(!"memory lost");                            \
        }                                                       \
        rut;})

    /* Asserting unix fs wrappers.
    **
    **  these all crash the process if passed a non-canonical
    **  path (i.e., one containing '.', '..', or the empty path
    **  component), so make sure you don't pass them one. if you
    **  find yourself fighting with them, then please delete them
    **  and do a sed search-and-replace to remove the `c3_` from
    **  their call sites; their goal is to decrease maintenance
    **  burden, not increase it.
    */
      // defined in vere/io/unix.c.
      c3_t u3_unix_cane(const c3_c* pax_c);
#     define c3_open(a, ...) ({                                 \
        open(a, __VA_ARGS__);})
#     define c3_opendir(a) ({                                   \
        opendir(a);})
#     define c3_mkdir(a, b) ({                                  \
        mkdir(a, b);})
#     define c3_rmdir(a) ({                                     \
        rmdir(a);})
#     define c3_link(a, b) ({                                   \
        link(a, b);})
#     define c3_unlink(a) ({                                    \
        unlink(a);})
#     define c3_fopen(a, b) ({                                  \
        fopen(a, b);})
#     define c3_remove(a) ({                                    \
        remove(a);})
#     define c3_rename(a, b) ({                                 \
        rename(a, b);})

/* c3_align(
       x    - the address/quantity to align,
       al   - the alignment,
       hilo - [C3_ALGHI, C3_ALGLO] high or low align
   )

   hi or lo align x to al

   unless effective type of x is c3_w or c3_d, assumes x is a pointer.
*/
#define c3_align(x, al, hilo)                   \
  _Generic((x),                                 \
           c3_w     : c3_align_w,               \
           c3_d     : c3_align_d,               \
           default  : c3_align_p)               \
       (x, al, hilo)
typedef enum { C3_ALGHI=1, C3_ALGLO=0 } align_dir;
inline c3_w
c3_align_w(c3_w x, c3_w al, align_dir hilo) {
  c3_dessert(hilo <= C3_ALGHI && hilo >= C3_ALGLO);
  x += hilo * (al - 1);
  x &= ~(al - 1);
  return x;
}
inline c3_d
c3_align_d(c3_d x, c3_d al, align_dir hilo) {
  c3_dessert(hilo <= C3_ALGHI && hilo >= C3_ALGLO);
  x += hilo * (al - 1);
  x &= ~(al - 1);
  return x;
}
inline void*
c3_align_p(void const * p, size_t al, align_dir hilo) {
  uintptr_t x = (uintptr_t)p;
  c3_dessert(hilo <= C3_ALGHI && hilo >= C3_ALGLO);
  x += hilo * (al - 1);
  x &= ~(al - 1);
  return (void*)x;
}

#endif /* ifndef C3_DEFS_H */
