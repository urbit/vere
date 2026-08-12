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

//  NB: resolved dynamically. these live in kernelbase.dll, which mingw's
//  import libraries do not reliably cover, and their absence is a
//  recoverable condition (windows older than 10 1803) rather than a link
//  error. extended parameters are unused, so they are typed void*.
//
typedef PVOID (WINAPI *_wnd_valloc2_f)(HANDLE, PVOID, SIZE_T, ULONG, ULONG,
                                       void*, ULONG);
typedef PVOID (WINAPI *_wnd_mapview3_f)(HANDLE, HANDLE, PVOID, ULONG64,
                                        SIZE_T, ULONG, ULONG, void*, ULONG);
typedef BOOL  (WINAPI *_wnd_unmapex_f)(PVOID, ULONG);

static struct {
  c3_o            yes_o;   //  placeholder apis resolved
  void*           bas_v;   //  loom base
  size_t          len_i;   //  loom length, bytes
  size_t          map_i;   //  image mapping length, bytes (0 if none)
  HANDLE          sec_h;   //  pagefile section backing the loom
  size_t          gan_i;   //  allocation granularity
  _wnd_valloc2_f  val_f;
  _wnd_mapview3_f map_f;
  _wnd_unmapex_f  unm_f;
} wnd_u;

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
}

/* _wnd_procs(): resolve the placeholder apis.
*/
static c3_o
_wnd_procs(void)
{
  HMODULE mod_h = GetModuleHandleW(L"kernelbase.dll");

  if ( !mod_h ) {
    mod_h = LoadLibraryW(L"kernelbase.dll");
  }

  if ( mod_h ) {
    wnd_u.val_f = (_wnd_valloc2_f)(void*)
                    GetProcAddress(mod_h, "VirtualAlloc2");
    wnd_u.map_f = (_wnd_mapview3_f)(void*)
                    GetProcAddress(mod_h, "MapViewOfFile3");
    wnd_u.unm_f = (_wnd_unmapex_f)(void*)
                    GetProcAddress(mod_h, "UnmapViewOfFileEx");

    if ( wnd_u.val_f && wnd_u.map_f && wnd_u.unm_f ) {
      return c3y;
    }
  }

  fprintf(stderr, "loom: no placeholder support "
                  "(windows 10 1803 or later required)\r\n");
  return c3n;
}

