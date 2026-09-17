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

//  mode for a finished blob file.
//
//    blobs are immutable once written, so posix creates them read-only.
//    windows turns a mode without owner-write into
//    FILE_ATTRIBUTE_READONLY and then refuses to delete the file, which
//    would break u3_blob_wipe and with it blob gc and |chop.  give
//    windows an owner-writable mode instead; nothing ever opens a blob
//    for writing on any platform.
//
#ifdef U3_OS_windows
#  define BLOB_FILE_MODE 0600
#else
#  define BLOB_FILE_MODE 0400
#endif

//  maximum bytes per single read()/write() call.
//  POSIX allows read()/write() to return EINVAL if count > SSIZE_MAX;
//  macOS returns EINVAL if count > INT_MAX.  cap conservatively at 1 GiB.
//
#define BLOB_IO_MAX  ((size_t)0x40000000UL)

//  window for backward scans (u3_blob_hand_met).
//
#define BLOB_WIN_MAX ((size_t)4096)

/* handle registry: bid -> u3_blob_hand*.
**
**   process-global, C heap.  values are pointers so a view's hand stays
**   valid while later opens rehash the table.  every mutation runs inside
**   u3m_crit_enter()/u3m_crit_leave(): the table is not async-signal-safe,
**   and an fd must be recorded before a signal can unwind the opener.
*/
static c3_d
_blob_hand_hash(c3_d bid_d)
{
  return bid_d * 11400714819323198485ULL;
}

static c3_i
_blob_hand_cmp(c3_d a_d, c3_d b_d)
{
  return a_d == b_d;
}

#define NAME    _blob_hands
#define KEY_TY  c3_d
#define VAL_TY  u3_blob_hand*
#define HASH_FN _blob_hand_hash
#define CMPR_FN _blob_hand_cmp
#include "verstable.h"

static _blob_hands _blob_han_u;

/* u3_blob_init(): initialize the handle registry.
*/
void
u3_blob_init(void)
{
  vt_init(&_blob_han_u);
}

/* u3_blob_hands(): number of open hands.
*/
c3_z
u3_blob_hands(void)
{
  return vt_size(&_blob_han_u);
}

/* _blob_bid(): registry key for (mug_h, seq_h).
*/
static inline c3_d
_blob_bid(c3_h mug_h, c3_h seq_h)
{
  return ((c3_d)mug_h << 32) | seq_h;
}

/* _blob_hand_free(): close and free [han_u]; the caller erases it and
**   holds the critical section.
*/
static void
_blob_hand_free(u3_blob_hand* han_u)
{
  if ( han_u->fid_i >= 0 ) {
    c3_i fid_i   = han_u->fid_i;
    han_u->fid_i = -1;
    close(fid_i);
  }

  c3_free(han_u->buf_y);
  han_u->buf_y = 0;

  {
    u3_blob_ref* ref_u = han_u->ref_u;
    while ( ref_u ) {
      u3_blob_ref* nex_u = ref_u->nex_u;
      c3_free(ref_u);
      ref_u = nex_u;
    }
    han_u->ref_u = 0;
  }

  c3_free(han_u);
}

/* _blob_hand_gain(): add a reference to [han_u] owned by [rod_v].
*/
static void
_blob_hand_gain(u3_blob_hand* han_u, void* rod_v)
{
  u3_blob_ref* ref_u = han_u->ref_u;

  while ( ref_u && (ref_u->rod_v != rod_v) ) {
    ref_u = ref_u->nex_u;
  }

  if ( !ref_u ) {
    ref_u        = c3_calloc(sizeof(*ref_u));
    ref_u->rod_v = rod_v;
    ref_u->nex_u = han_u->ref_u;
    han_u->ref_u = ref_u;
  }

  ref_u->ref_w += 1;
  han_u->ref_w += 1;
}

/* _blob_hand_lose(): drop one reference to [han_u] owned by [rod_v].
**
**   Returns c3n if [rod_v] holds none.
*/
static c3_o
_blob_hand_lose(u3_blob_hand* han_u, void* rod_v)
{
  u3_blob_ref** lin_u = &han_u->ref_u;

  while ( *lin_u && ((*lin_u)->rod_v != rod_v) ) {
    lin_u = &(*lin_u)->nex_u;
  }

  if ( !*lin_u ) {
    return c3n;
  }

  {
    u3_blob_ref* ref_u = *lin_u;

    ref_u->ref_w -= 1;
    han_u->ref_w -= 1;

    if ( 0 == ref_u->ref_w ) {
      *lin_u = ref_u->nex_u;
      c3_free(ref_u);
    }
  }

  return c3y;
}

