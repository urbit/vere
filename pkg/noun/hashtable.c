/// @file

#include "hashtable.h"

#include "allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "xtract.h"

/* CUT_END(): extract [b_w] low bits from [a_w]
*/
#define CUT_END(a_w, b_w) ((a_w) & (((c3_w)1 << (b_w)) - 1))

/* BIT_SET(): [1] if bit [b_w] is set in [a_w]
*/
#define BIT_SET(a_w, b_w) ((a_w) & ((c3_w)1 << (b_w)))

static u3_weak
_ch_trim_slot(u3h_root* har_u, u3h_slot *sot_w, c3_w lef_w, c3_w rem_w);

static u3_weak
_ch_trim_root(u3h_root* har_u);

c3_w
_ch_skip_slot(c3_w mug_w, c3_w lef_w);

/* u3h_new_cache(): create hashtable with bounded size.
*/
u3p(u3h_root)
u3h_new_cache(c3_w max_w)
{
  u3h_root*     har_u = u3a_walloc(c3_wiseof(u3h_root));
  u3p(u3h_root) har_p = u3of(u3h_root, har_u);
  c3_w        i_w;

  har_u->max_w       = max_w;
  har_u->use_w       = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    har_u->sot_w[i_w] = 0;
  }
  return har_p;
}

/* u3h_new(): create hashtable.
*/
u3p(u3h_root)
u3h_new(void)
{
  return u3h_new_cache(0);
}

/* _ch_popcount(): number of bits set in word.  A standard intrinsic.
*/
static c3_w
_ch_popcount(c3_w num_w)
{
  return c3_pc_w(num_w);
}

/* _ch_buck_new(): create new bucket.
*/
static u3h_buck*
_ch_buck_new(c3_w len_w)
{
  u3h_buck* hab_u = u3a_walloc(c3_wiseof(u3h_buck) +
                               (len_w * c3_wiseof(u3h_slot)));
  hab_u->len_w = len_w;
  return hab_u;
}

/* _ch_node_new(): create new node.
*/
static u3h_node*
_ch_node_new(c3_w len_w)
{
  u3h_node* han_u = u3a_walloc(c3_wiseof(u3h_node) +
                               (len_w * c3_wiseof(u3h_slot)));
  han_u->map_w = 0;
  return han_u;
}

static void _ch_slot_put(u3h_slot*, u3_noun, c3_w, c3_w, c3_w*);

/* _ch_node_add(): add to node.
*/
static u3h_node*
_ch_node_add(u3h_node* han_u, c3_w lef_w, c3_w rem_w, u3_noun kev, c3_w *use_w)
{
  c3_w bit_w, inx_w, map_w, i_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;
  inx_w = _ch_popcount(CUT_END(map_w, bit_w));

  if ( BIT_SET(map_w, bit_w) ) {
    _ch_slot_put(&(han_u->sot_w[inx_w]), kev, lef_w, rem_w, use_w);
    return han_u;
  }
  else {
    //  nothing was at this slot.
    //  Optimize: use u3a_wealloc.
    //
    c3_w      len_w = _ch_popcount(map_w);
    u3h_node* nah_u = _ch_node_new(1 + len_w);
    nah_u->map_w    = han_u->map_w | ((c3_w)1 << bit_w);

    for ( i_w = 0; i_w < inx_w; i_w++ ) {
      nah_u->sot_w[i_w] = han_u->sot_w[i_w];
    }
    nah_u->sot_w[inx_w] = u3h_noun_be_warm(u3h_noun_to_slot(kev));
    for ( i_w = inx_w; i_w < len_w; i_w++ ) {
      nah_u->sot_w[i_w + 1] = han_u->sot_w[i_w];
    }

    u3a_wfree(han_u);
    *use_w += 1;
    return nah_u;
  }
}

/* ch_buck_add(): add to bucket.
*/
static u3h_buck*
_ch_buck_add(u3h_buck* hab_u, u3_noun kev, c3_w *use_w)
{
  c3_w i_w;

  //  if our key is equal to any of the existing keys in the bucket,
  //  then replace that key-value pair with kev.
  //
  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun kov = u3h_slot_to_noun(hab_u->sot_w[i_w]);
    if ( c3y == u3r_sing(u3h(kev), u3h(kov)) ) {
      hab_u->sot_w[i_w] = u3h_noun_to_slot(kev);
      u3z(kov);
      return hab_u;
    }
  }

  //  create mutant bucket with added key-value pair.
  //  Optimize: use u3a_wealloc().
  {
    u3h_buck* bah_u = _ch_buck_new(1 + hab_u->len_w);
    bah_u->sot_w[0] = u3h_noun_be_warm(u3h_noun_to_slot(kev));

    for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
      bah_u->sot_w[i_w + 1] = hab_u->sot_w[i_w];
    }

    u3a_wfree(hab_u);
    *use_w += 1;
    return bah_u;
  }
}

