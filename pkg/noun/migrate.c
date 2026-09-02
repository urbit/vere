#include "allocate.h"
#include "migrate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "options.h"
#include "retrieve.h"
#include "vortex.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* Bit-pinned home/road globals.  Only the off-bitness slot is populated
** during a migration; the matching-bitness slot is unused (the native
** loom has u3H/u3R).
*/
u3v_home_h* u3v_Home_h;
u3v_home_d* u3v_Home_d;
u3a_road_h* u3a_Road_h;
u3a_road_d* u3a_Road_d;

/* u3_load_h(): locate u3v_home_h in the mapped 32-bit image.
*/
void
u3_load_h(c3_z wor_i)
{
  (void)wor_i;
  u3H_h = (u3v_home_h *)u3_Loom_h;
  u3R_h = &u3H_h->rod_u;
}

/* u3_load_d(): locate u3v_home_d in the mapped 64-bit image.
*/
void
u3_load_d(c3_z wor_i)
{
  (void)wor_i;
  u3H_d = (u3v_home_d *)u3_Loom_d;
  u3R_d = &u3H_d->rod_u;
}

#ifdef VERE64

/*  32 → 64 migration: 64-bit vere reading a 32-bit snapshot.
*/

#define U3C_PREFIX        32
#define U3C_OLD_NOUN      u3_noun_h
#define U3C_OLD_ATOM_T    u3a_atom_h
#define U3C_OLD_IS_CAT    u3a_is_cat_h
#define U3C_OLD_IS_CELL   u3a_is_cell_h
#define U3C_OLD_TO_PTR    u3a_to_ptr_h
#define U3C_OLD_HEAD      u3a_head_h
#define U3C_OLD_TAIL      u3a_tail_h
#define U3C_ATOM_MODE     U3C_ATOM_32_TO_64
#define U3C_NEW_I_CELL    u3i_cell
#define U3C_NEW_H_PUT     u3h_put
#define U3C_NEW_A_GAIN    u3a_gain
#define U3C_NEW_A_LOSE    u3a_lose
#define U3C_NEW_A_WALLOC  u3a_walloc
#define U3C_NEW_A_TO_PUG  u3a_to_pug
#define U3C_NEW_A_OUTA    u3a_outa
#define U3C_NEW_ATOM_T    u3a_atom
#include "copy_migrate.h"

void
u3_migrate_d(c3_d eve_d)
{
  _copy_32_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_load_h(u3C.wor_i);

  if ( eve_d != u3A_h->eve_d ) {
    fprintf(stderr, "loom: migrate (to 64-bit) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_h->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: 32->64 migration running...\r\n");

  _copy_32_init(&cop_u);

  u3A->eve_d = u3A_h->eve_d;
  u3A->roc   = _copy_32_noun(&cop_u, u3A_h->roc);

  cop_u.ham_p = u3R->jed.cod_p;
  u3h_walk_with_h(u3R_h->jed.cod_p, _copy_32_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.per_p;
  u3h_walk_with_h(u3R_h->cax.per_p, _copy_32_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.for_p;
  u3h_walk_with_h(u3R_h->cax.for_p, _copy_32_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_boot(c3y);
  u3j_ream();

  _copy_32_done(&cop_u);

  fprintf(stderr, "loom: 32->64 migration done\r\n");
}

#else /* !VERE64 */

/*  64 → 32 migration: 32-bit vere reading a 64-bit snapshot.
*/

/* _copy_64_cat(): map a 64-bit source cat to a native 32-bit noun.
**
**   64-bit cats span 63 bits; native 32-bit cats only 31.  Values that
**   overflow 31 bits become native indirect atoms with mug_w=0 (the
**   snapshot does not precompute mugs for cats).
*/
static u3_noun
_copy_64_cat(u3_noun_d old)
{
  if ( old <= u3a_direct_max_h ) {
    return (u3_noun)old;
  }

  c3_w lo_w  = (c3_w)(old & 0xFFFFFFFFULL);
  c3_w hi_w  = (c3_w)(old >> 32);
  c3_w len_w = hi_w ? 2 : 1;

  c3_w     *nov_w = u3a_walloc(len_w + c3_wiseof(u3a_atom));
  u3a_atom *vat_u = (void *)nov_w;

  vat_u->use_w    = 1;
  vat_u->mug_w    = 0;
  vat_u->len_w    = len_w;
  vat_u->buf_w[0] = lo_w;
  if ( len_w > 1 ) vat_u->buf_w[1] = hi_w;

  return u3a_to_pug(u3a_outa(nov_w));
}

#define U3C_PREFIX        64
#define U3C_OLD_NOUN      u3_noun_d
#define U3C_OLD_ATOM_T    u3a_atom_d
#define U3C_OLD_IS_CAT    u3a_is_cat_d
#define U3C_OLD_IS_CELL   u3a_is_cell_d
#define U3C_OLD_TO_PTR    u3a_to_ptr_d
#define U3C_OLD_HEAD      u3a_head_d
#define U3C_OLD_TAIL      u3a_tail_d
#define U3C_ATOM_MODE     U3C_ATOM_64_TO_32
#define U3C_COPY_CAT(old) _copy_64_cat(old)
#define U3C_NEW_I_CELL    u3i_cell
#define U3C_NEW_H_PUT     u3h_put
#define U3C_NEW_A_GAIN    u3a_gain
#define U3C_NEW_A_LOSE    u3a_lose
#define U3C_NEW_A_WALLOC  u3a_walloc
#define U3C_NEW_A_TO_PUG  u3a_to_pug
#define U3C_NEW_A_OUTA    u3a_outa
#define U3C_NEW_ATOM_T    u3a_atom
#include "copy_migrate.h"

void
u3_migrate_h(c3_d eve_d)
{
  _copy_64_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_load_d(0);

  if ( eve_d != u3A_d->eve_d ) {
    fprintf(stderr, "loom: migrate (to 32-bit) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_d->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: 64->32 migration running...\r\n");

  _copy_64_init(&cop_u);

  u3A->eve_d = u3A_d->eve_d;
  u3A->roc   = _copy_64_noun(&cop_u, u3A_d->roc);

  cop_u.ham_p = u3R->jed.cod_p;
  u3h_walk_with_d(u3R_d->jed.cod_p, _copy_64_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.per_p;
  u3h_walk_with_d(u3R_d->cax.per_p, _copy_64_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.for_p;
  u3h_walk_with_d(u3R_d->cax.for_p, _copy_64_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_boot(c3y);
  u3j_ream();

  _copy_64_done(&cop_u);

  fprintf(stderr, "loom: 64->32 migration done\r\n");
}

#endif /* !VERE64 */
