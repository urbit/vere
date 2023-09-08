/// @file

#ifndef U3_JETS_V1_H
#define U3_JETS_V1_H

#include "pkg/noun/allocate.h"
#include "c3.h"
#include "types.h"

      /* u3j_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
      */
        void
        u3j_v1_reclaim(void);

      /* u3j_v1_rewrite_compact(): rewrite jet state for compaction.
      */
        void
        u3j_v1_rewrite_compact();

#endif /* ifndef U3_JETS_V1_H */
