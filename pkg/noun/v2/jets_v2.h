/// @file

#ifndef U3_JETS_V2_H
#define U3_JETS_V2_H

#include "pkg/noun/allocate.h"
#include "pkg/noun/jets.h"


  /**  Aliases.
  **/
#     define  u3j_v2_core       u3j_core
#     define  u3j_v2_fink       u3j_fink
#     define  u3j_v2_fist       u3j_fist
#     define  u3j_v2_hank       u3j_hank
#     define  u3j_v2_free_hank  u3j_free_hank
#     define  u3j_v2_harm       u3j_harm
#     define  u3j_v2_rite       u3j_rite
#     define  u3j_v2_site       u3j_site
#     define  u3j_v2_rite_lose  u3j_rite_lose
#     define  u3j_v2_site_lose  u3j_site_lose

  /**  Functions.
  **/
    /* u3j_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3j_v2_reclaim(void);

    /* u3j_v2_mig_rewrite_compact(): rewrite jet state for compaction.
    */
      void
      u3j_v2_mig_rewrite_compact();

#endif /* ifndef U3_JETS_V2_H */