/* _ch_some_add(): add to node or bucket.
*/
static void*
_ch_some_add(void* han_v, c3_w lef_w, c3_w rem_w, u3_noun kev, c3_w *use_w)
{
  if ( 0 == lef_w ) {
    return _ch_buck_add((u3h_buck*)han_v, kev, use_w);
  }
  else return _ch_node_add((u3h_node*)han_v, lef_w, rem_w, kev, use_w);
}

/* _ch_two(): create a new node with two leaves underneath
*/
u3h_slot
_ch_two(u3h_slot had_w, u3h_slot add_w, c3_w lef_w, c3_w ham_w, c3_w mad_w)
{
  void* ret;

  if ( 0 == lef_w ) {
    u3h_buck* hab_u = _ch_buck_new(2);
    ret = hab_u;
    hab_u->sot_w[0] = had_w;
    hab_u->sot_w[1] = add_w;
  }
  else {
    c3_w hop_w, tad_w;
    lef_w -= 5;
    hop_w = ham_w >> lef_w;
    tad_w = mad_w >> lef_w;
    if ( hop_w == tad_w ) {
      // fragments collide: store in a child node.
      u3h_node* han_u = _ch_node_new(1);
      ret             = han_u;
      han_u->map_w    = (c3_w)1 << hop_w;
      ham_w           = CUT_END(ham_w, lef_w);
      mad_w           = CUT_END(mad_w, lef_w);
      han_u->sot_w[0] = _ch_two(had_w, add_w, lef_w, ham_w, mad_w);
    }
    else {
      u3h_node* han_u = _ch_node_new(2);
      ret             = han_u;
      han_u->map_w    = ((c3_w)1 << hop_w) | ((c3_w)1 << tad_w);
      // smaller mug fragments go in earlier slots
      if ( hop_w < tad_w ) {
        han_u->sot_w[0] = had_w;
        han_u->sot_w[1] = add_w;
      }
      else {
        han_u->sot_w[0] = add_w;
        han_u->sot_w[1] = had_w;
      }
    }
  }

  return u3h_node_to_slot(ret);
}

/* _ch_slot_put(): store a key-value pair in a non-null slot
*/
static void
_ch_slot_put(u3h_slot* sot_w, u3_noun kev, c3_w lef_w, c3_w rem_w, c3_w* use_w)
{
  if ( c3n == u3h_slot_is_noun(*sot_w) ) {
    void* hav_v = _ch_some_add(u3h_slot_to_node(*sot_w),
                               lef_w,
                               rem_w,
                               kev,
                               use_w);

    u3_assert( c3y == u3h_slot_is_node(*sot_w) );
    *sot_w = u3h_node_to_slot(hav_v);
  }
  else {
    u3_noun  kov   = u3h_slot_to_noun(*sot_w);
    u3h_slot add_w = u3h_noun_be_warm(u3h_noun_to_slot(kev));
    if ( c3y == u3r_sing(u3h(kev), u3h(kov)) ) {
      // replace old value
      u3z(kov);
      *sot_w = add_w;
    }
    else {
      c3_w ham_w = CUT_END(u3r_mug(u3h(kov)), lef_w);
      *sot_w     = _ch_two(*sot_w, add_w, lef_w, ham_w, rem_w);
      *use_w    += 1;
    }
  }
}

/* u3h_put_get(): insert in caching hashtable, returning deleted key-value pair
**
** `key` is RETAINED; `val` is transferred.
*/
u3_weak
u3h_put_get(u3p(u3h_root) har_p, u3_noun key, u3_noun val)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  u3_noun   kev   = u3nc(u3k(key), val);
  c3_w      mug_w = u3r_mug(key);
  c3_w      inx_w = (mug_w >> 25);  //  6 bits
  c3_w      rem_w = CUT_END(mug_w, 25);
  u3h_slot* sot_w = &(har_u->sot_w[inx_w]);

  if ( c3y == u3h_slot_is_null(*sot_w) ) {
    *sot_w = u3h_noun_be_warm(u3h_noun_to_slot(kev));
    har_u->use_w += 1;
  }
  else {
    _ch_slot_put(sot_w, kev, 25, rem_w, &(har_u->use_w));
  }

  {
    u3_weak ret = u3_none;

    if ( har_u->max_w && (har_u->use_w > har_u->max_w) ) {
      do {
        ret = _ch_trim_root(har_u);
      }
      while ( u3_none == ret );
      har_u->use_w -= 1;
    }

    return ret;
  }
}

