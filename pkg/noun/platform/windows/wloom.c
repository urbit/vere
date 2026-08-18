/// @file
///
/// windows loom mapping.
///
/// ### why
///
/// the demand-paging path in events.c wants POSIX mmap(MAP_FIXED) semantics:
/// drop a file-backed mapping over the bottom of an already-mapped loom,
/// atomically, leaving the rest of the loom alone. windows has no such
/// operation on ordinary mappings -- MapViewOfFileEx() simply fails if the
/// address is occupied, and UnmapViewOfFile() only accepts the base of a
/// whole view, so a hole cannot be punched in the loom.
///
/// windows 10 1803 added placeholder reservations, which do provide it:
/// a range reserved with MEM_RESERVE_PLACEHOLDER can be split, and each
/// piece replaced by a mapping with MEM_REPLACE_PLACEHOLDER. that is the
/// mechanism used here.
///
/// ### layout
///
/// the loom is one placeholder, split in two at the image boundary:
///
///   [0, map_i)      copy-on-write view of image.bin, protected read-only
///   [map_i, len_i)  read/write view of [sec_h], a pagefile section
///
/// with no image mapped, it is a single view of [sec_h] covering the loom.
///
/// the volatile part of the loom is a *section* view rather than ordinary
/// private memory, because moving the boundary means unmapping and remapping
/// it, and windows cannot resize a private region from below without
/// discarding it. section contents outlive their views, so the loom's heap
/// and stack survive a remap. the view always sits at section offset ==
/// loom offset, so addresses are stable across boundary moves.
///
/// ### limitations
///
///   - placeholders can only be split at the allocation granularity (64KB),
///     while loom pages are 16KB. events.c maps the granularity-floored
///     prefix of the image and blits the ragged tail (at most 3 pages).
///   - the image is mapped PAGE_WRITECOPY and then protected down to
///     PAGE_READONLY, so that stores fault into u3e_fault() instead of
///     being silently privatized. the view must be *created* write-copy;
///     a read-only view cannot be upgraded later.
///   - remapping resets every protection above the image, so the caller
///     must re-post the guard page.
///   - committed pages of a section view cannot be decommitted; they stay
///     committed until the view is unmapped. so the loom's commit charge
///     is a high-water mark, and u3_wnd_loom_toss() returns resident
///     memory without returning commit.
///
/// ### minimum windows
///
/// VirtualAlloc2() and MapViewOfFile3() arrived in windows 10 1803, and are
/// exported from an api set rather than kernel32, so linking them raises
/// vere's floor to that release: the loader resolves imports eagerly, and
/// an older windows will refuse to start the binary at all. that floor is
/// deliberate. see build.zig for the import library.
///

#include "noun.h"
#include "wloom.h"

#include <io.h>
#include <windows.h>

//  placeholder flags, in case we are built against older headers
//
#ifndef MEM_RESERVE_PLACEHOLDER
#  define MEM_RESERVE_PLACEHOLDER    0x00040000
#endif
#ifndef MEM_REPLACE_PLACEHOLDER
#  define MEM_REPLACE_PLACEHOLDER    0x00004000
#endif
#ifndef MEM_PRESERVE_PLACEHOLDER
#  define MEM_PRESERVE_PLACEHOLDER   0x00000002
#endif
#ifndef MEM_COALESCE_PLACEHOLDERS
#  define MEM_COALESCE_PLACEHOLDERS  0x00000001
#endif

//  a region of address space we have reserved as a placeholder. slot 0 is
//  the live loom; the rest are stale looms held open during migration,
//  which sit at their own bases and coexist with it.
//
//  NB: occupancy is [len_i], not a c3_o. a loobean c3y is 0, so a zeroed
//  "in use" flag reads as *yes*, and every slot would look taken.
//
typedef struct {
  void*  bas_v;   //  base
  size_t len_i;   //  length, bytes (0 if slot free)
  size_t map_i;   //  image mapping length, bytes (0 if none)
  HANDLE sec_h;   //  pagefile section backing the volatile part
} _wnd_reg;

#define _wnd_regs  2
#define _wnd_chunk ((size_t)16 << 20)

static _wnd_reg wnd_u[_wnd_regs];
static size_t   wnd_gan_i;   //  allocation granularity

