/// @file

#include "hashtable.h"

#include "allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "xtract.h"

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

/* u3h_new_cache(): create hashtable with bounded size.
*/
u3p(u3h_root)
u3h_new_cache(c3_w max_w)
{
  u3h_root* har_u = u3a_walloc(c3_wiseof(u3h_root));
  har_u->use_w = 0;
  har_u->loc_w = u3h_size_min;
  har_u->max_w = max_w;
  u3_noun* kev_u = u3a_walloc(u3h_size_min);
  har_u->kev_p = u3of(u3_noun, kev_u);
  memset(kev_u, 0, u3h_size_min * sizeof(c3_w));
  return u3of(u3h_root, har_u);
}

/* u3h_new(): create hashtable.
*/
u3p(u3h_root)
u3h_new(void)
{
  return u3h_new_cache(0);
}

#define _h_walk_to_empty(...)                                                   \
  do {                                                                          \
    u3_noun old;                                                                \
    c3_w inc_w = 1;                                                             \
    while ( (old = kev_u[idx_w]) && c3n == u3r_sing(key, _h_hed(old)) ) {       \
      __VA_ARGS__                                                               \
      idx_w = (idx_w + inc_w) & mak_w;                                          \
      inc_w++;                                                                  \
    }                                                                           \
  } while (0)

#define _h_for_kev(...)                                                         \
  do {                                                                          \
    u3_noun* kevs_u = u3to(u3_noun, har_u->kev_p);                               \
    c3_w loc_w = har_u->loc_w;                                                  \
    for (c3_w i_w = 0; i_w < loc_w; i_w++) {                                    \
      u3_noun* vek_u = &kevs_u[i_w];                                                 \
      if ( *vek_u ) {                                                              \
        __VA_ARGS__                                                             \
      }                                                                         \
    }                                                                           \
  } while (0)

static void _h_walk_with(u3h_root* har_u,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  _h_for_kev( fun_f(*vek_u, wit); );
}

static void
_h_uni(u3h_root* har_u, u3h_root* rah_u);

static c3_o
_h_expand_table(u3h_root* har_u)
{
  c3_w loc_w = har_u->loc_w;
  //  assert that loc_w is a power of two
  u3_assert(1 == c3_pc_w(loc_w));
  loc_w *= 2;
  if ( loc_w < har_u->loc_w ) return u3m_bail(c3__fail);
  if ( har_u->max_w && loc_w > har_u->max_w ) return c3n;
  u3h_root old_u = {har_u->use_w, har_u->loc_w, har_u->max_w, har_u->kev_p};
  har_u->loc_w = loc_w;
  u3_noun* kev_u = u3a_walloc(loc_w);
  har_u->kev_p   = u3of(u3_noun, kev_u);
  memset(kev_u, 0, loc_w * sizeof(c3_w));
  har_u->use_w = 0;
  _h_uni(har_u, &old_u);
  u3a_wfree(u3to(u3h_root, old_u.kev_p));
  return c3y;
}

void
u3h_walk_with(u3p(u3h_root) har_p,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  _h_walk_with(u3to(u3h_root, har_p), fun_f, wit);
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

static void
_h_put(u3h_root* har_u, u3_noun key, u3_noun val)
{
  u3_noun old, kev = u3nc(u3k(key), val);

  c3_w mug_w = u3r_mug(key),
       mak_w = har_u->loc_w - 1,
       idx_w = mug_w & mak_w;
  
  c3_o del_o = c3n;

  if ( (har_u->use_w + 1) * 100 >= har_u->loc_w * u3h_load_factor_percent ) {
    del_o = _h_expand_table(har_u);
  }

  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);

  if ( c3n == del_o ) {
    _h_walk_to_empty();
  }
  if ( kev_u[idx_w] ) {
    old = kev_u[idx_w];
    kev_u[idx_w] = 0;
    u3z(old);
    har_u->use_w--;
  }

  kev_u[idx_w] = kev;
  har_u->use_w++;
}

/* u3h_put(): insert in hashtable.
**
** `key` is RETAINED; `val` is transferred.
*/
void
u3h_put(u3p(u3h_root) har_p, u3_noun key, u3_noun val)
{
  _h_put(u3to(u3h_root, har_p), key, val);
}

/* _ch_uni_with(): key/value callback, put into [*wit]
*/
static void
_ch_uni_with(u3_noun kev, void* wit)
{
  u3p(u3h_root) har_p = *(u3p(u3h_root)*)wit;
  u3_noun key, val;
  u3_assert(c3y == u3r_cell(kev, &key, &val));

  u3h_put(har_p, key, u3k(val));
}

static void
_ch_uni_with_pointer(u3_noun kev, void* wit)
{
  u3h_root* har_u = wit;
  u3_noun key, val;
  u3_assert(c3y == u3r_cell(kev, &key, &val));

  _h_put(har_u, key, u3k(val));
}

static void
_h_uni(u3h_root* har_u, u3h_root* rah_u)
{
  _h_walk_with(rah_u, _ch_uni_with_pointer, rah_u);
}

/* u3h_uni(): unify hashtables, copying [rah_p] into [har_p]
*/
void
u3h_uni(u3p(u3h_root) har_p, u3p(u3h_root) rah_p)
{
  u3h_walk_with(rah_p, _ch_uni_with, &har_p);
}

