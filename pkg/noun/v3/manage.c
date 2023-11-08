/// @file

#include "pkg/noun/v3/manage.h"

#include "pkg/noun/v2/jets.h"
#include "pkg/noun/v2/nock.h"
#include "pkg/noun/v2/vortex.h"

#include "pkg/noun/v3/allocate.h"
#include "pkg/noun/v3/hashtable.h"
#include "pkg/noun/version.h"
#include "pkg/noun/v3/vortex.h"
#include <v2/allocate.h>

/* u3m_v3_migrate: perform loom migration if necessary.
*/
void
u3m_v3_migrate()
{
  fprintf(stderr, "loom: memoization migration running...\r\n");


  c3_w ver_w = *(u3_Loom + u3C.wor_i - 1);
  c3_w *mem_w = u3_Loom + u3a_v3_walign;
  c3_w  len_w = u3C.wor_i - u3a_v3_walign;
  c3_w  suz_w = c3_wiseof(u3v_v2_home);
  c3_w *mut_w = c3_align(mem_w + len_w - suz_w, u3a_v3_balign, C3_ALGLO);

  //  old road
  u3v_v2_home* hum_u = (u3v_v2_home*)mut_w;
  u3a_v2_road* rud_u = &hum_u->rod_u;
  size_t ruz_t = sizeof(u3a_v2_road);

  //  set v2 globals
  u3H_v2 = (void *)mut_w;
  u3R_v2 = &u3H_v2->rod_u;
  u3R_v2->cap_p = u3R_v2->mat_p = u3a_v2_outa(u3H_v2);

  //  free bytecode caches in old road
  u3j_v2_reclaim();
  u3n_v2_reclaim();

  //  new home, new road
  u3v_v3_home hom_u = {0};
  u3a_v3_road rod_u = {0};

  //  copy members, one-by-one, from old road to new road
  rod_u.par_p = rud_u->par_p;
  rod_u.kid_p = rud_u->kid_p;
  rod_u.nex_p = rud_u->nex_p;

  rod_u.cap_p = rud_u->cap_p;
  rod_u.hat_p = rud_u->hat_p;
  rod_u.mat_p = rud_u->mat_p;
  rod_u.rut_p = rud_u->rut_p;
  rod_u.ear_p = rud_u->ear_p;

  //  no need to zero-out fut_w
  //  no need to do anything with esc

  rod_u.how.fag_w = rud_u->how.fag_w;

  memcpy(rod_u.all.fre_p, rud_u->all.fre_p, sizeof(rud_u->all.fre_p));
  rod_u.all.cel_p = rud_u->all.cel_p;
  rod_u.all.fre_w = rud_u->all.fre_w;
  rod_u.all.max_w = rud_u->all.max_w;

  rod_u.jed.hot_p = rud_u->jed.hot_p;
  rod_u.jed.war_p = rud_u->jed.war_p;
  rod_u.jed.cod_p = rud_u->jed.cod_p;
  rod_u.jed.han_p = rud_u->jed.han_p;
  rod_u.jed.bas_p = rud_u->jed.bas_p;

  rod_u.byc.har_p = rud_u->byc.har_p;

  rod_u.ski.gul = rud_u->ski.gul;

  rod_u.bug.tax = rud_u->bug.tax;
  rod_u.bug.mer = rud_u->bug.mer;

  rod_u.pro.nox_d = rud_u->pro.nox_d;
  rod_u.pro.cel_d = rud_u->pro.cel_d;
  rod_u.pro.don   = rud_u->pro.don;
  rod_u.pro.trace = rud_u->pro.trace;
  rod_u.pro.day   = rud_u->pro.day;

  rod_u.cax.har_p = rud_u->cax.har_p;

  //  prepare the new home, update the version
  hom_u.arv_u = hum_u->arv_u;
  hom_u.rod_u = rod_u;

  //  place the new home over the old one
  c3_w  siz_w = c3_wiseof(u3v_v3_home);
  c3_w *mat_w = c3_align(mem_w + len_w - siz_w, u3a_v3_balign, C3_ALGLO);
  memcpy(mat_w, &hom_u, sizeof(u3v_v3_home));

  //  set globals
  u3H_v3 = (void*)mat_w;
  u3R_v3 = &u3H_v3->rod_u;
  u3H_v3->ver_w = U3V_VER3;

  //  initialize persistent cache
  u3R_v3->cax.per_p = u3h_v3_new_cache(u3C.per_w);

  fprintf(stderr, "loom: memoization migration done\r\n");
}
