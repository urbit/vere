/// @file

#ifndef U3_VORTEX_V2_H
#define U3_VORTEX_V2_H

#include "../vortex.h"

#include "v2/allocate.h"
#include "../version.h"

  /**  Aliases.
  **/
#     define     u3v_v2_arvo  u3v_arvo

  /**  Data structures.
  **/
    /* u3v_v2_home: all internal (within image) state.
    **       NB: version must be last for discriminability in north road
    */
      typedef struct _u3v_v2_home {
        u3a_v2_road    rod_u;                //  storage state
        u3v_v2_arvo    arv_u;                //  arvo state
        u3v_version    ver_w;                //  version number
      } u3v_v2_home;

  /**  Globals.
  **/
      /// Arvo internal state.
      extern u3v_v2_home* u3v_v2_Home;
#       define u3H_v2  u3v_v2_Home
#       define u3A_v2  (&(u3v_v2_Home->arv_u))

  /**  Functions.
  **/
    /* u3v_v2_mig_rewrite_compact(): rewrite arvo kernel for compaction.
    */
      void
      u3v_v2_mig_rewrite_compact(void);

#endif /* ifndef U3_VORTEX_V2_H */
