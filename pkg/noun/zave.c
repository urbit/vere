/// @file

#include "zave.h"

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "options.h"
#include "vortex.h"

/* u3z_key(): construct a memo cache-key.  Arguments retained.
*/
u3_noun
u3z_key(c3_m fun, u3_noun one)
{
  return u3nc(fun, u3k(one));
}
u3_noun
u3z_key_2(c3_m fun, u3_noun one, u3_noun two)
{
  return u3nt(fun, u3k(one), u3k(two));
}
u3_noun
u3z_key_3(c3_m fun, u3_noun one, u3_noun two, u3_noun tri)
{
  return u3nq(fun, u3k(one), u3k(two), u3k(tri));
}
u3_noun
u3z_key_4(c3_m fun, u3_noun one, u3_noun two, u3_noun tri, u3_noun qua)
{
  return u3nc(fun, u3nq(u3k(one), u3k(two), u3k(tri), u3k(qua)));
}
u3_noun
u3z_key_5(c3_m fun, u3_noun one, u3_noun two, u3_noun tri, u3_noun qua, u3_noun qin)
{
  return u3nc(fun, u3nq(u3k(one), u3k(two), u3k(tri), u3nc(u3k(qua), u3k(qin))));
}

/* _har(): get the memo cache for the given cid.
*/
static u3p(u3h_root)
_har(u3a_road* rod_u, u3z_cid cid)
{
  switch ( cid ) {
    case u3z_memo_toss:
      return rod_u->cax.har_p;
    case u3z_memo_keep:
      return rod_u->cax.per_p;
  }
  u3_assert(0);
}

/* u3z_find(): find in memo cache.  Arguments retained.
*/
u3_weak
u3z_find(u3z_cid cid, u3_noun key)
{
  if ( (u3z_memo_toss == cid) || (u3R->how.fag_w & u3a_flag_cash) ) {
    // XX under cash lookup in parent roads,
    // copying cache hits into the current road
    return u3h_get(_har(u3R, cid), key);
  }
  else {
    //  XX needs to be benchmarked (up vs. down search)
    u3a_road* rod_u = &(u3H->rod_u);
    while ( 1 ) {
      u3_weak got = u3h_get(_har(rod_u, cid), key);
      if ( u3_none != got ) {
        return got;
      }
      if ( 0 == rod_u->kid_p ) {
        return u3_none;
      }
      rod_u = u3to(u3a_road, rod_u->kid_p);
    };
  }
}

/* u3z_find_up(): find in persistent memo cache,
  starting from the current road. Arguments retained
*/
u3_weak
u3z_find_up(u3_noun key)
{
  u3a_road* rod_u = u3R;
  while ( 1 ) {
    u3_weak got = u3h_get(_har(rod_u, u3z_memo_keep), key);
    if ( u3_none != got ) {
      return got;
    }
    if ( rod_u == &(u3H->rod_u) ) {
      return u3_none;
    }
    rod_u = u3to(u3a_road, rod_u->par_p);
  }
}

u3_weak
u3z_find_m(u3z_cid cid, c3_m fun, u3_noun one)
{
  u3_noun key = u3nc(fun, u3k(one));
  u3_weak val;
  val = u3z_find(cid, key);
  u3z(key);

  return val;
}

/* u3z_save(): save in memo cache. TRANSFER key; RETAIN val
*/
u3_noun
u3z_save(u3z_cid cid, u3_noun key, u3_noun val)
{
  u3h_put(_har(u3R, cid), key, u3k(val));
  u3z(key);
  return val;
}

/* u3z_save_m(): save in memo cache. Arguments retained.
*/
u3_noun
u3z_save_m(u3z_cid cid, c3_m fun, u3_noun one, u3_noun val)
{
  u3_noun key = u3nc(fun, u3k(one));

  u3h_put(_har(u3R, cid), key, u3k(val));
  u3z(key);
  return val;
}

/* u3z_uniq(): uniquify with memo cache. XX not used.
*/
u3_noun
u3z_uniq(u3z_cid cid, u3_noun som)
{
  u3_noun key = u3nc(c3__uniq, u3k(som));
  u3_noun val = u3h_get(_har(u3R, cid), key);

  if ( u3_none != val ) {
    u3z(key); u3z(som); return val;
  }
  else {
    u3h_put(_har(u3R, cid), key, u3k(som));
    return som;
  }
}

/* u3z_reap(): promote memoization cache state.
*/
void
u3z_reap(u3z_cid cid, u3p(u3h_root) har_p)
{
  u3_assert(u3z_memo_toss != cid);

  u3h_uni(_har(u3R, cid), har_p);
  u3h_free(har_p);
}
