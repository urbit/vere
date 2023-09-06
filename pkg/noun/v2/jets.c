/// @file

#include "pkg/noun/jets.h"
#include "pkg/noun/v2/jets.h"

#include "v2/hashtable.h"

/**  Data structures.
**/

/* _cj_hank: cached hook information.
**       NB: copy of _cj_hank from pkg/noun/jets.c
 */
typedef struct {
  u3_weak  hax;                     //  axis of hooked inner core
  u3j_site sit_u;                   //  call-site data
} _cj_hank;

/* _cj_free_hank(): free an entry from the hank cache.
**              NB: copy of _cj_free_hank() from pkg/noun/jets.c
*/
static void
_cj_free_hank(u3_noun kev)
{
  _cj_hank* han_u = u3to(_cj_hank, u3t(kev));
  if ( u3_none != han_u->hax ) {
    u3z(han_u->hax);
    u3j_site_lose(&(han_u->sit_u));
  }
  u3a_wfree(han_u);
}


/* u3j_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
**  XX need to version
*/
void
u3j_v2_reclaim(void)
{
  //  re-establish the warm jet state
  //
  //    XX might this reduce fragmentation?
  //
  // if ( &(u3H->rod_u) == u3R ) {
  //   u3j_v2_ream();
  // }

  //  clear the jet hank cache
  //
  u3h_v1_walk(u3R->jed.han_p, _cj_free_hank);
  u3h_v1_free(u3R->jed.han_p);
  u3R->jed.han_p = u3h_v1_new();
}

/* u3j_v2_rewrite_compact(): rewrite jet state for compaction.
 *
 * NB: u3R->jed.han_p *must* be cleared (currently via u3j_v2_reclaim above)
 * since it contains hanks which are not nouns but have loom pointers.
 * Alternately, rewrite the entries with u3h_v1_walk, using u3j_v2_mark as a
 * template for how to walk.  There's an untested attempt at this in git
 * history at e8a307a.
*/
void
u3j_v2_rewrite_compact()
{
  u3h_v2_rewrite(u3R->jed.war_p);
  u3h_v2_rewrite(u3R->jed.cod_p);
  u3h_v2_rewrite(u3R->jed.han_p);
  u3h_v2_rewrite(u3R->jed.bas_p);

  if ( u3R == &(u3H->rod_u) ) {
    u3h_v1_rewrite(u3R->jed.hot_p);
    u3R->jed.hot_p = u3a_v2_rewritten(u3R->jed.hot_p);
  }

  u3R->jed.war_p = u3a_v2_rewritten(u3R->jed.war_p);
  u3R->jed.cod_p = u3a_v2_rewritten(u3R->jed.cod_p);
  u3R->jed.han_p = u3a_v2_rewritten(u3R->jed.han_p);
  u3R->jed.bas_p = u3a_v2_rewritten(u3R->jed.bas_p);
}
