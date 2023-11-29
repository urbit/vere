/// @file

#ifndef U3_ZAVE_H
#define U3_ZAVE_H

#include "c3.h"
#include "types.h"

  /**  Memoization.
  ***
  ***  The memo cache is keyed by an arbitrary symbolic function
  ***  and a noun argument to that (logical) function.  Functions
  ***  are predefined by C-level callers, but 0 means nock.
  ***
  ***  Memo functions RETAIN keys and transfer values.
  **/
    /* u3z_cid: cache id
    */
      typedef enum {
        u3z_memo_toss  = 0,
        u3z_memo_keep  = 1,
        // u3z_memo_ford  = 2,
        // u3z_memo_ames  = 3,
        // ...
      } u3z_cid;

    /* u3z_key*(): construct a memo cache-key.  Arguments retained.
    */
      u3_noun u3z_key(c3_m, u3_noun);
      u3_noun u3z_key_2(c3_m, u3_noun, u3_noun);
      u3_noun u3z_key_3(c3_m, u3_noun, u3_noun, u3_noun);
      u3_noun u3z_key_4(c3_m, u3_noun, u3_noun, u3_noun, u3_noun);
      u3_noun u3z_key_5(c3_m, u3_noun, u3_noun, u3_noun, u3_noun, u3_noun);

    /* u3z_find*(): find in memo cache. Arguments retained
    */
      u3_weak u3z_find(u3z_cid cid, u3_noun key);
      u3_weak u3z_find_m(u3z_cid cid, c3_m fun_m, u3_noun one);

    /* u3z_save(): save in memo cache. TRANSFER key; RETAIN val;
    */
      u3_noun u3z_save(u3z_cid cid, u3_noun key, u3_noun val);

    /* u3z_save_m(): save in memo cache. Arguments retained
    */
      u3_noun u3z_save_m(u3z_cid cid, c3_m fun_m, u3_noun one, u3_noun val);

    /* u3z_uniq(): uniquify with memo cache.
    */
      u3_noun
      u3z_uniq(u3z_cid cid, u3_noun som);

    /* u3z_reap(): promote persistent memoization cache.
    */
      void
      u3z_reap(u3p(u3h_root) per_p);

    /* u3z_free(): free memoization cache.
    */
      void
      u3z_free(u3z_cid cid);

    /* u3z_ream(): refresh after restoring from checkpoint.
    */
      void
      u3z_ream(u3z_cid cid);

#endif /* ifndef U3_ZAVE_H */
