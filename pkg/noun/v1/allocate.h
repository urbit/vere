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

#endif /* ifndef U3_ALLOCATE_H */
