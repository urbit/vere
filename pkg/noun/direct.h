#ifndef U3_DIRECT_H
#define U3_DIRECT_H

#include "error.h"
#include "hashtable.h"
#include "manage.h"
#include "nock.h"
#include "retrieve.h"
#include "serial.h"
#include "ska_core.h"
#include "vortex.h"
#include "xtract.h"

void
u3d_rout(u3_noun sub, u3_noun fol);

u3n_prog*
u3d_search(u3_noun sub, u3_noun fol);

u3_weak
u3d_match_sock(u3_noun cape, u3_noun data, u3_noun list);

#endif /* ifndef U3_DIRECT_H */
