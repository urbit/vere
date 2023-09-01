/// @file

#include "pkg/noun/vortex.h"

#include "pkg/noun/jets.h"
#include "pkg/noun/v2/jets.h"

#include "pkg/noun/v2/allocate.h"
#include "pkg/noun/v2/hashtable.h"
#include "pkg/noun/v2/vortex.h"

/**  Data structures.
**/

/**  Functions.
**/

/* u3j_v2_rewrite_compact(): rewrite jet state for compaction.
 *
 * NB: u3R_v2->jed.han_p *must* be cleared (currently via u3j_v2_reclaim above)
 * since it contains hanks which are not nouns but have loom pointers.
 * Alternately, rewrite the entries with u3h_v2_walk, using u3j_v2_mark as a
 * template for how to walk.  There's an untested attempt at this in git
 * history at e8a307a.
*/
void
u3j_v2_rewrite_compact()
{
  u3h_v2_rewrite(u3R_v2->jed.war_p);
  u3h_v2_rewrite(u3R_v2->jed.cod_p);
  u3h_v2_rewrite(u3R_v2->jed.han_p);
  u3h_v2_rewrite(u3R_v2->jed.bas_p);

  if ( u3R_v2 == &(u3H_v2->rod_u) ) {
    u3h_v2_rewrite(u3R_v2->jed.hot_p);
    u3R_v2->jed.hot_p = u3a_v2_rewritten(u3R_v2->jed.hot_p);
  }

  u3R_v2->jed.war_p = u3a_v2_rewritten(u3R_v2->jed.war_p);
  u3R_v2->jed.cod_p = u3a_v2_rewritten(u3R_v2->jed.cod_p);
  u3R_v2->jed.han_p = u3a_v2_rewritten(u3R_v2->jed.han_p);
  u3R_v2->jed.bas_p = u3a_v2_rewritten(u3R_v2->jed.bas_p);
}
