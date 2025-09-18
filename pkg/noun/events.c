//! @file events.c
//!
//! incremental, orthogonal, paginated loom snapshots
//!
//! ### components
//!
//!   - page: 16KB chunk of the loom.
//!   - image (u3e_image, image.bin): low contiguous loom pages,
//!     (in practice, the home road heap). indexed from low to high:
//!     in-order on disk. in a file-backed mapping by default.
//!   - patch memory (memory.bin): new or changed pages since the last snapshot
//!   - patch control (u3e_control control.bin): patch metadata, watermarks,
//!     and indices/checksums for pages in patch memory.
//!
//! ### initialization (u3e_live())
//!
//!   - with the loom already mapped, all pages are marked dirty in a bitmap.
//!   - if snapshot is missing or partial, empty segments are created.
//!   - if a patch is present, it's applied (crash recovery).
//!   - snapshot segments are mapped or copied onto the loom;
//!     all included pages are marked clean and protected (read-only).
//!
//! #### page faults (u3e_fault())
//!
//!   - stores into protected pages generate faults (currently SIGSEGV,
//!     handled outside this module).
//!   - faults are handled by dirtying the page and switching protections to
//!     read/write.
//!   - a guard page is initially placed in the approximate middle of the free
//!     space between the heap and stack at the time of the first page fault.
//!     when a fault is detected in the guard page, the guard page is recentered
//!     in the free space of the current road. if the guard page cannot be
//!     recentered, then memory exhaustion has occurred.
//!
//! ### updates (u3e_save())
//!
//!   - all updates to a snapshot are made through a patch.
//!   - high/low watermarks are established,
//!     and dirty pages below the low mark are added to the patch.
//!     - modifications have been caught by the fault handler.
//!     - newly-used pages are automatically included (preemptively dirtied).
//!     - unused, innermost pages are reclaimed (segments are truncated to the
//!       high/low watermarks; the last page in each is always adjacent to the
//!       contiguous free space).
//!   - patch pages are written to memory.bin, metadata to control.bin.
//!   - the patch is applied to the snapshot segments, in-place.
//!   - segments are fsync'd; patch files are deleted.
//!   - memory protections (and file-backed mappings) are re-established.
//!
//! ### invariants
//!
//!  definitions:
//!    - a clean page is PROT_READ and 0 in the bitmap
//!    - a dirty page is (PROT_READ|PROT_WRITE) and 1 in the bitmap
//!    - the guard page is PROT_NONE and 1 in the bitmap
//!
//!  assumptions:
//!    - all memory access patterns are outside-in, a page at a time
//!      - ad-hoc exceptions are supported by calling u3e_ward()
//!
//!  - there is a single guard page, between the segments
//!  - dirty pages only become clean by being:
//!    - loaded from a snapshot during initialization
//!    - present in a snapshot after save
//!  - clean pages only become dirty by being:
//!    - modified (and caught by the fault handler)
//!    - orphaned due to segment truncation (explicitly dirtied)
//!  - at points of quiescence (initialization, after save)
//!    - all pages of the image are clean
//!    - all other pages are dirty
//!
//! ### limitations
//!
//!   - loom page size is fixed (16 KB), and must be a multiple of the
//!     system page size.
//!   - update atomicity is crucial:
//!     - patch application must either completely succeed or
//!       leave on-disk segments (memory image) intact.
//!     - unapplied patches can be discarded (triggering event replay),
//!       but once patch application begins it must succeed.
//!     - may require integration into the overall signal-handling regime.
//!   - any errors are handled with assertions; error messages are poor;
//!     failed/partial writes are not retried.
//!
//! ### enhancements
//!
//!   - use platform specific page fault mechanism (mach rpc, userfaultfd, &c).
//!   - parallelism (conflicts with demand paging)
//!

#include "events.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stddef.h>

#include "log.h"
#include "murmur3.h"
#include "options.h"

/* _ce_len:       byte length of pages
** _ce_len_words: word length of pages
** _ce_page:      byte length of a single page
** _ce_ptr:       void pointer to a page
*/
#define _ce_len(i)        ((size_t)(i) << (u3a_page + 2))
#define _ce_len_words(i)  ((size_t)(i) << u3a_page)
#define _ce_page          _ce_len(1)
#define _ce_ptr(i)        ((void *)((c3_c*)u3_Loom + _ce_len(i)))

/// Snapshotting system.
u3e_pool u3e_Pool;

static c3_w
_ce_muk_buf(c3_w len_w, void* ptr_v)
{
  c3_w haz_w;
  MurmurHash3_x86_32(ptr_v, len_w, 0xcafebabeU, &haz_w);
  return haz_w;
}

static c3_w
_ce_muk_page(void* ptr_v)
{
  return _ce_muk_buf(_ce_page, ptr_v);
}

/* _ce_flaw_mmap(): remap non-guard page after fault.
*/
static inline c3_i
_ce_flaw_mmap(c3_w pag_w)
{
  // NB: must be static, since the stack is grown via page faults, and
  // we're already in a page fault handler.
  //
  static c3_y con_y[_ce_page];

  // save contents of page, to be restored after the mmap
  //
  memcpy(con_y, _ce_ptr(pag_w), _ce_page);

  // map the dirty page into the ephemeral file
  //
  if ( MAP_FAILED == mmap(_ce_ptr(pag_w),
                          _ce_page,
                          (PROT_READ | PROT_WRITE),
                          (MAP_FIXED | MAP_SHARED),
                          u3P.eph_i, _ce_len(pag_w)) )
  {
    fprintf(stderr, "loom: fault mmap failed (%u): %s\r\n",
                     pag_w, strerror(errno));
    return 1;
  }

  // restore contents of page
  //
  memcpy(_ce_ptr(pag_w), con_y, _ce_page);

  return 0;
}

/* _ce_flaw_mprotect(): protect page after fault.
*/
static inline c3_i
_ce_flaw_mprotect(c3_w pag_w)
{
  if ( 0 != mprotect(_ce_ptr(pag_w), _ce_page, (PROT_READ | PROT_WRITE)) ) {
    fprintf(stderr, "loom: fault mprotect (%u): %s\r\n",
                     pag_w, strerror(errno));
    return 1;
  }

  return 0;
}

