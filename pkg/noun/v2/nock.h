/// @file

#ifndef U3_NOCK_V2_H
#define U3_NOCK_V2_H

#include "pkg/noun/nock.h"

#include "types.h"

  /**  Aliases.
  **/
#     define  u3n_v2_memo     u3n_memo
#     define  u3n_v2_prog     u3n_prog
#     define  u3n_v2_reclaim  u3n_reclaim

    /* u3n_v2_rewrite_compact(): rewrite bytecode cache for compaction.
     */
      void
      u3n_v2_rewrite_compact();

#endif /* ifndef U3_NOCK_V2_H */
