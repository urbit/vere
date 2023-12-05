/// @file

#include "pkg/noun/vortex.h"

#include "pkg/noun/jets.h"
#include "pkg/noun/v1/jets.h"
#include "pkg/noun/v2/jets.h"

#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v1/hashtable.h"


/* _cj_fink_free(): lose and free everything in a u3j_v1_fink.
*/
static void
_cj_v1_fink_free(u3p(u3j_v1_fink) fin_p)
{
  c3_w i_w;
  u3j_v1_fink* fin_u = u3to(u3j_v1_fink, fin_p);
  u3a_v1_lose(fin_u->sat);
  for ( i_w = 0; i_w < fin_u->len_w; ++i_w ) {
    u3j_v1_fist* fis_u = &(fin_u->fis_u[i_w]);
    u3a_v1_lose(fis_u->bat);
    u3a_v1_lose(fis_u->pax);
  }
  u3a_v1_wfree(fin_u);
}

/* u3j_v1_rite_lose(): lose references of u3j_v1_rite (but do not free).
 */
void
u3j_v1_rite_lose(u3j_v1_rite* rit_u)
{
  if ( (c3y == rit_u->own_o) && u3_none != rit_u->clu ) {
    u3a_v1_lose(rit_u->clu);
    _cj_v1_fink_free(rit_u->fin_p);
  }
}


/* u3j_v1_site_lose(): lose references of u3j_v1_site (but do not free).
 */
void
u3j_v1_site_lose(u3j_v1_site* sit_u)
{
  u3a_v1_lose(sit_u->axe);
  if ( u3_none != sit_u->bat ) {
    u3a_v1_lose(sit_u->bat);
  }
  if ( u3_none != sit_u->bas ) {
    u3a_v1_lose(sit_u->bas);
  }
  if ( u3_none != sit_u->loc ) {
    u3a_v1_lose(sit_u->loc);
    u3a_v1_lose(sit_u->lab);
    if ( c3y == sit_u->fon_o ) {
      if ( sit_u->fin_p ) {
      _cj_v1_fink_free(sit_u->fin_p);
      }
    }
  }
}

/* _cj_v1_free_hank(): free an entry from the hank cache.
*/
static void
_cj_v1_free_hank(u3_noun kev)
{
  u3a_v1_cell* cel_u = (u3a_v1_cell*) u3a_v1_to_ptr(kev);
  u3j_v1_hank* han_u = u3to(u3j_v1_hank, cel_u->tel);
  if ( u3_none != han_u->hax ) {
    u3a_v1_lose(han_u->hax);
    u3j_v1_site_lose(&(han_u->sit_u));
  }
  u3a_v1_wfree(han_u);
}

/* u3j_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3j_v1_reclaim(void)
{
  //  clear the jet hank cache
  //
  u3h_v1_walk(u3R_v1->jed.han_p, _cj_v1_free_hank);
  u3h_v1_free_nodes(u3R_v1->jed.han_p);
}
