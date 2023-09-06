#ifndef U3_ALLOCATE_V1_H
#define U3_ALLOCATE_V1_H

#include "pkg/noun/allocate.h"

#include "error.h"
#include "manage.h"
#include "options.h"

  /**  Constants.
  **/

  /**  Structures.
  **/
    /* u3a_v1_road: contiguous allocation and execution context.
    */
      typedef struct _u3a_v1_road {
        u3p(struct _u3a_v1_road) par_p;          //  parent road
        u3p(struct _u3a_v1_road) kid_p;          //  child road list
        u3p(struct _u3a_v1_road) nex_p;          //  sibling road

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

        struct {                              //  allocation pools
          u3p(u3a_v1_fbox) fre_p[u3a_fbox_no];   //  heap by node size log
          u3p(u3a_v1_fbox) cel_p;                //  custom cell allocator
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
      } u3a_v1_road;
      typedef u3a_v1_road u3_v1_road;

  /**  Macros.  Should be better commented.
  **/
    /* Inside a noun.
    */

    /* u3a_to_off(): mask off bits 30 and 31 from noun [som].
    */
#     define u3a_v1_to_off(som)    ((som) & 0x3fffffff)

    /* u3a_v1_to_ptr(): convert noun [som] into generic pointer into loom.
    */
#     define u3a_v1_to_ptr(som)    (u3a_into(u3a_v1_to_off(som)))

    /* u3a_v1_to_wtr(): convert noun [som] into word pointer into loom.
    */
#     define u3a_v1_to_wtr(som)    ((c3_w *)u3a_v1_to_ptr(som))

    /* u3a_v1_to_pug(): set bit 31 of [off].
    */
#     define u3a_v1_to_pug(off)    (off | 0x80000000)

    /* u3a_v1_to_pom(): set bits 30 and 31 of [off].
    */
#     define u3a_v1_to_pom(off)    (off | 0xc0000000)

    /* u3a_v1_h(): get head of cell [som]. Bail if [som] is not cell.
    */
#     define u3a_v1_h(som) \
        ( _(u3a_v1_is_cell(som)) \
           ? ( ((u3a_v1_cell *)u3a_v1_to_ptr(som))->hed )\
           : u3m_bail(c3__exit) )

    /* u3a_v1_t(): get tail of cell [som]. Bail if [som] is not cell.
    */
#     define u3a_v1_t(som) \
        ( _(u3a_v1_is_cell(som)) \
           ? ( ((u3a_v1_cell *)u3a_v1_to_ptr(som))->tel )\
           : u3m_bail(c3__exit) )

#     define  u3a_v1_north_is_senior(r, dog) \
                __((u3a_v1_to_off(dog) < (r)->rut_p) ||  \
                       (u3a_v1_to_off(dog) >= (r)->mat_p))

#     define  u3a_v1_north_is_junior(r, dog) \
                __((u3a_v1_to_off(dog) >= (r)->cap_p) && \
                       (u3a_v1_to_off(dog) < (r)->mat_p))

#     define  u3a_v1_north_is_normal(r, dog) \
                c3a(!(u3a_v1_north_is_senior(r, dog)),  \
                       !(u3a_v1_north_is_junior(r, dog)))

#     define  u3a_v1_south_is_senior(r, dog) \
                __((u3a_v1_to_off(dog) < (r)->mat_p) || \
                       (u3a_v1_to_off(dog) >= (r)->rut_p))

#     define  u3a_v1_south_is_junior(r, dog) \
                __((u3a_v1_to_off(dog) < (r)->cap_p) && \
                       (u3a_v1_to_off(dog) >= (r)->mat_p))

#     define  u3a_v1_south_is_normal(r, dog) \
                c3a(!(u3a_v1_south_is_senior(r, dog)),  \
                       !(u3a_v1_south_is_junior(r, dog)))

#     define  u3a_v1_is_junior(r, som) \
                ( _(u3a_v1_is_cat(som)) \
                      ?  c3n \
                      :  _(u3a_is_north(r)) \
                         ?  u3a_v1_north_is_junior(r, som) \
                         :  u3a_v1_south_is_junior(r, som) )

#     define  u3a_v1_is_senior(r, som) \
                ( _(u3a_v1_is_cat(som)) \
                      ?  c3y \
                      :  _(u3a_is_north(r)) \
                         ?  u3a_v1_north_is_senior(r, som) \
                         :  u3a_v1_south_is_senior(r, som) )

#     define  u3a_v1_is_mutable(r, som) \
                ( _(u3a_v1_is_atom(som)) \
                  ? c3n \
                  : _(u3a_v1_is_senior(r, som)) \
                  ? c3n \
                  : _(u3a_v1_is_junior(r, som)) \
                  ? c3n \
                  : (u3a_v1_botox(u3a_v1_to_ptr(som))->use_w == 1) \
                  ? c3y : c3n )



  /**  Globals.
  **/
      /// Current road (thread-local).
      extern u3_v1_road* u3a_v1_Road;
#       define u3R_v1  u3a_v1_Road

  /**  Functions.
  **/
    /**  Allocation.
    **/
      /* Word-aligned allocation.
      */
        /* u3a_v1_walloc(): allocate storage measured in words.
        */
          void*
          u3a_v1_walloc(void);

        /* u3a_v1_celloc(): allocate a cell.  Faster, sometimes.
        */
          c3_w*
          u3a_v1_celloc(void);

        /* u3a_v1_wtrim(): trim storage.
        */
          void
          u3a_v1_wtrim(void* tox_v, c3_w old_w, c3_w len_w);

        /* u3a_v1_wealloc(): word realloc.
        */
          void*
          u3a_v1_wealloc(void* lag_v, c3_w len_w);

      /* C-style aligned allocation - *not* compatible with above.
      */
        /* u3a_v1_malloc(): aligned storage measured in bytes.
        */
          void*
          u3a_v1_malloc(size_t len_i);

        /* u3a_v1_calloc(): aligned storage measured in bytes.
        */
          void*
          u3a_v1_calloc(size_t num_i, size_t len_i);

        /* u3a_v1_realloc(): aligned realloc in bytes.
        */
          void*
          u3a_v1_realloc(void* lag_v, size_t len_i);

        /* u3a_v1_free(): free for aligned malloc.
        */
          void
          u3a_v1_free(void* tox_v);

        /* u3a_v1_mark_noun(): mark a noun for gc.  Produce size.
        */
          c3_w
          u3a_v1_mark_noun(u3_noun som);

        /* u3a_v1_mark_road(): mark ad-hoc persistent road structures.
        */
          c3_w
          u3a_v1_mark_road(FILE* fil_u);

        /* u3a_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
        */
          void
          u3a_v1_reclaim(void);

        /* u3a_v1_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
        */
          void
          u3a_v1_rewrite_compact(void);

        /* u3a_v1_rewrite_noun(): rewrite a noun for compaction.
        */
          void
          u3a_v1_rewrite_noun(u3_noun som);

        /* u3a_v1_rewritten(): rewrite a pointer for compaction.
        */
          u3_post
          u3a_v1_rewritten(u3_post som_p);

        /* u3a_v1_rewritten(): rewritten noun pointer for compaction.
        */
          u3_noun
          u3a_v1_rewritten_noun(u3_noun som);

        /* u3a_v1_discount_noun(): clean up after counting a noun.
        */
          c3_w
          u3a_v1_discount_noun(u3_noun som);

        /* u3a_v1_count_ptr(): count a pointer for gc.  Produce size.  */
          c3_w
          u3a_v1_count_ptr(void* ptr_v);

        /* u3a_v1_discount_ptr(): discount a pointer for gc.  Produce size.  */
          c3_w
          u3a_v1_discount_ptr(void* ptr_v);

        /* u3a_v1_prof(): mark/measure/print memory profile. RETAIN.
        */
          c3_w
          u3a_v1_prof(FILE* fil_u, c3_w den_w, u3_noun mas);

#endif /* ifndef U3_ALLOCATE_H */