/* _wnd_fail(): report a win32 failure.
*/
static void
_wnd_fail(const c3_c* str_c)
{
  DWORD err_u = GetLastError();

  fprintf(stderr, "loom: %s: win32 error %lu\r\n",
                  str_c, (unsigned long)err_u);

  //  windows does not overcommit: the whole loom is charged against RAM
  //  plus the paging file, whether or not it is ever touched.
  //
  if ( ERROR_COMMITMENT_LIMIT == err_u ) {
    fprintf(stderr, "loom: insufficient commit charge for the loom.\r\n"
                    "      either boot with a smaller --loom, or grow the\r\n"
                    "      paging file to exceed the loom size.\r\n");
  }

  //  487. windows will not place a mapping over occupied address space,
  //  so this is a leftover reservation, not a shortage of anything.
  //
  if ( ERROR_INVALID_ADDRESS == err_u ) {
    fprintf(stderr, "loom: the loom address is already occupied\r\n");
  }
}

/* _wnd_read(): read [byt_i] bytes of [fid_i] to [bas_v], from its start.
**
**   NB: goes direct rather than through the pread() shim, whose offset is
**   an off_t and so may be 32 bits wide. a stale image can exceed 2GB.
*/
static c3_o
_wnd_read(c3_i fid_i, void* bas_v, size_t byt_i)
{
  HANDLE fil_h = (HANDLE)_get_osfhandle(fid_i);
  size_t off_i = 0;

  if ( INVALID_HANDLE_VALUE == fil_h ) {
    fprintf(stderr, "loom: read: bad fd %d\r\n", fid_i);
    return c3n;
  }

  while ( off_i < byt_i ) {
    size_t     lef_i = byt_i - off_i;
    DWORD      red_u = (DWORD)((lef_i > _wnd_chunk) ? _wnd_chunk : lef_i);
    DWORD      got_u = 0;
    OVERLAPPED ovl_u = {0};

    ovl_u.Offset     = (DWORD)(off_i & 0xffffffffULL);
    ovl_u.OffsetHigh = (DWORD)((c3_d)off_i >> 32);

    if ( !ReadFile(fil_h, (c3_y*)bas_v + off_i, red_u, &got_u, &ovl_u) ) {
      _wnd_fail("read");
      return c3n;
    }

    if ( !got_u ) {
      fprintf(stderr, "loom: read: short at %zu of %zu\r\n", off_i, byt_i);
      return c3n;
    }

    off_i += got_u;
  }

  return c3y;
}

/* u3_wnd_truncate(): resize [fid_i] to [off_d].
*/
c3_i
u3_wnd_truncate(c3_i fid_i, c3_d off_d)
{
  HANDLE        fil_h = (HANDLE)_get_osfhandle(fid_i);
  LARGE_INTEGER off_u;

  if ( INVALID_HANDLE_VALUE == fil_h ) {
    fprintf(stderr, "loom: truncate: bad fd %d\r\n", fid_i);
    return -1;
  }

  off_u.QuadPart = (LONGLONG)off_d;

  if ( !SetFilePointerEx(fil_h, off_u, NULL, FILE_BEGIN) ) {
    _wnd_fail("truncate seek");
    return -1;
  }

  if ( !SetEndOfFile(fil_h) ) {
    DWORD err_u = GetLastError();

    _wnd_fail("truncate");

    //  1224. the file still has a section, and windows keeps one attached
    //  to the handle that created it until that handle is closed.
    //
    if ( ERROR_USER_MAPPED_FILE == err_u ) {
      fprintf(stderr, "loom: image is still mapped; a view or a descriptor "
                      "that created one has not been released\r\n");
    }

    return -1;
  }

  return 0;
}

/* _wnd_hold(): collapse the current mapping into one placeholder.
*/
static c3_o
_wnd_hold(_wnd_reg* reg_u)
{
  c3_y* bas_y = (c3_y*)reg_u->bas_v;

  if ( !UnmapViewOfFileEx(bas_y, MEM_PRESERVE_PLACEHOLDER) ) {
    _wnd_fail("unmap low");
    return c3n;
  }

  if ( reg_u->map_i ) {
    if ( !UnmapViewOfFileEx(bas_y + reg_u->map_i, MEM_PRESERVE_PLACEHOLDER) ) {
      _wnd_fail("unmap high");
      return c3n;
    }

    if ( !VirtualFree(bas_y, reg_u->len_i,
                      MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS) )
    {
      _wnd_fail("coalesce");
      return c3n;
    }
  }

  reg_u->map_i = 0;

  return c3y;
}

