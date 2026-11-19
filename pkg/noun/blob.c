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

/* _blob_pread(): pread at a 64-bit offset.  the windows shim's off_t
**   is 32 bits, so there the read goes straight to ReadFile.
*/
static ssize_t
_blob_pread(c3_i fid_i, void* buf_v, size_t len_i, c3_d off_d)
{
#ifdef U3_OS_windows
  HANDLE     fil_h = (HANDLE)_get_osfhandle(fid_i);
  OVERLAPPED ovl_u = {0};
  DWORD      got_u = 0;

  ovl_u.Offset     = (DWORD)(off_d & 0xffffffffULL);
  ovl_u.OffsetHigh = (DWORD)(off_d >> 32);

  if ( !ReadFile(fil_h, buf_v, (DWORD)len_i, &got_u, &ovl_u) ) {
    if ( ERROR_HANDLE_EOF == GetLastError() ) {
      return 0;
    }
    errno = EIO;
    return -1;
  }
  return (ssize_t)got_u;
#else
  return pread(fid_i, buf_v, len_i, (off_t)off_d);
#endif
}

//  maximum bytes per single read()/write() call.
//  POSIX allows read()/write() to return EINVAL if count > SSIZE_MAX;
//  macOS returns EINVAL if count > INT_MAX.  cap conservatively at 1 GiB.
//
#define BLOB_IO_MAX  ((size_t)0x40000000UL)

/* blob reads: a process-wide fd cache and road-owned mappings.
**
**   no caller holds an fd once a call returns.  every fd a read uses
**   lives in a slot of _blob_fds_u, a direct-mapped table keyed by
**   blob id, so any slot may be closed between calls and nothing counts
**   its users.  a signal that unwinds through a read leaves the fd in
**   its slot.  out of descriptors, a miss closes every slot and retries
**   once, so a computation never fails for the fds it has touched.
**
**   a flat view needs bytes that outlive the call, so it gets a
**   mapping, which holds no fd.  mappings live on one C-heap list, each
**   tagged with the road that made it.  an inner road shares its own
**   and its inner ancestors' mappings and releases them when it falls
**   (u3b_drain) or at a signal reset (u3b_drain_kids).  the home road
**   never falls: each of its flat views maps alone and unmaps at done.
**
**   a cached fd must not outlive the file it names: the store drops a
**   slot before it unlinks or installs that id, and flushes the table
**   when it kills an epoch or changes its root (u3b_flush).  the king
**   drops its slot when its last bob over an id dies, before it returns
**   the holds that let mars wipe the file.
**
**   every change to the table or the list runs inside
**   u3m_crit_inn()/u3m_crit_out(), so a signal can neither leak an fd
**   or a mapping nor cut the list.  one thread per process reads.
*/

//  slots in the fd cache; a power of two
//
#define BLOB_FDS  64

//  read-only, and never inherited by a process this one starts
//
#ifdef U3_OS_windows
#  define BLOB_O_READ  (O_RDONLY | _O_NOINHERIT)
#else
#  define BLOB_O_READ  (O_RDONLY | O_CLOEXEC)
#endif

/* _blob_fd: a cache slot.  bid_d is 0 when empty: no blob has seq 0.
*/
typedef struct {
  c3_d bid_d;   //  (mug_h << 32) | seq_h, or 0
  c3_i fid_i;   //  open read-only, when bid_d is set
  c3_d len_d;   //  file size at open
  c3_d bit_d;   //  bit length of the content
} _blob_fd;

static _blob_fd _blob_fds_u[BLOB_FDS];

/* _blob_flat: a mapping, owned by the road that made it.
*/
typedef struct _blob_flat {
  struct _blob_flat* nex_u;   //  next on the process's list
  u3a_road*          rod_u;   //  owning road
  c3_d               bid_d;   //  blob id
  c3_y*              map_y;   //  read-only mapping
  c3_d               map_d;   //  mapped extent, for munmap
} _blob_flat;

static _blob_flat* _blob_flats_u;

