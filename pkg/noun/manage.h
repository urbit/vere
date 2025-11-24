/// @file

#ifndef U3_MANAGE_H
#define U3_MANAGE_H

// #include "v1/manage.h"
// #include "v2/manage.h"

#include "c3/c3.h"
#include "types.h"
#include "version.h"
#include "rsignal.h"

    /** System management.
    **/
      /* u3m_boot(): start the u3 system. return next event, starting from 1.
      */
        c3_d
        u3m_boot(c3_c* dir_c, size_t len_i);

      /* u3m_pier(): make a pier.
      */
        c3_c*
        u3m_pier(c3_c* dir_c);

      /* u3m_boot_lite(): start without checkpointing.
      */
        c3_d
        u3m_boot_lite(size_t len_i);

      /* u3m_stop(): graceful shutdown cleanup. */
        void
        u3m_stop(void);

      /* u3m_bail(): bail out.  Does not return.
      **
      **  Bail motes:
      **
      **    %exit               ::  semantic failure
      **    %evil               ::  bad crypto
      **    %intr               ::  interrupt
      **    %fail               ::  execution failure
      **    %foul               ::  assert failure
      **    %need               ::  network block
      **    %meme               ::  out of memory
      **    %time               ::  timed out
      **    %oops               ::  assertion failure
      */
        c3_i
        u3m_bail(c3_m how_m) __attribute__((noreturn));

      /* u3m_fault(): handle a memory event with libsigsegv protocol.
      */
        c3_i
        u3m_fault(void* adr_v, c3_i ser_i);

      /* u3m_foul(): dirty all pages and disable tracking.
      */
        void
        u3m_foul(void);

      /* u3m_backup(): copy snapshot to .urb/bhk (if it doesn't exist yet).
      */
        c3_o
        u3m_backup(c3_o);

      /* u3m_save(): update the checkpoint.
      */
        void
        u3m_save(void);

      /* u3m_toss(): discard ephemeral memory.
      */
        void
        u3m_toss(void);

      /* u3m_ward(): tend the guardpage.
      */
        void
        u3m_ward(void);

      /* u3m_init(): start the environment.
      */
        void
        u3m_init(size_t len_i);

      /* u3m_pave(): instantiate or activate image.
      */
        void
        u3m_pave(c3_o nuu_o);

      /* u3m_signal(): treat a nock-level exception as a signal interrupt.
      */
        void
        u3m_signal(u3_noun sig_l);

      /* u3m_file(): load file, as atom, or bail.
      */
        u3_noun
        u3m_file(c3_c* pas_c);

      /* u3m_error(): bail out with %exit, ct_pushing error.
      */
        c3_i
        u3m_error(c3_c* str_c);

      /* u3m_hate(): new, integrated leap mechanism (enter).
      */
        void
        u3m_hate(c3_w pad_w);

      /* u3m_love(): return product from leap.
      */
        u3_noun
        u3m_love(u3_noun pro);

      /* u3m_soft(): system soft wrapper.  unifies unix and nock errors.
      **
      **  Produces [%$ result] or [%error (list tank)].
      */
        u3_noun
        u3m_soft(c3_w mil_w, u3_funk fun_f, u3_noun arg);

      /* u3m_soft_cax(): descend into virtualization context, with cache.
      */
        u3_noun
        u3m_soft_cax(u3_funq fun_f, u3_noun aga, u3_noun agb);

      /* u3m_soft_slam: top-level call.
      */
        u3_noun
        u3m_soft_slam(u3_noun gat, u3_noun sam);

      /* u3m_soft_nock: top-level nock.
      */
        u3_noun
        u3m_soft_nock(u3_noun bus, u3_noun fol);

      /* u3m_soft_sure(): top-level call assumed correct.
      */
        u3_noun
        u3m_soft_sure(u3_funk fun_f, u3_noun arg);

      /* u3m_soft_run(): descend into virtualization context.
      */
        u3_noun
        u3m_soft_run(u3_noun gul,
                     u3_funq fun_f,
                     u3_noun aga,
                     u3_noun agb);

      /* u3m_soft_esc(): namespace lookup to (unit ,*).
      */
        u3_noun
        u3m_soft_esc(u3_noun ref, u3_noun sam);


      /* u3m_quac: memory report.
      */
        typedef struct _u3m_quac {
          c3_c* nam_c;
          c3_w  siz_w;
          struct _u3m_quac** qua_u;
        } u3m_quac;

      /* u3m_mark(): mark all nouns in the road.
      */
        u3m_quac**
        u3m_mark();

      /* u3m_grab(): garbage-collect the world, plus extra roots.
      */
        void
        u3m_grab(u3_noun som, ...);   // terminate with u3_none

      /* u3m_water(): produce high and low watermarks.  Asserts u3R == u3H.
      */
        void
        u3m_water(u3_post* low_p, u3_post* hig_p);

      /* u3m_pretty(): dumb prettyprint to string.  RETAIN.
      */
        c3_c*
        u3m_pretty(u3_noun som);

      /* u3m_pretty_road(): dumb prettyprint to string. Road allocation
      */
        c3_c*
        u3m_pretty_road(u3_noun som);

      /* u3m_pretty_path(): prettyprint a path to string.  RETAIN.
      */
        c3_c*
        u3m_pretty_path(u3_noun som);

      /* u3m_p(): dumb print with caption.  RETAIN.
      */
        void
        u3m_p(const c3_c* cap_c, u3_noun som);

      /* u3m_tape(): dump a tape to stdout.
      */
        void
        u3m_tape(u3_noun tep);

      /* u3m_wall(): dump a wall to stdout.
      */
        void
        u3m_wall(u3_noun wol);

      /* u3m_reclaim: clear persistent caches to reclaim memory.
      */
        void
        u3m_reclaim(void);

      /* u3m_pack: compact (defragment) memory, returns u3a_open delta.
      */
        c3_w
        u3m_pack(void);

    /*  Urbit time: 128 bits, leap-free.
    **
    **  High 64 bits: 0x8000.000c.cea3.5380 + Unix time at leap 25 (Jul 2012)
    **  Low 64 bits: 1/2^64 of a second.
    **
    **  Seconds per Gregorian 400-block: 12.622.780.800
    **  400-blocks from 0 to 0AD: 730.692.561
    **  Years from 0 to 0AD: 292.277.024.400
    **  Seconds from 0 to 0AD: 9.223.372.029.693.628.800
    **  Seconds between 0A and Unix epoch: 62.167.219.200
    **  Seconds before Unix epoch: 9.223.372.091.860.848.000
    **  The same, in C hex notation: 0x8000000cce9e0d80ULL
    **
    **  XX: needs to be adjusted to implement Google leap-smear time.
    */
      /* u3m_time_sec_in(): urbit seconds from unix time.
      **
      ** Adjust (externally) for future leap secs!
      */
        c3_d
        u3m_time_sec_in(c3_w unx_w);

      /* u3m_time_sec_out(): unix time from urbit seconds.
      **
      ** Adjust (externally) for future leap secs!
      */
        c3_w
        u3m_time_sec_out(c3_d urs_d);

      /* u3m_time_fsc_in(): urbit fracto-seconds from unix microseconds.
      */
        c3_d
        u3m_time_fsc_in(c3_w usc_w);

      /* u3m_time_fsc_out: unix microseconds from urbit fracto-seconds.
      */
        c3_w
        u3m_time_fsc_out(c3_d ufc_d);

      /* u3m_time_in_tv(): urbit time from struct timeval.
      */
        u3_atom
        u3m_time_in_tv(struct timeval* tim_tv);

      /* u3m_time_out_tv(): struct timeval from urbit time.
      */
        void
        u3m_time_out_tv(struct timeval* tim_tv, u3_noun now);

      /* u3m_time_in_ts(): urbit time from struct timespec.
      */
        u3_atom
        u3m_time_in_ts(struct timespec* tim_ts);
        #if defined(U3_OS_linux) || defined(U3_OS_windows)
        /* u3m_time_t_in_ts(): urbit time from time_t.
        */
        u3_atom
        u3m_time_t_in_ts(time_t tim);
        #endif
        
      /* u3m_time_out_ts(): struct timespec from urbit time.
      */
        void
        u3m_time_out_ts(struct timespec* tim_ts, u3_noun now);

      /* u3m_time_out_it(): struct itimerval from urbit time gap.
      ** returns true if it_value is set to non-zero values, false otherwise
      */
        c3_t
        u3m_time_out_it(struct itimerval* tim_it, u3_noun gap);

      /* u3m_time_gap_ms(): (wen - now) in ms.
      */
        c3_d
        u3m_time_gap_ms(u3_noun now, u3_noun wen);

      /* u3m_timer_set(): push a new timer to the timer stack.
      ** gap is @dr, gap != 0
      */
        void
        u3m_timer_set(u3_atom gap);

      /* u3m_timer_pop(): pop a timer off the timer stack.
      ** timer stack must be non-empty
      */
        void
        u3m_timer_pop(void);

      /* u3m_time_gap_in_mil(): urbit time gap from milliseconds
      */
        u3_atom
        u3m_time_gap_in_mil(c3_w mil_w);

#endif /* ifndef U3_MANAGE_H */