/* _wnd_hold(): collapse the current mapping into one placeholder.
*/
static c3_o
_wnd_hold(void)
{
  c3_y* bas_y = (c3_y*)wnd_u.bas_v;

  if ( !wnd_u.unm_f(bas_y, MEM_PRESERVE_PLACEHOLDER) ) {
    _wnd_fail("unmap low");
    return c3n;
  }

  if ( wnd_u.map_i ) {
    if ( !wnd_u.unm_f(bas_y + wnd_u.map_i, MEM_PRESERVE_PLACEHOLDER) ) {
      _wnd_fail("unmap high");
      return c3n;
    }

    if ( !VirtualFree(bas_y, wnd_u.len_i,
                      MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS) )
    {
      _wnd_fail("coalesce");
      return c3n;
    }
  }

  wnd_u.map_i = 0;

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

/* _wnd_remap(): rebuild the loom mapping with [byt_i] bytes of [fid_i].
*/
static c3_o
_wnd_remap(c3_i fid_i, size_t byt_i)
{
  c3_y*  bas_y = (c3_y*)wnd_u.bas_v;
  HANDLE img_h = NULL;
  DWORD  old_u;

  if ( byt_i % wnd_u.gan_i ) {
    fprintf(stderr, "loom: unaligned image mapping (%zu)\r\n", byt_i);
    return c3n;
  }

  if ( byt_i >= wnd_u.len_i ) {
    fprintf(stderr, "loom: image mapping too big (%zu)\r\n", byt_i);
    return c3n;
  }

  //  the image section must be opened before we tear anything down,
  //  so that a failure here is not destructive.
  //
  if ( byt_i && !(img_h = _wnd_image(fid_i, byt_i)) ) {
    return c3n;
  }

  if ( c3n == _wnd_hold() ) {
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

    if ( !wnd_u.map_f(img_h, NULL, bas_y, 0, byt_i,
                      MEM_REPLACE_PLACEHOLDER, PAGE_WRITECOPY, NULL, 0) )
    {
      _wnd_fail("map image");
      CloseHandle(img_h);
      return c3n;
    }

    //  the view is created write-copy so that it *can* be dirtied, but
    //  runs read-only so that dirtying faults.
    //
    if ( !VirtualProtect(bas_y, byt_i, PAGE_READONLY, &old_u) ) {
      _wnd_fail("protect image");
      CloseHandle(img_h);
      return c3n;
    }

    //  the view holds its own reference
    //
    CloseHandle(img_h);
    wnd_u.map_i = byt_i;
  }

  if ( !wnd_u.map_f(wnd_u.sec_h, NULL, bas_y + byt_i, (ULONG64)byt_i,
                    wnd_u.len_i - byt_i,
                    MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) )
  {
    _wnd_fail("map loom");
    return c3n;
  }

  return c3y;
}

/* u3_wnd_loom_init(): reserve loom address space, back it with a section.
*/
c3_o
u3_wnd_loom_init(void* bas_v, size_t len_i)
{
  SYSTEM_INFO inf_u;

  if ( c3n == _wnd_procs() ) {
    return c3n;
  }

  GetSystemInfo(&inf_u);

  wnd_u.bas_v = bas_v;
  wnd_u.len_i = len_i;
  wnd_u.map_i = 0;
  wnd_u.gan_i = inf_u.dwAllocationGranularity;

  wnd_u.sec_h = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                   (DWORD)((c3_d)len_i >> 32),
                                   (DWORD)(len_i & 0xffffffffULL),
                                   NULL);

  if ( !wnd_u.sec_h ) {
    _wnd_fail("loom section");
    return c3n;
  }

  if ( !wnd_u.val_f(NULL, bas_v, len_i,
                    MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                    NULL, 0) )
  {
    _wnd_fail("reserve");
    CloseHandle(wnd_u.sec_h);
    wnd_u.sec_h = NULL;
    return c3n;
  }

  if ( !wnd_u.map_f(wnd_u.sec_h, NULL, bas_v, 0, len_i,
                    MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) )
  {
    _wnd_fail("map loom");
    VirtualFree(bas_v, 0, MEM_RELEASE);
    CloseHandle(wnd_u.sec_h);
    wnd_u.sec_h = NULL;
    return c3n;
  }

  wnd_u.yes_o = c3y;

  return c3y;
}

/* u3_wnd_loom_gran(): virtual memory allocation granularity, in bytes.
*/
size_t
u3_wnd_loom_gran(void)
{
  if ( !wnd_u.gan_i ) {
    SYSTEM_INFO inf_u;
    GetSystemInfo(&inf_u);
    wnd_u.gan_i = inf_u.dwAllocationGranularity;
  }

  return wnd_u.gan_i;
}

/* u3_wnd_loom_mapf(): map [byt_i] of [fid_i] over the bottom of the loom.
*/
c3_o
u3_wnd_loom_mapf(c3_i fid_i, size_t byt_i)
{
  if ( c3y != wnd_u.yes_o ) {
    fprintf(stderr, "loom: mapf without placeholder loom\r\n");
    return c3n;
  }

  return _wnd_remap(fid_i, byt_i);
}

/* u3_wnd_loom_unmapf(): drop the image mapping, restoring pagefile backing.
*/
c3_o
u3_wnd_loom_unmapf(void)
{
  if ( c3y != wnd_u.yes_o ) {
    return c3y;
  }

  if ( !wnd_u.map_i ) {
    return c3y;
  }

  return _wnd_remap(-1, 0);
}

/* u3_wnd_loom_yolo(): make the whole loom writable, region by region.
*/
c3_o
u3_wnd_loom_yolo(void)
{
  c3_y* bas_y = (c3_y*)wnd_u.bas_v;
  c3_y* end_y = bas_y + wnd_u.len_i;
  c3_y* cur_y = bas_y;

  if ( c3y != wnd_u.yes_o ) {
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