/* u3h_put(): insert in hashtable.
**
** `key` is RETAINED; `val` is transferred.
*/
void
u3h_put(u3p(u3h_root) har_p, u3_noun key, u3_noun val)
{
  u3_weak del = u3h_put_get(har_p, key, val);
  if ( u3_none != del ) {
    u3z(del);
  }
}

/* _ch_buck_del(): delete from bucket
*/
static c3_o
_ch_buck_del(u3h_slot* sot_w, u3_noun key)
{
  u3h_buck* hab_u = u3h_slot_to_node(*sot_w);
  c3_w fin_w = hab_u->len_w;
  c3_w i_w;
  //
  //  find index of key to be deleted
  //
  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun kov = u3h_slot_to_noun(hab_u->sot_w[i_w]);
    if ( c3y == u3r_sing(key, u3h(kov)) ) {
      fin_w = i_w;
      u3z(kov);
      break;
    }
  }

  // no key found, no-op
  if ( fin_w == hab_u->len_w ) {
    return c3n;
  }

  {
    hab_u->len_w--;
    // u3_assert(c3y == u3h_slot_is_noun(hab_u->sot_w[fin_w]));
    for ( i_w = fin_w;  i_w < hab_u->len_w; i_w++ ) {
      hab_u->sot_w[i_w] = hab_u->sot_w[i_w + 1];
    }

    return c3y;
  }
}

static c3_o _ch_some_del(u3h_slot*, u3_noun, c3_w, c3_w);

/* _ch_slot_del(): delete from slot
*/
static c3_o
_ch_slot_del(u3h_slot* sot_w, u3_noun key, c3_w lef_w, c3_w rem_w)
{
  if ( c3y == u3h_slot_is_noun(*sot_w) ) {
    u3_noun kev = u3h_slot_to_noun(*sot_w);
    *sot_w = 0;
    u3z(kev);
    return c3y;
  }
  else {
    return _ch_some_del(sot_w, key, lef_w, rem_w);
  }
}

/* _ch_slot_del(): delete from node
*/
static c3_o
_ch_node_del(u3h_slot* sot_w, u3_noun key, c3_w lef_w, c3_w rem_w)
{
  u3h_node* han_u = (u3h_node*) u3h_slot_to_node(*sot_w);
  u3h_slot* tos_w;

  c3_w bit_w, inx_w, map_w, i_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;
  inx_w = _ch_popcount(CUT_END(map_w, bit_w));

  tos_w = &(han_u->sot_w[inx_w]);

  // nothing at slot, no-op
  if ( !BIT_SET(map_w, bit_w) ) {
    return c3n;
  }

  if ( c3n == _ch_slot_del(tos_w, key, lef_w, rem_w) ) {
    // nothing deleted
    return c3n;
  }
  else if ( 0 != *tos_w  ) {
    // something deleted, but slot still has value
    return c3y;
  }
  else {
    // shrink!
    c3_w i_w, ken_w, len_w = _ch_popcount(map_w);
    u3h_slot  kes_w;

    if ( 2 == len_w && ((ken_w = (0 == inx_w) ? 1 : 0),
                        (kes_w = han_u->sot_w[ken_w]),
                        (c3y == u3h_slot_is_noun(kes_w))) )
    {
      // only one side left, and the other is a noun. debucketize.
      *sot_w = kes_w;
      u3a_wfree(han_u);
    }
    else {
      // shrink node in place; don't reallocate, we could be low on memory
      //
      han_u->map_w &= ~(1 << bit_w);
      --len_w;

      for ( i_w = inx_w; i_w < len_w; i_w++ ) {
        han_u->sot_w[i_w] = han_u->sot_w[i_w + 1];
      }
    }
    return c3y;
  }
}

/* _ch_some_del(): delete from node or buck
*/
static c3_o
_ch_some_del(u3h_slot* sot_w, u3_noun key, c3_w lef_w, c3_w rem_w)
{
  if ( 0 == lef_w ) {
    return _ch_buck_del(sot_w, key);
  }

  return _ch_node_del(sot_w, key, lef_w, rem_w);
}

