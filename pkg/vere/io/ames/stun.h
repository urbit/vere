#include "vere.h"
#include "c3/types.h"

#ifndef U3_STUN_H
#define U3_STUN_H

        typedef enum _u3_stun_state {
          STUN_OFF = 0,
          STUN_TRYING = 1,
          STUN_KEEPALIVE = 2,
        } u3_stun_state;
        
        typedef struct _u3_stun_client {    //    stun client state:
          u3_auto*       car_u;             //  driver backpointer
          u3_stun_state  sat_y;             //  formal state
          c3_y           tid_y[12];         //  last transaction id
          c3_y           dad_y;             //  sponsoring galaxy
          sockaddr_in    lan_u;             //  sponsoring galaxy IP and port
          uv_timer_t     tim_u;             //  keepalive timer handle
          struct timeval sar_u;             //  date we started trying to send
          sockaddr_in    sef_u;             //  our lane, if we know it
          c3_o           wok_o;             //  STUN worked, set on first success
          c3_o           net_o;             // online heuristic to limit verbosity
        } u3_stun_client;                            //

      /* u3_stun_start(): begin/restart STUN state machine.
      */
        void
        u3_stun_start(u3_stun_client* sam_u, c3_w tim_w);

      /* u3_stun_hear(): maybe hear stun packet
      */
        c3_o
        u3_stun_hear(u3_stun_client* sun_u,
                   const struct sockaddr_in* adr_u,
                   c3_w     len_w,
                   c3_y*    hun_y);


#endif /* ifndef U3_STUN_H */