/* u3h_get(): read from hashtable.
**
** `key` is RETAINED; result is PRODUCED.
*/
u3_weak
u3h_get(u3p(u3h_root) har_p, u3_noun key)
{
  u3_weak val = u3h_git(har_p, key);
  if ( u3_none == val ) return u3_none;
  return u3k(val);
}

/* u3h_git(): read from hashtable, retaining result.
**
** `key` is RETAINED; result is RETAINED.
*/
u3_weak
u3h_git(u3p(u3h_root) har_p, u3_noun key)
{
  u3h_root* har_u = u3to(u3h_root, har_p);

  c3_w mug_w = u3r_mug(key),
       mak_w = har_u->loc_w - 1,
       idx_w = mug_w & mak_w;
  
  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);
  
  _h_walk_to_empty(
    if ( har_u->use_w * 100 >= har_u->loc_w * u3h_load_factor_percent ) {
      kev_u[idx_w] = 0;
      u3z(old);
      har_u->use_w--;
    }
  );
  if ( 0 == kev_u[idx_w] ) return u3_none;
  return _h_tel(kev_u[idx_w]);
}

/* u3h_del(); delete from hashtable.
**
** `key` is RETAINED
*/
void
u3h_del(u3p(u3h_root) har_p, u3_noun key)
{
  u3h_root* har_u = u3to(u3h_root, har_p);

  c3_w mug_w = u3r_mug(key),
       mak_w = har_u->loc_w - 1,
       idx_w = mug_w & mak_w;
  
  u3_noun old;
  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);
  _h_walk_to_empty(
    if ( har_u->use_w * 100 >= har_u->loc_w * u3h_load_factor_percent ) {
      kev_u[idx_w] = 0;
      u3z(old);
      har_u->use_w--;
    }
  );
  if ( (old = kev_u[idx_w]) ) {
    kev_u[idx_w] = 0;
    u3z(old);
    har_u->use_w--;
  }
}

/* u3h_trim_to(): trim to n key-value pairs
*/
void
u3h_trim_to(u3p(u3h_root) har_p, c3_w n_w)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w idx_w = 0;
  u3_noun old;
  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);
  while ( har_u->use_w > n_w ) {
    u3_assert(idx_w < har_u->loc_w);
    while ( 0 == (old = kev_u[idx_w]) ) idx_w++;
    kev_u[idx_w] = 0;
    u3z(old);
    har_u->use_w--;
  }
}

/* u3h_free(): free hashtable.
*/
void
u3h_free(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);
  _h_for_kev( u3z(*vek_u); );
  u3a_wfree(kev_u);
  u3a_wfree(har_u);
}

/* u3h_mark(): mark hashtable for gc.
*/
c3_w
u3h_mark(u3p(u3h_root) har_p)
{
  c3_w tot_w = 0;
  u3h_root* har_u = u3to(u3h_root, har_p);
  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);
  _h_for_kev(
    tot_w += u3a_mark_noun(*vek_u);
  );
  tot_w += u3a_mark_ptr(kev_u);
  tot_w += u3a_mark_ptr(har_u);
  return tot_w;
}

/* u3h_relocate(): relocate hashtable for compaction.
*/
void
u3h_relocate(u3p(u3h_root) *har_p)
{
  u3_post new_p, old_p = *har_p;
  u3h_root*  har_u = u3to(u3h_root, old_p);
  u3_noun* kev_u = u3to(u3_noun, har_u->kev_p);
  c3_t    fir_t;
  new_p  = u3a_mark_relocate_post(old_p, &fir_t);
  *har_p = new_p;

  if ( !fir_t ) return;

  for (c3_w i_w = 0; i_w < har_u->loc_w; i_w++) {
    if ( kev_u[i_w] ) {
      u3a_relocate_noun(&kev_u[i_w]);
    }
  }
}

/* u3h_count(): count hashtable for gc.
*/
c3_w
u3h_count(u3p(u3h_root) har_p)
{
  // XX not used?
  return 0;
}

/* u3h_discount(): discount hashtable for gc.
*/
c3_w
u3h_discount(u3p(u3h_root) har_p)
{
  // XX not used?
  return 0;
}

/* u3h_wyt(): number of entries
*/
c3_w
u3h_wyt(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  return har_u->use_w;
}

typedef struct {
  u3p(u3h_root) rah_p;
  u3_funk fun_f;
} _take_with_dat;

static void
_h_kev_take_with(u3_noun kov, void* dat_v)
{
  _take_with_dat* dat_u = dat_v;
  u3_noun key = u3a_take(_h_hed(kov));
  u3_noun val = dat_u->fun_f(_h_tel(kov));
  u3h_put(dat_u->rah_p, key, val);
  u3z(key);
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
  _take_with_dat dat = {rah_p, fun_f};

  u3h_walk_with(har_p, _h_kev_take_with, &dat);

  return rah_p;
}

/* u3h_take(): gain hashtable, copying junior nouns
*/
u3p(u3h_root)
u3h_take(u3p(u3h_root) har_p)
{
  return u3h_take_with(har_p, u3a_take);
}