/* _blob_slot(): the cache slot of [bid_d].
*/
static inline _blob_fd*
_blob_slot(c3_d bid_d)
{
  return &_blob_fds_u[((bid_d >> 32) ^ bid_d) & (BLOB_FDS - 1)];
}

/* _blob_slot_close(): empty [fd_u].  the caller holds the critical
**   section.
*/
static void
_blob_slot_close(_blob_fd* fd_u)
{
  if ( fd_u->bid_d ) {
    c3_i fid_i   = fd_u->fid_i;
    fd_u->bid_d  = 0;
    fd_u->fid_i  = -1;
    close(fid_i);
  }
}

/* _blob_slots_close(): empty every slot.  the caller holds the
**   critical section.
*/
static void
_blob_slots_close(void)
{
  for ( c3_w i_w = 0; i_w < BLOB_FDS; i_w++ ) {
    _blob_slot_close(&_blob_fds_u[i_w]);
  }
}

/* _blob_read_fd(): read [len_z] bytes at [off_d] of blob [bid_d],
**   open on [fid_i], into [dst_y]: a pread in BLOB_IO_MAX chunks,
**   retrying EINTR.  short only at end of file or on error.
*/
static c3_z
_blob_read_fd(c3_i fid_i, c3_d bid_d, c3_d off_d, c3_y* dst_y, c3_z len_z)
{
  c3_z tot_z = 0;

  while ( len_z ) {
    size_t  ask_i = ( len_z < BLOB_IO_MAX ) ? len_z : BLOB_IO_MAX;
    ssize_t got_i = _blob_pread(fid_i, dst_y, ask_i, off_d);

    if ( got_i < 0 ) {
      if ( EINTR == errno ) {
        continue;
      }
      fprintf(stderr, "blob: read: " U3A_BID_FMT " at %" PRIc3_d
                      ": %s\r\n",
              u3a_bid_mug(bid_d), u3a_bid_seq(bid_d),
              off_d, strerror(errno));
      break;
    }

    if ( 0 == got_i ) {
      break;
    }

    tot_z += (c3_z)got_i;
    dst_y += got_i;
    off_d += (c3_d)got_i;
    len_z -= (c3_z)got_i;
  }

  return tot_z;
}

/* _blob_met(): bit length of the content of blob [bid_d], [len_d]
**   bytes open on [fid_i]: u3r_met(0, atom).
**
**   a blob is canonical, so its last byte is nonzero and the length
**   follows from that byte alone.  returns 0 if it is zero or cannot
**   be read: no blob denotes such an atom.
*/
static c3_d
_blob_met(c3_i fid_i, c3_d bid_d, c3_d len_d)
{
  c3_y las_y;

  if (  !len_d
     || (1 != _blob_read_fd(fid_i, bid_d, len_d - 1, &las_y, 1))
     || !las_y )
  {
    return 0;
  }

  return ((len_d - 1) * 8) + (32 - __builtin_clz((unsigned int)las_y));
}

/* _blob_fd_get(): the cache slot holding blob [bid_d] open, filled on a
**   miss; 0 if the file is missing, empty, all zero, or unreadable.
**
**   a miss replaces the slot's blob.  out of descriptors, it closes
**   every slot and retries once: no caller holds a cached fd past its
**   call, so every slot is free to go.
*/
static _blob_fd*
_blob_fd_get(const c3_c* bob_c, c3_d bid_d)
{
  _blob_fd*   fd_u = _blob_slot(bid_d);
  c3_c        fil_c[8192];
  struct stat st_u;
  c3_i        fid_i;
  c3_d        len_d;
  c3_d        bit_d;

  if ( bid_d == fd_u->bid_d ) {
    return fd_u;
  }

  //  a process that never learned its epoch has no store
  //
  if ( !bob_c ) {
    return 0;
  }

  u3b_path(fil_c, bob_c, u3a_bid_mug(bid_d), u3a_bid_seq(bid_d));

  u3m_crit_inn();

  _blob_slot_close(fd_u);

  if (  (-1 == (fid_i = open(fil_c, BLOB_O_READ)))
     && ((EMFILE == errno) || (ENFILE == errno)) )
  {
    _blob_slots_close();
    fid_i = open(fil_c, BLOB_O_READ);
  }

  if ( -1 == fid_i ) {
    u3m_crit_out();
    fprintf(stderr, "blob: open %s: %s\r\n", fil_c, strerror(errno));
    return 0;
  }

  if ( -1 == fstat(fid_i, &st_u) ) {
    close(fid_i);
    u3m_crit_out();
    fprintf(stderr, "blob: fstat %s: %s\r\n", fil_c, strerror(errno));
    return 0;
  }

  len_d = (c3_d)st_u.st_size;

  //  empty, all zero, or unreadable: no blob denotes such an atom
  //
  if ( !(bit_d = _blob_met(fid_i, bid_d, len_d)) ) {
    close(fid_i);
    u3m_crit_out();
    return 0;
  }

  fd_u->bid_d = bid_d;
  fd_u->fid_i = fid_i;
  fd_u->len_d = len_d;
  fd_u->bit_d = bit_d;

  u3m_crit_out();

  return fd_u;
}

