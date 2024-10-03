#ifndef U3_ALLOCATE_V2_H
#define U3_ALLOCATE_V2_H

#include "pkg/noun/allocate.h"

#include "pkg/noun/v2/manage.h"
#include "options.h"

  /**  Aliases.
  **/
#     define u3a_v2_botox        u3a_botox
#     define u3a_v2_box          u3a_box
#     define u3a_v2_cell         u3a_cell
#     define u3a_v2_fbox         u3a_fbox
#     define u3a_v2_fbox_no      u3a_fbox_no
#     define u3a_v2_free         u3a_free
#     define u3a_v2_heap         u3a_heap
#     define u3a_v2_into         u3a_into
#     define u3a_v2_is_cat       u3a_is_cat
#     define u3a_v2_is_cell      u3a_is_cell
#     define u3a_v2_is_north     u3a_is_north
#     define u3a_v2_is_pom       u3a_is_pom
#     define u3a_v2_is_pug       u3a_is_pug
#     define u3a_v2_lose         u3a_lose
#     define u3a_v2_malloc       u3a_malloc
#     define u3a_v2_minimum      u3a_minimum
#     define u3a_v2_outa         u3a_outa
#     define u3a_v2_pack_seek    u3a_pack_seek
#     define u3a_v2_rewrite      u3a_rewrite
#     define u3a_v2_rewrite_ptr  u3a_rewrite_ptr
#     define u3a_v2_rewritten    u3a_rewritten
#     define u3a_v2_to_off       u3a_to_off
#     define u3a_v2_to_ptr       u3a_to_ptr
#     define u3a_v2_to_wtr       u3a_to_wtr
#     define u3a_v2_to_pug       u3a_to_pug
#     define u3a_v2_to_pom       u3a_to_pom
#     define u3a_v2_wfree        u3a_wfree

  /**  Data structures.
  **/
    /* u3a_v2_road: contiguous allocation and execution context.
    */
      typedef struct _u3a_v2_road {
        u3p(struct _u3a_v2_road) par_p;          //  parent road
        u3p(struct _u3a_v2_road) kid_p;          //  child road list
        u3p(struct _u3a_v2_road) nex_p;          //  sibling road

        u3p(c3_w) cap_p;                      //  top of transient region
        u3p(c3_w) hat_p;                      //  top of durable region
        u3p(c3_w) mat_p;                      //  bottom of transient region
        u3p(c3_w) rut_p;                      //  bottom of durable region
        u3p(c3_w) ear_p;                      //  original cap if kid is live

        c3_w fut_w[32];                       //  futureproof buffer

        struct {                              //  escape buffer
          union {
            jmp_buf buf;
            c3_w buf_w[256];                  //  futureproofing
          };
        } esc;

        struct {                              //  miscellaneous config
          c3_w fag_w;                         //  flag bits
        } how;                                //

        struct {                                   //  allocation pools
          u3p(u3a_v2_fbox) fre_p[u3a_v2_fbox_no];  //  heap by node size log
          u3p(u3a_fbox) cel_p;                //  custom cell allocator
          c3_w fre_w;                         //  number of free words
          c3_w max_w;                         //  maximum allocated
        } all;

        u3a_jets jed;                         //  jet dashboard

        struct {                              // bytecode state
          u3p(u3h_root) har_p;                // formula->post of bytecode
        } byc;

        struct {                              //  namespace
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

        struct {                              //  memoization
          u3p(u3h_root) har_p;                //  (map (pair term noun) noun)
        } cax;
      } u3a_v2_road;

  /**  Globals.
  **/
      /// Current road (thread-local).
      extern u3a_v2_road* u3a_v2_Road;
#       define u3R_v2  u3a_v2_Road

  /**  Functions.
  **/
    /**  Allocation.
    **/
      /* Reference and arena control.
      */
        /* u3a_v2_mig_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
        */
          void
          u3a_v2_mig_rewrite_compact(void);

        /* u3a_v2_rewrite_noun(): rewrite a noun for compaction.
        */
          void
          u3a_v2_rewrite_noun(u3_noun som);

        /* u3a_v2_rewritten(): rewrite a pointer for compaction.
        */
          u3_post
          u3a_v2_rewritten(u3_post som_p);

        /* u3a_v2_rewritten(): rewritten noun pointer for compaction.
        */
          u3_noun
          u3a_v2_rewritten_noun(u3_noun som);

#endif /* ifndef U3_ALLOCATE_V2_H */
