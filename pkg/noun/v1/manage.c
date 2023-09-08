/// @file

#include "pkg/noun/v1/manage.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "pkg/noun/v1/allocate.h"
#include "events.h"
#include "pkg/noun/v1/hashtable.h"
#include "imprison.h"
#include "pkg/noun/jets.h"
#include "pkg/noun/v1/jets.h"
#include "jets/k.h"
#include "log.h"
#include "nock.h"
#include "openssl/crypto.h"
#include "options.h"
#include "platform/rsignal.h"
#include "retrieve.h"
#include "trace.h"
#include "urcrypt/urcrypt.h"
#include "pkg/noun/vortex.h"
#include "xtract.h"

/* u3m_v1_reclaim: clear persistent caches to reclaim memory
*/
void
u3m_v1_reclaim(void)
{
  u3v_reclaim();
  u3j_v1_reclaim();
  u3n_v1_reclaim();
  u3a_v1_reclaim();
}
