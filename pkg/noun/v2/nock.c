/// @file

#include "pkg/noun/v2/nock.h"

#include "pkg/noun/v2/hashtable.h"

/* u3n_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
**  XX need to version
*/
void
u3n_v2_reclaim(void)
{
  //  clear the bytecode cache
  //
  //    We can't just u3h_free() -- the value is a post to a u3n_v2_prog.
  //    Note that the hank cache *must* also be freed (in u3j_reclaim())
  //
  u3n_v2_free();
  u3R->byc.har_p = u3h_new();
}

/* u3n_v2_free(): free bytecode cache
 */
void
u3n_v2_free()
{
  u3p(u3h_root) har_p = u3R->byc.har_p;
  u3h_v2_walk(har_p, _n_feb);
  u3h_v2_free(har_p);
}