#ifdef U3_GUARD_PAGE
/* _ce_ward_protect(): protect the guard page.
*/
static inline c3_i
_ce_ward_protect(void)
{
  if ( 0 != mprotect(_ce_ptr(u3P.gar_w), _ce_page, PROT_NONE) ) {
    fprintf(stderr, "loom: failed to protect guard page (%u): %s\r\n",
                    u3P.gar_w, strerror(errno));
    return 1;
  }

  return 0;
}

/* _ce_ward_post(): set the guard page.
*/
static inline c3_i
_ce_ward_post(c3_w nop_w, c3_w sop_w)
{
  u3P.gar_w = nop_w + ((sop_w - nop_w) / 2);
  return _ce_ward_protect();
}

/* _ce_ward_clip(): hit the guard page.
*/
static inline u3e_flaw
_ce_ward_clip(c3_w nop_w, c3_w sop_w)
{
  c3_w old_w = u3P.gar_w;

  if ( !u3P.gar_w || ((nop_w < u3P.gar_w) && (sop_w > u3P.gar_w)) ) {
    fprintf(stderr, "loom: ward bogus (>%u %u %u<)\r\n",
                    nop_w, u3P.gar_w, sop_w);
    return u3e_flaw_sham;
  }

  if ( sop_w <= (nop_w + 1) ) {
    return u3e_flaw_meme;
  }

  if ( _ce_ward_post(nop_w, sop_w) ) {
    return u3e_flaw_base;
  }

  u3_assert( old_w != u3P.gar_w );

  return u3e_flaw_good;
}
#endif /* ifdef U3_GUARD_PAGE */

/* u3e_fault(): handle a memory fault.
*/
u3e_flaw
u3e_fault(u3_post low_p, u3_post hig_p, u3_post off_p)
{
  c3_w pag_w = off_p >> u3a_page;
  c3_w blk_w = pag_w >> 5;
  c3_w bit_w = pag_w & 31;

#ifdef U3_GUARD_PAGE
  c3_w gar_w = u3P.gar_w;

  if ( pag_w == gar_w ) {
    u3e_flaw fal_e = _ce_ward_clip(low_p >> u3a_page, hig_p >> u3a_page);

    if ( u3e_flaw_good != fal_e ) {
      return fal_e;
    }

    if ( !(u3P.dit_w[blk_w] & ((c3_w)1 << bit_w)) ) {
      fprintf(stderr, "loom: strange guard (%d)\r\n", pag_w);
      return u3e_flaw_sham;
    }

    if ( _ce_flaw_mprotect(pag_w) ) {
      return u3e_flaw_base;
    }

    return u3e_flaw_good;
  }
#endif

  if ( u3P.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
    fprintf(stderr, "loom: strange page (%d): %x\r\n", pag_w, off_p);
    return u3e_flaw_sham;
  }

  u3P.dit_w[blk_w] |= ((c3_w)1 << bit_w);

  if ( u3P.eph_i ) {
    if ( _ce_flaw_mmap(pag_w) ) {
      return u3e_flaw_base;
    }
  }
  else if ( _ce_flaw_mprotect(pag_w) ) {
    return u3e_flaw_base;
  }

  return u3e_flaw_good;
}

typedef enum {
  _ce_img_good = 0,
  _ce_img_fail = 1,
  _ce_img_size = 2
} _ce_img_stat;

/* _ce_image_stat(): measure image.
*/
static _ce_img_stat
_ce_image_stat(u3e_image* img_u, c3_w* pgs_w)
{
  struct stat buf_u;

  if ( -1 == fstat(img_u->fid_i, &buf_u) ) {
    fprintf(stderr, "loom: image stat: %s\r\n", strerror(errno));
    u3_assert(0);
    return _ce_img_fail;
  }
  else {
    c3_z siz_z = buf_u.st_size;
    c3_z pgs_z = (siz_z + (_ce_page - 1)) >> (u3a_page + 2);

    if ( !siz_z ) {
      *pgs_w = 0;
      return _ce_img_good;
    }
    else if ( siz_z != _ce_len(pgs_z) ) {
      fprintf(stderr, "loom: image corrupt size %zu\r\n", siz_z);
      return _ce_img_size;
    }
    else if ( pgs_z > UINT32_MAX ) {
      fprintf(stderr, "loom: image overflow %zu\r\n", siz_z);
      return _ce_img_fail;
    }
    else {
      *pgs_w = (c3_w)pgs_z;
      return _ce_img_good;
    }
  }
}

