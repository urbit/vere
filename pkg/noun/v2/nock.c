/// @file

#include "pkg/noun/v2/nock.h"

#include "pkg/noun/v2/allocate.h"
#include "pkg/noun/v2/hashtable.h"

/* u3n_v2_rewrite_compact(): rewrite the bytecode cache for compaction.
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
u3n_v2_rewrite_compact()
{
  u3h_v2_rewrite(u3R_v2->byc.har_p);
  u3R_v2->byc.har_p = u3a_v2_rewritten(u3R_v2->byc.har_p);
}
