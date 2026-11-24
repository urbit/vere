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
///
/// An interrupt may unwind a grow or a shrink anywhere, if the crib's
/// user keeps the capacity it relies on within the crib's length at
/// every step: raised after a grow returns, lowered before a shrink is
/// called. The crib does the same with the memory under its length
/// (committed before the length is raised, given back after it's
/// lowered), and nothing else is written, so no step leaves a state the
/// next call can't go on from. What an interrupted shrink does leave is
/// memory past the length that wasn't given back: it keeps its contents
/// when grown into again.

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

    /* u3a_crib_shrink(): give back everything past the first [len_i]
    **                    bytes; it reads as zero when grown into again,
    **                    unless an interrupt unwound the shrink.
    */
      void
      u3a_crib_shrink(u3a_crib* cib_u, size_t len_i);

  /**  Yards.
  ***
  ***  A yard (u3a_yard) is a crib used as a bump arena: memory is taken
  ***  from its end and given back by putting the end back, so one object
  ***  at a time grows in it, contiguous, and its users nest as scopes.
  ***  Two yards used together hold two objects that grow at once (a
  ***  stack in one, a list in the other). Memory taken isn't zeroed.
  ***
  ***  Like a crib's, a yard's growth allocates nothing. It's for the
  ***  home road's scratch all the same: its fields aren't written in an
  ***  order that an interrupt could unwind.
  ***
  ***  The runtime has two, reserved by u3m_init(): u3a_Yard_wok for
  ***  scratch, and u3a_Yard_res for results. A user is a scope: it reads
  ***  a yard's end (u3a_yard_here()), takes what it needs, and puts the
  ***  end back (u3a_yard_back()) before it returns, so users nest. What a
  ***  callee takes from the result yard and leaves there is its caller's
  ***  to give back.
  ***
  ***  Nothing holds a yard's memory across an excursion from the home
  ***  road: u3m_leap() from home puts both ends back to the start, as a
  ***  failed event's reset does (_cm_signal_reset()). That catches a
  ***  user that never reached its u3a_yard_back(); it's a bug all the
  ***  same, and asserted under C3DBG.
  ***
  ***  A yard put back to its start gives back what it committed past
  ***  its floor. A user that grew it did work for every byte, so
  ***  committing it again costs little beside that; and no caller has
  ***  to trim.
  **/
    /* u3a_yard: a reserved region and its end.
    */
      typedef struct {
        c3_y*  buf_y;   //  start
        size_t max_i;   //  bytes reserved
        size_t min_i;   //  floor: bytes kept committed, once used
        size_t use_i;   //  bytes in use
        size_t com_i;   //  bytes committed
        //  measurement: the reader's to zero
        //
        size_t pek_i;   //  most in use
        c3_w   gro_w;   //  commits
        c3_w   shr_w;   //  times memory was given back
      } u3a_yard;

    /* u3a_yard_init(): reserve [max_i] bytes (rounded down to 16, 64 at
    **                  least), committing none. releases [yar_u]'s old
    **                  region, if any. home road only.
    */
      void
      u3a_yard_init(u3a_yard* yar_u, size_t max_i);

    /* u3a_Yard_wok, u3a_Yard_res: the runtime's scratch and result
    **                             yards (see above).
    */
      extern u3a_yard u3a_Yard_wok;
      extern u3a_yard u3a_Yard_res;

    /* u3a_yard_more(): u3a_yard_take() of [len_i] bytes at [bas_i],
    **                  past what's committed.
    */
      void*
      u3a_yard_more(u3a_yard* yar_u, size_t bas_i, size_t len_i);

    /* u3a_yard_take(): [len_i] more bytes, at the first multiple of
    **                  [aln_i] (a power of two, a page at most) after
    **                  the last ones taken; NULL past the reservation,
    **                  or if the commit fails, and then nothing is
    **                  taken. takes of one size and alignment are
    **                  contiguous after the first.
    */
      static inline void*
      u3a_yard_take(u3a_yard* yar_u, size_t len_i, size_t aln_i)
      {
        size_t bas_i = (yar_u->use_i + (aln_i - 1)) & ~(aln_i - 1);
        size_t new_i = bas_i + len_i;

        if ( c3_unlikely(new_i > yar_u->com_i) ) {
          return u3a_yard_more(yar_u, bas_i, len_i);
        }
        if ( new_i > yar_u->pek_i ) {
          yar_u->pek_i = new_i;
        }
        yar_u->use_i = new_i;

        return yar_u->buf_y + bas_i;
      }

    /* u3a_yard_here(): the yard's end, for u3a_yard_back().
    */
      static inline size_t
      u3a_yard_here(u3a_yard* yar_u)
      {
        return yar_u->use_i;
      }

    /* u3a_yard_drop(): give back what's committed past the floor.
    */
      void
      u3a_yard_drop(u3a_yard* yar_u);

    /* u3a_yard_back(): give back everything taken since the end was
    **                  [end_i]. back at the start, the yard gives back
    **                  what it committed past its floor.
    */
      static inline void
      u3a_yard_back(u3a_yard* yar_u, size_t end_i)
      {
        yar_u->use_i = end_i;

        if ( c3_unlikely(!end_i && (yar_u->com_i > yar_u->min_i)) ) {
          u3a_yard_drop(yar_u);
        }
      }

    /* u3a_yard_give(): give back the last [len_i] bytes taken: a pop.
    **                  commits nothing back, even at the start.
    */
      static inline void
      u3a_yard_give(u3a_yard* yar_u, size_t len_i)
      {
        yar_u->use_i -= len_i;
      }

#endif /* ifndef U3_CRIB_H */
