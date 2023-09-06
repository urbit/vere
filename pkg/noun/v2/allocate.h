#ifndef U3_ALLOCATE_V2_H
#define U3_ALLOCATE_V2_H

#include "error.h"
#include "pkg/noun/v2/manage.h"
#include "options.h"

  /**  Constants.  XX TODO
  **/

  /**  inline functions.
  **/
  /* u3a_v2_config_loom(): configure loom information by u3v version
   */
  inline void u3a_v2_config_loom(c3_w ver_w) {
    switch (ver_w) {
    case U3V_VER1:
      u3C.vits_w = 0;
      break;
    case U3V_VER2:
      u3C.vits_w = 1;
      break;
    default:
      u3_assert(0);
    }

    u3C.walign_w = 1 << u3C.vits_w;
    u3C.balign_d = sizeof(c3_w) * u3C.walign_w;
  }

  /* u3a_v2_to_off(): mask off bits 30 and 31 from noun [som].
   */
  inline c3_w u3a_v2_to_off(c3_w som) {
    return (som & 0x3fffffff) << u3C.vits_w;
  }

  /* u3a_v2_to_ptr(): convert noun [som] into generic pointer into loom.
   */
  inline void *u3a_v2_to_ptr(c3_w som) {
    return u3a_v2_into(u3a_v2_to_off(som));
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
    return (off >> u3C.vits_w) | 0x80000000;
  }

  /* u3a_v2_to_pom(): set bits 30 and 31 of [off].
   */
  inline c3_w u3a_v2_to_pom(c3_w off) {
    c3_dessert((off & u3C.walign_w-1) == 0);
    return (off >> u3C.vits_w) | 0xc0000000;
  }

  /**  Functions.
  **/
    /**  Allocation.
    **/
      /* Reference and arena control.
      */
        /* u3a_v2_reclaim(): clear ad-hoc persistent caches to reclaim memory.
        */
          void
          u3a_v2_reclaim(void);

        /* u3a_v2_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
        */
          void
          u3a_v2_rewrite_compact(void);

        /* u3a_v2_rewrite_noun(): rewrite a noun for compaction.
        */
          void
          u3a_v2_rewrite_noun(u3_noun som);

#endif /* ifndef U3_ALLOCATE_V2_H */