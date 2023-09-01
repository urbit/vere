#ifndef U3_ALLOCATE_V2_H
#define U3_ALLOCATE_V2_H

#include "pkg/noun/allocate.h"

#include "pkg/noun/v2/manage.h"
#include "options.h"

  /**  Aliases.
  **/
#     define u3R_v2              u3R
#     define u3a_v2_botox        u3a_botox
#     define u3a_v2_box          u3a_box
#     define u3a_v2_cell         u3a_cell
#     define u3a_v2_fbox_no      u3a_fbox_no
#     define u3a_v2_free         u3a_free
#     define u3a_v2_heap         u3a_heap
#     define u3a_v2_into         u3a_into
#     define u3a_v2_is_cat       u3a_is_cat
#     define u3a_v2_is_cell      u3a_is_cell
#     define u3a_v2_is_pug       u3a_is_pug
#     define u3a_v2_malloc       u3a_malloc
#     define u3a_v2_outa         u3a_outa
#     define u3a_v2_pack_seek    u3a_pack_seek
#     define u3a_v2_rewrite      u3a_rewrite
#     define u3a_v2_rewrite_ptr  u3a_rewrite_ptr
#     define u3a_v2_rewritten    u3a_rewritten
#     define u3a_v2_road         u3a_road
#     define u3a_v2_wfree        u3a_wfree
#     define u3a_v2_to_off       u3a_to_off
#     define u3a_v2_to_ptr       u3a_to_ptr
#     define u3a_v2_to_wtr       u3a_to_wtr
#     define u3a_v2_to_pug       u3a_to_pug
#     define u3a_v2_to_pom       u3a_to_pom

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