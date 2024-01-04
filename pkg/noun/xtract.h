/// @file

#ifndef U3_XTRACT_H
#define U3_XTRACT_H

#include "c3.h"
#include "types.h"
#include "allocate.h"
#include "manage.h"

  /**  Constants.
  **/
    /* Conventional axes for gate call.
    */
#     define u3x_pay         3       //  payload
#     define u3x_sam         6       //  sample
#       define u3x_sam_1     6
#       define u3x_sam_2     12
#       define u3x_sam_3     13
#       define u3x_sam_4     24
#       define u3x_sam_5     25
#       define u3x_sam_6     26
#       define u3x_sam_12    52
#       define u3x_sam_13    53
#       define u3x_sam_7     27
#       define u3x_sam_14    54
#       define u3x_sam_15    55
#       define u3x_sam_30    110
#       define u3x_sam_31    111
#       define u3x_sam_62    222
#       define u3x_sam_63    223
#     define u3x_con         7       //  context
#     define u3x_con_2       14      //  context
#     define u3x_con_3       15      //  context
#     define u3x_con_sam     30      //  sample in gate context
#       define u3x_con_sam_2 60
#       define u3x_con_sam_3 61
#     define u3x_bat         2       //  battery


  /**  Macros.
  **/
    /* Word axis macros.  For 31-bit axes only.
    */

      /* u3x_at (u3at): fragment.
      */
#       define u3x_at(a, b)   u3x_good(u3r_at(a, b))
#       define u3at(a, b)     u3x_at(a, b)

      /* u3x_bite(): xtract/default $bloq and $step from $bite.
      */
#       define u3x_bite(a, b, c)                      \
          do {                                        \
            if ( c3n == u3r_bite(a, b, c) ) {         \
              u3m_bail(c3__exit);                     \
            }                                         \
          } while (0)

      /* u3x_dep(): number of axis bits.
      */
#       define u3x_dep(a_w)   (c3_bits_word(a_w) - 1)

      /* u3x_cap(): root axis, 2 or 3.
      */
#       define u3x_cap(a_w) ({                        \
          u3_assert( 1 < a_w );                       \
          (0x2 | (a_w >> (u3x_dep(a_w) - 1))); })

      /* u3x_mas(): remainder after cap.
      */
#       define u3x_mas(a_w) ({                        \
          u3_assert( 1 < a_w );                       \
          ( (a_w & ~(1 << u3x_dep(a_w))) | (1 << (u3x_dep(a_w) - 1)) ); })

      /* u3x_peg(): connect two axes.
      */
#       define u3x_peg(a_w, b_w) \
          ( (a_w << u3x_dep(b_w)) | (b_w &~ (1 << u3x_dep(b_w))) )

      /* u3x_cell(): divide `a` as a cell `[b c]`.
      */
#       define u3x_cell(a, b, c)                      \
          do {                                        \
            if ( c3n == u3r_cell(a, b, c) ) {         \
              u3m_bail(c3__exit);                     \
            }                                         \
          } while (0)

      /* u3x_trel(): divide `a` as a trel `[b c d]`, or bail.
      */
#       define u3x_trel(a, b, c, d)                   \
          do {                                        \
            if ( c3n == u3r_trel(a, b, c, d) ) {      \
              u3m_bail(c3__exit);                     \
            }                                         \
          } while (0)

      /* u3x_qual(): divide `a` as a quadruple `[b c d e]`.
      */
#       define u3x_qual(a, b, c, d, e)                \
          do {                                        \
            if ( c3n == u3r_qual(a, b, c, d, e) ) {   \
              u3m_bail(c3__exit);                     \
            }                                         \
          } while (0)

      /* u3x_quil(): divide `a` as a quintuple `[b c d e f]`.
      */
#       define u3x_quil(a, b, c, d, e, f)                  \
          do {                                             \
            if ( c3n == u3r_quil(a, b, c, d, e, f) ) {     \
              u3m_bail(c3__exit);                          \
            }                                              \
          } while (0)

      /* u3x_hext(): divide `a` as a hextuple `[b c d e f g]`.
      */
#       define u3x_hext(a, b, c, d, e, f, g)               \
          do {                                             \
            if ( c3n == u3r_hext(a, b, c, d, e, f, g) ) {  \
              u3m_bail(c3__exit);                          \
            }                                              \
          } while (0)

  /**  Functions.
  **/
    /** u3x_*: read, but bail with c3__exit on a crash.
    **/
      /* u3x_atom(): atom or exit.
      */
        inline u3_atom
        u3x_atom(u3_noun a)
        {
          return ( c3y == u3a_is_cell(a) ) ? u3m_bail(c3__exit) : a;
        }

      /* u3x_good(): test for u3_none.
      */
        inline u3_noun
        u3x_good(u3_weak som)
        {
          return ( u3_none == som ) ? u3m_bail(c3__exit) : som;
        }

      /* u3x_mean():
      **
      **   Attempt to deconstruct `a` by axis, noun pairs; 0 terminates.
      **   Axes must be sorted in tree order.
      */
        void
        u3x_mean(u3_noun a, ...);

#endif /* ifndef U3_XTRACT_H */
