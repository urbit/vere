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
      /* u3h_{32,64}_slot: map slot.
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
        typedef c3_h u3h_32_slot;
        typedef c3_d u3h_64_slot;

      /* u3h_{32,64}_node: map node.
      */
        typedef struct {
          c3_h        map_h;     // bitmap for [sot_w]
          u3h_32_slot sot_w[];   // filled slots
        } u3h_32_node;

        typedef struct {
          c3_h        map_h;
          c3_h        pad_h;
          u3h_64_slot sot_w[];
        } u3h_64_node;

      /* u3h_{32,64}_root: hash root table
      */
        typedef struct {
          c3_h        max_w;
          c3_h        use_w;
          struct {
            c3_h  mug_h;
            c3_h  inx_h;
            c3_h  buc_o;        // XX remove
          } arm_u;
          u3h_32_slot sot_w[64];
        } u3h_32_root;

        typedef struct {
          c3_d        max_w;
          c3_d        use_w;
          struct {
            c3_h    mug_h;
            c3_h    inx_h;
            c3_o    buc_o;
            c3_y    pad_y[3];
          } arm_u;
          c3_y        pad_y[4];
          u3h_64_slot sot_w[64];
        } u3h_64_root;

      /* u3h_{32,64}_buck: bottom bucket.
      */
        typedef struct {
          c3_h        len_h;     // length of [sot_w]
          u3h_32_slot sot_w[];
        } u3h_32_buck;

        typedef struct {
          c3_h        len_h;
          c3_h        pad_h;
          u3h_64_slot sot_w[];
        } u3h_64_buck;

      /* Native u3h_slot/node/buck/root: aliases of the matching bitness.
      */
#ifndef VERE64
        typedef u3h_32_slot u3h_slot;
        typedef u3h_32_node u3h_node;
        typedef u3h_32_buck u3h_buck;
        typedef u3h_32_root u3h_root;
#else
        typedef u3h_64_slot u3h_slot;
        typedef u3h_64_node u3h_node;
        typedef u3h_64_buck u3h_buck;
        typedef u3h_64_root u3h_root;
#endif

    /**  HAMT macros.
    ***
    ***  Coordinate with u3_noun definition!
    **/
      /* u3h_*_slot_is_null(): yes iff slot is empty
      ** u3h_*_slot_is_noun(): yes iff slot contains a key/value cell
      ** u3h_*_slot_is_node(): yes iff slot contains a subtable/bucket
      ** u3h_*_slot_is_warm(): yes iff fresh bit is set
      ** u3h_*_slot_to_node(): slot to node pointer
      ** u3h_*_node_to_slot(): node pointer to slot
      ** u3h_*_slot_to_noun(): slot to cell
      ** u3h_*_noun_to_slot(): cell to slot
      ** u3h_*_noun_be_warm(): warm mutant
      ** u3h_*_noun_be_cold(): cold mutant
      */
#     define  u3h_32_slot_is_null(sot)  ((0 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_32_slot_is_node(sot)  ((1 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_32_slot_is_noun(sot)  ((1 == ((sot) >> 31)) ? c3y : c3n)
#     define  u3h_32_slot_is_warm(sot)  (((sot) & 0x40000000) ? c3y : c3n)
#     define  u3h_32_slot_to_node(sot)  (u3a_32_into(((sot) & 0x3fffffff) << u3a_32_vits))
#     define  u3h_32_node_to_slot(ptr)  ((u3a_32_outa((ptr)) >> u3a_32_vits) | 0x40000000)
#     define  u3h_32_noun_be_warm(sot)  ((sot) | 0x40000000)
#     define  u3h_32_noun_be_cold(sot)  ((sot) & ~0x40000000)
#     define  u3h_32_slot_to_noun(sot)  (0x40000000 | (sot))
#     define  u3h_32_noun_to_slot(som)  (u3h_32_noun_be_warm(som))

#     define  u3h_64_slot_is_null(sot)  ((0 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_64_slot_is_node(sot)  ((1 == ((sot) >> 62)) ? c3y : c3n)
#     define  u3h_64_slot_is_noun(sot)  ((1 == ((sot) >> 63)) ? c3y : c3n)
#     define  u3h_64_slot_is_warm(sot)  (((sot) & 0x4000000000000000ULL) ? c3y : c3n)
#     define  u3h_64_slot_to_node(sot)  (u3a_64_into(((sot) & 0x3fffffffffffffff) << u3a_64_vits))
#     define  u3h_64_node_to_slot(ptr)  ((u3a_64_outa((ptr)) >> u3a_64_vits) | 0x4000000000000000ULL)
#     define  u3h_64_noun_be_warm(sot)  ((sot) | 0x4000000000000000ULL)
#     define  u3h_64_noun_be_cold(sot)  ((sot) & ~0x4000000000000000ULL)
#     define  u3h_64_slot_to_noun(sot)  (0x4000000000000000ULL | (sot))
#     define  u3h_64_noun_to_slot(som)  (u3h_64_noun_be_warm(som))

#ifndef VERE64
#     define  u3h_slot_is_null  u3h_32_slot_is_null
#     define  u3h_slot_is_node  u3h_32_slot_is_node
#     define  u3h_slot_is_noun  u3h_32_slot_is_noun
#     define  u3h_slot_is_warm  u3h_32_slot_is_warm
#     define  u3h_slot_to_node  u3h_32_slot_to_node
#     define  u3h_node_to_slot  u3h_32_node_to_slot
#     define  u3h_noun_be_warm  u3h_32_noun_be_warm
#     define  u3h_noun_be_cold  u3h_32_noun_be_cold
#     define  u3h_slot_to_noun  u3h_32_slot_to_noun
#     define  u3h_noun_to_slot  u3h_32_noun_to_slot
#else
#     define  u3h_slot_is_null  u3h_64_slot_is_null
#     define  u3h_slot_is_node  u3h_64_slot_is_node
#     define  u3h_slot_is_noun  u3h_64_slot_is_noun
#     define  u3h_slot_is_warm  u3h_64_slot_is_warm
#     define  u3h_slot_to_node  u3h_64_slot_to_node
#     define  u3h_node_to_slot  u3h_64_node_to_slot
#     define  u3h_noun_be_warm  u3h_64_noun_be_warm
#     define  u3h_noun_be_cold  u3h_64_noun_be_cold
#     define  u3h_slot_to_noun  u3h_64_slot_to_noun
#     define  u3h_noun_to_slot  u3h_64_noun_to_slot
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
        u3h_32_walk_with(c3_h har_p,
                         void (*fun_f)(c3_h, void*),
                         void* wit);

        void
        u3h_64_walk_with(c3_d har_p,
                         void (*fun_f)(c3_d, void*),
                         void* wit);

#ifndef VERE64
#       define u3h_walk_with u3h_32_walk_with
#else
#       define u3h_walk_with u3h_64_walk_with
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
