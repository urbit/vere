/// @file

#include "pkg/noun/v3/hashtable.h"

#include "pkg/noun/allocate.h"
#include "pkg/noun/vortex.h"

#include "pkg/noun/v3/allocate.h"
#include "pkg/noun/v3/vortex.h"

/* u3h_v3_new_cache(): create hashtable with bounded size.
*/
u3p(u3h_v3_root)
u3h_v3_new_cache(c3_w max_w)
{
  //  set globals (required for aliased functions)
  u3H = (u3v_home*) u3H_v3;
  u3R = (u3a_road*) u3R_v3;

  u3h_v3_root*     har_u = u3a_v3_walloc(c3_wiseof(u3h_v3_root));
  u3p(u3h_v3_root) har_p = u3of(u3h_v3_root, har_u);
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