/* _wnd_image(): open a copy-on-write section over the low [byt_i] of [fid_i].
*/
static HANDLE
_wnd_image(c3_i fid_i, size_t byt_i)
{
  HANDLE fil_h = (HANDLE)_get_osfhandle(fid_i);
  HANDLE sec_h;

  if ( INVALID_HANDLE_VALUE == fil_h ) {
    fprintf(stderr, "loom: image handle: bad fd %d\r\n", fid_i);
    return NULL;
  }

  sec_h = CreateFileMappingW(fil_h, NULL, PAGE_WRITECOPY,
                             (DWORD)((c3_d)byt_i >> 32),
                             (DWORD)(byt_i & 0xffffffffULL),
                             NULL);

  if ( !sec_h ) {
    _wnd_fail("image section");
  }

  return sec_h;
}

/* _wnd_remap(): rebuild [reg_u] with [byt_i] bytes of [fid_i], leaving the
**               image pages protected [pro_u].
*/
static c3_o
_wnd_remap(_wnd_reg* reg_u, c3_i fid_i, size_t byt_i, DWORD pro_u)
{
  c3_y*  bas_y = (c3_y*)reg_u->bas_v;
  HANDLE img_h = NULL;
  DWORD  old_u;

  if ( byt_i % wnd_gan_i ) {
    fprintf(stderr, "loom: unaligned image mapping (%zu)\r\n", byt_i);
    return c3n;
  }

  if ( byt_i >= reg_u->len_i ) {
    fprintf(stderr, "loom: image mapping too big (%zu)\r\n", byt_i);
    return c3n;
  }

  //  the image section must be opened before we tear anything down,
  //  so that a failure here is not destructive.
  //
  if ( byt_i && !(img_h = _wnd_image(fid_i, byt_i)) ) {
    return c3n;
  }

  if ( c3n == _wnd_hold(reg_u) ) {
    if ( img_h ) {
      CloseHandle(img_h);
    }
    return c3n;
  }

  //  past this point the loom is unmapped, and failure is fatal.
  //
  if ( byt_i ) {
    if ( !VirtualFree(bas_y, byt_i, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) ) {
      _wnd_fail("split");
      CloseHandle(img_h);
      return c3n;
    }

    if ( !MapViewOfFile3(img_h, NULL, bas_y, 0, byt_i,
                      MEM_REPLACE_PLACEHOLDER, PAGE_WRITECOPY, NULL, 0) )
    {
      _wnd_fail("map image");
      CloseHandle(img_h);
      return c3n;
    }

    //  the view is created write-copy so that it *can* be dirtied. the
    //  live loom then runs it read-only, so that dirtying faults; a stale
    //  loom is scratch, and stays writable.
    //
    if (  (PAGE_WRITECOPY != pro_u)
       && !VirtualProtect(bas_y, byt_i, pro_u, &old_u) )
    {
      _wnd_fail("protect image");
      CloseHandle(img_h);
      return c3n;
    }

    //  the view holds its own reference
    //
    CloseHandle(img_h);
    reg_u->map_i = byt_i;
  }

  if ( !MapViewOfFile3(reg_u->sec_h, NULL, bas_y + byt_i, (ULONG64)byt_i,
                    reg_u->len_i - byt_i,
                    MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) )
  {
    _wnd_fail("map loom");
    return c3n;
  }

  return c3y;
}

static c3_o
_wnd_release(_wnd_reg* reg_u);

