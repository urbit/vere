/* copy_migrate.h: template for full-noun-copy migrations.
**
**   Each copying migration in pkg/past/ shares the same skeleton:
**     - verstable dedup map keyed by old noun
**     - iterative DFS descent over the source noun with a frame stack
**     - HAMT walker that copies key/val pairs into the target loom
**
**   The shape is pasted into each migration TU by setting the U3C_* macros
**   below and then including this header.  Every macro is undefined
**   at the bottom, matching the verstable.h idiom.
**
**   Do NOT include this header more than once per translation unit.
**
**   Required macros:
**
**     U3C_PREFIX         token for generated symbols  (e.g. v5, v5_64, v4)
**     U3C_OLD_NOUN       source noun type             (u3_v5_noun, ...)
**     U3C_OLD_ATOM_T     source atom struct type      (u3a_v5_atom, ...)
**     U3C_OLD_IS_CAT(s)  cat predicate macro
**     U3C_OLD_IS_CELL(s) cell predicate macro
**     U3C_OLD_TO_PTR(s)  source loom pointer deref
**     U3C_OLD_HEAD(s)    cell head accessor
**     U3C_OLD_TAIL(s)    cell tail accessor
**
**     U3C_ATOM_MODE      one of U3C_ATOM_SAME / U3C_ATOM_32_TO_64 /
**                        U3C_ATOM_64_TO_32
**
**     U3C_NEW_I_CELL     target cell allocator        (u3i_cell / u3i_v5_cell)
**     U3C_NEW_H_PUT      target HAMT put              (u3h_put  / u3h_v5_put)
**     U3C_NEW_A_GAIN     target refcount gain         (u3a_gain / u3a_v5_gain)
**     U3C_NEW_A_LOSE     target refcount lose         (u3a_lose / u3a_v5_lose)
**     U3C_NEW_A_WALLOC   target word allocator
**     U3C_NEW_A_TO_PUG   target indirect-atom tag
**     U3C_NEW_A_OUTA     target outa
**     U3C_NEW_ATOM_T     target atom struct type      (u3a_atom / u3a_v5_atom)
**
**   Optional overrides:
**
**     U3C_COPY_CAT(old)  expression run on cat-tagged source nouns, before
**                        the dedup check.  Default: identity ((u3_noun)(old)).
**                        override when the target cat boundary is narrower
**                        than the source (64 -> 32 must promote large cats
**                        to indirect atoms).
**
**     U3C_OLD_NONE       value used to initialize frame->hed.  Defaults to
**                        u3_none.
**
**   LANDMINE: U3C_NEW_* must point at the allocator family that owns the
**   TARGET loom.  In the v4 -> v5 migration that is the v5 family
**   (u3i_v5_cell, u3h_v5_put, ...) even though the migration runs on 32-bit
**   vere; if you accidentally use the native family here the migration will
**   silently corrupt the in-progress v5 loom.
*/

