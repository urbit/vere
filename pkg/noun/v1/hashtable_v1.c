/// @file

#include "pkg/noun/hashtable.h"
#include "pkg/noun/v1/hashtable.h"

#include "pkg/noun/allocate.h"
#include "pkg/noun/v1/allocate.h"


/* _ch_v1_popcount(): number of bits set in word.  A standard intrinsic.
**             NB: copy of _ch_v1_popcount in pkg/noun/hashtable.c
*/
static c3_w
_ch_v1_popcount(c3_w num_w)
{
  return __builtin_popcount(num_w);
}

/* _ch_v1_free_buck(): free bucket
**              NB: copy of _ch_v1_free_buck in pkg/noun/hashtable.c
*/
static void
_ch_v1_free_buck(u3h_v1_buck* hab_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3a_v1_lose(u3h_v1_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_v1_wfree(hab_u);
}

/* _ch_v1_free_node(): free node.
*/
static void
_ch_v1_free_node(u3h_v1_node* han_u, c3_w lef_w)
{
  c3_w len_w = _ch_v1_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3a_v1_lose(u3h_v1_slot_to_noun(sot_w));
    }
    else {
      void* hav_v = u3h_v1_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v1_free_buck(hav_v);
      } else {
        _ch_v1_free_node(hav_v, lef_w);
      }
    }
  }
  u3a_v1_wfree(han_u);
}

/* u3h_v1_free_nodes(): free hashtable nodes.
*/
void
u3h_v1_free_nodes(u3p(u3h_v1_root) har_p)
{
  u3h_v1_root* har_u = u3to(u3h_v1_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3a_v1_lose(u3h_v1_slot_to_noun(sot_w));
    }
    else if ( _(u3h_v1_slot_is_node(sot_w)) ) {
      u3h_v1_node* han_u = (u3h_v1_node*) u3h_v1_slot_to_node(sot_w);

      _ch_v1_free_node(han_u, 25);
    }
    har_u->sot_w[i_w] = 0;
  }
  har_u->use_w       = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;
}

/* _ch_v1_walk_buck(): walk bucket for gc.
**              NB: copy of _ch_v1_walk_buck in pkg/noun/hashtable.c
*/
static void
_ch_v1_walk_buck(u3h_v1_buck* hab_u, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    fun_f(u3h_v1_slot_to_noun(hab_u->sot_w[i_w]), wit);
  }
}

/* _ch_v1_walk_node(): walk node for gc.
*/
static void
_ch_v1_walk_node(u3h_v1_node* han_u, c3_w lef_w, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w len_w = _ch_v1_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v1_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else {
      void* hav_v = u3h_v1_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v1_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_v1_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_v1_walk_with(): traverse hashtable with key, value fn and data
 *                  argument; RETAINS.
*/
void
u3h_v1_walk_with(u3p(u3h_v1_root) har_p,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  u3h_v1_root* har_u = u3to(u3h_v1_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v1_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else if ( _(u3h_v1_slot_is_node(sot_w)) ) {
      u3h_v1_node* han_u = (u3h_v1_node*) u3h_v1_slot_to_node(sot_w);

      _ch_v1_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

/* _ch_v1_walk_plain(): use plain u3_noun fun_f for each node
 */
static void
_ch_v1_walk_plain(u3_noun kev, void* wit)
{
  void (*fun_f)(u3_noun) = wit;
  fun_f(kev);
}

/* u3h_v1_walk(): u3h_v1_walk_with, but with no data argument
*/
void
u3h_v1_walk(u3p(u3h_v1_root) har_p, void (*fun_f)(u3_noun))
{
  u3h_v1_walk_with(har_p, _ch_v1_walk_plain, fun_f);
}
