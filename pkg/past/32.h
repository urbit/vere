#ifndef U3_PAST_32_H
#define U3_PAST_32_H

#ifndef VERE64

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"
#include "events.h"

      typedef uint64_t c3_64_w;
      typedef int64_t  c3_64_ws;

#     define  u3a_64_page              11ULL
#     define  u3a_64_word_bytes_shift  3
#     define  u3a_64_vits              0
#     define  u3a_64_crag_no           10u

#     define  u3_Loom_64  ((c3_64_w *)((c3_c *)u3_Loom + ((c3_z)8 << u3a_bits_max)))

#     define  _ce_len_64(i)   ((size_t)(i) << (u3a_64_page + u3a_64_word_bytes_shift))
#     define  _ce_page_64     _ce_len_64(1)

      c3_i
      u3e_64_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z);

      typedef c3_64_w u3_64_noun;
      typedef c3_64_w u3h_64_slot;

#     define  u3a_64_is_cat(som)   (((som) >> 63) ? c3n : c3y)
#     define  u3a_64_is_pug(som)   ((0x2ULL == ((som) >> 62)) ? c3y : c3n)
#     define  u3a_64_is_pom(som)   ((0x3ULL == ((som) >> 62)) ? c3y : c3n)
#     define  u3a_64_is_cell(som)  u3a_64_is_pom(som)

#     define  u3a_64_to_off(som)   ((som) & 0x3fffffffffffffffULL)
#     define  u3a_64_into(x)       ((void *)(u3_Loom_64 + (x)))
#     define  u3a_64_to_ptr(som)   (u3a_64_into(u3a_64_to_off(som)))

#     define  u3a_64_head(som)     (((u3a_64_cell *)u3a_64_to_ptr(som))->hed)
#     define  u3a_64_tail(som)     (((u3a_64_cell *)u3a_64_to_ptr(som))->tel)

      typedef struct {
        c3_64_w  use_w;
        c3_w     mug_w;
        c3_64_w  len_w;
        c3_64_w  buf_w[0];
      } u3a_64_atom;

      typedef struct {
        c3_64_w     use_w;
        c3_w        mug_w;
        u3_64_noun  hed;
        u3_64_noun  tel;
      } u3a_64_cell;

#     define  u3h_64_slot_is_null(sot) ((0 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_64_slot_is_node(sot) ((1 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_64_slot_is_noun(sot) ((1 == ((sot) >> 63)) ? c3y : c3n)
#     define  u3h_64_slot_to_node(sot) (u3a_64_into(((sot) & 0x3fffffffffffffffULL)))
#     define  u3h_64_slot_to_noun(sot) ((u3_64_noun)(0x4000000000000000ULL | (sot)))

      typedef struct {
        c3_h         map_h;
        c3_h         pad_h;
        u3h_64_slot  sot_w[];
      } u3h_64_node;

      typedef struct {
        c3_h         len_h;
        c3_h         pad_h;
        u3h_64_slot  sot_w[];
      } u3h_64_buck;

      typedef struct {
        c3_64_w     max_w;
        c3_64_w     use_w;
        struct {
          c3_h    mug_h;
          c3_h    inx_h;
          c3_o    buc_o;
          c3_y    pad_y[3];
        } arm_u;
        c3_y         pad_y[4];
        u3h_64_slot  sot_w[64];
      } u3h_64_root;

      typedef struct _u3a_64_road {
        c3_64_w par_p;
        c3_64_w kid_p;
        c3_64_w nex_p;

        c3_64_w cap_p;
        c3_64_w hat_p;
        c3_64_w mat_p;
        c3_64_w rut_p;
        c3_64_w ear_p;

        c3_64_w    off_w;
        c3_64_w    fow_w;
        c3_64_w    lop_p;
        u3_64_noun tim;

        c3_64_w fut_w[28];

        struct {
          union {
            c3_64_w buf_w[256];
          };
        } esc;

        struct { c3_64_w fag_w; } how;

        struct {
          c3_64_w fre_w;
          c3_64_w max_w;
        } all;

        struct {
          c3_64_w  fre_p;
          c3_64_w  erf_p;
          c3_64_w  cac_p;
          c3_64_ws dir_ws;
          c3_64_ws off_ws;
          c3_64_w  siz_w;
          c3_64_w  len_w;
          c3_64_w  pag_p;
          c3_64_w  wee_p[u3a_64_crag_no];
        } hep;

        struct {
          c3_64_w cel_p;
          c3_64_w hav_w;
          c3_64_w bat_w;
        } cel;

        struct {
          c3_64_w hot_p;
          c3_64_w war_p;
          c3_64_w cod_p;
          c3_64_w han_p;
          c3_64_w bas_p;
        } jed;

        struct { c3_64_w    har_p;                        } byc;
        struct { u3_64_noun gul;                          } ski;
        struct { u3_64_noun tax; u3_64_noun mer;          } bug;

        struct {
          c3_d        nox_d;
          c3_d        cel_d;
          u3_64_noun  don;
          u3_64_noun  trace;
          u3_64_noun  day;
        } pro;

        struct {
          c3_64_w har_p;
          c3_64_w per_p;
          c3_64_w for_p;
        } cax;
      } u3a_64_road;

      typedef struct {
        c3_d        eve_d;
        u3_64_noun  roc;
        u3_64_noun  yot;
      } u3v_64_arvo;

      typedef struct {
        u3v_version  ver_d;
        c3_d         pam_d;
        u3v_64_arvo  arv_u;
        u3a_64_road  rod_u;
      } u3v_64_home;

      extern u3v_64_home* u3v_64_Home;
      extern u3a_64_road* u3a_64_Road;
#       define u3H_64  u3v_64_Home
#       define u3R_64  u3a_64_Road
#       define u3A_64  (&(u3v_64_Home->arv_u))

      void
      u3_64_load(c3_z wor_i);

      void
      u3h_64_walk_with(c3_64_w har_p,
                       void (*fun_f)(u3_64_noun, void*),
                       void* wit);

#endif /* !VERE64 */
#endif /* U3_PAST_32_H */
