/// @file

#ifndef U3_NOUN_BLOB_H
#define U3_NOUN_BLOB_H

#include "c3/c3.h"
#include "types.h"
#include "serial.h"

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

  /* U3_BLOB_MIN: least significant byte length a blob may hold.
  **
  ** The loom normalizes atoms: _ci_atom_mint() returns a cat whenever the
  ** value fits, so an ordinary pug always denotes a value greater than
  ** u3a_direct_max. u3r_sing relies on that — _cr_sing_atom rejects any
  ** pair that is not two pugs before it ever consults u3a_is_bob — and a
  ** bob is a pug whose value is whatever its file holds.  So a blob must
  ** denote an atom the loom would also make indirect.
  **
  ** sizeof(c3_w) bytes is ambiguous (a 4- or 8-byte value is direct when
  ** its top bit is clear); one more byte is always indirect in both
  ** bitnesses. Callers apply the far larger U3_BLOB_THRESH as policy;
  ** this is the floor the representation itself requires.
  */
#   define U3_BLOB_MIN  (sizeof(c3_w) + 1)

  _Static_assert(U3_BLOB_THRESH > U3_BLOB_MIN,
                 "blobified atoms must be indirect in the loom");

    /* u3_blob_bob_dir(): write the $pier/.urb/bob path into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.  Pier setup (disk.c) creates this
    ** directory; the blob store only reads/writes files beneath it.
    */
      void
      u3_blob_bob_dir(c3_c* out_c, const c3_c* pax_c);

    /* u3_blob_stg_dir(): write the $pier/.urb/bob/stg staging path into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.
    */
      void
      u3_blob_stg_dir(c3_c* out_c, const c3_c* pax_c);

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

    /* u3_blob_hand: process-global handle on an open blob file.
    **
    **   One hand per blob per process: every concurrent reader shares the
    **   fd and, once a flat view asks for it, the whole-file buffer.  A
    **   hand is referenced per road; it closes when its last reference
    **   goes, or when a bail or signal unwinds a road that still holds
    **   references (u3_blob_sweep, u3_blob_sweep_kids).  Every mutation of
    **   the registry runs inside u3m_crit_enter()/u3m_crit_leave(), so a
    **   signal can neither leak the fd nor interrupt the table.
    */
      typedef struct _u3_blob_ref {
        void*                rod_v;    //  owning road (u3R at open)
        c3_w                 ref_w;    //  references held by that road
        struct _u3_blob_ref* nex_u;
      } u3_blob_ref;

      typedef struct _u3_blob_hand {
        c3_d         bid_d;   //  (mug_h << 32) | seq_h
        c3_i         fid_i;   //  O_RDONLY fd, open for the hand's lifetime
        c3_d         len_d;   //  file size
        c3_d         met_d;   //  cached bit-length (0 until asked)
        c3_y*        buf_y;   //  whole-file buffer (0 until u3_blob_data)
        c3_w         ref_w;   //  total references
        u3_blob_ref* ref_u;   //  per-road references
      } u3_blob_hand;

    /* u3_blob_init(): initialize the handle registry (from u3m_init).
    */
      void
      u3_blob_init(void);

    /* u3_blob_stop(): release every handle and the registry (from u3m_stop).
    */
      void
      u3_blob_stop(void);

    /* u3_blob_open(): open a blob, or add a reference to its open hand.
    **
    **   The reference is owned by the current road.  Returns 0 (without
    **   bailing) if the file is missing or empty.
    */
      u3_blob_hand*
      u3_blob_open(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3_blob_close(): drop the current road's reference to [han_u].
    **
    **   The hand is closed and freed when its last reference goes.
    */
      void
      u3_blob_close(u3_blob_hand* han_u);

    /* u3_blob_read(): read [len_z] bytes at [off_d] into [dst_y].
    **
    **   Returns the number of bytes read; short only at end of file or
    **   on error.  Never allocates.
    */
      c3_z
      u3_blob_read(u3_blob_hand* han_u, c3_d off_d, c3_y* dst_y, c3_z len_z);

    /* u3_blob_data(): the whole file, read into a heap buffer on first use.
    **
    **   The buffer lives as long as the hand and is zero-padded to a
    **   multiple of 8 bytes, so word-at-a-time readers stay in bounds.
    **   Returns 0 on a short read.
    */
      const c3_y*
      u3_blob_data(u3_blob_hand* han_u);

    /* u3_blob_hand_met(): bit-length of the blob's content, cached on the hand.
    **
    **   Equivalent to u3r_met(0, atom).  Scans backward from the end of the
    **   file in small windows.  Returns 0 if the content is all zero.
    */
      c3_d
      u3_blob_hand_met(u3_blob_hand* han_u);

    /* u3_blob_sweep(): release every reference held by road [rod_v].
    **
    **   Called from u3m_bail before the longjmp: the road's C frames, and
    **   with them every view they held, are about to vanish.
    */
      void
      u3_blob_sweep(void* rod_v);

    /* u3_blob_sweep_kids(): release every reference not held by the home road.
    **
    **   Called after a signal unwinds to the top level, and at the end of
    **   u3m_soft_top.  Returns the number of references released.
    */
      c3_w
      u3_blob_sweep_kids(void);

    /* u3_blob_hands(): number of open hands.
    */
      c3_z
      u3_blob_hands(void);

    /* u3_blob_met(): compute the bit-length of a blob without full materialization.
    **
    **   Equivalent to u3r_met(0, materialized_atom) but avoids loading the whole
    **   blob into the loom.  Returns 0 on error (blob missing or empty).
    */
      c3_d
      u3_blob_met(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3_blob_bsink: streaming byte sink for blob-aware cue (u3s_bsink).
    **
    ** Streams a large cued atom's bytes to a staging file, installs it
    ** into the blob store (dedup via u3_blob_move_stg), and yields a bob
    ** atom — the bytes never touch the loom.  Pass &bsk_u->snk_u to
    ** u3s_cue_xeno_blob(); num_w counts installed blobs.
    */
      typedef struct _u3_blob_bsink {
        u3s_bsink   snk_u;          //  callback table (pass to cue)
        const c3_c* pax_c;          //  pier path
        c3_i        fid_i;          //  staging fd (-1 = closed)
        c3_w        num_w;          //  blobs installed
        c3_c        stg_c[8192];    //  staging file path
      } u3_blob_bsink;

    /* u3_blob_bsink_init(): prepare a sink targeting [pax_c]'s blob store.
    */
      void
      u3_blob_bsink_init(u3_blob_bsink* bsk_u, const c3_c* pax_c);

#endif /* ifndef U3_VERE_BLOB_H */
