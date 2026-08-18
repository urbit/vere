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

  /* u3_wnd_loom_live(): c3y if the loom is a placeholder reservation.
  **
  **   c3n means we degraded to a plain mapping, which is neither sparse
  **   nor demand-pageable.
  */
    c3_o
    u3_wnd_loom_live(void);

  /* u3_wnd_loom_commit(): commit [len_i] bytes at [adr_v].
  **
  **   idempotent, and a no-op on a loom that is not a placeholder
  **   reservation. any range that will be written by the kernel -- read(2)
  **   into the loom, say -- must be committed first, since the fault
  **   handler cannot rescue a failed kernel-mode access.
  */
    c3_o
    u3_wnd_loom_commit(void* adr_v, size_t len_i);

  /* u3_wnd_loom_fault(): commit [len_i] at [adr_v] if it is reserved.
  **
  **   returns c3y if the fault was a first touch and has been resolved,
  **   c3n if the page was already committed and the fault means something
  **   else.
  */
    c3_o
    u3_wnd_loom_fault(void* adr_v, size_t len_i);

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

  /* u3_wnd_truncate(): resize [fid_i] to [off_d]. 0 on success, -1 on error.
  **
  **   the CRT reports every cause of a failed resize as EACCES, which does
  **   not distinguish a still-mapped file from a sharing violation from a
  **   permissions problem. this reports the win32 error.
  */
    c3_i
    u3_wnd_truncate(c3_i fid_i, c3_d off_d);

  /* u3_wnd_loom_hold(): reserve a stale loom of [len_i] at [bas_v], and read
  **                     [byt_i] bytes of [fid_i] into its bottom.
  **
  **   for the old snapshot a migration reads out of. it coexists with the
  **   live loom at its own base, and is never tracked or saved.
  **
  **   NB: read rather than mapped, deliberately. the stale image and the
  **   migrated snapshot are the same file, and a mapping would hold it
  **   against being resized. see wloom.c.
  */
    c3_o
    u3_wnd_loom_hold(void* bas_v, size_t len_i, c3_i fid_i, size_t byt_i);

  /* u3_wnd_loom_drop(): release a stale loom.
  */
    c3_o
    u3_wnd_loom_drop(void* bas_v);

  /* u3_wnd_loom_toss(): release the physical pages of [len_i] at [adr_v].
  **
  **   the windows analogue of madvise(MADV_DONTNEED). DiscardVirtualMemory
  **   frees the pages and leaves them reading as zero, exactly as linux
  **   does, but rejects anything not committed and accessible -- so the
  **   range is walked and applied region by region, skipping reserved
  **   pages and the guard page.
  **
  **   NB: resident memory only. committed pages of a section view cannot
  **   be decommitted, so the loom's commit charge is a high-water mark.
  */
    c3_o
    u3_wnd_loom_toss(void* adr_v, size_t len_i);

  /* u3_wnd_loom_yolo(): make the whole loom writable, region by region.
  **
  **   VirtualProtect() cannot span separate mappings, and a demand-paged
  **   loom is two of them.
  */
    c3_o
    u3_wnd_loom_yolo(void);

#endif /* ifndef U3_WLOOM_H */