/* _ce_ephemeral_open(): open or create ephemeral file
*/
static c3_o
_ce_ephemeral_open(c3_i* eph_i)
{
  c3_i mod_i = O_RDWR | O_CREAT;
  c3_c ful_c[8193];

  if ( u3C.eph_c == 0 ) {
    snprintf(ful_c, 8192, "%s", u3P.dir_c);
    c3_mkdir(ful_c, 0700);

    snprintf(ful_c, 8192, "%s/.urb", u3P.dir_c);
    c3_mkdir(ful_c, 0700);

    snprintf(ful_c, 8192, "%s/.urb/chk", u3P.dir_c);
    c3_mkdir(ful_c, 0700);

    snprintf(ful_c, 8192, "%s/.urb/chk/limbo.bin", u3P.dir_c);
    u3C.eph_c = strdup(ful_c);
  }

  if ( -1 == (*eph_i = c3_open(u3C.eph_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: ephemeral c3_open %s: %s\r\n", u3C.eph_c,
            strerror(errno));
    return c3n;
  }

  if ( ftruncate(*eph_i, _ce_len(u3P.pag_w)) < 0 ) {
    fprintf(stderr, "loom: ephemeral ftruncate %s: %s\r\n", u3C.eph_c,
            strerror(errno));
    return c3n;
  }
  return c3y;
}

/* _ce_image_open(): open or create image.
*/
static _ce_img_stat
_ce_image_open(u3e_image* img_u, c3_c* ful_c)
{
  c3_i mod_i = O_RDWR | O_CREAT;

  c3_c pax_c[8192];
  snprintf(pax_c, 8192, "%s/%s.bin", ful_c, img_u->nam_c);
  if ( -1 == (img_u->fid_i = c3_open(pax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", pax_c, strerror(errno));
    return _ce_img_fail;
  }

  return _ce_image_stat(img_u, &img_u->pgs_w);
}

c3_i
u3e_image_open_any(c3_c* nam_c, c3_c* dir_c, c3_z* len_z)
{
  u3e_image img_u = { .nam_c = nam_c };

  switch ( _ce_image_open(&img_u, dir_c) ) {
    case _ce_img_good: {
      *len_z = _ce_len(img_u.pgs_w);
      return img_u.fid_i;
    } break;

    case _ce_img_fail:
    case _ce_img_size: {
      *len_z = 0;
      return -1;
    } break;
  }
}

/* _ce_patch_write_control(): write control block file.
*/
static void
_ce_patch_write_control(u3_ce_patch* pat_u)
{
  ssize_t ret_i;
  c3_w    len_w = sizeof(u3e_control) +
                  (pat_u->con_u->pgs_w * sizeof(u3e_line));

  if ( len_w != (ret_i = write(pat_u->ctl_i, pat_u->con_u, len_w)) ) {
    if ( 0 < ret_i ) {
      fprintf(stderr, "loom: patch ctl partial write: %zu\r\n", (size_t)ret_i);
    }
    else {
      fprintf(stderr, "loom: patch ctl write: %s\r\n", strerror(errno));
    }
    u3_assert(0);
  }
}

/* _ce_patch_read_control(): read control block file.
*/
static c3_o
_ce_patch_read_control(u3_ce_patch* pat_u)
{
  c3_w len_w;

  u3_assert(0 == pat_u->con_u);
  {
    struct stat buf_u;

    if ( -1 == fstat(pat_u->ctl_i, &buf_u) ) {
      u3_assert(0);
      return c3n;
    }
    len_w = (c3_w) buf_u.st_size;
  }

  if (0 == len_w) {
    return c3n;
  }
  
  pat_u->con_u = c3_malloc(len_w);
  if ( (len_w != read(pat_u->ctl_i, pat_u->con_u, len_w)) ||
        (len_w != sizeof(u3e_control) +
                  (pat_u->con_u->pgs_w * sizeof(u3e_line))) )
  {
    c3_free(pat_u->con_u);
    pat_u->con_u = 0;
    return c3n;
  }
  return c3y;
}

/* _ce_patch_create(): create patch files.
*/
static void
_ce_patch_create(u3_ce_patch* pat_u)
{
  c3_c ful_c[8193];

  snprintf(ful_c, 8192, "%s", u3P.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb", u3P.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb/chk/control.bin", u3P.dir_c);
  if ( -1 == (pat_u->ctl_i = c3_open(ful_c, O_RDWR | O_CREAT | O_EXCL, 0600)) ) {
    fprintf(stderr, "loom: patch c3_open control.bin: %s\r\n", strerror(errno));
    u3_assert(0);
  }

  snprintf(ful_c, 8192, "%s/.urb/chk/memory.bin", u3P.dir_c);
  if ( -1 == (pat_u->mem_i = c3_open(ful_c, O_RDWR | O_CREAT | O_EXCL, 0600)) ) {
    fprintf(stderr, "loom: patch c3_open memory.bin: %s\r\n", strerror(errno));
    u3_assert(0);
  }
}

/* _ce_patch_delete(): delete a patch.
*/
static void
_ce_patch_delete(void)
{
  c3_c ful_c[8193];

  snprintf(ful_c, 8192, "%s/.urb/chk/control.bin", u3P.dir_c);
  if ( unlink(ful_c) ) {
    fprintf(stderr, "loom: failed to delete control.bin: %s\r\n",
                    strerror(errno));
  }

  snprintf(ful_c, 8192, "%s/.urb/chk/memory.bin", u3P.dir_c);
  if ( unlink(ful_c) ) {
    fprintf(stderr, "loom: failed to remove memory.bin: %s\r\n",
                    strerror(errno));
  }
}

/* _ce_patch_verify(): check patch data checksum.
*/
static c3_o
_ce_patch_verify(u3_ce_patch* pat_u)
{
  c3_w  pag_w, has_w;
  c3_y  buf_y[_ce_page];
  c3_zs ret_zs;

  if ( U3P_VERLAT != pat_u->con_u->ver_w ) {
    fprintf(stderr, "loom: patch version mismatch: have %"PRIc3_w", need %u\r\n",
                    pat_u->con_u->ver_w,
                    U3P_VERLAT);
    return c3n;
  }

  {
    c3_w  len_w = sizeof(u3e_control) + (pat_u->con_u->pgs_w * sizeof(u3e_line));
    c3_w  off_w = offsetof(u3e_control, tot_w);
    c3_y *ptr_y = (c3_y*)pat_u->con_u + off_w;
    c3_w  has_w = _ce_muk_buf(len_w - off_w, ptr_y);

    if ( has_w != pat_u->con_u->has_w ) {
      fprintf(stderr, "loom: patch meta checksum fail: "
                      "have=0x%"PRIxc3_w", need=0x%"PRIxc3_w"\r\n",
                      has_w, pat_u->con_u->has_w);
      return c3n;
    }
  }

  //  XX check for sorted page numbers?
  //
  for ( c3_z i_z = 0; i_z < pat_u->con_u->pgs_w; i_z++ ) {
    pag_w = pat_u->con_u->mem_u[i_z].pag_w;
    has_w = pat_u->con_u->mem_u[i_z].has_w;

    if ( _ce_page !=
         (ret_zs = pread(pat_u->mem_i, buf_y, _ce_page, _ce_len(i_z))) )
    {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: patch partial read: %"PRIc3_zs"\r\n", ret_zs);
      }
      else {
        fprintf(stderr, "loom: patch read: fail %s\r\n", strerror(errno));
      }
      return c3n;
    }

    {
      c3_w nas_w = _ce_muk_page(buf_y);

      if ( has_w != nas_w ) {
        fprintf(stderr, "loom: patch page (%"PRIc3_w") checksum fail: "
                        "have=0x%"PRIxc3_w", need=0x%"PRIxc3_w"\r\n",
                        pag_w, nas_w, has_w);
        return c3n;
      }
#if 0
      else {
        u3l_log("verify: patch %"PRIc3_w"/%"PRIc3_z", %"PRIxc3_w"\r\n", pag_w, i_z, has_w);
      }
#endif
    }
  }

  return c3y;
}

/* _ce_patch_free(): free a patch.
*/
static void
_ce_patch_free(u3_ce_patch* pat_u)
{
  c3_free(pat_u->con_u);
  close(pat_u->ctl_i);
  close(pat_u->mem_i);
  c3_free(pat_u);
}

/* _ce_patch_open(): open patch, if any.
*/
static u3_ce_patch*
_ce_patch_open(void)
{
  u3_ce_patch* pat_u;
  c3_c ful_c[8193];
  c3_i ctl_i, mem_i;

  snprintf(ful_c, 8192, "%s", u3P.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb", u3P.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb/chk/control.bin", u3P.dir_c);
  if ( -1 == (ctl_i = c3_open(ful_c, O_RDWR)) ) {
    return 0;
  }

  snprintf(ful_c, 8192, "%s/.urb/chk/memory.bin", u3P.dir_c);
  if ( -1 == (mem_i = c3_open(ful_c, O_RDWR)) ) {
    close(ctl_i);

    _ce_patch_delete();
    return 0;
  }
  pat_u = c3_malloc(sizeof(u3_ce_patch));
  pat_u->ctl_i = ctl_i;
  pat_u->mem_i = mem_i;
  pat_u->con_u = 0;

  if ( c3n == _ce_patch_read_control(pat_u) ) {
    close(pat_u->ctl_i);
    close(pat_u->mem_i);
    c3_free(pat_u);

    _ce_patch_delete();
    return 0;
  }
  if ( c3n == _ce_patch_verify(pat_u) ) {
    _ce_patch_free(pat_u);
    _ce_patch_delete();
    return 0;
  }
  return pat_u;
}

/* _ce_patch_write_page(): write a page of patch memory.
*/
static void
_ce_patch_write_page(u3_ce_patch* pat_u,
                     c3_w         pgc_w,
                     c3_w*        mem_w)
{
  c3_zs ret_zs;

  if ( _ce_page !=
       (ret_zs = pwrite(pat_u->mem_i, mem_w, _ce_page, _ce_len(pgc_w))) )
  {
    if ( 0 < ret_zs ) {
      fprintf(stderr, "loom: patch partial write: %"PRIc3_zs"\r\n", ret_zs);
    }
    else {
      fprintf(stderr, "loom: patch write: fail: %s\r\n", strerror(errno));
    }
    fprintf(stderr, "info: you probably have insufficient disk space");
    u3_assert(0);
  }
}

/* _ce_patch_count_page(): count a page, producing new counter.
*/
static c3_w
_ce_patch_count_page(c3_w pag_w,
                     c3_w off_w,
                     c3_w pgc_w)
{
  c3_w blk_w = (pag_w >> 5);
  c3_w bit_w = (pag_w & 31);

  if (  (u3P.dit_w[blk_w] & ((c3_w)1 << bit_w))
     && (  (pag_w < off_w)
     || (u3R->hep.len_w <= (pag_w - off_w))
     || (u3a_free_pg != (u3to(u3_post, u3R->hep.pag_p))[pag_w - off_w]) ) )
  {
    pgc_w += 1;
  }

  return pgc_w;
}

/* _ce_patch_save_page(): save a page, producing new page counter.
*/
static c3_w
_ce_patch_save_page(u3_ce_patch* pat_u,
                    c3_w         pag_w,
                    c3_w         off_w,
                    c3_w         pgc_w)
{
  c3_w  blk_w = (pag_w >> 5);
  c3_w  bit_w = (pag_w & 31);

  if ( u3P.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
    if (  (pag_w >= off_w)
       && (u3R->hep.len_w > (pag_w - off_w))
       && (u3a_free_pg == (u3to(u3_post, u3R->hep.pag_p))[pag_w - off_w]) )
    {
      // fprintf(stderr, "save: skip %u\r\n", pag_w);
      pat_u->sip_w++;
      return pgc_w;
    }

    c3_w* mem_w = _ce_ptr(pag_w);

    pat_u->con_u->mem_u[pgc_w].pag_w = pag_w;
    pat_u->con_u->mem_u[pgc_w].has_w = _ce_muk_page(mem_w);

#if 0
    fprintf(stderr, "loom: save page %d %x\r\n",
                    pag_w, pat_u->con_u->mem_u[pgc_w].has_w);
#endif
    _ce_patch_write_page(pat_u, pgc_w, mem_w);

    pgc_w += 1;
  }
  return pgc_w;
}

/* _ce_patch_compose(): make and write current patch.
*/
static u3_ce_patch*
_ce_patch_compose(c3_w max_w)
{
  c3_w pgs_w = 0;
  c3_w off_w = u3R->rut_p >> u3a_page;

  /* Count dirty pages.
  */
  {
    c3_w i_w;

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      pgs_w = _ce_patch_count_page(i_w, off_w, pgs_w);
    }
  }

  if ( !pgs_w ) {
    return 0;
  }
  else {
    u3_ce_patch* pat_u = c3_malloc(sizeof(u3_ce_patch));
    c3_w i_w, len_w, pgc_w;

    pat_u->sip_w = 0;

    _ce_patch_create(pat_u);
    len_w = sizeof(u3e_control) + (pgs_w * sizeof(u3e_line));
    pat_u->con_u = c3_malloc(len_w);
    pat_u->con_u->ver_w = U3P_VERLAT;
    pgc_w = 0;

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      pgc_w = _ce_patch_save_page(pat_u, i_w, off_w, pgc_w);
    }

    u3_assert( pgc_w == pgs_w );

    pat_u->con_u->tot_w = max_w;
    pat_u->con_u->pgs_w = pgc_w;

    {
      c3_w  off_w = offsetof(u3e_control, tot_w);
      c3_y *ptr_y = (c3_y*)pat_u->con_u + off_w;

      pat_u->con_u->has_w = _ce_muk_buf(len_w - off_w, ptr_y);
    }

    _ce_patch_write_control(pat_u);
    return pat_u;
  }
}

