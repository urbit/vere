#ifndef U3_HASHTABLE_H
#define U3_HASHTABLE_H

#include "c3/c3.h"
#include "types.h"

  /**  Data structures.
  **/
    /**  Straightforward implementation of the classic Bagwell
    ***  HAMT (hash array mapped trie), using a mug hash.
    ***
    ***  Because a mug is 31 bits, the root table has 64 slots.
    ***  The 31 bits of a mug are divided into the first lookup,
    ***  which is 6 bits (corresponding to the 64 entries in the
    ***  root table), followed by 5 more branchings of 5 bits each,
    ***  corresponding to the 32-slot nodes for everything under
    ***  the root node.
    ***
    ***  We store an extra "freshly warm" bit and use it for a simple
    ***  clock-algorithm reclamation policy.
    ***
    ***  Both 32-bit and 64-bit HAMT layouts are declared unconditionally
    ***  so cross-bitness migration walkers can be defined alongside the
    ***  native one.  Native u3h_slot/node/buck/root are typedef aliases
    ***  to whichever bitness matches this build.
    **/
      /* u3h_slot_{h,d}: map slot.
      **
      **   Either a key-value cell or a loom offset, decoded as a pointer
      **   to a u3h_node, or a u3h_buck at the bottom.  Matches the u3_noun
      **   format - coordinate with allocate.h.  The top two bits are:
      **
      **     00 - empty (in the root table only)
      **     01 - table (node or buck)
      **     02 - entry, stale
      **     03 - entry, fresh
      */
        typedef c3_h u3h_slot_h;
        typedef c3_d u3h_slot_d;

      /* u3h_node_{h,d}: map node.
      */
#define U3H_NODE_BODY(S)                       \
  c3_h                  map_h;                 \
  U3_PASTE(u3h_slot, S) sot_w[];

        U3_DEFINE_PAIR(u3h_node, U3H_NODE_BODY);

      /* u3h_root_{h,d}: hash root table.
      */
#define U3H_ROOT_BODY(S)                       \
  U3_W(S) max_w;                               \
  U3_W(S) use_w;                               \
  struct {                                     \
    c3_h mug_h;                                \
    c3_h inx_h;                                \
    c3_o buc_o;  /* XX remove */               \
  } arm_u;                                     \
  U3_PASTE(u3h_slot, S) sot_w[64];

        U3_DEFINE_PAIR(u3h_root, U3H_ROOT_BODY);

      /* u3h_buck_{h,d}: bottom bucket.
      */
#define U3H_BUCK_BODY(S)                       \
  c3_h                  len_h;                 \
  U3_PASTE(u3h_slot, S) sot_w[];

        U3_DEFINE_PAIR(u3h_buck, U3H_BUCK_BODY);

      /* native u3h_slot: alias of the matching bitness. node/buck/root
      ** native aliases are emitted by U3_DEFINE_PAIR above.
      */
#ifndef VERE64
        typedef u3h_slot_h u3h_slot;
#else
        typedef u3h_slot_d u3h_slot;
#endif

    /**  HAMT macros.
    ***
    ***  Coordinate with u3_noun definition!
    **/
      /* u3h_slot_is_null_*(): yes iff slot is empty
      ** u3h_slot_is_noun_*(): yes iff slot contains a key/value cell
      ** u3h_slot_is_node_*(): yes iff slot contains a subtable/bucket
      ** u3h_slot_is_warm_*(): yes iff fresh bit is set
      ** u3h_slot_to_node_*(): slot to node pointer
      ** u3h_node_to_slot_*(): node pointer to slot
      ** u3h_slot_to_noun_*(): slot to cell
      ** u3h_noun_to_slot_*(): cell to slot
      ** u3h_noun_be_warm_*(): warm mutant
      ** u3h_noun_be_cold_*(): cold mutant
      */
#     define  u3h_slot_is_null_h(sot)  ((0 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_slot_is_node_h(sot)  ((1 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_slot_is_noun_h(sot)  ((1 == ((sot) >> 31)) ? c3y : c3n)
#     define  u3h_slot_is_warm_h(sot)  (((sot) & 0x40000000) ? c3y : c3n)
#     define  u3h_slot_to_node_h(sot)  (u3a_into_h(((sot) & 0x3fffffff) << u3a_vits_h))
#     define  u3h_node_to_slot_h(ptr)  ((u3a_outa_h((ptr)) >> u3a_vits_h) | 0x40000000)
#     define  u3h_noun_be_warm_h(sot)  ((sot) | 0x40000000)
#     define  u3h_noun_be_cold_h(sot)  ((sot) & ~0x40000000)
#     define  u3h_slot_to_noun_h(sot)  (0x40000000 | (sot))
#     define  u3h_noun_to_slot_h(som)  (u3h_noun_be_warm_h(som))

#     define  u3h_slot_is_null_d(sot)  ((0 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_slot_is_node_d(sot)  ((1 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_slot_is_noun_d(sot)  ((1 == ((sot) >> 63)) ? c3y : c3n)
#     define  u3h_slot_is_warm_d(sot)  (((sot) & 0x4000000000000000ULL) ? c3y : c3n)
#     define  u3h_slot_to_node_d(sot)  (u3a_into_d(((sot) & 0x3fffffffffffffff) << u3a_vits_d))
#     define  u3h_node_to_slot_d(ptr)  ((u3a_outa_d((ptr)) >> u3a_vits_d) | 0x4000000000000000ULL)
#     define  u3h_noun_be_warm_d(sot)  ((sot) | 0x4000000000000000ULL)
#     define  u3h_noun_be_cold_d(sot)  ((sot) & ~0x4000000000000000ULL)
#     define  u3h_slot_to_noun_d(sot)  (0x4000000000000000ULL | (sot))
#     define  u3h_noun_to_slot_d(som)  (u3h_noun_be_warm_d(som))