#ifndef U3C_PREFIX
#  error "copy_migrate.h: U3C_PREFIX must be defined before include"
#endif
#ifndef U3C_OLD_NOUN
#  error "copy_migrate.h: U3C_OLD_NOUN must be defined"
#endif
#ifndef U3C_OLD_ATOM_T
#  error "copy_migrate.h: U3C_OLD_ATOM_T must be defined"
#endif
#ifndef U3C_OLD_IS_CAT
#  error "copy_migrate.h: U3C_OLD_IS_CAT must be defined"
#endif
#ifndef U3C_OLD_IS_CELL
#  error "copy_migrate.h: U3C_OLD_IS_CELL must be defined"
#endif
#ifndef U3C_OLD_TO_PTR
#  error "copy_migrate.h: U3C_OLD_TO_PTR must be defined"
#endif
#ifndef U3C_OLD_HEAD
#  error "copy_migrate.h: U3C_OLD_HEAD must be defined"
#endif
#ifndef U3C_OLD_TAIL
#  error "copy_migrate.h: U3C_OLD_TAIL must be defined"
#endif
#ifndef U3C_ATOM_MODE
#  error "copy_migrate.h: U3C_ATOM_MODE must be defined"
#endif
#ifndef U3C_NEW_I_CELL
#  error "copy_migrate.h: U3C_NEW_I_CELL must be defined"
#endif
#ifndef U3C_NEW_H_PUT
#  error "copy_migrate.h: U3C_NEW_H_PUT must be defined"
#endif
#ifndef U3C_NEW_A_GAIN
#  error "copy_migrate.h: U3C_NEW_A_GAIN must be defined"
#endif
#ifndef U3C_NEW_A_LOSE
#  error "copy_migrate.h: U3C_NEW_A_LOSE must be defined"
#endif
#ifndef U3C_NEW_A_WALLOC
#  error "copy_migrate.h: U3C_NEW_A_WALLOC must be defined"
#endif
#ifndef U3C_NEW_A_TO_PUG
#  error "copy_migrate.h: U3C_NEW_A_TO_PUG must be defined"
#endif
#ifndef U3C_NEW_A_OUTA
#  error "copy_migrate.h: U3C_NEW_A_OUTA must be defined"
#endif
#ifndef U3C_NEW_ATOM_T
#  error "copy_migrate.h: U3C_NEW_ATOM_T must be defined"
#endif

#ifndef U3C_COPY_CAT
#  define U3C_COPY_CAT(old) ((u3_noun)(old))
#endif
#ifndef U3C_OLD_NONE
#  define U3C_OLD_NONE u3_none
#endif

#define U3C_ATOM_SAME       1
#define U3C_ATOM_32_TO_64   2
#define U3C_ATOM_64_TO_32   3

#define U3C_CAT_(a,b) a##b
#define U3C_CAT(a,b)  U3C_CAT_(a,b)
#define U3C_SYM(suf)  U3C_CAT(U3C_CAT(_copy_, U3C_PREFIX), suf)
#define U3C_MAP_TY    U3C_CAT(U3C_CAT(_, U3C_PREFIX), _to_new)
#define U3C_ITR_TY    U3C_CAT(U3C_MAP_TY, _itr)

static c3_d
U3C_SYM(_hash)(U3C_OLD_NOUN foo)
{
  return (c3_d)foo * 11400714819323198485ULL;
}

static c3_i
U3C_SYM(_cmp)(U3C_OLD_NOUN a, U3C_OLD_NOUN b)
{
  return a == b;
}

#define NAME    U3C_MAP_TY
#define KEY_TY  U3C_OLD_NOUN
#define VAL_TY  u3_noun
#define HASH_FN U3C_SYM(_hash)
#define CMPR_FN U3C_SYM(_cmp)
#include "verstable.h"

typedef struct {
  u3_weak       hed;
  U3C_OLD_NOUN  cel;
} U3C_SYM(_frame);

typedef struct {
  U3C_MAP_TY         map_u;
  c3_w               len_w;
  c3_w               siz_w;
  U3C_SYM(_frame)   *tac;
  u3p(u3h_root)      ham_p;
} U3C_SYM(_ctx);

