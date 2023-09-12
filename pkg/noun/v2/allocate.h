#ifndef U3_ALLOCATE_V2_H
#define U3_ALLOCATE_V2_H

#define VITS_W 1

#include "pkg/noun/allocate.h"

#include "pkg/noun/v2/manage.h"
#include "options.h"

  /**  Constants.
  **/

  /**  inline functions.
  **/
  /* u3a_v2_to_off(): mask off bits 30 and 31 from noun [som].
   */
  inline c3_w u3a_v2_to_off(c3_w som) {
    return (som & 0x3fffffff) << VITS_W;
  }

  /* u3a_v2_to_ptr(): convert noun [som] into generic pointer into loom.
   */
  inline void *u3a_v2_to_ptr(c3_w som) {
    return u3a_into(u3a_v2_to_off(som));
  }

  /* u3a_v2_to_wtr(): convert noun [som] into word pointer into loom.
   */
  inline c3_w *u3a_v2_to_wtr(c3_w som) {
    return (c3_w *)u3a_v2_to_ptr(som);
  }

  /* u3a_v2_to_pug(): set bit 31 of [off].
   */
  inline c3_w u3a_v2_to_pug(c3_w off) {
    c3_dessert((off & u3C.walign_w-1) == 0);
    return (off >> VITS_W) | 0x80000000;
  }

  /* u3a_v2_to_pom(): set bits 30 and 31 of [off].
   */
  inline c3_w u3a_v2_to_pom(c3_w off) {
    c3_dessert((off & u3C.walign_w-1) == 0);
    return (off >> VITS_W) | 0xc0000000;
  }

  /**  Functions.
  **/
    /**  Allocation.
    **/
      /* Reference and arena control.
      */
        /* u3a_v2_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
        */
          void
          u3a_v2_rewrite_compact(void);

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