#include "v6.h"
#include "options.h"

#ifndef VERE64

static c3_d
_v6_hash(u3_v6_noun foo)
{
  return foo * 11400714819323198485ULL;
}

static c3_i
_v6_cmp(u3_v6_noun a, u3_v6_noun b)
{
  return a == b;
}

#define NAME    _v6_to_v5
#define KEY_TY  u3_v6_noun
#define VAL_TY  u3_noun
#define HASH_FN _v6_hash
#define CMPR_FN _v6_cmp
#include "verstable.h"

typedef struct {
  u3_weak    hed;  // v5 noun head (u3_none if not yet copied)
  u3_v6_noun cel;  // v6 cell being processed
} _copy_frame;

typedef struct {
  _v6_to_v5   map_u;
  c3_w        len_w;
  c3_w        siz_w;
  _copy_frame *tac;
  u3p(u3h_root) ham_p;
} _copy_ctx;

/* _copy_atom(): copy a v6 (64-bit) indirect atom into the v5 (32-bit) loom.
**
**   v6 atoms: len_w counts uint64_t words in buf_w
**   v5 atoms: len_w counts uint32_t words in buf_w
**   conversion: new_len_w = old_len_w * 2, then trim trailing zero uint32_t words
**
**   normalization: v5 cats are 31-bit (≤ 2^31-1); v6 atoms with such small
**   values must become v5 cats, not indirect atoms
*/
static u3_atom
_copy_atom(u3_v6_noun old)
{
  u3a_v6_atom *old_u = (u3a_v6_atom *)u3a_v6_to_ptr(old);
  c3_v6_w      old_w = old_u->len_w;  /* count of uint64_t words */

  /* treat buf_w as uint32_t array (little-endian: low word first) */
  c3_w        *buf_w = (c3_w *)old_u->buf_w;
  c3_w         new_w = (c3_w)(old_w * 2);

  /* trim trailing zero uint32_t words */
  while ( new_w > 0 && 0 == buf_w[new_w - 1] ) {
    new_w--;
  }

  /* normalize to v5 cat if value fits in 31 bits */
  if ( 0 == new_w ) return (u3_noun)0;
  if ( 1 == new_w && buf_w[0] <= u3a_32_direct_max ) {
    return (u3_noun)buf_w[0];
  }

  c3_w     *nov_w = u3a_walloc(new_w + c3_wiseof(u3a_atom));
  u3a_atom *vat_u = (void *)nov_w;

  vat_u->use_w = 1;
  vat_u->mug_h = old_u->mug_h;
  vat_u->len_w = new_w;

  memcpy(vat_u->buf_w, buf_w, (size_t)new_w * sizeof(c3_w));

  return u3a_to_pug(u3a_outa(nov_w));
}

/* _copy_v6_next(): iterative descent into a v6 noun, returning the v5 copy.
**
**   v6 cats with values ≥ 2^31 cannot be v5 cats; they become v5 indirect atoms
*/
static u3_noun
_copy_v6_next(_copy_ctx *cop_u, u3_v6_noun old)
{
  _v6_to_v5_itr vit_u;
  _copy_frame  *top_u;

  while ( 1 ) {
    if ( c3y == u3a_v6_is_cat(old) ) {
      //  direct atom in v6
      if ( old <= u3a_32_direct_max ) {
        return (u3_noun)old;  //  fits in v5 cat
      }
      else {
        //  v6 cat ≥ 2^31: must become a v5 indirect atom
        c3_w lo_w = (c3_w)(old & 0xFFFFFFFFULL);
        c3_w hi_w = (c3_w)(old >> 32);
        c3_w len_w = hi_w ? 2 : 1;

        c3_w     *nov_w = u3a_walloc(len_w + c3_wiseof(u3a_atom));
        u3a_atom *vat_u = (void *)nov_w;

        vat_u->use_w = 1;
        vat_u->mug_h = 0;  //  mug not precomputed for large v6 cats
        vat_u->len_w = len_w;
        vat_u->buf_w[0] = lo_w;
        if ( len_w > 1 ) vat_u->buf_w[1] = hi_w;

        return u3a_to_pug(u3a_outa(nov_w));
      }
    }

    //  indirect noun: check dedup map
    vit_u = vt_get(&(cop_u->map_u), old);

    if ( !vt_is_end(vit_u) ) return u3a_gain(vit_u.data->val);

    if ( c3n == u3a_v6_is_cell(old) ) {
      //  indirect atom
      u3_atom new = _copy_atom(old);
      vit_u = vt_insert( &(cop_u->map_u), old, new );
      u3_assert( !vt_is_end(vit_u) );
      return new;
    }

    //  cell: push head onto stack and recurse into head
    if ( cop_u->len_w == cop_u->siz_w ) {
      cop_u->siz_w += c3_min(cop_u->siz_w, 1024);
      cop_u->tac    = c3_realloc(cop_u->tac, sizeof(*cop_u->tac) * cop_u->siz_w);
    }

    top_u      = &(cop_u->tac[cop_u->len_w++]);
    top_u->hed = u3_none;
    top_u->cel = old;
    old = u3a_v6_head(old);
    continue;
  }
}

static u3_noun
_copy_v6_noun(_copy_ctx *cop_u, u3_v6_noun old)
{
  _v6_to_v5_itr vit_u;
  _copy_frame  *top_u;
  u3_noun        new;

  cop_u->len_w = 0;

  new = _copy_v6_next(cop_u, old);

  while ( cop_u->len_w ) {
    top_u = &(cop_u->tac[cop_u->len_w - 1]);

    if ( u3_none == top_u->hed ) {
      top_u->hed = new;
      new = _copy_v6_next(cop_u, u3a_v6_tail(top_u->cel));
    }
    else {
      new = u3i_cell(top_u->hed, new);
      vit_u = vt_insert( &(cop_u->map_u), top_u->cel, new );
      u3_assert( !vt_is_end(vit_u) );
      cop_u->len_w--;
    }
  }

  return new;
}

static void
_copy_v6_hamt(u3_v6_noun kev, void* ptr_v)
{
  _copy_ctx *cop_u = ptr_v;
  u3_noun key = _copy_v6_noun(cop_u, u3a_v6_head(kev));
  u3_noun val = _copy_v6_noun(cop_u, u3a_v6_tail(kev));
  u3h_put(cop_u->ham_p, key, val);
  u3a_lose(key);
}

void
u3_restore_v5(c3_d eve_d)
{
  _copy_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_v6_load(0);

  if ( eve_d != u3A_v6->eve_d ) {
    fprintf(stderr, "loom: migrate (v5) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_v6->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: 64->32 migration running...\r\n");

  cop_u.siz_w = 32;
  cop_u.tac   = c3_malloc(sizeof(*cop_u.tac) * cop_u.siz_w);
  vt_init(&(cop_u.map_u));

  u3A->eve_d = u3A_v6->eve_d;
  u3A->roc   = _copy_v6_noun(&cop_u, u3A_v6->roc);

  cop_u.ham_p = u3R->jed.cod_p;
  u3h_v6_walk_with(u3R_v6->jed.cod_p, _copy_v6_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.per_p;
  u3h_v6_walk_with(u3R_v6->cax.per_p, _copy_v6_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_boot(c3y);
  u3j_ream();

  vt_cleanup(&cop_u.map_u);

  c3_free(cop_u.tac);

  fprintf(stderr, "loom: 64->32 migration done\r\n");
}

#endif
