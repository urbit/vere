/// @file

#include "pkg/noun/hashtable.h"
#include "pkg/noun/v1/hashtable.h"
#include "pkg/noun/v2/hashtable.h"

#include "pkg/noun/allocate.h"
#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v2/allocate.h"

/* _ch_v2_popcount(): number of bits set in word.  A standard intrinsic.
**             NB: copy of _ch_v2_popcount in pkg/noun/hashtable.c
*/
static c3_w
_ch_v2_popcount(c3_w num_w)
{
  return __builtin_popcount(num_w);
}

/* _ch_v2_free_buck(): free bucket
**              NB: copy of _ch_v2_free_buck in pkg/noun/hashtable.c
*/
static void
_ch_v2_free_buck(u3h_v2_buck* hab_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3z(u3h_v2_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_v2_wfree(hab_u);
}

/* _ch_v2_free_node(): free node.
*/
static void
_ch_v2_free_node(u3h_v2_node* han_u, c3_w lef_w)
{
  c3_w len_w = _ch_v2_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v2_slot_is_noun(sot_w)) ) {
      u3z(u3h_v2_slot_to_noun(sot_w));
    }
    else {
      //  NB: u3h_v2_slot_to_node()
      void* hav_v = u3h_v2_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v2_free_buck(hav_v);
      } else {
        _ch_v2_free_node(hav_v, lef_w);
      }
    }
  }
  u3a_v2_wfree(han_u);
}

/* _ch_v2_rewrite_buck(): rewrite buck for compaction.
*/
void
_ch_v2_rewrite_buck(u3h_v2_buck* hab_u)
{
  if ( c3n == u3a_v2_rewrite_ptr(hab_u) ) return;
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_noun som = u3h_v2_slot_to_noun(hab_u->sot_w[i_w]);
    hab_u->sot_w[i_w] = u3h_v2_noun_to_slot(u3a_v2_rewritten_noun(som));
    u3a_v2_rewrite_noun(som);
  }
}

/* _ch_v2_rewrite_node(): rewrite node for compaction.
*/
void
_ch_v2_rewrite_node(u3h_v2_node* han_u, c3_w lef_w)
{
  if ( c3n == u3a_v2_rewrite_ptr(han_u) ) return;

  c3_w len_w = _ch_v2_popcount(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v2_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v2_slot_to_noun(sot_w);
      han_u->sot_w[i_w] = u3h_v2_noun_to_slot(u3a_v2_rewritten_noun(kev));

      u3a_v2_rewrite_noun(kev);
    }
    else {
      void* hav_v = u3h_v1_slot_to_node(sot_w);
      u3h_v2_node* nod_u = u3to(u3h_v2_node, u3a_v2_rewritten(u3of(u3h_v2_node,hav_v)));

      han_u->sot_w[i_w] = u3h_v2_node_to_slot(nod_u);

      if ( 0 == lef_w ) {
        _ch_v2_rewrite_buck(hav_v);
      } else {
        _ch_v2_rewrite_node(hav_v, lef_w);
      }
    }
  }
}

/* u3h_v2_rewrite(): rewrite pointers during compaction.
*/
void
u3h_v2_rewrite(u3p(u3h_v2_root) har_p)
{
  u3h_v2_root* har_u = u3to(u3h_v2_root, har_p);
  c3_w        i_w;

  if ( c3n == u3a_v2_rewrite_ptr(har_u) ) return;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v2_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v2_slot_to_noun(sot_w);
      har_u->sot_w[i_w] = u3h_v2_noun_to_slot(u3a_v2_rewritten_noun(kev));

      u3a_v2_rewrite_noun(kev);
    }
    else if ( _(u3h_v2_slot_is_node(sot_w)) ) {
      u3h_v2_node* han_u = (u3h_v2_node*) u3h_v1_slot_to_node(sot_w);
      u3h_v2_node* nod_u = u3to(u3h_v2_node, u3a_v2_rewritten(u3of(u3h_v2_node,han_u)));

      har_u->sot_w[i_w] = u3h_v2_node_to_slot(nod_u);

      _ch_v2_rewrite_node(han_u, 25);
    }
  }
}
