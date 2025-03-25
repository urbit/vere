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
          u3_lane        lan_u;             //  sponsoring galaxy IP and port
          uv_timer_t     tim_u;             //  keepalive timer handle
          struct timeval sar_u;             //  date we started trying to send
          u3_lane        sef_u;             //  our lane, if we know it
          c3_o           wok_o;             //  STUN worked, set on first success
          c3_o           net_o;             // online heuristic to limit verbosity
        } u3_stun_client;                            //

      /* u3_stun_is_request(): buffer is a stun request.
      */
        c3_o
        u3_stun_is_request(c3_y* buf_y, c3_w len_w);

      /* u3_stun_is_our_response(): buffer is a response to our request.
      */
        c3_o
        u3_stun_is_our_response(c3_y* buf_y, c3_y tid_y[12], c3_w len_w);

      /* u3_stun_make_request(): serialize stun request.
      */
        void
        u3_stun_make_request(c3_y buf_y[28], c3_y tid_y[12]);

      /* u3_stun_make_response(): serialize stun response from request.
      */
        void
        u3_stun_make_response(const c3_y req_y[20],
                              u3_lane*   lan_u,
                              c3_y       buf_y[40]);

      /* u3_stun_find_xor_mapped_address(): extract lane from response.
      */
        c3_o
        u3_stun_find_xor_mapped_address(c3_y*    buf_y,
                                        c3_w     len_w,
                                        u3_lane* lan_u);

      /* u3_stun_start(): begin/restart STUN state machine.
      */
        void
        u3_stun_start(u3_stun_client* sam_u, c3_w tim_w);

      /* u3_stun_shear(): maybe hear stun packet
      */
        c3_o
        u3_stun_hear(u3_stun_client* sun_u,
                   const struct sockaddr* adr_u,
                   c3_w     len_w,
                   c3_y*    hun_y);


#endif /* ifndef U3_STUN_H */
