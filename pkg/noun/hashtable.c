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

#define SWAP(l, r)    \
  do { typeof(l) t = l; l = r; r = t; } while (0)

static u3h_slot
_push_pair(u3_noun kev, u3_noun kov, c3_w mug_w, c3_w mog_w)
{
  u3_noun *hed, *tel, *tmp, out = u3i_defcons(&hed, &tel);
  while (1) {
    if ( (mug_w & 1) != (mog_w & 1) ) {
      if ( (mug_w & 1) ) SWAP(kev, kov);
      *hed = kev;
      *tel = kov;
      return out;
    }
    if ( (mug_w & 1) ) {
      *hed = u3_nul;
      tmp = tel;
      *tmp = u3i_defcons(&hed, &tel);
    }
    else {
      *tel = u3_nul;
      tmp = hed;
      *tmp = u3i_defcons(&hed, &tel);
    }
    mug_w >>= 1;
    mog_w >>= 1;
  }
}

// kev in transferred, sot_w is RETAINED
static u3h_slot
_put_slot(u3h_slot sot_w, u3_noun key, u3_noun kev, c3_w mug_w)
{
  if ( 1 == mug_w ) {
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
  return _push_pair(kev, u3k(kov), mug_w,
    (u3r_mug(kov) | (1 << 31)) >> c3_lz_w(mug_w)
  );
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
    u3h_slot new_w = _put_slot(har_u->sot_w, key, kev, mug_w | (1 << 31));
    c3_w old_w = har_u->sot_w;
    har_u->sot_w = new_w;
    u3z(u3h_slot_to_noun(old_w));
  }

  har_u->use_w++;
}

/* u3h_get(): read from hashtable.
**
** `key` is RETAINED; result is PRODUCED.
*/
u3_weak
u3h_get(u3h_root* har_u, u3_noun key)
{
  u3_weak pro = u3h_git(har_u, key);
  return ( u3_none == pro ) ? u3_none : u3k(pro);
}

static u3_weak
_h_git_slot(u3h_slot sot_w, u3_noun key, c3_w mug_w)
{
  while (1 != mug_w) {
    if ( 0 == sot_w ) return u3_none;
    if ( c3y == u3h_slot_is_kev(sot_w) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);
      return ( c3y == u3r_sing(key, u3h(kev)) ) ? u3t(kev) : u3_none;
    }
    sot_w = ( 0 == (mug_w & 1) ) ? u3h(sot_w) : u3t(sot_w);
    mug_w >>= 1;
  }

  u3_assert(!"mug exhausted");
}

/* u3h_git(): read from hashtable, retaining result.
**
** `key` is RETAINED; result is RETAINED.
*/
u3_weak
u3h_git(u3h_root* har_u, u3_noun key)
{
  u3h_slot sot_w = har_u->sot_w;
  c3_w mug_w = u3r_mug(key) | (1 << 31);
  return _h_git_slot(har_u->sot_w, key, mug_w);
}

/* u3h_trim_to(): trim to n key-value pairs
*/
void
u3h_trim_to(u3h_root* har_u, c3_w n_w)
{
  c3_stub;
}

/* u3h_free(): free hashtable.
*/
void
u3h_free(u3h_root* har_u)
{
  c3_stub;
}

/* u3h_mark(): mark hashtable for gc.
*/
c3_w
u3h_mark(u3h_root* har_u)
{
  c3_stub;
}

/* u3h_relocate(): relocate hashtable for compaction.
*/
void
u3h_relocate(u3h_root* har_u)
{
  c3_stub;
}

/* u3h_count(): count hashtable for gc.
*/
c3_w
u3h_count(u3h_root* har_u)
{
  c3_stub;
}

/* u3h_discount(): discount hashtable for gc.
*/
c3_w
u3h_discount(u3h_root* har_u)
{
  c3_stub;
}

/* u3h_walk_with(): traverse hashtable with key, value fn and data
 *                  argument; RETAINS.
*/
void
u3h_walk_with(u3h_root* har_u,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  c3_stub;
}

/* _ch_walk_plain(): use plain u3_noun fun_f for each node
 */
static void
_ch_walk_plain(u3_noun kev, void* wit)
{
  void (*fun_f)(u3_noun) = (void (*)(u3_noun))wit;
  fun_f(kev);
}

/* u3h_walk(): u3h_walk_with, but with no data argument
*/
void
u3h_walk(u3h_root* har_u, void (*fun_f)(u3_noun))
{
  u3h_walk_with(har_u, _ch_walk_plain, (void *)fun_f);
}

/* u3h_take_with(): gain har_u, copying junior keys
** and calling [fun_f] on values, moving the hashtable to new_u
*/
void
u3h_take_with(u3h_root* new_u, u3h_root* har_u, u3_funk fun_f){
  c3_stub;
}

/* u3h_take(): gain hashtable, copying junior nouns
*/
void
u3h_take(u3h_root* new_u, u3h_root* har_u)
{
  c3_stub;
}

/* u3h_wyt(): number of entries
*/
c3_w
u3h_wyt(u3h_root* har_u)
{
  return har_u->use_w;
}