/* _ce_patch_sync(): make sure patch is synced to disk.
*/
static void
_ce_patch_sync(u3_ce_patch* pat_u)
{
  if ( -1 == c3_sync(pat_u->ctl_i) ) {
    fprintf(stderr, "loom: control file sync failed: %s\r\n",
                    strerror(errno));
    u3_assert(!"loom: control sync");
  }

  if ( -1 == c3_sync(pat_u->mem_i) ) {
    fprintf(stderr, "loom: patch file sync failed: %s\r\n",
                    strerror(errno));
    u3_assert(!"loom: patch sync");
  }
}

/* _ce_image_sync(): make sure image is synced to disk.
*/
static c3_o
_ce_image_sync(u3e_image* img_u)
{
  if ( -1 == c3_sync(img_u->fid_i) ) {
    fprintf(stderr, "loom: image sync failed: %s\r\n", strerror(errno));
    return c3n;
  }

  return c3y;
}

/* _ce_image_resize(): resize image, truncating if it shrunk.
*/
static void
_ce_image_resize(u3e_image* img_u, c3_w pgs_w)
{
  c3_z  off_z = _ce_len(pgs_w);
  off_t off_i = (off_t)off_z;

  if ( img_u->pgs_w > pgs_w ) {
    if ( off_z != (size_t)off_i ) {
      fprintf(stderr, "loom: image truncate: "
                      "offset overflow (%" PRId64 ") for page %u\r\n",
                      (c3_ds)off_i, pgs_w);
      u3_assert(0);
    }

    if ( ftruncate(img_u->fid_i, off_i) ) {
      fprintf(stderr, "loom: image truncate: %s\r\n", strerror(errno));
      u3_assert(0);
    }
  }

  img_u->pgs_w = pgs_w;
}

