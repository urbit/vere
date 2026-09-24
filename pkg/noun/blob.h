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

    /* u3_blob_hand: an open blob file, owned by the road that opened it.
    **
    **   Hands are the view layer's: only retrieve.c (behind u3r_view)
    **   and the blob tests open, read, or close one.  Everything else
    **   reads a bob through a u3r_view.
    **
    **   An inner road keeps its hands on a list headed by its bob_p,
    **   allocated in its own heap and deduplicated by blob id; they are
    **   retained until the road falls (u3_blob_drain from u3m_fall), so
    **   a trap that reads one blob many times opens it once.  The home
    **   road never falls and every caller there is C with an explicit
    **   close, so its hands live on the C heap, one per open, freed on
    **   close.  Every mutation runs inside u3m_crit_enter()/
    **   u3m_crit_leave(), so a signal can neither leak an fd nor
    **   interrupt a list.
    */
      typedef struct _u3_blob_hand {
        struct _u3_blob_hand* nex_u;  //  next on the owning road
        struct _u3_blob_hand* pre_u;  //  previous: constant-time unlink
        c3_d   bid_d;   //  (mug_h << 32) | seq_h: inner-road dedup key
        c3_i   fid_i;   //  O_RDONLY fd, open for the hand's lifetime
        c3_w   use_w;   //  live views (inner road: evictable at zero)
        c3_d   len_d;   //  file size: bounds reads, sizes the mapping
        c3_d   met_d;   //  cached bit-length (0 until asked)
        c3_y*  map_y;   //  read-only mapping (0 until u3_blob_data)
        c3_d   map_d;   //  mapping length: u3_blob_hand_pad, or wider
      } u3_blob_hand;

    /* u3_blob_stop(): close every home-road hand (from u3m_stop).
    */
      void
      u3_blob_stop(void);

    /* u3_blob_open(): open a blob on the current road.
    **
    **   An inner road returns its own hand for an already-open blob, or
    **   one held by an inner ancestor.  Returns 0 (without bailing) if
    **   the file is missing or empty.  Out of descriptors, an inner road
    **   evicts its idle hands and, if none are, bails %file; the home
    **   road returns 0.
    */
      u3_blob_hand*
      u3_blob_open(const c3_c* pax_c, c3_h mug_h, c3_h seq_h);

    /* u3_blob_close(): the current road is done with [han_u].
    **
    **   On an inner road the hand stays open for reuse; only its live-view
    **   count drops.  On the home road the fd, mapping, and node go.
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

    /* u3_blob_data(): the file mapped read-only, zero out to [wid_d].
    **
    **   The mapping lives as long as the hand and is at least the file's
    **   pages (u3_blob_hand_pad); bytes past the end of the file read as
    **   zero.  A [wid_d] beyond that adds anonymous zero pages after the
    **   file's, so one pointer covers any width.  A wider request remaps
    **   only while no other view holds the hand (use_w is 1), since a
    **   live view aliases the old pages; otherwise, and on a platform
    **   without fixed mappings, a request past the pad returns 0 and the
    **   caller pads another way.  Returns 0 if the mapping fails.
    */
      const c3_y*
      u3_blob_data(u3_blob_hand* han_u, c3_d wid_d);

    /* u3_blob_hand_pad(): bytes readable through u3_blob_data: the file
    **   length rounded up to the mapping's zero-tail granularity.
    */
      c3_d
      u3_blob_hand_pad(u3_blob_hand* han_u);

    /* u3_blob_hand_met(): bit-length of the blob's content, cached on the hand.
    **
    **   Equivalent to u3r_met(0, atom).  Scans backward from the end of the
    **   file in small windows.  Returns 0 if the content is all zero.
    */
      c3_d
      u3_blob_hand_met(u3_blob_hand* han_u);

    /* u3_blob_drain(): close every hand held by road [rod_v].
    **
    **   Called from u3m_fall: the road's C frames, and with them every
    **   view they held, are gone.  Returns the number closed.
    */
      c3_w
      u3_blob_drain(void* rod_v);

    /* u3_blob_drain_kids(): close every hand held below the home road.
    **
    **   Called after a signal unwinds to the top level, the one path
    **   that skips u3m_fall.  Returns the number closed.
    */
      c3_w
      u3_blob_drain_kids(void);

    /* u3_blob_hands(): hands open on the home road.
    */
      c3_z
      u3_blob_hands(void);

    /* u3_blob_hands_road(): hands open on road [rod_v].  Test support.
    */
      c3_z
      u3_blob_hands_road(void* rod_v);

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
