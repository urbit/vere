/// @file

#ifndef U3_NOUN_BLOB_H
#define U3_NOUN_BLOB_H

#include "c3/c3.h"
#include "types.h"
#include "serial.h"

  /* Blob store: content-addressed storage for large atoms.
  **
  ** An epoch owns its blob files: they live in
  ** $pier/.urb/log/0i<epoch>/bob/<mug>/<seq>, the store root of that
  ** epoch (u3b_root), with <mug> in hex (U3B_MUG_FMT) and <seq> in
  ** decimal.  Every store function takes a root; a process
  ** keeps the current one in u3C.bob_c, and an unset root declines.
  **
  ** Mars is the sole writer, so the next seq in a bucket is one past
  ** the highest there.  A client such as the king never touches
  ** <root>/<mug>/<seq>.  It stages bytes under $pier/.urb/stg
  ** (u3b_stage, u3b_stage_fd), which report the content mug; puts a
  ** stub, u3i_blob of that mug with seq 0, where the bob belongs in
  ** its event; and sends the staging path with the event.  Mars
  ** installs the file (u3b_move_stg) and puts the real bob in the
  ** stub's place before it computes the event.
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

  /* U3B_MUG_FMT: a bucket's name: its mug as eight lowercase hex digits,
  **   as U3A_BID_FMT prints the mug in logs.
  */
