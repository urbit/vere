/// @file

#ifndef U3_VORTEX_V1_H
#define U3_VORTEX_V1_H

#include "pkg/noun/allocate.h"
#include "pkg/noun/v2/vortex.h"

  /**  Aliases.
  **/
#     define  u3H_v1       u3H_v2
#     define  u3A_v1       u3A_v2
#     define  u3v_v1_home  u3v_v2_home

  /**  Functions.
  **/
    /* u3v_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3v_v1_reclaim(void);

#endif /* ifndef U3_VORTEX_V1_H */
