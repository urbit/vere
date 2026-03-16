#ifndef U3_V5_H
#define U3_V5_H

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "nock.h"
#include "retrieve.h"
#include "vortex.h"

  /***  current
  ***/
#     define  u3_v5_cell          u3_cell
#     define  u3_v5_noun          u3_noun
#     define  u3_v5_none          u3_none

#     define  u3A_v5              u3A
#     define  u3R_v5              u3R
#     define  u3j_v5_boot         u3j_boot
#     define  u3j_v5_ream         u3j_ream
#     define  u3a_v5_walloc       u3a_walloc
#     define  u3a_v5_to_pug       u3a_to_pug
#     define  u3a_v5_outa         u3a_outa
#     define  u3a_v5_gain         u3a_gain
#     define  u3i_v5_cell         u3i_cell
#     define  u3h_v5_put          u3h_put
#     define  u3a_v5_lose         u3a_lose

#     define  u3a_v5_atom             u3a_atom
#     define  u3a_v5_is_atom          u3a_is_atom
#     define  u3a_v5_is_pom           u3a_is_pom
#     define  u3a_v5_north_is_normal  u3a_north_is_normal
#     define  u3n_v5_prog             u3n_prog
#     define  u3r_v5_mug_both         u3r_mug_both
#     define  u3r_v5_mug_words        u3r_mug_words

#     define  u3a_v5_heap         u3a_heap
#     define  u3a_v5_is_cat       u3a_is_cat
#     define  u3a_v5_is_cell      u3a_is_cell
#     define  u3a_v5_is_north     u3a_is_north
#     define  u3a_v5_is_pom       u3a_is_pom
#     define  u3a_v5_is_pug       u3a_is_pug

#     define  u3j_v5_fink         u3j_fink
#     define  u3j_v5_fist         u3j_fist
#     define  u3j_v5_hank         u3j_hank
#     define  u3j_v5_rite         u3j_rite
#     define  u3j_v5_site         u3j_site

// #     define  u3h_v5_buck         u3h_buck
// #     define  u3h_v5_node         u3h_node
// #     define  u3h_v5_root         u3h_root

 typedef c3_w u3h_v5_slot;

      /* u3h_node: map node.
      */
        typedef struct {
          c3_w     map_w;     // bitmap for [sot_w]
          u3h_v5_slot sot_w[];   // filled slots
        } u3h_v5_node;

      /* u3h_root: hash root table
      */
        typedef struct {
          c3_w     max_w;     // number of cache lines (0 for no trimming)
          c3_w     use_w;     // number of lines currently filled
          struct {
            c3_w  mug_w;      // current hash
            c3_w  inx_w;      // index into current hash bucket
            c3_o  buc_o;      // XX remove
          } arm_u;            // clock arm
          u3h_v5_slot sot_w[64]; // slots
        } u3h_v5_root;

      /* u3h_buck: bottom bucket.
      */
        typedef struct {
          c3_w     len_w;     // length of [sot_w]
          u3h_v5_slot sot_w[];   // filled slots
        } u3h_v5_buck;

#     define  u3h_v5_slot_is_node(sot) (u3a_into(((sot) & 0x3fffffff) << u3a_vits))
#     define  u3h_v5_slot_is_noun(sot) ((1 == ((sot) >> 31)) ? c3y : c3n)
#     define  u3h_v5_slot_is_null(sot) ((0 == ((sot) >> 30)) ? c3y : c3n)
#     define  u3h_v5_noun_to_slot(sot) ((sot) | 0x40000000)
#     define  u3h_v5_slot_to_noun(sot) (0x40000000 | (sot))

#endif /* U3_V5_H */
