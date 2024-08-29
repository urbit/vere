/// @file

#include "pkg/noun/vortex.h"

#include "pkg/noun/jets.h"
#include "pkg/noun/v2/jets.h"

#include "pkg/noun/v2/allocate.h"
#include "pkg/noun/v2/hashtable.h"
#include "pkg/noun/v2/vortex.h"

#include "pkg/noun/v3/hashtable.h"
#include "pkg/noun/v3/jets.h"

/* u3j_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3j_v2_reclaim(void)
{
  //  set globals (required for aliased functions)
  u3H = (u3v_home*) u3H_v2;
  u3R = (u3a_road*) u3R_v2;

  //  clear the jet hank cache
  //
  u3h_v3_walk(u3R->jed.han_p, u3j_v3_free_hank);
  u3h_v3_free(u3R->jed.han_p);
  u3R->jed.han_p = u3h_v3_new();
}

/* u3j_v2_mig_rewrite_compact(): rewrite jet state for compaction.
 *
 * NB: u3R_v2->jed.han_p *must* be cleared (currently via u3j_v2_reclaim above)
 * since it contains hanks which are not nouns but have loom pointers.
 * Alternately, rewrite the entries with u3h_v2_walk, using u3j_v2_mark as a
 * template for how to walk.  There's an untested attempt at this in git
 * history at e8a307a.
*/
void
u3j_v2_mig_rewrite_compact()
{
  u3h_v2_rewrite(u3R_v2->jed.war_p);
  u3h_v2_rewrite(u3R_v2->jed.cod_p);
  u3h_v2_rewrite(u3R_v2->jed.han_p);
  u3h_v2_rewrite(u3R_v2->jed.bas_p);

  u3h_v2_rewrite(u3R_v2->jed.hot_p);
  u3R_v2->jed.hot_p = u3a_v2_rewritten(u3R_v2->jed.hot_p);

  u3R_v2->jed.war_p = u3a_v2_rewritten(u3R_v2->jed.war_p);
  u3R_v2->jed.cod_p = u3a_v2_rewritten(u3R_v2->jed.cod_p);
  u3R_v2->jed.han_p = u3a_v2_rewritten(u3R_v2->jed.han_p);
  u3R_v2->jed.bas_p = u3a_v2_rewritten(u3R_v2->jed.bas_p);
}