/* _wnd_reserve(): reserve [len_i] at [bas_v], backed by a reserved section.
*/
static c3_o
_wnd_reserve(_wnd_reg* reg_u, void* bas_v, size_t len_i)
{
  SYSTEM_INFO inf_u;
  HANDLE      sec_h;

  GetSystemInfo(&inf_u);
  wnd_gan_i = inf_u.dwAllocationGranularity;

  //  SEC_RESERVE: the section's pages are reserved, not committed, so the
  //  loom costs commit charge only as it is touched. windows does not
  //  overcommit, and a committed section of the whole loom cannot be
  //  created at all on a box whose RAM plus paging file is smaller.
  //
  sec_h = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
                             PAGE_READWRITE | SEC_RESERVE,
                             (DWORD)((c3_d)len_i >> 32),
                             (DWORD)(len_i & 0xffffffffULL),
                             NULL);

  if ( !sec_h ) {
    _wnd_fail("loom section");
    return c3n;
  }

  if ( !VirtualAlloc2(NULL, bas_v, len_i,
                  MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                  NULL, 0) )
  {
    _wnd_fail("reserve");
    CloseHandle(sec_h);
    return c3n;
  }

  if ( !MapViewOfFile3(sec_h, NULL, bas_v, 0, len_i,
                  MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) )
  {
    _wnd_fail("map loom");
    VirtualFree(bas_v, 0, MEM_RELEASE);
    CloseHandle(sec_h);
    return c3n;
  }

  //  NB: committed to the slot only on success, so that a failed
  //  reservation leaves it free rather than half-claimed.
  //
  reg_u->bas_v = bas_v;
  reg_u->len_i = len_i;
  reg_u->map_i = 0;
  reg_u->sec_h = sec_h;

  return c3y;
}

/* _wnd_find(): the region containing [adr_v], if any.
*/
static _wnd_reg*
_wnd_find(void* adr_v)
{
  c3_y* adr_y = (c3_y*)adr_v;
  c3_w  i_w;

  for ( i_w = 0; i_w < _wnd_regs; i_w++ ) {
    c3_y* bas_y = (c3_y*)wnd_u[i_w].bas_v;

    if (  wnd_u[i_w].len_i
       && (adr_y >= bas_y)
       && (adr_y < (bas_y + wnd_u[i_w].len_i)) )
    {
      return &wnd_u[i_w];
    }
  }

  return 0;
}

/* u3_wnd_loom_init(): reserve loom address space, back it with a section.
*/
c3_o
u3_wnd_loom_init(void* bas_v, size_t len_i)
{
  //  NB: idempotent, because mmap(MAP_FIXED) is. u3m_init() is called a
  //  second time after a migration, and on windows a reservation that is
  //  still standing makes the address unavailable rather than being
  //  silently replaced.
  //
  if ( wnd_u[0].len_i ) {
    if ( c3n == _wnd_release(&wnd_u[0]) ) {
      fprintf(stderr, "loom: failed to release the previous loom\r\n");
      return c3n;
    }
  }

  return _wnd_reserve(&wnd_u[0], bas_v, len_i);
}

/* u3_wnd_loom_gran(): virtual memory allocation granularity, in bytes.
*/
size_t
u3_wnd_loom_gran(void)
{
  if ( !wnd_gan_i ) {
    SYSTEM_INFO inf_u;
    GetSystemInfo(&inf_u);
    wnd_gan_i = inf_u.dwAllocationGranularity;
  }

  return wnd_gan_i;
}

/* u3_wnd_loom_live(): c3y if the loom is a placeholder reservation.
*/
c3_o
u3_wnd_loom_live(void)
{
  return wnd_u[0].len_i ? c3y : c3n;
}

/* u3_wnd_loom_commit(): commit [len_i] bytes at [adr_v].
*/
c3_o
u3_wnd_loom_commit(void* adr_v, size_t len_i)
{
  if ( !_wnd_find(adr_v) ) {
    return c3y;
  }

  //  NB: within an image view this is a no-op that succeeds; file-backed
  //  pages are committed by the mapping.
  //
  if ( !VirtualAlloc(adr_v, len_i, MEM_COMMIT, PAGE_READWRITE) ) {
    _wnd_fail("commit");
    return c3n;
  }

  return c3y;
}

/* u3_wnd_loom_fault(): commit [len_i] at [adr_v] if it is reserved.
*/
c3_o
u3_wnd_loom_fault(void* adr_v, size_t len_i)
{
  MEMORY_BASIC_INFORMATION inf_u;

  //  NB: membership matters. this runs on every fault, and must not
  //  commit reserved memory that belongs to something else.
  //
  if ( !_wnd_find(adr_v) ) {
    return c3n;
  }

  if ( !VirtualQuery(adr_v, &inf_u, sizeof(inf_u)) ) {
    _wnd_fail("fault query");
    return c3n;
  }

  //  already committed: not a first touch, and not ours to resolve
  //
  if ( MEM_RESERVE != inf_u.State ) {
    return c3n;
  }

  if ( !VirtualAlloc(adr_v, len_i, MEM_COMMIT, PAGE_READWRITE) ) {
    _wnd_fail("fault commit");
    return c3n;
  }

  return c3y;
}

