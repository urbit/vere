/// @file

#include "pkg/noun/vortex.h"

#include "pkg/noun/jets.h"
#include "pkg/noun/v1/jets.h"
#include "pkg/noun/v2/jets.h"

#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v1/hashtable.h"

/**  Data structures.
**/

/* _cj_v1_hank: cached hook information.
**       NB: copy of _cj_hank from pkg/noun/jets.c
 */
typedef struct {
  u3_weak  hax;                     //  axis of hooked inner core
  u3j_site sit_u;                   //  call-site data
} _cj_v1_hank;

/**  Functions.
**/

/* _cj_v1_free_hank(): free an entry from the hank cache.
**              NB: copy of _cj_v1_free_hank() from pkg/noun/jets.c
*/
static void
_cj_v1_free_hank(u3_noun kev)
{
  _cj_v1_hank* han_u = u3to(_cj_v1_hank, u3t(kev));
  if ( u3_none != han_u->hax ) {
    u3a_v1_lose(han_u->hax);
    u3j_site_lose(&(han_u->sit_u));
  }
  u3a_wfree(han_u);
}

/* u3j_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3j_v1_reclaim(void)
{
  //  re-establish the warm jet state
  //
  //    XX might this reduce fragmentation?
  //
  // if ( &(u3H->rod_u) == u3R ) {
  //   u3j_ream();
  // }

  //  clear the jet hank cache
  //
  u3h_v1_walk(u3R->jed.han_p, _cj_v1_free_hank);
  u3h_v1_free(u3R->jed.han_p);
  u3R->jed.han_p = u3h_new();  //  XX maybe initialize these at end of migration
}
