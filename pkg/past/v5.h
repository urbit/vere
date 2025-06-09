#ifndef U3_V5_H
#define U3_V5_H

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "retrieve.h"


  /***  current
  ***/
#     define  u3a_v5_atom             u3a_atom
#     define  u3a_v5_gain             u3a_gain
#     define  u3a_v5_heap             u3a_heap
#     define  u3a_v5_is_atom          u3a_is_atom
#     define  u3a_v5_is_cat           u3a_is_cat
#     define  u3a_v5_is_cell          u3a_is_cell
#     define  u3a_v5_is_north         u3a_is_north
#     define  u3a_v5_is_pom           u3a_is_pom
#     define  u3a_v5_is_pug           u3a_is_pug
#     define  u3a_v5_north_is_normal  u3a_north_is_normal
#     define  u3a_v5_pile             u3a_pile
#     define  u3a_v5_walloc           u3a_walloc

#     define  u3h_v5_put              u3h_put
#     define  u3h_v5_buck             u3h_buck
#     define  u3h_v5_node             u3h_node
#     define  u3h_v5_root             u3h_root
#     define  u3h_v5_slot_is_node     u3h_slot_is_node
#     define  u3h_v5_slot_is_noun     u3h_slot_is_noun
#     define  u3h_v5_slot_is_null     u3h_slot_is_null
#     define  u3h_v5_noun_to_slot     u3h_noun_to_slot
#     define  u3h_v5_slot_to_noun     u3h_slot_to_noun
#     define  u3h_v5_walk_with        u3h_walk_with

#     define  u3i_v5_cell             u3i_cell

#     define  u3j_v5_boot             u3j_boot
#     define  u3j_v5_fink             u3j_fink
#     define  u3j_v5_fist             u3j_fist
#     define  u3j_v5_hank             u3j_hank
#     define  u3j_v5_ream             u3j_ream
#     define  u3j_v5_rite             u3j_rite
#     define  u3j_v5_site             u3j_site

#     define  u3n_v5_prog             u3n_prog

#     define  u3r_v5_mug              u3r_mug
#     define  u3r_v5_mug_bytes        u3r_mug_bytes
#     define  u3r_v5_mug_words        u3r_mug_words
#     define  u3r_v5_mug_both         u3r_mug_both

  /***  allocate.h
  ***/

#     define  u3_Loom_v5      (u3_Loom + ((c3_z)1 << u3a_bits_max))
#     define  u3a_v5_vits     1
#     define  u3a_v5_walign   (1 << u3a_v5_vits)
#     define  u3a_v5_balign   (sizeof(c3_w)*u3a_v5_walign)
#     define  u3a_v5_fbox_no  27
#     define  u3a_v5_minimum  ((c3_w)( 1 + c3_wiseof(u3a_v5_box) + c3_wiseof(u3a_v5_cell) ))
#     define  u3a_v5_into(x)  ((void *)(u3_Loom_v5 + (x)))
#     define  u3a_v5_outa(p)  ((c3_w *)(void *)(p) - u3_Loom_v5)
#     define  u3v5to(type, x) ((type *)u3a_v5_into(x))
#     define  u3v5of(type, x) (u3a_v5_outa((type*)x))


      typedef struct {
        c3_w mug_w;
      } u3a_v5_noun;

      typedef struct {
        c3_w    mug_w;
        u3_noun hed;
        u3_noun tel;
      } u3a_v5_cell;

      typedef struct _u3a_v5_box {
        c3_w   siz_w;                       // size of this box
        c3_w   use_w;                       // reference count; free if 0
      } u3a_v5_box;

      typedef struct _u3a_v5_fbox {
        u3a_v5_box               box_u;
        u3p(struct _u3a_v5_fbox) pre_p;
        u3p(struct _u3a_v5_fbox) nex_p;
      } u3a_v5_fbox;

      typedef struct _u3a_v5_road {
        u3p(struct _u3a_v5_road) par_p;          //  parent road
        u3p(struct _u3a_v5_road) kid_p;          //  child road list
        u3p(struct _u3a_v5_road) nex_p;          //  sibling road

        u3p(c3_w) cap_p;                      //  top of transient region
        u3p(c3_w) hat_p;                      //  top of durable region
        u3p(c3_w) mat_p;                      //  bottom of transient region
        u3p(c3_w) rut_p;                      //  bottom of durable region
        u3p(c3_w) ear_p;                      //  original cap if kid is live

        c3_w off_w;                           //  spin stack offset
        c3_w fow_w;                           //  spin stack overflow count

        c3_w fut_w[30];                       //  futureproof buffer

        struct {                              //  escape buffer
          union {
            jmp_buf buf;
            c3_w buf_w[256];                  //  futureproofing
          };
        } esc;

        struct {                              //  miscellaneous config
          c3_w fag_w;                         //  flag bits
        } how;                                //

        struct {                              //  allocation pools
          u3p(u3a_v5_fbox) fre_p[u3a_v5_fbox_no];   //  heap by node size log
          u3p(u3a_v5_fbox) cel_p;                //  custom cell allocator
          c3_w fre_w;                         //  number of free words
          c3_w max_w;                         //  maximum allocated
        } all;

        struct {
          u3p(u3h_root) hot_p;                  //  hot state (home road only)
          u3p(u3h_root) war_p;                  //  warm state
          u3p(u3h_root) cod_p;                  //  cold state
          u3p(u3h_root) han_p;                  //  hank cache
          u3p(u3h_root) bas_p;                  //  battery hashes
        } jed;                                //  jet dashboard

        struct {                              //  bytecode state
          u3p(u3h_v5_root) har_p;                //  formula->post of bytecode
        } byc;

        struct {                              //  scry namespace
          u3_noun gul;                        //  (list $+(* (unit (unit)))) now
        } ski;

        struct {                              //  trace stack
          u3_noun tax;                        //  (list ,*)
          u3_noun mer;                        //  emergency buffer to release
        } bug;

        struct {                              //  profile stack
          c3_d    nox_d;                      //  nock steps
          c3_d    cel_d;                      //  cell allocations
          u3_noun don;                        //  (list batt)
          u3_noun trace;                      //  (list trace)
          u3_noun day;                        //  doss, only in u3H (moveme)
        } pro;

        struct {                              //  memoization caches
          u3p(u3h_v5_root) har_p;                //  transient
          u3p(u3h_v5_root) per_p;                //  persistent
        } cax;
      } u3a_v5_road;
      typedef u3a_v5_road u3_v5_road;

      extern u3a_v5_road* u3a_v5_Road;