#ifndef VERE64
#     define  u3h_slot_is_null  u3h_slot_is_null_h
#     define  u3h_slot_is_node  u3h_slot_is_node_h
#     define  u3h_slot_is_noun  u3h_slot_is_noun_h
#     define  u3h_slot_is_warm  u3h_slot_is_warm_h
#     define  u3h_slot_to_node  u3h_slot_to_node_h
#     define  u3h_node_to_slot  u3h_node_to_slot_h
#     define  u3h_noun_be_warm  u3h_noun_be_warm_h
#     define  u3h_noun_be_cold  u3h_noun_be_cold_h
#     define  u3h_slot_to_noun  u3h_slot_to_noun_h
#     define  u3h_noun_to_slot  u3h_noun_to_slot_h
#else
#     define  u3h_slot_is_null  u3h_slot_is_null_d
#     define  u3h_slot_is_node  u3h_slot_is_node_d
#     define  u3h_slot_is_noun  u3h_slot_is_noun_d
#     define  u3h_slot_is_warm  u3h_slot_is_warm_d
#     define  u3h_slot_to_node  u3h_slot_to_node_d
#     define  u3h_node_to_slot  u3h_node_to_slot_d
#     define  u3h_noun_be_warm  u3h_noun_be_warm_d
#     define  u3h_noun_be_cold  u3h_noun_be_cold_d
#     define  u3h_slot_to_noun  u3h_slot_to_noun_d
#     define  u3h_noun_to_slot  u3h_noun_to_slot_d
#endif
    /**  Functions.
    ***
    ***  Needs: delete and merge functions; clock reclamation function.
    **/
      /* u3h_new_cache(): create hashtable with bounded size.
      */
        u3p(u3h_root)
        u3h_new_cache(c3_w clk_w);

      /* u3h_new(): create hashtable.
      */
        u3p(u3h_root)
        u3h_new(void);

      /* u3h_put(): insert in hashtable.
      **
      ** `key` is RETAINED; `val` is transferred.
      */
        void
        u3h_put(u3p(u3h_root) har_p, u3_noun key, u3_noun val);

      /* u3h_put_get(): insert in caching hashtable, returning deleted entry
      **
      ** `key` is RETAINED; `val` is transferred.
      */
      u3_weak
      u3h_put_get(u3p(u3h_root) har_p, u3_noun key, u3_noun val);

      /* u3h_uni(): unify hashtables, copying [rah_p] into [har_p]
      */
        void
        u3h_uni(u3p(u3h_root) har_p, u3p(u3h_root) rah_p);

      /* u3h_get(): read from hashtable.
      **
      ** `key` is RETAINED; result is PRODUCED.
      */
        u3_weak
        u3h_get(u3p(u3h_root) har_p, u3_noun key);

      /* u3h_git(): read from hashtable, retaining result.
      **
      ** `key` is RETAINED; result is RETAINED.
      */
        u3_weak
        u3h_git(u3p(u3h_root) har_p, u3_noun key);

      /* u3h_del(); delete from hashtable.
      **
      ** `key` is RETAINED
      */
        void
        u3h_del(u3p(u3h_root) har_p, u3_noun key);

      /* u3h_trim_to(): trim to n key-value pairs
      */
        void
        u3h_trim_to(u3p(u3h_root) har_p, c3_h n_h);

      /* u3h_trim_with(): trim to n key-value pairs, with deletion callback
      */
        void
        u3h_trim_with(u3p(u3h_root) har_p, c3_h n_h, void (*del_cb)(u3_noun));

      /* u3h_free(): free hashtable.
      */
        void
        u3h_free(u3p(u3h_root) har_p);

      /* u3h_mark(): mark hashtable for gc.
      */
        c3_w
        u3h_mark(u3p(u3h_root) har_p);

      /* u3h_relocate(): relocate hashtable for compaction.
      */
        void
        u3h_relocate(u3p(u3h_root) *har_p);

      /* u3h_count(): count hashtable for gc.
      */
        c3_w
        u3h_count(u3p(u3h_root) har_p);

      /* u3h_discount(): discount hashtable for gc.
      */
        c3_w
        u3h_discount(u3p(u3h_root) har_p);

      /* u3h_walk_with(): traverse hashtable with key, value fn and data
      **                  argument; RETAINS.  Aliases the matching bitness.
      */
        void
        u3h_walk_with_h(c3_h har_p,
                         void (*fun_f)(c3_h, void*),
                         void* wit);

        void
        u3h_walk_with_d(c3_d har_p,
                         void (*fun_f)(c3_d, void*),
                         void* wit);

#ifndef VERE64
#       define u3h_walk_with u3h_walk_with_h
#else
#       define u3h_walk_with u3h_walk_with_d
#endif

      /* u3h_walk(): u3h_walk_with, but with no data argument
      */
        void
        u3h_walk(u3p(u3h_root) har_p, void (*fun_f)(u3_noun));

      /* u3h_take_with(): gain hashtable, copying junior keys
      ** and calling [fun_f] on values
      */
        u3p(u3h_root)
        u3h_take_with(u3p(u3h_root) har_p, u3_funk fun_f);

      /* u3h_take(): gain hashtable, copying junior nouns
      */
        u3p(u3h_root)
        u3h_take(u3p(u3h_root) har_p);

      /* u3h_wyt(): number of entries
      */
        c3_w
        u3h_wyt(u3p(u3h_root) har_p);

#endif /* ifndef U3_HASHTABLE_H */
