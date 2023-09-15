/// @file

#ifndef U3_VORTEX_V3_H
#define U3_VORTEX_V3_H

#include "pkg/noun/vortex.h"

#include "pkg/noun/v3/allocate.h"
#include "pkg/noun/version.h"

  /**  Aliases.
  **/
#     define     u3v_v3_arvo  u3v_arvo

  /**  Data structures.
  **/
    /* u3v_v3_home: all internal (within image) state.
    **       NB: version must be last for discriminability in north road
    */
      typedef struct _u3v_v3_home {
        u3a_v3_road    rod_u;                //  storage state
        u3v_v3_arvo    arv_u;                //  arvo state
        u3v_version    ver_w;                //  version number
      } u3v_v3_home;

  /**  Globals.
  **/
      /// Arvo internal state.
      extern u3v_v3_home* u3v_v3_Home;
#       define u3H_v3  u3v_v3_Home
#       define u3A_v3  (&(u3v_v3_Home->arv_u))

  /**  Functions.
  **/
    /* u3v_v3_rewrite_compact(): rewrite arvo kernel for compaction.
    */
      void
      u3v_v3_rewrite_compact();

#endif /* ifndef U3_VORTEX_V3_H */
