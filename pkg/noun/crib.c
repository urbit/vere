/// @file

#include "crib.h"
#include "error.h"
#include "log.h"

#ifdef U3_OS_windows
#  include <windows.h>
#endif

#define _CRIB_WIN  16   //  probe window, entries
#define _CRIB_TOMB 1    //  a removed entry: probes continue past it
#define _CRIB_LIS  256  //  slots logged for a cheap clear

/* region primitives: reserve address space, commit as it's used, and
**   give memory back. on POSIX the whole reservation is readable and
**   writable from the start (pages are materialized on first touch), so
**   committing is a no-op.
*/
static size_t
_crib_page(void)
{
  static size_t pag_i;

  if ( !pag_i ) {
#ifdef U3_OS_windows
    SYSTEM_INFO inf_u;
    GetSystemInfo(&inf_u);
    pag_i = inf_u.dwPageSize;
#else
    pag_i = (size_t)sysconf(_SC_PAGESIZE);
#endif
  }
  return pag_i;
}

static void*
_crib_reserve(size_t len_i)
{
#ifdef U3_OS_windows
  void* ptr_v = VirtualAlloc(NULL, len_i, MEM_RESERVE, PAGE_READWRITE);
  u3_assert( ptr_v );
#else
  c3_i flg_i = MAP_PRIVATE | MAP_ANON;
#  ifdef MAP_NORESERVE
  flg_i |= MAP_NORESERVE;
#  endif
  void* ptr_v = mmap(NULL, len_i, PROT_READ | PROT_WRITE, flg_i, -1, 0);
  u3_assert( MAP_FAILED != ptr_v );
#endif
  return ptr_v;
}

static void
_crib_release(void* ptr_v, size_t len_i)
{
#ifdef U3_OS_windows
  (void)len_i;
  VirtualFree(ptr_v, 0, MEM_RELEASE);
#else
  munmap(ptr_v, len_i);
#endif
}

static c3_t
_crib_commit(void* ptr_v, size_t len_i)
{
#ifdef U3_OS_windows
  return NULL != VirtualAlloc(ptr_v, len_i, MEM_COMMIT, PAGE_READWRITE);
#else
  (void)ptr_v; (void)len_i;
  return 1;
#endif
}

/* _crib_drop(): zero [ptr_v, +len_i), giving back its whole pages. the
**   pages come back zero-filled when next touched: on Linux by
**   madvise(MADV_DONTNEED), on Windows by decommitting and committing
**   again, and elsewhere (macOS, BSD), where MADV_DONTNEED doesn't
**   promise that, by mapping fresh pages over them. only partial pages
**   at the edges, and ranges too small to be worth a system call, are
**   written.
*/
#define _CRIB_DROP_MIN  (64 * 1024)

static void
_crib_drop(void* ptr_v, size_t len_i, c3_t kep_t)
{
  size_t    pag_i = _crib_page();
  uintptr_t bot_p = ((uintptr_t)ptr_v + pag_i - 1) & ~(uintptr_t)(pag_i - 1);
  uintptr_t top_p = ((uintptr_t)ptr_v + len_i) & ~(uintptr_t)(pag_i - 1);

  if ( (len_i < _CRIB_DROP_MIN) || (top_p <= bot_p) ) {
    memset(ptr_v, 0, len_i);
    return;
  }

  memset(ptr_v, 0, bot_p - (uintptr_t)ptr_v);
  memset((void*)top_p, 0, ((uintptr_t)ptr_v + len_i) - top_p);

#if defined(U3_OS_windows)
  VirtualFree((void*)bot_p, top_p - bot_p, MEM_DECOMMIT);
  if ( kep_t ) {
    u3_assert( VirtualAlloc((void*)bot_p, top_p - bot_p, MEM_COMMIT, PAGE_READWRITE) );
  }
#elif defined(U3_OS_linux)
  (void)kep_t;
  u3_assert( 0 == madvise((void*)bot_p, top_p - bot_p, MADV_DONTNEED) );
#else
  (void)kep_t;
  {
    c3_i flg_i = MAP_PRIVATE | MAP_ANON | MAP_FIXED;
#  ifdef MAP_NORESERVE
    flg_i |= MAP_NORESERVE;
#  endif
    u3_assert( MAP_FAILED != mmap((void*)bot_p, top_p - bot_p,
                                  PROT_READ | PROT_WRITE, flg_i, -1, 0) );
  }
#endif
}

static inline void
_crib_fence(void)
{
  //  order stores against an interrupt that unwinds this thread
  //
  __atomic_signal_fence(__ATOMIC_SEQ_CST);
}

