/// @file

#ifndef U3_RETRIEVE_H
#define U3_RETRIEVE_H

#include "c3/c3.h"
#include "allocate.h"
#include "error.h"
#include "gmp.h"
#include "types.h"

    /** u3r_*: read without ever crashing.
    **/

      /* u3r_cell(): factor (a) as a cell (b c).
      */
        inline c3_o
        u3r_cell(u3_noun a, u3_noun* b, u3_noun* c)
        {
          u3a_cell* cel_u;

          u3_assert(u3_none != a);

          if ( c3y == u3a_is_cell(a) ) {
            cel_u = u3a_to_ptr(a);
            if ( b ) *b = cel_u->hed;
            if ( c ) *c = cel_u->tel;
            return c3y;
          }
          else {
            return c3n;
          }
        }

      /* u3r_trel(): factor (a) as a trel (b c d).
      */
        inline c3_o
        u3r_trel(u3_noun a, u3_noun *b, u3_noun *c, u3_noun *d)
        {
          u3_noun guf;

          if ( (c3y == u3r_cell(a, b, &guf)) &&
               (c3y == u3r_cell(guf, c, d)) ) {
            return c3y;
          }
          else {
            return c3n;
          }
        }

      /* u3r_qual(): factor (a) as a qual (b c d e).
      */
        inline c3_o
        u3r_qual(u3_noun  a,
                 u3_noun* b,
                 u3_noun* c,
                 u3_noun* d,
                 u3_noun* e)
        {
          u3_noun guf;

          if ( (c3y == u3r_cell(a, b, &guf)) &&
               (c3y == u3r_trel(guf, c, d, e)) ) {
            return c3y;
          }
          else return c3n;
        }

      /* u3r_quil(): factor (a) as a quil (b c d e f).
      */
        inline c3_o
        u3r_quil(u3_noun  a,
                 u3_noun* b,
                 u3_noun* c,
                 u3_noun* d,
                 u3_noun* e,
                 u3_noun* f)
        {
          u3_noun guf;

          if ( (c3y == u3r_cell(a, b, &guf)) &&
               (c3y == u3r_qual(guf, c, d, e, f)) ) {
            return c3y;
          }
          else return c3n;
        }

      /* u3r_hext(): factor (a) as a hext (b c d e f g)
      */
        inline c3_o
        u3r_hext(u3_noun  a,
                 u3_noun* b,
                 u3_noun* c,
                 u3_noun* d,
                 u3_noun* e,
                 u3_noun* f,
                 u3_noun* g)
        {
          u3_noun guf;

          if ( (c3y == u3r_cell(a, b, &guf)) &&
               (c3y == u3r_quil(guf, c, d, e, f, g)) ) {
            return c3y;
          }
          else return c3n;
        }

      /* u3r_at(): fragment `a` of `b`, or u3_none.
      */
        u3_weak
        u3r_at(u3_atom a, u3_weak b);

      /* u3r_mean():
      **
      **   Attempt to deconstruct `a` by axis, noun pairs.
      **   Axes must be sorted in tree order.
      */
        typedef struct {c3_w axe_w; u3_noun* som;} u3r_mean_pair;

        c3_o
        u3r_vmean(u3_noun a, u3r_mean_pair pairs[], c3_z len_z);

