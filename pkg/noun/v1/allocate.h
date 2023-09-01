#ifndef U3_ALLOCATE_V1_H
#define U3_ALLOCATE_V1_H

#include "pkg/noun/allocate.h"
#include "pkg/noun/v2/allocate.h"

  /**  Aliases.
  **/
#     define u3R_v1         u3R_v2
#     define u3a_v1_botox   u3a_v2_botox
#     define u3a_v1_box     u3a_v2_box
#     define u3a_v1_cell    u3a_v2_cell
#     define u3a_v1_free    u3a_v2_free
#     define u3a_v1_into    u3a_v2_into
#     define u3a_v1_is_cat  u3a_v2_is_cat
#     define u3a_v1_is_pom  u3a_is_pom
#     define u3a_v1_wfree   u3a_v2_wfree

  /**  Structures.
  **/
  /**  Macros.  Should be better commented.
  **/
    /* Inside a noun.
    */
    /* u3a_v1_to_off(): mask off bits 30 and 31 from noun [som].
    */
#     define u3a_v1_to_off(som)    ((som) & 0x3fffffff)

    /* u3a_v1_to_ptr(): convert noun [som] into generic pointer into loom.
    */
#     define u3a_v1_to_ptr(som)    (u3a_v1_into(u3a_v1_to_off(som)))

    /* u3a_v1_to_wtr(): convert noun [som] into word pointer into loom.
    */
#     define u3a_v1_to_wtr(som)    ((c3_w *)u3a_v1_to_ptr(som))

    /* u3a_v1_to_pug(): set bit 31 of [off].
    */
#     define u3a_v1_to_pug(off)    (off | 0x80000000)

    /* u3a_v1_to_pom(): set bits 30 and 31 of [off].
    */
#     define u3a_v1_to_pom(off)    (off | 0xc0000000)

  /**  Functions.
  **/
    /**  Allocation.
    **/
      /* Reference and arena control.
      */
        /* u3a_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
        */
          void
          u3a_v1_reclaim(void);

        /* u3a_v1_lose(): lose a reference count.
        */
          void
          u3a_v1_lose(u3_noun som);

#endif /* ifndef U3_ALLOCATE_H */
