/// @file

#include "pkg/noun/vortex.h"
#include "pkg/noun/v1/vortex.h"

#include "pkg/noun/v1/allocate.h"

/* u3v_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3v_v1_reclaim(void)
{
  //  clear the u3v_wish cache
  //
  u3a_v1_lose(u3A_v1->yot);
  u3A_v1->yot = u3_nul;
}
