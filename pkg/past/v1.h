#ifndef U3_V1_H
#define U3_V1_H

#include "v2.h"

  /***  {c3,noun}/types.h
  ***/
#     define  c3_v1_l                 c3_v2_l
#     define  c3_v1_w                 c3_v2_w
#     define  u3_v1_noun              u3_v2_noun
#     define  u3_v1_none              u3_v2_none
#     define  u3_v1_weak              u3_v2_weak
#     define  u3_v1_post              u3_v2_post
#     define  u3v1p(type)             u3_v1_post

  /***  c3/defs.h
  ***/
#     define  c3_v1_wiseof            c3_v2_wiseof

  /***  allocate.h
  ***/
#     define  u3_Loom_v1          u3_Loom_v2

#     define  u3a_v1_to_off(som)  ((som) & 0x3fffffff)
#     define  u3a_v1_to_ptr(som)  (u3a_v1_into(u3a_v1_to_off(som)))

#     define  u3a_v1_into         u3a_v2_into
#     define  u3a_v1_outa         u3a_v2_outa
#     define  u3R_v1              u3R_v2


  /***  hashtable.h
  ***/
#     define  u3h_v1_slot_to_node(sot)  (u3a_v1_into((sot) & 0x3fffffff))


  /***  manage.h
  ***/
      /* u3m_v1_reclaim: clear persistent caches to reclaim memory
      */
        void
        u3m_v1_reclaim(void);


  /***  vortex.h
  ***/
#     define  u3H_v1       u3H_v2
#     define  u3v_v1_home  u3v_v2_home


  /***  init
  ***/
        void
        u3_v1_load(c3_z wor_i);

#endif /* U3_V1_H */
