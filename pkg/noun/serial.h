/// @file

#ifndef U3_SERIAL_H
#define U3_SERIAL_H

#include "c3/c3.h"
#include "imprison.h"
#include "types.h"
    /*  constants
    */
      /* u3_dit_y: digit table for @ux/@uv/@uw.
      */
        extern const c3_y u3s_dit_y[64];

    /*  opaque handles
    */
      /* u3_cue_xeno: handle for cue-ing with an off-loom dictionary.
      */
        typedef struct _u3_cue_xeno u3_cue_xeno;

      /* u3s_bsink: byte sink for streaming large cued atoms off-loom.
      **
      **   when installed on a cue_xeno handle (u3s_cue_xeno_blob), atoms
      **   larger than the threshold are streamed through [wri_f] in
      **   chunks instead of being allocated on the loom; [don_f] returns
      **   the atom to decode in their place (typically a bob atom
      **   referencing a blob-store file built from the bytes).
      */
        typedef struct _u3s_bsink {
          void*    ptr_v;                              //  callback state
          c3_o   (*opn_f)(void*);                      //  begin atom stream
          c3_o   (*wri_f)(void*, const c3_y*, c3_z);   //  append bytes
          u3_weak (*don_f)(void*);                     //  finish -> atom
        } u3s_bsink;

    /*  Noun serialization. All noun arguments RETAINED.
    */

      /* u3s_jam_fib(): jam without atom allocation.
      **
      **   returns atom-suitable words, and *bit_w will have
      **   the length (in bits). return should be freed with u3a_wfree().
      */
        c3_w
        u3s_jam_fib(u3i_slab* sab_u, u3_noun a);

      /* u3s_jam_xeno(): jam with off-loom buffer (re-)allocation.
      */
        c3_d
        u3s_jam_xeno(u3_noun a, c3_d* len_d, c3_y** byt_y);

      /* u3s_cue(): cue [a]
      */
        u3_noun
        u3s_cue(u3_atom a);

      /* u3s_cue_xeno_init_with(): initialize a cue_xeno handle as specified.
      */
        u3_cue_xeno*
        u3s_cue_xeno_init_with(c3_d pre_d, c3_d siz_d);

      /* u3s_cue_xeno_init(): initialize a cue_xeno handle.
      */
        u3_cue_xeno*
        u3s_cue_xeno_init(void);

      /* u3s_cue_xeno_init(): cue on-loom, with off-loom dictionary in handle.
      */
        u3_weak
        u3s_cue_xeno_with(u3_cue_xeno* sil_u,
                          c3_d         len_d,
                          const c3_y*  byt_y);

      /* u3s_cue_xeno_blob(): install a byte sink on a cue_xeno handle.
      **
      **   atoms larger than [thr_d] bytes stream through [snk_u] rather
      **   than materializing on the loom.  [thr_d] of 0 disables.  the
      **   sink must outlive the handle's use.
      */
        void
        u3s_cue_xeno_blob(u3_cue_xeno* sil_u,
                          c3_d         thr_d,
                          u3s_bsink*   snk_u);

      /* u3s_cue_xeno_init(): dispose cue_xeno handle.
      */
        void
        u3s_cue_xeno_done(u3_cue_xeno* sil_u);

      /* u3s_cue_xeno(): cue on-loom, with off-loom dictionary.
      */
        u3_weak
        u3s_cue_xeno(c3_d        len_d,
                     const c3_y* byt_y);

      /* u3s_cue_bytes(): cue bytes onto the loom.
      */
        u3_noun
        u3s_cue_bytes(c3_d len_d, const c3_y* byt_y);

      /* u3s_cue_atom(): cue atom.
      */
        u3_noun
        u3s_cue_atom(u3_atom a);

      /* u3s_etch_ud_smol(): c3_d to @ud
      **
      **   =(26 (met 3 (scot %ud (dec (bex 64)))))
      */
        c3_y*
        u3s_etch_ud_smol(c3_d a_d, c3_y hun_y[26]);

      /* u3s_etch_ud(): atom to @ud.
      */
        u3_atom
        u3s_etch_ud(u3_atom a);

      /* u3s_etch_ud_c(): atom to @ud, as a malloc'd c string.
      */
        size_t
        u3s_etch_ud_c(u3_atom a, c3_c** out_c);

      /* u3s_etch_ux(): atom to @ux.
      */
        u3_atom
        u3s_etch_ux(u3_atom a);

      /* u3s_etch_ux_c(): atom to @ux, as a malloc'd c string.
      */
        size_t
        u3s_etch_ux_c(u3_atom a, c3_c** out_c);

      /* u3s_etch_uv(): atom to @uv.
      */
        u3_atom
        u3s_etch_uv(u3_atom a);

      /* u3s_etch_uv_c(): atom to @uv, as a malloc'd c string.
      */
        size_t
        u3s_etch_uv_c(u3_atom a, c3_c** out_c);

      /* u3s_etch_uw(): atom to @uw.
      */
        u3_atom
        u3s_etch_uw(u3_atom a);

      /* u3s_etch_uw_c(): atom to @uw, as a malloc'd c string.
      */
        size_t
        u3s_etch_uw_c(u3_atom a, c3_c** out_c);

      /* u3s_sift_ud_bytes: parse @ud.
      */
        u3_weak
        u3s_sift_ud_bytes(c3_w len_w, c3_y* byt_y);

      /* u3s_sift_ud: parse @ud.
      */
        u3_weak
        u3s_sift_ud(u3_atom a);

    /*  Ram/Tap — reference-aware serialization that encodes bob atoms.
    **
    **  Ram extends jam with a 2-bit tag scheme:
    **    00 = normal atom   (costs 1 extra bit vs jam)
    **    01 = blob ref      (mat(mug) + mat(seq))
    **    10 = cell
    **    11 = backref
    **
    **  Wire format: [magic "RAM\0" 4B][version 0x01 1B][ram_bits...]
    */

      /* u3s_ram_xeno(): ram with off-loom buffer (re-)allocation.
      **
      **   Encodes bob atoms as 01-tagged blob refs.
      **   Normal atoms (including large non-bob atoms) are 00-tagged.
      **   Does NOT blobify atoms — caller must do that first.
      **
      **   Returns number of bytes written (including 5-byte header).
      **   On error returns 0 and *byt_y is unchanged.
      */
        c3_d
        u3s_ram_xeno(u3_noun a, c3_d* len_d, c3_y** byt_y);

      /* u3s_tap_xeno(): tap on-loom, with off-loom dictionary.
      **
      **   Decodes ram bytes. 01-tagged blob refs become bob atoms.
      **   Returns u3_none on any error.
      */
        u3_weak
        u3s_tap_xeno(c3_d len_d, const c3_y* byt_y);

#endif /* ifndef U3_SERIAL_H */