/* u3_wnd_loom_mapf(): map [byt_i] of [fid_i] over the bottom of the loom.
*/
c3_o
u3_wnd_loom_mapf(c3_i fid_i, size_t byt_i)
{
  if ( !wnd_u[0].len_i ) {
    fprintf(stderr, "loom: mapf without placeholder loom\r\n");
    return c3n;
  }

  return _wnd_remap(&wnd_u[0], fid_i, byt_i, PAGE_READONLY);
}

/* u3_wnd_loom_unmapf(): drop the image mapping, restoring pagefile backing.
*/
c3_o
u3_wnd_loom_unmapf(void)
{
  if ( !wnd_u[0].len_i || !wnd_u[0].map_i ) {
    return c3y;
  }

  return _wnd_remap(&wnd_u[0], -1, 0, PAGE_READONLY);
}

/* u3_wnd_loom_hold(): reserve a stale loom at [bas_v] and map [byt_i] bytes
**                     of [fid_i] writable over its bottom.
*/
c3_o
u3_wnd_loom_hold(void* bas_v, size_t len_i, c3_i fid_i, size_t byt_i)
{
  _wnd_reg* reg_u = 0;
  c3_w      i_w;

  for ( i_w = 1; i_w < _wnd_regs; i_w++ ) {
    if ( !wnd_u[i_w].len_i ) {
      reg_u = &wnd_u[i_w];
      break;
    }
  }

  if ( !reg_u ) {
    fprintf(stderr, "loom: no free stale loom slot\r\n");
    return c3n;
  }

  if ( byt_i > len_i ) {
    fprintf(stderr, "loom: stale image (%zu) exceeds loom (%zu)\r\n",
                    byt_i, len_i);
    return c3n;
  }

  if ( c3n == _wnd_reserve(reg_u, bas_v, len_i) ) {
    return c3n;
  }

  if ( !byt_i ) {
    return c3y;
  }

  //  NB: read, not mapped.
  //
  //    the stale image and the migrated snapshot are the same file: a
  //    migration reads the old image.bin and writes the new one back over
  //    it. windows keeps a file's section attached to the handle that
  //    created it, so a mapped stale loom holds that file against every
  //    later resize of it -- both the crash-recovery patch u3e_live()
  //    applies and the truncate at the end of u3m_save().
  //
  //    so the stale loom is read into its reservation and holds nothing.
  //    it costs commit charge for the image, which a mapping would not,
  //    but a migration is a one-time operation.
  //
  if ( c3n == u3_wnd_loom_commit(bas_v, byt_i) ) {
    return c3n;
  }

  return _wnd_read(fid_i, bas_v, byt_i);
}

/* _wnd_release(): unmap and release a region, freeing its slot.
*/
static c3_o
_wnd_release(_wnd_reg* reg_u)
{
  //  NB: the image view must go, or the file stays mapped.
  //
  if ( !UnmapViewOfFileEx(reg_u->bas_v, MEM_PRESERVE_PLACEHOLDER) ) {
    _wnd_fail("release unmap low");
    return c3n;
  }

  if ( reg_u->map_i ) {
    if ( !UnmapViewOfFileEx((c3_y*)reg_u->bas_v + reg_u->map_i,
                    MEM_PRESERVE_PLACEHOLDER) )
    {
      _wnd_fail("release unmap high");
      return c3n;
    }

    //  the reservation was split, so each half is released on its own
    //
    VirtualFree((c3_y*)reg_u->bas_v + reg_u->map_i, 0, MEM_RELEASE);
  }

  VirtualFree(reg_u->bas_v, 0, MEM_RELEASE);
  CloseHandle(reg_u->sec_h);

  memset(reg_u, 0, sizeof(*reg_u));

  return c3y;
}

/* u3_wnd_loom_drop(): release a stale loom.
*/
c3_o
u3_wnd_loom_drop(void* bas_v)
{
  _wnd_reg* reg_u = _wnd_find(bas_v);

  if ( !reg_u ) {
    fprintf(stderr, "loom: drop: no region at %p\r\n", bas_v);
    return c3n;
  }

  if ( reg_u == &wnd_u[0] ) {
    fprintf(stderr, "loom: drop: refusing to drop the live loom\r\n");
    return c3n;
  }

  return _wnd_release(reg_u);
}

