#ifndef U3_V5_H
#define U3_V5_H

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"
// #include "v6.h"

  /***  current
  ***/

#     define  u3a_v5_is_north     u3a_is_north
#     define  u3a_v5_heap         u3a_heap
#     define  u3a_v5_hunk_dose    u3a_hunk_dose

//  XX
#     define  u3i_v5_cell         u3i_cell

//  XX?
#     define  u3r_v5_mug          u3r_mug
#     define  u3r_v5_mug_both     u3r_mug_both
#     define  u3r_v5_mug_words    u3r_mug_words

//  XX
#     define  u3j_v5_boot         u3j_boot
#     define  u3j_v5_hank         u3j_hank
#     define  u3j_v5_ream         u3j_ream
#     define  u3j_v5_rite         u3j_rite
#     define  u3j_v5_site         u3j_site

  /***  c3/defs.h
  ***/
#     define c3_v5_wiseof(x)      (((sizeof (x)) + 3) >> 2)
#     define c3_v5_bits_word(w)   ((w) ? (32 - c3_lz_w(w)) : 0)
#if   (32 == (CHAR_BIT * __SIZEOF_INT__))
#     define c3_v5_lz_w __builtin_clz
#     define c3_v5_tz_w __builtin_ctz
#     define c3_v5_pc_w __builtin_popcount
#elif (32 == (CHAR_BIT * __SIZEOF_LONG__))
#     define c3_v5_lz_w __builtin_clzl
#     define c3_v5_tz_w __builtin_ctzl
#     define c3_v5_pc_w __builtin_popcountl
#else
#     error  "port me"
#endif

  /***  c3/types.h
  ***/
      typedef uint32_t c3_v5_l;
      typedef uint32_t c3_v5_w;
      typedef int32_t c3_v5_ws;

  /***  noun/types.h
  ***/
      typedef c3_v5_w             u3_v5_noun;
      typedef u3_v5_noun          u3_v5_atom;
      typedef u3_v5_noun          u3_v5_cell;
      typedef u3_v5_noun          u3_v5_weak;
#     define  u3_v5_none         (u3_v5_noun)0xffffffff
      typedef c3_v5_w             u3_v5_post;
#     define  u3v5p(type)         u3_v5_post


  /***  allocate.h
  ***/
#     define  u3_Loom_v5           (((c3_v5_w *)(void *)U3_OS_LoomBase) + ((c3_z)1 << u3a_v5_bits_max))
#     define  u3a_v5_vits          2
#     define  u3a_v5_bits_max      (8 * sizeof(c3_v5_w) + u3a_v5_vits)
#     define  u3a_v5_walign        (1 << u3a_v5_vits)
#     define  u3a_v5_balign        (sizeof(c3_v5_w)*u3a_v5_walign)
#     define  u3a_v5_into(x)       ((void *)(u3_Loom_v5 + (x)))
#     define  u3a_v5_outa(p)       ((c3_v5_w *)(void *)(p) - u3_Loom_v5)
#     define  u3v5to(type, x)      ((type *)u3a_v5_into(x))
#     define  u3v5of(type, x)      (u3a_v5_outa((type*)x))
#     define  u3a_v5_is_pom(som)   ((0b11 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_v5_is_cat(som)   (((som) >> 31) ? c3n : c3y)
#     define  u3a_v5_is_cell(som)  u3a_v5_is_pom(som)
#     define  u3a_v5_is_pug(som)   ((0b10 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_v5_is_atom(som)  c3o(u3a_v5_is_cat(som), u3a_v5_is_pug(som))
#     define  u3a_v5_to_off(som)   (((som) & 0x3fffffff) << u3a_v5_vits)
#     define  u3a_v5_to_ptr(som)   (u3a_v5_into(u3a_v5_to_off(som)))
#     define  u3v5ud(som)          u3a_is_atom(som)
#     define  u3a_v5_page          12ULL
#     define  u3v5to(type, x)      ((type *)u3a_v5_into(x))
#     define  u3v5tn(type, x)      (x) ? (type*)u3a_v5_into(x) : (void*)NULL
#     define  u3a_v5_min_log       2
#     define  u3a_v5_free_pg       (u3v5p(u3a_v5_crag))0
#     define  u3a_v5_head_pg       (u3v5p(u3a_v5_crag))1
#     define  u3a_v5_rest_pg       (u3v5p(u3a_v5_crag))2
#     define  u3a_v5_crag_no       (u3a_v5_page - u3a_v5_min_log)
#     define  u3a_v5_minimum       ((c3_v5_w)c3_v5_wiseof(u3a_v5_cell))
#     define  u3v5z(som)           u3a_v5_lose(som)
#     define  u3v5k(som)           u3a_v5_gain(som)

