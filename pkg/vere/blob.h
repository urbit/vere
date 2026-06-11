/// @file

#ifndef U3_VERE_BLOB_H
#define U3_VERE_BLOB_H

#include "c3/c3.h"
#include "noun.h"

  /* Blob store: content-addressed storage for large atoms.
  **
  ** Files live in $pier/.urb/bob/<mug>/<seq>.
  ** Each mug bucket has a lockfile ($pier/.urb/bob/<mug>/lock) holding
  ** the next available sequence number (ASCII decimal).
  **
  ** Earth is the sole writer; Mars is read-only.
  */

  /* U3_BLOB_THRESH: atoms larger than this (in bytes) are blobified.
  */
#   define U3_BLOB_THRESH  (32ULL * 1024ULL * 1024ULL)

    /* u3_blob_init(): initialize blob store; create .urb/bob/ if needed.
    */
      void
      u3_blob_init(const c3_c* pax_c);

    /* u3_blob_stg_init(): initialize staging area; create .urb/bob/stg/ if needed.
    **
    ** The staging dir holds mkstemp(3) temp files written by Earth before
    ** they are handed to Mars for rename(2) into the final bob/<mug>/<seq>
    ** location.  Cleaned (emptied) on every boot.
    */
      void
      u3_blob_stg_init(const c3_c* pax_c);

    /* u3_blob_save(): write bytes to blob store.
    **
    ** Deduplicates within the mug bucket (byte-for-byte comparison).
    ** On success, returns c3y and sets *mug_h and *seq_h.
    */
      c3_o
      u3_blob_save(const c3_c* pax_c,
                   const c3_y* dat_y,
                   c3_d        len_d,
                   c3_h*       mug_h,
                   c3_h*       seq_h);

    /* u3_blob_save_fd(): streaming write from open file descriptor.
    **
    ** Reads [len_d] bytes from [fid_i], writes to blob store.
    ** Avoids double-buffering for large file ingestion.
    ** On success, returns c3y and sets *mug_h and *seq_h.
    */
      c3_o
      u3_blob_save_fd(const c3_c* pax_c,
                      c3_i        fid_i,
                      c3_d        len_d,
                      c3_h*       mug_h,
                      c3_h*       seq_h);

    /* u3_blob_load(): read blob into a loom atom.
    **
    ** Returns u3_none on failure.
    */
      u3_weak
      u3_blob_load(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3_blob_live(): check whether a blob file exists.
    */
      c3_o
      u3_blob_live(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3_blob_wipe(): delete a blob file.
    **
    ** Called when a bob atom's total refcount reaches zero.
    */
      void
      u3_blob_wipe(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3_blob_walk(): enumerate every blob file in the store.
    **
    ** Calls [fun_f] with (mug_h, seq_h) for each $pier/.urb/bob/<mug>/<seq>
    ** file on disk.  Skips the staging dir and bucket lockfiles.  The
    ** callback must not create or delete blob files (collect, then act).
    */
      void
      u3_blob_walk(const c3_c* pax_c,
                   void*       ptr_v,
                   void      (*fun_f)(void*, c3_h, c3_h));

    /* u3_blob_move_stg(): install a staging file into the blob store.
    **
    ** [stg_c] is the path of a temp file in $pier/.urb/bob/stg/.
    ** Computes mug, deduplicates, then rename(2)s into bob/<mug>/<seq>.
    ** The staging file is always consumed on success.
    ** On success, returns c3y and sets *mug_h and *seq_h.
    */
      c3_o
      u3_blob_move_stg(const c3_c* pax_c,
                          const c3_c* stg_c,
                          c3_h*       mug_h,
                          c3_h*       seq_h);

    /* u3_blob_path(): write filesystem path for a blob into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.
    */
      void
      u3_blob_path(c3_c*       out_c,
                   const c3_c* pax_c,
                   c3_h        mug_h,
                   c3_h        seq_h);

    /* u3_blob_map(): mmap a blob file for direct byte access.
    **
    ** Returns a read-only pointer to the blob's bytes (length in *len_d),
    ** or NULL on failure.  The mapping must be released via u3_blob_unmap().
    ** No loom allocation is performed.
    */
      const c3_y*
      u3_blob_mmap(const c3_c* pax_c, c3_h mug_h, c3_h seq_h, c3_d* len_d);

    /* u3_blob_unmap(): release a mapping returned by u3_blob_map().
    */
      void
      u3_blob_umap(const c3_y* ptr_y, c3_d len_d);

    /* u3_blob_met(): compute the bit-length of a blob without full materialization.
    **
    ** Equivalent to u3r_met(0, materialized_atom) but avoids loading the whole
    ** blob into the loom.  Reads only the file size and last byte.
    ** Returns 0 on error (blob missing or empty).
    */
      c3_d
      u3_blob_met(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

#endif /* ifndef U3_VERE_BLOB_H */
