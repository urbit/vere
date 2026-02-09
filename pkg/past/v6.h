#ifndef U3_V6_H
#define U3_V6_H

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef VERE64

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"
#include "events.h"

#else

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"
#include "events.h"

      typedef uint64_t c3_v6_w;
      typedef int64_t  c3_v6_ws;

#     define  u3a_v6_page              11ULL
#     define  u3a_v6_word_bytes_shift  3
#     define  u3a_v6_vits              0
#     define  u3a_v6_crag_no           10u

#     define  u3_Loom_v6  ((c3_v6_w *)((c3_c *)u3_Loom + ((c3_z)8 << u3a_bits_max)))

#     define  _ce_len_v6(i)   ((size_t)(i) << (u3a_v6_page + u3a_v6_word_bytes_shift))
#     define  _ce_page_v6     _ce_len_v6(1)

      c3_i
      u3e_v6_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z);

      typedef c3_v6_w u3_v6_noun;
      typedef c3_v6_w u3h_v6_slot;

#     define  u3a_v6_is_cat(som)   (((som) >> 63) ? c3n : c3y)
#     define  u3a_v6_is_pug(som)   ((0x2ULL == ((som) >> 62)) ? c3y : c3n)
#     define  u3a_v6_is_pom(som)   ((0x3ULL == ((som) >> 62)) ? c3y : c3n)
#     define  u3a_v6_is_cell(som)  u3a_v6_is_pom(som)

#     define  u3a_v6_to_off(som)   ((som) & 0x3fffffffffffffffULL)
#     define  u3a_v6_into(x)       ((void *)(u3_Loom_v6 + (x)))
#     define  u3a_v6_to_ptr(som)   (u3a_v6_into(u3a_v6_to_off(som)))

#     define  u3a_v6_head(som)     (((u3a_v6_cell *)u3a_v6_to_ptr(som))->hed)
#     define  u3a_v6_tail(som)     (((u3a_v6_cell *)u3a_v6_to_ptr(som))->tel)

      typedef struct __attribute__((aligned(8))) {
        c3_v6_w    use_w;
        c3_h       mug_h;
        c3_h       fut_h;
        c3_v6_w    len_w;
        c3_v6_w    buf_w[0];
      } u3a_v6_atom;

      typedef struct __attribute__((aligned(8))) {
        c3_v6_w    use_w;
        c3_h       mug_h;
        c3_h       fut_h;
        u3_v6_noun hed;
        u3_v6_noun tel;
      } u3a_v6_cell;

#     define  u3h_v6_slot_is_null(sot) ((0 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_v6_slot_is_node(sot) ((1 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_v6_slot_is_noun(sot) ((1 == ((sot) >> 63)) ? c3y : c3n)
#     define  u3h_v6_slot_to_node(sot) (u3a_v6_into(((sot) & 0x3fffffffffffffffULL)))
#     define  u3h_v6_slot_to_noun(sot) ((u3_v6_noun)(0x4000000000000000ULL | (sot)))

      typedef struct {
        c3_h        map_h;
        c3_h        pad_h;
        u3h_v6_slot sot_w[];
      } u3h_v6_node;

      typedef struct {
        c3_h        len_h;
        c3_h        pad_h;
        u3h_v6_slot sot_w[];
      } u3h_v6_buck;

      typedef struct {
        c3_v6_w     max_w;
        c3_v6_w     use_w;
        struct {
          c3_h    mug_h;
          c3_h    inx_h;
          c3_o    buc_o;
          c3_y    pad_y[3];
        } arm_u;
        c3_y        pad_y[4];
        u3h_v6_slot sot_w[64];
      } u3h_v6_root;

      typedef struct _u3a_v6_road {
        c3_v6_w par_p;
        c3_v6_w kid_p;
        c3_v6_w nex_p;

        c3_v6_w cap_p;
        c3_v6_w hat_p;
        c3_v6_w mat_p;
        c3_v6_w rut_p;
        c3_v6_w ear_p;

        c3_v6_w    off_w;
        c3_v6_w    fow_w;
        c3_v6_w    lop_p;
        u3_v6_noun tim;

        c3_v6_w fut_w[28];

        struct {
          union {
            c3_v6_w buf_w[256];
          };
        } esc;

        struct { c3_v6_w fag_w; } how;

        struct {
          c3_v6_w fre_w;
          c3_v6_w max_w;
        } all;

        struct {
          c3_v6_w  fre_p;
          c3_v6_w  erf_p;
          c3_v6_w  cac_p;
          c3_v6_ws dir_ws;
          c3_v6_ws off_ws;
          c3_v6_w  siz_w;
          c3_v6_w  len_w;
          c3_v6_w  pag_p;
          c3_v6_w  wee_p[u3a_v6_crag_no];
        } hep;

        struct {
          c3_v6_w cel_p;
          c3_v6_w hav_w;
          c3_v6_w bat_w;
        } cel;

        struct {
          c3_v6_w hot_p;
          c3_v6_w war_p;
          c3_v6_w cod_p;
          c3_v6_w han_p;
          c3_v6_w bas_p;
        } jed;

        struct { c3_v6_w    har_p;              } byc;
        struct { u3_v6_noun gul;                } ski;
        struct { u3_v6_noun tax; u3_v6_noun mer; } bug;

        struct {
          c3_d       nox_d;
          c3_d       cel_d;
          u3_v6_noun don;
          u3_v6_noun trace;
          u3_v6_noun day;
        } pro;

        struct {
          c3_v6_w har_p;
          c3_v6_w per_p;
        } cax;
      } u3a_v6_road;

      typedef struct {
        c3_d       eve_d;
        u3_v6_noun roc;
        u3_v6_noun yot;
      } u3v_v6_arvo;

      typedef struct {
        u3v_version  ver_d;
        c3_d         pam_d;
        u3v_v6_arvo  arv_u;
        u3a_v6_road  rod_u;
      } u3v_v6_home;

      extern u3v_v6_home* u3v_v6_Home;
      extern u3a_v6_road* u3a_v6_Road;
#       define u3H_v6  u3v_v6_Home
#       define u3R_v6  u3a_v6_Road
#       define u3A_v6  (&(u3v_v6_Home->arv_u))

      void
      u3_v6_load(c3_z wor_i);

      void
      u3h_v6_walk_with(c3_v6_w har_p,
                       void (*fun_f)(u3_v6_noun, void*),
                       void* wit);

#endif /* VERE64 */

#endif /* U3_V6_H */
