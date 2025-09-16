#include "v3.h"

/***  init
***/

void
u3_v3_load(c3_z wor_i)
{
  c3_w ver_w = *(u3_Loom_v3 + wor_i - 1);

  u3_assert( U3V_VER3 == ver_w );

  c3_w* mem_w = u3_Loom_v3 + u3a_v3_walign;
  c3_w  siz_w = c3_wiseof(u3v_v3_home);
  c3_w  len_w = wor_i - u3a_v3_walign;
  c3_w* mat_w = c3_align(mem_w + len_w - siz_w, u3a_v3_balign, C3_ALGLO);

  u3H_v3 = (void *)mat_w;
  u3R_v3 = &u3H_v3->rod_u;

  u3R_v3->cap_p = u3R_v3->mat_p = u3a_v3_outa(u3H_v3);
}
