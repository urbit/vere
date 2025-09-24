#include "v4.h"
#include "options.h"

static c3_d
_v4_hash(u3_noun foo)
{
  return foo * 11400714819323198485ULL;
}

static c3_i
_v4_cmp(u3_noun a, u3_noun b)
{
  return a == b;
}

#define NAME    _v4_to_v5
#define KEY_TY  u3_noun
#define VAL_TY  u3_noun
#define HASH_FN _v4_hash
#define CMPR_FN _v4_cmp
#include "verstable.h"

typedef struct {
  u3_weak    hed;
  u3_v4_noun cel;
} _copy_frame;

typedef struct {
  _v4_to_v5  map_u;
  c3_w       len_w;
  c3_w       siz_w;
  _copy_frame *tac;
  u3_post    ham_p;
} _copy_ctx;

static u3_atom
_copy_atom(u3_atom old)
{
  u3a_v4_atom *old_u = u3a_v4_to_ptr(old);
  c3_w        *nov_w = u3a_v5_walloc(old_u->len_w + c3_wiseof(u3a_v5_atom));
  u3a_v5_atom *vat_u = (void *)nov_w;

  vat_u->use_w = 1;
  vat_u->mug_w = old_u->mug_w;
  vat_u->len_w = old_u->len_w;

  memcpy(vat_u->buf_w, old_u->buf_w, old_u->len_w << 2);

  return u3a_v5_to_pug(u3a_v5_outa(nov_w));
}

static u3_noun
_copy_v4_next(_copy_ctx *cop_u, u3_noun old)
{
  _v4_to_v5_itr vit_u;
  _copy_frame  *top_u;

  while ( 1 ) {
    if ( c3y == u3a_v4_is_cat(old) ) return old;

    vit_u = vt_get(&(cop_u->map_u), old);

    if ( !vt_is_end(vit_u) ) return u3a_v5_gain(vit_u.data->val);

    if ( c3n == u3a_v4_is_cell(old) ) {
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
    top_u->hed = u3_v4_none;
    top_u->cel = old;
    old = u3a_v4_head(old);
    continue;
  }
}

static u3_noun
_copy_v4_noun(_copy_ctx *cop_u, u3_noun old)
{
  _v4_to_v5_itr vit_u;
  _copy_frame  *top_u;
  u3a_v4_cell  *cel_u;
  u3_noun         new;

  cop_u->len_w = 0;

  new = _copy_v4_next(cop_u, old);

  while ( cop_u->len_w ) {
    top_u = &(cop_u->tac[cop_u->len_w - 1]);

    if ( u3_none == top_u->hed ) {
      top_u->hed = new;
      new = _copy_v4_next(cop_u, u3a_v4_tail(top_u->cel));
    }
    else {
      new = u3i_v5_cell(top_u->hed, new);
      vit_u = vt_insert( &(cop_u->map_u), top_u->cel, new );
      u3_assert( !vt_is_end(vit_u) );
      cop_u->len_w--;
    }
  }
  
  return new;
}

static void
_copy_v4_hamt(u3_noun kev, void* ptr_v)
{
  _copy_ctx *cop_u = ptr_v;
  u3_noun key = _copy_v4_noun(cop_u, u3a_v4_head(kev));
  u3_noun val = _copy_v4_noun(cop_u, u3a_v4_tail(kev));
  u3h_v5_put(cop_u->ham_p, key, val);
  u3a_v5_lose(key);
}

void
u3_migrate_v5(c3_d eve_d)
{
  _copy_ctx cop_u = {0};

  //  XX assumes u3m_init() and u3m_pave(c3y) have already been called

  u3_v4_load(u3C.wor_i);

  if ( eve_d != u3A_v4->eve_d ) {
    fprintf(stderr, "loom: migrate (v5) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3A_v4->eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: allocator migration running...\r\n");

  cop_u.siz_w = 32;
  cop_u.tac   = c3_malloc(sizeof(*cop_u.tac) * cop_u.siz_w);
  vt_init(&(cop_u.map_u));

  //  XX install cel_p temporarily?

  u3A_v5->eve_d = u3A_v4->eve_d;
  u3A_v5->roc   = _copy_v4_noun(&cop_u, u3A_v4->roc);

  cop_u.ham_p = u3R_v5->jed.cod_p;
  u3h_v4_walk_with(u3R_v4->jed.cod_p, _copy_v4_hamt, &cop_u);
  cop_u.ham_p = u3R_v5->cax.per_p;
  u3h_v4_walk_with(u3R_v4->cax.per_p, _copy_v4_hamt, &cop_u);

  //  NB: pave does *not* allocate hot_p
  //
  u3j_v5_boot(c3y);
  u3j_v5_ream();

  vt_cleanup(&cop_u.map_u);

  c3_free(cop_u.tac);

  fprintf(stderr, "loom: allocator migration done\r\n");
}
