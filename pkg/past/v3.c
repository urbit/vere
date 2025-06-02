#include "v3.h"

#include "vortex.h"

/* u3h_v3_new_cache(): create hashtable with bounded size.
*/
u3p(u3h_v3_root)
u3h_v3_new_cache(c3_w max_w)
{
  //  set globals (required for aliased functions)
  u3H = (u3v_home*) u3H_v3;
  u3R = (u3a_road*) u3R_v3;

  u3h_v3_root*     har_u = u3a_v3_walloc(c3_wiseof(u3h_v3_root));
  u3p(u3h_v3_root) har_p = u3v3of(u3h_v3_root, har_u);
  c3_w        i_w;

  har_u->max_w       = max_w;
  har_u->use_w       = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    har_u->sot_w[i_w] = 0;
  }
  return har_p;
}


/***  init
***/

void
u3_v3_load(c3_z wor_i)
{
  c3_w ver_w = *(u3_Loom + wor_i - 1);

  u3_assert( U3V_VER3 == ver_w );

  c3_w* mem_w = u3_Loom + u3a_v3_walign;
  c3_w  siz_w = c3_wiseof(u3v_v3_home);
  c3_w  len_w = wor_i - u3a_v3_walign;
  c3_w* mat_w = c3_align(mem_w + len_w - siz_w, u3a_v3_balign, C3_ALGLO);

  u3H_v3 = (void *)mat_w;
  u3R_v3 = &u3H_v3->rod_u;

  u3R_v3->cap_p = u3R_v3->mat_p = u3a_v3_outa(u3H_v3);
}
