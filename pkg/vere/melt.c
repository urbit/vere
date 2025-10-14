#include "allocate.h"
#include "hashtable.h"
#include "jets.h"
#include "options.h"
#include "retrieve.h"
#include "vortex.h"

static c3_d
_melt_hash(u3_noun foo)
{
  return u3r_mug(foo) * 11400714819323198485ULL;
}

static c3_i
_melt_cmp_atoms(u3_atom a, u3_atom b)
{
  if ( a == b ) return 1;

  //  XX assume(c3y == u3a_is_pug(a) && c3y == u3a_is_pug(b))
  u3a_atom *a_u = u3a_to_ptr(a);
  u3a_atom *b_u = u3a_to_ptr(b);

  //  XX assume( a_u->mug_w && b_u->mug_w )
  if ( a_u->mug_w != b_u->mug_w ) return 0;

  if ( a_u->len_w != b_u->len_w ) return 0;

  return 0 == memcmp(a_u->buf_w, b_u->buf_w, a_u->len_w << 2);
}

#define NAME    _coins
#define KEY_TY  u3_atom
#define HASH_FN _melt_hash
#define CMPR_FN _melt_cmp_atoms
#include "verstable.h"

static c3_i
_melt_cmp_cells(u3_cell a, u3_cell b)
{
  if ( a == b ) return 1;

  u3a_cell *a_u = u3a_to_ptr(a);
  u3a_cell *b_u = u3a_to_ptr(b);

  //  XX assume( a_u->mug_w && b_u->mug_w )
  if ( a_u->mug_w != b_u->mug_w ) return 0;

  c3_d *a_d = (c3_d*)&(a_u->hed);
  c3_d *b_d = (c3_d*)&(b_u->hed);

  return *a_d == *b_d;
}

#define NAME    _cells
#define KEY_TY  u3_cell
#define HASH_FN _melt_hash
#define CMPR_FN _melt_cmp_cells
#include "verstable.h"

typedef struct {
  _coins  vat_u;
  _cells  cel_u;
  c3_w    len_w;
  c3_w    siz_w;
  u3_noun *tac;
} _melt_ctx;

static u3_noun
_melt_canon_next(_melt_ctx *can_u, u3_noun som)
{
  _coins_itr vit_u;
  _cells_itr cit_u;

  while ( 1 ) {
    if ( c3y == u3a_is_cat(som) ) return som;

    if ( c3n == u3a_is_cell(som) ) {
      vit_u  = vt_get_or_insert(&can_u->vat_u, som);
      u3_assert( !vt_is_end(vit_u) ); // OOM
      return vit_u.data->key;
    }

    cit_u = vt_get(&can_u->cel_u, som);
    if ( !vt_is_end(cit_u) ) return cit_u.data->key;

    if ( can_u->len_w == can_u->siz_w ) {
      can_u->siz_w += c3_min(can_u->siz_w, 1024);
      can_u->tac    = c3_realloc(can_u->tac, sizeof(*can_u->tac) * can_u->siz_w);
    }

    can_u->tac[can_u->len_w++] = u3a_to_off(som) >> u3a_vits;
    som = u3h(som);
    continue;
  }
}

static inline void __attribute__((always_inline))
_melt_xchange(u3_noun new, u3_noun *som)
{
  u3_noun old = *som;

  if ( old != new ) {
    *som = u3k(new);
    u3z(old);
  }
}

static u3_noun
_melt_canon(_melt_ctx *can_u, u3_noun can)
{
  _cells_itr cit_u;
  u3a_cell  *cel_u;
  u3_noun     *top;

  can_u->len_w = 0;

  can = _melt_canon_next(can_u, can);

  while ( can_u->len_w ) {
    top = &(can_u->tac[can_u->len_w - 1]);

    if ( !(*top >> 31) ) {                       // head frame
      cel_u = u3to(u3a_cell, *top << u3a_vits);
      _melt_xchange(can, &cel_u->hed);
      *top |= 1U << 31;                          // tail frame
      can   = _melt_canon_next(can_u, cel_u->tel);
    }
    else {
      *top  &= (1U << 31) - 1;
      *top <<= u3a_vits;
      cel_u  = u3to(u3a_cell, *top);
      _melt_xchange(can, &cel_u->tel);
      cit_u  = vt_get_or_insert(&can_u->cel_u, u3a_to_pom(*top));
      u3_assert( !vt_is_end(cit_u) ); // OOM
      can    = cit_u.data->key;

      can_u->len_w--;
    }
  }

  return can;
}

static void
_melt_canon_ptr(_melt_ctx *can_u, u3_noun *som)
{
  u3_noun can = _melt_canon(can_u, *som);
  _melt_xchange(can, som);
}

static void
_melt_walk_hamt(u3_noun kev, void* ptr_v)
{
  _melt_ctx *can_u = ptr_v;
  (void)_melt_canon(can_u, kev);
}

c3_w
u3_melt_all(FILE *fil_u)
{
  c3_w     pre_w = u3a_idle(u3R);
  _melt_ctx can_u = {0};

  // Verify that we're on the main road.
  //
  u3_assert( &(u3H->rod_u) == u3R );

  can_u.siz_w = 32;
  can_u.tac   = c3_malloc(sizeof(*can_u.tac) * can_u.siz_w);

  vt_init(&can_u.vat_u);
  vt_init(&can_u.cel_u);

  u3m_reclaim();

  _melt_canon_ptr(&can_u, &(u3A->roc));

  u3h_walk_with(u3R->jed.cod_p, _melt_walk_hamt, &can_u);
  u3h_walk_with(u3R->cax.per_p, _melt_walk_hamt, &can_u);

  u3j_boot(c3n);
  u3j_ream();

  if ( fil_u ) {
    fprintf(fil_u, "atoms (%zu)", vt_size(&can_u.vat_u));
    u3a_print_memory(fil_u, "", (c3_w)vt_bucket_count(&can_u.vat_u));
    fprintf(fil_u, "cells (%zu)", vt_size(&can_u.cel_u));
    u3a_print_memory(fil_u, "", (c3_w)vt_bucket_count(&can_u.cel_u));
  }

  vt_cleanup(&can_u.vat_u);
  vt_cleanup(&can_u.cel_u);

  c3_free(can_u.tac);

  return u3a_idle(u3R) - pre_w;
}

c3_w
u3_meld_all(FILE *fil_u)
{
  c3_w pre_w = u3a_open(u3R);

  u3h_free(u3R->cax.per_p);
  u3R->cax.per_p = u3h_new_cache(u3C.per_w);

  (void)u3_melt_all(fil_u);
  (void)u3m_pack();

  return u3a_open(u3R) - pre_w;
}
