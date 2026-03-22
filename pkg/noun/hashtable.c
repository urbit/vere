/// @file

#include "hashtable.h"

#include "allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "xtract.h"
#include "options.h"


/* u3h_new_cache(): create hashtable with bounded size.
*/
void
u3h_new_cache(u3h_root* har_u, c3_w max_w)
{
  // max_w is ignored for now
  har_u->use_w = 0;
  har_u->max_w = 0;
  har_u->sot_w = u3_nul;
}

/* u3h_new(): create hashtable.
*/
void
u3h_new(u3h_root* har_u)
{
  u3h_new_cache(har_u, 0);
}

// kev in transferred, sot_w is RETAINED
static u3h_slot
_put_slot(u3h_slot sot_w, u3_noun key, u3_noun kev, c3_w mug_w)
{
  if ( !mug_w ) {
    //  mug exhausted, we got to a list of kevs
    u3_assert(c3y == u3a_is_cell(sot_w));
    if ( 0 == sot_w ) return kev;
    return u3nc(kev, u3k(sot_w));
  }

  if ( c3n == u3h_slot_is_kev(sot_w) ) {
    // a pair of slots
    u3_assert(c3y == u3a_is_cell(sot_w));
    if ( 0 == (mug_w & 1) ) {
      return u3nc(_put_slot(u3h(sot_w), key, kev, mug_w >> 1), u3k(u3t(sot_w)));
    }
    return u3nc(u3k(u3h(sot_w)), _put_slot(u3t(sot_w), key, kev, mug_w >> 1));
  }

  u3_noun kov = u3h_slot_to_noun(sot_w);
  if ( c3y == u3r_sing(key, u3h(kov)) ) {
    return u3h_kev_to_slot(kev);
  }
}

/* u3h_put(): insert in hashtable.
**
** `key` is RETAINED; `val` is transferred.
*/
void
u3h_put(u3h_root* har_u, u3_noun key, u3_noun val)
{
  u3_noun kev = u3nc(u3k(key), val);

  if ( 0 == har_u->sot_w ) {
    har_u->sot_w = u3h_kev_to_slot(kev);
  }
  else {
    c3_w mug_w = u3r_mug(key);
    u3h_slot new_w = _put_slot(har_u->sot_w, key, kev, mug_w);
    c3_w old_w = har_u->sot_w;
    har_u->sot_w = new_w;
    u3z(u3h_slot_to_noun(old_w));
  }

  har_u->use_w++;
}