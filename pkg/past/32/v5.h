#ifndef U3_V5_H
#define U3_V5_H

#ifdef VERE64
#include "../64/v5.h"

      typedef uint32_t c3_v5_w;
      typedef int32_t  c3_v5_ws;

#     define  u3a_v5_page              12ULL
#     define  u3a_v5_word_bytes_shift  2
#     define  u3a_v5_vits              2
#     define  u3a_v5_bits_max          34

#     define  u3_Loom_v5  ((c3_v5_w *)((c3_c *)u3_Loom + ((c3_z)8 << u3a_v5_bits_max)))

#     define  _ce_len_v5(i)        ((size_t)(i) << (u3a_v5_page + u3a_v5_word_bytes_shift))
#     define  _ce_len_words_v5(i)  ((size_t)(i) << u3a_v5_page)
#     define  _ce_page_v5          _ce_len_v5(1)
#     define  _ce_ptr_v5(i)        ((void *)((c3_c*)u3_Loom_v5 + _ce_len_v5(i)))

      c3_i
      u3e_v5_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z);

      typedef c3_v5_w u3_v5_noun;

#     define  u3a_v5_walign   ((c3_v5_w)1 << u3a_v5_vits)
#     define  u3a_v5_balign   (sizeof(c3_v5_w) * u3a_v5_walign)
#     define  u3a_v5_crag_no  10u

#     define  u3a_v5_is_cat(som)   (((som) >> 31) ? c3n : c3y)
#     define  u3a_v5_is_pug(som)   ((0b10 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_v5_is_pom(som)   ((0b11 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_v5_is_cell(som)  u3a_v5_is_pom(som)

#     define  u3a_v5_to_off(som)   (((som) & 0x3fffffffu) << u3a_v5_vits)
#     define  u3a_v5_into(x)       ((void *)(u3_Loom_v5 + (x)))
#     define  u3a_v5_outa(p)       ((c3_v5_w *)(void *)(p) - u3_Loom_v5)
#     define  u3a_v5_to_ptr(som)   (u3a_v5_into(u3a_v5_to_off(som)))
#     define  u3a_v5_to_pug(off)   (((off) >> u3a_v5_vits) | 0x80000000u)

#     define  u3a_v5_head(som)  (((u3a_v5_cell *)u3a_v5_to_ptr(som))->hed)
#     define  u3a_v5_tail(som)  (((u3a_v5_cell *)u3a_v5_to_ptr(som))->tel)

      typedef struct {
        c3_v5_w    use_w;
        c3_v5_w    mug_w;
        c3_v5_w    len_w;
        c3_v5_w    buf_w[0];
      } u3a_v5_atom;

      typedef struct {
        c3_v5_w    use_w;
        c3_v5_w    mug_w;
        u3_v5_noun hed;
        u3_v5_noun tel;
      } u3a_v5_cell;

      typedef c3_v5_w u3h_v5_slot;