/* _blob_hand_trim(): shrink the table after a burst has drained.
*/
static void
_blob_hand_trim(void)
{
  c3_z siz_z = vt_size(&_blob_han_u);
  c3_z buk_z = vt_bucket_count(&_blob_han_u);

  if ( (buk_z > 64) && (siz_z < (buk_z / 4)) ) {
    vt_shrink(&_blob_han_u);
  }
}

/* u3_blob_open(): open a blob, or add a reference to its open hand.
*/
u3_blob_hand*
u3_blob_open(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  c3_d          bid_d = _blob_bid(mug_h, seq_h);
  u3_blob_hand* han_u;

  u3m_crit_enter();

  {
    _blob_hands_itr itr_u = vt_get_or_insert(&_blob_han_u, bid_d, 0);
    u3_assert( !vt_is_end(itr_u) );  //  OOM

    han_u = itr_u.data->val;

    if ( !han_u ) {
      c3_c        fil_c[8192];
      struct stat st_u;

      //  the table entry exists before the fd does, so a sweep between
      //  the two releases whatever open() produced.
      //
      han_u           = c3_calloc(sizeof(*han_u));
      han_u->bid_d    = bid_d;
      han_u->fid_i    = -1;
      itr_u.data->val = han_u;

      u3_blob_path(fil_c, pax_c, mug_h, seq_h);

      han_u->fid_i = open(fil_c, O_RDONLY);

      if ( -1 == han_u->fid_i ) {
        fprintf(stderr, "blob: open failed %s: %s\r\n",
                fil_c, strerror(errno));
      }
      else if ( -1 == fstat(han_u->fid_i, &st_u) ) {
        fprintf(stderr, "blob: fstat failed %s: %s\r\n",
                fil_c, strerror(errno));
      }
      else {
        han_u->len_d = (c3_d)st_u.st_size;
      }

      if ( 0 == han_u->len_d ) {
        vt_erase(&_blob_han_u, bid_d);
        _blob_hand_free(han_u);
        u3m_crit_leave();
        return 0;
      }
    }

    _blob_hand_gain(han_u, u3R);
  }

  u3m_crit_leave();
  return han_u;
}

/* u3_blob_close(): drop the current road's reference to [han_u].
*/
void
u3_blob_close(u3_blob_hand* han_u)
{
  u3m_crit_enter();

  if ( c3n == _blob_hand_lose(han_u, u3R) ) {
    fprintf(stderr, "blob: close: %08" PRIx32 "/%08" PRIx32 ": "
                    "no reference from this road\r\n",
            (c3_h)(han_u->bid_d >> 32), (c3_h)(han_u->bid_d & 0xFFFFFFFF));
    u3_assert(!"blob close");
  }

  if ( 0 == han_u->ref_w ) {
    vt_erase(&_blob_han_u, han_u->bid_d);
    _blob_hand_free(han_u);
    _blob_hand_trim();
  }

  u3m_crit_leave();
}

/* _blob_sweep_with(): release every reference whose road satisfies [own_f].
**
**   Returns the number of references released.
*/
static c3_w
_blob_sweep_with(c3_o (*own_f)(void*, void*), void* ptr_v)
{
  c3_w num_w = 0;

  u3m_crit_enter();

  {
    _blob_hands_itr itr_u = vt_first(&_blob_han_u);

    while ( !vt_is_end(itr_u) ) {
      u3_blob_hand* han_u = itr_u.data->val;
      u3_blob_ref** lin_u = &han_u->ref_u;

      while ( *lin_u ) {
        u3_blob_ref* ref_u = *lin_u;

        if ( c3y == own_f(ref_u->rod_v, ptr_v) ) {
          han_u->ref_w -= ref_u->ref_w;
          num_w        += ref_u->ref_w;
          *lin_u        = ref_u->nex_u;
          c3_free(ref_u);
        }
        else {
          lin_u = &ref_u->nex_u;
        }
      }

      if ( 0 == han_u->ref_w ) {
        _blob_hand_free(han_u);
        itr_u = vt_erase_itr(&_blob_han_u, itr_u);
      }
      else {
        itr_u = vt_next(itr_u);
      }
    }

    _blob_hand_trim();
  }

  u3m_crit_leave();
  return num_w;
}

static c3_o
_blob_own_rod(void* rod_v, void* ptr_v)
{
  return ( rod_v == ptr_v ) ? c3y : c3n;
}

static c3_o
_blob_own_kid(void* rod_v, void* ptr_v)
{
  return ( rod_v != ptr_v ) ? c3y : c3n;
}

static c3_o
_blob_own_all(void* rod_v, void* ptr_v)
{
  return c3y;
}

/* u3_blob_sweep(): release every reference held by road [rod_v].
*/
void
u3_blob_sweep(void* rod_v)
{
  _blob_sweep_with(_blob_own_rod, rod_v);
}

