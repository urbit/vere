#include "../32/v5.h"
#include "options.h"

#ifdef VERE64

static c3_d
_v5_hash(u3_v5_noun foo)
{
  return (c3_d)foo * 11400714819323198485ULL;
}

static c3_i
_v5_cmp(u3_v5_noun a, u3_v5_noun b)
{
  return a == b;
}

#define NAME    _v5_to_native
#define KEY_TY  u3_v5_noun
#define VAL_TY  u3_noun
#define HASH_FN _v5_hash
#define CMPR_FN _v5_cmp
#include "verstable.h"

typedef struct {
  u3_weak    hed;  // native noun head (u3_none if not yet copied)
  u3_v5_noun cel;  // v5 32-bit cell being processed
} _copy_frame;

typedef struct {
  _v5_to_native map_u;
  c3_w          len_w;
  c3_w          siz_w;
  _copy_frame  *tac;
  u3p(u3h_root) ham_p;
} _copy_ctx;

/* _copy_atom(): copy a v5 32-bit indirect atom into the native 64-bit loom.
**
**   v5 32-bit atoms: len_w counts uint32_t words in buf_w
**   native 64-bit atoms: len_w counts uint64_t words in buf_w
**   conversion: new_len_w = ceil(old_len_w / 2) = (old_len_w + 1) / 2
**
**   normalization: v5 32-bit cats are 31-bit (≤ 2^31-1); native cats are 63-bit
**   (≤ 2^63-1). v5 indirect atoms with values < 2^63 must be returned as native
**   cats, or u3r_comp() will compare raw noun pointers instead of atom values
**   and produce wrong results (indirect-atom pointer has bit 63 set > any cat)
*/
static u3_atom
_copy_atom(u3_v5_noun old)
{
  u3a_v5_atom *old_u    = (u3a_v5_atom *)u3a_v5_to_ptr(old);
  c3_v5_w      old_w    = old_u->len_w;

  /*  normalize small atoms to native cats (values < 2^63 don't need indirection) */
  if ( old_w <= 2 ) {
    c3_d val_d = ( old_w >= 1 ) ? (c3_d)old_u->buf_w[0] : 0;
    if ( 2 == old_w ) val_d |= ((c3_d)old_u->buf_w[1] << 32);
    if ( val_d <= u3a_direct_max ) return (u3_noun)val_d;
  }

  c3_w         new_w    = (old_w + 1) / 2;
  c3_w        *nov_w    = u3a_walloc(new_w + c3_wiseof(u3a_atom));
  u3a_atom    *vat_u    = (void *)nov_w;

  vat_u->use_w = 1;
  vat_u->mug_w = old_u->mug_w;
  vat_u->len_w = new_w;

  if ( new_w ) {
    //  zero last uint64_t word so upper 32 bits are clean when old_w is odd
    vat_u->buf_w[new_w - 1] = 0;
  }
  memcpy(vat_u->buf_w, old_u->buf_w, (size_t)old_w * sizeof(c3_v5_w));

  return u3a_to_pug(u3a_outa(nov_w));
}

static u3_noun
_copy_v5_next(_copy_ctx *cop_u, u3_v5_noun old)
{
  _v5_to_native_itr vit_u;
  _copy_frame      *top_u;

  while ( 1 ) {
    if ( c3y == u3a_v5_is_cat(old) ) return (u3_noun)old;

    vit_u = vt_get(&(cop_u->map_u), old);

    if ( !vt_is_end(vit_u) ) return u3a_gain(vit_u.data->val);

    if ( c3n == u3a_v5_is_cell(old) ) {
      u3_atom new = _copy_atom(old);
      vit_u = vt_insert( &(cop_u->map_u), old, new );
      u3_assert( !vt_is_end(vit_u) );
      return new;
    }

    if ( cop_u->len_w == cop_u->siz_w ) {
      cop_u->siz_w += c3_min(cop_u->siz_w, 1024);
      cop_u->tac    = c3_realloc(cop_u->tac, sizeof(*cop_u->tac) * cop_u->siz_w);
    }

    top_u      = &(cop_u->tac[cop_u->len_w++]);
    top_u->hed = u3_none;
    top_u->cel = old;
    old = u3a_v5_head(old);
    continue;
  }
}

static u3_noun
_copy_v5_noun(_copy_ctx *cop_u, u3_v5_noun old)
{
  _v5_to_native_itr vit_u;
  _copy_frame      *top_u;
  u3_noun            new;

  cop_u->len_w = 0;

  new = _copy_v5_next(cop_u, old);

  while ( cop_u->len_w ) {
    top_u = &(cop_u->tac[cop_u->len_w - 1]);

    if ( u3_none == top_u->hed ) {
      top_u->hed = new;
      new = _copy_v5_next(cop_u, u3a_v5_tail(top_u->cel));
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
_copy_v5_hamt(u3_v5_noun kev, void* ptr_v)
{
  _copy_ctx *cop_u = ptr_v;
  u3_noun key = _copy_v5_noun(cop_u, u3a_v5_head(kev));
  u3_noun val = _copy_v5_noun(cop_u, u3a_v5_tail(kev));
  u3h_put(cop_u->ham_p, key, val);
  u3a_lose(key);
}

void
u3_migrate_64(c3_d eve_d)
{
  _copy_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_v5_load(u3C.wor_i);

  if ( eve_d != u3A_v5->eve_d ) {
    fprintf(stderr, "loom: migrate (to 64-bit) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_v5->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: 32->64 migration running...\r\n");

  cop_u.siz_w = 32;
  cop_u.tac   = c3_malloc(sizeof(*cop_u.tac) * cop_u.siz_w);
  vt_init(&(cop_u.map_u));

  u3A->eve_d = u3A_v5->eve_d;
  u3A->roc   = _copy_v5_noun(&cop_u, u3A_v5->roc);

  cop_u.ham_p = u3R->jed.cod_p;
  u3h_v5_walk_with(u3R_v5->jed.cod_p, _copy_v5_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.per_p;
  u3h_v5_walk_with(u3R_v5->cax.per_p, _copy_v5_hamt, &cop_u);
  cop_u.ham_p = u3R->cax.for_p;
  u3h_v5_walk_with(u3R_v5->cax.for_p, _copy_v5_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_boot(c3y);
  u3j_ream();

  vt_cleanup(&cop_u.map_u);

  c3_free(cop_u.tac);

  fprintf(stderr, "loom: 32->64 migration done\r\n");
}

#endif /* VERE64 */
