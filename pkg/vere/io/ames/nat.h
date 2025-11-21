#include "vere.h"
#include "natpmp.h"

typedef struct _u3_natpmp_state {
      u3_auto*       car_u;
      natpmp_t       req_u;             //  libnatpmp struct for mapping request
      uv_poll_t      pol_u;             //  handle waits on libnatpmp socket
      uv_timer_t     tim_u;             //  every two hours if mapping succeeds
    } u3_natpmp_state;                            //  libnatpmp stuff for port forwarding
                                                //
void natpmp_init(uv_timer_t* handle);