#   define U3B_MUG_FMT  "%08" PRIx32

    /* u3b_root(): write the store root of epoch [epo_d] in pier [pax_c]
    **   into [out_c]: $pier/.urb/log/0i<epo>/bob.
    **
    ** [out_c] must be at least 8192 bytes.  The epoch code (disk.c)
    ** creates the directory and points u3C.bob_c at it.
    */
      void
      u3b_root(c3_c* out_c, const c3_c* pax_c, c3_d epo_d);

    /* u3b_stg_dir(): write the $pier/.urb/stg staging path into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.
    */
      void
      u3b_stg_dir(c3_c* out_c, const c3_c* pax_c);

    /* u3b_stage(): write [len_d] bytes to a new staging file.
    **
    ** Sets [stg_c] (at least 8192 bytes) to the file's path, which
    ** rides with the event that installs it, and [mug_h] to the content
    ** mug, which the event's stub carries.  Content with no bob
    ** representation (under U3_BLOB_MIN significant bytes) is refused.
    ** On failure the file is gone and c3n is returned.
    */
      c3_o
      u3b_stage(const c3_c* pax_c,
                    const c3_y* dat_y,
                    c3_d        len_d,
                    c3_c*       stg_c,
                c3_h*       mug_h);

    /* u3b_stage_fd(): copy [len_d] bytes from [fid_i] to a new
    **   staging file, as u3b_stage.
    */
      c3_o
      u3b_stage_fd(const c3_c* pax_c,
                       c3_i        fid_i,
                       c3_d        len_d,
                       c3_c*       stg_c,
                   c3_h*       mug_h);

    /* u3b_mug_fd(): the content mug of the file open on [fid_i], as an
    **   install would measure it, without copying it; c3n if it has no
    **   bob representation or cannot be read.
    */
      c3_o
      u3b_mug_fd(c3_i fid_i, c3_h* mug_h);

    /* u3b_live(): check whether a blob file exists.
    */
      c3_o
      u3b_live(const c3_c* bob_c, c3_h mug_h, c3_h seq_h);

    /* u3b_wipe(): delete a blob file.
    **
    ** Called when a bob atom's total refcount reaches zero.
    */
      void
      u3b_wipe(const c3_c* bob_c, c3_h mug_h, c3_h seq_h);

    /* u3b_walk(): enumerate every blob file in the store.
    **
    ** Calls [fun_f] with (mug_h, seq_h) for each <root>/<mug>/<seq>
    ** file on disk.  Skips any name u3b_path would not write.  The
    ** callback must not create or delete blob files (collect, then act).
    */
      void
      u3b_walk(const c3_c* bob_c,
                     void* ptr_v,
                     void  (*fun_f)(void*, c3_h, c3_h));

    /* u3b_link(): carry blob (mug, seq) from store [old_c] into store
    **   [new_c]: a hard link, or a copy where links are unavailable.
    */
      c3_o
      u3b_link(const c3_c* old_c, const c3_c* new_c, c3_h mug_h, c3_h seq_h);

    /* u3b_kill(): remove the store root [bob_c] and everything in it,
    **   for an epoch that is being deleted.  A missing root is fine.
    */
      void
      u3b_kill(const c3_c* bob_c);

    /* u3b_move_stg(): install a staging file into the blob store.
    **
    ** [stg_c] is the path of a temp file in $pier/.urb/stg/.
    ** Computes mug, deduplicates, then rename(2)s into <root>/<mug>/<seq>.
    ** The staging file is always consumed on success.
    ** On success, returns c3y and sets *mug_h and *seq_h.
    */
      c3_o
      u3b_move_stg(const c3_c* bob_c,
                   const c3_c* stg_c,
                         c3_h* mug_h,
                         c3_h* seq_h);

    /* u3b_path(): write filesystem path for a blob into [out_c].
    **
    ** [out_c] must be at least 8192 bytes.
    */
      void
      u3b_path(c3_c* out_c, const c3_c* bob_c, c3_h mug_h, c3_h seq_h);

    /* Reads.  Every read of a blob goes through a read-only mapping that
    **   one view owns (u3r_view, retrieve.h).  A mapping holds the
    **   file's bytes, then zeros: to at least one page past the file, or
    **   to the width asked for.  No fd outlives the call that maps.  An
    **   inner road lists its mappings in its fla_p slot, records on its
    **   own heap, so its fall or a signal reset releases any a bail left;
    **   the home road lists none.  See blob.c.
    */

    /* u3b_flat: a mapping.  On an inner road the record lives on the
    **   road's heap and is listed from the road's fla_p slot; on the
    **   home road it lives in the view that owns the mapping.
    */
      typedef struct _u3b_flat {
        u3_post nex_p;    //  next record on its inner road's list
        c3_y*   map_y;    //  base: the content's bytes, then zeros
        c3_d    ext_d;    //  readable extent, in bytes
        c3_d    hed_d;    //  windows: bytes of the file view at map_y
      } u3b_flat;

    /* u3b_map(): map blob [bid_d] read-only into [fla_u]: its bytes,
    **   then zeros to [wid_d] and at least one page past them.
    **
    **   Sets [bit_d] to the content's bit length.  On failure returns c3n
    **   and sets [mot_m]: c3__file for a store fault (an unset root, a
    **   missing, empty, or non-canonical file, a failed open or map),
    **   c3__meme when the address space cannot hold the extent.  On an
    **   inner road [fla_u] must be a record on the road's heap, which a
    **   success lists.
    */
      c3_o
      u3b_map(u3b_flat*   fla_u,
              const c3_c* bob_c,
              c3_d        bid_d,
              c3_d        wid_d,
              c3_d*       bit_d,
              c3_m*       mot_m);

    /* u3b_pad(): map a read-only anonymous copy of [len_d] bytes at
    **   [src_y] into [fla_u], zero to [wid_d].  c3n if the address space
    **   cannot hold it.  Listed as u3b_map lists.
    */
      c3_o
      u3b_pad(u3b_flat* fla_u, const c3_y* src_y, c3_d len_d, c3_d wid_d);

    /* u3b_unmap(): release [fla_u], on the road that mapped it, and
    **   unlist it on an inner road.  The record's memory is the
    **   caller's.  A no-op on an empty record.
    */
      void
      u3b_unmap(u3b_flat* fla_u);

    /* u3b_drain(): release every mapping road [rod_v] lists (u3m_fall).
    **   The records go with the road's memory.
    */
      void
      u3b_drain(void* rod_v);

    /* u3b_drain_kids(): release every mapping every road above home
    **   lists: a signal unwound them all, skipping u3m_fall.
    */
      void
      u3b_drain_kids(void);

    /* u3b_mark(): mark the current road's mapping records for gc; words
    **   marked.
    */
      c3_w
      u3b_mark(void);

    /* u3b_maps(): mappings the process holds.
    */
      c3_z
      u3b_maps(void);

    /* u3b_maps_road(): mappings road [rod_v] lists.  Test support.
    */
      c3_z
      u3b_maps_road(void* rod_v);

    /* u3b_mapped(): bytes the process holds mapped.
    */
      c3_d
      u3b_mapped(void);

    /* u3b_bsink: streaming byte sink for blob-aware cue (u3s_bsink).
    **
    ** Streams a large cued atom's bytes to a staging file, installs it
    ** into the blob store (dedup via u3b_move_stg), and yields a bob
    ** atom — the bytes never touch the loom.  Pass &bsk_u->snk_u to
    ** u3s_cue_xeno_blob(); num_w counts installed blobs.
    */
      typedef struct _u3b_bsink {
        u3s_bsink   snk_u;          //  callback table (pass to cue)
        const c3_c* pax_c;          //  pier path (staging)
        const c3_c* bob_c;          //  store root (install)
        c3_i        fid_i;          //  staging fd (-1 = closed)
        c3_w        num_w;          //  blobs installed
        c3_c        stg_c[8192];    //  staging file path
      } u3b_bsink;

    /* u3b_bsink_init(): prepare a sink staging in pier [pax_c] and
    **   installing into the store root [bob_c].
    */
      void
      u3b_bsink_init(u3b_bsink* bsk_u, const c3_c* pax_c, const c3_c* bob_c);

#endif /* ifndef U3_VERE_BLOB_H */
