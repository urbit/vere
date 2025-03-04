/// @file

#ifndef U3_MANAGE_H
#define U3_MANAGE_H

#include "v1/manage.h"
#include "v2/manage.h"

#include "c3/c3.h"
#include "types.h"
#include "version.h"

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

#endif /* ifndef U3_MANAGE_H */
