/// @file
///
/// Cribs (u3a_crib): regions of address space off the loom, reserved up
/// to a ceiling and committed as they're used, for the hash-consing and
/// unification state that lives for one excursion from the home road
/// (u3m_sole_try()'s visited set, the home lose list). Like a crib
/// sheet, they hold answers already worked out, and are thrown away
/// after.
///
/// Growing within the reservation allocates nothing and, on POSIX, makes
/// no system call (the whole reservation is mapped from the start, and
/// pages materialize on first touch), so it's safe wherever an
/// interrupt may unwind, and with async signals held.

#ifndef U3_CRIB_H
#define U3_CRIB_H

#include "c3/c3.h"

  /**  Data structures.
  **/
    /* u3a_crib: a reserved region.
    */
      typedef struct {
        c3_y*  buf_y;   //  start
        size_t len_i;   //  bytes committed
        size_t max_i;   //  bytes reserved
      } u3a_crib;

  /**  Functions.
  **/
    /* u3a_crib_init(): reserve [max_i] bytes, committing the first
    **                  [len_i]. releases [cib_u]'s old region, if any.
    **                  home road only.
    */
      void
      u3a_crib_init(u3a_crib* cib_u, size_t len_i, size_t max_i);

    /* u3a_crib_grow(): commit at least the first [len_i] bytes; 0 if
    **                  that's past the reservation, or the commit fails.
    **                  memory first committed reads as zero.
    */
      c3_t
      u3a_crib_grow(u3a_crib* cib_u, size_t len_i);

    /* u3a_crib_zero(): zero [off_i, +len_i), which stays committed;
    **                  large ranges give their whole pages back to the
    **                  system.
    */
      void
      u3a_crib_zero(u3a_crib* cib_u, size_t off_i, size_t len_i);

    /* u3a_crib_shrink(): give back everything past the first [len_i]
    **                    bytes; it reads as zero when grown into again.
    */
      void
      u3a_crib_shrink(u3a_crib* cib_u, size_t len_i);

#endif /* ifndef U3_CRIB_H */
