#ifndef U3_V3_H
#define U3_V3_H

#include "v4.h"

  /***  allocate.h
  ***/
#     define  u3_Loom_v3          u3_Loom_v4
#     define  u3R_v3              u3a_v4_Road

#     define  u3a_v3_heap         u3a_v4_heap
#     define  u3a_v3_is_cat       u3a_v4_is_cat
#     define  u3a_v3_is_cell      u3a_v4_is_cell
#     define  u3a_v3_is_north     u3a_v4_is_north
#     define  u3a_v3_is_pom       u3a_v4_is_pom
#     define  u3a_v3_is_pug       u3a_v4_is_pug
#     define  u3a_v3_vits         u3a_v4_vits

#     define  u3a_v3_road         u3a_v4_road
#     define  u3a_v3_walloc       u3a_v4_walloc
#     define  u3a_v3_into         u3a_v4_into
#     define  u3a_v3_outa         u3a_v4_outa

#     define  u3a_v3_botox        u3a_v4_botox
#     define  u3a_v3_box          u3a_v4_box
#     define  u3a_v3_cell         u3a_v4_cell
#     define  u3a_v3_fbox         u3a_v4_fbox
#     define  u3a_v3_fbox_no      u3a_v4_fbox_no
#     define  u3a_v3_minimum      u3a_v4_minimum
#     define  u3a_v3_rewrite      u3a_v4_rewrite
#     define  u3a_v3_rewrite_ptr  u3a_v4_rewrite_ptr
#     define  u3a_v3_rewritten    u3a_v4_rewritten
#     define  u3a_v3_to_pug       u3a_v4_to_pug
#     define  u3a_v3_to_pom       u3a_v4_to_pom
#     define  u3a_v3_wfree        u3a_v4_wfree

#     define  u3v3to              u3v4to
#     define  u3v3of              u3v4of

#     define  u3a_v3_free         u3a_v4_free
#     define  u3a_v3_lose         u3a_v4_lose
#     define  u3a_v3_to_off       u3a_v4_to_off
#     define  u3a_v3_to_ptr       u3a_v4_to_ptr
#     define  u3a_v3_ream         u3a_v4_ream
#     define  u3a_v3_balign       u3a_v4_balign
#     define  u3a_v3_walign       u3a_v4_walign


  /***  jets.h
  ***/
#     define  u3j_v3_fink         u3j_v4_fink
#     define  u3j_v3_fist         u3j_v4_fist
#     define  u3j_v3_hank         u3j_v4_hank
#     define  u3j_v3_rite         u3j_v4_rite
#     define  u3j_v3_site         u3j_v4_site

#     define  u3j_v3_rite_lose    u3j_v4_rite_lose
#     define  u3j_v3_site_lose    u3j_v4_site_lose
#     define  u3j_v3_free_hank    u3j_v4_free_hank


  /***  hashtable.h
  ***/
#     define  u3h_v3_buck         u3h_v4_buck
#     define  u3h_v3_node         u3h_v4_node
#     define  u3h_v3_root         u3h_v4_root

#     define  u3h_v3_free         u3h_v4_free
#     define  u3h_v3_walk         u3h_v4_walk
#     define  u3h_v3_new          u3h_v4_new
#     define  u3h_v3_new_cache    u3h_v4_new_cache
#     define  u3h_v3_slot_is_node u3h_v4_slot_is_node
#     define  u3h_v3_slot_is_noun u3h_v4_slot_is_noun
#     define  u3h_v3_noun_to_slot u3h_v4_noun_to_slot
#     define  u3h_v3_slot_to_noun u3h_v4_slot_to_noun


  /***  vortex.h
  ***/
#     define  u3v_v3_version          u3v_v4_version
#     define  u3v_v3_arvo             u3v_v4_arvo
#     define  u3H_v3                  u3v_v4_Home
#     define  u3A_v3                  (&(u3H_v3)->arv_u)
#     define  u3v_v3_home             u3v_v4_home


  /***  init
  ***/
        void
        u3_v3_load(c3_z wor_i);

#endif /* U3_V3_H */