/* u3b_stat(): the bit length of blob [bid_d] in [bit_d].
*/
c3_o
u3b_stat(const c3_c* bob_c, c3_d bid_d, c3_d* bit_d)
{
  _blob_fd* fd_u = _blob_fd_get(bob_c, bid_d);

  if ( !fd_u ) {
    return c3n;
  }
  *bit_d = fd_u->bit_d;
  return c3y;
}

/* u3b_read(): read [len_z] bytes at [off_d] of blob [bid_d] into [dst_y].
*/
c3_z
u3b_read(const c3_c* bob_c, c3_d bid_d, c3_d off_d, c3_y* dst_y, c3_z len_z)
{
  _blob_fd* fd_u = _blob_fd_get(bob_c, bid_d);

  if ( !fd_u ) {
    return 0;
  }
  return _blob_read_fd(fd_u->fid_i, bid_d, off_d, dst_y, len_z);
}

/* u3b_drop(): close the cached fd of blob (mug, seq), if any.
*/
void
u3b_drop(c3_h mug_h, c3_h seq_h)
{
  _blob_fd* fd_u = _blob_slot(u3a_bid(mug_h, seq_h));

  if ( u3a_bid(mug_h, seq_h) == fd_u->bid_d ) {
    u3m_crit_inn();
    _blob_slot_close(fd_u);
    u3m_crit_out();
  }
}

/* u3b_flush(): close every cached fd.
*/
void
u3b_flush(void)
{
  u3m_crit_inn();
  _blob_slots_close();
  u3m_crit_out();
}

/* u3b_fds(): fds the cache holds open.
*/
c3_z
u3b_fds(void)
{
  c3_z num_z = 0;

  for ( c3_w i_w = 0; i_w < BLOB_FDS; i_w++ ) {
    num_z += !!_blob_fds_u[i_w].bid_d;
  }
  return num_z;
}

