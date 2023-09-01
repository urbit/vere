/// @file

#include "pkg/noun/nock.h"
#include "pkg/noun/v1/nock.h"

#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v1/hashtable.h"
#include "pkg/noun/jets.h"
#include "pkg/noun/v1/jets.h"

/* u3n_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3n_v1_reclaim(void)
{
  //  clear the bytecode cache
  //
  //    We can't just u3h_v1_free() -- the value is a post to a u3n_v1_prog.
  //    Note that the hank cache *must* also be freed (in u3j_v1_reclaim())
  //
  u3n_v1_free();
  u3R->byc.har_p = u3h_v1_new();
}

/* _cn_prog_free(): free memory retained by program pog_u
**              NB: copy of _cn_prog_free in pkg/noun/nock.c
*/
static void
_cn_prog_free(u3n_v1_prog* pog_u)
{
  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3a_v1_lose(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3a_v1_lose(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_v1_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_v1_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_v1_free(pog_u);
}

/* _n_feb(): u3h_v1_walk helper for u3n_v1_free
 */
static void
_n_feb(u3_noun kev)
{
  u3a_v1_cell *cel_u = (u3a_v1_cell*) u3a_v1_to_ptr(kev);
  _cn_prog_free(u3to(u3n_v1_prog, cel_u->tel));
}

/* u3n_v1_free(): free bytecode cache
 */
void
u3n_v1_free()
{
  u3p(u3h_v1_root) har_p = u3R->byc.har_p;
  u3h_v1_walk(har_p, _n_feb);
  u3h_v1_free(har_p);
}
