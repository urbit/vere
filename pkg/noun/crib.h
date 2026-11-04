/// @file
///
/// Cribs (u3c): growable off-loom open-addressing sets and arrays for
/// the hash-consing and unification state that lives for one excursion
/// from the home road. Like a crib sheet, they hold answers already
/// worked out (cells already canonicalized, values to lose later), and
/// are thrown away after.

#ifndef U3_CRIB_H
#define U3_CRIB_H

#include "c3/c3.h"

  /**  Data structures.
  **/
    /* u3c_stat: a crib's use since the last u3c_*_stat(), for measurement.
    */
      typedef struct {
        const c3_c* nam_c;   //  name
        c3_d   cap_d;   //  capacity now, entries
        c3_d   top_d;   //  largest capacity
        c3_d   max_d;   //  ceiling
        c3_d   pek_d;   //  most entries in use at once
        c3_d   gro_d;   //  grows
        c3_d   shr_d;   //  shrinks
        c3_d   clg_d;   //  clears through the log (sets)
        c3_d   cdp_d;   //  clears by dropping pages (sets)
        c3_d   ovf_d;   //  evictions or drops at the ceiling
      } u3c_stat;

    /* u3c_set: a set of keys, each > 1 (0 marks an empty slot, 1 a
    **          removed one).
    **
    **   a region is reserved for the most entries the set may grow to,
    **   and committed as it grows, so growing allocates nothing and makes
    **   no system call on POSIX: it's safe at any point an interrupt may
    **   unwind. an entry is one word, so an interrupted write never leaves
    **   a torn one. a full set at its ceiling evicts an entry.
    */
      typedef struct {
        const c3_c* nam_c;   //  name, for the log
        c3_d   ovf_d;   //  evictions at the ceiling, since the last report
        c3_d*  tab_d;   //  1 word per entry
        c3_y   log_y;   //  capacity, log2
        c3_y   min_y;   //  starting capacity, log2
        c3_y   max_y;   //  ceiling, log2
        c3_d   use_d;   //  inserts since the last clear
        c3_d   pek_d;   //  most inserts between clears since the last shrink
        c3_w   evk_w;   //  evictions, to rotate through the probe window
        c3_w   lin_w;   //  slots logged for a cheap clear, or past the log
        c3_w   lis_w[256];
        u3c_stat sat_u; //  use since the last u3c_set_stat()
      } u3c_set;

    /* u3c_arr: an array that grows in place, up to a ceiling.
    */
      typedef struct {
        const c3_c* nam_c;   //  name, for the log
        c3_d   drp_d;   //  entries dropped at the ceiling, since the last report
        c3_y*  buf_y;
        size_t siz_i;   //  entry size, bytes
        size_t cap_i;   //  entries committed
        size_t min_i;   //  starting capacity
        size_t max_i;   //  ceiling
        size_t pek_i;   //  most entries used since the last shrink
        u3c_stat sat_u; //  use since the last u3c_arr_stat()
      } u3c_arr;

  /**  Functions.
  **/
    /* u3c_set_init(): reserve for 2^max_y entries, starting at 2^min_y.
    **                 replaces a set already made. home road only.
    */
      void
      u3c_set_init(u3c_set* set_u, const c3_c* nam_c, c3_y min_y, c3_y max_y);

    /* u3c_set_has(): [a_d] is in the set.
    */
      c3_t
      u3c_set_has(u3c_set* set_u, c3_d a_d);

    /* u3c_set_put(): add [a_d], growing if need be.
    */
      void
      u3c_set_put(u3c_set* set_u, c3_d a_d);

    /* u3c_set_clear(): drop every entry, cheaply if few were added.
    */
      void
      u3c_set_clear(u3c_set* set_u);

    /* u3c_set_shrink(): clear, and give back memory if the capacity is
    **                   well above recent use. home road only.
    */
      void
      u3c_set_shrink(u3c_set* set_u);

    /* u3c_set_report(): log any evictions at the ceiling since the last
    **                   call. home road only, between events (logging may
    **                   allocate).
    */
      void
      u3c_set_report(u3c_set* set_u);

    /* u3c_set_cap(): current capacity, in entries.
    */
      c3_d
      u3c_set_cap(u3c_set* set_u);

    /* u3c_set_stat(): the set's use since the last call, and reset it.
    */
      u3c_stat
      u3c_set_stat(u3c_set* set_u);

    /* u3c_arr_init(): reserve for [max_i] entries of [siz_i] bytes, starting
    **                 at [min_i]. home road only.
    */
      void
      u3c_arr_init(u3c_arr* arr_u, const c3_c* nam_c,
                   size_t siz_i, size_t min_i, size_t max_i);

    /* u3c_arr_at(): entry [i_i], growing if need be; 0 past the ceiling.
    */
      void*
      u3c_arr_at(u3c_arr* arr_u, size_t i_i);

    /* u3c_arr_shrink(): note [len_i] entries were used; give back memory
    **                   if the capacity is well above recent use. home
    **                   road only.
    */
      void
      u3c_arr_shrink(u3c_arr* arr_u, size_t len_i);

    /* u3c_arr_used(): note [len_i] entries in use, for u3c_arr_stat(),
    **                 without shrinking.
    */
      void
      u3c_arr_used(u3c_arr* arr_u, size_t len_i);

    /* u3c_arr_report(): log any entries dropped at the ceiling since the
    **                   last call. home road only, between events (logging
    **                   may allocate).
    */
      void
      u3c_arr_report(u3c_arr* arr_u);

    /* u3c_arr_stat(): the array's use since the last call, and reset it.
    */
      u3c_stat
      u3c_arr_stat(u3c_arr* arr_u);

#endif /* ifndef U3_CRIB_H */
