#ifndef U3_PAST_64_H
#define U3_PAST_64_H

#ifdef VERE64

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"
#include "events.h"

      typedef uint32_t c3_32_w;
      typedef int32_t  c3_32_ws;

#     define  u3a_32_page              12ULL
#     define  u3a_32_word_bytes_shift  2
#     define  u3a_32_vits              2
#     define  u3a_32_bits_max          34

#     define  u3_Loom_32  ((c3_32_w *)((c3_c *)u3_Loom + ((c3_z)8 << u3a_32_bits_max)))

#     define  _ce_len_32(i)        ((size_t)(i) << (u3a_32_page + u3a_32_word_bytes_shift))
#     define  _ce_len_words_32(i)  ((size_t)(i) << u3a_32_page)
#     define  _ce_page_32          _ce_len_32(1)
#     define  _ce_ptr_32(i)        ((void *)((c3_c*)u3_Loom_32 + _ce_len_32(i)))

      c3_i
      u3e_32_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z);

      typedef c3_32_w u3_32_noun;

#     define  u3a_32_walign   ((c3_32_w)1 << u3a_32_vits)
#     define  u3a_32_balign   (sizeof(c3_32_w) * u3a_32_walign)
#     define  u3a_32_crag_no  10u

#     define  u3a_32_is_cat(som)   (((som) >> 31) ? c3n : c3y)
#     define  u3a_32_is_pug(som)   ((0b10 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_32_is_pom(som)   ((0b11 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_32_is_cell(som)  u3a_32_is_pom(som)

#     define  u3a_32_to_off(som)   (((som) & 0x3fffffffu) << u3a_32_vits)
#     define  u3a_32_into(x)       ((void *)(u3_Loom_32 + (x)))
#     define  u3a_32_outa(p)       ((c3_32_w *)(void *)(p) - u3_Loom_32)
#     define  u3a_32_to_ptr(som)   (u3a_32_into(u3a_32_to_off(som)))
#     define  u3a_32_to_pug(off)   (((off) >> u3a_32_vits) | 0x80000000u)

#     define  u3a_32_head(som)  (((u3a_32_cell *)u3a_32_to_ptr(som))->hed)
#     define  u3a_32_tail(som)  (((u3a_32_cell *)u3a_32_to_ptr(som))->tel)

      typedef struct {
        c3_32_w    use_w;
        c3_32_w    mug_w;
        c3_32_w    len_w;
        c3_32_w    buf_w[0];
      } u3a_32_atom;

      typedef struct {
        c3_32_w    use_w;
        c3_32_w    mug_w;
        u3_32_noun hed;
        u3_32_noun tel;
      } u3a_32_cell;

      typedef c3_32_w u3h_32_slot;

#     define  u3h_32_slot_is_null(sot) ((0 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_32_slot_is_node(sot) ((1 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_32_slot_is_noun(sot) ((1 == ((sot) >> 31)) ? c3y : c3n)
#     define  u3h_32_slot_to_node(sot) (u3a_32_into(((sot) & 0x3fffffffu) << u3a_32_vits))
#     define  u3h_32_slot_to_noun(sot) ((u3_32_noun)(0x40000000u | (sot)))

      typedef struct {
        c3_h        map_h;
        u3h_32_slot sot_w[];
      } u3h_32_node;

      typedef struct {
        c3_h        len_h;
        u3h_32_slot sot_w[];
      } u3h_32_buck;

      typedef struct {
        c3_32_w     max_w;
        c3_32_w     use_w;
        struct {
          c3_h    mug_h;
          c3_h    inx_h;
          c3_32_w buc_o;
        } arm_u;
        u3h_32_slot sot_w[64];
      } u3h_32_root;

      typedef struct _u3a_32_road {
        c3_32_w par_p;
        c3_32_w kid_p;
        c3_32_w nex_p;

        c3_32_w cap_p;
        c3_32_w hat_p;
        c3_32_w mat_p;
        c3_32_w rut_p;
        c3_32_w ear_p;

        c3_32_w    off_w;
        c3_32_w    fow_w;
        c3_32_w    lop_p;
        u3_32_noun tim;

        c3_32_w fut_w[28];

        struct {
          union {
            jmp_buf buf;
            c3_32_w buf_w[256];
          };
        } esc;

        struct { c3_32_w fag_w; } how;

        struct {
          c3_32_w fre_w;
          c3_32_w max_w;
        } all;

        struct {
          c3_32_w  fre_p;
          c3_32_w  erf_p;
          c3_32_w  cac_p;
          c3_32_ws dir_ws;
          c3_32_ws off_ws;
          c3_32_w  siz_w;
          c3_32_w  len_w;
          c3_32_w  pag_p;
          c3_32_w  wee_p[u3a_32_crag_no];
        } hep;

        struct {
          c3_32_w cel_p;
          c3_32_w hav_w;
          c3_32_w bat_w;
        } cel;

        struct {
          c3_32_w hot_p;
          c3_32_w war_p;
          c3_32_w cod_p;
          c3_32_w han_p;
          c3_32_w bas_p;
        } jed;

        struct { c3_32_w    har_p;              } byc;
        struct { u3_32_noun gul;                } ski;
        struct { u3_32_noun tax; u3_32_noun mer; } bug;

        struct {
          c3_d       nox_d;
          c3_d       cel_d;
          u3_32_noun don;
          u3_32_noun trace;
          u3_32_noun day;
        } pro;

        struct {
          c3_32_w har_p;
          c3_32_w per_p;
          c3_32_w for_p;
        } cax;
      } u3a_32_road;

      typedef struct {
        c3_d       eve_d;
        u3_32_noun roc;
        u3_32_noun yot;
      } u3v_32_arvo;

      typedef struct {
        u3v_version ver_d;
        c3_d        pam_d;
        u3v_32_arvo arv_u;
        u3a_32_road rod_u;
      } u3v_32_home;

      extern u3v_32_home* u3v_32_Home;
      extern u3a_32_road* u3a_32_Road;
#       define u3H_32  u3v_32_Home
#       define u3R_32  u3a_32_Road
#       define u3A_32  (&(u3v_32_Home->arv_u))

      void
      u3_32_load(c3_z wor_i);

      void
      u3h_32_walk_with(c3_32_w har_p,
                       void (*fun_f)(u3_32_noun, void*),
                       void* wit);

#endif /* VERE64 */
#endif /* U3_PAST_64_H */
