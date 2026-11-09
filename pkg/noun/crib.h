/// @file
///
/// Cribs (u3c): regions of address space off the loom, reserved up to a
/// ceiling and committed as they're used, for the hash-consing and
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
    /* u3c_crib: a reserved region.
    */
      typedef struct {
        c3_y*  buf_y;   //  start
        size_t len_i;   //  bytes committed
        size_t max_i;   //  bytes reserved
      } u3c_crib;

    /* u3c_stat: an off-loom table's use since its last report, for the
    **           measurement log's N lines.
    */
      typedef struct {
        const c3_c* nam_c;   //  name
        c3_d   cap_d;   //  capacity now, entries
        c3_d   top_d;   //  largest capacity
        c3_d   max_d;   //  ceiling
        c3_d   pek_d;   //  most entries in use at once
        c3_d   gro_d;   //  grows
        c3_d   shr_d;   //  shrinks
        c3_d   clg_d;   //  clears through a log of slots (sets)
        c3_d   cdp_d;   //  clears by zeroing the table (sets)
        c3_d   ovf_d;   //  evictions or drops at the ceiling
      } u3c_stat;

  /**  Functions.
  **/
    /* u3c_init(): reserve [max_i] bytes, committing the first [len_i].
    **             releases [cib_u]'s old region, if any. home road only.
    */
      void
      u3c_init(u3c_crib* cib_u, size_t len_i, size_t max_i);

    /* u3c_grow(): commit at least the first [len_i] bytes; 0 if that's
    **             past the reservation, or the commit fails. memory first
    **             committed reads as zero.
    */
      c3_t
      u3c_grow(u3c_crib* cib_u, size_t len_i);

    /* u3c_zero(): zero [off_i, +len_i), which stays committed; large
    **             ranges give their whole pages back to the system.
    */
      void
      u3c_zero(u3c_crib* cib_u, size_t off_i, size_t len_i);

    /* u3c_shrink(): give back everything past the first [len_i] bytes;
    **               it reads as zero when grown into again.
    */
      void
      u3c_shrink(u3c_crib* cib_u, size_t len_i);

    /* u3c_fence(): order stores against an interrupt that unwinds this
    **              thread.
    */
      static inline void
      u3c_fence(void)
      {
        __atomic_signal_fence(__ATOMIC_SEQ_CST);
      }

#endif /* ifndef U3_CRIB_H */