/* _blob_map_fd(): map [len_d] bytes open on [fid_i] read-only, zero to
**   one page past them, setting [map_d] to the extent munmap is given.
**
**   within the file's own pages the kernel zero-fills the tail of the
**   last.  past them, on posix, the bytes come from an anonymous zero
**   page the file's pages are fixed over, so a reader may run one page
**   past the end.  windows refuses a section longer than the file, so
**   there the mapping is the file's pages and a reader stays within
**   the last word, as every accessor does.  the mapping holds no fd:
**   it outlives [fid_i] on both.
*/
static c3_y*
_blob_map_fd(c3_i fid_i, c3_d len_d, c3_d* map_d)
{
#ifdef U3_OS_windows
  void* map_v = mmap(0, (size_t)len_d, PROT_READ, MAP_PRIVATE, fid_i, 0);

  if ( MAP_FAILED == map_v ) {
    return 0;
  }
  *map_d = len_d;
  return map_v;
#else
  c3_d  pag_d = (c3_d)sysconf(_SC_PAGESIZE);
  c3_d  siz_d = ((len_d + pag_d - 1) & ~(pag_d - 1)) + pag_d;
  void* res_v = mmap(0, (size_t)siz_d, PROT_READ,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if ( MAP_FAILED == res_v ) {
    return 0;
  }
  if ( MAP_FAILED == mmap(res_v, (size_t)len_d, PROT_READ,
                          MAP_PRIVATE | MAP_FIXED, fid_i, 0) )
  {
    munmap(res_v, (size_t)siz_d);
    return 0;
  }
  *map_d = siz_d;
  return res_v;
#endif
}

/* _blob_road_line(): [rod_u] is the current inner road or an inner
**   ancestor of it.
*/
static c3_t
_blob_road_line(const u3a_road* rod_u)
{
  for ( u3a_road* lin_u = u3R;
        lin_u != &(u3H->rod_u);
        lin_u = u3to(u3a_road, lin_u->par_p) )
  {
    if ( lin_u == rod_u ) {
      return 1;
    }
  }
  return 0;
}

/* u3b_map(): blob [bid_d], [len_d] bytes, mapped read-only.
**
**   an ancestor outlives its children, so an inner road may return any
**   mapping on its line.  a file shortened under the view would fault
**   inside the mapping instead of failing a read, so one is refused.
*/
const c3_y*
u3b_map(const c3_c* bob_c, c3_d bid_d, c3_d len_d)
{
  _blob_fd*   fd_u;
  _blob_flat* fla_u;
  c3_y*       map_y;
  struct stat st_u;

  if ( &(u3H->rod_u) != u3R ) {
    for ( fla_u = _blob_flats_u; fla_u; fla_u = fla_u->nex_u ) {
      if ( (bid_d == fla_u->bid_d) && _blob_road_line(fla_u->rod_u) ) {
        return fla_u->map_y;
      }
    }
  }

  if ( !(fd_u = _blob_fd_get(bob_c, bid_d)) ) {
    return 0;
  }

  if (  (-1 == fstat(fd_u->fid_i, &st_u))
     || ((c3_d)st_u.st_size < len_d) )
  {
    fprintf(stderr, "blob: map: " U3A_BID_FMT ": file shrank\r\n",
            u3a_bid_mug(bid_d), u3a_bid_seq(bid_d));
    return 0;
  }

  fla_u = c3_malloc(sizeof(*fla_u));
  fla_u->rod_u = u3R;
  fla_u->bid_d = bid_d;

  u3m_crit_inn();
  if ( (map_y = _blob_map_fd(fd_u->fid_i, len_d, &fla_u->map_d)) ) {
    fla_u->map_y  = map_y;
    fla_u->nex_u  = _blob_flats_u;
    _blob_flats_u = fla_u;
  }
  u3m_crit_out();

  if ( !map_y ) {
    fprintf(stderr, "blob: map: " U3A_BID_FMT ": mmap: %s\r\n",
            u3a_bid_mug(bid_d), u3a_bid_seq(bid_d), strerror(errno));
    c3_free(fla_u);
  }
  return map_y;
}

/* _blob_flats_free(): unmap every mapping [fun_f] selects; how many.
*/
static c3_w
_blob_flats_free(c3_t (*fun_f)(const _blob_flat*, const void*),
                 const void* arg_v)
{
  _blob_flat** fla_u;
  c3_w         num_w = 0;

  //  most roads map nothing: those skip the critical section
  //
  if ( !_blob_flats_u ) {
    return 0;
  }

  u3m_crit_inn();

  for ( fla_u = &_blob_flats_u; *fla_u; ) {
    _blob_flat* dis_u = *fla_u;

    if ( fun_f(dis_u, arg_v) ) {
      *fla_u = dis_u->nex_u;
      munmap(dis_u->map_y, (size_t)dis_u->map_d);
      c3_free(dis_u);
      num_w++;
    }
    else {
      fla_u = &dis_u->nex_u;
    }
  }

  u3m_crit_out();

  return num_w;
}

static c3_t
_blob_flat_ptr(const _blob_flat* fla_u, const void* map_v)
{
  return (&(u3H->rod_u) == fla_u->rod_u) && (map_v == fla_u->map_y);
}

static c3_t
_blob_flat_road(const _blob_flat* fla_u, const void* rod_v)
{
  return rod_v == fla_u->rod_u;
}

static c3_t
_blob_flat_kid(const _blob_flat* fla_u, const void* rod_v)
{
  (void)rod_v;
  return &(u3H->rod_u) != fla_u->rod_u;
}

static c3_t
_blob_flat_any(const _blob_flat* fla_u, const void* rod_v)
{
  (void)fla_u;
  (void)rod_v;
  return 1;
}

/* u3b_unmap(): the home road is done with mapping [map_y].
*/
void
u3b_unmap(const c3_y* map_y)
{
  //  an inner road's mappings go with the road
  //
  if ( &(u3H->rod_u) == u3R ) {
    c3_w num_w = _blob_flats_free(_blob_flat_ptr, map_y);
    u3_assert( 1 == num_w );
  }
}

/* u3b_drain(): release every mapping road [rod_v] made.
*/
void
u3b_drain(void* rod_v)
{
  _blob_flats_free(_blob_flat_road, rod_v);
}

/* u3b_drain_kids(): release every mapping made below the home road.
*/
void
u3b_drain_kids(void)
{
  _blob_flats_free(_blob_flat_kid, 0);
}

/* u3b_stop(): release every mapping and close every cached fd.
*/
void
u3b_stop(void)
{
  _blob_flats_free(_blob_flat_any, 0);
  u3b_flush();
}

/* u3b_maps(): mappings the process holds.
*/
c3_z
u3b_maps(void)
{
  c3_z num_z = 0;

  for ( _blob_flat* fla_u = _blob_flats_u; fla_u; fla_u = fla_u->nex_u ) {
    num_z++;
  }
  return num_z;
}

/* u3b_maps_road(): mappings road [rod_v] made.
*/
c3_z
u3b_maps_road(void* rod_v)
{
  c3_z num_z = 0;

  for ( _blob_flat* fla_u = _blob_flats_u; fla_u; fla_u = fla_u->nex_u ) {
    num_z += ( rod_v == fla_u->rod_u );
  }
  return num_z;
}

/* u3b_mapped(): bytes the process holds mapped.
*/
c3_d
u3b_mapped(void)
{
  c3_d tot_d = 0;

  for ( _blob_flat* fla_u = _blob_flats_u; fla_u; fla_u = fla_u->nex_u ) {
    tot_d += fla_u->map_d;
  }
  return tot_d;
}

/* _blob_mapped_bid(): mappings of blob [bid_d] on any road.
*/
static c3_w
_blob_mapped_bid(c3_d bid_d)
{
  c3_w num_w = 0;

  for ( _blob_flat* fla_u = _blob_flats_u; fla_u; fla_u = fla_u->nex_u ) {
    num_w += ( bid_d == fla_u->bid_d );
  }
  return num_w;
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

  //  a wipe is driven by the blob's refcount reaching zero, so no reader
  //  can still map it: a reader holds a live bob, and the king keeps
  //  the bob behind each view, and so its holds, until the view closes.
  //  a mapping here is an accounting bug; the check sees only this
  //  process.  posix keeps the inode alive for the mapping regardless;
  //  windows refuses the unlink, which the error below reports.  the
  //  cached fd goes first: it would hold the file on windows, and read
  //  the old bytes on posix once the seq is reused
  //
  {
    c3_w num_w = _blob_mapped_bid(u3a_bid(mug_h, seq_h));

    if ( num_w ) {
      fprintf(stderr, "blob: wipe: " U3A_BID_FMT ": %" PRIc3_w
                      " mapping(s) live\r\n", mug_h, seq_h, num_w);
    }
  }

  u3b_drop(mug_h, seq_h);

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

  //  no cached fd may hold a file of the epoch, which windows would
  //  refuse to delete
  //
  u3b_flush();

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

  //  a seq is reused once its bucket's highest is wiped: no cached fd
  //  may name the file this one replaces
  //
  u3b_drop(*mug_h, nex_h);

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