#       define u3R_v5  u3a_v5_Road

#     define u3a_v5_to_off(som)  (((som) & 0x3fffffff) << u3a_v5_vits)
#     define u3a_v5_to_ptr(som)  (u3a_v5_into(u3a_v5_to_off(som)))
#     define u3a_v5_to_pug(off)  ((off >> u3a_v5_vits) | 0x80000000)
#     define u3a_v5_to_pom(off)  ((off >> u3a_v5_vits) | 0xc0000000)
#     define u3a_v5_botox(tox_v)  ( (u3a_v5_box *)(void *)(tox_v) - 1 )

        void*
        u3a_v5_walloc(c3_w len_w);
        void
        u3a_v5_wfree(void* lag_v);
        void
        u3a_v5_free(void* tox_v);
        void
        u3a_v5_lose(u3_weak som);
        void
        u3a_v5_ream(void);
        c3_o
        u3a_v5_rewrite_ptr(void* ptr_v);
        u3_post
        u3a_v5_rewritten(u3_post som_p);


  /***  jets.h
  ***/
        void
        u3j_v5_rite_lose(u3j_v5_rite* rit_u);
        void
        u3j_v5_site_lose(u3j_v5_site* sit_u);
        void
        u3j_v5_free_hank(u3_noun kev);


  /***  hashtable.h
  ***/
        void
        u3h_v5_free(u3p(u3h_v5_root) har_p);
        u3p(u3h_v5_root)
        u3h_v5_new(void);
        u3p(u3h_v5_root)
        u3h_v5_new_cache(c3_w max_w);
        void
        u3h_v5_walk(u3p(u3h_v5_root) har_p, void (*fun_f)(u3_noun));

  /***  manage.h
  ***/
        void
        u3m_v5_reclaim(void);

  /***  vortex.h
  ***/
      typedef struct __attribute__((__packed__)) _u3v_v5_arvo {
        c3_d  eve_d;                      //  event number
        u3_noun yot;                      //  cached gates
        u3_noun now;                      //  current time
        u3_noun roc;                      //  kernel core
      } u3v_v5_arvo;

      typedef c3_w  u3v_v5_version;

    /* u3v_home: all internal (within image) state.
    **       NB: version must be last for discriminability in north road
    */
      typedef struct _u3v_v5_home {
        u3a_v5_road    rod_u;                //  storage state
        u3v_v5_arvo    arv_u;                //  arvo state
        u3v_v5_version ver_w;                //  version number
      } u3v_v5_home;

extern u3v_v5_home* u3v_v5_Home;
#       define u3H_v5  u3v_v5_Home
#       define u3A_v5  (&(u3v_v5_Home->arv_u))


  /***  init
  ***/
        void
        u3_v5_load(c3_z wor_i);

#endif /* U3_V5_H */
