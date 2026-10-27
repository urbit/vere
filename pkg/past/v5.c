#include "v5.h"
#include "options.h"

/*
**   The version 5 home road: the native layout up to the memoization
**   caches, with the SKA state of version 6 after them left zero.
*/

/* u3m_v5_pave(): instantiate a pristine version 5 home road, as 4.0
**                paved it: the allocator, the jet dashboard, the bytecode
**                and memoization caches, the deadline list and the
**                deduplication set.
*/
void
u3m_v5_pave(void)
{
  u3_post top_p = u3C.wor_i - u3a_walign;
  u3_post bot_p = ((c3_w)1) << u3a_page;

  u3H_v5 = u3to(u3v_v5_home, 0);
  memset(u3H_v5, 0, (c3_z)bot_p * sizeof(c3_w));
  u3H_v5->ver_d = U3V_VER5;

  //  pam_d bits: { word-size[1], virtual-bits[2], page-size[3], bytecode[5] }
  //
  u3H_v5->pam_d = 0
                ^ ((8 == sizeof(c3_w)) ? 1 : 0)
                ^ (u3a_vits << 1)
                ^ ((u3a_page + 2 - 12) << 3)
                ^ (U3N_VERLAT << 6);

  u3R_v5 = &u3H_v5->rod_u;
  u3R_v5->rut_p = u3R_v5->hat_p = bot_p;
  u3R_v5->mat_p = u3R_v5->cap_p = top_p;

  u3a_v5_init_heap();

  u3R_v5->cax.har_p = u3h_v5_new_cache(u3C.hap_w);  //  transient
  u3R_v5->cax.per_p = u3h_v5_new_cache(u3C.per_w);  //  persistent
  u3R_v5->cax.for_p = u3h_v5_new_cache(u3C.per_w);  //  ford
  u3R_v5->jed.war_p = u3h_v5_new();
  u3R_v5->jed.cod_p = u3h_v5_new();
  u3R_v5->jed.han_p = u3h_v5_new();
  u3R_v5->jed.bas_p = u3h_v5_new();
  u3R_v5->byc.har_p = u3h_v5_new();
  u3R_v5->lop_p     = u3h_v5_new();
  u3R_v5->tim       = u3_nul;
  u3R_v5->dup_p     = u3h_v5_new();
  u3R_v5->how.fag_w = 0;
}
