/// @file

#include "pkg/noun/v2/hashtable.h"

#include "pkg/noun/allocate.h"
#include "pkg/noun/v2/allocate.h"
#include "imprison.h"
#include "retrieve.h"
#include "xtract.h"

/* _ch_popcount(): number of bits set in word.  A standard intrinsic.
**             NB: copy of _ch_popcount in pkg/noun/hashtable.c
*/
static c3_w
_ch_popcount(c3_w num_w)
{
  return __builtin_popcount(num_w);
}

/* _ch_free_buck(): free bucket
**              NB: copy of _ch_free_buck in pkg/noun/hashtable.c
*/
static void
_ch_free_buck(u3h_buck* hab_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3z(u3h_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_wfree(hab_u);
}

/* _ch_free_node(): free node.
*/
static void
_ch_free_node(u3h_node* han_u, c3_w lef_w)
{
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3z(u3h_slot_to_noun(sot_w));
    }
    else {
      //  NB: u3h_v2_slot_to_node()
      void* hav_v = u3h_v2_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_free_buck(hav_v);
      } else {
        _ch_free_node(hav_v, lef_w);
      }
    }
  }
  u3a_wfree(han_u);
}

/* u3h_v2_free(): free hashtable.
*/
void
u3h_v2_free(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3z(u3h_slot_to_noun(sot_w));
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_v2_slot_to_node(sot_w);

      _ch_free_node(han_u, 25);
    }
  }
  u3a_wfree(har_u);
}

/* _ch_walk_buck(): walk bucket for gc.
**              NB: copy of _ch_walk_buck in pkg/noun/hashtable.c
*/
static void
_ch_walk_buck(u3h_buck* hab_u, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    fun_f(u3h_slot_to_noun(hab_u->sot_w[i_w]), wit);
  }
}

/* _ch_walk_node(): walk node for gc.
**              NB: copy of _ch_walk_node in pkg/noun/hashtable.c
*/
static void
_ch_walk_node(u3h_node* han_u, c3_w lef_w, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else {
      void* hav_v = u3h_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_v2_walk_with(): traverse hashtable with key, value fn and data
 *                  argument; RETAINS.
*/
void
u3h_v2_walk_with(u3p(u3h_root) har_p,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_v2_slot_to_node(sot_w);

      _ch_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

/* _ch_walk_plain(): use plain u3_noun fun_f for each node
 */
static void
_ch_walk_plain(u3_noun kev, void* wit)
{
  void (*fun_f)(u3_noun) = wit;
  fun_f(kev);
}

/* u3h_v2_walk(): u3h_v2_walk_with, but with no data argument
*/
void
u3h_v2_walk(u3p(u3h_root) har_p, void (*fun_f)(u3_noun))
{
  u3h_v2_walk_with(har_p, _ch_walk_plain, fun_f);
}

/* _ch_rewrite_buck(): rewrite buck for compaction.
*/
void
_ch_rewrite_buck(u3h_buck* hab_u)
{
  if ( c3n == u3a_rewrite_ptr(hab_u) ) return;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun som = u3h_slot_to_noun(hab_u->sot_w[i_w]);
    hab_u->sot_w[i_w] = u3h_noun_to_slot(u3a_v2_rewritten_noun(som));
    u3a_v2_rewrite_noun(som);
  }
}

/* _ch_rewrite_node(): rewrite node for compaction.
*/
void
_ch_rewrite_node(u3h_node* han_u, c3_w lef_w)
{
  if ( c3n == u3a_rewrite_ptr(han_u) ) return;

  c3_w len_w = _ch_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);
      han_u->sot_w[i_w] = u3h_noun_to_slot(u3a_v2_rewritten_noun(kev));

      u3a_v2_rewrite_noun(kev);
    }
    else {
      void* hav_v = u3h_v2_slot_to_node(sot_w);
      u3h_node* nod_u = u3to(u3h_node,u3a_v2_rewritten(u3of(u3h_node,hav_v)));

      if (u3C.migration_state == MIG_REWRITE_COMPRESSED)
        u3C.vits_w = 1;

      han_u->sot_w[i_w] = u3h_v2_node_to_slot(nod_u);

      if (u3C.migration_state == MIG_REWRITE_COMPRESSED)
        u3C.vits_w = 0;

      if ( 0 == lef_w ) {
        _ch_rewrite_buck(hav_v);
      } else {
        _ch_rewrite_node(hav_v, lef_w);
      }
    }
  }
}

/* u3h_v2_rewrite(): rewrite pointers during compaction.
*/
void
u3h_v2_rewrite(u3p(u3h_root) har_p)
{
  u3h_root* har_u = u3to(u3h_root, har_p);
  c3_w        i_w;

  if ( c3n == u3a_rewrite_ptr(har_u) ) return;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_slot_to_noun(sot_w);
      har_u->sot_w[i_w] = u3h_noun_to_slot(u3a_v2_rewritten_noun(kev));

      u3a_v2_rewrite_noun(kev);
    }
    else if ( _(u3h_slot_is_node(sot_w)) ) {
      u3h_node* han_u = u3h_v2_slot_to_node(sot_w);
      u3h_node* nod_u = u3to(u3h_node, u3a_rewritten(u3of(u3h_node,han_u)));

      if (u3C.migration_state == MIG_REWRITE_COMPRESSED)
        u3C.vits_w = 1;

      har_u->sot_w[i_w] = u3h_v2_node_to_slot(nod_u);

      if (u3C.migration_state == MIG_REWRITE_COMPRESSED)
        u3C.vits_w = 0;

      _ch_rewrite_node(han_u, 25);
    }
  }
}
