/// @file

#include "v4/manage.h"
#include "stdio.h"
#include "manage.h"
#include "allocate.h"
#include "vortex.h"
#include "options.h"

/* u3m_v4_migrate: perform loom migration if necessary.
*/
void
u3m_v4_migrate(void)
{
  fprintf(stderr, "loom: bytecode alignment migration running...\r\n");

  c3_w* mem_w = u3_Loom + u3a_walign;
  c3_w  siz_w = c3_wiseof(u3v_home);
  c3_w  len_w = u3C.wor_i - u3a_walign;
  c3_w* mat_w = c3_align(mem_w + len_w - siz_w, u3a_balign, C3_ALGLO);

  u3H = (void *)mat_w;
  u3R = &u3H->rod_u;

  u3m_reclaim();

  u3H->ver_w = U3V_VER4;

  fprintf(stderr, "loom: bytecode alignment migration done\r\n");
}
