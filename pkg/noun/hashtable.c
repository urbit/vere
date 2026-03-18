/// @file

#include "hashtable.h"

#include "allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "xtract.h"
#include "options.h"

#define BEX32_PHI 0x9e3779b9u  // 2^32 divided by golden ratio

/* asserting noun deconstruction to make sure hashtables are bail-safe
*/
static inline u3_noun
_h_hed(u3_noun som)
{
  u3_assert( _(u3a_is_cell(som)) );
  return ((u3a_cell *)u3a_to_ptr(som))->hed;
}

static inline u3_noun
_h_tel(u3_noun som)
{
  u3_assert( _(u3a_is_cell(som)) );
  return ((u3a_cell *)u3a_to_ptr(som))->tel;
}

static void
u3h_new_cache_sized(u3h_root* har_u, c3_w siz_w, c3_w max_w)
{
  u3_assert(1 == c3_pc_w(siz_w));

  har_u->use_w = 0;
  har_u->loc_w = siz_w;
  har_u->max_w = max_w;
  har_u->fil_w = 0;
  har_u->arm_w = 0;
  u3h_slot* sot_u = u3a_walloc(siz_w);
  har_u->sot_p = u3of(u3_noun, sot_u);
  memset(sot_u, u3h_slot_free, siz_w * sizeof(c3_w));
}

/* u3h_new_cache(): create hashtable with bounded size.
*/
void
u3h_new_cache(u3h_root* har_u, c3_w max_w)
{
  u3h_new_cache_sized(har_u, u3h_size_min, max_w);
}

/* u3h_new(): create hashtable.
*/
void
u3h_new(u3h_root* har_u)
{
  return u3h_new_cache(har_u, 0);
}

//  walk to an empty slot. naturally it would skip equal keys.
//  it is used when rehashing a table to avoid doing extra work, since there
//  are no duplicate keys and no tombstones in the new table
//
//  key is RETAINED
static inline c3_w
_h_walk_empty(u3h_root* har_u, u3_noun key)
{
  u3_assert(har_u->fil_w < har_u->loc_w);

  c3_w mug_w = u3r_mug(key),
       mak_w = har_u->loc_w - 1,
       idx_w = mug_w & mak_w,
       inc_w = 1;

  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  u3h_slot old_u;

  while ( (old_u = sot_u[idx_w]) ) {
    idx_w = (idx_w + inc_w) & mak_w;
    inc_w++;
  }

  return idx_w;
}

// walk to an empty slot or a slot with the same key, whichever comes first
// returns the index we walked to or the index of the first tombstone if the
// key was not found and we did see a tombstone
//
// key is RETAINED
static inline c3_w
_h_walk(u3h_root* har_u, u3_noun key)
{
  c3_w mug_w = u3r_mug(key),
       mak_w = har_u->loc_w - 1,
       idx_w = mug_w & mak_w,
       inc_w = 1;

  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  u3h_slot old_u;
  c3_w tom_w = u3_none;

  while ( (old_u = sot_u[idx_w]) ) {
    if ( u3h_slot_tomb != old_u ) {
      if ( c3y == u3r_sing(key, _h_hed(u3h_slot_to_noun(old_u))) ) {
        return idx_w;
      }
    }
    else {
      tom_w = (u3_none == tom_w) ? idx_w : tom_w;
    }

    idx_w = (idx_w + inc_w) & mak_w;
    inc_w++;
  }

  return ( u3_none == tom_w ) ? idx_w : tom_w;
}

#define _h_for_full(HAR, KEV, ...)                                              \
  do {                                                                          \
    u3h_slot* sot_u = u3to(u3_noun, HAR->sot_p);                                \
    u3_noun KEV;                                                                \
    for (c3_w i_w = 0; i_w < HAR->loc_w; i_w++) {                               \
      if ( sot_u[i_w] <= u3h_slot_tomb) continue;                               \
      KEV = u3h_slot_to_noun(sot_u[i_w]);                                       \
      __VA_ARGS__                                                               \
    }                                                                           \
  } while (0)

void
u3h_walk_with(u3h_root* har_u,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  _h_for_full(har_u, kev,
    fun_f(kev, wit);
  );
}

static void
_ch_uni_steal(u3_noun kev, void* wit)
{
  u3h_root* har_u = wit;
  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  c3_w idx_w = _h_walk_empty(har_u, _h_hed(kev));

  sot_u[idx_w] = kev;
  har_u->use_w++;
  har_u->fil_w++;
}

static void
_h_uni_steal(u3h_root* har_u, u3h_root* rah_u)
{
  u3h_walk_with(rah_u, _ch_uni_steal, har_u);
}

