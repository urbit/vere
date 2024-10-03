#ifndef U3_HASHTABLE_V3_H
#define U3_HASHTABLE_V3_H

#define u3h_v3_free          u3h_free
#define u3h_v3_new           u3h_new
#define u3h_v3_root          u3h_root
#define u3h_v3_walk          u3h_walk

#include "pkg/noun/hashtable.h"

  /**  Functions.
  **/
    /* u3h_v3_new_cache(): create hashtable with bounded size.
    */
      u3p(u3h_v3_root)
      u3h_v3_new_cache(c3_w clk_w);

#endif /* U3_HASHTABLE_V3_H */
