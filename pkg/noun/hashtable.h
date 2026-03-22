#ifndef U3_HASHTABLE_H
#define U3_HASHTABLE_H

#include "c3/c3.h"
#include "types.h"

    typedef c3_w u3h_slot;

    #define u3h_slot_warm_flag    ((u3h_slot)1 << 30)
    #define u3h_slot_kev_mask     ((u3h_slot)1 << 31)
    #define u3h_slot_is_kev(sot)  ((sot) >> 31 ? c3n : c3y)
    #define u3h_kev_is_warm(sot)  (((sot) & u3h_slot_warm_flag) ? c3y : c3n)
    #define u3h_kev_be_warm(sot)  ((sot) | u3h_slot_warm_flag)
    #define u3h_kev_be_cold(sot)  ((sot) & ~u3h_slot_warm_flag)
    #define u3h_kev_to_slot(kev)  ((kev) & ~u3h_slot_kev_mask)
    #define u3h_slot_to_noun(sot) ((sot) | u3h_slot_kev_mask | u3h_slot_warm_flag)

    /* Tree layout:

      A slot is either 0 or a key-value pair "kev" or a pair of two
      slots, or a list of key-value pairs at the very bottom of the tree.

      Given a mug hash, starting from least significant bits, we go either left
      or right, until we either reach a "kev" or exhaust all bits, reaching a
      list of kevs.

    */

    typedef struct {
      c3_w use_w;
      c3_w max_w;
      u3h_slot sot_w;
    } u3h_root;

    /**  Functions.
    ***
    **/
      /* u3h_new_cache(): create hashtable with bounded size.
      */
        void
        u3h_new_cache(u3h_root* har_u, c3_w clk_w);

      /* u3h_new(): create hashtable.
      */
        void
        u3h_new(u3h_root* har_u);

      /* u3h_put(): insert in hashtable.
      **
      ** `key` is RETAINED; `val` is transferred.
      */
        void
        u3h_put(u3h_root* har_u, u3_noun key, u3_noun val);

      /* u3h_uni(): unify hashtables, copying [rah_u] into [har_u]
      */
        void
        u3h_uni(u3h_root* har_u, u3h_root* rah_u);

      /* u3h_get(): read from hashtable.
      **
      ** `key` is RETAINED; result is PRODUCED.
      */
        u3_weak
        u3h_get(u3h_root* har_u, u3_noun key);

      /* u3h_git(): read from hashtable, retaining result.
      **
      ** `key` is RETAINED; result is RETAINED.
      */
        u3_weak
        u3h_git(u3h_root* har_u, u3_noun key);

      /* u3h_trim_to(): trim to n key-value pairs
      */
        void
        u3h_trim_to(u3h_root* har_u, c3_w n_w);

      /* u3h_free(): free hashtable.
      */
        void
        u3h_free(u3h_root* har_u);

      /* u3h_mark(): mark hashtable for gc.
      */
        c3_w
        u3h_mark(u3h_root* har_u);

      /* u3h_relocate(): relocate hashtable for compaction.
      */
        void
        u3h_relocate(u3h_root* har_u);

      /* u3h_count(): count hashtable for gc.
      */
        c3_w
        u3h_count(u3h_root* har_u);

      /* u3h_discount(): discount hashtable for gc.
      */
        c3_w
        u3h_discount(u3h_root* har_u);

      /* u3h_walk_with(): traverse hashtable with key, value fn and data
       *                  argument; RETAINS.
      */
        void
        u3h_walk_with(u3h_root* har_u,
                      void (*fun_f)(u3_noun, void*),
                      void* wit);

      /* u3h_walk(): u3h_walk_with, but with no data argument
      */
        void
        u3h_walk(u3h_root* har_u, void (*fun_f)(u3_noun));

      /* u3h_take_with(): gain har_u, copying junior keys
      ** and calling [fun_f] on values, moving the hashtable to new_u
      */
        void
        u3h_take_with(u3h_root* new_u, u3h_root* har_u, u3_funk fun_f);

      /* u3h_take(): gain hashtable, copying junior nouns
      */
        void
        u3h_take(u3h_root* new_u, u3h_root* har_u);

      /* u3h_wyt(): number of entries
      */
        c3_w
        u3h_wyt(u3h_root* har_u);

#endif /* ifndef U3_HASHTABLE_H */