/* u3h_del(); delete from hashtable.
*/
void
u3h_del(u3p(u3h_root) har_p, u3_noun key)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w      mug_w = u3r_mug(key);
  c3_w      inx_w = (mug_w >> 25);
  c3_w      rem_w = CUT_END(mug_w, 25);
  u3h_slot* sot_w = &(har_u->sot_w[inx_w]);

  if (  (c3n == u3h_slot_is_null(*sot_w))
     && (c3y == _ch_slot_del(sot_w, key, 25, rem_w)) )
  {
    har_u->use_w--;
  }
}

/* _ch_uni_with(): key/value callback, put into [*wit]
*/
static void
_ch_uni_with(u3_noun kev, void* wit)
{
  u3p(u3h_root) har_p = *(u3p(u3h_root)*)wit;
  u3_noun key, val;
  u3x_cell(kev, &key, &val);

  u3h_put(har_p, key, u3k(val));
}

/* u3h_uni(): unify hashtables, copying [rah_p] into [har_p]
*/
void
u3h_uni(u3p(u3h_root) har_p, u3p(u3h_root) rah_p)
{
  u3h_walk_with(rah_p, _ch_uni_with, &har_p);
}

/* _ch_trim_node(): trim one entry from a node slot or its children
*/
static u3_weak
_ch_trim_node(u3h_root* har_u, u3h_slot* sot_w, c3_w lef_w, c3_w rem_w)
{
  c3_w bit_w, map_w, inx_w;
  u3h_slot* tos_w;
  u3h_node* han_u = (u3h_node*) u3h_slot_to_node(*sot_w);

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  map_w = han_u->map_w;

  if ( !BIT_SET(map_w, bit_w) ) {
    har_u->arm_u.mug_w = _ch_skip_slot(har_u->arm_u.mug_w, lef_w);
    return c3n;
  }

  rem_w = CUT_END(rem_w, lef_w);
  inx_w = _ch_popcount(CUT_END(map_w, bit_w));
  tos_w = &(han_u->sot_w[inx_w]);

  u3_weak ret = _ch_trim_slot(har_u, tos_w, lef_w, rem_w);
  if ( (u3_none != ret) && (0 == *tos_w) ) {
    // shrink!
    c3_w i_w, ken_w, len_w = _ch_popcount(map_w);
    u3h_slot  kes_w;

    if ( 2 == len_w && ((ken_w = (0 == inx_w) ? 1 : 0),
                        (kes_w = han_u->sot_w[ken_w]),
                        (c3y == u3h_slot_is_noun(kes_w))) ) {
      // only one side left, and the other is a noun. debucketize.
      *sot_w = kes_w;
      u3a_wfree(han_u);
    }
    else {
      // shrink node in place; don't reallocate, we could be low on memory
      //
      han_u->map_w &= ~(1 << bit_w);
      --len_w;

      for ( i_w = inx_w; i_w < len_w; i_w++ ) {
        han_u->sot_w[i_w] = han_u->sot_w[i_w + 1];
      }
    }
  }
  return ret;
}

/* _ch_trim_kev(): trim a single entry slot
*/
static u3_weak
_ch_trim_kev(u3h_slot *sot_w)
{
  if ( _(u3h_slot_is_warm(*sot_w)) ) {
    *sot_w = u3h_noun_be_cold(*sot_w);
    return u3_none;
  }
  else {
    u3_noun kev = u3h_slot_to_noun(*sot_w);
    *sot_w = 0;
    return kev;
  }
}

/* _ch_trim_node(): trim one entry from a bucket slot
*/
static u3_weak
_ch_trim_buck(u3h_root* har_u, u3h_slot* sot_w)
{
  c3_w i_w, len_w;
  u3h_buck* hab_u = u3h_slot_to_node(*sot_w);

  for ( len_w = hab_u->len_w;
        har_u->arm_u.inx_w < len_w;
        har_u->arm_u.inx_w += 1 )
  {
    u3_weak ret = _ch_trim_kev(&(hab_u->sot_w[har_u->arm_u.inx_w]));
    if ( u3_none != ret ) {
      if ( 2 == len_w ) {
        // 2 things in bucket: debucketize to key-value pair, the next
        // run will point at this pair (same mug_w, no longer in bucket)
        *sot_w = hab_u->sot_w[ (0 == har_u->arm_u.inx_w) ? 1 : 0 ];
        u3a_wfree(hab_u);
        har_u->arm_u.inx_w = 0;
      }
      else {
        // shrink bucket in place; don't reallocate, we could be low on memory
        hab_u->len_w = --len_w;

        for ( i_w = har_u->arm_u.inx_w; i_w < len_w; ++i_w ) {
          hab_u->sot_w[i_w] = hab_u->sot_w[i_w + 1];
        }
        // leave the arm pointing at the next index in the bucket
        ++(har_u->arm_u.inx_w);
      }
      return ret;
    }
  }

  har_u->arm_u.mug_w = (har_u->arm_u.mug_w + 1) & 0x7FFFFFFF; // modulo 2^31
  har_u->arm_u.inx_w = 0;
  return u3_none;
}

