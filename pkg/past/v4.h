#ifndef U3_V4_H
#define U3_V4_H

#include "v5.h"

#     define  u3a_v4_heap             u3a_v5_heap
#     define  u3a_v4_is_north         u3a_v5_is_north
#     define  u3a_v4_north_is_normal  u3a_v5_north_is_normal

//  XX
#     define  u3j_v4_fink             u3j_v5_fink
#     define  u3j_v4_fist             u3j_v5_fist
#     define  u3j_v4_hank             u3j_v5_hank
#     define  u3j_v4_rite             u3j_v5_rite
#     define  u3j_v4_site             u3j_v5_site

  /***  {c3,noun}/types.h
  ***/

#     define  c3_v4_l                 c3_v5_l
#     define  c3_v4_w                 c3_v5_w
#     define  u3_v4_noun              u3_v5_noun
#     define  u3_v4_none              u3_v5_none
#     define  u3_v4_weak              u3_v5_weak
#     define  u3_v4_post              u3_v5_post
#     define  u3v4p(type)             u3_v4_post

  /***  c3/defs.h
  ***/
#     define  c3_v4_wiseof            c3_v5_wiseof
#     define  c3_v4_align(x, al, hilo)             \
        _Generic((x),                              \
           c3_v4_w     : c3_v4_align_w,            \
           c3_d        : c3_align_d,               \
           default     : c3_align_p)               \
       (x, al, hilo)
inline c3_v4_w
c3_v4_align_w(c3_v4_w x, c3_v4_w al, align_dir hilo) {
  c3_dessert(hilo <= C3_ALGHI && hilo >= C3_ALGLO);
  x += hilo * (al - 1);
  x &= ~(al - 1);
  return x;
}

  /***  allocate.h
  ***/

#     define  u3_Loom_v4           u3_Loom_v5
#     define  u3a_v4_vits          1
#     define  u3a_v4_bits_max      u3a_v5_bits_max
#     define  u3a_v4_walign        (1 << u3a_v4_vits)
#     define  u3a_v4_balign        (sizeof(c3_v4_w)*u3a_v4_walign)
#     define  u3a_v4_fbox_no       27
#     define  u3a_v4_minimum       ((c3_v4_w)( 1 + c3_v4_wiseof(u3a_v4_box) + c3_v4_wiseof(u3a_v4_cell) ))
#     define  u3a_v4_into(x)       ((void *)(u3_Loom_v4 + (x)))
#     define  u3a_v4_outa(p)       ((c3_v4_w *)(void *)(p) - u3_Loom_v4)
#     define  u3v4to(type, x)      ((type *)u3a_v4_into(x))
#     define  u3v4of(type, x)      (u3a_v4_outa((type*)x))
#     define  u3a_v4_is_cat        u3a_v5_is_cat
#     define  u3a_v4_is_pom        u3a_v5_is_pom
#     define  u3a_v4_is_cell       u3a_v5_is_pom
#     define  u3v4du               u3a_v5_is_cell
#     define  u3a_v4_is_pug        u3a_v5_is_pug


      typedef struct {
        c3_v4_w mug_w;
      } u3a_v4_noun;

      typedef struct {
        c3_v4_w mug_w;
        c3_v4_w len_w;
        c3_v4_w buf_w[0];
      } u3a_v4_atom;

      typedef struct {
        c3_v4_w    mug_w;
        u3_v4_noun hed;
        u3_v4_noun tel;
      } u3a_v4_cell;

      typedef struct _u3a_v4_box {
        c3_v4_w   siz_w;                       // size of this box
        c3_v4_w   use_w;                       // reference count; free if 0
      } u3a_v4_box;

      typedef struct _u3a_v4_fbox {
        u3a_v4_box               box_u;
        u3v4p(struct _u3a_v4_fbox) pre_p;
        u3v4p(struct _u3a_v4_fbox) nex_p;
      } u3a_v4_fbox;

      typedef struct _u3a_v4_road {
        u3v4p(struct _u3a_v4_road) par_p;          //  parent road
        u3v4p(struct _u3a_v4_road) kid_p;          //  child road list
        u3v4p(struct _u3a_v4_road) nex_p;          //  sibling road

        u3v4p(c3_v4_w) cap_p;                      //  top of transient region
        u3v4p(c3_v4_w) hat_p;                      //  top of durable region
        u3v4p(c3_v4_w) mat_p;                      //  bottom of transient region
        u3v4p(c3_v4_w) rut_p;                      //  bottom of durable region
        u3v4p(c3_v4_w) ear_p;                      //  original cap if kid is live

        c3_v4_w off_w;                           //  spin stack offset
        c3_v4_w fow_w;                           //  spin stack overflow count

        c3_v4_w fut_w[30];                       //  futureproof buffer

        struct {                              //  escape buffer
          union {
            jmp_buf buf;
            c3_v4_w buf_w[256];                  //  futureproofing
          };
        } esc;

        struct {                              //  miscellaneous config
          c3_v4_w fag_w;                         //  flag bits
        } how;                                //

        struct {                              //  allocation pools
          u3v4p(u3a_v4_fbox) fre_p[u3a_v4_fbox_no];   //  heap by node size log
          u3v4p(u3a_v4_fbox) cel_p;                //  custom cell allocator
          c3_v4_w fre_w;                         //  number of free words
          c3_v4_w max_w;                         //  maximum allocated
        } all;

        struct {
          u3v4p(u3h_v4_root) hot_p;                  //  hot state (home road only)
          u3v4p(u3h_v4_root) war_p;                  //  warm state
          u3v4p(u3h_v4_root) cod_p;                  //  cold state
          u3v4p(u3h_v4_root) han_p;                  //  hank cache
          u3v4p(u3h_v4_root) bas_p;                  //  battery hashes
        } jed;                                //  jet dashboard

        struct {                              //  bytecode state
          u3v4p(u3h_v4_root) har_p;                //  formula->post of bytecode
        } byc;

        struct {                              //  scry namespace
          u3_v4_noun gul;                        //  (list $+(* (unit (unit)))) now
        } ski;

        struct {                              //  trace stack
          u3_v4_noun tax;                        //  (list ,*)
          u3_v4_noun mer;                        //  emergency buffer to release
        } bug;

        struct {                              //  profile stack
          c3_d    nox_d;                      //  nock steps
          c3_d    cel_d;                      //  cell allocations
          u3_v4_noun don;                        //  (list batt)
          u3_v4_noun trace;                      //  (list trace)
          u3_v4_noun day;                        //  doss, only in u3H (moveme)
        } pro;

        struct {                              //  memoization caches
          u3v4p(u3h_v4_root) har_p;                //  transient
          u3v4p(u3h_v4_root) per_p;                //  persistent
        } cax;
      } u3a_v4_road;
      typedef u3a_v4_road u3_v4_road;

      extern u3a_v4_road* u3a_v4_Road;
