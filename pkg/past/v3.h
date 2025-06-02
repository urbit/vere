#ifndef U3_V3_H
#define U3_V3_H

#include "allocate.h"
#include "hashtable.h"

  /***  allocate.h
  ***/
#     define u3R_v3              u3a_Road
#     define u3a_v3_balign       u3a_balign
#     define u3a_v3_road         u3a_road
#     define u3a_v3_walign       u3a_walign
#     define u3a_v3_walloc       u3a_walloc

#     define u3v3of              u3of

  /***  hashtable.h
  ***/
#     define u3h_v3_root         u3h_root

    /* u3h_v3_new_cache(): create hashtable with bounded size.
    */
      u3p(u3h_v3_root)
      u3h_v3_new_cache(c3_w clk_w);


  /***  vortex.h
  ***/
#     define     u3v_v3_arvo             u3v_arvo
#     define     u3H_v3                  u3v_Home
#     define     u3A_v3                  (&(u3H_v3)->arv_u)
#     define     u3v_v3_home             u3v_home

#endif /* U3_V3_H */
