#include "v1.h"
#include "v2.h"
#include "options.h"

static void
_migv2h_rewrite(u3p(u3h_root) har_p);


/***  allocate.c
***/

static u3_noun
_migv2_rewritten_noun(u3_noun som)
{
  if ( c3y == u3a_v2_is_cat(som) ) {
    return som;
  }
  u3_post som_p = u3a_v2_rewritten(u3a_v1_to_off(som));

  if ( c3y == u3a_v2_is_pug(som) ) {
    som_p = u3a_v2_to_pug(som_p);
  }
  else {
    som_p = u3a_v2_to_pom(som_p);
  }

  return som_p;
}

static void
_migv2_rewrite_noun(u3_noun som)
{
  if ( c3n == u3a_v2_is_cell(som) ) {
    return;
  }

  if ( c3n == u3a_v2_rewrite_ptr(u3a_v1_to_ptr((som))) ) return;

  u3a_v2_cell* cel = (u3a_v2_cell*) u3a_v1_to_ptr(som);

  _migv2_rewrite_noun(cel->hed);
  _migv2_rewrite_noun(cel->tel);

  cel->hed = _migv2_rewritten_noun(cel->hed);
  cel->tel = _migv2_rewritten_noun(cel->tel);
}

/* _migv2a_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
*/
void
_migv2a_rewrite_compact(void)
{
  _migv2_rewrite_noun(u3R_v2->ski.gul);
  _migv2_rewrite_noun(u3R_v2->bug.tax);
  _migv2_rewrite_noun(u3R_v2->bug.mer);
  _migv2_rewrite_noun(u3R_v2->pro.don);
  _migv2_rewrite_noun(u3R_v2->pro.day);
  _migv2_rewrite_noun(u3R_v2->pro.trace);
  _migv2h_rewrite(u3R_v2->cax.har_p);

  u3R_v2->ski.gul = _migv2_rewritten_noun(u3R_v2->ski.gul);
  u3R_v2->bug.tax = _migv2_rewritten_noun(u3R_v2->bug.tax);
  u3R_v2->bug.mer = _migv2_rewritten_noun(u3R_v2->bug.mer);
  u3R_v2->pro.don = _migv2_rewritten_noun(u3R_v2->pro.don);
  u3R_v2->pro.day = _migv2_rewritten_noun(u3R_v2->pro.day);
  u3R_v2->pro.trace = _migv2_rewritten_noun(u3R_v2->pro.trace);
  u3R_v2->cax.har_p = u3a_v2_rewritten(u3R_v2->cax.har_p);
}


/***  hashtable.c
***/

/* _migv2h_rewrite_buck(): rewrite buck for compaction.
*/
void
_migv2h_rewrite_buck(u3h_v2_buck* hab_u)
{
  if ( c3n == u3a_v2_rewrite_ptr(hab_u) ) return;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun som = u3h_v2_slot_to_noun(hab_u->sot_w[i_w]);
    hab_u->sot_w[i_w] = u3h_v2_noun_to_slot(_migv2_rewritten_noun(som));
    _migv2_rewrite_noun(som);
  }
}

/* _migv2h_rewrite_node(): rewrite node for compaction.
*/
void
_migv2h_rewrite_node(u3h_v2_node* han_u, c3_w lef_w)
{
  if ( c3n == u3a_v2_rewrite_ptr(han_u) ) return;

  c3_w len_w = c3_pc_w(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v2_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v2_slot_to_noun(sot_w);
      han_u->sot_w[i_w] = u3h_v2_noun_to_slot(_migv2_rewritten_noun(kev));

      _migv2_rewrite_noun(kev);
    }
    else {
      void* hav_v = u3h_v1_slot_to_node(sot_w);
      u3h_v2_node* nod_u = u3v2to(u3h_v2_node, u3a_v2_rewritten(u3v2of(u3h_v2_node,hav_v)));

      han_u->sot_w[i_w] = u3h_v2_node_to_slot(nod_u);

      if ( 0 == lef_w ) {
        _migv2h_rewrite_buck(hav_v);
      } else {
        _migv2h_rewrite_node(hav_v, lef_w);
      }
    }
  }
}