/* _ce_patch_apply(): apply patch to images.
*/
static void
_ce_patch_apply(u3_ce_patch* pat_u)
{
  c3_zs ret_zs;
  c3_w     i_w;

  //  resize images
  //
  _ce_image_resize(&u3P.img_u, pat_u->con_u->tot_w);

  //  seek to begining of patch
  //
  if ( -1 == lseek(pat_u->mem_i, 0, SEEK_SET) ) {
    fprintf(stderr, "loom: patch apply seek: %s\r\n", strerror(errno));
    u3_assert(0);
  }

  c3_i fid_i = u3P.img_u.fid_i;

  //  write patch pages into the appropriate image
  //
  for ( i_w = 0; i_w < pat_u->con_u->pgs_w; i_w++ ) {
    c3_w pag_w = pat_u->con_u->mem_u[i_w].pag_w;
    c3_y buf_y[_ce_page];
    c3_z off_z = _ce_len(pag_w);

    if ( _ce_page != (ret_zs = read(pat_u->mem_i, buf_y, _ce_page)) ) {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: patch apply partial read: %"PRIc3_zs"\r\n",
                        ret_zs);
      }
      else {
        fprintf(stderr, "loom: patch apply read: %s\r\n", strerror(errno));
      }
      u3_assert(0);
    }
    else {
      if ( _ce_page !=
           (ret_zs = pwrite(fid_i, buf_y, _ce_page, off_z)) )
      {
        if ( 0 < ret_zs ) {
          fprintf(stderr, "loom: patch apply partial write: %"PRIc3_zs"\r\n",
                          ret_zs);
        }
        else {
          fprintf(stderr, "loom: patch apply write: %s\r\n", strerror(errno));
        }
        fprintf(stderr, "info: you probably have insufficient disk space");
        u3_assert(0);
      }
    }
#if 0
    u3l_log("apply: %d, %x", pag_w, _ce_muk_page(buf_y));
#endif
  }
}

/* _ce_loom_track_sane(): quiescent page state invariants.
*/
static c3_o
_ce_loom_track_sane(void)
{
  c3_w blk_w, bit_w, max_w, i_w = 0;
  c3_o san_o = c3y;

  max_w = u3P.img_u.pgs_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;

    if ( u3P.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
      fprintf(stderr, "loom: insane image %u\r\n", i_w);
      san_o = c3n;
    }
  }

  max_w = u3P.pag_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;

    if ( !(u3P.dit_w[blk_w] & ((c3_w)1 << bit_w)) ) {
      fprintf(stderr, "loom: insane open %u\r\n", i_w);
      san_o = c3n;
    }
  }

  return san_o;
}

/* _ce_loom_track(): [pgs_w] clean, followed by [dif_w] dirty.
*/
void
_ce_loom_track(c3_w pgs_w, c3_w dif_w)
{
  c3_w blk_w, bit_w, i_w = 0, max_w = pgs_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;
    u3P.dit_w[blk_w] &= ~((c3_w)1 << bit_w);
  }

  max_w += dif_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;
    u3P.dit_w[blk_w] |= ((c3_w)1 << bit_w);
  }
}

/* _ce_loom_protect(): protect/track pages from the bottom of memory.
*/
static void
_ce_loom_protect(c3_w pgs_w, c3_w old_w)
{
  c3_w dif_w = 0;

  if ( pgs_w ) {
    if ( 0 != mprotect(_ce_ptr(0), _ce_len(pgs_w), PROT_READ) ) {
      fprintf(stderr, "loom: pure (%u pages): %s\r\n",
                      pgs_w, strerror(errno));
      u3_assert(0);
    }
  }

  if ( old_w > pgs_w ) {
    dif_w = old_w - pgs_w;

    if ( 0 != mprotect(_ce_ptr(pgs_w),
                       _ce_len(dif_w),
                       (PROT_READ | PROT_WRITE)) )
    {
      fprintf(stderr, "loom: foul (%u pages, %u old): %s\r\n",
                      pgs_w, old_w, strerror(errno));
      u3_assert(0);
    }

#ifdef U3_GUARD_PAGE
    //  protect guard page if clobbered
    //
    //    NB: < pgs_w is precluded by assertion in u3e_save()
    //
    if ( u3P.gar_w < old_w ) {
      fprintf(stderr, "loom: guard on reprotect\r\n");
      u3_assert( !_ce_ward_protect() );
    }
#endif
  }

  _ce_loom_track(pgs_w, dif_w);
}

/* _ce_loom_mapf_ephemeral(): map entire loom into ephemeral file
*/
static void
_ce_loom_mapf_ephemeral(void)
{
  if ( MAP_FAILED == mmap(_ce_ptr(0),
                          _ce_len(u3P.pag_w),
                          (PROT_READ | PROT_WRITE),
                          (MAP_FIXED | MAP_SHARED),
                          u3P.eph_i, 0) )
  {
    fprintf(stderr, "loom: initial ephemeral mmap failed (%u pages): %s\r\n",
                    u3P.pag_w, strerror(errno));
    u3_assert(0);
  }
}

