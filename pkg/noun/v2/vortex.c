/// @file

#include "pkg/noun/vortex.h"
#include "pkg/noun/v2/vortex.h"

#include "pkg/noun/v2/allocate.h"

u3v_v2_home* u3v_v2_Home;

/* u3v_v2_mig_rewrite_compact(): rewrite arvo kernel for compaction.
*/
void
u3v_v2_mig_rewrite_compact()
{
  u3v_v2_arvo* arv_u = &(u3H_v2->arv_u);

  u3a_v2_rewrite_noun(arv_u->roc);
  u3a_v2_rewrite_noun(arv_u->now);
  u3a_v2_rewrite_noun(arv_u->yot);

  arv_u->roc = u3a_v2_rewritten_noun(arv_u->roc);
  arv_u->now = u3a_v2_rewritten_noun(arv_u->now);
  arv_u->yot = u3a_v2_rewritten_noun(arv_u->yot);
}