/* _ch_trim_some(): trim one entry from a bucket or node slot
*/
static u3_weak
_ch_trim_some(u3h_root* har_u, u3h_slot* sot_w, c3_w lef_w, c3_w rem_w)
{
  if ( 0 == lef_w ) {
    return _ch_trim_buck(har_u, sot_w);
  }
  else {
    return _ch_trim_node(har_u, sot_w, lef_w, rem_w);
  }
}

/* _ch_skip_slot(): increment arm over hash prefix.
*/
c3_w
_ch_skip_slot(c3_w mug_w, c3_w lef_w)
{
  c3_w hig_w = mug_w >> lef_w;
  c3_w new_w = CUT_END(hig_w + 1, (31 - lef_w)); // modulo 2^(31 - lef_w)
  return new_w << lef_w;
}

/* _ch_trim_slot(): trim one entry from a non-bucket slot
*/
static u3_weak
_ch_trim_slot(u3h_root* har_u, u3h_slot *sot_w, c3_w lef_w, c3_w rem_w)
{
  if ( c3y == u3h_slot_is_noun(*sot_w) ) {
    har_u->arm_u.mug_w = _ch_skip_slot(har_u->arm_u.mug_w, lef_w);
    return _ch_trim_kev(sot_w);
  }
  else {
    return _ch_trim_some(har_u, sot_w, lef_w, rem_w);
  }
}

/* _ch_trim_root(): trim one entry from a hashtable
*/
static u3_weak
_ch_trim_root(u3h_root* har_u)
{
  c3_w      mug_w = har_u->arm_u.mug_w;
  c3_w      inx_w = mug_w >> 25; // 6 bits
  u3h_slot* sot_w = &(har_u->sot_w[inx_w]);

  if ( c3y == u3h_slot_is_null(*sot_w) ) {
    har_u->arm_u.mug_w = _ch_skip_slot(har_u->arm_u.mug_w, 25);
    return u3_none;
  }

  return _ch_trim_slot(har_u, sot_w, 25, CUT_END(mug_w, 25));
}

/* u3h_trim_to(): trim to n key-value pairs
*/
void
u3h_trim_to(u3p(u3h_root) har_p, c3_w n_w)
{
  u3h_trim_with(har_p, n_w, u3a_lose);
}

/* u3h_trim_to(): trim to n key-value pairs
*/
void
u3h_trim_with(u3p(u3h_root) har_p, c3_w n_w, void (*del_cb)(u3_noun))
{
  u3h_root* har_u = u3to(u3h_root, har_p);

  while ( har_u->use_w > n_w ) {
    u3_weak del = _ch_trim_root(har_u);
    if ( u3_none != del ) {
      har_u->use_w -= 1;
      del_cb(del);
    }
  }
}

/* _ch_buck_hum(): read in bucket.
*/
static c3_o
_ch_buck_hum(u3h_buck* hab_u, c3_w mug_w)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    if ( mug_w == u3r_mug(u3h(u3h_slot_to_noun(hab_u->sot_w[i_w]))) ) {
      return c3y;
    }
  }
  return c3n;
}

/* _ch_node_hum(): read in node.
*/
static c3_o
_ch_node_hum(u3h_node* han_u, c3_w lef_w, c3_w rem_w, c3_w mug_w)
{
  c3_w bit_w, map_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;

  if ( !BIT_SET(map_w, bit_w) ) {
    return c3n;
  }
  else {
    c3_w inx_w = _ch_popcount(CUT_END(map_w, bit_w));
    c3_w sot_w = han_u->sot_w[inx_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      if ( mug_w == u3r_mug(u3h(kev)) ) {
        return c3y;
      }
      else {
        return c3n;
      }
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        return _ch_buck_hum(hav_v, mug_w);
      }
      else return _ch_node_hum(hav_v, lef_w, rem_w, mug_w);
    }
  }
}