/* _ce_loom_mapf(): map [pgs_w] of [fid_i] into the bottom of memory
**                        (and ephemeralize [old_w - pgs_w] after if needed).
*/
static void
_ce_loom_mapf(c3_i fid_i, c3_w pgs_w, c3_w old_w)
{
  c3_w dif_w = 0;

  if ( pgs_w ) {
    if ( MAP_FAILED == mmap(_ce_ptr(0),
                            _ce_len(pgs_w),
                            PROT_READ,
                            (MAP_FIXED | MAP_PRIVATE),
                            fid_i, 0) )
    {
      fprintf(stderr, "loom: file-backed mmap failed (%u pages): %s\r\n",
                      pgs_w, strerror(errno));
      u3_assert(0);
    }
  }

  if ( old_w > pgs_w ) {
    dif_w = old_w - pgs_w;

    if ( u3C.wag_w & u3o_swap ) {
      if ( MAP_FAILED == mmap(_ce_ptr(pgs_w),
                              _ce_len(dif_w),
                              (PROT_READ | PROT_WRITE),
                              (MAP_FIXED | MAP_SHARED),
                              u3P.eph_i, _ce_len(pgs_w)) )
      {
        fprintf(stderr, "loom: ephemeral mmap failed (%u pages, %u old): %s\r\n",
                        pgs_w, old_w, strerror(errno));
        u3_assert(0);
      }
    }
    else {
      if ( MAP_FAILED == mmap(_ce_ptr(pgs_w),
                              _ce_len(dif_w),
                              (PROT_READ | PROT_WRITE),
                              (MAP_ANON | MAP_FIXED | MAP_PRIVATE),
                              -1, 0) )
      {
        fprintf(stderr, "loom: anonymous mmap failed (%u pages, %u old): %s\r\n",
                        pgs_w, old_w, strerror(errno));
        u3_assert(0);
      }
    }

#ifdef U3_GUARD_PAGE
    //  protect guard page if clobbered
    //
    //    NB: < pgs_w is precluded by assertion in u3e_save()
    //
    if ( u3P.gar_w < old_w ) {
      fprintf(stderr, "loom: guard on remap\r\n");
      u3_assert( !_ce_ward_protect() );
    }
#endif
  }

  _ce_loom_track(pgs_w, dif_w);
}

/* _ce_loom_blit(): apply pages, in order, from the bottom of memory.
*/
static void
_ce_loom_blit(c3_i fid_i, c3_w pgs_w)
{
  c3_w    i_w;
  void* ptr_v;
  c3_zs ret_zs;

  for ( i_w = 0; i_w < pgs_w; i_w++ ) {
    ptr_v = _ce_ptr(i_w);

    if ( _ce_page != (ret_zs = pread(fid_i, ptr_v, _ce_page, _ce_len(i_w))) ) {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: blit partial read: %"PRIc3_zs"\r\n",
                        ret_zs);
      }
      else {
        fprintf(stderr, "loom: blit read %s\r\n", strerror(errno));
      }
      u3_assert(0);
    }
  }

  _ce_loom_protect(pgs_w, 0);
}

#ifdef U3_SNAPSHOT_VALIDATION
/* _ce_page_fine(): compare page in memory and on disk.
*/
static c3_o
_ce_page_fine(u3e_image* img_u, c3_w pag_w, c3_z off_z)
{
  ssize_t ret_i;
  c3_y    buf_y[_ce_page];

  if ( _ce_page !=
       (ret_i = pread(img_u->fid_i, buf_y, _ce_page, off_z)) )
  {
    if ( 0 < ret_i ) {
      fprintf(stderr, "loom: image fine partial read: %zu\r\n", (size_t)ret_i);
    }
    else {
      fprintf(stderr, "loom: image fine read: %s\r\n", strerror(errno));
    }
    u3_assert(0);
  }

  {
    c3_w mas_w = _ce_muk_page(_ce_ptr(pag_w));
    c3_w fas_w = _ce_muk_page(buf_y);

    if ( mas_w != fas_w ) {
      fprintf(stderr, "loom: image checksum mismatch: "
                      "page %d, mem_w %x, fil_w %x\r\n",
                      pag_w, mas_w, fas_w);
      return c3n;
    }
  }

  return c3y;
}

/* _ce_loom_fine(): compare clean pages in memory and on disk.
*/
static c3_o
_ce_loom_fine(void)
{
  c3_w off_w = u3R->rut_p >> u3a_page;
  c3_w blk_w, bit_w, pag_w, i_w;
  c3_o fin_o = c3y;

  for ( i_w = 0; i_w < u3P.img_u.pgs_w; i_w++ ) {
    pag_w = i_w;
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;

    if ( !(u3P.dit_w[blk_w] & ((c3_w)1 << bit_w))
       && (  (pag_w < off_w)
          || (u3R->hep.len_w <= (pag_w - off_w))
          || (u3a_free_pg != (u3to(u3_post, u3R->hep.pag_p))[pag_w - off_w]) ) )
    {
      fin_o = c3a(fin_o, _ce_page_fine(&u3P.img_u, pag_w, _ce_len(pag_w)));
    }
  }

  return fin_o;
}
#endif

/* _ce_image_copy(): copy all of [fom_u] to [tou_u]
*/
static c3_o
_ce_image_copy(u3e_image* fom_u, u3e_image* tou_u)
{
  ssize_t ret_i;
  c3_w      i_w;

  //  resize images
  //
  _ce_image_resize(tou_u, fom_u->pgs_w);

  //  seek to begining of patch and images
  //
  if (  (-1 == lseek(fom_u->fid_i, 0, SEEK_SET))
     || (-1 == lseek(tou_u->fid_i, 0, SEEK_SET)) )
  {
    fprintf(stderr, "loom: image copy seek: %s\r\n", strerror(errno));
    return c3n;
  }

  //  copy pages into destination image
  //
  for ( i_w = 0; i_w < fom_u->pgs_w; i_w++ ) {
    c3_y buf_y[_ce_page];
    c3_w off_w = i_w;

    if ( _ce_page != (ret_i = read(fom_u->fid_i, buf_y, _ce_page)) ) {
      if ( 0 < ret_i ) {
        fprintf(stderr, "loom: image copy partial read: %zu\r\n",
                        (size_t)ret_i);
      }
      else {
        fprintf(stderr, "loom: image copy read: %s\r\n",
                        strerror(errno));
      }
      return c3n;
    }
    else {
      if ( -1 == lseek(tou_u->fid_i, _ce_len(off_w), SEEK_SET) ) {
        fprintf(stderr, "loom: image copy seek: %s\r\n", strerror(errno));
        return c3n;
      }
      if ( _ce_page != (ret_i = write(tou_u->fid_i, buf_y, _ce_page)) ) {
        if ( 0 < ret_i ) {
          fprintf(stderr, "loom: image copy partial write: %zu\r\n",
                          (size_t)ret_i);
        }
        else {
          fprintf(stderr, "loom: image copy write: %s\r\n", strerror(errno));
        }
        fprintf(stderr, "info: you probably have insufficient disk space");
        return c3n;
      }
    }
  }

  return c3y;
}

