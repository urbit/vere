/// @file

#include "manage_v1.h"

#include "allocate_v1.h"
#include "hashtable_v1.h"
#include "jets_v1.h"
#include "nock_v1.h"
#include "vortex_v1.h"

/* u3m_v1_reclaim: clear persistent caches to reclaim memory
*/
void
u3m_v1_reclaim(void)
{
  u3v_v1_reclaim();
  u3j_v1_reclaim();
  u3n_v1_reclaim();
  u3a_v1_reclaim();
}
