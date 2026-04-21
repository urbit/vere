#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "64.h"
#include "options.h"

#ifdef VERE64

typedef enum {
  _ce_img_good_32 = 0,
  _ce_img_fail_32 = 1,
  _ce_img_size_32 = 2
} _ce_img_stat_32;

/* _ce_image_stat_32(): measure 32-bit image.
*/
static _ce_img_stat_32
_ce_image_stat_32(u3e_image* img_u, c3_w* pgs_w)
{
  struct stat buf_u;

  if ( -1 == fstat(img_u->fid_i, &buf_u) ) {
    fprintf(stderr, "loom: image stat: %s\r\n", strerror(errno));
    u3_assert(0);
    return _ce_img_fail_32;
  }
  else {
    c3_z siz_z = buf_u.st_size;
    c3_z pgs_z = (siz_z + (_ce_page_32 - 1))
                   >> (u3a_32_page + u3a_32_word_bytes_shift);

    if ( !siz_z ) {
      *pgs_w = 0;
      return _ce_img_good_32;
    }
    else if ( siz_z != _ce_len_32(pgs_z) ) {
      fprintf(stderr, "loom: image corrupt size %zu\r\n", siz_z);
      return _ce_img_size_32;
    }
    else if ( pgs_z > c3_h_max ) {
      fprintf(stderr, "loom: %s overflow %zu\r\n", img_u->nam_c, siz_z);
      return _ce_img_fail_32;
    }
    else {
      *pgs_w = (c3_w)pgs_z;
      return _ce_img_good_32;
    }
  }
}

/* _ce_image_open_32(): open 32-bit image file.
*/
static _ce_img_stat_32
_ce_image_open_32(u3e_image* img_u, c3_c* ful_c)
{
  c3_i mod_i = O_RDWR | O_CREAT;

  c3_c pax_c[8192];
  snprintf(pax_c, 8192, "%s/%s.bin", ful_c, img_u->nam_c);
  if ( -1 == (img_u->fid_i = c3_open(pax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", pax_c, strerror(errno));
    return _ce_img_fail_32;
  }

  return _ce_image_stat_32(img_u, &img_u->pgs_w);
}

c3_i
u3e_32_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z)
{
  u3e_image img_u = { .nam_c = nam_c };

  switch ( _ce_image_open_32(&img_u, dir_c) ) {
    case _ce_img_good_32: {
      *len_z = _ce_len_32(img_u.pgs_w);
      return img_u.fid_i;
    } break;

    case _ce_img_fail_32:
    case _ce_img_size_32: {
      *len_z = 0;
      return -1;
    } break;
  }
}

/* global definitions for 32-bit source home/road.
*/
u3v_32_home* u3v_32_Home;
u3a_32_road* u3a_32_Road;

/* u3_32_load(): locate u3v_32_home in the mapped 32-bit image.
*/
void
u3_32_load(c3_z wor_i)
{
  (void)wor_i;
  u3H_32 = (u3v_32_home *)u3_Loom_32;
  u3R_32 = &u3H_32->rod_u;
}

/* _ch_32_walk_buck(): walk 32-bit HAMT bucket.
*/
static void
_ch_32_walk_buck(u3h_32_buck* hab_u, void (*fun_f)(u3_32_noun, void*), void* wit)
{
  c3_h i_h;

  for ( i_h = 0; i_h < hab_u->len_h; i_h++ ) {
    fun_f(u3h_32_slot_to_noun(hab_u->sot_w[i_h]), wit);
  }
}

/* _ch_32_walk_node(): walk 32-bit HAMT node.
*/
static void
_ch_32_walk_node(u3h_32_node* han_u, c3_w lef_w, void (*fun_f)(u3_32_noun, void*), void* wit)
{
  c3_w len_w = c3_pc_w(han_u->map_h);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    u3h_32_slot sot_w = han_u->sot_w[i_w];

    if ( _(u3h_32_slot_is_noun(sot_w)) ) {
      fun_f(u3h_32_slot_to_noun(sot_w), wit);
    }
    else if ( _(u3h_32_slot_is_node(sot_w)) ) {
      void* hav_v = u3h_32_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_32_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_32_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_32_walk_with(): traverse 32-bit HAMT calling fun_f(kev, wit) on each entry.
*/
void
u3h_32_walk_with(c3_32_w har_p,
                 void (*fun_f)(u3_32_noun, void*),
                 void* wit)
{
  u3h_32_root* har_u = (u3h_32_root *)u3a_32_into(har_p);
  c3_w i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    u3h_32_slot sot_w = har_u->sot_w[i_w];

    if ( _(u3h_32_slot_is_noun(sot_w)) ) {
      fun_f(u3h_32_slot_to_noun(sot_w), wit);
    }
    else if ( _(u3h_32_slot_is_node(sot_w)) ) {
      u3h_32_node* han_u = (u3h_32_node *)u3h_32_slot_to_node(sot_w);

      _ch_32_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

#define U3C_PREFIX        32
#define U3C_OLD_NOUN      u3_32_noun
#define U3C_OLD_ATOM_T    u3a_32_atom
#define U3C_OLD_IS_CAT    u3a_32_is_cat
#define U3C_OLD_IS_CELL   u3a_32_is_cell
#define U3C_OLD_TO_PTR    u3a_32_to_ptr
#define U3C_OLD_HEAD      u3a_32_head
#define U3C_OLD_TAIL      u3a_32_tail
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
u3_migrate_64(c3_d eve_d)
{
  _copy_32_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_32_load(u3C.wor_i);

  if ( eve_d != u3A_32->eve_d ) {
    fprintf(stderr, "loom: migrate (to 64-bit) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_32->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: 32->64 migration running...\r\n");

  cop_u.siz_w = 32;
  cop_u.tac   = c3_malloc(sizeof(*cop_u.tac) * cop_u.siz_w);
  vt_init(&(cop_u.map_u));

  u3A->eve_d = u3A_32->eve_d;
  u3A->roc   = _copy_32_noun(&cop_u, u3A_32->roc);

  cop_u.ham_p = u3R->jed.cod_p;
  u3h_32_walk_with(u3R_32->jed.cod_p, _copy_32_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.per_p;
  u3h_32_walk_with(u3R_32->cax.per_p, _copy_32_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.for_p;
  u3h_32_walk_with(u3R_32->cax.for_p, _copy_32_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_boot(c3y);
  u3j_ream();

  vt_cleanup(&cop_u.map_u);

  c3_free(cop_u.tac);

  fprintf(stderr, "loom: 32->64 migration done\r\n");
}

#endif /* VERE64 */
