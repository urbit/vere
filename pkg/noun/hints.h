/// @file
///
/// Growable hint tables (u3g): off-loom open-addressing sets and arrays
/// for the hash-consing and unification state that lives for one
/// excursion from the home road, or one comparison.

#ifndef U3_HINTS_H
#define U3_HINTS_H

#include "c3/c3.h"

  /**  Data structures.
  **/
    /* u3g_set: a set of key pairs [a b], a != 0 (0 marks an empty slot).
    **
    **   a region is reserved for the most entries the set may grow to,
    **   and committed as it grows, so growing allocates nothing and makes
    **   no system call on POSIX: it's safe at any point an interrupt may
    **   unwind. writes are ordered (a last), so an interrupted write
    **   leaves an empty slot, never a torn pair. a full set at its ceiling
    **   evicts an entry.
    */
      typedef struct {
        c3_d*  tab_d;   //  2 words per entry
        c3_y   log_y;   //  capacity, log2
        c3_y   min_y;   //  starting capacity, log2
        c3_y   max_y;   //  ceiling, log2
        c3_d   use_d;   //  inserts since the last clear
        c3_d   pek_d;   //  most inserts between clears since the last shrink
        c3_w   evk_w;   //  evictions, to rotate through the probe window
        c3_w   lin_w;   //  slots logged for a cheap clear, or past the log
        c3_w   lis_w[256];
      } u3g_set;

    /* u3g_arr: an array that grows in place, up to a ceiling.
    */
      typedef struct {
        c3_y*  buf_y;
        size_t siz_i;   //  entry size, bytes
        size_t cap_i;   //  entries committed
        size_t min_i;   //  starting capacity
        size_t max_i;   //  ceiling
        size_t pek_i;   //  most entries used since the last shrink
      } u3g_arr;

  /**  Functions.
  **/
    /* u3g_set_init(): reserve for 2^max_y entries, starting at 2^min_y.
    **                 replaces a set already made. home road only.
    */
      void
      u3g_set_init(u3g_set* set_u, c3_y min_y, c3_y max_y);

    /* u3g_set_has(): [a b] is in the set.
    */
      c3_t
      u3g_set_has(u3g_set* set_u, c3_d a_d, c3_d b_d);

    /* u3g_set_put(): add [a b], growing if need be.
    */
      void
      u3g_set_put(u3g_set* set_u, c3_d a_d, c3_d b_d);

    /* u3g_set_clear(): drop every entry, cheaply if few were added.
    */
      void
      u3g_set_clear(u3g_set* set_u);

    /* u3g_set_shrink(): clear, and give back memory if the capacity is
    **                   well above recent use. home road only.
    */
      void
      u3g_set_shrink(u3g_set* set_u);

    /* u3g_set_cap(): current capacity, in entries.
    */
      c3_d
      u3g_set_cap(u3g_set* set_u);

    /* u3g_arr_init(): reserve for [max_i] entries of [siz_i] bytes, starting
    **                 at [min_i]. home road only.
    */
      void
      u3g_arr_init(u3g_arr* arr_u, size_t siz_i, size_t min_i, size_t max_i);

    /* u3g_arr_at(): entry [i_i], growing if need be; 0 past the ceiling.
    */
      void*
      u3g_arr_at(u3g_arr* arr_u, size_t i_i);

    /* u3g_arr_shrink(): note [len_i] entries were used; give back memory
    **                   if the capacity is well above recent use. home
    **                   road only.
    */
      void
      u3g_arr_shrink(u3g_arr* arr_u, size_t len_i);

#endif /* ifndef U3_HINTS_H */