#       define u3R_v4  u3a_v4_Road

#     define u3a_v4_to_off(som)  (((som) & 0x3fffffff) << u3a_v4_vits)
#     define u3a_v4_to_ptr(som)  (u3a_v4_into(u3a_v4_to_off(som)))
#     define u3a_v4_to_pug(off)  ((off >> u3a_v4_vits) | 0x80000000)
#     define u3a_v4_to_pom(off)  ((off >> u3a_v4_vits) | 0xc0000000)
#     define u3a_v4_botox(tox_v)  ( (u3a_v4_box *)(void *)(tox_v) - 1 )

//  XX abort() instead of u3m_bail?
//
#     define u3a_v4_head(som) \
        ( _(u3a_v4_is_cell(som)) \
           ? ( ((u3a_v4_cell *)u3a_v4_to_ptr(som))->hed )\
           : u3m_bail(c3__exit) )
#     define u3a_v4_tail(som) \
        ( _(u3a_v4_is_cell(som)) \
           ? ( ((u3a_v4_cell *)u3a_v4_to_ptr(som))->tel )\
           : u3m_bail(c3__exit) )

        void*
        u3a_v4_walloc(c3_v4_w len_w);
        void
        u3a_v4_wfree(void* lag_v);
        void
        u3a_v4_free(void* tox_v);
        void
        u3a_v4_lose(u3_v4_weak som);
        void
        u3a_v4_ream(void);
        c3_o
        u3a_v4_rewrite_ptr(void* ptr_v);
        u3_v4_post
        u3a_v4_rewritten(u3_v4_post som_p);


  /***  jets.h
  ***/
        void
        u3j_v4_rite_lose(u3j_v4_rite* rit_u);
        void
        u3j_v4_site_lose(u3j_v4_site* sit_u);
        void
        u3j_v4_free_hank(u3_v4_noun kev);


  /***  hashtable.h
  ***/

#     define  u3h_v4_buck             u3h_v5_buck
#     define  u3h_v4_node             u3h_v5_node
#     define  u3h_v4_root             u3h_v5_root
#     define  u3h_v4_slot_is_node     u3h_v5_slot_is_node
#     define  u3h_v4_slot_is_noun     u3h_v5_slot_is_noun
#     define  u3h_v4_slot_is_null     u3h_v5_slot_is_null
#     define  u3h_v4_noun_to_slot     u3h_v5_noun_to_slot
#     define  u3h_v4_slot_to_noun     u3h_v5_slot_to_noun

        void
        u3h_v4_free(u3v4p(u3h_v4_root) har_p);
        u3v4p(u3h_v4_root)
        u3h_v4_new(void);
        u3v4p(u3h_v4_root)
        u3h_v4_new_cache(c3_v4_w max_w);
        void
        u3h_v4_walk(u3v4p(u3h_v4_root) har_p, void (*fun_f)(u3_v4_noun));
        void
        u3h_v4_walk_with(u3v4p(u3h_v4_root) har_p,
                      void (*fun_f)(u3_v4_noun, void*),
                      void* wit);

  /***  manage.h
  ***/
        void
        u3m_v4_reclaim(void);

  /***  nock.h
  ***/
#      define  u3n_v4_memo  u3n_v5_memo

  /***  vortex.h
  ***/
      typedef struct __attribute__((__packed__)) _u3v_v4_arvo {
        c3_d  eve_d;                      //  event number
        u3_v4_noun yot;                      //  cached gates
        u3_v4_noun now;                      //  current time
        u3_v4_noun roc;                      //  kernel core
      } u3v_v4_arvo;

      typedef c3_v4_w  u3v_v4_version;

    /* u3v_home: all internal (within image) state.
    **       NB: version must be last for discriminability in north road
    */
      typedef struct _u3v_v4_home {
        u3a_v4_road    rod_u;                //  storage state
        u3v_v4_arvo    arv_u;                //  arvo state
        u3v_v4_version ver_w;                //  version number
      } u3v_v4_home;

extern u3v_v4_home* u3v_v4_Home;
#       define u3H_v4  u3v_v4_Home
#       define u3A_v4  (&(u3v_v4_Home->arv_u))


  /***  init
  ***/
        void
        u3_v4_load(c3_z wor_i);

#endif /* U3_V4_H */
