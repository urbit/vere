#include "v3.h"
#include "v4.h"
#include "options.h"

#     define  u3m_v3_reclaim  u3m_v4_reclaim

/* u3_migrate_v4: perform loom migration if necessary.
*/
void
u3_migrate_v4(void)
{
  u3_v3_load(u3C.wor_i);

  fprintf(stderr, "loom: bytecode alignment migration running...\r\n");

  u3m_v3_reclaim();

  u3H_v4 = u3H_v3;
  u3H_v4->ver_w = U3V_VER4;

  fprintf(stderr, "loom: bytecode alignment migration done\r\n");
}
