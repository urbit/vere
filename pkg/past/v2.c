#include "v2.h"

#     define  u3h_v2_free  u3h_v3_free
#     define  u3h_v2_walk  u3h_v3_walk
#     define  u3h_v2_new   u3h_v3_new

u3a_v2_road* u3a_v2_Road;
u3v_v2_home* u3v_v2_Home;


/***  jets.c
***/

/* u3j_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3j_v2_reclaim(void)
{
  //  set globals (required for aliased functions)
  //  XX confirm
  u3H_v3 = (u3v_v3_home*) u3H_v2;
  u3R_v3 = (u3a_v3_road*) u3R_v2;

  //  clear the jet hank cache
  //
  u3h_v2_walk(u3R_v2->jed.han_p, u3j_v2_free_hank);
  u3h_v2_free(u3R_v2->jed.han_p);
  u3R_v2->jed.han_p = u3h_v2_new();
}


/***  nock.c
***/

/* _cn_v2_prog_free(): free memory retained by program pog_u
*/
static void
_cn_v2_prog_free(u3n_v2_prog* pog_u)
{
  // fix up pointers for loom portability
  pog_u->byc_u.ops_y = (c3_y*) ((void*) pog_u) + sizeof(u3n_v2_prog);
  pog_u->lit_u.non   = (u3_noun*) (pog_u->byc_u.ops_y + pog_u->byc_u.len_w);
  pog_u->mem_u.sot_u = (u3n_v2_memo*) (pog_u->lit_u.non + pog_u->lit_u.len_w);
  pog_u->cal_u.sit_u = (u3j_v2_site*) (pog_u->mem_u.sot_u + pog_u->mem_u.len_w);
  pog_u->reg_u.rit_u = (u3j_v2_rite*) (pog_u->cal_u.sit_u + pog_u->cal_u.len_w);

  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3a_v2_lose(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3a_v2_lose(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_v2_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_v2_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_v2_free(pog_u);
}

/* _n_v2_feb(): u3h_v2_walk helper for u3n_v2_free
 */
static void
_n_v2_feb(u3_noun kev)
{
  u3a_v2_cell *cel_u = (u3a_v2_cell*) u3a_v2_to_ptr(kev);
  _cn_v2_prog_free(u3v2to(u3n_v2_prog, cel_u->tel));
}

/* u3n_v2_free(): free bytecode cache
 */
void
u3n_v2_free(void)
{
  u3p(u3h_v2_root) har_p = u3R_v2->byc.har_p;
  u3h_v2_walk(har_p, _n_v2_feb);
  u3h_v2_free(har_p);
}

/* u3n_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3n_v2_reclaim(void)
{
  //  set globals (required for aliased functions)
  u3H_v3 = (u3v_v3_home*) u3H_v2;
  u3R_v3 = (u3a_v3_road*) u3R_v2;

  //  clear the bytecode cache
  u3n_v2_free();
  u3R_v2->byc.har_p = u3h_v2_new();
}


/***  init
***/

void
u3_v2_load(c3_z wor_i)
{
  c3_w ver_w = *(u3_Loom_v2 + wor_i - 1);

  u3_assert( U3V_VER2 == ver_w );

  c3_w *mem_w = u3_Loom_v2 + u3a_v2_walign;
  c3_w  len_w = wor_i - u3a_v2_walign;
  c3_w  suz_w = c3_wiseof(u3v_v2_home);
  c3_w *mut_w = c3_align(mem_w + len_w - suz_w, u3a_v2_balign, C3_ALGLO);

  //  set v2 globals
  u3H_v2 = (void *)mut_w;
  u3R_v2 = &u3H_v2->rod_u;
  u3R_v2->cap_p = u3R_v2->mat_p = u3a_v2_outa(u3H_v2);
}
