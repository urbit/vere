/// @file

#include "pkg/noun/v2/nock.h"

#include "pkg/noun/vortex.h"

#include "pkg/noun/v2/allocate.h"
#include "pkg/noun/v2/hashtable.h"
#include "pkg/noun/v2/vortex.h"

#include "pkg/noun/v3/hashtable.h"

/* u3n_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3n_v2_reclaim(void)
{
  //  set globals (required for aliased functions)
  u3H = (u3v_home*) u3H_v2;
  u3R = (u3a_road*) u3R_v2;

  //  clear the bytecode cache
  u3n_v2_free();
  u3R->byc.har_p = u3h_v2_new();
}

/* _cn_v2_prog_free(): free memory retained by program pog_u
*/
static void
_cn_v2_prog_free(u3n_v2_prog* pog_u)
{
  // fix up pointers for loom portability
  pog_u->byc_u.ops_y = (c3_y*) ((void*) pog_u) + sizeof(u3n_v2_prog);
  pog_u->lit_u.non   = (u3_noun*) (pog_u->byc_u.ops_y + pog_u->byc_u.len_w);
  pog_u->mem_u.sot_u = (u3n_v2_memo*) (pog_u->lit_u.non + pog_u->lit_u.len_w);
  pog_u->cal_u.sit_u = (u3j_v2_site*) (pog_u->mem_u.sot_u + pog_u->mem_u.len_w);
  pog_u->reg_u.rit_u = (u3j_v2_rite*) (pog_u->cal_u.sit_u + pog_u->cal_u.len_w);

  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3a_v2_lose(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3a_v2_lose(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_v2_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_v2_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_v2_free(pog_u);
}

/* _n_v2_feb(): u3h_v2_walk helper for u3n_v2_free
 */
static void
_n_v2_feb(u3_noun kev)
{
  u3a_v2_cell *cel_u = (u3a_v2_cell*) u3a_v2_to_ptr(kev);
  _cn_v2_prog_free(u3to(u3n_v2_prog, cel_u->tel));
}

/* u3n_v2_free(): free bytecode cache
 */
void
u3n_v2_free()
{
  u3p(u3h_v2_root) har_p = u3R_v2->byc.har_p;
  u3h_v2_walk(har_p, _n_v2_feb);
  u3h_v2_free(har_p);
}

/* u3n_v2_mig_rewrite_compact(): rewrite the bytecode cache for compaction.
 *
 * NB: u3R_v2->byc.har_p *must* be cleared (currently via u3n_v2_reclaim above),
 * since it contains things that look like nouns but aren't.
 * Specifically, it contains "cells" where the tail is a
 * pointer to a u3a_v2_malloc'ed block that contains loom pointers.
 *
 * You should be able to walk this with u3h_v2_walk and rewrite the
 * pointers, but you need to be careful to handle that u3a_v2_malloc
 * pointers can't be turned into a box by stepping back two words. You
 * must step back one word to get the padding, step then step back that
 * many more words (plus one?).
 */
void
u3n_v2_mig_rewrite_compact()
{
  u3h_v2_rewrite(u3R_v2->byc.har_p);
  u3R_v2->byc.har_p = u3a_v2_rewritten(u3R_v2->byc.har_p);
}
