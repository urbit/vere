/// @file

#ifndef U3_MANAGE_V2_H
#define U3_MANAGE_V2_H

#include "c3.h"
#include "types.h"
#include "version.h"

    /** System management.
    **/
      /* u3m_v2_reclaim: clear persistent caches to reclaim memory
      */
        void
        u3m_v2_reclaim(void);

      /* u3m_v2_migrate: perform pointer compression loom migration if necessary.
         ver_w - target version
      */
        void
        u3m_v2_migrate();

#endif /* ifndef U3_MANAGE_V2_H */