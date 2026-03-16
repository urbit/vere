#ifndef U3_HASHTABLE_H
#define U3_HASHTABLE_H

#include "c3/c3.h"
#include "types.h"

    /**  Data structures
    ***
    ***  There are two kinds of hashtables that we need: those that get promoted
    ***  to senior roads, and those who don't. The former need to be traversable
    ***  at all times (due to async signal stuff), the latter can be simpler.
    ***
    ***  Both can be implemented as simple hash tables with arrays of key-value
    ***  pairs, with the only difference that the simpler hashtable can have its
    ***  key-hash-value splat into a struct for better cache coherency, while
    ***  the "atomic" one will have to store them in a separately allocated
    ***  structure, pointer to which is stored in the array. In practice it
    ***  means using cells as such structs.
    ***
    ***  This hashtable implementation is inspired by the one in Jai module
    ***  shipped with the compiler.
    **/
    #define u3h_size_min    32  // must be a power of two
    #define u3h_slot_empty  0
    #define u3h_load_factor_percent 70

    typedef struct {
      c3_w      use_w;  // number of valid items
      c3_w      loc_w;  // number of allocated slots
      c3_w      max_w;  // bounded size if max_w > 0
      u3_noun   kev[0];
    } u3h_root;

    /**  Functions.
    ***
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
        u3h_trim_to(u3p(u3h_root) har_p, c3_w n_w);

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
       *                  argument; RETAINS.
      */
        void
        u3h_walk_with(u3p(u3h_root) har_p,
                      void (*fun_f)(u3_noun, void*),
                      void* wit);

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
