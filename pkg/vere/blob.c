/// @file

#include "blob.h"
#include "vere.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

//  maximum bytes per single read()/write() call.
//  POSIX allows read()/write() to return EINVAL if count > SSIZE_MAX;
//  macOS returns EINVAL if count > INT_MAX.  cap conservatively at 1 GiB.
//
#define BLOB_IO_MAX  ((size_t)0x40000000UL)

/* _blob_bob_dir(): write path to $pier/.urb/bob/ into [out_c].
*/
static void
_blob_bob_dir(c3_c* out_c, const c3_c* pax_c)
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

/* u3_blob_init(): initialize blob store; create .urb/bob/ if needed.
*/
void
u3_blob_init(const c3_c* pax_c)
{
  c3_c bob_c[8192];
  _blob_bob_dir(bob_c, pax_c);

  if ( 0 != c3_mkdir(bob_c, 0700) && EEXIST != errno ) {
    fprintf(stderr, "blob: failed to create %s: %s\r\n",
            bob_c, strerror(errno));
  }
}

/* _blob_stg_dir(): write path to $pier/.urb/bob/stg/ into [out_c].
*/
static void
_blob_stg_dir(c3_c* out_c, const c3_c* pax_c)
{
  snprintf(out_c, 8192, "%s/.urb/bob/stg", pax_c);
}

/* _blob_stg_rm_rf(): recursively delete all files inside staging dir.
**
** Only removes regular files, not subdirectories.  Staging should
** only ever contain flat temp files so this is sufficient.
*/
static void
_blob_stg_clean(const c3_c* stg_c)
{
  DIR* dir_u = opendir(stg_c);
  if ( !dir_u ) {
    return;
  }
  struct dirent* ent_u;
  while ( (ent_u = readdir(dir_u)) ) {
    if ( '.' == ent_u->d_name[0] ) {
      continue;
    }
    c3_c fil_c[8192];
    snprintf(fil_c, sizeof(fil_c), "%s/%s", stg_c, ent_u->d_name);
    c3_unlink(fil_c);
  }
  closedir(dir_u);
}

