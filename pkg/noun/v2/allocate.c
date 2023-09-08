/// @file

#include "pkg/noun/allocate.h"
#include "pkg/noun/v2/allocate.h"

#include "pkg/noun/v2/hashtable.h"
#include "log.h"
#include "pkg/noun/v2/manage.h"
#include "options.h"
#include "retrieve.h"
#include "trace.h"
#include "vortex.h"

/* u3a_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3a_v2_reclaim(void)
{
  //  clear the memoization cache
  //
  u3h_free(u3R->cax.har_p);
  u3R->cax.har_p = u3h_new();
}

u3_noun
u3a_v2_rewritten_noun(u3_noun som)
{
  if ( c3y == u3a_is_cat(som) ) {
    return som;
  }
  u3_post som_p = u3a_rewritten(u3a_v2_to_off(som));

  /* If this is being called during a migration, one-bit pointer compression
     needs to be temporarily enabled so the rewritten reference is compressed */
  if (u3C.migration_state == MIG_REWRITE_COMPRESSED)
    u3C.vits_w = 1;

  if ( c3y == u3a_is_pug(som) ) {
    som_p = u3a_v2_to_pug(som_p);
  }
  else {
    som_p = u3a_v2_to_pom(som_p);
  }

  /* likewise, pointer compression is disabled until migration is complete */
  if (u3C.migration_state == MIG_REWRITE_COMPRESSED)
    u3C.vits_w = 0;

  return som_p;
}

/* u3a_v2_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
*/
void
u3a_v2_rewrite_compact(void)
{
  u3a_v2_rewrite_noun(u3R->ski.gul);
  u3a_v2_rewrite_noun(u3R->bug.tax);
  u3a_v2_rewrite_noun(u3R->bug.mer);
  u3a_v2_rewrite_noun(u3R->pro.don);
  u3a_v2_rewrite_noun(u3R->pro.day);
  u3a_v2_rewrite_noun(u3R->pro.trace);
  u3h_v2_rewrite(u3R->cax.har_p);

  u3R->ski.gul = u3a_v2_rewritten_noun(u3R->ski.gul);
  u3R->bug.tax = u3a_v2_rewritten_noun(u3R->bug.tax);
  u3R->bug.mer = u3a_v2_rewritten_noun(u3R->bug.mer);
  u3R->pro.don = u3a_v2_rewritten_noun(u3R->pro.don);
  u3R->pro.day = u3a_v2_rewritten_noun(u3R->pro.day);
  u3R->pro.trace = u3a_rewritten_noun(u3R->pro.trace);
  u3R->cax.har_p = u3a_rewritten(u3R->cax.har_p);
}

void
u3a_v2_rewrite_noun(u3_noun som)
{
  if ( c3n == u3a_is_cell(som) ) {
    return;
  }

  if ( c3n == u3a_rewrite_ptr(u3a_v2_to_ptr((som))) ) return;

  u3a_cell* cel = u3a_v2_to_ptr(som);

  u3a_v2_rewrite_noun(cel->hed);
  u3a_v2_rewrite_noun(cel->tel);

  cel->hed = u3a_v2_rewritten_noun(cel->hed);
  cel->tel = u3a_v2_rewritten_noun(cel->tel);
}