#     define  u3a_v5_north_is_senior(r, dog) \
                __((u3a_v5_to_off(dog) < (r)->rut_p) ||  \
                       (u3a_v5_to_off(dog) >= (r)->mat_p))

#     define  u3a_v5_north_is_junior(r, dog) \
                __((u3a_v5_to_off(dog) >= (r)->cap_p) && \
                       (u3a_v5_to_off(dog) < (r)->mat_p))

#     define  u3a_v5_north_is_normal(r, dog) \
                c3a(!(u3a_v5_north_is_senior(r, dog)),  \
                       !(u3a_v5_north_is_junior(r, dog)))

#     define  u3a_v5_south_is_senior(r, dog) \
                __((u3a_v5_to_off(dog) < (r)->mat_p) || \
                       (u3a_v5_to_off(dog) >= (r)->rut_p))

#     define  u3a_v5_south_is_junior(r, dog) \
                __((u3a_v5_to_off(dog) < (r)->cap_p) && \
                       (u3a_v5_to_off(dog) >= (r)->mat_p))

#     define  u3a_v5_south_is_normal(r, dog) \
                c3a(!(u3a_v5_south_is_senior(r, dog)),  \
                       !(u3a_v5_south_is_junior(r, dog)))

#     define  u3a_v5_is_senior(r, som) \
                ( _(u3a_v5_is_cat(som)) \
                      ?  c3y \
                      :  _(u3a_v5_is_north(r)) \
                         ?  u3a_v5_north_is_senior(r, som) \
                         :  u3a_v5_south_is_senior(r, som) )

      extern u3a_v5_hunk_dose u3a_v5_Hunk[u3a_v5_crag_no];

      u3_v5_weak
      u3a_v5_gain(u3_v5_weak som);

      void
      u3a_v5_lose(u3_v5_noun som);

      void*
      u3a_v5_walloc(c3_v5_w len_w);

      void
      u3a_v5_wed(u3_v5_noun *restrict a, u3_v5_noun *restrict b);

      inline c3_v5_w u3a_v5_to_pug(c3_v5_w off) {
        c3_dessert((off & u3a_v5_walign-1) == 0);
        return (off >> u3a_v5_vits) | 0x80000000;
      }

      typedef struct {
        c3_v5_w use_w;
        c3_v5_w mug_w;
      } u3a_v5_noun;

      typedef struct {
        c3_v5_w use_w;
        c3_v5_w mug_w;
        c3_v5_w len_w;
        c3_v5_w buf_w[0];
      } u3a_v5_atom;

      typedef struct {
        c3_v5_w    use_w;
        c3_v5_w    mug_w;
        u3_v5_noun hed;
        u3_v5_noun tel;
      } u3a_v5_cell;

      typedef struct _u3a_v5_crag {
        u3v5p(struct _u3a_v5_crag) nex_p;     //  next
        c3_v5_w               pag_w;     //  page index
        c3_s                  log_s;     //  size log2
        c3_s                  fre_s;     //  free chunks
        c3_v5_w               map_w[1];  //  free-chunk bitmap
      } u3a_v5_crag;

      typedef struct _u3a_v5_dell {
        u3p(struct _u3a_v5_dell) nex_p;     //  next
        u3p(struct _u3a_v5_dell) pre_p;     //  prev
        c3_v5_w               pag_w;     //  page index
        c3_v5_w               siz_w;     //  number of pages
      } u3a_v5_dell;

          /* u3a_road: contiguous allocation and execution context.
    */
      typedef struct _u3a_v5_road {
        u3v5p(struct _u3a_v5_road) par_p;          //  parent road
        u3v5p(struct _u3a_v5_road) kid_p;          //  child road list
        u3v5p(struct _u3a_v5_road) nex_p;          //  sibling road

        u3v5p(c3_v5_w) cap_p;                      //  top of transient region
        u3v5p(c3_v5_w) hat_p;                      //  top of durable region
        u3v5p(c3_v5_w) mat_p;                      //  bottom of transient region
        u3v5p(c3_v5_w) rut_p;                      //  bottom of durable region
        u3v5p(c3_v5_w) ear_p;                      //  original cap if kid is live

        c3_v5_w off_w;                           //  spin stack offset
        c3_v5_w fow_w;                           //  spin stack overflow count
        u3v5p(u3h_v5_root) lop_p;                  //  %loop hint set
        u3_v5_noun tim;                          //  list of absolute deadlines

        c3_v5_w fut_w[28];                       //  futureproof buffer

        struct {                              //  escape buffer
          union {
            jmp_buf buf;
            c3_v5_w buf_w[256];                  //  futureproofing
          };
        } esc;

        struct {                              //  miscellaneous config
          c3_v5_w fag_w;                         //  flag bits
        } how;                                //

        //  XX re/move
        struct {                              //  allocation pools
          c3_v5_w fre_w;                         //  number of free words
          c3_v5_w max_w;                         //  maximum allocated
        } all;

        struct {                              //    heap allocator
          u3v5p(u3a_v5_dell)  fre_p;                  //  free list entry
          u3v5p(u3a_v5_dell)  erf_p;                  //  free list exit
          u3v5p(u3a_v5_dell)  cac_p;                  //  cached pgfree struct
          c3_v5_ws            dir_ws;                 //  1 || -1 (multiplicand for local offsets)
          c3_v5_ws            off_ws;                 //  0 || -1 (word-offset for hat && rut)
          c3_v5_w             siz_w;                  //  directory size
          c3_v5_w             len_w;                  //  directory entries
          u3v5p(u3a_v5_crag*) pag_p;                  //  directory
          u3v5p(u3a_v5_crag)  wee_p[u3a_v5_crag_no];  //  chunk lists
        } hep;

        struct {                              //    cell pool
          u3_v5_post cel_p;                      //  array of cells
          c3_v5_w    hav_w;                      //  length
          c3_v5_w    bat_w;                      //  batch counter
        } cel;

        u3a_jets jed;                         //  jet dashboard

        struct {                              //  bytecode state
          u3v5p(u3h_v5_root) har_p;                //  formula->post of bytecode
        } byc;

        struct {                              //  scry namespace
          u3_v5_noun gul;                        //  (list $+(* (unit (unit)))) now
        } ski;

        struct {                              //  trace stack
          u3_v5_noun tax;                        //  (list ,*)
          u3_v5_noun mer;                        //  emergency buffer to release
        } bug;

        struct {                              //  profile stack
          c3_d    nox_d;                      //  nock steps
          c3_d    cel_d;                      //  cell allocations
          u3_v5_noun don;                        //  (list batt)
          u3_v5_noun trace;                      //  (list trace)
          u3_v5_noun day;                        //  doss, only in u3H (moveme)
        } pro;

        struct {                              //  memoization caches
          u3v5p(u3h_v5_root) har_p;                //  transient
          u3v5p(u3h_v5_root) per_p;                //  persistent
        } cax;
      } u3a_v5_road;
      typedef u3a_v5_road u3_v5_road;

      extern u3a_v5_road* u3a_v5_Road;