/* u3h_hum(): check presence in hashtable.
**
** `key` is RETAINED.
*/
c3_o
u3h_hum(u3p(u3h_root) har_p, c3_w mug_w)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w      inx_w = (mug_w >> 25);
  c3_w      rem_w = CUT_END(mug_w, 25);
  c3_w      sot_w = har_u->sot_w[inx_w];

  if ( _(u3h_slot_is_null(sot_w)) ) {
    return c3n;
  }
  else if ( _(u3h_slot_is_noun(sot_w)) ) {
    u3_noun kev = u3h_slot_to_noun(sot_w);

    if ( mug_w == u3r_mug(u3h(kev)) ) {
      return c3y;
    }
    else {
      return c3n;
    }
  }
  else {
    u3h_node* han_u = u3h_slot_to_node(sot_w);

    return _ch_node_hum(han_u, 25, rem_w, mug_w);
  }
}

/* _ch_buck_git(): read in bucket.
*/
static u3_weak
_ch_buck_git(u3h_buck* hab_u, u3_noun key)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun kev = u3h_slot_to_noun(hab_u->sot_w[i_w]);
    if ( _(u3r_sing(key, u3h(kev))) ) {
      return u3t(kev);
    }
  }
  return u3_none;
}

/* _ch_node_git(): read in node.
*/
static u3_weak
_ch_node_git(u3h_node* han_u, c3_w lef_w, c3_w rem_w, u3_noun key)
{
  c3_w bit_w, map_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = CUT_END(rem_w, lef_w);
  map_w = han_u->map_w;

  if ( !BIT_SET(map_w, bit_w) ) {
    return u3_none;
  }
  else {
    c3_w inx_w = _ch_popcount(CUT_END(map_w, bit_w));
    c3_w sot_w = han_u->sot_w[inx_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      if ( _(u3r_sing(key, u3h(kev))) ) {
        return u3t(kev);
      }
      else {
        return u3_none;
      }
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        return _ch_buck_git(hav_v, key);
      }
      else return _ch_node_git(hav_v, lef_w, rem_w, key);
    }
  }
}

/* u3h_git(): read from hashtable.
**
** `key` is RETAINED; result is RETAINED.
*/
u3_weak
u3h_git(u3p(u3h_root) har_p, u3_noun key)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w      mug_w = u3r_mug(key);
  c3_w      inx_w = (mug_w >> 25);
  c3_w      rem_w = CUT_END(mug_w, 25);
  c3_w      sot_w = har_u->sot_w[inx_w];

  if ( _(u3h_slot_is_null(sot_w)) ) {
    return u3_none;
  }
  else if ( _(u3h_slot_is_noun(sot_w)) ) {
    u3_noun kev = u3h_slot_to_noun(sot_w);

    if ( _(u3r_sing(key, u3h(kev))) ) {
      har_u->sot_w[inx_w] = u3h_noun_be_warm(sot_w);
      return u3t(kev);
    }
    else {
      return u3_none;
    }
  }
  else {
    u3h_node* han_u = u3h_slot_to_node(sot_w);

    return _ch_node_git(han_u, 25, rem_w, key);
  }
}

/* u3h_get(): read from hashtable, incrementing refcount.
**
** `key` is RETAINED; result is PRODUCED.
*/
u3_weak
u3h_get(u3p(u3h_root) har_p, u3_noun key)
{
  u3_noun pro = u3h_git(har_p, key);

  if ( u3_none != pro ) {
    u3k(pro);
  }
  return pro;
}

/* _ch_free_buck(): free bucket
*/
static void
_ch_free_buck(u3h_buck* hab_u)
{
  //fprintf(stderr, "free buck\r\n");
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3z(u3h_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_wfree(hab_u);
}

/* _ch_free_node(): free node.
*/
static void
_ch_free_node(u3h_node* han_u, c3_w lef_w, c3_o pin_o)
{
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];
    if ( _(u3h_slot_is_null(sot_w))) {
    }  else if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3z(u3h_slot_to_noun(sot_w));
    } else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_free_buck(hav_v);
      } else {
        _ch_free_node(hav_v, lef_w, pin_o);
      }
    }
  }
  u3a_wfree(han_u);
}

/* u3h_free(): free hashtable.
*/
void
u3h_free(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3z(u3h_slot_to_noun(sot_w));
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_slot_to_node(sot_w);

      _ch_free_node(han_u, 25, i_w == 57);
    }
  }
  u3a_wfree(har_u);
}

/* _ch_walk_buck(): walk bucket for gc.
*/
static void
_ch_walk_buck(u3h_buck* hab_u, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    fun_f(u3h_slot_to_noun(hab_u->sot_w[i_w]), wit);
  }
}

