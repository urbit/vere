/// @file

#include "v1/manage.h"

#include "v1/allocate.h"
#include "v1/hashtable.h"
#include "v1/jets.h"
#include "v1/nock.h"
#include "v1/vortex.h"

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