#     define  u3h_v5_slot_is_null(sot) ((0 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_v5_slot_is_node(sot) ((1 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_v5_slot_is_noun(sot) ((1 == ((sot) >> 31)) ? c3y : c3n)
#     define  u3h_v5_slot_to_node(sot) (u3a_v5_into(((sot) & 0x3fffffffu) << u3a_v5_vits))
#     define  u3h_v5_slot_to_noun(sot) ((u3_v5_noun)(0x40000000u | (sot)))

      typedef struct {
        c3_h        map_h;
        u3h_v5_slot sot_w[];
      } u3h_v5_node;

      typedef struct {
        c3_h        len_h;
        u3h_v5_slot sot_w[];
      } u3h_v5_buck;

      typedef struct {
        c3_v5_w     max_w;
        c3_v5_w     use_w;
        struct {
          c3_h    mug_h;
          c3_h    inx_h;
          c3_v5_w buc_o;
        } arm_u;
        u3h_v5_slot sot_w[64];
      } u3h_v5_root;

      typedef struct _u3a_v5_road {
        c3_v5_w par_p;
        c3_v5_w kid_p;
        c3_v5_w nex_p;

        c3_v5_w cap_p;
        c3_v5_w hat_p;
        c3_v5_w mat_p;
        c3_v5_w rut_p;
        c3_v5_w ear_p;

        c3_v5_w    off_w;
        c3_v5_w    fow_w;
        c3_v5_w    lop_p;
        u3_v5_noun tim;

        c3_v5_w fut_w[28];

        struct {
          union {
            jmp_buf buf;
            c3_v5_w buf_w[256];
          };
        } esc;

        struct { c3_v5_w fag_w; } how;

        struct {
          c3_v5_w fre_w;
          c3_v5_w max_w;
        } all;

        struct {
          c3_v5_w  fre_p;
          c3_v5_w  erf_p;
          c3_v5_w  cac_p;
          c3_v5_ws dir_ws;
          c3_v5_ws off_ws;
          c3_v5_w  siz_w;
          c3_v5_w  len_w;
          c3_v5_w  pag_p;
          c3_v5_w  wee_p[u3a_v5_crag_no];
        } hep;

        struct {
          c3_v5_w cel_p;
          c3_v5_w hav_w;
          c3_v5_w bat_w;
        } cel;

        struct {
          c3_v5_w hot_p;
          c3_v5_w war_p;
          c3_v5_w cod_p;
          c3_v5_w han_p;
          c3_v5_w bas_p;
        } jed;

        struct { c3_v5_w    har_p;              } byc;
        struct { u3_v5_noun gul;                } ski;
        struct { u3_v5_noun tax; u3_v5_noun mer; } bug;

        struct {
          c3_d       nox_d;
          c3_d       cel_d;
          u3_v5_noun don;
          u3_v5_noun trace;
          u3_v5_noun day;
        } pro;

        struct {
          c3_v5_w har_p;
          c3_v5_w per_p;
          c3_v5_w for_p;
        } cax;
      } u3a_v5_road;

      typedef struct {
        c3_d       eve_d;
        u3_v5_noun roc;
        u3_v5_noun yot;
      } u3v_v5_arvo;

      typedef struct {
        u3v_version ver_d;
        c3_d        pam_d;
        u3v_v5_arvo arv_u;
        u3a_v5_road rod_u;
      } u3v_v5_home;

      extern u3v_v5_home* u3v_v5_Home;
      extern u3a_v5_road* u3a_v5_Road;
#       define u3H_v5  u3v_v5_Home
#       define u3R_v5  u3a_v5_Road
#       define u3A_v5  (&(u3v_v5_Home->arv_u))

      void
      u3_v5_load(c3_z wor_i);

      void
      u3h_v5_walk_with(c3_v5_w har_p,
                       void (*fun_f)(u3_v5_noun, void*),
                       void* wit);

#else
#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"

#     define  u3_v5_cell          u3_cell
#     define  u3_v5_noun          u3_noun
#     define  u3_v5_none          u3_none

#     define  u3A_v5              u3A
#     define  u3R_v5              u3R
#     define  u3j_v5_boot         u3j_boot
#     define  u3j_v5_ream         u3j_ream
#     define  u3a_v5_walloc       u3a_walloc
#     define  u3a_v5_to_pug       u3a_to_pug
#     define  u3a_v5_outa         u3a_outa
#     define  u3a_v5_gain         u3a_gain
#     define  u3i_v5_cell         u3i_cell
#     define  u3h_v5_put          u3h_put
#     define  u3a_v5_lose         u3a_lose

#     define  u3a_v5_atom             u3a_atom
#     define  u3a_v5_is_atom          u3a_is_atom
#     define  u3a_v5_is_pom           u3a_is_pom
#     define  u3a_v5_north_is_normal  u3a_north_is_normal
#     define  u3n_v5_prog             u3n_prog
#     define  u3r_v5_mug_both         u3r_mug_both
#     define  u3r_v5_mug_words        u3r_mug_words

#     define  u3a_v5_heap         u3a_heap
#     define  u3a_v5_is_cat       u3a_is_cat
#     define  u3a_v5_is_cell      u3a_is_cell
#     define  u3a_v5_is_north     u3a_is_north
#     define  u3a_v5_is_pom       u3a_is_pom
#     define  u3a_v5_is_pug       u3a_is_pug

#     define  u3j_v5_fink         u3j_fink
#     define  u3j_v5_fist         u3j_fist
#     define  u3j_v5_hank         u3j_hank
#     define  u3j_v5_rite         u3j_rite
#     define  u3j_v5_site         u3j_site

#     define  u3h_v5_buck         u3h_buck
#     define  u3h_v5_node         u3h_node
#     define  u3h_v5_root         u3h_root
#     define  u3h_v5_slot_is_node u3h_slot_is_node
#     define  u3h_v5_slot_is_noun u3h_slot_is_noun
#     define  u3h_v5_slot_is_null u3h_slot_is_null
#     define  u3h_v5_noun_to_slot u3h_noun_to_slot
#     define  u3h_v5_slot_to_noun u3h_slot_to_noun

#endif /* VERE64 */
#endif /* U3_V5_H */
