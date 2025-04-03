#include "io/mesa/mesa.h"

#define _CZAR_GONE  (htonl(UINT32_MAX))
#define NLOCALHOST (htonl(0x7f000001))


typedef struct _u3_ames_lamp_state {                            //
  u3_auto*   car_u;
  per_map*   per_u;
  c3_c**     dns_c;                 //    domain
  c3_o       dom_o;                 //    have domain
  uv_timer_t tim_u;                 //    resolve timer
  c3_s       pen_s;                 //    pending
} u3_ames_lamp_state;               //

