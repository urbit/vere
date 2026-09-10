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

    /* u3d_boot(): install the SKA core from a steel pill [%steel nok trap ~].
    */
      void
      u3d_boot(u3_noun steel);

    /* u3d_full(): compile [sub fol] as a unary function of its whole
    **             subject.  RETAINS sub and fol.  Produces the IR and,
    **             in *bell, the [sock formula] identity of the function.
    */
      u3_noun
      u3d_full(u3_noun sub, u3_noun fol, u3_noun* bell);

    /* u3d_dire(): compile a bell [sock formula] as a function of the
    **             parts of its subject that it uses.  RETAINS.
    */
      u3_noun
      u3d_dire(u3_noun bell);

    /* u3d_match(): find the most specific [sock *] in `lis` whose sock
    **              matches sub.  RETAINS.  Produces u3_none if none do.
    */
      u3_weak
      u3d_match(u3_noun sub, u3_noun lis);

#endif /* ifndef U3_DIRECT_H */