static c3_o
_h_rehash_grow(u3h_root* har_u, c3_o inc_o)
{
  c3_w loc_w = har_u->loc_w;
  //  assert that loc_w is a power of two
  u3_assert(1 == c3_pc_w(loc_w));

  //  if the table is full of tombstones and rehashing would be enough to double
  //  its capacity, don't grow
  inc_o = c3a(inc_o,
    __( (har_u->use_w * 2 + 1) * 100 >= (har_u->loc_w * u3h_grow_threshold) )
  );

  if ( c3y == inc_o ) {
    loc_w *= 2;
  }

  if ( loc_w < har_u->loc_w ) return u3m_bail(c3__fail);

  u3h_root old_u = *har_u;

  har_u->use_w = 0;
  har_u->loc_w = loc_w;
  har_u->fil_w = 0;
  u3h_slot* sot_u = u3a_walloc(loc_w);
  har_u->sot_p    = u3of(u3_noun, sot_u);
  memset(sot_u, u3h_slot_free, loc_w * sizeof(c3_w));

  _h_uni_steal(har_u, &old_u);
  u3a_wfree(u3to(u3h_root, old_u.sot_p));
  return c3y;
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

static void
_h_trim_one(u3h_root* har_u)
{
  u3_assert(har_u->use_w <= har_u->fil_w);
  if ( 0 == har_u->use_w ) return;
  c3_w idx_w = har_u->arm_w,
       mak_w = har_u->loc_w - 1,
       bit_w = c3_bits_word(mak_w),
       tep_w = (BEX32_PHI >> (32 - bit_w)) | 1;  // har_u->loc_w / phi
  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  while ( 1 ) {
    if ( sot_u[idx_w] > u3h_slot_tomb ) {
      if ( c3y == u3h_slot_is_warm(sot_u[idx_w]) ) {
        sot_u[idx_w] = u3h_slot_be_cold(sot_u[idx_w]);
      }
      else {
        u3_noun old = u3h_slot_to_noun(sot_u[idx_w]);
        sot_u[idx_w] = u3h_slot_tomb;
        u3z(old);
        har_u->use_w--;
        idx_w = (idx_w + tep_w) & mak_w;
        break;
      }
    }
    idx_w = (idx_w + tep_w) & mak_w;
  }

  har_u->arm_w = idx_w;
}

/* u3h_put(): insert in hashtable.
**
** `key` is RETAINED; `val` is transferred.
*/
void
u3h_put(u3h_root* har_u, u3_noun key, u3_noun val)
{
  c3_t cannot_grow_t = (har_u->max_w) && (har_u->max_w < har_u->loc_w * 2);

  if ( cannot_grow_t
    && ( (har_u->use_w + 1) * 100 >= har_u->loc_w * u3h_trim_threshold ) ) {
      _h_trim_one(har_u);
    }
  
  if ( (har_u->fil_w + 1) * 100 >= har_u->loc_w * u3h_grow_threshold ) {
    _h_rehash_grow(har_u, __(!cannot_grow_t));
  }

  u3_noun kev = u3nc(u3k(key), val);

  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  c3_w idx_w = _h_walk(har_u, key);

  if ( sot_u[idx_w] ) {
    har_u->fil_w--;
    u3h_slot old_u = sot_u[idx_w];
    if ( u3h_slot_tomb != old_u ) {
      har_u->use_w--;
      sot_u[idx_w] = 0;
      u3z(u3h_slot_to_noun(old_u));
    }
  }

  sot_u[idx_w] = kev;
  har_u->use_w++;
  har_u->fil_w++;
}

/* _ch_uni_with(): key/value callback, put into [*wit]
*/
static void
_ch_uni_with(u3_noun kev, void* wit)
{
  u3h_root* har_u = wit;
  u3_noun key, val;
  u3_assert(c3y == u3r_cell(kev, &key, &val));

  u3h_put(har_u, key, u3k(val));
}

/* u3h_uni(): unify hashtables, copying [rah_p] into [har_p]
*/
void
u3h_uni(u3h_root* har_u, u3h_root* rah_u)
{
  u3h_walk_with(rah_u, _ch_uni_with, har_u);
}

/* u3h_get(): read from hashtable.
**
** `key` is RETAINED; result is PRODUCED.
*/
u3_weak
u3h_get(u3h_root* har_u, u3_noun key)
{
  u3_weak val = u3h_git(har_u, key);
  return ( u3_none == val ) ? u3_none : u3k(val);
}

/* u3h_git(): read from hashtable, retaining result.
**
** `key` is RETAINED; result is RETAINED.
*/
u3_weak
u3h_git(u3h_root* har_u, u3_noun key)
{
  c3_w idx_w = _h_walk(har_u, key);
  
  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  
  if ( sot_u[idx_w] <= u3h_slot_tomb ) return u3_none;
  sot_u[idx_w] = u3h_slot_be_warm(sot_u[idx_w]);
  return _h_tel(u3h_slot_to_noun(sot_u[idx_w]));
}

/* u3h_del(); delete from hashtable.
**
** `key` is RETAINED
*/
void
u3h_del(u3h_root* har_u, u3_noun key)
{
  c3_w idx_w = _h_walk(har_u, key);
  
  u3h_slot* sot_u = u3to(u3_noun, har_u->sot_p);
  if ( sot_u[idx_w] <= u3h_slot_tomb ) return;
  u3_noun old = u3h_slot_to_noun(sot_u[idx_w]);
  sot_u[idx_w] = u3h_slot_tomb;
  u3z(old);
  har_u->use_w--;
}

/* u3h_trim_to(): trim to n key-value pairs
*/
void
u3h_trim_to(u3h_root* har_u, c3_w n_w)
{
  while ( har_u->use_w > n_w ) {
    _h_trim_one(har_u);
  }
}

/* u3h_free(): free hashtable.
*/
void
u3h_free(u3h_root* har_u)
{
  _h_for_full(har_u, kev,
    u3z(kev);
  );
  u3a_wfree(u3to(u3_noun, har_u->sot_p));
}

/* u3h_mark(): mark hashtable for gc.
*/
c3_w
u3h_mark(u3h_root* har_u)
{
  c3_w tot_w = 0;
  _h_for_full(har_u, kev,
    tot_w += u3a_mark_noun(kev);
  );
  
  tot_w += u3a_mark_ptr(u3to(u3_noun, har_u->sot_p));
  return tot_w;
}

/* u3h_relocate(): relocate hashtable for compaction.
*/
void
u3h_relocate(u3h_root* har_u)
{
  c3_t fir_t;
  u3_post new_p = u3a_mark_relocate_post(har_u->sot_p, &fir_t);
  har_u->sot_p = new_p;
  if ( !fir_t ) return;
  _h_for_full(har_u, kev, 
    u3a_relocate_noun(&kev);
    sot_u[i_w] = kev;
  );
}

/* u3h_count(): count hashtable for gc.
*/
c3_w
u3h_count(u3h_root* har_u)
{
  // XX not used?
  return 0;
}

/* u3h_discount(): discount hashtable for gc.
*/
c3_w
u3h_discount(u3h_root* har_u)
{
  // XX not used?
  return 0;
}

/* u3h_wyt(): number of entries
*/
c3_w
u3h_wyt(u3h_root* har_u)
{
  return har_u->use_w;
}

typedef struct {
  u3h_root* new_u;
  u3_funk fun_f;
} _take_with_dat;

static void
_h_kev_take_with(u3_noun kov, void* dat_v)
{
  _take_with_dat* dat_u = dat_v;
  u3_noun key = u3a_take(_h_hed(kov));
  u3_noun val = dat_u->fun_f(_h_tel(kov));
  c3_w idx_w = _h_walk_empty(dat_u->new_u, key);
  u3h_slot* sot_u = u3to(u3_noun, dat_u->new_u->sot_p);
  u3_noun kev = u3nc(key, val);
  sot_u[idx_w] = kev;
  dat_u->new_u->use_w++;
  dat_u->new_u->fil_w++;
}

static void
_h_kev_take(u3_noun kov, void* dat_v)
{
  u3h_root* new_u = dat_v;

  u3_noun kev = u3a_take(kov);
  c3_w idx_w = _h_walk_empty(new_u, _h_hed(kev));
  u3h_slot* sot_u = u3to(u3_noun, new_u->sot_p);
  sot_u[idx_w] = kev;
  new_u->use_w++;
  new_u->fil_w++;
}

/* u3h_take_with(): gain hashtable, copying junior keys
** and calling [fun_f] on values
*/
void
u3h_take_with(u3h_root* new_u, u3h_root* har_u, u3_funk fun_f)
{
  u3h_new_cache_sized(new_u, har_u->loc_w, har_u->max_w);
  _take_with_dat dat = {new_u, fun_f};
  u3h_walk_with(har_u, _h_kev_take_with, &dat);
}

/* u3h_take(): gain hashtable, copying junior nouns
*/
void
u3h_take(u3h_root* new_u, u3h_root* har_u)
{
  u3h_new_cache_sized(new_u, har_u->loc_w, har_u->max_w);
  u3h_walk_with(har_u, _h_kev_take, new_u);
}

#undef _h_for_full
