#include "v3.h"
#include "v4.h"
#include "options.h"

#     define  u3m_v3_reclaim  u3m_v4_reclaim

/* u3_migrate_v4: perform loom migration if necessary.
*/
void
u3_migrate_v4(c3_d eve_d)
{
  u3_v3_load(u3C.wor_i);

  if ( eve_d != u3H_v3->arv_u.eve_d ) {
    fprintf(stderr, "loom: migrate (v4) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3H_v3->arv_u.eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: bytecode alignment migration running...\r\n");

  u3m_v3_reclaim();

  u3H_v4 = u3H_v3;
  u3H_v4->ver_w = U3V_VER4;

  fprintf(stderr, "loom: bytecode alignment migration done\r\n");
}
