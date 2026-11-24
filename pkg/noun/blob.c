/// @file

#include "blob.h"
#include "imprison.h"
#include "manage.h"
#include "retrieve.h"
#include "vortex.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef U3_OS_windows
#  include <io.h>
#  include <windows.h>
#endif

//  mode for a finished blob file.
//
//    blobs are immutable once written, so posix creates them read-only.
//    windows turns a mode without owner-write into
//    FILE_ATTRIBUTE_READONLY and then refuses to delete the file, which
//    would break u3b_wipe and with it blob gc and |chop.  give
//    windows an owner-writable mode instead; nothing ever opens a blob
//    for writing on any platform.
//
#ifdef U3_OS_windows
#  define BLOB_FILE_MODE 0600
#else
#  define BLOB_FILE_MODE 0400
#endif
//  applied by u3b_move_stg before the rename (posix) and by its copy
//  fallback; on windows the staging file's 0600 already is the mode.

//  maximum bytes per single read()/write() call.
//  POSIX allows read()/write() to return EINVAL if count > SSIZE_MAX;
//  macOS returns EINVAL if count > INT_MAX.  cap conservatively at 1 GiB.
//
#define BLOB_IO_MAX  ((size_t)0x40000000UL)

/* blob reads: one mapping per view.
**
**   every read of a blob goes through a read-only mapping that one view
**   owns.  a mapping holds the file's bytes, then zeros: to at least one
**   page past the file, or to the width the view asked for.  the fd that
**   makes it lives only inside the critical section that opens, maps,
**   and closes it.  no fd outlives a call, and nothing caches one.
**
**   an inner road keeps a record of each of its mappings on its own
**   heap, listed from its fla_p slot.  a bail or signal that skips a
**   view's done leaves nothing behind: u3m_fall drains the road's list,
**   the signal reset drains every kid road's, and the records go with
**   the road's memory.  the home road lists nothing: a home bail aborts
**   and a signal never unwinds a home frame, so its views always reach
**   done, and the slot the snapshot saves stays zero.
**
**   every change to a mapping or a list runs inside u3m_crit_inn() /
**   u3m_crit_out(), and nothing in such a section allocates or bails.
**   one thread per process reads.
*/

//  read-only, and never inherited by a process this one starts
//
#ifdef U3_OS_windows
#  define BLOB_O_READ  (O_RDONLY | _O_NOINHERIT)
#else
#  define BLOB_O_READ  (O_RDONLY | O_CLOEXEC)
#endif

//  the widest view: far past any address space, and far from wrapping
//  a c3_d when rounded
//
#define BLOB_WID_MAX  (1ULL << 62)

#ifdef U3_OS_windows
//  placeholder flags, in case we are built against older headers
//
#  ifndef MEM_RESERVE_PLACEHOLDER
#    define MEM_RESERVE_PLACEHOLDER    0x00040000
#  endif
#  ifndef MEM_REPLACE_PLACEHOLDER
#    define MEM_REPLACE_PLACEHOLDER    0x00004000
#  endif
#  ifndef MEM_PRESERVE_PLACEHOLDER
#    define MEM_PRESERVE_PLACEHOLDER   0x00000002
#  endif
#endif

static c3_z _blob_maps_z;   //  mappings live in this process
static c3_d _blob_mapd_d;   //  bytes they span

/* _blob_page(): the os page size.
*/
static c3_d
_blob_page(void)
{
  static c3_d pag_d;

  if ( !pag_d ) {
#ifdef U3_OS_windows
    SYSTEM_INFO inf_u;
    GetSystemInfo(&inf_u);
    pag_d = (c3_d)inf_u.dwPageSize;
#else
    pag_d = (c3_d)sysconf(_SC_PAGESIZE);
#endif
  }
  return pag_d;
}

/* _blob_gran(): the unit a mapping is laid out in: the page size on
**   posix; on windows the allocation granularity, at which a
**   placeholder splits.
*/
static c3_d
_blob_gran(void)
{
#ifdef U3_OS_windows
  static c3_d gan_d;

  if ( !gan_d ) {
    SYSTEM_INFO inf_u;
    GetSystemInfo(&inf_u);
    gan_d = (c3_d)inf_u.dwAllocationGranularity;
  }
  return gan_d;
#else
  return _blob_page();
#endif
}

/* _blob_round(): [len_d] rounded up to a multiple of [gan_d], a power
**   of two.
*/
static inline c3_d
_blob_round(c3_d len_d, c3_d gan_d)
{
  return (len_d + gan_d - 1) & ~(gan_d - 1);
}

/* _blob_ext(): the extent that holds [len_d] bytes read as [wid_d]: at
**   least one page past the bytes, and at least the width.
*/
static c3_d
_blob_ext(c3_d len_d, c3_d wid_d)
{
  c3_d min_d = len_d + _blob_page();
  return _blob_round(c3_max(min_d, wid_d), _blob_gran());
}

/* _blob_home(): the current road is the home road.
*/
static inline c3_t
_blob_home(void)
{
  return &(u3H->rod_u) == u3R;
}

/* _blob_list(): an inner road lists [fla_u], a fresh mapping; the home
**   road lists nothing.  counts it either way.  the caller holds the
**   critical section.
*/
static void
_blob_list(u3b_flat* fla_u)
{
  if ( !_blob_home() ) {
    fla_u->nex_p = u3R->fla_p;
    u3R->fla_p   = u3of(u3b_flat, fla_u);
  }
  _blob_maps_z++;
  _blob_mapd_d += fla_u->ext_d;
}

/* _blob_unmap_raw(): unmap [fla_u]'s extent.
*/
static void
_blob_unmap_raw(u3b_flat* fla_u)
{
#ifdef U3_OS_windows
  if ( fla_u->hed_d ) {
    UnmapViewOfFile(fla_u->map_y);
  }
  VirtualFree(fla_u->map_y + fla_u->hed_d, 0, MEM_RELEASE);
#else
  munmap(fla_u->map_y, (size_t)fla_u->ext_d);
#endif
}

/* _blob_release(): unmap [fla_u] and uncount it.  the caller holds the
**   critical section and has taken it off any list.
*/
static void
_blob_release(u3b_flat* fla_u)
{
  _blob_unmap_raw(fla_u);

  _blob_maps_z--;
  _blob_mapd_d -= fla_u->ext_d;
  fla_u->map_y  = 0;
  fla_u->ext_d  = 0;
  fla_u->hed_d  = 0;
}

