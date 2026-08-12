/// @file
///
/// windows loom mapping. see wloom.c for the design.

#ifndef U3_WLOOM_H
#define U3_WLOOM_H

#include "c3/c3.h"

  /* u3_wnd_loom_init(): reserve [len_i] bytes at [bas_v] and back them with
  **                     a pagefile section. returns c3n if the placeholder
  **                     apis are unavailable, in which case the caller must
  **                     fall back to a plain anonymous mapping (and to
  **                     u3o_no_demand).
  */
    c3_o
    u3_wnd_loom_init(void* bas_v, size_t len_i);

  /* u3_wnd_loom_gran(): virtual memory allocation granularity, in bytes.
  */
    size_t
    u3_wnd_loom_gran(void);

  /* u3_wnd_loom_mapf(): map the low [byt_i] bytes of [fid_i] over the bottom
  **                     of the loom, backing the remainder with the pagefile
  **                     section. [byt_i] must be a multiple of the allocation
  **                     granularity; zero drops the image mapping entirely.
  **
  **   the image is mapped copy-on-write but protected read-only, so that
  **   stores still fault into u3e_fault(). all protections above [byt_i]
  **   are reset to read/write; the caller must re-establish the guard page.
  */
    c3_o
    u3_wnd_loom_mapf(c3_i fid_i, size_t byt_i);

  /* u3_wnd_loom_unmapf(): drop the image mapping, restoring pagefile backing.
  **
  **   windows refuses to truncate or coherently rewrite a mapped file, so
  **   this must be called before a patch is applied to the image.
  */
    c3_o
    u3_wnd_loom_unmapf(void);

  /* u3_wnd_loom_yolo(): make the whole loom writable, region by region.
  **
  **   VirtualProtect() cannot span separate mappings, and a demand-paged
  **   loom is two of them.
  */
    c3_o
    u3_wnd_loom_yolo(void);

#endif /* ifndef U3_WLOOM_H */