/* _ch_walk_node(): walk node for gc.
*/
static void
_ch_walk_node(u3h_node* han_u, c3_w lef_w, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_walk_with(): traverse hashtable with key, value fn and data
 *                  argument; RETAINS.
*/
void
u3h_walk_with(u3p(u3h_root) har_p,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_slot_to_node(sot_w);

      _ch_walk_node(han_u, 25, fun_f, wit);
    }
  }
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
u3h_walk(u3p(u3h_root) har_p, void (*fun_f)(u3_noun))
{
  u3h_walk_with(har_p, _ch_walk_plain, (void *)fun_f);
}

/* _ch_take_noun(): take key and call [fun_f] on val.
*/
static u3h_slot
_ch_take_noun(u3h_slot sot_w, u3_funk fun_f)
{
  u3_noun kov = u3h_slot_to_noun(sot_w);
  u3_noun kev = u3nc(u3a_take(u3h(kov)),
                     fun_f(u3t(kov)));

  return u3h_noun_to_slot(kev);
}

/* _ch_take_buck(): take bucket and contents
*/
static u3h_slot
_ch_take_buck(u3h_slot sot_w, u3_funk fun_f)
{
  u3h_buck* hab_u = u3h_slot_to_node(sot_w);
  u3h_buck* bah_u = _ch_buck_new(hab_u->len_w);
  c3_w        i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    bah_u->sot_w[i_w] = _ch_take_noun(hab_u->sot_w[i_w], fun_f);
  }

  return u3h_node_to_slot(bah_u);
}

/* _ch_take_node(): take node and contents
*/
static u3h_slot
_ch_take_node(u3h_slot sot_w, c3_w lef_w, u3_funk fun_f)
{
  u3h_node* han_u = u3h_slot_to_node(sot_w);
  c3_w      len_w = _ch_popcount(han_u->map_w);
  u3h_node* nah_u = _ch_node_new(len_w);
  c3_w       i_w;

  nah_u->map_w = han_u->map_w;
  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w        tos_w = han_u->sot_w[i_w];
    nah_u->sot_w[i_w] = ( c3y == u3h_slot_is_noun(tos_w) )
                        ? _ch_take_noun(tos_w, fun_f)
                        :  ( 0 == lef_w )
                           ? _ch_take_buck(tos_w, fun_f)
                           : _ch_take_node(tos_w, lef_w, fun_f);
  }

  return u3h_node_to_slot(nah_u);
}

/* u3h_take_with(): gain hashtable, copying junior keys
** and calling [fun_f] on values
*/
u3p(u3h_root)
u3h_take_with(u3p(u3h_root) har_p, u3_funk fun_f)
{
  u3h_root*     har_u = u3to(u3h_root, har_p);
  u3p(u3h_root) rah_p = u3h_new_cache(har_u->max_w);
  u3h_root*     rah_u = u3to(u3h_root, rah_p);
  c3_w     sot_w, i_w;

  rah_u->use_w = har_u->use_w;
  rah_u->arm_u = har_u->arm_u;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    sot_w = har_u->sot_w[i_w];
    if ( c3n == u3h_slot_is_null(sot_w) ) {
      rah_u->sot_w[i_w] = ( c3y == u3h_slot_is_noun(sot_w) )
                          ? _ch_take_noun(sot_w, fun_f)
                          : _ch_take_node(sot_w, 25, fun_f);
    }
  }

  return rah_p;
}

/* u3h_take(): gain hashtable, copying junior nouns
*/
u3p(u3h_root)
u3h_take(u3p(u3h_root) har_p)
{
  return u3h_take_with(har_p, u3a_take);
}

/* _ch_mark_buck(): mark bucket for gc.
*/
c3_w
_ch_mark_buck(u3h_buck* hab_u)
{
  c3_w tot_w = 0;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    tot_w += u3a_mark_noun(u3h_slot_to_noun(hab_u->sot_w[i_w]));
  }
  tot_w += u3a_mark_ptr(hab_u);

  return tot_w;
}

/* _ch_mark_node(): mark node for gc.
*/
c3_w
_ch_mark_node(u3h_node* han_u, c3_w lef_w)
{
  c3_w tot_w = 0;
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      tot_w += u3a_mark_noun(kev);
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        tot_w += _ch_mark_buck(hav_v);
      } else {
        tot_w += _ch_mark_node(hav_v, lef_w);
      }
    }
  }

  tot_w += u3a_mark_ptr(han_u);

  return tot_w;
}

/* u3h_mark(): mark hashtable for gc.
*/
c3_w
u3h_mark(u3p(u3h_root) har_p)
{
  c3_w tot_w = 0;
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      tot_w += u3a_mark_noun(kev);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_slot_to_node(sot_w);

      tot_w += _ch_mark_node(han_u, 25);
    }
  }

  tot_w += u3a_mark_ptr(har_u);

  return tot_w;
}

