#include <stdbool.h>

#include "jets/q.h"
#include "jets/w.h"
#include "c3.h"

#include "allocate.h"
#include "noun.h"

#define OFS_DELTA (u3i_string("ofs-delta"))
#define REF_DELTA (u3i_string("ref-delta"))

static u3_atom _q_octs;

static c3_y* _unpack_octs(u3_atom p_octs, u3_atom* q_octs_p, c3_w* len_wp) {

  c3_y* buf_y;
  c3_w len_w;

  u3_atom q_octs = *q_octs_p;

  u3_assert(c3y == u3a_is_cat(p_octs));

  if (c3y == u3a_is_cat(q_octs)) {

    buf_y = (c3_y*)q_octs_p;
    len_w = sizeof(q_octs);
  }
  else {
    u3a_atom* a = u3a_to_ptr(q_octs);
    buf_y = (c3_y*)a->buf_w;
    len_w = u3r_met(3, q_octs);
  }

  u3_assert(len_w <= p_octs);

  *len_wp = len_w;

  return buf_y;
}

static inline int _read_octs_byte(c3_y* buf_y, c3_w* pos_wp, c3_w buf_len_w, c3_w len_w) {

  c3_w pos_w = *pos_wp;

  u3_assert(buf_len_w <= len_w);
  u3_assert(pos_w < len_w);

  c3_y bat_y = 0;

  if (pos_w < buf_len_w) {
    bat_y = *(buf_y + pos_w);
  }

  (*pos_wp)++;

  return bat_y;
}

static c3_w _read_size(c3_y* buf_y, c3_w* pos_wp, c3_w buf_len_w, c3_w len_w) {

  c3_y bat_y = 0;
  c3_w bits = 0;
  c3_w size = 0;

  c3_w pos_w = *pos_wp;

  // fprintf(stderr, "_read_size: ***\r\n");
  while (pos_w < len_w) {
    bat_y = _read_octs_byte(buf_y, &pos_w, buf_len_w, len_w);

    // fprintf(stderr, "_read_size [%d/%d]: 0x%hhx\r\n", pos_w, len_w, bat_y);

    size += (bat_y & 0x7f) << bits;
    bits += 7;

    if (!(bat_y & 0x80)) break;
  }

  *pos_wp = pos_w;

  return size;
}