/* sets
*/
static inline c3_d
_crib_msk(u3c_set* set_u)
{
  return ((c3_d)1 << set_u->log_y) - 1;
}

static inline c3_d
_crib_idx(u3c_set* set_u, c3_d a_d, c3_d b_d)
{
  //  the low bits index the table, so that doubling it sends each entry
  //  either to its own slot or to the empty upper half (see _crib_grow())
  //
  c3_d h_d = (a_d * 0x9e3779b97f4a7c15ULL) ^ (b_d * 0xc2b2ae3d27d4eb4fULL);
  h_d ^= h_d >> 31;
  h_d *= 0xbf58476d1ce4e5b9ULL;
  h_d ^= h_d >> 29;
  return h_d & _crib_msk(set_u);
}


static inline void
_crib_log(u3c_set* set_u, c3_d k_d)
{
  if ( set_u->lin_w < _CRIB_LIS ) {
    set_u->lis_w[set_u->lin_w++] = (c3_w)k_d;
  }
  else {
    set_u->lin_w = _CRIB_LIS + 1;
  }
}

/* _crib_write(): store [a b] in slot [k_d], publishing [a] last.
*/
static inline void
_crib_write(u3c_set* set_u, c3_d k_d, c3_d a_d, c3_d b_d)
{
  c3_d* sot_d = &set_u->tab_d[2 * k_d];

  sot_d[0] = 0;
  _crib_fence();
  sot_d[1] = b_d;
  _crib_fence();
  sot_d[0] = a_d;
}

/* _crib_place(): put [a b] in its window, if there's room: 1 if placed,
**                2 if present, 0 if the window is full. an empty slot ends
**                the window's entries; a tombstone doesn't, but can be
**                reused.
*/
static c3_y
_crib_place(u3c_set* set_u, c3_d a_d, c3_d b_d, c3_t log_t)
{
  c3_d msk_d = _crib_msk(set_u);
  c3_d i_d   = _crib_idx(set_u, a_d, b_d);
  c3_d fre_d = ~(c3_d)0;
  c3_w j_w;

  for ( j_w = 0; j_w < _CRIB_WIN; j_w++ ) {
    c3_d  k_d   = (i_d + j_w) & msk_d;
    c3_d* sot_d = &set_u->tab_d[2 * k_d];

    if ( !sot_d[0] ) {
      if ( ~(c3_d)0 == fre_d ) fre_d = k_d;
      break;
    }
    if ( _CRIB_TOMB == sot_d[0] ) {
      if ( ~(c3_d)0 == fre_d ) fre_d = k_d;
      continue;
    }
    if ( (sot_d[0] == a_d) && (sot_d[1] == b_d) ) return 2;
  }

  if ( ~(c3_d)0 == fre_d ) return 0;
  _crib_write(set_u, fre_d, a_d, b_d);
  if ( log_t ) _crib_log(set_u, fre_d);
  return 1;
}

/* _crib_grow(): double the capacity in place and rehash. an interrupted
**               rehash loses entries, never corrupts one. 0 at the ceiling.
*/
static c3_t
_crib_grow(u3c_set* set_u)
{
  c3_d old_d = (c3_d)1 << set_u->log_y;
  c3_d i_d;

  //  NB: every slot past the capacity is zero (see u3c_set_shrink())
  //
  if ( set_u->log_y >= set_u->max_y ) return 0;
  if ( !_crib_commit(&set_u->tab_d[2 * old_d], old_d * 16) ) return 0;

  set_u->log_y++;
  set_u->lin_w = _CRIB_LIS + 1;
  set_u->sat_u.gro_d++;
  if ( ((c3_d)1 << set_u->log_y) > set_u->sat_u.top_d ) {
    set_u->sat_u.top_d = (c3_d)1 << set_u->log_y;
  }

  //  move each entry to its place under the new mask. a moved entry
  //  leaves a tombstone, so that entries placed past it stay reachable;
  //  the next clear removes them
  //
  for ( i_d = 0; i_d < old_d; i_d++ ) {
    c3_d a_d = set_u->tab_d[2 * i_d];
    c3_d b_d = set_u->tab_d[(2 * i_d) + 1];

    if ( !a_d || (_CRIB_TOMB == a_d) ) continue;
    set_u->tab_d[2 * i_d] = _CRIB_TOMB;
    _crib_fence();
    (void)_crib_place(set_u, a_d, b_d, 0);   //  dropped if its window is full
  }
  return 1;
}