#ifdef U3_OS_windows
/* _blob_map_fd(): map [len_d] bytes open on [fid_i] read-only into
**   [fla_u], zero to [wid_d] and at least one page past them.  0, or
**   the failure: c3__meme for address space, c3__file for the file.
**
**   a placeholder reserves the extent.  the file's whole granules are a
**   view of it in place; the rest of the extent is private memory
**   holding the file's ragged last bytes, then zeros, protected
**   read-only.  windows splits a placeholder only at the allocation
**   granularity, so the ragged bytes cannot be a view of the file.
*/
static c3_m
_blob_map_fd(c3_i fid_i, c3_d len_d, c3_d wid_d, u3b_flat* fla_u)
{
  HANDLE fil_h = (HANDLE)_get_osfhandle(fid_i);
  c3_d   ext_d = _blob_ext(len_d, wid_d);
  c3_d   hed_d = len_d & ~(_blob_gran() - 1);
  HANDLE sec_h = NULL;
  c3_y*  bas_y;
  DWORD  old_u;

  if ( INVALID_HANDLE_VALUE == fil_h ) {
    return c3__file;
  }

  sec_h = CreateFileMappingW(fil_h, NULL, PAGE_READONLY,
                             (DWORD)(len_d >> 32),
                             (DWORD)(len_d & 0xffffffffULL),
                             NULL);
  if ( !sec_h ) {
    return c3__file;
  }

  bas_y = VirtualAlloc2(NULL, NULL, (size_t)ext_d,
                        MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                        NULL, 0);
  if ( !bas_y ) {
    CloseHandle(sec_h);
    return c3__meme;
  }

  //  the whole granules: a view of the file in place
  //
  if ( hed_d ) {
    if ( !VirtualFree(bas_y, (size_t)hed_d,
                      MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) )
    {
      VirtualFree(bas_y, 0, MEM_RELEASE);
      CloseHandle(sec_h);
      return c3__file;
    }

    if ( !MapViewOfFile3(sec_h, NULL, bas_y, 0, (size_t)hed_d,
                         MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, NULL, 0) )
    {
      VirtualFree(bas_y, 0, MEM_RELEASE);
      VirtualFree(bas_y + hed_d, 0, MEM_RELEASE);
      CloseHandle(sec_h);
      return c3__file;
    }
  }

  //  the rest: private memory, the ragged bytes and then zeros
  //
  if ( !VirtualAlloc2(NULL, bas_y + hed_d, (size_t)(ext_d - hed_d),
                      MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER,
                      PAGE_READWRITE, NULL, 0) )
  {
    if ( hed_d ) {
      UnmapViewOfFile(bas_y);
    }
    VirtualFree(bas_y + hed_d, 0, MEM_RELEASE);
    CloseHandle(sec_h);
    return c3__meme;
  }

  fla_u->map_y = bas_y;
  fla_u->ext_d = ext_d;
  fla_u->hed_d = hed_d;

  if ( len_d > hed_d ) {
    c3_y* rag_y = MapViewOfFile(sec_h, FILE_MAP_READ,
                                (DWORD)(hed_d >> 32),
                                (DWORD)(hed_d & 0xffffffffULL),
                                (size_t)(len_d - hed_d));
    if ( !rag_y ) {
      _blob_unmap_raw(fla_u);
      CloseHandle(sec_h);
      return c3__file;
    }
    memcpy(bas_y + hed_d, rag_y, (size_t)(len_d - hed_d));
    UnmapViewOfFile(rag_y);
  }

  CloseHandle(sec_h);

  if ( !VirtualProtect(bas_y + hed_d, (size_t)(ext_d - hed_d),
                       PAGE_READONLY, &old_u) )
  {
    _blob_unmap_raw(fla_u);
    return c3__file;
  }

  return 0;
}
#else
/* _blob_map_fd(): map [len_d] bytes open on [fid_i] read-only into
**   [fla_u], zero to [wid_d] and at least one page past them.  0, or
**   the failure: c3__meme for address space, c3__file for the file.
**
**   an anonymous read-only reservation spans the extent, and the file's
**   pages are fixed over its start.  the kernel zero-fills the rest of
**   the file's last page; past it, the reservation reads as zero.  one
**   munmap releases both.
*/
static c3_m
_blob_map_fd(c3_i fid_i, c3_d len_d, c3_d wid_d, u3b_flat* fla_u)
{
  c3_d  ext_d = _blob_ext(len_d, wid_d);
  void* bas_v = mmap(0, (size_t)ext_d, PROT_READ,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if ( MAP_FAILED == bas_v ) {
    return c3__meme;
  }

  if ( MAP_FAILED == mmap(bas_v, (size_t)len_d, PROT_READ,
                          MAP_PRIVATE | MAP_FIXED, fid_i, 0) )
  {
    munmap(bas_v, (size_t)ext_d);
    return c3__file;
  }

  fla_u->map_y = bas_v;
  fla_u->ext_d = ext_d;
  fla_u->hed_d = 0;
  return 0;
}
#endif

/* u3b_map(): map blob [bid_d] into [fla_u]: its bytes, then zeros to
**   [wid_d] and at least one page past them.
*/
c3_o
u3b_map(u3b_flat*   fla_u,
        const c3_c* bob_c,
        c3_d        bid_d,
        c3_d        wid_d,
        c3_d*       bit_d,
        c3_m*       mot_m)
{
  c3_c        fil_c[8192];
  struct stat st_u;
  c3_i        fid_i;
  c3_d        len_d;
  c3_m        why_m = 0;
  c3_y        las_y;

  fla_u->nex_p = 0;
  fla_u->map_y = 0;
  fla_u->ext_d = 0;
  fla_u->hed_d = 0;

  //  a process that never learned its epoch has no store
  //
  if ( !bob_c ) {
    *mot_m = c3__file;
    return c3n;
  }

  if ( wid_d > BLOB_WID_MAX ) {
    *mot_m = c3__meme;
    return c3n;
  }

  u3b_path(fil_c, bob_c, u3a_bid_mug(bid_d), u3a_bid_seq(bid_d));

  //  the fd lives only inside this section: a signal can neither leak
  //  it nor leave a mapping off its road's list
  //
  u3m_crit_inn();

  if ( -1 == (fid_i = open(fil_c, BLOB_O_READ)) ) {
    u3m_crit_out();
    fprintf(stderr, "blob: open %s: %s\r\n", fil_c, strerror(errno));
    *mot_m = c3__file;
    return c3n;
  }

  if ( -1 == fstat(fid_i, &st_u) ) {
    why_m = c3__file;
    fprintf(stderr, "blob: fstat %s: %s\r\n", fil_c, strerror(errno));
  }
  else if ( !(len_d = (c3_d)st_u.st_size) ) {
    why_m = c3__file;
  }
  else if ( (why_m = _blob_map_fd(fid_i, len_d, wid_d, fla_u)) ) {
    fprintf(stderr, "blob: map %s: %s\r\n", fil_c, strerror(errno));
  }
  else {
    _blob_list(fla_u);
  }

  close(fid_i);
  u3m_crit_out();

  if ( why_m ) {
    *mot_m = why_m;
    return c3n;
  }

  //  a canonical blob's last byte is nonzero, and gives its bit length.
  //  a file truncated in place under the mapping faults here; only an
  //  outside actor can truncate a read-only blob
  //
  if ( !(las_y = fla_u->map_y[len_d - 1]) ) {
    fprintf(stderr, "blob: " U3A_BID_FMT ": not canonical\r\n",
            u3a_bid_mug(bid_d), u3a_bid_seq(bid_d));
    u3b_unmap(fla_u);
    *mot_m = c3__file;
    return c3n;
  }

  *bit_d = ((len_d - 1) * 8) + (32 - __builtin_clz((unsigned int)las_y));
  return c3y;
}

/* u3b_pad(): map [len_d] bytes at [src_y] into [fla_u], zero to
**   [wid_d]: a read-only anonymous copy.
*/
c3_o
u3b_pad(u3b_flat* fla_u, const c3_y* src_y, c3_d len_d, c3_d wid_d)
{
  c3_d ext_d;

  fla_u->nex_p = 0;
  fla_u->map_y = 0;
  fla_u->ext_d = 0;
  fla_u->hed_d = 0;

  if ( (wid_d > BLOB_WID_MAX) || (len_d > BLOB_WID_MAX) ) {
    return c3n;
  }
  ext_d = _blob_round(c3_max(len_d, wid_d), _blob_gran());

  u3m_crit_inn();
  {
#ifdef U3_OS_windows
    void* bas_v = VirtualAlloc(NULL, (size_t)ext_d, MEM_RESERVE | MEM_COMMIT,
                               PAGE_READWRITE);
    if ( !bas_v ) {
      u3m_crit_out();
      return c3n;
    }
#else
    void* bas_v = mmap(0, (size_t)ext_d, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ( MAP_FAILED == bas_v ) {
      u3m_crit_out();
      return c3n;
    }
#endif
    fla_u->map_y = bas_v;
    fla_u->ext_d = ext_d;
    _blob_list(fla_u);
  }
  u3m_crit_out();

  //  filled outside the section: a signal meanwhile finds the mapping
  //  listed, and its road's drain takes it
  //
  memcpy(fla_u->map_y, src_y, (size_t)len_d);

#ifdef U3_OS_windows
  {
    DWORD old_u;
    VirtualProtect(fla_u->map_y, (size_t)ext_d, PAGE_READONLY, &old_u);
  }
#else
  mprotect(fla_u->map_y, (size_t)ext_d, PROT_READ);
#endif

  return c3y;
}

/* u3b_unmap(): release [fla_u], on the road that made it.
*/
void
u3b_unmap(u3b_flat* fla_u)
{
  if ( !fla_u->map_y ) {
    return;
  }

  u3m_crit_inn();

  //  an inner road unlists it.  a road holds few live mappings at once,
  //  since each view's done unmaps its own, so the walk is short
  //
  if ( !_blob_home() ) {
    u3_post  fla_p = u3of(u3b_flat, fla_u);
    u3_post* lin_p = &u3R->fla_p;

    while ( *lin_p != fla_p ) {
      u3_assert( *lin_p );
      lin_p = &(u3to(u3b_flat, *lin_p)->nex_p);
    }
    *lin_p = fla_u->nex_p;
  }

  _blob_release(fla_u);

  u3m_crit_out();
}

/* _blob_drain(): unmap every mapping on [rod_u]'s list and empty it.
**   the records stay where they are: they go with the road's memory.
*/
static void
_blob_drain(u3a_road* rod_u)
{
  //  most roads map nothing: those skip the critical section
  //
  if ( !rod_u->fla_p ) {
    return;
  }

  u3m_crit_inn();

  while ( rod_u->fla_p ) {
    u3b_flat* fla_u = u3to(u3b_flat, rod_u->fla_p);
    rod_u->fla_p = fla_u->nex_p;
    _blob_release(fla_u);
  }

  u3m_crit_out();
}

/* u3b_drain(): release every mapping road [rod_v] made.
*/
void
u3b_drain(void* rod_v)
{
  _blob_drain(rod_v);
}

/* u3b_drain_kids(): release every mapping on every road above home.
*/
void
u3b_drain_kids(void)
{
  u3_post kid_p = u3H->rod_u.kid_p;

  while ( kid_p ) {
    u3a_road* rod_u = u3to(u3a_road, kid_p);
    _blob_drain(rod_u);
    kid_p = rod_u->kid_p;
  }
}

/* u3b_mark(): mark the current road's mapping records; words marked.
**
**   an inner road keeps them on its own heap, reachable only from its
**   fla_p slot, so a sweep would free them as leaks.
*/
c3_w
u3b_mark(void)
{
  c3_w    siz_w = 0;
  u3_post fla_p = u3R->fla_p;

  while ( fla_p ) {
    u3b_flat* fla_u = u3to(u3b_flat, fla_p);
    siz_w += u3a_mark_ptr(fla_u);
    fla_p  = fla_u->nex_p;
  }
  return siz_w;
}

/* u3b_maps(): mappings the process holds.
*/
c3_z
u3b_maps(void)
{
  return _blob_maps_z;
}

/* u3b_maps_road(): mappings road [rod_v] lists.
*/
c3_z
u3b_maps_road(void* rod_v)
{
  u3a_road* rod_u = rod_v;
  c3_z      num_z = 0;
  u3_post   fla_p = rod_u->fla_p;

  while ( fla_p ) {
    num_z++;
    fla_p = u3to(u3b_flat, fla_p)->nex_p;
  }
  return num_z;
}

/* u3b_mapped(): bytes the process holds mapped.
*/
c3_d
u3b_mapped(void)
{
  return _blob_mapd_d;
}

/* u3b_root(): write the store root of epoch [epo_d] into [out_c].
**
**   $pier/.urb/log/0i<epo>/bob: an epoch owns its blob files, and every
**   other path here is relative to a root.
*/
void
u3b_root(c3_c* out_c, const c3_c* pax_c, c3_d epo_d)
{
  snprintf(out_c, 8192, "%s/.urb/log/0i%" PRIc3_d "/bob", pax_c, epo_d);
}

/* _blob_mug_dir(): write path to <root>/<mug>/ into [out_c].
*/
static void
_blob_mug_dir(c3_c* out_c, const c3_c* bob_c, c3_h mug_h)
{
  snprintf(out_c, 8192, "%s/" U3B_MUG_FMT, bob_c, mug_h);
}

/* _blob_sync_path(): fsync directory [dir_c] so a freshly-created
**   entry survives a crash before the next global sync.  No-op on
**   Windows, which cannot open a directory as a file descriptor.
*/
static void
_blob_sync_path(const c3_c* dir_c)
{
#ifndef U3_OS_windows
  c3_i dir_i = open(dir_c, O_RDONLY);
  if ( -1 == dir_i ) {
    return;
  }
  c3_sync(dir_i);
  close(dir_i);
#endif
}

/* _blob_sync_dir(): fsync the mug bucket of (bob_c, mug_h).
*/
static void
_blob_sync_dir(const c3_c* bob_c, c3_h mug_h)
{
  c3_c dir_c[8192];
  _blob_mug_dir(dir_c, bob_c, mug_h);
  _blob_sync_path(dir_c);
}

/* _blob_bucket_make(): make the store root and the mug bucket if
**   missing.  a new bucket is an entry in the root, synced like a new
**   blob is in its bucket.
*/
static c3_o
_blob_bucket_make(const c3_c* bob_c, c3_h mug_h)
{
  c3_c dir_c[8192];
  _blob_mug_dir(dir_c, bob_c, mug_h);

  if ( 0 != c3_mkdir(bob_c, 0700) && EEXIST != errno ) {
    fprintf(stderr, "blob: failed to create store %s: %s\r\n",
            bob_c, strerror(errno));
    return c3n;
  }
  if ( 0 == c3_mkdir(dir_c, 0700) ) {
    _blob_sync_path(bob_c);
  }
  else if ( EEXIST != errno ) {
    fprintf(stderr, "blob: failed to create bucket %s: %s\r\n",
            dir_c, strerror(errno));
    return c3n;
  }
  return c3y;
}

/* u3b_path(): write filesystem path for a blob into [out_c].
*/
void
u3b_path(c3_c* out_c, const c3_c* bob_c, c3_h mug_h, c3_h seq_h)
{
  snprintf(out_c, 8192, "%s/" U3B_MUG_FMT "/%" PRIc3_h, bob_c, mug_h, seq_h);
}

/* u3b_stg_dir(): write path to $pier/.urb/stg/ into [out_c].
*/
void
u3b_stg_dir(c3_c* out_c, const c3_c* pax_c)
{
  snprintf(out_c, 8192, "%s/.urb/stg", pax_c);
}

static c3_o
_blob_name_num(const c3_c* nam_c, c3_h* out_h);

/* _blob_bucket_next(): one past the highest seq in bucket [dir_c].
**
**   the bucket is the record of which seqs are taken, so reading it
**   cannot disagree with it.  mars is the one writer: nothing else
**   allocates between this read and the install's rename.  Returns 0
**   if the bucket cannot be read.
*/
static c3_h
_blob_bucket_next(const c3_c* dir_c)
{
  DIR* dir_u = opendir(dir_c);
  c3_h max_h = 0;

  if ( !dir_u ) {
    fprintf(stderr, "blob: failed to read bucket %s: %s\r\n",
            dir_c, strerror(errno));
    return 0;
  }

  struct dirent* ent_e;
  while ( (ent_e = readdir(dir_u)) ) {
    c3_h seq_h;
    if ( (c3y == _blob_name_num(ent_e->d_name, &seq_h)) && (seq_h > max_h) ) {
      max_h = seq_h;
    }
  }
  closedir(dir_u);
  return max_h + 1;
}

/* _blob_dedup(): scan bucket for byte-equal content.
**
** Sets [dup_h] to the sequence number of an existing equal blob, or 0
** if none.  Returns c3n if a blob in the bucket could not be read: an
** install must not mint a second seq for content it could not compare,
** since equality by seq (u3r_sing, meld) relies on one seq per content.
*/
static c3_o
_blob_dedup(const c3_c* bob_c, c3_h mug_h, c3_h max_h,
            const c3_y* dat_y, c3_d len_d, c3_h* dup_h)
{
  *dup_h = 0;

  for ( c3_h seq_h = 1; seq_h < max_h; seq_h++ ) {
    c3_c fil_c[8192];
    u3b_path(fil_c, bob_c, mug_h, seq_h);

    //  a gap in the sequence is normal: the gc and refcount deletion
    //  remove files
    //
    struct stat st_u;
    if ( -1 == stat(fil_c, &st_u) ) {
      if ( ENOENT == errno ) {
        continue;
      }
      fprintf(stderr, "blob: dedup: stat failed on %s: %s\r\n",
              fil_c, strerror(errno));
      return c3n;
    }
    if ( (c3_d)st_u.st_size != len_d ) {
      continue;
    }

    c3_i fid_i = open(fil_c, O_RDONLY);
    if ( -1 == fid_i ) {
      fprintf(stderr, "blob: dedup: open failed on %s: %s\r\n",
              fil_c, strerror(errno));
      return c3n;
    }

    c3_o eql_o = c3y;
    c3_d rem_d = len_d;
    const c3_y* ptr_y = dat_y;
    c3_y buf_y[4096];

    while ( rem_d > 0 ) {
      c3_d ask_d = ( rem_d < sizeof(buf_y) ) ? rem_d : sizeof(buf_y);
      ssize_t got_i = read(fid_i, buf_y, ask_d);

      if ( got_i < 0 && EINTR == errno ) {
        continue;
      }
      //  a short read is a file that changed under us: the sole-writer
      //  rule says that cannot happen, so treat it as the error it is
      //
      if ( got_i <= 0 || (c3_d)got_i != ask_d ) {
        fprintf(stderr, "blob: dedup: read failed on %s: %s\r\n",
                fil_c, (got_i < 0) ? strerror(errno) : "short read");
        close(fid_i);
        return c3n;
      }
      if ( 0 != memcmp(ptr_y, buf_y, ask_d) ) {
        eql_o = c3n;
        break;
      }
      ptr_y += ask_d;
      rem_d -= ask_d;
    }

    close(fid_i);
    if ( c3y == eql_o ) {
      *dup_h = seq_h;
      return c3y;
    }
  }
  return c3y;
}

/* _blob_sig(): significant byte length of blob content, a la u3r_met(3, ...).
**
**   An atom has no trailing zero bytes, but a blob file may: it is written
**   verbatim from a request body, a packet, or a file on disk.  Scanning
**   them off here is what makes _blob_mug agree with u3r_mug_words.
*/
static c3_d
_blob_sig(const c3_y* dat_y, c3_d len_d)
{
  while ( len_d && !dat_y[len_d - 1] ) {
    len_d--;
  }
  return len_d;
}

/* _blob_meas(): measure content as an install does: [sig_d] its
**   significant length, [mug_h] the mug of those bytes.  c3n if it has
**   no bob representation (fewer than U3_BLOB_MIN significant bytes,
**   all-zero content included).
**
**   the one measure: staging reports this mug for a stub to carry, and
**   install recomputes it to file the blob, so the two must agree.  the
**   mug is the one the same atom gets in the loom, since u3i_blob puts
**   it in the bob's header.
*/
static c3_o
_blob_meas(const c3_y* dat_y, c3_d len_d, c3_d* sig_d, c3_h* mug_h)
{
  *sig_d = _blob_sig(dat_y, len_d);

  if ( *sig_d < U3_BLOB_MIN ) {
    return c3n;
  }
  *mug_h = u3r_mug_bytes(dat_y, *sig_d);
  return c3y;
}

/* _blob_stage_open(): create a staging file, its path in [stg_c].
*/
static c3_i
_blob_stage_open(const c3_c* pax_c, c3_c* stg_c)
{
  c3_i fid_i;

  u3b_stg_dir(stg_c, pax_c);
  strcat(stg_c, "/blob-XXXXXX");

  if ( -1 == (fid_i = mkstemp(stg_c)) ) {
    fprintf(stderr, "blob: stage: mkstemp %s: %s\r\n", stg_c, strerror(errno));
  }
#ifndef U3_OS_windows
  //  a process this one starts inherits no staging fd
  //
  else {
    fcntl(fid_i, F_SETFD, FD_CLOEXEC);
  }
#endif
  return fid_i;
}

/* _blob_write(): write all [len_z] bytes to [fid_i], in BLOB_IO_MAX
**   chunks, retrying EINTR and short writes.
*/
static c3_o
_blob_write(c3_i fid_i, const c3_y* dat_y, c3_z len_z)
{
  while ( len_z ) {
    size_t  ask_i = ( len_z < BLOB_IO_MAX ) ? len_z : BLOB_IO_MAX;
    ssize_t wrt_i = write(fid_i, dat_y, ask_i);

    if ( wrt_i <= 0 ) {
      if ( (wrt_i < 0) && (EINTR == errno) ) {
        continue;
      }
      fprintf(stderr, "blob: write: %s\r\n", strerror(errno));
      return c3n;
    }
    dat_y += wrt_i;
    len_z -= (c3_z)wrt_i;
  }
  return c3y;
}

/* _blob_stage_done(): sync and close a staging fd; unlink it on failure.
*/
static c3_o
_blob_stage_done(c3_i fid_i, const c3_c* stg_c, c3_o ok_o)
{
  //  no sync: a staged file is purged at the next start whatever
  //  happens to it, and the install syncs what it keeps
  //
  close(fid_i);

  if ( c3n == ok_o ) {
    c3_unlink(stg_c);
  }
  return ok_o;
}

/* u3b_mug_fd(): the content mug of the file open on [fid_i], measured
**   as an install measures it (_blob_meas), without copying it; c3n if
**   it has no bob representation or cannot be read.
*/
c3_o
u3b_mug_fd(c3_i fid_i, c3_h* mug_h)
{
  struct stat st_u;

  if ( (-1 == fstat(fid_i, &st_u)) || !st_u.st_size ) {
    return c3n;
  }

  c3_d  map_d = (c3_d)st_u.st_size;
  void* map_v = mmap(0, (size_t)map_d, PROT_READ, MAP_PRIVATE, fid_i, 0);

  if ( MAP_FAILED == map_v ) {
    return c3n;
  }

  c3_d sig_d;
  c3_o ok_o = _blob_meas((const c3_y*)map_v, map_d, &sig_d, mug_h);

  munmap(map_v, (size_t)map_d);
  return ok_o;
}

/* _blob_measure(): the content mug of staged file [stg_c], or c3n.
*/
static c3_o
_blob_measure(const c3_c* stg_c, c3_h* mug_h)
{
  c3_i fid_i = open(stg_c, BLOB_O_READ);

  if ( -1 == fid_i ) {
    return c3n;
  }
  c3_o ok_o = u3b_mug_fd(fid_i, mug_h);
  close(fid_i);
  return ok_o;
}

/* u3b_stage(): write [len_d] bytes to a new staging file.
**
**   Refuses content with no bob representation (under U3_BLOB_MIN
**   significant bytes) before writing anything, so the caller goes
**   inline at once; otherwise sets [mug_h] to the content mug.
*/
c3_o
u3b_stage(const c3_c* pax_c,
              const c3_y* dat_y,
              c3_d        len_d,
              c3_c*       stg_c,
              c3_h*       mug_h)
{
  c3_d sig_d;

  if ( c3n == _blob_meas(dat_y, len_d, &sig_d, mug_h) ) {
    fprintf(stderr, "blob: stage: %" PRIc3_d " significant byte(s) denote "
                    "a direct atom\r\n", sig_d);
    return c3n;
  }

  c3_i fid_i = _blob_stage_open(pax_c, stg_c);

  if ( -1 == fid_i ) {
    return c3n;
  }
  return _blob_stage_done(fid_i, stg_c,
                          _blob_write(fid_i, dat_y, (c3_z)len_d));
}

/* u3b_stage_fd(): copy [len_d] bytes from [fid_i] to a new staging file.
**
**   As u3b_stage: the staged file is measured once written, and one
**   with no bob representation is removed again.
*/
c3_o
u3b_stage_fd(const c3_c* pax_c,
                 c3_i        fid_i,
                 c3_d        len_d,
                 c3_c*       stg_c,
                 c3_h*       mug_h)
{
  c3_i stg_i = _blob_stage_open(pax_c, stg_c);
  c3_o ok_o  = c3y;

  if ( -1 == stg_i ) {
    return c3n;
  }

  {
    c3_y buf_y[65536];

    while ( len_d ) {
      size_t  ask_i = ( len_d < sizeof(buf_y) ) ? (size_t)len_d : sizeof(buf_y);
      ssize_t got_i = read(fid_i, buf_y, ask_i);

      if ( got_i <= 0 ) {
        if ( (got_i < 0) && (EINTR == errno) ) {
          continue;
        }
        fprintf(stderr, "blob: stage: read: %s\r\n",
                got_i ? strerror(errno) : "short file");
        ok_o = c3n;
        break;
      }
      if ( c3n == (ok_o = _blob_write(stg_i, buf_y, (c3_z)got_i)) ) {
        break;
      }
      len_d -= (c3_d)got_i;
    }
  }

  if ( c3n == _blob_stage_done(stg_i, stg_c, ok_o) ) {
    return c3n;
  }

  if ( c3n == _blob_measure(stg_c, mug_h) ) {
    fprintf(stderr, "blob: stage: %s denotes a direct atom\r\n", stg_c);
    c3_unlink(stg_c);
    return c3n;
  }
  return c3y;
}

/* u3b_live(): check whether a blob file exists.
*/
c3_o
u3b_live(const c3_c* bob_c, c3_h mug_h, c3_h seq_h)
{
  if ( !bob_c ) {
    return c3n;
  }

  c3_c fil_c[8192];
  u3b_path(fil_c, bob_c, mug_h, seq_h);

  struct stat st_u;
  return ( 0 == stat(fil_c, &st_u) ) ? c3y : c3n;
}

/* u3b_wipe(): delete a blob file.
*/
void
u3b_wipe(const c3_c* bob_c, c3_h mug_h, c3_h seq_h)
{
  if ( !bob_c ) {
    return;
  }

  c3_c fil_c[8192];
  u3b_path(fil_c, bob_c, mug_h, seq_h);

  //  a wipe is driven by the blob's last atom dying, and a view retains
  //  its atom until done unmaps, so no view in this process maps the
  //  file.  windows would refuse the unlink of a mapped file, which the
  //  error below reports; posix would keep the inode for the mapping
  //
  if ( 0 != unlink(fil_c) && ENOENT != errno ) {
    fprintf(stderr, "blob: failed to delete %s: %s\r\n",
            fil_c, strerror(errno));
  }

  //  the last blob takes its bucket with it
  //
  {
    c3_c dir_c[8192];
    _blob_mug_dir(dir_c, bob_c, mug_h);

    if (  (0 != rmdir(dir_c))
       && (ENOTEMPTY != errno) && (EEXIST != errno) && (ENOENT != errno) )
    {
      fprintf(stderr, "blob: failed to remove bucket %s: %s\r\n",
              dir_c, strerror(errno));
    }
  }
}

/* _blob_name_num(): parse a strictly-decimal dirent name into [out_h].
**
**   Rejects anything else: dotfiles, junk.
*/
static c3_o
_blob_name_num(const c3_c* nam_c, c3_h* out_h)
{
  //  digits only, no sign, space, or leading zero: the spelling
  //  u3b_path writes, and so one name per number
  //
  c3_d val_d = 0;
  c3_w len_w = 0;

  for ( ; nam_c[len_w]; len_w++ ) {
    if ( (nam_c[len_w] < '0') || (nam_c[len_w] > '9') || (len_w >= 10) ) {
      return c3n;
    }
    val_d = (10 * val_d) + (c3_d)(nam_c[len_w] - '0');
  }

  if ( !len_w || (('0' == nam_c[0]) && (len_w > 1)) || (val_d > 0xFFFFFFFFULL) ) {
    return c3n;
  }

  *out_h = (c3_h)val_d;
  return c3y;
}

/* _blob_name_mug(): parse a bucket name, eight lowercase hex digits,
**   into [out_h].
**
**   Rejects anything else: dotfiles, junk, uppercase, other widths.
*/
static c3_o
_blob_name_mug(const c3_c* nam_c, c3_h* out_h)
{
  //  exactly the spelling U3B_MUG_FMT writes, so one name per mug
  //
  c3_h val_h = 0;
  c3_w len_w = 0;

  for ( ; nam_c[len_w]; len_w++ ) {
    c3_c   c_c = nam_c[len_w];
    c3_h dig_h;

    if ( len_w >= 8 ) {
      return c3n;
    }
    else if ( (c_c >= '0') && (c_c <= '9') ) {
      dig_h = (c3_h)(c_c - '0');
    }
    else if ( (c_c >= 'a') && (c_c <= 'f') ) {
      dig_h = (c3_h)(c_c - 'a' + 10);
    }
    else {
      return c3n;
    }
    val_h = (val_h << 4) | dig_h;
  }

  if ( 8 != len_w ) {
    return c3n;
  }

  *out_h = val_h;
  return c3y;
}

/* u3b_walk(): enumerate every blob file in the store.
*/
void
u3b_walk(const c3_c* bob_c,
             void*       ptr_v,
             void      (*fun_f)(void*, c3_h, c3_h))
{
  if ( !bob_c ) {
    return;
  }

  DIR* bob_u = opendir(bob_c);
  if ( !bob_u ) {
    return;
  }

  struct dirent* mug_e;
  while ( (mug_e = readdir(bob_u)) ) {
    c3_h mug_h;
    if (  ('.' == mug_e->d_name[0])
       || (c3n == _blob_name_mug(mug_e->d_name, &mug_h)) )
    {
      continue;
    }

    c3_c dir_c[8192];
    _blob_mug_dir(dir_c, bob_c, mug_h);

    DIR* dir_u = opendir(dir_c);
    if ( !dir_u ) {
      continue;
    }

    struct dirent* seq_e;
    while ( (seq_e = readdir(dir_u)) ) {
      c3_h seq_h;
      if (  ('.' == seq_e->d_name[0])
         || (c3n == _blob_name_num(seq_e->d_name, &seq_h)) )
      {
        continue;
      }
      fun_f(ptr_v, mug_h, seq_h);
    }
    closedir(dir_u);
  }
  closedir(bob_u);
}

/* _blob_copy(): copy [src_c] to a new file [dst_c] with BLOB_FILE_MODE,
**   synced.  Refuses to replace; removes a partial copy on failure.
*/
static c3_o
_blob_copy(const c3_c* src_c, const c3_c* dst_c)
{
  c3_i src_i = open(src_c, O_RDONLY);
  c3_i dst_i = open(dst_c, O_WRONLY | O_CREAT | O_EXCL, BLOB_FILE_MODE);

  if ( -1 == src_i || -1 == dst_i ) {
    fprintf(stderr, "blob: copy %s to %s: %s\r\n",
            src_c, dst_c, strerror(errno));
    if ( -1 != src_i ) close(src_i);
    if ( -1 != dst_i ) { close(dst_i); unlink(dst_c); }
    return c3n;
  }

  c3_y buf_y[65536];
  c3_o ok_o = c3y;

  while ( c3y == ok_o ) {
    ssize_t red_i = read(src_i, buf_y, sizeof(buf_y));

    if ( 0 == red_i ) {
      break;
    }
    if ( red_i < 0 ) {
      if ( EINTR == errno ) {
        continue;
      }
      fprintf(stderr, "blob: copy: read %s: %s\r\n", src_c, strerror(errno));
      ok_o = c3n;
    }
    else {
      ok_o = _blob_write(dst_i, buf_y, (c3_z)red_i);
    }
  }

  if ( (c3y == ok_o) && (0 != c3_sync(dst_i)) ) {
    fprintf(stderr, "blob: copy: sync %s: %s\r\n", dst_c, strerror(errno));
    ok_o = c3n;
  }
  close(src_i);
  if ( (0 != close(dst_i)) && (c3y == ok_o) ) {
    fprintf(stderr, "blob: copy: close %s: %s\r\n", dst_c, strerror(errno));
    ok_o = c3n;
  }
  if ( c3n == ok_o ) {
    unlink(dst_c);
  }
  return ok_o;
}

/* _blob_same_file(): [a_c] and [b_c] name one file.  windows has no
**   inode in stat; its volume serial and file index are the same thing.
*/
static c3_o
_blob_same_file(const c3_c* a_c, const c3_c* b_c)
{
#ifdef U3_OS_windows
  c3_i a_i = open(a_c, O_RDONLY);
  c3_i b_i = open(b_c, O_RDONLY);
  c3_o sam_o = c3n;

  if ( (-1 != a_i) && (-1 != b_i) ) {
    BY_HANDLE_FILE_INFORMATION a_u, b_u;

    if (  GetFileInformationByHandle((HANDLE)_get_osfhandle(a_i), &a_u)
       && GetFileInformationByHandle((HANDLE)_get_osfhandle(b_i), &b_u) )
    {
      sam_o = __(  (a_u.dwVolumeSerialNumber == b_u.dwVolumeSerialNumber)
                && (a_u.nFileIndexHigh == b_u.nFileIndexHigh)
                && (a_u.nFileIndexLow == b_u.nFileIndexLow) );
    }
  }
  if ( -1 != a_i ) close(a_i);
  if ( -1 != b_i ) close(b_i);
  return sam_o;
#else
  struct stat a_u, b_u;

  return __(  (0 == stat(a_c, &a_u))
           && (0 == stat(b_c, &b_u))
           && (a_u.st_dev == b_u.st_dev)
           && (a_u.st_ino == b_u.st_ino) );
#endif
}

/* u3b_link(): carry a blob from store [old_c] into store [new_c].
**
**   A hard link, so the file is one inode owned by both epochs until
**   the older is chopped; a copy where the filesystem has no links.
**   The new bucket's directory is synced so the entry survives a crash.
*/
c3_o
u3b_link(const c3_c* old_c, const c3_c* new_c, c3_h mug_h, c3_h seq_h)
{
  c3_c src_c[8192];
  c3_c dst_c[8192];

  u3b_path(src_c, old_c, mug_h, seq_h);
  u3b_path(dst_c, new_c, mug_h, seq_h);

  if ( c3n == _blob_bucket_make(new_c, mug_h) ) {
    return c3n;
  }

  if ( 0 != c3_link(src_c, dst_c) ) {
    if ( EEXIST == errno ) {
      //  a retried roll finds its own link; anything else under that
      //  name is a different file and must not pass for this one
      //
      if ( c3y == _blob_same_file(src_c, dst_c) ) {
        return c3y;
      }
      fprintf(stderr, "blob: link: %s exists and is not %s\r\n", dst_c, src_c);
      return c3n;
    }
    if ( c3n == _blob_copy(src_c, dst_c) ) {
      fprintf(stderr, "blob: link %s into %s failed\r\n", src_c, new_c);
      return c3n;
    }
  }

  _blob_sync_dir(new_c, mug_h);
  return c3y;
}

/* _blob_dir_names(): the entry names of [dir_c] but . and .., or 0 if
**   it cannot be opened (errno says why).  An empty directory is a
**   non-null, empty list.  Read in full before anything is removed,
**   so the removal never races the enumeration.
*/
static c3_c**
_blob_dir_names(const c3_c* dir_c, c3_w* len_w)
{
  DIR*   dir_u = opendir(dir_c);
  c3_w   cap_w = 16;
  c3_c** nam_c;

  *len_w = 0;

  if ( !dir_u ) {
    return 0;
  }
  nam_c = c3_malloc(cap_w * sizeof(*nam_c));

  struct dirent* ent_e;
  while ( (ent_e = readdir(dir_u)) ) {
    if (  !strcmp(".", ent_e->d_name)
       || !strcmp("..", ent_e->d_name) )
    {
      continue;
    }
    if ( *len_w == cap_w ) {
      cap_w *= 2;
      nam_c = c3_realloc(nam_c, cap_w * sizeof(*nam_c));
    }
    nam_c[(*len_w)++] = strdup(ent_e->d_name);
  }
  closedir(dir_u);

  return nam_c;
}

/* u3b_kill(): remove a store root and everything in it.
**
**   An epoch's files die with the epoch.  Two levels, no recursion:
**   a bucket holds blob files, nothing else.  Every entry goes,
**   dotfiles included, or the rmdirs fail.  An entry is unlinked
**   before it is opened as a bucket, so a stray file or a link is
**   removed itself and never followed.  A root that does not exist
**   is not an error.
*/
void
u3b_kill(const c3_c* bob_c)
{
  c3_w   mug_w;
  c3_c** mug_c;

  mug_c = _blob_dir_names(bob_c, &mug_w);

  if ( !mug_c ) {
    if ( ENOENT != errno ) {
      fprintf(stderr, "blob: kill: open %s: %s\r\n", bob_c, strerror(errno));
    }
    return;
  }

  for ( c3_w i_w = 0; i_w < mug_w; i_w++ ) {
    c3_c   dir_c[8192];
    c3_w   seq_w;
    c3_c** seq_c;

    snprintf(dir_c, sizeof(dir_c), "%s/%s", bob_c, mug_c[i_w]);
    c3_free(mug_c[i_w]);

    //  a stray file or a link in the root goes; unlink refuses a bucket
    //
    if ( 0 == c3_unlink(dir_c) ) {
      continue;
    }

    if ( !(seq_c = _blob_dir_names(dir_c, &seq_w)) ) {
      fprintf(stderr, "blob: kill: open %s: %s\r\n", dir_c, strerror(errno));
      continue;
    }

    for ( c3_w j_w = 0; j_w < seq_w; j_w++ ) {
      c3_c fil_c[8192];
      snprintf(fil_c, sizeof(fil_c), "%s/%s", dir_c, seq_c[j_w]);
      c3_free(seq_c[j_w]);

      if ( 0 != c3_unlink(fil_c) ) {
        fprintf(stderr, "blob: kill: unlink %s: %s\r\n",
                fil_c, strerror(errno));
      }
    }
    c3_free(seq_c);

    if ( 0 != c3_rmdir(dir_c) ) {
      fprintf(stderr, "blob: kill: rmdir %s: %s\r\n", dir_c, strerror(errno));
    }
  }
  c3_free(mug_c);

  if ( 0 != c3_rmdir(bob_c) ) {
    fprintf(stderr, "blob: kill: rmdir %s: %s\r\n", bob_c, strerror(errno));
  }
}

/* u3b_move_stg(): install a staging file into the blob store.
**
**   [stg_c] is the path to a temp file under $pier/.urb/stg/.
**   Computes the mug of its content, checks for duplicates, then either
**   renames the staging file into <root>/<mug>/<seq> (no dup) or unlinks it
**   (dup found).  On success sets *mug_h and *seq_h.
**
**   The staging file is always consumed (renamed or unlinked) on success.
**   On failure the staging file is left in place.
*/
c3_o
u3b_move_stg(const c3_c* bob_c,
                    const c3_c* stg_c,
                    c3_h*       mug_h,
                    c3_h*       seq_h)
{
  if ( !bob_c ) {
    return c3n;
  }

  struct stat st_u;
  if ( -1 == stat(stg_c, &st_u) ) {
    fprintf(stderr, "blob: install_stg: stat failed on %s: %s\r\n",
            stg_c, strerror(errno));
    return c3n;
  }

  //  map_d is the mapping extent and must be what munmap is given; len_d
  //  shrinks to the atom's byte length once trailing zeros are trimmed
  //
  c3_d map_d = (c3_d)st_u.st_size;
  c3_d len_d = map_d;

  if ( 0 == map_d ) {
    fprintf(stderr, "blob: install_stg: refusing empty staging file %s\r\n",
            stg_c);
    return c3n;
  }

  //  O_RDWR so we can trim trailing zeros below; the staging file is ours
  //
  c3_i fid_i = open(stg_c, O_RDWR);
  if ( -1 == fid_i ) {
    fprintf(stderr, "blob: install_stg: open failed on %s: %s\r\n",
            stg_c, strerror(errno));
    return c3n;
  }

  void* map_v = mmap(0, (size_t)map_d, PROT_READ, MAP_PRIVATE, fid_i, 0);

  if ( MAP_FAILED == map_v ) {
    fprintf(stderr, "blob: install_stg: mmap failed on %s: %s\r\n",
            stg_c, strerror(errno));
    close(fid_i);
    return c3n;
  }
  madvise(map_v, (size_t)map_d, MADV_SEQUENTIAL);

  //  the atom's byte length: what lands in the store must be canonical,
  //  so the file is trimmed to match further down.  content whose atom
  //  the loom would keep direct has no bob representation (see
  //  U3_BLOB_MIN), and all-zero content lands there too, denoting 0.
  //
  if ( c3n == _blob_meas((const c3_y*)map_v, map_d, &len_d, mug_h) ) {
    fprintf(stderr, "blob: install_stg: %s denotes a direct atom "
                    "(%" PRIc3_d " significant bytes)\r\n", stg_c, len_d);
    munmap(map_v, (size_t)map_d);
    close(fid_i);
    return c3n;
  }

  //  the next seq: one past the highest in the bucket
  //
  c3_h nex_h = 0;
  {
    c3_c dir_c[8192];
    _blob_mug_dir(dir_c, bob_c, *mug_h);

    if ( c3y == _blob_bucket_make(bob_c, *mug_h) ) {
      nex_h = _blob_bucket_next(dir_c);
    }
  }
  if ( 0 == nex_h ) {
    munmap(map_v, (size_t)map_d);
    close(fid_i);
    return c3n;
  }

  //  check for duplicate content; a bucket that cannot be read refuses
  //  the install rather than risk a second seq for the same bytes
  //
  c3_h dup_h;
  if ( c3n == _blob_dedup(bob_c, *mug_h, nex_h,
                          (const c3_y*)map_v, len_d, &dup_h) )
  {
    munmap(map_v, (size_t)map_d);
    close(fid_i);
    return c3n;
  }

  //  NB: the mapping goes before the truncate below.  windows refuses to
  //  resize a file while a section is open on it, so trimming under the
  //  mapping -- which posix permits -- fails there with every trailing
  //  zero byte.  nothing reads the mapping past this point.
  //
  munmap(map_v, (size_t)map_d);

  if ( 0 != dup_h ) {
    //  duplicate found — consume staging file and return existing seq
    //
    close(fid_i);
    c3_unlink(stg_c);
    *seq_h = dup_h;
    return c3y;
  }

  //  trim the trailing zeros off the file itself, now that the mapping
  //  is gone, so the installed blob is byte-exact with the atom
  //
  if ( (len_d != map_d) && (0 != ftruncate(fid_i, (off_t)len_d)) ) {
    fprintf(stderr, "blob: install_stg: ftruncate failed on %s: %s\r\n",
            stg_c, strerror(errno));
    close(fid_i);
    return c3n;
  }

  //  the one sync of the blob's bytes and length: staging leaves it to
  //  this, so content dedup discards is never synced at all
  //
  if ( 0 != c3_sync(fid_i) ) {
    fprintf(stderr, "blob: install_stg: fsync failed on %s: %s\r\n",
            stg_c, strerror(errno));
    close(fid_i);
    return c3n;
  }

#ifndef U3_OS_windows
  //  the staging file keeps mkstemp's 0600; the installed blob carries
  //  BLOB_FILE_MODE, so nothing opens it for writing again
  //
  if ( 0 != fchmod(fid_i, BLOB_FILE_MODE) ) {
    fprintf(stderr, "blob: install_stg: fchmod failed on %s: %s\r\n",
            stg_c, strerror(errno));
    close(fid_i);
    return c3n;
  }
#endif

  close(fid_i);

  //  rename staging file into final location.  an install never
  //  replaces a file: a seq the bucket already holds is an accounting
  //  bug, and overwriting would silently change what every bob naming
  //  it denotes
  //
  c3_c dst_c[8192];
  u3b_path(dst_c, bob_c, *mug_h, nex_h);

  {
    struct stat st_u;
    if ( 0 == stat(dst_c, &st_u) ) {
      fprintf(stderr, "blob: install_stg: %s exists; refusing to replace it\r\n",
              dst_c);
      return c3n;
    }
  }

  if ( 0 != rename(stg_c, dst_c) ) {
    //  rename can fail cross-device; fall back to copy-and-unlink
    //
    if ( c3n == _blob_copy(stg_c, dst_c) ) {
      fprintf(stderr, "blob: install_stg: rename+fallback failed on %s\r\n",
              stg_c);
      return c3n;
    }
    c3_unlink(stg_c);
  }

  //  fsync the bucket directory so the new entry survives a crash
  //  before the next global sync — a committed event must never name
  //  a blob whose directory entry was lost
  //
  _blob_sync_dir(bob_c, *mug_h);

  *seq_h = nex_h;
  return c3y;
}

/* u3b_bsink: streaming byte sink for blob-aware cue.
**
**   receives a large atom's bytes in chunks, writes them to a staging
**   file, then installs the file into the blob store (mug, dedup,
**   rename via u3b_move_stg) and returns a bob atom.  the atom's
**   bytes never touch the loom.
*/

static c3_o
_blob_bsink_opn(void* ptr_v)
{
  u3b_bsink* bsk_u = ptr_v;

  c3_c stg_c[8192];
  u3b_stg_dir(stg_c, bsk_u->pax_c);
  snprintf(bsk_u->stg_c, sizeof(bsk_u->stg_c), "%s/cue-XXXXXX", stg_c);

  bsk_u->fid_i = mkstemp(bsk_u->stg_c);

  if ( bsk_u->fid_i < 0 ) {
    fprintf(stderr, "blob: bsink: mkstemp failed (%s): %s\r\n",
            bsk_u->stg_c, strerror(errno));
    return c3n;
  }

  return c3y;
}

static c3_o
_blob_bsink_wri(void* ptr_v, const c3_y* byt_y, c3_z len_z)
{
  u3b_bsink* bsk_u = ptr_v;

  //  the same chunked writer the drivers stage through: capped at
  //  BLOB_IO_MAX per call, EINTR retried
  //
  if ( c3n == _blob_write(bsk_u->fid_i, byt_y, len_z) ) {
    fprintf(stderr, "blob: bsink: write failed (%s)\r\n", bsk_u->stg_c);
    close(bsk_u->fid_i);
    c3_unlink(bsk_u->stg_c);
    bsk_u->fid_i = -1;
    return c3n;
  }

  return c3y;
}

static u3_weak
_blob_bsink_don(void* ptr_v)
{
  u3b_bsink* bsk_u = ptr_v;
  c3_h mug_h = 0;
  c3_h seq_h = 0;

  if ( bsk_u->fid_i < 0 ) {
    return u3_none;
  }

  close(bsk_u->fid_i);
  bsk_u->fid_i = -1;

  if ( c3n == u3b_move_stg(bsk_u->bob_c, bsk_u->stg_c,
                               &mug_h, &seq_h) )
  {
    fprintf(stderr, "blob: bsink: install failed (%s)\r\n", bsk_u->stg_c);
    c3_unlink(bsk_u->stg_c);
    return u3_none;
  }

  bsk_u->num_w += 1;

  return u3i_blob(mug_h, seq_h);
}

/* u3b_bsink_init(): prepare a sink staging in pier [pax_c] and
**   installing into the store root [bob_c].
*/
void
u3b_bsink_init(u3b_bsink* bsk_u, const c3_c* pax_c, const c3_c* bob_c)
{
  memset(bsk_u, 0, sizeof(*bsk_u));

  bsk_u->pax_c       = pax_c;
  bsk_u->bob_c       = bob_c;
  bsk_u->fid_i       = -1;
  bsk_u->snk_u.ptr_v = bsk_u;
  bsk_u->snk_u.opn_f = _blob_bsink_opn;
  bsk_u->snk_u.wri_f = _blob_bsink_wri;
  bsk_u->snk_u.don_f = _blob_bsink_don;
}
