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
  ** Mars is the sole writer.  A client such as the king never touches
  ** bob/<mug>/<seq>: it writes bytes to a staging file (u3b_stage,
  ** u3b_stage_fd) and sends the path in a %blob writ; mars installs
  ** it (u3b_move_stg), leases it, and acks with the mug and seq.
  */

  /* U3_BLOB_THRESH: atoms larger than this (in bytes) are blobified.
  */
#   define U3_BLOB_THRESH  (32ULL * 1024ULL * 1024ULL)

  /* u3b_over(): true if an atom of [len_d] bytes is blobified at ingress.
  **
  ** The one comparison every ingress point uses: strictly over the
  ** threshold, matching the streaming cue.
  */
    static inline c3_o
    u3b_over(c3_d len_d)
    {
      return ( len_d > U3_BLOB_THRESH ) ? c3y : c3n;
    }

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

    /* u3b_bob_dir(): write the $pier/.urb/bob path into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.  Pier setup (disk.c) creates this
    ** directory; the blob store only reads/writes files beneath it.
    */
      void
      u3b_bob_dir(c3_c* out_c, const c3_c* pax_c);

    /* u3b_stg_dir(): write the $pier/.urb/bob/stg staging path into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.
    */
      void
      u3b_stg_dir(c3_c* out_c, const c3_c* pax_c);

    /* u3b_stage(): write [len_d] bytes to a new staging file.
    **
    ** Sets [stg_c] (at least 8192 bytes) to the file's path, for a
    ** %blob writ.  On failure the file is gone and c3n is returned.
    */
      c3_o
      u3b_stage(const c3_c* pax_c,
                    const c3_y* dat_y,
                    c3_d        len_d,
                    c3_c*       stg_c);

    /* u3b_stage_fd(): copy [len_d] bytes from [fid_i] to a new
    **   staging file, as u3b_stage.
    */
      c3_o
      u3b_stage_fd(const c3_c* pax_c,
                       c3_i        fid_i,
                       c3_d        len_d,
                       c3_c*       stg_c);

    /* u3b_live(): check whether a blob file exists.
    */
      c3_o
      u3b_live(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3b_wipe(): delete a blob file.
    **
    ** Called when a bob atom's total refcount reaches zero.
    */
      void
      u3b_wipe(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3b_walk(): enumerate every blob file in the store.
    **
    ** Calls [fun_f] with (mug_h, seq_h) for each $pier/.urb/bob/<mug>/<seq>
    ** file on disk.  Skips the staging dir and bucket lockfiles.  The
    ** callback must not create or delete blob files (collect, then act).
    */
      void
      u3b_walk(const c3_c* pax_c,
                     void* ptr_v,
                     void  (*fun_f)(void*, c3_h, c3_h));

    /* u3b_move_stg(): install a staging file into the blob store.
    **
    ** [stg_c] is the path of a temp file in $pier/.urb/bob/stg/.
    ** Computes mug, deduplicates, then rename(2)s into bob/<mug>/<seq>.
    ** The staging file is always consumed on success.
    ** On success, returns c3y and sets *mug_h and *seq_h.
    */
      c3_o
      u3b_move_stg(const c3_c* pax_c,
                   const c3_c* stg_c,
                         c3_h* mug_h,
                         c3_h* seq_h);

    /* u3b_path(): write filesystem path for a blob into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.
    */
      void
      u3b_path(c3_c* out_c, const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3b_hand: an open blob file handle, owned by the road that
    **           opened it; exclusively accessed publicly via views.
    */
      typedef struct _u3b_hand {
        struct _u3b_hand* nex_u;  //  next on the owning road
        struct _u3b_hand* pre_u;  //  previous: constant-time unlink
        c3_d   bid_d;   //  (mug_h << 32) | seq_h: inner-road dedup key
        c3_i   fid_i;   //  O_RDONLY fd, open for the hand's lifetime
        c3_w   use_w;   //  live views (inner road: evictable at zero)
        c3_d   len_d;   //  file size: bounds reads, sizes the mapping
        c3_d   bit_d;   //  bit length of the content, from the file's tail at open
        c3_y*  map_y;   //  read-only mapping of the file (0 until u3b_mmap)
        c3_y*  cax_y;   //  window cache, C heap (0 until a small read)
        c3_d   cax_d;   //  file offset of the cached window
        c3_z   cax_z;   //  bytes valid in it
        c3_z   win_z;   //  next fill size: grows while reads stay adjacent
      } u3b_hand;

    /* u3b_open(): open a blob on the current road.
    **
    **   An inner road returns its own hand for an already-open blob, or
    **   one held by an inner ancestor.  Returns 0 (without bailing) if
    **   the file is missing, empty, or all zero.  Out of descriptors, an inner road
    **   evicts its idle hands and, if none are, bails %file; the home
    **   road returns 0.
    */
      u3b_hand*
      u3b_open(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3b_shut(): the current road is done with [han_u].
    **
    **   On an inner road the hand stays open for reuse; only its live-view
    **   count drops.  On the home road the fd, mapping, and node go.
    */
      void
      u3b_shut(u3b_hand* han_u);

    /* u3b_read(): read [len_z] bytes at [off_d] into [dst_y].
    **
    **   Returns the number of bytes read; short only at end of file or
    **   on error.  A read of at most BLOB_CAX_MAX bytes is served from
    **   the hand's window cache, filled by one pread per window, so a
    **   word-at-a-time walk costs one syscall per window rather than
    **   per word.  Never allocates on the loom.
    */
      c3_z
      u3b_read(u3b_hand* han_u, c3_d off_d, c3_y* dst_y, c3_z len_z);

    /* u3b_mmap(): the file mapped read-only.
    **
    **   The mapping lives as long as the hand.  The rest of its last
    **   page reads as zero, so a reader may run past the file's end to
    **   the next word boundary.  Returns 0 if the mapping fails or the
    **   file was shortened under the hand.
    */
      const c3_y*
      u3b_mmap(u3b_hand* han_u);

    /* u3b_stop(): close every home-road hand (from u3m_stop).
    */
      void
      u3b_stop(void);

    /* u3b_drain(): close every hand held by road [rod_v].
    **
    **   Called from u3m_fall: the road's C frames, and with them every
    **   view they held, are gone.  Returns the number closed.
    */
      c3_w
      u3b_drain(void* rod_v);

    /* u3b_drain_kids(): close every hand held below the home road.
    **
    **   Called after a signal unwinds to the top level, the one path
    **   that skips u3m_fall.  Returns the number closed.
    */
      c3_w
      u3b_drain_kids(void);

    /* u3b_hands(): hands open on the home road.
    */
      c3_z
      u3b_hands(void);

    /* u3b_hands_road(): hands open on road [rod_v].  Test support.
    */
      c3_z
      u3b_hands_road(void* rod_v);

    /* u3b_bsink: streaming byte sink for blob-aware cue (u3s_bsink).
    **
    ** Streams a large cued atom's bytes to a staging file, installs it
    ** into the blob store (dedup via u3b_move_stg), and yields a bob
    ** atom — the bytes never touch the loom.  Pass &bsk_u->snk_u to
    ** u3s_cue_xeno_blob(); num_w counts installed blobs.
    */
      typedef struct _u3b_bsink {
        u3s_bsink   snk_u;          //  callback table (pass to cue)
        const c3_c* pax_c;          //  pier path
        c3_i        fid_i;          //  staging fd (-1 = closed)
        c3_w        num_w;          //  blobs installed
        c3_c        stg_c[8192];    //  staging file path
      } u3b_bsink;

    /* u3b_bsink_init(): prepare a sink targeting [pax_c]'s blob store.
    */
      void
      u3b_bsink_init(u3b_bsink* bsk_u, const c3_c* pax_c);

#endif /* ifndef U3_VERE_BLOB_H */
