#include "vere.h"

#ifndef U3_STUN_H
#define U3_STUN_H

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

#endif /* ifndef U3_STUN_H */
