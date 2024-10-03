#ifndef U3_HASHTABLE_V2_H
#define U3_HASHTABLE_V2_H

#define u3h_v2_buck          u3h_buck
#define u3h_v2_free          u3h_free
#define u3h_v2_new           u3h_new
#define u3h_v2_node          u3h_node
#define u3h_v2_noun_to_slot  u3h_noun_to_slot
#define u3h_v2_root          u3h_root
#define u3h_v2_slot_is_node  u3h_slot_is_node
#define u3h_v2_slot_is_noun  u3h_slot_is_noun
#define u3h_v2_slot_to_noun  u3h_slot_to_noun
#define u3h_v2_walk          u3h_walk
#define u3h_v2_walk_with     u3h_walk_with

#include "pkg/noun/hashtable.h"

#include "c3.h"
#include "types.h"

  /**  Data structures.
  **/

    /**  HAMT macros.
    ***
    ***  Coordinate with u3_noun definition!
    **/
      /* u3h_v2_slot_to_node(): slot to node pointer
      ** u3h_v2_node_to_slot(): node pointer to slot
      */
#     define  u3h_v2_slot_to_node(sot)  (u3a_v2_into(((sot) & 0x3fffffff) << u3a_vits))
#     define  u3h_v2_node_to_slot(ptr)  ((u3a_v2_outa((ptr)) >> u3a_vits) | 0x40000000)

    /**  Functions.
    ***
    **/
      /* u3h_v2_rewrite(): rewrite hashtable for compaction.
      */
        void
        u3h_v2_rewrite(u3p(u3h_root) har_p);

#endif /* U3_HASHTABLE_V2_H */
