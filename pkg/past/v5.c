#include "v5.h"

#ifdef VERE64

typedef enum {
  _ce_img_good_v5 = 0,
  _ce_img_fail_v5 = 1,
  _ce_img_size_v5 = 2
} _ce_img_stat_v5;

/* _ce_image_stat_v5(): measure v5 (32-bit) image.
*/
static _ce_img_stat_v5
_ce_image_stat_v5(u3e_image* img_u, c3_w* pgs_w)
{
  struct stat buf_u;

  if ( -1 == fstat(img_u->fid_i, &buf_u) ) {
    fprintf(stderr, "loom: image stat: %s\r\n", strerror(errno));
    u3_assert(0);
    return _ce_img_fail_v5;
  }
  else {
    c3_z siz_z = buf_u.st_size;
    c3_z pgs_z = (siz_z + (_ce_page_v5 - 1))
                   >> (u3a_v5_page + u3a_v5_word_bytes_shift);

    if ( !siz_z ) {
      *pgs_w = 0;
      return _ce_img_good_v5;
    }
    else if ( siz_z != _ce_len_v5(pgs_z) ) {
      fprintf(stderr, "loom: image corrupt size %zu\r\n", siz_z);
      return _ce_img_size_v5;
    }
    else if ( pgs_z > c3_h_max ) {
      fprintf(stderr, "loom: %s overflow %zu\r\n", img_u->nam_c, siz_z);
      return _ce_img_fail_v5;
    }
    else {
      *pgs_w = (c3_w)pgs_z;
      return _ce_img_good_v5;
    }
  }
}

/* _ce_image_open_v5(): open v5 image file.
*/
static _ce_img_stat_v5
_ce_image_open_v5(u3e_image* img_u, c3_c* ful_c)
{
  c3_i mod_i = O_RDWR | O_CREAT;

  c3_c pax_c[8192];
  snprintf(pax_c, 8192, "%s/%s.bin", ful_c, img_u->nam_c);
  if ( -1 == (img_u->fid_i = c3_open(pax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", pax_c, strerror(errno));
    return _ce_img_fail_v5;
  }

  return _ce_image_stat_v5(img_u, &img_u->pgs_w);
}

c3_i
u3e_v5_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z)
{
  u3e_image img_u = { .nam_c = nam_c };

  switch ( _ce_image_open_v5(&img_u, dir_c) ) {
    case _ce_img_good_v5: {
      *len_z = _ce_len_v5(img_u.pgs_w);
      return img_u.fid_i;
    } break;

    case _ce_img_fail_v5:
    case _ce_img_size_v5: {
      *len_z = 0;
      return -1;
    } break;
  }
}

/* Global definitions for v5 home/road.
*/
u3v_v5_home* u3v_v5_Home;
u3a_v5_road* u3a_v5_Road;

/* u3_v5_load(): locate u3v_v5_home in the mapped v5 image.
**
**   unlike v4 (which placed home at the high end of the loom),
**   v5 (palloc) places home at position 0, just like the current
**   64-bit layout. the version sentinel is therefore the first
**   8 bytes of the image, not the last word
*/
void
u3_v5_load(c3_z wor_i)
{
  (void)wor_i;
  u3H_v5 = (u3v_v5_home *)u3_Loom_v5;
  u3R_v5 = &u3H_v5->rod_u;
}

/* _ch_v5_walk_buck(): walk v5 HAMT bucket.
*/
static void
_ch_v5_walk_buck(u3h_v5_buck* hab_u, void (*fun_f)(u3_v5_noun, void*), void* wit)
{
  c3_h i_h;

  for ( i_h = 0; i_h < hab_u->len_h; i_h++ ) {
    fun_f(u3h_v5_slot_to_noun(hab_u->sot_w[i_h]), wit);
  }
}

/* _ch_v5_walk_node(): walk v5 HAMT node.
*/
static void
_ch_v5_walk_node(u3h_v5_node* han_u, c3_w lef_w, void (*fun_f)(u3_v5_noun, void*), void* wit)
{
  c3_w len_w = c3_pc_w(han_u->map_h);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    u3h_v5_slot sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v5_slot_is_noun(sot_w)) ) {
      fun_f(u3h_v5_slot_to_noun(sot_w), wit);
    }
    else if ( _(u3h_v5_slot_is_node(sot_w)) ) {
      void* hav_v = u3h_v5_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v5_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_v5_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_v5_walk_with(): traverse v5 HAMT calling fun_f(kev, wit) on each entry.
*/
void
u3h_v5_walk_with(c3_v5_w har_p,
                 void (*fun_f)(u3_v5_noun, void*),
                 void* wit)
{
  u3h_v5_root* har_u = (u3h_v5_root *)u3a_v5_into(har_p);
  c3_w i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    u3h_v5_slot sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v5_slot_is_noun(sot_w)) ) {
      fun_f(u3h_v5_slot_to_noun(sot_w), wit);
    }
    else if ( _(u3h_v5_slot_is_node(sot_w)) ) {
      u3h_v5_node* han_u = (u3h_v5_node *)u3h_v5_slot_to_node(sot_w);

      _ch_v5_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

#endif /* VERE64 */
