#ifndef U3_HASHTABLE_V1_H
#define U3_HASHTABLE_V1_H

#include "pkg/noun/hashtable.h"
#include "pkg/noun/v2/hashtable.h"

  /**  Aliases.
  **/
#     define  u3h_v1_buck          u3h_v2_buck
#     define  u3h_v1_node          u3h_v2_node
#     define  u3h_v1_root          u3h_v2_root
#     define  u3h_v1_slot_is_node  u3h_v2_slot_is_node
#     define  u3h_v1_slot_is_noun  u3h_v2_slot_is_noun
#     define  u3h_v1_slot_to_noun  u3h_v2_slot_to_noun

  /**  Data structures.
  **/

    /**  HAMT macros.
    ***
    ***  Coordinate with u3_noun definition!
    **/
      /* u3h_v1_slot_to_node(): slot to node pointer
      ** u3h_v1_node_to_slot(): node pointer to slot
      */
#     define  u3h_v1_slot_to_node(sot)  (u3a_v1_into((sot) & 0x3fffffff))
#     define  u3h_v1_node_to_slot(ptr)  (u3a_v1_outa(ptr) | 0x40000000)

    /**  Functions.
    ***
    ***  Needs: delete and merge functions; clock reclamation function.
    **/
      /* u3h_v1_free(): free hashtable.
      */
        void
        u3h_v1_free_nodes(u3p(u3h_root) har_p);

      /* u3h_v1_walk_with(): traverse hashtable with key, value fn and data
       *                  argument; RETAINS.
      */
        void
        u3h_v1_walk_with(u3p(u3h_root) har_p,
                      void (*fun_f)(u3_noun, void*),
                      void* wit);

      /* u3h_v1_walk(): u3h_v1_walk_with, but with no data argument
      */
        void
        u3h_v1_walk(u3p(u3h_root) har_p, void (*fun_f)(u3_noun));

#endif /* ifndef U3_HASHTABLE_H */