static u3_atom
U3C_SYM(_atom)(U3C_OLD_NOUN old)
{
  U3C_OLD_ATOM_T *old_u = (U3C_OLD_ATOM_T *)U3C_OLD_TO_PTR(old);

#if U3C_ATOM_MODE == U3C_ATOM_SAME
  c3_w            old_w = (c3_w)old_u->len_w;
  c3_w           *nov_w = U3C_NEW_A_WALLOC(old_w + c3_wiseof(U3C_NEW_ATOM_T));
  U3C_NEW_ATOM_T *vat_u = (void *)nov_w;

  vat_u->use_w = 1;
  vat_u->mug_w = old_u->mug_w;
  vat_u->len_w = old_w;

  memcpy(vat_u->buf_w, old_u->buf_w,
         (size_t)old_w * sizeof(old_u->buf_w[0]));

  return U3C_NEW_A_TO_PUG(U3C_NEW_A_OUTA(nov_w));

#elif U3C_ATOM_MODE == U3C_ATOM_32_TO_64
  //  old: uint32_t words, count = old_u->len_w
  //  new: uint64_t words, count = ceil(old_w / 2)
  //
  //  Normalize small atoms to native cats: a v5 indirect atom whose value
  //  fits in 63 bits MUST be a native cat, or u3r_comp() will compare raw
  //  noun pointers instead of atom values (indirect-atom pointer has bit 63
  //  set, which is > any cat).
  //
  c3_w old_w = (c3_w)old_u->len_w;

  if ( old_w <= 2 ) {
    c3_d val_d = ( old_w >= 1 ) ? (c3_d)old_u->buf_w[0] : 0;
    if ( 2 == old_w ) val_d |= ((c3_d)old_u->buf_w[1] << 32);
    if ( val_d <= u3a_direct_max ) return (u3_noun)val_d;
  }

  c3_w            new_w = (old_w + 1) / 2;
  c3_w           *nov_w = U3C_NEW_A_WALLOC(new_w + c3_wiseof(U3C_NEW_ATOM_T));
  U3C_NEW_ATOM_T *vat_u = (void *)nov_w;

  vat_u->use_w = 1;
  vat_u->mug_w = old_u->mug_w;
  vat_u->len_w = new_w;

  if ( new_w ) {
    //  Zero the final uint64 so its upper 32 bits are clean when old_w is odd.
    vat_u->buf_w[new_w - 1] = 0;
  }
  memcpy(vat_u->buf_w, old_u->buf_w,
         (size_t)old_w * sizeof(old_u->buf_w[0]));

  return U3C_NEW_A_TO_PUG(U3C_NEW_A_OUTA(nov_w));

#elif U3C_ATOM_MODE == U3C_ATOM_64_TO_32
  //  old: uint64_t words, count = old_u->len_w
  //  new: uint32_t words, count = old_w * 2, trimmed of trailing zeros
  //
  c3_w  old_w = (c3_w)old_u->len_w;
  c3_w *buf_w = (c3_w *)old_u->buf_w;  // reinterpret as uint32_t array
  c3_w  new_w = old_w * 2;

  while ( new_w > 0 && 0 == buf_w[new_w - 1] ) new_w--;

  //  Normalize to a native 32-bit cat when the result fits.
  //
  if ( 0 == new_w ) return (u3_noun)0;
  if ( 1 == new_w && buf_w[0] <= u3a_direct_max_h ) {
    return (u3_noun)buf_w[0];
  }

  c3_w           *nov_w = U3C_NEW_A_WALLOC(new_w + c3_wiseof(U3C_NEW_ATOM_T));
  U3C_NEW_ATOM_T *vat_u = (void *)nov_w;

  vat_u->use_w = 1;
  vat_u->mug_w = old_u->mug_w;
  vat_u->len_w = new_w;

  memcpy(vat_u->buf_w, buf_w, (size_t)new_w * sizeof(c3_w));

  return U3C_NEW_A_TO_PUG(U3C_NEW_A_OUTA(nov_w));

#else
#  error "copy_migrate.h: unknown U3C_ATOM_MODE"
#endif
}