/* u3_blob_sweep_kids(): release every reference not held by the home road.
*/
c3_w
u3_blob_sweep_kids(void)
{
  return _blob_sweep_with(_blob_own_kid, &u3H->rod_u);
}

/* u3_blob_stop(): release every handle and the registry.
*/
void
u3_blob_stop(void)
{
  _blob_sweep_with(_blob_own_all, 0);
  vt_cleanup(&_blob_han_u);
}

/* u3_blob_read(): read [len_z] bytes at [off_d] into [dst_y].
*/
c3_z
u3_blob_read(u3_blob_hand* han_u, c3_d off_d, c3_y* dst_y, c3_z len_z)
{
  c3_z tot_z = 0;

  while ( len_z ) {
    size_t  ask_i = ( len_z < BLOB_IO_MAX ) ? len_z : BLOB_IO_MAX;
    ssize_t got_i = pread(han_u->fid_i, dst_y, ask_i, (off_t)off_d);

    if ( got_i < 0 ) {
      if ( EINTR == errno ) {
        continue;
      }
      fprintf(stderr, "blob: read: %08" PRIx32 "/%08" PRIx32 " at %" PRIc3_d
                      ": %s\r\n",
              (c3_h)(han_u->bid_d >> 32), (c3_h)(han_u->bid_d & 0xFFFFFFFF),
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

/* u3_blob_data(): the whole file, read into a heap buffer on first use.
*/
const c3_y*
u3_blob_data(u3_blob_hand* han_u)
{
  if ( han_u->buf_y ) {
    return han_u->buf_y;
  }

  //  the buffer is recorded on the hand before it is filled, so a signal
  //  during the read cannot leak it; nothing observes it before we return.
  //
  //  it is padded with zeros to a whole number of 64-bit words: readers
  //  such as u3r_chop_words walk a view a word at a time, which a
  //  page-granular mapping tolerated and an exact-size heap buffer would
  //  not.
  //
  c3_y* buf_y;
  c3_z  siz_z = ((c3_z)han_u->len_d + 7) & ~(c3_z)7;

  u3m_crit_enter();
  buf_y        = c3_malloc(siz_z);
  han_u->buf_y = buf_y;
  u3m_crit_leave();

  memset(buf_y + han_u->len_d, 0, siz_z - (c3_z)han_u->len_d);

  if ( han_u->len_d != u3_blob_read(han_u, 0, buf_y, (c3_z)han_u->len_d) ) {
    fprintf(stderr, "blob: data: %08" PRIx32 "/%08" PRIx32 ": short read\r\n",
            (c3_h)(han_u->bid_d >> 32), (c3_h)(han_u->bid_d & 0xFFFFFFFF));

    u3m_crit_enter();
    han_u->buf_y = 0;
    c3_free(buf_y);
    u3m_crit_leave();
    return 0;
  }

  return buf_y;
}

/* u3_blob_hand_met(): bit-length of the blob's content, cached on the hand.
**
**   Scans backward from the end of the file for the last nonzero byte,
**   then returns (pos * 8 + 8 - clz(byte)).  This matches u3r_met(0, atom).
**   Returns 0 if the content is all zero or the read fails.
*/
c3_d
u3_blob_hand_met(u3_blob_hand* han_u)
{
  if ( han_u->met_d ) {
    return han_u->met_d;
  }

  c3_y win_y[BLOB_WIN_MAX];
  c3_d pos_d = han_u->len_d;

  while ( pos_d ) {
    c3_z ask_z = ( pos_d < BLOB_WIN_MAX ) ? (c3_z)pos_d : BLOB_WIN_MAX;
    c3_d beg_d = pos_d - ask_z;

    if ( ask_z != u3_blob_read(han_u, beg_d, win_y, ask_z) ) {
      return 0;
    }

    for ( c3_z i_z = ask_z; i_z--; ) {
      if ( win_y[i_z] ) {
        //  __builtin_clz operates on unsigned int (32 bits); subtract 24
        //  to get the leading-zero count within just the low byte.
        //
        c3_y clz_y = (c3_y)(__builtin_clz((unsigned int)win_y[i_z]) - 24);
        han_u->met_d = (beg_d + i_z) * 8 + (c3_d)(8 - clz_y);
        return han_u->met_d;
      }
    }

    pos_d = beg_d;
  }

  return 0;
}

/* u3_blob_bob_dir(): write path to $pier/.urb/bob/ into [out_c].
*/
void
u3_blob_bob_dir(c3_c* out_c, const c3_c* pax_c)
{
  snprintf(out_c, 8192, "%s/.urb/bob", pax_c);
}

/* _blob_mug_dir(): write path to $pier/.urb/bob/<mug>/ into [out_c].
*/
static void
_blob_mug_dir(c3_c* out_c, const c3_c* pax_c, c3_h mug_h)
{
  snprintf(out_c, 8192, "%s/.urb/bob/%" PRIc3_h, pax_c, mug_h);
}

/* _blob_sync_dir(): fsync a mug bucket directory so a freshly-created
**   entry survives a crash before the next global sync.  No-op on
**   Windows, which cannot open a directory as a file descriptor.
*/
static void
_blob_sync_dir(const c3_c* pax_c, c3_h mug_h)
{
#ifndef U3_OS_windows
  c3_c dir_c[8192];
  _blob_mug_dir(dir_c, pax_c, mug_h);

  c3_i dir_i = open(dir_c, O_RDONLY);
  if ( -1 == dir_i ) {
    return;
  }
  c3_sync(dir_i);
  close(dir_i);
#endif
}

/* _blob_lock_path(): write path to $pier/.urb/bob/<mug>/lock into [out_c].
*/
static void
_blob_lock_path(c3_c* out_c, const c3_c* pax_c, c3_h mug_h)
{
  snprintf(out_c, 8192, "%s/.urb/bob/%" PRIc3_h "/lock", pax_c, mug_h);
}

/* u3_blob_path(): write filesystem path for a blob into [out_c].
*/
void
u3_blob_path(c3_c* out_c, const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  snprintf(out_c, 8192, "%s/.urb/bob/%" PRIc3_h "/%" PRIc3_h,
           pax_c, mug_h, seq_h);
}

/* u3_blob_stg_dir(): write path to $pier/.urb/bob/stg/ into [out_c].
*/
void
u3_blob_stg_dir(c3_c* out_c, const c3_c* pax_c)
{
  snprintf(out_c, 8192, "%s/.urb/bob/stg", pax_c);
}

/* _blob_lock_acquire(): acquire mug bucket lock, return next seq number.
**
** Creates the mug directory and lockfile if needed.
** Returns 0 on failure.
*/
static c3_h
_blob_lock_acquire(const c3_c* pax_c, c3_h mug_h)
{
  c3_c dir_c[8192];
  c3_c lck_c[8192];
  _blob_mug_dir(dir_c, pax_c, mug_h);
  _blob_lock_path(lck_c, pax_c, mug_h);

  //  create mug bucket directory if needed
  if ( 0 != c3_mkdir(dir_c, 0700) && EEXIST != errno ) {
    fprintf(stderr, "blob: failed to create bucket %s: %s\r\n",
            dir_c, strerror(errno));
    return 0;
  }

  //  open lockfile, creating if needed
  c3_i lok_i = c3_open(lck_c, O_RDWR | O_CREAT, 0600);
  if ( -1 == lok_i ) {
    fprintf(stderr, "blob: failed to open lock %s: %s\r\n",
            lck_c, strerror(errno));
    return 0;
  }

  //  exclusive advisory lock (blocking)
  //
#ifdef U3_OS_windows
  {
    HANDLE han_u = (HANDLE)_get_osfhandle(lok_i);
    OVERLAPPED olp_u = {0};
    if ( !LockFileEx(han_u, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &olp_u) ) {
      fprintf(stderr, "blob: failed to lock %s\r\n", lck_c);
      close(lok_i);
      return 0;
    }
  }
#else
  {
    struct flock flk_u = {
      .l_type   = F_WRLCK,
      .l_whence = SEEK_SET,
      .l_start  = 0,
      .l_len    = 0,
    };
    if ( -1 == fcntl(lok_i, F_SETLKW, &flk_u) ) {
      fprintf(stderr, "blob: failed to lock %s: %s\r\n",
              lck_c, strerror(errno));
      close(lok_i);
      return 0;
    }
  }
#endif

  //  read current next-seq (0 means empty/new file)
  c3_c buf_c[32] = {0};
  ssize_t red_i = read(lok_i, buf_c, sizeof(buf_c) - 1);
  c3_h nex_h = ( red_i > 0 ) ? (c3_h)strtoul(buf_c, 0, 10) : 1;
  if ( 0 == nex_h ) {
    nex_h = 1;
  }

  //  write incremented value back
  if ( -1 == lseek(lok_i, 0, SEEK_SET) ) {
    fprintf(stderr, "blob: lseek failed on %s: %s\r\n",
            lck_c, strerror(errno));
    close(lok_i);
    return 0;
  }
  if ( -1 == ftruncate(lok_i, 0) ) {
    fprintf(stderr, "blob: ftruncate failed on %s: %s\r\n",
            lck_c, strerror(errno));
    close(lok_i);
    return 0;
  }

  c3_c wri_c[32];
  snprintf(wri_c, sizeof(wri_c), "%" PRIc3_h, nex_h + 1);
  if ( -1 == write(lok_i, wri_c, strlen(wri_c)) ) {
    fprintf(stderr, "blob: failed to write lock %s: %s\r\n",
            lck_c, strerror(errno));
    close(lok_i);
    return 0;
  }

  //  fsync and close (releases lock)
  c3_sync(lok_i);
  close(lok_i);

  return nex_h;
}

/* _blob_dedup(): scan bucket for byte-equal content.
**
** Returns the sequence number of an existing equal blob, or 0 if none.
*/
static c3_h
_blob_dedup(const c3_c* pax_c, c3_h mug_h, c3_h max_h,
            const c3_y* dat_y, c3_d len_d)
{
  for ( c3_h seq_h = 1; seq_h < max_h; seq_h++ ) {
    c3_c fil_c[8192];
    u3_blob_path(fil_c, pax_c, mug_h, seq_h);

    struct stat st_u;
    if ( -1 == stat(fil_c, &st_u) ) {
      continue;
    }
    if ( (c3_d)st_u.st_size != len_d ) {
      continue;
    }

    c3_i fid_i = open(fil_c, O_RDONLY);
    if ( -1 == fid_i ) {
      continue;
    }

    c3_o eql_o = c3y;
    c3_d rem_d = len_d;
    const c3_y* ptr_y = dat_y;
    c3_y buf_y[4096];

    while ( rem_d > 0 ) {
      c3_d ask_d = ( rem_d < sizeof(buf_y) ) ? rem_d : sizeof(buf_y);
      ssize_t got_i = read(fid_i, buf_y, ask_d);
      if ( got_i <= 0 || (c3_d)got_i != ask_d ||
           0 != memcmp(ptr_y, buf_y, ask_d) )
      {
        eql_o = c3n;
        break;
      }
      ptr_y += ask_d;
      rem_d -= ask_d;
    }

    close(fid_i);
    if ( c3y == eql_o ) {
      return seq_h;
    }
  }
  return 0;
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

/* _blob_mug(): compute the 31-bit mug of blob content.
**
**   This is not merely a bucketing hash: u3i_blob() memoizes it as the
**   bob atom's mug_w, so it must equal the mug the same atom would get
**   in the loom (u3r_mug_words -> strip trailing zeros -> u3r_mug_bytes).
*/
static c3_h
_blob_mug(const c3_y* dat_y, c3_d len_d)
{
  return u3r_mug_bytes(dat_y, _blob_sig(dat_y, len_d));
}

/* u3_blob_save(): write bytes to blob store.
*/
c3_o
u3_blob_save(const c3_c* pax_c,
             const c3_y* dat_y,
             c3_d        len_d,
             c3_h*       mug_h,
             c3_h*       seq_h)
{
  //  store the atom's bytes, not the caller's buffer: a blob denotes an
  //  atom, and an atom has no trailing zeros.  this is what u3i_bytes()
  //  does for sub-threshold content, and keeping the two in agreement is
  //  what makes dedup (and hence bob-vs-bob u3r_sing) see "abc" and
  //  "abc\0" as the one atom they are.
  //
  len_d = _blob_sig(dat_y, len_d);

  //  content whose atom the loom would keep direct has no bob
  //  representation (see U3_BLOB_MIN); the caller must use the loom.
  //  all-zero content lands here too, denoting the atom 0.
  //
  if ( len_d < U3_BLOB_MIN ) {
    return c3n;
  }

  *mug_h = _blob_mug(dat_y, len_d);

  //  acquire lock and get next sequence number
  c3_h nex_h = _blob_lock_acquire(pax_c, *mug_h);
  if ( 0 == nex_h ) {
    return c3n;
  }

  //  check for duplicate before writing
  c3_h dup_h = _blob_dedup(pax_c, *mug_h, nex_h, dat_y, len_d);
  if ( 0 != dup_h ) {
    *seq_h = dup_h;
    //  we already incremented the lock counter, but that's harmless —
    //  nex_w slot will simply be skipped (sparse sequence numbers are fine)
    return c3y;
  }

  //  write blob file
  c3_c fil_c[8192];
  u3_blob_path(fil_c, pax_c, *mug_h, nex_h);

  c3_i fid_i = open(fil_c, O_WRONLY | O_CREAT | O_EXCL, BLOB_FILE_MODE);
  if ( -1 == fid_i ) {
    fprintf(stderr, "blob: failed to create %s: %s\r\n",
            fil_c, strerror(errno));
    return c3n;
  }

  c3_d rem_d = len_d;
  const c3_y* ptr_y = dat_y;
  while ( rem_d > 0 ) {
    size_t  ask_i = ( rem_d < BLOB_IO_MAX ) ? (size_t)rem_d : BLOB_IO_MAX;
    ssize_t wrt_i = write(fid_i, ptr_y, ask_i);
    if ( wrt_i <= 0 ) {
      fprintf(stderr, "blob: write failed on %s: %s\r\n",
              fil_c, strerror(errno));
      close(fid_i);
      unlink(fil_c);
      return c3n;
    }
    ptr_y += wrt_i;
    rem_d -= wrt_i;
  }

  c3_sync(fid_i);
  close(fid_i);

  *seq_h = nex_h;
  return c3y;
}

/* u3_blob_save_fd(): write from open file descriptor into the blob store.
**
**   Uses mmap() to avoid a large malloc: the OS pages in only what
**   _blob_mug and the dedup scan actually touch, and can evict cold pages
**   immediately.  Works for files of any size that fit in the address space.
*/
c3_o
u3_blob_save_fd(const c3_c* pax_c,
                c3_i        fid_i,
                c3_d        len_d,
                c3_h*       mug_h,
                c3_h*       seq_h)
{
  if ( 0 == len_d ) {
    fprintf(stderr, "blob: refusing to save empty file\r\n");
    return c3n;
  }

  void* map_v = mmap(0, (size_t)len_d, PROT_READ, MAP_PRIVATE, fid_i, 0);
  if ( MAP_FAILED == map_v ) {
    fprintf(stderr, "blob: mmap failed (%" PRIc3_d " bytes): %s\r\n",
            len_d, strerror(errno));
    return c3n;
  }
  madvise(map_v, (size_t)len_d, MADV_SEQUENTIAL);

  c3_o ret_o = u3_blob_save(pax_c, (const c3_y*)map_v, len_d, mug_h, seq_h);
  munmap(map_v, (size_t)len_d);
  return ret_o;
}

/* u3_blob_load(): read blob into a loom atom.
**
**   Reads straight from the fd into a u3i_slab, so blobs of any size
**   (including >4 GiB) cost no heap.  The slab allocation can bail; the
**   hand is then swept along with the road.
*/
u3_weak
u3_blob_load(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  u3_blob_hand* han_u = u3_blob_open(pax_c, mug_h, seq_h);
  if ( !han_u ) {
    return u3_none;
  }

  c3_d len_d = han_u->len_d;

  //  NB: use u3i_slab_init (not u3i_slab_bare) so the trailing bytes of
  //  the last loom word are zeroed when len_d isn't word-aligned.
  //  Otherwise u3r_met/u3r_word/etc. would read garbage from those bytes.
  //  Zeroing also touches every page, so they are mapped writable before
  //  pread() lands in them (a syscall cannot take the loom's fault handler).
  //
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, len_d);

  if ( len_d != u3_blob_read(han_u, 0, sab_u.buf_y, (c3_z)len_d) ) {
    fprintf(stderr, "blob: load: %08" PRIx32 "/%08" PRIx32 ": short read\r\n",
            mug_h, seq_h);
    u3_blob_close(han_u);
    u3i_slab_free(&sab_u);
    return u3_none;
  }

  u3_blob_close(han_u);
  return u3i_slab_mint_bytes(&sab_u);
}

/* u3_blob_exists(): check whether a blob file exists.
*/
c3_o
u3_blob_live(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  c3_c fil_c[8192];
  u3_blob_path(fil_c, pax_c, mug_h, seq_h);

  struct stat st_u;
  return ( 0 == stat(fil_c, &st_u) ) ? c3y : c3n;
}

/* u3_blob_wipe(): delete a blob file.
*/
void
u3_blob_wipe(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  c3_c fil_c[8192];
  u3_blob_path(fil_c, pax_c, mug_h, seq_h);

  //  a wipe is driven by the blob's refcount reaching zero, so no reader
  //  can still hold it open; an open hand here is an accounting bug.
  //  posix keeps the inode alive for the reader regardless; windows
  //  refuses the unlink, which the error below reports.
  //
  u3m_crit_enter();
  {
    _blob_hands_itr itr_u = vt_get(&_blob_han_u, _blob_bid(mug_h, seq_h));
    if ( !vt_is_end(itr_u) ) {
      fprintf(stderr, "blob: wipe: %08" PRIx32 "/%08" PRIx32 ": %" PRIc3_w
                      " live reference(s)\r\n",
              mug_h, seq_h, itr_u.data->val->ref_w);
    }
  }
  u3m_crit_leave();

  if ( 0 != unlink(fil_c) && ENOENT != errno ) {
    fprintf(stderr, "blob: failed to delete %s: %s\r\n",
            fil_c, strerror(errno));
  }

  //  attempt to clean up the mug bucket directory if it is now empty.
  //
  //    the lockfile is the only non-blob resident; we must remove it before
  //    rmdir can succeed.  the two-step unlink+rmdir is safe because vere is
  //    single-threaded and blob installs never interleave with GC:
  //
  //    - if another blob exists in the bucket, rmdir fails ENOTEMPTY — fine.
  //    - if a concurrent install races the window between unlink(lock) and
  //      rmdir, it recreates the lockfile, rmdir fails ENOTEMPTY — fine.
  //
  c3_c dir_c[8192];
  c3_c lck_c[8192];
  _blob_mug_dir(dir_c, pax_c, mug_h);
  _blob_lock_path(lck_c, pax_c, mug_h);

  //  first attempt: rmdir without touching lock (fast path for non-empty dirs)
  //
  if ( 0 == rmdir(dir_c) || ENOENT == errno ) {
    return;
  }

  //  dir is non-empty: remove lock file and retry rmdir
  //
  if ( 0 != unlink(lck_c) && ENOENT != errno ) {
    fprintf(stderr, "blob: failed to remove lock %s: %s\r\n",
            lck_c, strerror(errno));
    return;
  }

  if ( 0 != rmdir(dir_c) && ENOTEMPTY != errno && ENOENT != errno ) {
    fprintf(stderr, "blob: failed to remove bucket %s: %s\r\n",
            dir_c, strerror(errno));
  }
}

/* _blob_name_num(): parse a strictly-decimal dirent name into [out_h].
**
**   Rejects anything else: "stg", "lock", dotfiles, junk.
*/
static c3_o
_blob_name_num(const c3_c* nam_c, c3_h* out_h)
{
  c3_c* end_c;

  if ( !nam_c[0] ) {
    return c3n;
  }

  errno = 0;
  unsigned long val = strtoul(nam_c, &end_c, 10);

  if ( errno || *end_c || (val > 0xFFFFFFFFUL) ) {
    return c3n;
  }

  *out_h = (c3_h)val;
  return c3y;
}

/* u3_blob_walk(): enumerate every blob file in the store.
*/
void
u3_blob_walk(const c3_c* pax_c,
             void*       ptr_v,
             void      (*fun_f)(void*, c3_h, c3_h))
{
  c3_c bob_c[8192];
  u3_blob_bob_dir(bob_c, pax_c);

  DIR* bob_u = opendir(bob_c);
  if ( !bob_u ) {
    return;
  }

  struct dirent* mug_e;
  while ( (mug_e = readdir(bob_u)) ) {
    c3_h mug_h;
    if (  ('.' == mug_e->d_name[0])
       || (c3n == _blob_name_num(mug_e->d_name, &mug_h)) )
    {
      continue;
    }

    c3_c dir_c[8192];
    _blob_mug_dir(dir_c, pax_c, mug_h);

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

/* u3_blob_move_stg(): install a staging file into the blob store.
**
**   [stg_c] is the path to a temp file under $pier/.urb/bob/stg/.
**   Computes the mug of its content, checks for duplicates, then either
**   renames the staging file into bob/<mug>/<seq> (no dup) or unlinks it
**   (dup found).  On success sets *mug_h and *seq_h.
**
**   The staging file is always consumed (renamed or unlinked) on success.
**   On failure the staging file is left in place.
*/
c3_o
u3_blob_move_stg(const c3_c* pax_c,
                    const c3_c* stg_c,
                    c3_h*       mug_h,
                    c3_h*       seq_h)
{
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

  //  the atom's byte length: what lands in the store must be canonical
  //  (see u3_blob_save).  the file is trimmed to match further down.
  //
  len_d = _blob_sig((const c3_y*)map_v, map_d);

  if ( len_d < U3_BLOB_MIN ) {
    fprintf(stderr, "blob: install_stg: %s denotes a direct atom "
                    "(%" PRIc3_d " significant bytes)\r\n", stg_c, len_d);
    munmap(map_v, (size_t)map_d);
    close(fid_i);
    return c3n;
  }

  *mug_h = _blob_mug((const c3_y*)map_v, len_d);

  //  acquire mug-bucket lock and get next sequence number
  //
  c3_h nex_h = _blob_lock_acquire(pax_c, *mug_h);
  if ( 0 == nex_h ) {
    munmap(map_v, (size_t)map_d);
    close(fid_i);
    return c3n;
  }

  //  check for duplicate content
  //
  c3_h dup_h = _blob_dedup(pax_c, *mug_h, nex_h,
                            (const c3_y*)map_v, len_d);

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

  close(fid_i);

  //  rename staging file into final location
  //
  c3_c dst_c[8192];
  u3_blob_path(dst_c, pax_c, *mug_h, nex_h);

  if ( 0 != rename(stg_c, dst_c) ) {
    //  rename can fail cross-device; fall back to copy-and-unlink
    //
    c3_i src_i = open(stg_c, O_RDONLY);
    c3_i dst_i = open(dst_c, O_WRONLY | O_CREAT | O_EXCL, BLOB_FILE_MODE);
    if ( -1 == src_i || -1 == dst_i ) {
      fprintf(stderr, "blob: install_stg: rename+fallback failed on %s: %s\r\n",
              stg_c, strerror(errno));
      if ( -1 != src_i ) close(src_i);
      if ( -1 != dst_i ) { close(dst_i); unlink(dst_c); }
      return c3n;
    }

    //  copy in chunks
    //
    c3_y buf_y[65536];
    ssize_t red_i;
    while ( (red_i = read(src_i, buf_y, sizeof(buf_y))) > 0 ) {
      if ( write(dst_i, buf_y, (size_t)red_i) != red_i ) {
        fprintf(stderr, "blob: install_stg: copy write failed: %s\r\n",
                strerror(errno));
        close(src_i);
        close(dst_i);
        unlink(dst_c);
        return c3n;
      }
    }
    c3_sync(dst_i);
    close(src_i);
    close(dst_i);
    c3_unlink(stg_c);
  }

  //  fsync the bucket directory so the new entry survives a crash
  //  before the next global sync — a committed lease row must never
  //  reference a blob whose directory entry was lost
  //
  _blob_sync_dir(pax_c, *mug_h);

  *seq_h = nex_h;
  return c3y;
}

/* u3_blob_met(): compute bit-length of blob content without full materialization.
*/
c3_d
u3_blob_met(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  u3_blob_hand* han_u = u3_blob_open(pax_c, mug_h, seq_h);
  if ( !han_u ) {
    return 0;
  }

  c3_d met_d = u3_blob_hand_met(han_u);
  u3_blob_close(han_u);
  return met_d;
}

/* u3_blob_bsink: streaming byte sink for blob-aware cue.
**
**   receives a large atom's bytes in chunks, writes them to a staging
**   file, then installs the file into the blob store (mug, dedup,
**   rename via u3_blob_move_stg) and returns a bob atom.  the atom's
**   bytes never touch the loom.
*/

static c3_o
_blob_bsink_opn(void* ptr_v)
{
  u3_blob_bsink* bsk_u = ptr_v;

  c3_c stg_c[8192];
  u3_blob_stg_dir(stg_c, bsk_u->pax_c);
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
  u3_blob_bsink* bsk_u = ptr_v;

  while ( len_z ) {
    ssize_t ret_i = write(bsk_u->fid_i, byt_y, len_z);

    if ( ret_i < 0 ) {
      if ( (EINTR == errno) || (EAGAIN == errno) ) {
        continue;
      }
      fprintf(stderr, "blob: bsink: write failed (%s): %s\r\n",
              bsk_u->stg_c, strerror(errno));
      close(bsk_u->fid_i);
      c3_unlink(bsk_u->stg_c);
      bsk_u->fid_i = -1;
      return c3n;
    }

    byt_y += ret_i;
    len_z -= (c3_z)ret_i;
  }

  return c3y;
}

static u3_weak
_blob_bsink_don(void* ptr_v)
{
  u3_blob_bsink* bsk_u = ptr_v;
  c3_h mug_h = 0;
  c3_h seq_h = 0;

  if ( bsk_u->fid_i < 0 ) {
    return u3_none;
  }

  c3_sync(bsk_u->fid_i);
  close(bsk_u->fid_i);
  bsk_u->fid_i = -1;

  if ( c3n == u3_blob_move_stg(bsk_u->pax_c, bsk_u->stg_c,
                               &mug_h, &seq_h) )
  {
    fprintf(stderr, "blob: bsink: install failed (%s)\r\n", bsk_u->stg_c);
    c3_unlink(bsk_u->stg_c);
    return u3_none;
  }

  bsk_u->num_w += 1;

  return u3i_blob(mug_h, seq_h);
}

/* u3_blob_bsink_init(): prepare a sink targeting [pax_c]'s blob store.
*/
void
u3_blob_bsink_init(u3_blob_bsink* bsk_u, const c3_c* pax_c)
{
  memset(bsk_u, 0, sizeof(*bsk_u));

  bsk_u->pax_c       = pax_c;
  bsk_u->fid_i       = -1;
  bsk_u->snk_u.ptr_v = bsk_u;
  bsk_u->snk_u.opn_f = _blob_bsink_opn;
  bsk_u->snk_u.wri_f = _blob_bsink_wri;
  bsk_u->snk_u.don_f = _blob_bsink_don;
}