#       define u3r_mean(a, ...) ({                                      \
          u3r_mean_pair _pairs[] = {__VA_ARGS__};                       \
          u3r_vmean(a, _pairs, sizeof(_pairs) / sizeof(u3r_mean_pair)); \
        })

      /* u3r_mug_both(): Join two mugs.
      */
        c3_h
        u3r_mug_both(c3_h lef_h, c3_h rit_h);

      /* u3r_mug_bytes(): Compute the mug of `buf`, `len`, LSW first.
      */
        c3_h
        u3r_mug_bytes(const c3_y *buf_y,
                      c3_h        len_h);

      /* u3r_mug_c(): Compute the mug of `a`, LSB first.
      */
        c3_h
        u3r_mug_c(const c3_c *a_c);

      /* u3r_mug_cell(): Compute the mug of the cell `[hed tel]`.
      */
        c3_h
        u3r_mug_cell(u3_noun hed,
                     u3_noun tel);

      /* u3r_mug_chub(): Compute the mug of `num`, LSW first.
      */
        c3_h
        u3r_mug_chub(c3_d num_d);

      /* u3r_mug_words(): 31-bit nonzero MurmurHash3 on raw words.
      */
        c3_h
        u3r_mug_halfs(const c3_h* key_h, c3_w len_w);

      /* u3r_mug_words(): 31-bit nonzero MurmurHash3 on raw words.
      */
        c3_h
        u3r_mug_chubs(const c3_d* key_d, c3_w len_w);

      /* u3r_mug_words(): 31-bit nonzero MurmurHash3 on raw words.
      */
        c3_h
        u3r_mug_words(const c3_w* key_d, c3_w len_w);

      /* u3r_mug(): statefully mug a noun with 31-bit murmur3.
      */
        c3_h
        u3r_mug(u3_noun veb);

      /* u3r_fing():
      **
      **   Yes iff (a) and (b) are the same copy of the same noun.
      **   (Ie, by pointer equality - u3r_sing with false negatives.)
      */
        c3_o
        u3r_fing(u3_noun a,
                 u3_noun b);

      /* u3r_fing_cell():
      **
      **   Yes iff `[p q]` and `b` are the same copy of the same noun.
      */
        c3_o
        u3r_fing_cell(u3_noun p,
                      u3_noun q,
                      u3_noun b);

      /* u3r_fing_mixt():
      **
      **   Yes iff `[p q]` and `b` are the same copy of the same noun.
      */
        c3_o
        u3r_fing_mixt(const c3_c* p_c,
                      u3_noun     q,
                      u3_noun     b);

      /* u3r_fing_trel():
      **
      **   Yes iff `[p q r]` and `b` are the same copy of the same noun.
      */
        c3_o
        u3r_fing_trel(u3_noun p,
                      u3_noun q,
                      u3_noun r,
                      u3_noun b);

      /* u3r_fing_qual():
      **
      **   Yes iff `[p q r s]` and `b` are the same copy of the same noun.
      */
        c3_o
        u3r_fing_qual(u3_noun p,
                      u3_noun q,
                      u3_noun r,
                      u3_noun s,
                      u3_noun b);

      /* u3r_sing(): noun value equality.
      **
      **   Unifies noun pointers on inner roads.
      */
        c3_o
        u3r_sing_imp(u3_noun a, u3_noun b);

        #define u3r_sing(a, b) ({                                               \
          u3_noun __a = a;                                                      \
          u3_noun __b = b;                                                      \
          ( __a == __b ) ? c3y : u3r_sing_imp(__a, __b);                        \
        })

      /* u3r_sing_c(): cord/C-string value equivalence.
      */
        c3_o
        u3r_sing_c(const c3_c* a_c,
                   u3_noun     b);

      /* u3r_sing_cell():
      **
      **   Yes iff `[p q]` and `b` are the same noun.
      */
        c3_o
        u3r_sing_cell(u3_noun p,
                      u3_noun q,
                      u3_noun b);

      /* u3r_sing_mixt():
      **
      **   Yes iff `[p q]` and `b` are the same noun.
      */
        c3_o
        u3r_sing_mixt(const c3_c* p_c,
                      u3_noun     q,
                      u3_noun     b);

      /* u3r_sing_trel():
      **
      **   Yes iff `[p q r]` and `b` are the same noun.
      */
        c3_o
        u3r_sing_trel(u3_noun p,
                      u3_noun q,
                      u3_noun r,
                      u3_noun b);

      /* u3r_sing_qual():
      **
      **   Yes iff `[p q r s]` and `b` are the same noun.
      */
        c3_o
        u3r_sing_qual(u3_noun p,
                      u3_noun q,
                      u3_noun r,
                      u3_noun s,
                      u3_noun b);

      /* u3r_nord():
      **
      **   Return 0, 1 or 2 if `a` is below, equal to, or above `b`.
      */
        u3_atom
        u3r_nord(u3_noun a,
                 u3_noun b);

      /* u3r_mold():
      **
      **   Divide `a` as a mold `[b.[p q] c]`.
      */
        c3_o
        u3r_mold(u3_noun  a,
                 u3_noun* b,
                 u3_noun* c);

      /* u3r_bite(): retrieve/default $bloq and $step from $bite.
      */
        c3_o
        u3r_bite(u3_noun bite, u3_atom* bloq, u3_atom *step);

      /* u3r_p():
      **
      **   & [0] if [a] is of the form [b *c].
      */
        c3_o
        u3r_p(u3_noun  a,
              u3_noun  b,
              u3_noun* c);

      /* u3r_bush():
      **
      **   Factor [a] as a bush [b.[p q] c].
      */
        c3_o
        u3r_bush(u3_noun  a,
                 u3_noun* b,
                 u3_noun* c);

      /* u3r_pq():
      **
      **   & [0] if [a] is of the form [b *c d].
      */
        c3_o
        u3r_pq(u3_noun  a,
               u3_noun  b,
               u3_noun* c,
               u3_noun* d);

      /* u3r_pqr():
      **
      **   & [0] if [a] is of the form [b *c *d *e].
      */
        c3_o
        u3r_pqr(u3_noun  a,
                u3_noun  b,
                u3_noun* c,
                u3_noun* d,
                u3_noun* e);

      /* u3r_pqrs():
      **
      **   & [0] if [a] is of the form [b *c *d *e *f].
      */
        c3_o
        u3r_pqrs(u3_noun  a,
                 u3_noun  b,
                 u3_noun* c,
                 u3_noun* d,
                 u3_noun* e,
                 u3_noun* f);

      /* u3r_met():
      **
      **   Return the size of (b) in bits, rounded up to
      **   (1 << a_y).
      **
      **   For example, (a_y == 3) returns the size in bytes.
      **   NB: (a_y) must be < 37.
      */
        c3_w
        u3r_met(c3_y    a_y,
                u3_atom b);

      /* u3r_bit():
      **
      **   Return bit (a_w) of (b).
      */
        c3_b
        u3r_bit(c3_w    a_w,
                u3_atom b);

      /* u3r_byte():
      **
      **   Return byte (a_w) of (b).
      */
        c3_y
        u3r_byte(c3_w    a_w,
                 u3_atom b);

      /* u3r_bytes():
      **
      **   Copy bytes (a_w) through (a_w + b_w - 1) from (d) to (c).
      */
        void
        u3r_bytes(c3_w    a_w,
                  c3_w    b_w,
                  c3_y*   c_y,
                  u3_atom d);

      /* u3r_bytes_fit():
      **
      **   Copy (len_w) bytes of (a) into (buf_y) if it fits, returning overage.
      */
        c3_w
        u3r_bytes_fit(c3_w    len_w,
                      c3_y*   buf_y,
                      u3_atom a);

      /* u3r_bytes_alloc():
      **
      **  Copy (len_w) bytes starting at (a_w) from (b) into a fresh allocation.
      */
        c3_y*
        u3r_bytes_alloc(c3_w    a_w,
                        c3_w    len_w,
                        u3_atom b);

      /* u3r_bytes_all():
      **
      **  Allocate and return a new byte array with all the bytes of (a),
      **  storing the length in (len_w).
      */
        c3_y*
        u3r_bytes_all(c3_w*   len_w,
                      u3_atom a);

      /* u3r_view: read-only byte view over an atom's significant bytes.
      **
      **   For bob atoms, mmaps the underlying blob file — the caller sees
      **   [byt_y, len_w) without any loom allocation.  For normal atoms,
      **   falls back to malloc + u3r_bytes (same cost as u3r_bytes_all).
      **
      **   Lifecycle: u3r_view_init / ...use byt_y[0..len_w]... /
      **              u3r_view_done.  byt_y is invalid after _done.
      **
      **   len_w matches u3r_met(3, a): significant-byte length (trailing
      **   zero bytes stripped).  For bob atoms this may be less than the
      **   on-disk file size; callers only see the logical bytes.
      */
        typedef struct {
          const c3_y* byt_y;    //  bytes (mmap or heap)
          c3_w        len_w;    //  significant byte length
          c3_d        map_d;    //  mmap size for unmap (0 if heap-backed)
          c3_y*       ali_y;    //  heap allocation to free (0 if mmap-backed)
        } u3r_view;

      /* u3r_view_init(): open a read-only byte view of [a].
      */
        void
        u3r_view_init(u3r_view* vu_u, u3_atom a);

      /* u3r_view_padded(): open a view of at least [wid_w] bytes.
      **
      **   After the call, byt_y[0..wid_w] is valid; len_w == wid_w.
      **   If the atom already has >= wid_w significant bytes we keep
      **   the zero-copy path (mmap for bobs, still cheap for cats
      **   that fit in a single word).  Otherwise we allocate a
      **   wid_w-byte heap buffer, copy what's there, and zero-pad
      **   the rest — exactly the semantics that callers previously
      **   got from u3r_bytes_alloc(0, wid_w, a).
      */
        void
        u3r_view_padded(u3r_view* vu_u, u3_atom a, c3_w wid_w);

      /* u3r_view_done(): release the view's backing memory.
      */
        void
        u3r_view_done(u3r_view* vu_u);

      /* u3r_chop_bits():
      **
      **   XOR `wid_d` bits from`src_w` at `bif_g` to `dst_w` at `bif_g`
      **
      **   NB: [dst_w] must have space for [bit_g + wid_d] bits
      */
        void
        u3r_chop_bits(c3_g  bif_g,
                      c3_d  wid_d,
                      c3_g  bit_g,
                      c3_w* dst_w,
                const c3_w* src_w);

      /* u3r_chop_words():
      **
      **   Into the bloq space of `met`, from position `fum` for a
      **   span of `wid`, to position `tou`, XOR from `src_w`
      **   into `dst_w`.
      **
      **   NB: [dst_w] must have space for [tou_w + wid_w] bloqs
      */
        void
        u3r_chop_words(c3_g  met_g,
                       c3_w  fum_w,
                       c3_w  wid_w,
                       c3_w  tou_w,
                       c3_w* dst_w,
                       c3_w  len_w,
                 const c3_w* src_w);

      /* u3r_chop():
      **
      **   Into the bloq space of `met`, from position `fum` for a
      **   span of `wid`, to position `tou`, XOR from atom `src`
      **   into `dst_w`.
      **
      **   NB: [dst_w] must have space for [tou_w + wid_w] bloqs
      */
        void
        u3r_chop(c3_g  met_g,
                 c3_w  fum_w,
                 c3_w  wid_w,
                 c3_w  tou_w,
                 c3_w* dst_w,
                 u3_atom src);

      /* u3r_mp():
      **
      **   Copy (b) into (a_mp).
      */
        void
        u3r_mp(mpz_t   a_mp,
               u3_atom b);

      /* u3r_short():
      **
      **   Return short (a_w) of (b).
      */
        c3_s
        u3r_short(c3_w  a_w,
                  u3_atom b);

      /* u3r_word():
      **
      **   Return word (a_w) of (b).
      */
        c3_h
        u3r_half(c3_w    a_w,
                 u3_atom b);

      /* u3r_chub():
      **
      **   Return double-word (a_w) of (b).
      */
        c3_d
        u3r_chub(c3_w    a_w,
                 u3_atom b);

      /* u3r_word():
      **
      **   Return double-word (a_w) of (b).
      */
        c3_w
        u3r_word(c3_w    a_w,
                 u3_atom b);


      /* u3r_word_fit():
      **
      **   Fill (out_w) with (a) if it fits, returning success.
      */
        c3_t
        u3r_half_fit(c3_h*   out_w,
                     u3_atom a);

      /* u3r_chub_fit():
      **
      **   Fill (out_w) with (a) if it fits, returning success.
      */
        c3_t
        u3r_chub_fit(c3_d*   out_w,
                     u3_atom a);

      /* u3r_word_fit():
      **
      **   Fill (out_w) with (a) if it fits, returning success.
      */
        c3_t
        u3r_word_fit(c3_w*   out_w,
                     u3_atom a);

      /* u3r_words():
      **
      **  copy words (a_w) through (a_w + b_w - 1) from (d) to (c).
      */
        void
        u3r_halfs(c3_w    a_w,
                  c3_w    b_w,
                  c3_h*   c_w,
                  u3_atom d);

      /* u3r_chubs():
      **
      **  Copy double-words (a_w) through (a_w + b_w - 1) from (d) to (c).
      */
        void
        u3r_chubs(c3_w    a_w,
                  c3_w    b_w,
                  c3_d*   c_d,
                  u3_atom d);


      /* u3r_words():
      **
      **  Copy double-words (a_w) through (a_w + b_w - 1) from (d) to (c).
      */
        void
        u3r_words(c3_w    a_w,
                  c3_w    b_w,
                  c3_w*   c_w,
                  u3_atom d);

      /* u3r_safe_byte(): validate and retrieve byte.
      */
        c3_o
        u3r_safe_byte(u3_noun dat, c3_y* out_y);

      /* u3r_safe_word(): validate and retrieve word.
      */
        c3_o
        u3r_safe_half(u3_noun dat, c3_h* out_w);

      /* u3r_safe_chub(): validate and retrieve chub.
      */
        c3_o
        u3r_safe_chub(u3_noun dat, c3_d* out_d);

      /* u3r_safe_word(): validate and retrieve word.
      */
        c3_o
        u3r_safe_word(u3_noun dat, c3_w* out_w);

      /* u3r_string(): `a`, a text atom, as malloced C string.
      */
        c3_c*
        u3r_string(u3_atom a);

      /* u3r_tape(): `a`, a list of bytes, as malloced C string.
      */
        c3_y*
        u3r_tape(u3_noun a);

      /* u3r_skip():
      **
      **  Extract a constant from a formula, ignoring
      **  safe/static hints, doing no computation.
      */
        u3_weak
        u3r_skip(u3_noun fol);

      /* u3r_safe():
      **
      **  Returns yes if the formula won't crash
      **  and has no hints, returning constant result
      **  if possible
      */
      c3_o
      u3r_safe(u3_noun fol, u3_weak* out);

      /* u3r_word_buffer(): returns word buffer pointer of atom `*a`
      ** and the length of the buffer
      */
      c3_w*
      u3r_word_buffer(u3_atom* a, c3_w* len_w);

      /* u3r_comp(): compares two atoms:
      ** returns 1 if a > b, -1 if a < b, 0 if they are equal
      */
      c3_ys
      u3r_comp(u3_atom a, u3_atom b);

      /* u3r_blob_load(): materialize a bob atom by loading from the blob store.
      **
      **   Returns a normal indirect atom with the blob's bytes, or u3_none on
      **   failure. [pax_c] is the pier path ($pier/).
      **   Does NOT consume [a]; caller must manage refcounts as usual.
      */
      u3_weak
      u3r_blob_load(u3_atom a, const c3_c* pax_c);

      /* u3r_blob_map(): mmap a bob atom's blob file for direct byte access.
      **
      **   Returns a read-only pointer to [*len_d] bytes, or NULL on failure.
      **   Release with u3r_blob_unmap(ptr, *len_d) when done.
      **   Uses u3C.dir_c as the pier path.
      **   No loom allocation is performed.
      */
      const c3_y*
      u3r_blob_map(u3_atom a, c3_d* len_d);

      /* u3r_blob_unmap(): release a mapping from u3r_blob_map().
      */
      void
      u3r_blob_unmap(const c3_y* ptr_y, c3_d len_d);

      /* u3r_blob_met(): compute bit-length of a bob atom without materialization.
      **
      **   Equivalent to u3r_met(0, materialized) but avoids loom allocation.
      **   Scans the last byte to strip trailing zeroes.
      **   Returns 0 on error.
      */
      c3_d
      u3r_blob_met(u3_atom a);

#endif /* ifndef U3_RETRIEVE_H */
