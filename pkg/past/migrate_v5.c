#include "v4.h"
#include "options.h"

#define U3C_PREFIX        v4
#define U3C_OLD_NOUN      u3_v4_noun
#define U3C_OLD_ATOM_T    u3a_v4_atom
#define U3C_OLD_IS_CAT    u3a_v4_is_cat
#define U3C_OLD_IS_CELL   u3a_v4_is_cell
#define U3C_OLD_TO_PTR    u3a_v4_to_ptr
#define U3C_OLD_HEAD      u3a_v4_head
#define U3C_OLD_TAIL      u3a_v4_tail
#define U3C_ATOM_MODE     U3C_ATOM_SAME
#define U3C_NEW_I_CELL    u3i_v5_cell
#define U3C_NEW_H_PUT     u3h_v5_put
#define U3C_NEW_A_GAIN    u3a_v5_gain
#define U3C_NEW_A_LOSE    u3a_v5_lose
#define U3C_NEW_A_WALLOC  u3a_v5_walloc
#define U3C_NEW_A_TO_PUG  u3a_v5_to_pug
#define U3C_NEW_A_OUTA    u3a_v5_outa
#define U3C_NEW_ATOM_T    u3a_v5_atom
#include "copy_migrate.h"

void
u3_migrate_v5(c3_d eve_d)
{
  _copy_v4_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_v4_load(u3C.wor_i);

  if ( eve_d != u3A_v4->eve_d ) {
    fprintf(stderr, "loom: migrate (v5) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_v4->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: allocator migration running...\r\n");

  _copy_v4_init(&cop_u);

  //  XX install cel_p temporarily?

  u3A_v5->eve_d = u3A_v4->eve_d;
  u3A_v5->roc   = _copy_v4_noun(&cop_u, u3A_v4->roc);

  cop_u.ham_p = u3R_v5->jed.cod_p;
  u3h_v4_walk_with(u3R_v4->jed.cod_p, _copy_v4_hamt, &cop_u);
  cop_u.ham_p = u3R_v5->cax.per_p;
  u3h_v4_walk_with(u3R_v4->cax.per_p, _copy_v4_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_v5_boot(c3y);
  u3j_v5_ream();

  _copy_v4_done(&cop_u);

  fprintf(stderr, "loom: allocator migration done\r\n");
}