#       define u3R_v5  u3a_v5_Road

      typedef struct _u3a_v5_pile {
        c3_v5_ws    mov_ws;
        c3_v5_ws    off_ws;
        u3_v5_post   top_p;
      } u3a_v5_pile;

      inline void
      u3a_v5_drop(const u3a_v5_pile* pil_u)
      {
        u3R_v5->cap_p -= pil_u->mov_ws;
      }

      inline void*
      u3a_v5_peek(const u3a_v5_pile* pil_u)
      {
        return u3v5to(void, (u3R_v5->cap_p + pil_u->off_ws));
      }

      inline void*
      u3a_v5_pop(const u3a_v5_pile* pil_u)
      {
        u3a_v5_drop(pil_u);
        return u3a_v5_peek(pil_u);
      }

      inline void*
      u3a_v5_push(const u3a_v5_pile* pil_u)
      {
        u3R_v5->cap_p += pil_u->mov_ws;

#ifndef U3_GUARD_PAGE
        //  !off means we're on a north road
        //
        if ( !pil_u->off_ws ) {
          if( !(u3R_v5->cap_p > u3R_v5->hat_p) ) {
            u3m_bail(c3__meme);
          }
        }
        else {
          if( !(u3R_v5->cap_p < u3R_v5->hat_p) ) {
            u3m_bail(c3__meme);
          }
        }
#endif /* ifndef U3_GUARD_PAGE */

        return u3a_v5_peek(pil_u);
      }

      inline c3_o
      u3a_v5_pile_done(const u3a_v5_pile* pil_u)
      {
        return (pil_u->top_p == u3R_v5->cap_p) ? c3y : c3n;
      }

      void
      u3a_v5_pile_prep(u3a_v5_pile* pil_u, c3_v5_w len_w);

  /***  hashtable.h
  ***/
        typedef c3_v5_w u3h_v5_slot;

        typedef struct {
          c3_v5_w     map_w;     // bitmap for [sot_w]
          u3h_v5_slot sot_w[];   // filled slots
        } u3h_v5_node;

        typedef struct {
          c3_v5_w     max_w;     // number of cache lines (0 for no trimming)
          c3_v5_w     use_w;     // number of lines currently filled
          struct {
            c3_v5_w  mug_w;      // current hash
            c3_v5_w  inx_w;      // index into current hash bucket
            c3_o  buc_o;      // XX remove
          } arm_u;            // clock arm
          u3h_v5_slot sot_w[64]; // slots
        } u3h_v5_root;  

        typedef struct {
          c3_v5_w     len_w;     // length of [sot_w]
          u3h_v5_slot sot_w[];   // filled slots
        } u3h_v5_buck;

