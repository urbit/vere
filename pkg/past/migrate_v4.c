#include "v3.h"
#include "options.h"

/* u3_migrate_v4: perform loom migration if necessary.
*/
void
u3_migrate_v4(void)
{
  u3_v3_load(u3C.wor_i);

  fprintf(stderr, "loom: bytecode alignment migration running...\r\n");

  u3H = u3H_v3;
  u3R = u3R_v3;
  u3m_v3_reclaim();

  u3H->ver_w = U3V_VER4;

  fprintf(stderr, "loom: bytecode alignment migration done\r\n");
}
