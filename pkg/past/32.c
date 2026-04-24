#include "32.h"
#include "options.h"

#ifndef VERE64

/* global definitions for 64-bit source home/road.
*/
u3v_64_home* u3v_64_Home;
u3a_64_road* u3a_64_Road;

/* u3_64_load(): locate u3v_64_home in the mapped 64-bit image.
*/
void
u3_64_load(c3_z wor_i)
{
  (void)wor_i;
  u3H_64 = (u3v_64_home *)u3_Loom_64;
  u3R_64 = &u3H_64->rod_u;
}

/* _ch_64_walk_buck(): walk 64-bit HAMT bucket.
*/
static void
_ch_64_walk_buck(u3h_64_buck* hab_u, void (*fun_f)(u3_64_noun, void*), void* wit)
{
  c3_h i_h;

  for ( i_h = 0; i_h < hab_u->len_h; i_h++ ) {
    fun_f(u3h_64_slot_to_noun(hab_u->sot_w[i_h]), wit);
  }
}

/* _ch_64_walk_node(): walk 64-bit HAMT node.
*/
static void
_ch_64_walk_node(u3h_64_node* han_u, c3_w lef_w, void (*fun_f)(u3_64_noun, void*), void* wit)
{
  c3_w len_w = c3_pc_w(han_u->map_h);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    u3h_64_slot sot_w = han_u->sot_w[i_w];

    if ( _(u3h_64_slot_is_noun(sot_w)) ) {
      fun_f(u3h_64_slot_to_noun(sot_w), wit);
    }
    else if ( _(u3h_64_slot_is_node(sot_w)) ) {
      void* hav_v = u3h_64_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_64_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_64_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_64_walk_with(): traverse 64-bit HAMT calling fun_f(kev, wit) on each entry.
*/
void
u3h_64_walk_with(c3_64_w har_p,
                 void (*fun_f)(u3_64_noun, void*),
                 void* wit)
{
  u3h_64_root* har_u = (u3h_64_root *)u3a_64_into(har_p);
  c3_w i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    u3h_64_slot sot_w = har_u->sot_w[i_w];

    if ( _(u3h_64_slot_is_noun(sot_w)) ) {
      fun_f(u3h_64_slot_to_noun(sot_w), wit);
    }
    else if ( _(u3h_64_slot_is_node(sot_w)) ) {
      u3h_64_node* han_u = (u3h_64_node *)u3h_64_slot_to_node(sot_w);

      _ch_64_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

/* _copy_64_cat(): map a 64-bit source cat to a native 32-bit noun.
**
**   64-bit cats span 63 bits; native 32-bit cats only 31.  Values that
**   overflow 31 bits become native indirect atoms with mug_w=0 (the
**   snapshot does not precompute mugs for cats).
*/
static u3_noun
_copy_64_cat(u3_64_noun old)
{
  if ( old <= u3a_32_direct_max ) {
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
#define U3C_OLD_NOUN      u3_64_noun
#define U3C_OLD_ATOM_T    u3a_64_atom
#define U3C_OLD_IS_CAT    u3a_64_is_cat
#define U3C_OLD_IS_CELL   u3a_64_is_cell
#define U3C_OLD_TO_PTR    u3a_64_to_ptr
#define U3C_OLD_HEAD      u3a_64_head
#define U3C_OLD_TAIL      u3a_64_tail
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
u3_migrate_32(c3_d eve_d)
{
  _copy_64_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_64_load(0);

  if ( eve_d != u3A_64->eve_d ) {
    fprintf(stderr, "loom: migrate (to 32-bit) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_64->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: 64->32 migration running...\r\n");

  _copy_64_init(&cop_u);

  u3A->eve_d = u3A_64->eve_d;
  u3A->roc   = _copy_64_noun(&cop_u, u3A_64->roc);

  cop_u.ham_p = u3R->jed.cod_p;
  u3h_64_walk_with(u3R_64->jed.cod_p, _copy_64_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.per_p;
  u3h_64_walk_with(u3R_64->cax.per_p, _copy_64_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.for_p;
  u3h_64_walk_with(u3R_64->cax.for_p, _copy_64_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_boot(c3y);
  u3j_ream();

  _copy_64_done(&cop_u);

  fprintf(stderr, "loom: 64->32 migration done\r\n");
}

#endif /* !VERE64 */
