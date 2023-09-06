#ifndef U3_HASHTABLE_V2_H
#define U3_HASHTABLE_V2_H

#include "pkg/noun/hashtable.h"

#include "c3.h"
#include "types.h"

  /**  Data structures.
  **/

    /**  HAMT macros.
    ***
    ***  Coordinate with u3_noun definition!
    **/
      /* u3h_v2_slot_is_null(): yes iff slot is empty
      ** u3h_v2_slot_is_noun(): yes iff slot contains a key/value cell
      ** u3h_v2_slot_is_node(): yes iff slot contains a subtable/bucket
      ** u3h_v2_slot_is_warm(): yes iff fresh bit is set
      ** u3h_v2_slot_to_node(): slot to node pointer
      ** u3h_v2_node_to_slot(): node pointer to slot
      ** u3h_v2_slot_to_noun(): slot to cell
      ** u3h_v2_noun_to_slot(): cell to slot
      ** u3h_v2_noun_be_warm(): warm mutant
      ** u3h_v2_noun_be_cold(): cold mutant
      */
#     define  u3h_v2_slot_to_node(sot)  (u3a_into(((sot) & 0x3fffffff) << u3C.vits_w))
#     define  u3h_v2_node_to_slot(ptr)  ((u3a_outa((ptr)) >> u3C.vits_w) | 0x40000000)

    /**  Functions.
    ***
    ***  Needs: delete and merge functions; clock reclamation function.
    **/
      /* u3h_v2_free(): free hashtable.
      */
        void
        u3h_v2_free(u3p(u3h_root) har_p);

      /* u3h_v2_walk_with(): traverse hashtable with key, value fn and data
       *                  argument; RETAINS.
      */
        void
        u3h_v2_walk_with(u3p(u3h_root) har_p,
                      void (*fun_f)(u3_noun, void*),
                      void* wit);

      /* u3h_v2_walk(): u3h_v2_walk_with, but with no data argument
      */
        void
        u3h_v2_walk(u3p(u3h_root) har_p, void (*fun_f)(u3_noun));

#endif /* U3_HASHTABLE_V2_H */