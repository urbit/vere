/// @file

#include "pkg/noun/allocate.h"
#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v2/allocate.h"

#include "pkg/noun/v2/hashtable.h"
#include "log.h"
#include "pkg/noun/v2/manage.h"
#include "options.h"
#include "retrieve.h"
#include "trace.h"
#include "vortex.h"

u3a_v2_road* u3a_v2_Road;

u3_noun
u3a_v2_rewritten_noun(u3_noun som)
{
  if ( c3y == u3a_v2_is_cat(som) ) {
    return som;
  }
  u3_post som_p = u3a_v2_rewritten(u3a_v1_to_off(som));

  if ( c3y == u3a_v2_is_pug(som) ) {
    som_p = u3a_v2_to_pug(som_p);
  }
  else {
    som_p = u3a_v2_to_pom(som_p);
  }

  return som_p;
}

/* u3a_v2_mig_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
*/
void
u3a_v2_mig_rewrite_compact(void)
{
  u3a_v2_rewrite_noun(u3R_v2->ski.gul);
  u3a_v2_rewrite_noun(u3R_v2->bug.tax);
  u3a_v2_rewrite_noun(u3R_v2->bug.mer);
  u3a_v2_rewrite_noun(u3R_v2->pro.don);
  u3a_v2_rewrite_noun(u3R_v2->pro.day);
  u3a_v2_rewrite_noun(u3R_v2->pro.trace);
  u3h_v2_rewrite(u3R_v2->cax.har_p);

  u3R_v2->ski.gul = u3a_v2_rewritten_noun(u3R_v2->ski.gul);
  u3R_v2->bug.tax = u3a_v2_rewritten_noun(u3R_v2->bug.tax);
  u3R_v2->bug.mer = u3a_v2_rewritten_noun(u3R_v2->bug.mer);
  u3R_v2->pro.don = u3a_v2_rewritten_noun(u3R_v2->pro.don);
  u3R_v2->pro.day = u3a_v2_rewritten_noun(u3R_v2->pro.day);
  u3R_v2->pro.trace = u3a_v2_rewritten_noun(u3R_v2->pro.trace);
  u3R_v2->cax.har_p = u3a_v2_rewritten(u3R_v2->cax.har_p);
}

void
u3a_v2_rewrite_noun(u3_noun som)
{
  if ( c3n == u3a_v2_is_cell(som) ) {
    return;
  }

  if ( c3n == u3a_v2_rewrite_ptr(u3a_v1_to_ptr((som))) ) return;

  u3a_v2_cell* cel = (u3a_v2_cell*) u3a_v1_to_ptr(som);

  u3a_v2_rewrite_noun(cel->hed);
  u3a_v2_rewrite_noun(cel->tel);

  cel->hed = u3a_v2_rewritten_noun(cel->hed);
  cel->tel = u3a_v2_rewritten_noun(cel->tel);
}
