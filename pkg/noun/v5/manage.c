/// @file

#include "v5/manage.h"

#include "stdio.h"
#include "jets.h"
#include "nock.h"
#include "vortex.h"
#include "options.h"

/* u3m_v5_migrate: perform loom migration if necessary.
*/
void
u3m_v5_migrate(void)
{
  // Update to the bytecode compiler and the interpreter made
  // Nock bytecode caches stale
  //
  fprintf(stderr, "loom: freeing stale bytecode...\r\n");

  c3_w* mem_w = u3_Loom + u3a_walign;
  c3_w  siz_w = c3_wiseof(u3v_home);
  c3_w  len_w = u3C.wor_i - u3a_walign;
  c3_w* mat_w = c3_align(mem_w + len_w - siz_w, u3a_balign, C3_ALGLO);

  u3H = (void *)mat_w;
  u3R = &u3H->rod_u;

  u3j_reclaim();
  u3n_reclaim();

  u3H->ver_w = U3V_VER5;

  fprintf(stderr, "loom: freeing stale bytecode done\r\n");

}