/* _migv2h_rewrite(): rewrite pointers during compaction.
*/
void
_migv2h_rewrite(u3p(u3h_v2_root) har_p)
{
  u3h_v2_root* har_u = u3v2to(u3h_v2_root, har_p);
  c3_w        i_w;

  if ( c3n == u3a_v2_rewrite_ptr(har_u) ) return;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v2_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v2_slot_to_noun(sot_w);
      har_u->sot_w[i_w] = u3h_v2_noun_to_slot(_migv2_rewritten_noun(kev));

      _migv2_rewrite_noun(kev);
    }
    else if ( _(u3h_v2_slot_is_node(sot_w)) ) {
      u3h_v2_node* han_u = (u3h_v2_node*) u3h_v1_slot_to_node(sot_w);
      u3h_v2_node* nod_u = u3v2to(u3h_v2_node, u3a_v2_rewritten(u3v2of(u3h_v2_node,han_u)));

      har_u->sot_w[i_w] = u3h_v2_node_to_slot(nod_u);

      _migv2h_rewrite_node(han_u, 25);
    }
  }
}


/* _migv2j_rewrite_compact(): rewrite jet state for compaction.
 *
 * NB: u3R_v2->jed.han_p *must* be cleared (currently via u3j_v2_reclaim above)
 * since it contains hanks which are not nouns but have loom pointers.
 * Alternately, rewrite the entries with u3h_v2_walk, using u3j_v2_mark as a
 * template for how to walk.  There's an untested attempt at this in git
 * history at e8a307a.
*/
void
_migv2j_rewrite_compact(void)
{
  _migv2h_rewrite(u3R_v2->jed.war_p);
  _migv2h_rewrite(u3R_v2->jed.cod_p);
  _migv2h_rewrite(u3R_v2->jed.han_p);
  _migv2h_rewrite(u3R_v2->jed.bas_p);

  _migv2h_rewrite(u3R_v2->jed.hot_p);
  u3R_v2->jed.hot_p = u3a_v2_rewritten(u3R_v2->jed.hot_p);

  u3R_v2->jed.war_p = u3a_v2_rewritten(u3R_v2->jed.war_p);
  u3R_v2->jed.cod_p = u3a_v2_rewritten(u3R_v2->jed.cod_p);
  u3R_v2->jed.han_p = u3a_v2_rewritten(u3R_v2->jed.han_p);
  u3R_v2->jed.bas_p = u3a_v2_rewritten(u3R_v2->jed.bas_p);
}

/* _migv2n_rewrite_compact(): rewrite the bytecode cache for compaction.
 *
 * NB: u3R_v2->byc.har_p *must* be cleared (currently via u3n_v2_reclaim above),
 * since it contains things that look like nouns but aren't.
 * Specifically, it contains "cells" where the tail is a
 * pointer to a u3a_v2_malloc'ed block that contains loom pointers.
 *
 * You should be able to walk this with u3h_v2_walk and rewrite the
 * pointers, but you need to be careful to handle that u3a_v2_malloc
 * pointers can't be turned into a box by stepping back two words. You
 * must step back one word to get the padding, step then step back that
 * many more words (plus one?).
 */
void
_migv2n_rewrite_compact(void)
{
  _migv2h_rewrite(u3R_v2->byc.har_p);
  u3R_v2->byc.har_p = u3a_v2_rewritten(u3R_v2->byc.har_p);
}

/* _migv2v_rewrite_compact(): rewrite arvo kernel for compaction.
*/
void
_migv2v_rewrite_compact(void)
{
  u3v_v2_arvo* arv_u = &(u3H_v2->arv_u);

  _migv2_rewrite_noun(arv_u->roc);
  _migv2_rewrite_noun(arv_u->now);
  _migv2_rewrite_noun(arv_u->yot);

  arv_u->roc = _migv2_rewritten_noun(arv_u->roc);
  arv_u->now = _migv2_rewritten_noun(arv_u->now);
  arv_u->yot = _migv2_rewritten_noun(arv_u->yot);
}

/* _cm_pack_rewrite(): trace through arena, rewriting pointers.
*/
static void
_cm_pack_rewrite(void)
{
  _migv2v_rewrite_compact();
  _migv2j_rewrite_compact();
  _migv2n_rewrite_compact();
  _migv2a_rewrite_compact();
}

static void
_migrate_reclaim(void)
{
  //  XX update this and similar printfs
  fprintf(stderr, "loom: migration reclaim\r\n");
  u3m_v1_reclaim();
}

