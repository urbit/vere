/// @file

#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"

/* _u_sam: axes of a gate sample, and of its head and tail.
*/
static const c3_l _u_sam[]   = { u3x_sam };
static const c3_l _u_sam_2[] = { u3x_sam_2, u3x_sam_3 };

#define U3U_UNARY(fun)  { u3w##fun, u3u##fun, 1, _u_sam,      false   }
#define U3U_BINARY(fun) { u3w##fun, u3u##fun, 2, _u_sam_2,    false }
#define U3U_PUNT(fun)   { u3w##fun, 0,        1, (c3_l[]){1}, true }

/* u3u_Harm: array drivers, keyed by their u3w counterparts.
*/
const u3u_harm u3u_Harm[] = {
  U3U_BINARY(a_add),
  U3U_UNARY(a_dec),
  U3U_BINARY(a_div),
  U3U_BINARY(a_gte),
  U3U_BINARY(a_gth),
  U3U_BINARY(a_lte),
  U3U_BINARY(a_lth),
  U3U_BINARY(a_max),
  U3U_BINARY(a_min),
  U3U_BINARY(a_mod),
  U3U_BINARY(a_mul),
  U3U_BINARY(a_sub),
  U3U_UNARY(c_bex),
  U3U_BINARY(c_dvr),
  U3U_PUNT(e_scot),
  {}
};