void
u3c_set_init(u3c_set* set_u, const c3_c* nam_c, c3_y min_y, c3_y max_y)
{
  if ( set_u->tab_d ) {
    _crib_release(set_u->tab_d, ((size_t)1 << set_u->max_y) * 16);
  }
  min_y = c3_max(min_y, 4);
  max_y = c3_max(max_y, min_y);

  memset(set_u, 0, sizeof(*set_u));
  set_u->nam_c = nam_c;
  set_u->min_y = set_u->log_y = min_y;
  set_u->max_y = max_y;
  set_u->sat_u.top_d = (c3_d)1 << min_y;
  set_u->tab_d = _crib_reserve(((size_t)1 << max_y) * 16);
  u3_assert( _crib_commit(set_u->tab_d, ((size_t)1 << min_y) * 16) );
}

c3_t
u3c_set_has(u3c_set* set_u, c3_d a_d, c3_d b_d)
{
  c3_d msk_d = _crib_msk(set_u);
  c3_d i_d   = _crib_idx(set_u, a_d, b_d);
  c3_w j_w;

  for ( j_w = 0; j_w < _CRIB_WIN; j_w++ ) {
    c3_d* sot_d = &set_u->tab_d[2 * ((i_d + j_w) & msk_d)];

    if ( !sot_d[0] ) return 0;
    if ( (sot_d[0] == a_d) && (sot_d[1] == b_d) ) return 1;
  }
  return 0;
}

void
u3c_set_put(u3c_set* set_u, c3_d a_d, c3_d b_d)
{
  //  keep the load at half or less while below the ceiling
  //
  if ( ((set_u->use_d + 1) * 2) > ((c3_d)1 << set_u->log_y) ) {
    (void)_crib_grow(set_u);
  }

  while ( 1 ) {
    c3_y pla_y = _crib_place(set_u, a_d, b_d, 1);

    if ( 2 == pla_y ) return;
    if ( 1 == pla_y ) break;
    if ( !_crib_grow(set_u) ) {
      //  at the ceiling: evict an entry from the window, in turn
      //
      c3_d k_d = (_crib_idx(set_u, a_d, b_d) + (set_u->evk_w++ & (_CRIB_WIN - 1)))
               & _crib_msk(set_u);
      _crib_write(set_u, k_d, a_d, b_d);
      set_u->ovf_d++;
      set_u->sat_u.ovf_d++;
      break;
    }
  }
  set_u->use_d++;
}

void
u3c_set_clear(u3c_set* set_u)
{
  if ( !set_u->use_d ) return;

  if ( set_u->lin_w <= _CRIB_LIS ) {
    for ( c3_w i_w = 0; i_w < set_u->lin_w; i_w++ ) {
      set_u->tab_d[2 * (c3_d)set_u->lis_w[i_w]] = 0;
    }
    set_u->sat_u.clg_d++;
  }
  else {
    _crib_drop(set_u->tab_d, ((size_t)1 << set_u->log_y) * 16, 1);
    set_u->sat_u.cdp_d++;
  }
  if ( set_u->use_d > set_u->pek_d ) set_u->pek_d = set_u->use_d;
  if ( set_u->use_d > set_u->sat_u.pek_d ) set_u->sat_u.pek_d = set_u->use_d;
  set_u->use_d = 0;
  set_u->lin_w = 0;
}

void
u3c_set_shrink(u3c_set* set_u)
{
  c3_d cap_d;

  u3c_set_clear(set_u);
  cap_d = (c3_d)1 << set_u->log_y;

  //  shrink once recent use is an eighth of the capacity; the peak
  //  decays by half at each call, so a burst is given back after a few
  //
  if ( (set_u->log_y > set_u->min_y) && ((set_u->pek_d * 8) < cap_d) ) {
    c3_y new_y = set_u->min_y;

    while ( (new_y < set_u->log_y) && (((c3_d)1 << new_y) < (set_u->pek_d * 4)) ) {
      new_y++;
    }
    _crib_drop(&set_u->tab_d[2 * ((c3_d)1 << new_y)],
               (cap_d - ((c3_d)1 << new_y)) * 16, 0);
    set_u->log_y = new_y;
    set_u->pek_d = 0;
    set_u->sat_u.shr_d++;
  }
  else {
    set_u->pek_d /= 2;
  }
}

void
u3c_set_report(u3c_set* set_u)
{
  if ( set_u->ovf_d ) {
    u3l_log("crib: %s reached its ceiling of %" PRIu64 " entries;"
            " %" PRIu64 " evicted since the last report",
            set_u->nam_c, (c3_d)1 << set_u->max_y, set_u->ovf_d);
    set_u->ovf_d = 0;
  }
}

