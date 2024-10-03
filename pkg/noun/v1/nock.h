/// @file

#ifndef U3_NOCK_V1_H
#define U3_NOCK_V1_H

#include "pkg/noun/v2/nock.h"

  /**  Aliases.
  **/
#     define  u3n_v1_memo  u3n_v2_memo
#     define  u3n_v1_prog  u3n_v2_prog

  /**  Functions.
  **/
    /* u3n_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3n_v1_reclaim(void);

    /* u3n_v1_free(): free bytecode cache.
     */
      void
      u3n_v1_free(void);

#endif /* ifndef U3_NOCK_V1_H */
