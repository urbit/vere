/// @file

#ifndef U3_DIRECT_H
#define U3_DIRECT_H

#include "c3/c3.h"
#include "types.h"

  /** u3d: the SKA core.
  ***
  *** The core (nock-compilation.hoon) analyzes and compiles nock to the
  *** IR consumed by nock-compile.c.  It is a plain noun in u3R->ska.cor,
  *** poked like an arvo core.
  **/

    /* u3d_boot(): bring the SKA core up to a jammed steel pill [%steel nok
    **             trap ~]: install it if there is no core; if the pill's
    **             version is not the installed core's, replace the core by
    **             the pill's +load of it and drop all programs; then
    **             u3d_hot().  The empty pill, (jam 0) = 2, runs no SKA: no
    **             core is installed, and an installed one is dropped with
    **             its programs.  TRANSFERS.
    */
      void
      u3d_boot(u3_atom pil);

    /* u3d_hot(): feed the core the jet drivers there are, and drop the
    **            programs of the functions a change in them, or a core
    **            registered since the last time, invalidates.  No compiled
    **            nock may be running: on the home road, at boot.
    */
      void
      u3d_hot(void);

    /* u3d_cold(): feed the core every registration there is, in place of
    **             those it has: after installing a core.
    */
      void
      u3d_cold(void);

    /* u3d_full(): compile [sub fol] as a unary function of its whole
    **             subject.  RETAINS sub and fol.  Produces the IR and,
    **             in *bell, the [sock formula] identity of the function.
    */
      u3_noun
      u3d_full(u3_noun sub, u3_noun fol, u3_noun* bell);

    /* u3d_dire(): compile a bell [sock formula] as a function of the
    **             parts of its subject that it uses or, if mon_o, as a
    **             unary function of its whole subject.  RETAINS.
    */
      u3_noun
      u3d_dire(u3_noun bell, c3_o mon_o);

    /* u3d_match(): find the most specific [sock *] in `lis` whose sock
    **              matches sub.  RETAINS.  Produces u3_none if none do.
    */
      u3_weak
      u3d_match(u3_noun sub, u3_noun lis);

    /* u3d_match_las(): u3d_match(), each candidate i checked against the
    **                  last subject that matched it, las_f(i, ptr_v)
    **                  (transferred, or u3_none): what is the same noun
    **                  there is skipped.  RETAINS.
    */
      u3_weak
      u3d_match_las(u3_noun sub, u3_noun lis,
                    u3_weak (*las_f)(u3_noun, void*), void* ptr_v);

#endif /* ifndef U3_DIRECT_H */
