/// @file

#ifndef U3_JETS_V1_H
#define U3_JETS_V1_H

#include "pkg/noun/jets.h"
#include "pkg/noun/v2/jets.h"

  /**  Aliases.
  **/
#     define u3j_v1_fink       u3j_v2_fink
#     define u3j_v1_fist       u3j_v2_fist
#     define u3j_v1_hank       u3j_v2_hank
#     define u3j_v1_rite       u3j_v2_rite
#     define u3j_v1_site       u3j_v2_site

  /**  Functions.
  **/
    /* u3j_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3j_v1_reclaim(void);

    /* u3j_v1_rite_lose(): lose references of u3j_rite (but do not free).
    */
      void
      u3j_v1_rite_lose(u3j_v1_rite* rit_u);

    /* u3j_v1_site_lose(): lose references of u3j_site (but do not free).
    */
      void
      u3j_v1_site_lose(u3j_v1_site* sit_u);

#endif /* ifndef U3_JETS_V1_H */
