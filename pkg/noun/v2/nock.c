/// @file

#include "pkg/noun/nock.h"
#include "pkg/noun/v2/nock.h"

#include "pkg/noun/v2/hashtable.h"

/* _cn_prog_free(): free memory retained by program pog_u
**              NB: copy of _cn_prog_free in pkg/noun/nock.c
*/
static void
_cn_prog_free(u3n_prog* pog_u)
{
  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3z(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3z(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_free(pog_u);
}


/* _n_feb(): u3h_walk helper for u3n_free
 */
static void
_n_feb(u3_noun kev)
{
  _cn_prog_free(u3to(u3n_prog, u3t(kev)));
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

/* u3n_v2_rewrite_compact(): rewrite the bytecode cache for compaction.
 *
 * NB: u3R->byc.har_p *must* be cleared (currently via u3n_v2_reclaim above),
 * since it contains things that look like nouns but aren't.
 * Specifically, it contains "cells" where the tail is a
 * pointer to a u3a_malloc'ed block that contains loom pointers.
 *
 * You should be able to walk this with u3h_walk and rewrite the
 * pointers, but you need to be careful to handle that u3a_malloc
 * pointers can't be turned into a box by stepping back two words. You
 * must step back one word to get the padding, step then step back that
 * many more words (plus one?).
 */
void
u3n_v2_rewrite_compact()
{
  u3h_v2_rewrite(u3R->byc.har_p);
  u3R->byc.har_p = u3a_rewritten(u3R->byc.har_p);
}