/* _ch_rewrite_buck(): rewrite buck for compaction.
*/
void
_ch_rewrite_buck(u3h_buck* hab_u)
{
  if ( c3n == u3a_rewrite_ptr(hab_u) ) return;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun som = u3h_slot_to_noun(hab_u->sot_w[i_w]);
    hab_u->sot_w[i_w] = u3h_noun_to_slot(u3a_rewritten_noun(som));
    u3a_rewrite_noun(som);
  }
}

/* _ch_rewrite_node(): rewrite node for compaction.
*/
void
_ch_rewrite_node(u3h_node* han_u, c3_w lef_w)
{
  if ( c3n == u3a_rewrite_ptr(han_u) ) return;

  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);
      han_u->sot_w[i_w] = u3h_noun_to_slot(u3a_rewritten_noun(kev));

      u3a_rewrite_noun(kev);
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);
      u3h_node* nod_u = u3to(u3h_node,u3a_rewritten(u3of(u3h_node,hav_v)));
      han_u->sot_w[i_w] = u3h_node_to_slot(nod_u);

      if ( 0 == lef_w ) {
        _ch_rewrite_buck(hav_v);
      } else {
        _ch_rewrite_node(hav_v, lef_w);
      }
    }
  }
}

/* u3h_rewrite(): rewrite pointers during compaction.
*/
void
u3h_rewrite(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  if ( c3n == u3a_rewrite_ptr(har_u) ) return;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);
      har_u->sot_w[i_w] = u3h_noun_to_slot(u3a_rewritten_noun(kev));

      u3a_rewrite_noun(kev);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_slot_to_node(sot_w);
      u3h_node* nod_u = u3to(u3h_node,u3a_rewritten(u3of(u3h_node,han_u)));
      har_u->sot_w[i_w] = u3h_node_to_slot(nod_u);

      _ch_rewrite_node(han_u, 25);
    }
  }
}

/* _ch_count_buck(): count bucket for gc.
*/
c3_w
_ch_count_buck(u3h_buck* hab_u)
{
  c3_w tot_w = 0;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    tot_w += u3a_count_noun(u3h_slot_to_noun(hab_u->sot_w[i_w]));
  }
  tot_w += u3a_count_ptr(hab_u);

  return tot_w;
}

/* _ch_count_node(): count node for gc.
*/
c3_w
_ch_count_node(u3h_node* han_u, c3_w lef_w)
{
  c3_w tot_w = 0;
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      tot_w += u3a_count_noun(kev);
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        tot_w += _ch_count_buck(hav_v);
      } else {
        tot_w += _ch_count_node(hav_v, lef_w);
      }
    }
  }

  tot_w += u3a_count_ptr(han_u);

  return tot_w;
}

/* u3h_count(): count hashtable for gc.
*/
c3_w
u3h_count(u3p(u3h_root) har_p)
{
  c3_w tot_w = 0;
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      tot_w += u3a_count_noun(kev);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_slot_to_node(sot_w);

      tot_w += _ch_count_node(han_u, 25);
    }
  }

  tot_w += u3a_count_ptr(har_u);

  return tot_w;
}

/* _ch_discount_buck(): discount bucket for gc.
*/
c3_w
_ch_discount_buck(u3h_buck* hab_u)
{
  c3_w tot_w = 0;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    tot_w += u3a_discount_noun(u3h_slot_to_noun(hab_u->sot_w[i_w]));
  }
  tot_w += u3a_discount_ptr(hab_u);

  return tot_w;
}

/* _ch_discount_node(): discount node for gc.
*/
c3_w
_ch_discount_node(u3h_node* han_u, c3_w lef_w)
{
  c3_w tot_w = 0;
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      tot_w += u3a_discount_noun(kev);
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        tot_w += _ch_discount_buck(hav_v);
      } else {
        tot_w += _ch_discount_node(hav_v, lef_w);
      }
    }
  }

  tot_w += u3a_discount_ptr(han_u);

  return tot_w;
}

/* u3h_discount(): discount hashtable for gc.
*/
c3_w
u3h_discount(u3p(u3h_root) har_p)
{
  c3_w tot_w = 0;
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      tot_w += u3a_discount_noun(kev);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_slot_to_node(sot_w);

      tot_w += _ch_discount_node(han_u, 25);
    }
  }

  tot_w += u3a_discount_ptr(har_u);

  return tot_w;
}

/* u3h_wyt(): number of entries
*/
c3_w
u3h_wyt(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  return har_u->use_w;
}