#     define  u3h_v5_slot_is_null(sot)  ((0 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_v5_slot_is_node(sot)  ((1 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_v5_slot_is_noun(sot)  ((1 == ((sot) >> 31)) ? c3y : c3n)
#     define  u3h_v5_slot_to_noun(sot)  (0x40000000 | (sot))
#     define  u3h_v5_noun_to_slot(som)  (u3h_v5_noun_be_warm(som))
#     define  u3h_v5_noun_be_warm(sot)  ((sot) | 0x40000000)
#     define  u3h_v5_slot_to_node(sot)  (u3a_v5_into(((sot) & 0x3fffffff) << u3a_v5_vits))
#     define  u3h_v5_node_to_slot(ptr)  ((u3a_v5_outa((ptr)) >> u3a_v5_vits) | 0x40000000)
#     define  u3h_v5_slot_is_warm(sot)  (((sot) & 0x40000000) ? c3y : c3n)

        u3v5p(u3h_v5_root)
        u3h_v5_new_cache(c3_v5_w clk_w);

        u3v5p(u3h_v5_root)
        u3h_v5_new(void);

        void
        u3h_v5_put(u3v5p(u3h_v5_root) har_p, u3_v5_noun key, u3_v5_noun val);

        u3_v5_weak
        u3h_v5_git(u3v5p(u3h_v5_root) har_p, u3_v5_noun key);

        void
        u3h_v5_free(u3v5p(u3h_v5_root) har_p);

  /***  imprison.h
  ***/
#     define u3v5nc(a, b) u3i_v5_cell(a, b)

  /***  jets.h
  ***/
      typedef struct {
        u3_v5_noun bat;                  //  battery
        u3_v5_noun pax;                  //  parent axis
      } u3j_v5_fist;

      typedef struct {
        c3_v5_w    len_w;                //  number of fists
        u3_v5_noun sat;                  //  static noun at end of check
        u3j_v5_fist fis_u[];             //  fists
      } u3j_v5_fink;

  /***  nock.h
  ***/
  typedef struct {
    c3_v5_l    sip_l;
    u3_v5_noun key;
    u3z_cid cid;
  } u3n_v5_memo;

  typedef struct _u3n_v5_prog {
    struct {
      c3_o      own_o;                // program owns ops_y?
      c3_v5_w      len_w;                // length of bytecode (bytes)
      c3_y*     ops_y;                // actual array of bytes
    } byc_u;                          // bytecode
    struct {
      c3_v5_w      len_w;                // number of literals
      u3_v5_noun*  non;                  // array of literals
    } lit_u;                          // literals
    struct {
      c3_v5_w      len_w;                // number of memo slots
      u3n_v5_memo* sot_u;                // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_v5_w      len_w;                // number of calls sites
      u3j_v5_site* sit_u;                // array of sites
    } cal_u;                          // call site data
    struct {
      c3_v5_w      len_w;                // number of registration sites
      u3j_v5_rite* rit_u;                // array of sites
    } reg_u;                          // registration site data
  } u3n_v5_prog;

  /***  retrieve.h
  ***/
  c3_o
  u3r_v5_sing(u3_v5_noun a, u3_v5_noun b);

  /***  vortex.h
  ***/
      typedef struct __attribute__((__packed__)) _u3v_v5_arvo {
        c3_d  eve_d;                      //  event number
        u3_v5_noun yot;                      //  cached gates
        u3_v5_noun now;                      //  current time
        u3_v5_noun roc;                      //  kernel core
      } u3v_v5_arvo;

      typedef c3_v5_w  u3v_v5_version;

      typedef struct _u3v_v5_home {
        u3a_v5_road    rod_u;                //  storage state
        u3v_v5_arvo    arv_u;                //  arvo state
        u3v_v5_version ver_w;                //  version number
      } u3v_v5_home;

extern u3v_v5_home* u3v_v5_Home;
#       define u3H_v5  u3v_v5_Home
#       define u3A_v5  (&(u3v_v5_Home->arv_u))

#endif /* U3_V5_H */