static u3_noun
U3C_SYM(_next)(U3C_SYM(_ctx) *cop_u, U3C_OLD_NOUN old)
{
  U3C_ITR_TY        vit_u;
  U3C_SYM(_frame)  *top_u;

  while ( 1 ) {
    if ( c3y == U3C_OLD_IS_CAT(old) ) {
      return U3C_COPY_CAT(old);
    }

    vit_u = vt_get(&(cop_u->map_u), old);

    if ( !vt_is_end(vit_u) ) return U3C_NEW_A_GAIN(vit_u.data->val);

    if ( c3n == U3C_OLD_IS_CELL(old) ) {
      u3_atom new = U3C_SYM(_atom)(old);
      vit_u = vt_insert(&(cop_u->map_u), old, new);
      u3_assert( !vt_is_end(vit_u) );
      return new;
    }

    if ( cop_u->len_w == cop_u->siz_w ) {
      cop_u->siz_w += c3_min(cop_u->siz_w, 1024);
      cop_u->tac    = c3_realloc(cop_u->tac,
                                 sizeof(*cop_u->tac) * cop_u->siz_w);
    }

    top_u      = &(cop_u->tac[cop_u->len_w++]);
    top_u->hed = U3C_OLD_NONE;
    top_u->cel = old;
    old        = U3C_OLD_HEAD(old);
    continue;
  }
}

static u3_noun
U3C_SYM(_noun)(U3C_SYM(_ctx) *cop_u, U3C_OLD_NOUN old)
{
  U3C_ITR_TY        vit_u;
  U3C_SYM(_frame)  *top_u;
  u3_noun           new;

  cop_u->len_w = 0;

  new = U3C_SYM(_next)(cop_u, old);

  while ( cop_u->len_w ) {
    top_u = &(cop_u->tac[cop_u->len_w - 1]);

    if ( U3C_OLD_NONE == top_u->hed ) {
      top_u->hed = new;
      new = U3C_SYM(_next)(cop_u, U3C_OLD_TAIL(top_u->cel));
    }
    else {
      new = U3C_NEW_I_CELL(top_u->hed, new);
      vit_u = vt_insert(&(cop_u->map_u), top_u->cel, new);
      u3_assert( !vt_is_end(vit_u) );
      cop_u->len_w--;
    }
  }

  return new;
}

static void
U3C_SYM(_hamt)(U3C_OLD_NOUN kev, void *ptr_v)
{
  U3C_SYM(_ctx) *cop_u = ptr_v;
  u3_noun key = U3C_SYM(_noun)(cop_u, U3C_OLD_HEAD(kev));
  u3_noun val = U3C_SYM(_noun)(cop_u, U3C_OLD_TAIL(kev));
  U3C_NEW_H_PUT(cop_u->ham_p, key, val);
  U3C_NEW_A_LOSE(key);
}

static void
U3C_SYM(_init)(U3C_SYM(_ctx) *cop_u)
{
  cop_u->len_w = 0;
  cop_u->siz_w = 32;
  cop_u->tac   = c3_malloc(sizeof(*cop_u->tac) * cop_u->siz_w);
  vt_init(&(cop_u->map_u));
}

static void
U3C_SYM(_done)(U3C_SYM(_ctx) *cop_u)
{
  vt_cleanup(&(cop_u->map_u));
  c3_free(cop_u->tac);
}

#undef U3C_PREFIX
#undef U3C_OLD_NOUN
#undef U3C_OLD_ATOM_T
#undef U3C_OLD_IS_CAT
#undef U3C_OLD_IS_CELL
#undef U3C_OLD_TO_PTR
#undef U3C_OLD_HEAD
#undef U3C_OLD_TAIL
#undef U3C_ATOM_MODE
#undef U3C_NEW_I_CELL
#undef U3C_NEW_H_PUT
#undef U3C_NEW_A_GAIN
#undef U3C_NEW_A_LOSE
#undef U3C_NEW_A_WALLOC
#undef U3C_NEW_A_TO_PUG
#undef U3C_NEW_A_OUTA
#undef U3C_NEW_ATOM_T
#undef U3C_COPY_CAT
#undef U3C_OLD_NONE

#undef U3C_ATOM_SAME
#undef U3C_ATOM_32_TO_64
#undef U3C_ATOM_64_TO_32

#undef U3C_CAT_
#undef U3C_CAT
#undef U3C_SYM
#undef U3C_MAP_TY
#undef U3C_ITR_TY
