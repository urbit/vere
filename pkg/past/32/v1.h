#ifndef U3_V1_H
#define U3_V1_H

#include "v2.h"

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
