#include "allocate.h"
#include "canon.h"
#include "events.h"
#include "hashtable.h"
#include "jets.h"
#include "options.h"
#include "retrieve.h"
#include "vortex.h"

typedef struct {
  _canon_atoms vat_u;
  _canon_cells cel_u;
  c3_w    len_w;
  c3_w    siz_w;
  u3_noun *tac;
} _melt_ctx;

static u3_noun
_melt_canon_next(_melt_ctx *can_u, u3_noun som)
{
  _canon_atoms_itr vit_u;
  _canon_cells_itr cit_u;

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
  _canon_cells_itr cit_u;
  u3a_cell  *cel_u;
  u3_noun     *top;

  can_u->len_w = 0;

  can = _melt_canon_next(can_u, can);

  while ( can_u->len_w ) {
    top = &(can_u->tac[can_u->len_w - 1]);

    if ( !(*top >> (u3a_word_bits - 1)) ) {                       // head frame
      cel_u = u3to(u3a_cell, *top << u3a_vits);
      _melt_xchange(can, &cel_u->hed);
      *top |= ((c3_w)1) << (u3a_word_bits - 1);                          // tail frame
      can   = _melt_canon_next(can_u, cel_u->tel);
    }
    else {
      *top  &= (((c3_w)1) << (u3a_word_bits - 1)) - 1;
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

  u3z(u3H->rod_u.bug.mer);

  can_u.siz_w = 32;
  can_u.tac   = c3_malloc(sizeof(*can_u.tac) * can_u.siz_w);

  vt_init(&can_u.vat_u);
  vt_init(&can_u.cel_u);

  u3m_reclaim();

  _melt_canon_ptr(&can_u, &(u3A->roc));

  u3h_walk_with(u3R->jed.cod_p, _melt_walk_hamt, &can_u);
  u3h_walk_with(u3R->cax.per_p, _melt_walk_hamt, &can_u);
  u3h_walk_with(u3R->cax.for_p, _melt_walk_hamt, &can_u);

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

  u3H->rod_u.bug.mer = u3i_tape("emergency buffer");

  return u3a_idle(u3R) - pre_w;
}

c3_w
u3_meld_all(FILE *fil_u, c3_o per_o, c3_o for_o)
{
  c3_w pre_w = u3a_open(u3R);
  c3_y pha_y = u3e_Phase;

  u3e_Phase = u3e_pha_pack;

  if ( _(per_o) ) {
    u3h_free(u3R->cax.per_p);
    u3R->cax.per_p = u3h_new_cache(u3C.per_w);
  }

  if ( _(for_o) ) {
    u3h_free(u3R->cax.for_p);
    u3R->cax.for_p = u3h_new_cache(u3C.per_w);
  }

  if ( _(c3a(for_o, per_o)) ) {
    u3h_free(u3R->dup_p);
    u3R->dup_p = u3h_new();
  }

  (void)u3_melt_all(fil_u);
  (void)u3m_pack();

  u3e_Phase = pha_y;
  return u3a_open(u3R) - pre_w;
}
