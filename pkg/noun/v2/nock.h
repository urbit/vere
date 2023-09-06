/// @file

#ifndef U3_NOCK_V2_H
#define U3_NOCK_V2_H

#include "pkg/noun/jets.h"

  /**  Functions.
  **/
    /* u3n_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3n_v2_reclaim(void);

    /* u3n_v2_free(): free bytecode cache.
     */
      void
      u3n_v2_free(void);

    /* u3n_v2_rewrite_compact(): rewrite bytecode cache for compaction.
     */
      void
      u3n_v2_rewrite_compact();

#endif /* ifndef U3_NOCK_V2_H */