c3_d
u3c_set_cap(u3c_set* set_u)
{
  return (c3_d)1 << set_u->log_y;
}

u3c_stat
u3c_set_stat(u3c_set* set_u)
{
  u3c_stat sat_u = set_u->sat_u;

  sat_u.nam_c = set_u->nam_c;
  sat_u.cap_d = (c3_d)1 << set_u->log_y;
  sat_u.max_d = (c3_d)1 << set_u->max_y;
  if ( set_u->use_d > sat_u.pek_d ) sat_u.pek_d = set_u->use_d;

  memset(&set_u->sat_u, 0, sizeof(set_u->sat_u));
  set_u->sat_u.top_d = sat_u.cap_d;
  return sat_u;
}

/* arrays
*/
void
u3c_arr_init(u3c_arr* arr_u, const c3_c* nam_c,
             size_t siz_i, size_t min_i, size_t max_i)
{
  if ( arr_u->buf_y ) {
    _crib_release(arr_u->buf_y, arr_u->max_i * arr_u->siz_i);
  }
  memset(arr_u, 0, sizeof(*arr_u));
  arr_u->nam_c = nam_c;
  arr_u->siz_i = siz_i;
  arr_u->min_i = arr_u->cap_i = min_i;
  arr_u->max_i = c3_max(max_i, min_i);
  arr_u->sat_u.top_d = min_i;
  arr_u->buf_y = _crib_reserve(arr_u->max_i * siz_i);
  u3_assert( _crib_commit(arr_u->buf_y, min_i * siz_i) );
}

void*
u3c_arr_at(u3c_arr* arr_u, size_t i_i)
{
  if ( i_i >= arr_u->cap_i ) {
    size_t new_i;

    if ( i_i >= arr_u->max_i ) {
      arr_u->drp_d++;
      arr_u->sat_u.ovf_d++;
      return 0;
    }
    new_i = c3_min(c3_max(arr_u->cap_i * 2, i_i + 1), arr_u->max_i);
    if ( !_crib_commit(arr_u->buf_y + (arr_u->cap_i * arr_u->siz_i),
                       (new_i - arr_u->cap_i) * arr_u->siz_i) )
    {
      return 0;
    }
    arr_u->cap_i = new_i;
    arr_u->sat_u.gro_d++;
    if ( new_i > arr_u->sat_u.top_d ) arr_u->sat_u.top_d = new_i;
  }
  return arr_u->buf_y + (i_i * arr_u->siz_i);
}

void
u3c_arr_shrink(u3c_arr* arr_u, size_t len_i)
{
  if ( len_i > arr_u->pek_i ) arr_u->pek_i = len_i;
  if ( len_i > arr_u->sat_u.pek_d ) arr_u->sat_u.pek_d = len_i;

  if ( (arr_u->cap_i > arr_u->min_i) && ((arr_u->pek_i * 8) < arr_u->cap_i) ) {
    size_t new_i = c3_max(arr_u->min_i, arr_u->pek_i * 2);

    _crib_drop(arr_u->buf_y + (new_i * arr_u->siz_i),
               (arr_u->cap_i - new_i) * arr_u->siz_i, 0);
    arr_u->cap_i = new_i;
    arr_u->pek_i = 0;
    arr_u->sat_u.shr_d++;
  }
  else {
    arr_u->pek_i /= 2;
  }
}

void
u3c_arr_used(u3c_arr* arr_u, size_t len_i)
{
  if ( len_i > arr_u->sat_u.pek_d ) arr_u->sat_u.pek_d = len_i;
}

void
u3c_arr_report(u3c_arr* arr_u)
{
  if ( arr_u->drp_d ) {
    u3l_log("crib: %s reached its ceiling of %zu entries;"
            " %" PRIu64 " dropped since the last report",
            arr_u->nam_c, arr_u->max_i, arr_u->drp_d);
    arr_u->drp_d = 0;
  }
}

u3c_stat
u3c_arr_stat(u3c_arr* arr_u)
{
  u3c_stat sat_u = arr_u->sat_u;

  sat_u.nam_c = arr_u->nam_c;
  sat_u.cap_d = arr_u->cap_i;
  sat_u.max_d = arr_u->max_i;

  memset(&arr_u->sat_u, 0, sizeof(arr_u->sat_u));
  arr_u->sat_u.top_d = sat_u.cap_d;
  return sat_u;
}