/* u3_wnd_loom_toss(): release the physical pages of [len_i] at [adr_v].
*/
c3_o
u3_wnd_loom_toss(void* adr_v, size_t len_i)
{
  _wnd_reg* reg_u = _wnd_find(adr_v);
  c3_y*     cur_y = (c3_y*)adr_v;
  c3_y*     end_y = cur_y + len_i;
  c3_y*     lim_y;
  c3_o      san_o = c3y;

  if ( !reg_u ) {
    return c3n;
  }

  //  NB: clamped to the region. the caller derives its length from the
  //  road watermarks, which can underflow to something enormous, and
  //  discarding past the loom would take memory that is not ours.
  //
  lim_y = (c3_y*)reg_u->bas_v + reg_u->len_i;

  if ( end_y > lim_y ) {
    end_y = lim_y;
  }

  while ( cur_y < end_y ) {
    MEMORY_BASIC_INFORMATION inf_u;
    size_t                   siz_i;
    DWORD                    ret_u;

    if ( !VirtualQuery(cur_y, &inf_u, sizeof(inf_u)) ) {
      _wnd_fail("toss query");
      return c3n;
    }

    //  NB: RegionSize runs from BaseAddress, which may precede [cur_y].
    //
    siz_i = inf_u.RegionSize - (size_t)(cur_y - (c3_y*)inf_u.BaseAddress);

    if ( !siz_i ) {
      break;
    }

    if ( (cur_y + siz_i) > end_y ) {
      siz_i = (size_t)(end_y - cur_y);
    }

    //  a reserved page has nothing to release, and the guard page is
    //  PAGE_NOACCESS. DiscardVirtualMemory rejects both with
    //  ERROR_INVALID_PARAMETER, which is why this walks the range
    //  instead of passing it whole.
    //
    if (  (MEM_COMMIT == inf_u.State)
       && !(inf_u.Protect & PAGE_NOACCESS) )
    {
      if ( 0 != (ret_u = DiscardVirtualMemory(cur_y, siz_i)) ) {
        fprintf(stderr, "loom: toss discard (%zu bytes at %p): win32 %lu\r\n",
                        siz_i, (void*)cur_y, (unsigned long)ret_u);
        san_o = c3n;
      }
    }

    cur_y += siz_i;
  }

  return san_o;
}

/* u3_wnd_loom_yolo(): make the whole loom writable, region by region.
*/
c3_o
u3_wnd_loom_yolo(void)
{
  c3_y* bas_y = (c3_y*)wnd_u[0].bas_v;
  c3_y* end_y = bas_y + wnd_u[0].len_i;
  c3_y* cur_y = bas_y;

  if ( !wnd_u[0].len_i ) {
    return c3n;
  }

  while ( cur_y < end_y ) {
    MEMORY_BASIC_INFORMATION inf_u;
    size_t                   siz_i;
    DWORD                    new_u, old_u;

    if ( !VirtualQuery(cur_y, &inf_u, sizeof(inf_u)) ) {
      _wnd_fail("yolo query");
      return c3n;
    }

    siz_i = inf_u.RegionSize;

    if ( (cur_y + siz_i) > end_y ) {
      siz_i = (size_t)(end_y - cur_y);
    }

    //  reserved but uncommitted: nothing has been written here, so there
    //  is nothing to make writable, and VirtualProtect would fail.
    //  committing would defeat the point of a sparse loom.
    //
    if ( MEM_RESERVE == inf_u.State ) {
      cur_y += siz_i;
      continue;
    }

    //  a copy-on-write region must stay copy-on-write; PAGE_READWRITE
    //  would both fail and, if it did not, write through to the image.
    //
    new_u = ( PAGE_WRITECOPY == inf_u.AllocationProtect )
            ? PAGE_WRITECOPY
            : PAGE_READWRITE;

    if ( !VirtualProtect(cur_y, siz_i, new_u, &old_u) ) {
      _wnd_fail("yolo protect");
      return c3n;
    }

    cur_y += siz_i;
  }

  return c3y;
}
