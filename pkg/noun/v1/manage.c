/// @file

#include "pkg/noun/v1/manage.h"

#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v1/hashtable.h"
#include "pkg/noun/v1/jets.h"
#include "pkg/noun/v1/nock.h"
#include "pkg/noun/v1/vortex.h"

/* u3m_v1_reclaim: clear persistent caches to reclaim memory
*/
void
u3m_v1_reclaim(void)
{
  fprintf(stderr, "u3m_v1_reclaim 1\r\n");
  u3v_v1_reclaim();
  fprintf(stderr, "u3m_v1_reclaim 2\r\n");
  u3j_v1_reclaim();
  fprintf(stderr, "u3m_v1_reclaim 3\r\n");
  u3n_v1_reclaim();
  fprintf(stderr, "u3m_v1_reclaim 4\r\n");
  u3a_v1_reclaim();
  fprintf(stderr, "u3m_v1_reclaim 5\r\n");
}
