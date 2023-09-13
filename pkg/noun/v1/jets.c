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

/* _cj_fink_free(): lose and free everything in a u3j_fink.
*/
static void
_cj_v1_fink_free(u3p(u3j_fink) fin_p)
{
  fprintf(stderr, "_cj_v1_fink_free 1\r\n");
  c3_w i_w;
  u3j_fink* fin_u = u3to(u3j_fink, fin_p);
  fprintf(stderr, "_cj_v1_fink_free 2 %p %u %x\r\n", fin_u, fin_u->len_w, fin_p);
  u3a_v1_lose(fin_u->sat);
  fprintf(stderr, "_cj_v1_fink_free 3\r\n");
  for ( i_w = 0; i_w < fin_u->len_w; ++i_w ) {
    u3j_fist* fis_u = &(fin_u->fis_u[i_w]);
    fprintf(stderr, "_cj_v1_fink_free 4 %u\r\n", i_w);
    u3a_v1_lose(fis_u->bat);
    fprintf(stderr, "_cj_v1_fink_free 5 %u\r\n", i_w);
    u3a_v1_lose(fis_u->pax);
    fprintf(stderr, "_cj_v1_fink_free 6 %u\r\n", i_w);
  }

  fprintf(stderr, "_cj_v1_fink_free 7\r\n");
  u3a_wfree(fin_u);

  fprintf(stderr, "_cj_v1_fink_free 8\r\n");
}

/* u3j_site_lose(): lose references of u3j_site (but do not free).
 */
void
u3j_v1_site_lose(u3j_site* sit_u)
{
  fprintf(stderr, "u3j_v1_site_lose 1\r\n");
  u3a_v1_lose(sit_u->axe);
  fprintf(stderr, "u3j_v1_site_lose 2\r\n");
  if ( u3_none != sit_u->bat ) {
    fprintf(stderr, "u3j_v1_site_lose 3\r\n");
    u3a_v1_lose(sit_u->bat);
  }
  if ( u3_none != sit_u->bas ) {
    fprintf(stderr, "u3j_v1_site_lose 4\r\n");
    u3a_v1_lose(sit_u->bas);
  }
  fprintf(stderr, "u3j_v1_site_lose 5\r\n");
  if ( u3_none != sit_u->loc ) {
    fprintf(stderr, "u3j_v1_site_lose 6\r\n");
    u3a_v1_lose(sit_u->loc);
    fprintf(stderr, "u3j_v1_site_lose 7\r\n");
    u3a_v1_lose(sit_u->lab);
    fprintf(stderr, "u3j_v1_site_lose 8\r\n");
    if ( c3y == sit_u->fon_o ) {
      fprintf(stderr, "u3j_v1_site_lose 9\r\n");
      if ( sit_u->fin_p ) {
      _cj_v1_fink_free(sit_u->fin_p);
      }
    }
  }
  fprintf(stderr, "u3j_v1_site_lose 10\r\n");
}

/* _cj_v1_free_hank(): free an entry from the hank cache.
**              NB: copy of _cj_v1_free_hank() from pkg/noun/jets.c
*/
static void
_cj_v1_free_hank(u3_noun kev)
{
  fprintf(stderr, "_cj_v1_free_hank 1\r\n");
  _cj_v1_hank* han_u = u3to(_cj_v1_hank, u3t(kev));
  fprintf(stderr, "_cj_v1_free_hank 2\r\n");
  if ( u3_none != han_u->hax ) {
    fprintf(stderr, "_cj_v1_free_hank 3\r\n");
    u3a_v1_lose(han_u->hax);
    fprintf(stderr, "_cj_v1_free_hank 4\r\n");
    u3j_v1_site_lose(&(han_u->sit_u));
  }
  fprintf(stderr, "_cj_v1_free_hank 5 %x %x\r\n", u3h(kev), u3t(kev));
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
  fprintf(stderr, "u3j_v1_reclaim 1 \r\n");
  u3h_v1_walk(u3R->jed.han_p, _cj_v1_free_hank);
  fprintf(stderr, "u3j_v1_reclaim 2 \r\n");
  u3h_v1_free(u3R->jed.han_p);
  fprintf(stderr, "u3j_v1_reclaim 3 \r\n");
  u3R->jed.han_p = u3h_new();  //  XX maybe initialize these at end of migration
  fprintf(stderr, "u3j_v1_reclaim 4 \r\n");
}
