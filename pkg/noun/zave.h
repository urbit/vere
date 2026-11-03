/// @file

#ifndef U3_ZAVE_H
#define U3_ZAVE_H

#include "c3/c3.h"
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
        u3z_memo_ford  = 2,
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
      u3_weak u3z_find_up(u3_noun key);

    /* u3z_save(): save in memo cache. TRANSFER key; RETAIN val;
    */
      u3_noun u3z_save(u3z_cid cid, u3_noun key, u3_noun val);

    /* u3z_save_m(): save in memo cache. Arguments retained
    */
      u3_noun u3z_save_m(u3z_cid cid, c3_m fun_m, u3_noun one, u3_noun val);

    /* u3z_save_m_dedup(): save in memo cache, deduplicate. All but val is retained.
    */
      u3_noun u3z_save_m_dedup(u3z_cid cid, c3_m fun, u3_noun one, u3_noun val);

    /* u3z_uniq(): uniquify with memo cache.
    */
      u3_noun
      u3z_uniq(u3z_cid cid, u3_noun som);

    /* u3z_reap(): promote memoization cache state.
    */
      void
      u3z_reap(u3z_cid cid, u3p(u3h_root) har_p);

    /* u3z_free(): free memoization cache.
    */
      void
      u3z_free(u3z_cid cid);

    /* u3z_ream(): refresh after restoring from checkpoint.
    */
      void
      u3z_ream(u3z_cid cid);

    /* u3z_ut_*(): transient memoization for the +ut jets.
    **
    **   The +ut door threads fan, the set of holds +rest is expanding, down
    **   every type computation.  Only +rest and +redo read it, both by
    **   membership tests, so a result depends on fan exactly through the
    **   tests made below it.  Each wrapper frame collects the tests of its
    **   subtree, pos (members) and neg (non-members), less those on a hold
    **   the subtree itself expands, and saves them with its result.  A
    **   cached result is reused only under a fan that agrees with every one.
    */
      //  kinds of frame: +rest extends fan.  A top frame keeps no tests and
      //  is saved as valid under the empty fan only: +mint and +mull, never
      //  called under +play and so never under a non-empty fan, and the
      //  other wrappers when their fan is empty, since their reuse across
      //  fans goes through the +rest and +redo frames below them anyway.
      //
      enum {
        u3z_ut_norm = 0,
        u3z_ut_rest = 1,
        u3z_ut_top  = 2
      };

      typedef struct _u3z_ut_frame {
        u3_noun pos;                    //  parent frame's pos
        u3_noun neg;                    //  parent frame's neg
        u3_weak fan;                    //  parent frame's fan
        c3_o    res;                    //  parent frame awaits its fan
        c3_o    own;                    //  parent frame holds its fan
        c3_o    opq;                    //  parent frame is empty-fan-only
        c3_w    kin;                    //  parent frame's kind
        u3_noun ent;                    //  this frame's fan on entry
        c3_w    kiz;                    //  this frame's kind
      } u3z_ut_frame;

    /* u3z_ut_find(): find a +ut memo entry valid under fan.
    **   RETAINS key, fan, cor; produces the value, or u3_none.
    **   On a hit, the entry's tests join the current frame's.
    **   With U3_UT_MEMO_CHECK set, hits are recomputed from cor and compared.
    */
      u3_weak u3z_ut_find(u3_noun key, u3_noun fan, u3_noun cor);

    /* u3z_ut_enter(): begin a wrapper frame computing under fan. RETAINS fan,
    **   which must stay live until u3z_ut_leave().  kin: the frame's kind;
    **   +rest extends fan before playing, the extension is learnt from
    **   +put:in or +gas:in (u3z_ut_grow()), and the holds it adds are its own.
    */
      void u3z_ut_enter(u3z_ut_frame* fra_u, u3_noun fan, c3_w kin_w);

    /* u3z_ut_leave(): end a wrapper frame.
    **   key: key to save val under, or u3_none. TRANSFERS key, RETAINS val.
    */
      void u3z_ut_leave(u3z_ut_frame* fra_u, u3_weak key, u3_noun val);

    /* u3z_ut_test(): record a set membership test, if the set is the fan.
    **   RETAINS all. Called by +has:in.
    */
      void u3z_ut_test(u3_noun set, u3_noun key, u3_noun res);

    /* u3z_ut_grow(): note a set grown by +put:in or +gas:in, if it is the fan.
    **   RETAINS all.
    */
      void u3z_ut_grow(u3_noun set, u3_noun out);

    /* u3z_ut_bail(): forget the frames of the road being abandoned.
    */
      void u3z_ut_bail(void);

#endif /* ifndef U3_ZAVE_H */