/* u3e_backup(): copy snapshot from [pux_c] to [pax_c],
 * overwriting optionally. note that image files must
 * be named "image".
*/
c3_o
u3e_backup(c3_c* pux_c, c3_c* pax_c, c3_o ovw_o)
{
  //  source image file from [pux_c]
  u3e_image nux_u = { .nam_c = "image", .pgs_w = 0 };

  //  destination image file to [pax_c]
  u3e_image nax_u = { .nam_c = "image", .pgs_w = 0 };

  c3_i mod_i = O_RDWR | O_CREAT;

  if ( !pux_c || !pax_c ) {
    fprintf(stderr, "loom: image backup: bad path\r\n");
    return c3n;
  }

  if ( (c3n == ovw_o) && c3_mkdir(pax_c, 0700) ) {
    if ( EEXIST != errno ) {
      fprintf(stderr, "loom: image backup: %s\r\n", strerror(errno));
    }
    return c3n;
  }

  //  open source image files if they exist
  //
  c3_c nux_c[8193];
  snprintf(nux_c, 8192, "%s/%s.bin", pux_c, nux_u.nam_c);
  if (  (0 != access(nux_c, F_OK))
     || (_ce_img_good != _ce_image_open(&nux_u, pux_c)) )
  {
    fprintf(stderr, "loom: couldn't open image at %s\r\n", pux_c);
    return c3n;
  }

  //  open destination image files
  c3_c nax_c[8193];
  snprintf(nax_c, 8192, "%s/%s.bin", pax_c, nax_u.nam_c);
  if ( -1 == (nax_u.fid_i = c3_open(nax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", nax_c, strerror(errno));
    return c3n;
  }

  if (  (c3n == _ce_image_copy(&nux_u, &nax_u))
     || (c3n == _ce_image_sync(&nax_u)) )
  {
    c3_unlink(nax_c);
    fprintf(stderr, "loom: image backup failed\r\n");
    return c3n;
  }

  close(nax_u.fid_i);
  fprintf(stderr, "loom: image backup complete\r\n");
  return c3y;
}

/*
  u3e_save(): save current changes.

  If we are in dry-run mode, do nothing.

  First, call `_ce_patch_compose` to write all dirty pages to disk and
  clear protection and dirty bits. If there were no dirty pages to write,
  then we're done.

  - Sync the patch files to disk.
  - Verify the patch (because why not?)
  - Write the patch data into the image file (This is idempotent.).
  - Sync the image file.
  - Delete the patchfile and free it.

  Once we've written the dirty pages to disk (and have reset their dirty bits
  and protection flags), we *could* handle the rest of the checkpointing
  process in a separate thread, but we'd need to wait until that finishes
  before we try to make another snapshot.
*/
void
u3e_save(u3_post low_p, u3_post hig_p)
{
  u3_ce_patch* pat_u;
  c3_w old_w = u3P.img_u.pgs_w;

  if ( u3C.wag_w & u3o_dryrun ) {
    return;
  }

  //  XX discard hig_p and friends
  {
    c3_w nop_w = (low_p >> u3a_page);
    c3_w nor_w = (low_p + (_ce_len_words(1) - 1)) >> u3a_page;
    c3_w sop_w = hig_p >> u3a_page;

    u3_assert( (u3P.gar_w > nop_w) && (u3P.gar_w < sop_w) );

    if ( !(pat_u = _ce_patch_compose(nor_w)) ) {
      return;
    }
  }

  if ( u3C.wag_w & u3o_verbose ) {
    fprintf(stderr, "sync: skipped %u free", pat_u->sip_w);
    u3a_print_memory(stderr, " pages", pat_u->sip_w << u3a_page);
  }

  //  attempt to avoid propagating anything insane to disk
  //
  u3a_loom_sane();

  if ( u3C.wag_w & u3o_verbose ) {
    u3a_print_memory(stderr, "sync: save", pat_u->con_u->pgs_w << u3a_page);
  }

  _ce_patch_sync(pat_u);

  if ( c3n == _ce_patch_verify(pat_u) ) {
    u3_assert(!"loom: save failed");
  }

#ifdef U3_SNAPSHOT_VALIDATION
  //  check that clean pages are correct
  //
  u3_assert( c3y == _ce_loom_fine() );
#endif

  _ce_patch_apply(pat_u);

  u3_assert( c3y == _ce_image_sync(&u3P.img_u) );

  _ce_patch_free(pat_u);
  _ce_patch_delete();

#ifdef U3_SNAPSHOT_VALIDATION
  {
    c3_w pgs_w;
    u3_assert( _ce_img_good == _ce_image_stat(&u3P.img_u, &pgs_w) );
    u3_assert( pgs_w == u3P.img_u.pgs_w );
  }

  //  check that all pages in the image are clean and *fine*,
  //  all others are dirty
  //
  //    since total finery requires total cleanliness,
  //    pages of the image are protected twice.
  //
  _ce_loom_protect(u3P.img_u.pgs_w, old_w);

  u3_assert( c3y == _ce_loom_track_sane() );
  u3_assert( c3y == _ce_loom_fine() );
#endif

  if ( u3C.wag_w & u3o_no_demand ) {
#ifndef U3_SNAPSHOT_VALIDATION
    _ce_loom_protect(u3P.img_u.pgs_w, old_w);
#endif
  }
  else {
    _ce_loom_mapf(u3P.img_u.fid_i, u3P.img_u.pgs_w, old_w);
  }

  u3e_toss(low_p, hig_p);
}

/* _ce_toss_pages(): discard ephemeral pages.
*/
static void
_ce_toss_pages(c3_w nor_w, c3_w sou_w)
{
  c3_w  pgs_w = u3P.pag_w - (nor_w + sou_w);
  void* ptr_v = _ce_ptr(nor_w);

  #ifndef U3_OS_windows
  if ( -1 == madvise(ptr_v, _ce_len(pgs_w), MADV_DONTNEED) ) {
      fprintf(stderr, "loom: madv_dontneed failed (%u pages at %u): %s\r\n",
                      pgs_w, nor_w, strerror(errno));
  }
  #endif
}

/* u3e_toss(): discard ephemeral pages.
*/
void
u3e_toss(u3_post low_p, u3_post hig_p)
{
  c3_w nor_w = (low_p + (_ce_len_words(1) - 1)) >> u3a_page;
  c3_w sou_w = u3P.pag_w - (hig_p >> u3a_page);

  _ce_toss_pages(nor_w, sou_w);
}

/* u3e_live(): start the checkpointing system.
*/
c3_o
u3e_live(c3_o nuu_o, c3_c* dir_c)
{
  //  require that our page size is a multiple of the system page size.
  //
  {
    size_t sys_i = sysconf(_SC_PAGESIZE);

    if ( _ce_page % sys_i ) {
      fprintf(stderr, "loom: incompatible system page size (%zuKB)\r\n",
                      sys_i >> 10);
      exit(1);
    }
  }

  u3P.dir_c = dir_c;
  u3P.eph_i = 0;
  u3P.img_u.nam_c = "image";
  u3P.pag_w = u3C.wor_i >> u3a_page;

  //  XX review dryrun requirements, enable or remove
  //
#if 0
  if ( u3C.wag_w & u3o_dryrun ) {
    return c3y;
  } else
#endif
  {
    //  Open the ephemeral space file.
    //
    if ( u3C.wag_w & u3o_swap ) {
      if ( c3n == _ce_ephemeral_open(&u3P.eph_i) ) {
        fprintf(stderr, "boot: failed to load ephemeral file\r\n");
        exit(1);
      }
    }

    //  Open image files.
    //
    c3_c chk_c[8193];
    snprintf(chk_c, 8193, "%s/.urb/chk", u3P.dir_c);

    _ce_img_stat sat_e = _ce_image_open(&u3P.img_u, chk_c);

    if ( _ce_img_fail == sat_e ) {
      fprintf(stderr, "boot: image failed\r\n");
      exit(1);
    }
    else {
      u3_ce_patch* pat_u;

      /* Load any patch files; apply them to images.
      */
      if ( 0 != (pat_u = _ce_patch_open()) ) {
        _ce_patch_apply(pat_u);
        u3_assert( c3y == _ce_image_sync(&u3P.img_u) );
        _ce_patch_free(pat_u);
        _ce_patch_delete();
      }
      else if ( _ce_img_size == sat_e ) {
        fprintf(stderr, "boot: image failed (size)\r\n");
        exit(1);
      }

      //  detect snapshots from a larger loom
      //
      if ( (u3P.img_u.pgs_w + 1) >= u3P.pag_w ) {  // XX?
        fprintf(stderr, "boot: snapshot too big for loom\r\n");
        exit(1);
      }

      //  mark all pages dirty (pages in the snapshot will be marked clean)
      //
      u3e_foul();

      /* Write image files to memory; reinstate protection.
      */
      {
        if ( u3C.wag_w & u3o_swap ) {
          _ce_loom_mapf_ephemeral();
        }

        if ( u3C.wag_w & u3o_no_demand ) {
          _ce_loom_blit(u3P.img_u.fid_i, u3P.img_u.pgs_w);
        }
        else {
          _ce_loom_mapf(u3P.img_u.fid_i, u3P.img_u.pgs_w, 0);
        }

        u3l_log("boot: protected loom");
      }

      /* If the images were empty, we are logically booting.
      */
      if ( !u3P.img_u.pgs_w ) {
        u3l_log("live: logical boot");
        nuu_o = c3y;
      }
      else if ( u3C.wag_w & u3o_no_demand ) {
        u3a_print_memory(stderr, "live: loaded", _ce_len_words(u3P.img_u.pgs_w));
      }
      else {
        u3a_print_memory(stderr, "live: mapped", _ce_len_words(u3P.img_u.pgs_w));
      }

#ifdef U3_GUARD_PAGE
      u3_assert( !_ce_ward_post(u3P.img_u.pgs_w, u3P.pag_w) );
#endif
    }
  }

  return nuu_o;
}

/* u3e_stop(): gracefully stop the persistence system.
*/
void
u3e_stop(void)
{
  if ( u3P.eph_i ) {
    _ce_toss_pages(u3P.img_u.pgs_w, u3P.pag_w);
    close(u3P.eph_i);
    unlink(u3C.eph_c);
  }

  close(u3P.img_u.fid_i);
}

/* u3e_yolo(): disable dirty page tracking, read/write whole loom.
*/
c3_o
u3e_yolo(void)
{
  //  NB: u3e_save() will reinstate protection flags
  //
  if ( 0 != mprotect(_ce_ptr(0),
                     _ce_len(u3P.pag_w),
                     (PROT_READ | PROT_WRITE)) )
  {
    //  XX confirm recoverable errors
    //
    fprintf(stderr, "loom: yolo: %s\r\n", strerror(errno));
    return c3n;
  }

  u3_assert( !_ce_ward_protect() );

  return c3y;
}

/* u3e_foul(): dirty all the pages of the loom.
*/
void
u3e_foul(void)
{
  memset((void*)u3P.dit_w, 0xff, sizeof(u3P.dit_w));
}

/* u3e_init(): initialize guard page tracking, dirty loom
*/
void
u3e_init(void)
{
  u3P.pag_w = u3C.wor_i >> u3a_page;

  u3P.img_u.fid_i = -1;

  u3e_foul();

#ifdef U3_GUARD_PAGE
  u3_assert( !_ce_ward_post(0, u3P.pag_w) );
#endif
}

/* u3e_ward(): reposition guard page if needed.
*/
void
u3e_ward(u3_post low_p, u3_post hig_p)
{
#ifdef U3_GUARD_PAGE
  c3_w nop_w = low_p >> u3a_page;
  c3_w sop_w = hig_p >> u3a_page;
  c3_w pag_w = u3P.gar_w;

  if ( !((pag_w > nop_w) && (pag_w < sop_w)) ) {
    u3_assert( !_ce_ward_post(nop_w, sop_w) );
    u3_assert( !_ce_flaw_mprotect(pag_w) );
    u3_assert( u3P.dit_w[pag_w >> 5] & ((c3_w)1 << (pag_w & 31)) );
  }
#endif
}