u3_noun u3qe_git_pak_expand_delta_object(u3_noun base, 
                                         u3_noun delta) {

  /* +$  raw-object  [type=object-type data=stream:libstream]
   */
  /* +$  pack-object  $%  raw-object
                          [%ofs-delta pos=@ud base-offset=@ud =octs]
                          [%ref-delta =octs]
                      ==
     +$  pack-delta-object  $>(?(%ofs-delta %ref-delta) pack-object)
  */

  // base=raw-object
  //
  u3_atom base_type;
  u3_noun base_data;

  u3_atom base_data_pos;
  u3_atom base_data_p_octs;
  u3_atom base_data_q_octs;

  u3x_cell(base, &base_type, &base_data);
  u3x_trel(base_data, &base_data_pos, &base_data_p_octs, &base_data_q_octs);

  // delta=pack-object
  //
  u3_noun delta_type;
  u3_noun delta_obj;

  u3_atom delta_pos;
  u3_atom delta_base_offset;
  u3_noun delta_octs;
  
  u3_atom delta_p_octs;
  u3_atom delta_q_octs;

  u3x_cell(delta, &delta_type, &delta_obj);

  if ( c3y == u3r_sing(delta_type, OFS_DELTA) ) {
    // fprintf(stderr, "Expanding %%ofs-delta object\r\n");
    u3x_trel(delta_obj, &delta_pos, &delta_base_offset, &delta_octs);
  }
  
  if ( c3y == u3r_sing(delta_type, REF_DELTA) ) {
    // fprintf(stderr, "Expanding %%ref-delta object\r\n");
    delta_octs = delta_obj;
    u3_assert(false);
  }

  u3x_cell(delta_octs, &delta_p_octs, &delta_q_octs);

  c3_y* sea_y;
  c3_y* sea_begin_y;
  c3_w  sea_pos_w;

  c3_w  sea_buf_len_w;
  c3_w  sea_len_w;

  sea_y = _unpack_octs(delta_p_octs, &delta_q_octs, &sea_buf_len_w);
  sea_begin_y = sea_y;
  sea_len_w = delta_p_octs;
  sea_pos_w = 0;

  c3_y* bas_y;
  c3_y* bas_begin_y;

  c3_w  bas_buf_len_w;
  c3_w  bas_len_w;

  bas_y = _unpack_octs(base_data_p_octs, &base_data_q_octs, &bas_buf_len_w);
  bas_begin_y = bas_y;
  bas_len_w = base_data_p_octs;

  /* Base object size (biz) and target 
   * object size (siz)
   */
  // XX should exit cleanly 
  c3_w biz_w = _read_size(sea_y, &sea_pos_w, sea_buf_len_w, sea_len_w);
  c3_w siz_w = _read_size(sea_y, &sea_pos_w, sea_buf_len_w, sea_len_w);

  // fprintf(stderr, "u3qe__pak_expand_delta_object: biz_w = %d, siz_w = %d\r\n", biz_w, siz_w);

  // Base size mismatch
  //
  if (biz_w != (base_data_p_octs - base_data_pos)) {
    fprintf(stderr, "bas_buf_len_w = %d, bas_len_w = %d\r\n", bas_buf_len_w, bas_len_w);
    fprintf(stderr, "sea_pos = %d, sea_buf_len = %d, sea_len = %d\r\n", sea_pos_w, sea_buf_len_w, sea_len_w);
    fprintf(stderr, "u3qe_git_pak_expand_delta_object: base (pos = %d) object size mismatch!\r\n", base_data_pos);

    // u3_assert(false);
    // _free();
    return u3_none;
  }

  // Target buffer
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, siz_w);

  c3_y* tar_y = sab_u.buf_y;
  c3_w  tar_len_w = siz_w;

  c3_y bat_y;

  while (sea_pos_w < sea_len_w) {

    bat_y = _read_octs_byte(sea_y, &sea_pos_w, sea_buf_len_w, sea_len_w);

    // XX ?>  (lth pos.sea p.octs.sea)
    if ( 0x0 == bat_y ) {
      fprintf(stderr, "u3qe__pak_expand_delta_object: hit reserved instruction 0x0\r\n");

      u3m_bail(c3__fail);
    }
    else {
      /* ADD instruction 
       */
      if (!(bat_y & 0x80)) {

        c3_w siz_w = bat_y & 0x7f;

        if (sea_len_w - sea_pos_w < siz_w) {
          fprintf(stderr, "u3qe__pak_expand_delta_object: invalid add instruction\r\n");
          
          u3_assert(false);
          return u3_none;
        }

        // fprintf(stderr, "u3qe__pak_expand_delta: ADD[siz_w = %d]\r\n", siz_w);

        if (tar_len_w < siz_w) {
          fprintf(stderr, "u3qe__pak_expand_delta: ADD overflowed\r\n");

          u3_assert(false);
          return u3_none;
        }

        // Some part to be copied falls inside 
        // the atom buffer
        //
        if (sea_buf_len_w > sea_pos_w) {

          c3_w cin_w = (sea_buf_len_w - sea_pos_w);

          if (siz_w < cin_w) {
            memcpy(tar_y, sea_y + sea_pos_w, siz_w);
          }
          else {
            memcpy(tar_y, sea_y + sea_pos_w, cin_w);
            memset(tar_y+cin_w, 0, siz_w - cin_w);
          }
        }
        else {
          memset(tar_y, 0, siz_w);
        }

        sea_pos_w += siz_w;

        tar_y += siz_w;
        tar_len_w -= siz_w;
      }
      /* COPY instruction 
       */
      else {

        /* Retrieve offset and size
         */
        c3_w off_w = 0;
        c3_w siz_w = 0;

#define _parse_cp_param(bit, var, shift) \
        { \
          if (bat_y & (bit)) { \
            if (!(sea_pos_w < sea_len_w)) { \
              u3_assert(false); \
              return u3_none; \
            } \
            var |= _read_octs_byte(sea_y, &sea_pos_w, sea_buf_len_w, sea_len_w) << shift; \
          } \
        } \

        /* Parse offset
         */
        _parse_cp_param(0x1, off_w, 0);
        _parse_cp_param(0x2, off_w, 8);
        _parse_cp_param(0x4, off_w, 16);
        _parse_cp_param(0x8, off_w, 24);

        /* Parse size
         */
        _parse_cp_param(0x10, siz_w, 0);
        _parse_cp_param(0x20, siz_w, 8);
        _parse_cp_param(0x40, siz_w, 16);

        if (siz_w == 0) {
          siz_w = 0x10000;
        }

        if (tar_len_w < siz_w || (bas_len_w - off_w) < siz_w) {
          fprintf(stderr, "u3qe__pak_expand_delta: copy out of range\r\n");
          u3_assert(false);
          return u3_none;
        }

        // fprintf(stderr, "u3qe__pak_expand_delta: COPY[siz_w = %d, off_w = %d]\r\n", siz_w, off_w);
        
        // Region to be copied overlaps with the atom buffer
        //
        if (bas_buf_len_w > off_w) {

          c3_w cin_w = (bas_buf_len_w - off_w);

          // Region to be copied is wholly inside
          //
          if (siz_w < cin_w) {
            memcpy(tar_y, bas_y + off_w, siz_w);
          }
          // Region to be copied is partially inside
          //
          else {
            memcpy(tar_y, bas_y + off_w, cin_w);
            memset(tar_y+cin_w, 0, siz_w - cin_w);
          }
        }
        else {
          memset(tar_y, 0, siz_w);
        }

        tar_y += siz_w;
        tar_len_w -= siz_w;
      }
    }
  }

  if (tar_len_w) {
    fprintf(stderr, "u3qe__pak_expand_delta: target object underfilled (%d bytes left)\r\n", tar_len_w);
    u3_assert(false);
    return u3_none;
  }

  u3_noun data = u3nc(0, u3nc(u3i_chub(siz_w), u3i_slab_mint(&sab_u)));
  u3_noun rob = u3nc(u3k(base_type), data);

  return rob;
}

u3_noun u3we_git_pak_expand_delta_object(u3_noun cor) {

  u3_noun base;
  u3_noun delta;

  u3x_mean(cor, u3x_sam_2, &base,
           u3x_sam_3, &delta, 0);

  return u3qe_git_pak_expand_delta_object(base, delta);
}
