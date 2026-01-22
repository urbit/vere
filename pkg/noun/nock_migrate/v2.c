/// @file

#include "v2.h"

#include "allocate.h"

static void
_cn_prog_free(u3n_nv2_prog* pog_u)
{
  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3z_nv2(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3z_nv2(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_nv2_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_nv2_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_nv2_free(pog_u);
}

static inline u3n_nv2_prog*
_cn_to_prog(c3_w pog_w)
{
  u3_post pog_p = pog_w << u3a_vits;
  return u3to_nv2(u3n_nv2_prog, pog_p);
}

static void
_n_feb(u3_nv2_noun kev)
{
  _cn_prog_free(_cn_to_prog(u3t_nv2(kev)));
}

void
u3n_nv2_free()
{
  u3h_nv2_walk(u3R->byc.har_p, _n_feb);
  u3h_nv2_free(u3R->byc.har_p);
}

void
u3n_nv2_reclaim(void)
{
  u3n_nv2_free();
  u3R->byc.har_p = u3h_nv2_new();
}

static void*
_n_prog_dat_nv2(u3n_nv2_prog* pog_u)
{
  return ((void*) pog_u) + sizeof(u3n_nv2_prog);
}

static void
_n_ream(u3_nv2_noun kev)
{
  u3n_nv2_prog* pog_u = _cn_to_prog(u3t_nv2(kev));

  c3_w pad_w = (8 - pog_u->byc_u.len_w % 8) % 8;
  c3_w pod_w = pog_u->lit_u.len_w % 2;
  c3_w ped_w = pog_u->mem_u.len_w % 2;
  // fix up pointers for loom portability
  pog_u->byc_u.ops_y = (c3_y*) _n_prog_dat_nv2(pog_u);
  pog_u->lit_u.non   = (u3_nv2_noun*) (pog_u->byc_u.ops_y + pog_u->byc_u.len_w + pad_w);
  pog_u->mem_u.sot_u = (u3n_nv2_memo*) (pog_u->lit_u.non + pog_u->lit_u.len_w + pod_w);
  pog_u->cal_u.sit_u = (u3j_nv2_site*) (pog_u->mem_u.sot_u + pog_u->mem_u.len_w + ped_w);
  pog_u->reg_u.rit_u = (u3j_nv2_rite*) (pog_u->cal_u.sit_u + pog_u->cal_u.len_w);

  for ( c3_w i_w = 0; i_w < pog_u->cal_u.len_w; ++i_w ) {
    u3j_nv2_site_ream(&(pog_u->cal_u.sit_u[i_w]));
  }
}


void
u3n_nv2_ream(void)
{
  u3_assert(u3R == &(u3H->rod_u));
  u3h_nv2_walk(u3R->byc.har_p, _n_ream);
}