static void
_migrate_seek(const u3a_v2_road *rod_u)
{
  /*
    very much like u3a_v2_pack_seek with the following changes:
    - there is no need to account for free space as |pack is performed before
      the migration
    - odd sized boxes will be padded by one word to achieve an even size
    - rut will be moved from one word ahead of u3_Loom to two words ahead
  */
  c3_w *    box_w = u3a_v2_into(rod_u->rut_p);
  c3_w *    end_w = u3a_v2_into(rod_u->hat_p);
  u3_post   new_p = (rod_u->rut_p + 1 + c3_wiseof(u3a_v2_box));
  u3a_v2_box * box_u = (void *)box_w;

  fprintf(stderr, "loom: migration seek\r\n");

  for (; box_w < end_w
         ; box_w += box_u->siz_w
         , box_u = (void*)box_w)
    {
      if (!box_u->use_w)
        continue;
      u3_assert(box_u->siz_w);
      u3_assert(box_u->use_w);
      box_w[box_u->siz_w - 1] = new_p;
      new_p = c3_align(new_p + box_u->siz_w, 2, C3_ALGHI);
    }
}

static void
_migrate_rewrite(void)
{
  fprintf(stderr, "loom: migration rewrite\r\n");

  _cm_pack_rewrite();
}

static void
_migrate_move(u3a_v2_road *rod_u)
{
  fprintf(stderr, "loom: migration move\r\n");

  c3_z hiz_z = u3a_v2_heap(rod_u) * sizeof(c3_w);

  /* calculate required shift distance to prevent write head overlapping read head */
  c3_w  off_w = 1;  /* at least 1 word because u3R_v1->rut_p migrates from 1 to 2 */
  for (u3a_v2_box *box_u = u3a_v2_into(rod_u->rut_p)
         ; (void *)box_u < u3a_v2_into(rod_u->hat_p)
         ; box_u = (void *)((c3_w *)box_u + box_u->siz_w))
    off_w += box_u->siz_w & 1; /* odd-sized boxes are padded by one word */

  /* shift */
  memmove(u3a_v2_into(u3H_v2->rod_u.rut_p + off_w),
          u3a_v2_into(u3H_v2->rod_u.rut_p),
          hiz_z);
  /* manually zero the former rut */
  *(c3_w *)u3a_v2_into(rod_u->rut_p) = 0;

  /* relocate boxes to DWORD-aligned addresses stored in trailing size word */
  c3_w *box_w = u3a_v2_into(rod_u->rut_p + off_w);
  c3_w *end_w = u3a_v2_into(rod_u->hat_p + off_w);
  u3a_v2_box *old_u = (void *)box_w;
  c3_w siz_w = old_u->siz_w;
  u3p(c3_w) new_p = rod_u->rut_p + 1 + c3_wiseof(u3a_v2_box);
  c3_w *new_w;

  for (; box_w < end_w
         ; box_w += siz_w
         , old_u = (void *)box_w
         , siz_w = old_u->siz_w) {
    old_u->use_w &= 0x7fffffff;

    if (!old_u->use_w)
      continue;

    new_w = (void *)u3a_v2_botox(u3a_v2_into(new_p));
    u3_assert(box_w[siz_w - 1] == new_p);
    u3_assert(new_w <= box_w);

    c3_w i_w;
    for (i_w = 0; i_w < siz_w - 1; i_w++)
      new_w[i_w] = box_w[i_w];

    if (siz_w & 1) {
      new_w[i_w++] = 0;         /* pad odd sized boxes */
      new_w[i_w++] = siz_w + 1; /* restore trailing size word */
      new_w[0] = siz_w + 1;     /* and the leading size word */
    }
    else {
      new_w[i_w++] = siz_w;
    }

    new_p += i_w;
  }

  /* restore proper heap state */
  rod_u->rut_p = 2;
  rod_u->hat_p = new_p - c3_wiseof(u3a_v2_box);

  /* like |pack, clear the free lists and cell allocator */
  for (c3_w i_w = 0; i_w < u3a_v2_fbox_no; i_w++)
    u3R_v1->all.fre_p[i_w] = 0;

  u3R_v1->all.fre_w = 0;
  u3R_v1->all.cel_p = 0;
}

void
u3_migrate_v2(c3_d eve_d)
{
  u3_v1_load(u3C.wor_i);

  if ( eve_d != u3H_v1->arv_u.eve_d ) {
    fprintf(stderr, "loom: migrate (v2) stale snapshot: have %"
                    PRIu64 ", need %" PRIu64 "\r\n",
                    u3H_v1->arv_u.eve_d, eve_d);
    abort();
  }

  fprintf(stderr, "loom: pointer compression migration running...\r\n");

  /* perform the migration in a pattern similar to |pack */
  _migrate_reclaim();
  _migrate_seek(&u3H_v1->rod_u);
  _migrate_rewrite();
  _migrate_move(&u3H_v1->rod_u);

  /* finally update the version and commit to disk */
  u3H_v1->ver_w = U3V_VER2;

  fprintf(stderr, "loom: pointer compression migration done\r\n");
}