/* u3_blob_stg_init(): initialize staging area; create/clean .urb/bob/stg/.
*/
void
u3_blob_stg_init(const c3_c* pax_c)
{
  c3_c stg_c[8192];
  _blob_stg_dir(stg_c, pax_c);

  if ( 0 != c3_mkdir(stg_c, 0700) && EEXIST != errno ) {
    fprintf(stderr, "blob: failed to create staging dir %s: %s\r\n",
            stg_c, strerror(errno));
    return;
  }

  //  clean any leftover temp files from a prior crash
  //
  _blob_stg_clean(stg_c);
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

/* _blob_mug(): compute a 31-bit mug suitable for blob bucketing.
**
**   u3r_mug_bytes takes c3_h (uint32_t) for the length, so we cap each
**   window at 0xFFFFFFFF bytes.  For files > 4 GiB we hash the first
**   window, mix in the high bits of length, and mix in the last window
**   so that files of identical size but different tail content get
**   distinct buckets.
*/
static c3_h
_blob_mug(const c3_y* dat_y, c3_d len_d)
{
  //  cap first window to UINT32_MAX to avoid hashing 0 bytes
  //
  c3_h win_h = ( len_d > 0xFFFFFFFFULL ) ? 0xFFFFFFFFu : (c3_h)len_d;
  c3_h mug_h = u3r_mug_bytes(dat_y, win_h);

  //  for files larger than 4 GiB, also fold in the high length bits
  //  and hash the last 4 GiB window for tail sensitivity
  //
  c3_h hig_h = (c3_h)(len_d >> 32);
  if ( hig_h ) {
    mug_h = u3r_mug_both(mug_h, hig_h);
    //  hash the final window (last min(len_d, 0xFFFFFFFF) bytes)
    c3_d off_d  = len_d > 0xFFFFFFFFULL ? len_d - 0xFFFFFFFFULL : 0;
    c3_h tel_h = u3r_mug_bytes(dat_y + off_d, (c3_h)(len_d - off_d));
    mug_h = u3r_mug_both(mug_h, tel_h);
  }
  return mug_h;
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

  c3_i fid_i = open(fil_c, O_WRONLY | O_CREAT | O_EXCL, 0400);
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
**   Uses mmap() and u3i_slab to handle blobs of any size, including >4 GiB.
**   The mapping is released immediately after the loom copy.
*/
u3_weak
u3_blob_load(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  c3_c fil_c[8192];
  u3_blob_path(fil_c, pax_c, mug_h, seq_h);

  struct stat st_u;
  if ( -1 == stat(fil_c, &st_u) ) {
    fprintf(stderr, "blob: missing blob %" PRIc3_h "/%" PRIc3_h ": %s\r\n",
            mug_h, seq_h, strerror(errno));
    return u3_none;
  }

  c3_d len_d = (c3_d)st_u.st_size;
  c3_i fid_i = open(fil_c, O_RDONLY);
  if ( -1 == fid_i ) {
    fprintf(stderr, "blob: failed to open %s: %s\r\n",
            fil_c, strerror(errno));
    return u3_none;
  }

  void* map_v = mmap(0, (size_t)len_d, PROT_READ, MAP_PRIVATE, fid_i, 0);
  close(fid_i);

  if ( MAP_FAILED == map_v ) {
    fprintf(stderr, "blob: mmap failed on %s: %s\r\n",
            fil_c, strerror(errno));
    return u3_none;
  }
  madvise(map_v, (size_t)len_d, MADV_SEQUENTIAL);

  //  use u3i_slab (c3_d length) to correctly handle blobs >4 GiB.
  //  bloq 3 = bytes; len_d = byte count.
  //
  //  NB: use u3i_slab_init (not u3i_slab_bare) so the trailing bytes of
  //  the last loom word are zeroed when len_d isn't word-aligned.
  //  Otherwise u3r_met/u3r_word/etc. would read garbage from those bytes.
  //
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, len_d);
  memcpy(sab_u.buf_y, map_v, (size_t)len_d);
  munmap(map_v, (size_t)len_d);

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
  _blob_bob_dir(bob_c, pax_c);

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

  c3_d len_d = (c3_d)st_u.st_size;

  if ( 0 == len_d ) {
    fprintf(stderr, "blob: install_stg: refusing empty staging file %s\r\n",
            stg_c);
    return c3n;
  }

  c3_i fid_i = open(stg_c, O_RDONLY);
  if ( -1 == fid_i ) {
    fprintf(stderr, "blob: install_stg: open failed on %s: %s\r\n",
            stg_c, strerror(errno));
    return c3n;
  }

  void* map_v = mmap(0, (size_t)len_d, PROT_READ, MAP_PRIVATE, fid_i, 0);
  close(fid_i);

  if ( MAP_FAILED == map_v ) {
    fprintf(stderr, "blob: install_stg: mmap failed on %s: %s\r\n",
            stg_c, strerror(errno));
    return c3n;
  }
  madvise(map_v, (size_t)len_d, MADV_SEQUENTIAL);

  *mug_h = _blob_mug((const c3_y*)map_v, len_d);

  //  acquire mug-bucket lock and get next sequence number
  //
  c3_h nex_h = _blob_lock_acquire(pax_c, *mug_h);
  if ( 0 == nex_h ) {
    munmap(map_v, (size_t)len_d);
    return c3n;
  }

  //  check for duplicate content
  //
  c3_h dup_h = _blob_dedup(pax_c, *mug_h, nex_h,
                            (const c3_y*)map_v, len_d);
  munmap(map_v, (size_t)len_d);

  if ( 0 != dup_h ) {
    //  duplicate found — consume staging file and return existing seq
    //
    c3_unlink(stg_c);
    *seq_h = dup_h;
    return c3y;
  }

  //  rename staging file into final location
  //
  c3_c dst_c[8192];
  u3_blob_path(dst_c, pax_c, *mug_h, nex_h);

  if ( 0 != rename(stg_c, dst_c) ) {
    //  rename can fail cross-device; fall back to copy-and-unlink
    //
    c3_i src_i = open(stg_c, O_RDONLY);
    c3_i dst_i = open(dst_c, O_WRONLY | O_CREAT | O_EXCL, 0400);
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

  *seq_h = nex_h;
  return c3y;
}

/* u3_blob_map(): mmap blob file for direct byte access.
*/
const c3_y*
u3_blob_mmap(const c3_c* pax_c, c3_h mug_h, c3_h seq_h, c3_d* len_d)
{
  c3_c fil_c[8192];
  u3_blob_path(fil_c, pax_c, mug_h, seq_h);

  struct stat st_u;
  if ( -1 == stat(fil_c, &st_u) ) {
    fprintf(stderr, "blob: map: stat failed %s: %s\r\n",
            fil_c, strerror(errno));
    return 0;
  }

  *len_d = (c3_d)st_u.st_size;
  if ( 0 == *len_d ) {
    return 0;
  }

  c3_i fid_i = open(fil_c, O_RDONLY);
  if ( -1 == fid_i ) {
    fprintf(stderr, "blob: map: open failed %s: %s\r\n",
            fil_c, strerror(errno));
    return 0;
  }

  void* map_v = mmap(0, (size_t)*len_d, PROT_READ, MAP_PRIVATE, fid_i, 0);
  close(fid_i);

  if ( MAP_FAILED == map_v ) {
    fprintf(stderr, "blob: map: mmap failed %s: %s\r\n",
            fil_c, strerror(errno));
    return 0;
  }

  return (const c3_y*)map_v;
}

/* u3_blob_unmap(): release mapping returned by u3_blob_map().
*/
void
u3_blob_umap(const c3_y* ptr_y, c3_d len_d)
{
  if ( ptr_y && len_d ) {
    munmap((void*)ptr_y, (size_t)len_d);
  }
}

/* u3_blob_met(): compute bit-length of blob content without full materialization.
**
**   Scans backward from end of file to find last non-zero byte, then
**   returns (pos * 8 + 8 - clz(byte)).  This matches u3r_met(0, atom).
**   Returns 0 if blob is missing, empty, or all-zero bytes.
*/
c3_d
u3_blob_met(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  c3_c fil_c[8192];
  u3_blob_path(fil_c, pax_c, mug_h, seq_h);

  struct stat st_u;
  if ( -1 == stat(fil_c, &st_u) || 0 == st_u.st_size ) {
    return 0;
  }

  c3_d len_d = (c3_d)st_u.st_size;
  c3_i fid_i = open(fil_c, O_RDONLY);
  if ( -1 == fid_i ) {
    return 0;
  }

  //  mmap and scan backward for last non-zero byte (strips trailing zeroes)
  //
  void* map_v = mmap(0, (size_t)len_d, PROT_READ, MAP_PRIVATE, fid_i, 0);
  close(fid_i);
  if ( MAP_FAILED == map_v ) {
    return 0;
  }

  const c3_y* byt_y = (const c3_y*)map_v;
  c3_d        pos_d = len_d;

  while ( pos_d > 0 && 0 == byt_y[pos_d - 1] ) {
    pos_d--;
  }

  c3_d met_d = 0;
  if ( pos_d > 0 ) {
    c3_y top_y = byt_y[pos_d - 1];
    //  bit count = (pos_d - 1) * 8 + (8 - count_of_leading_zeros_in_top_y)
    //  __builtin_clz operates on unsigned int (32 bits); subtract 24 to get
    //  the leading-zero count within just the low byte.
    //
    c3_y clz_y = (c3_y)(__builtin_clz((unsigned int)top_y) - 24);
    met_d = (pos_d - 1) * 8 + (c3_d)(8 - clz_y);
  }

  munmap(map_v, (size_t)len_d);
  return met_d;
}