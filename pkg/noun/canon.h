/// @file
///
/// Off-loom hash-consing tables, shared by u3m_dedup() and melt.
///
/// Members are canonical nouns, and the children of a member cell are
/// members, so cells compare by mug and child pointers alone: a cell is
/// looked up only once its children are canonical. Atoms compare by
/// value. The tables hold no references.
///
/// NB: instantiates static functions in each including translation unit.

#ifndef U3_CANON_H
#define U3_CANON_H

#include "c3/c3.h"
#include "allocate.h"
#include "retrieve.h"

/* _canon_hash(): table hash, from the (cached) mug.
*/
static inline c3_d
_canon_hash(u3_noun som)
{
  return u3r_mug(som) * 11400714819323198485ULL;
}

/* _canon_cmp_atoms(): compare indirect atoms by value.
*/
static inline c3_i
_canon_cmp_atoms(u3_atom a, u3_atom b)
{
  if ( a == b ) return 1;

  //  XX assume(c3y == u3a_is_pug(a) && c3y == u3a_is_pug(b))
  u3a_atom *a_u = u3a_to_ptr(a);
  u3a_atom *b_u = u3a_to_ptr(b);

  //  XX assume( a_u->mug_w && b_u->mug_w )
  if ( a_u->mug_w != b_u->mug_w ) return 0;

  if ( a_u->len_w != b_u->len_w ) return 0;

  //  a bob's body is its blob id, so the same compare serves both
  //  kinds; the mask drops the flag from len_w.  unifying duplicate
  //  bobs frees them via u3z, and _me_bob_dead takes each off its
  //  record, as each was counted at its own u3i_blob.
  //
  c3_w len_w = a_u->len_w & u3a_blob_mask;

  return 0 == memcmp(a_u->buf_w, b_u->buf_w, len_w << (u3a_word_bits_log-3));
}

#define NAME    _canon_atoms
#define KEY_TY  u3_atom
#define HASH_FN _canon_hash
#define CMPR_FN _canon_cmp_atoms
#include "verstable.h"

/* _canon_cmp_cells(): compare cells with canonical children.
*/
static inline c3_i
_canon_cmp_cells(u3_cell a, u3_cell b)
{
  if ( a == b ) return 1;

  u3a_cell *a_u = u3a_to_ptr(a);
  u3a_cell *b_u = u3a_to_ptr(b);

  //  XX assume( a_u->mug_w && b_u->mug_w )
  if ( a_u->mug_w != b_u->mug_w ) return 0;

  return a_u->hed == b_u->hed && a_u->tel == b_u->tel;
}

#define NAME    _canon_cells
#define KEY_TY  u3_cell
#define HASH_FN _canon_hash
#define CMPR_FN _canon_cmp_cells
#include "verstable.h"

#endif /* ifndef U3_CANON_H */
