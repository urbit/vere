/// @file

#include "crib.h"
#include "error.h"

#ifdef U3_OS_windows
#  include <windows.h>
#endif

/* on POSIX the whole reservation is readable and writable from the
** start (pages are materialized on first touch), so committing is a
** no-op; on Windows it's VirtualAlloc(MEM_COMMIT).
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
**   madvise(MADV_DONTNEED), on Windows by decommitting (and committing
**   again if [kep_t]), and elsewhere (macOS, BSD), where MADV_DONTNEED
**   doesn't promise that, by mapping fresh pages over them. only partial
**   pages at the edges, and ranges too small to be worth a system call,
**   are written.
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

void
u3c_init(u3c_crib* cib_u, size_t len_i, size_t max_i)
{
  if ( cib_u->buf_y ) {
    _crib_release(cib_u->buf_y, cib_u->max_i);
  }
  cib_u->max_i = c3_max(max_i, len_i);
  cib_u->len_i = len_i;
  cib_u->buf_y = _crib_reserve(cib_u->max_i);
  u3_assert( _crib_commit(cib_u->buf_y, len_i) );
}

c3_t
u3c_grow(u3c_crib* cib_u, size_t len_i)
{
  if ( len_i <= cib_u->len_i ) return 1;
  if ( len_i > cib_u->max_i ) return 0;
  if ( !_crib_commit(cib_u->buf_y + cib_u->len_i, len_i - cib_u->len_i) ) {
    return 0;
  }
  cib_u->len_i = len_i;
  return 1;
}

void
u3c_zero(u3c_crib* cib_u, size_t off_i, size_t len_i)
{
  u3_assert( (off_i + len_i) <= cib_u->len_i );
  _crib_drop(cib_u->buf_y + off_i, len_i, 1);
}

void
u3c_shrink(u3c_crib* cib_u, size_t len_i)
{
  if ( len_i < cib_u->len_i ) {
    _crib_drop(cib_u->buf_y + len_i, cib_u->len_i - len_i, 0);
    cib_u->len_i = len_i;
  }
}
