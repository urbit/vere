/// @file

#include "zave.h"

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "options.h"
#include "vortex.h"
#include "jets/k.h"
#include "jets/q.h"
#include "nock.h"
#include "xtract.h"

#include <stdio.h>
#include <stdlib.h>

/* u3z_key(): construct a memo cache-key.  Arguments retained.
*/
u3_noun
u3z_key(c3_m fun, u3_noun one)
{
  return u3nc(fun, u3k(one));
}
u3_noun
u3z_key_2(c3_m fun, u3_noun one, u3_noun two)
{
  return u3nt(fun, u3k(one), u3k(two));
}
u3_noun
u3z_key_3(c3_m fun, u3_noun one, u3_noun two, u3_noun tri)
{
  return u3nq(fun, u3k(one), u3k(two), u3k(tri));
}
u3_noun
u3z_key_4(c3_m fun, u3_noun one, u3_noun two, u3_noun tri, u3_noun qua)
{
  return u3nc(fun, u3nq(u3k(one), u3k(two), u3k(tri), u3k(qua)));
}
u3_noun
u3z_key_5(c3_m fun, u3_noun one, u3_noun two, u3_noun tri, u3_noun qua, u3_noun qin)
{
  return u3nc(fun, u3nq(u3k(one), u3k(two), u3k(tri), u3nc(u3k(qua), u3k(qin))));
}

/* _har(): get the memo cache for the given cid.
*/
static u3p(u3h_root)
_har(u3a_road* rod_u, u3z_cid cid)
{
  switch ( cid ) {
    case u3z_memo_toss:
      return rod_u->cax.har_p;
    case u3z_memo_keep:
      return rod_u->cax.per_p;
    case u3z_memo_ford:
      return rod_u->cax.for_p;  // design cache key to handle arvo changes that invalidate
  }
  u3_assert(0);
}

//  RETAINS
//  on hit returns the containing road via out_u
static u3_weak
_find_in_roads(u3z_cid cid, u3_noun key, u3a_road** out_u)
{
  //  XX needs to be benchmarked (up vs. down search)
  u3a_road* rod_u = &(u3H->rod_u);
  while ( 1 ) {
    u3_weak got = u3h_get(_har(rod_u, cid), key);
    if ( u3_none != got ) {
      if ( out_u ) *out_u = rod_u;
      return got;
    }
    if ( 0 == rod_u->kid_p ) {
      return u3_none;
    }
    rod_u = u3to(u3a_road, rod_u->kid_p);
  }
}

/* u3z_find(): find in memo cache.  Arguments retained.
*/
u3_weak
u3z_find(u3z_cid cid, u3_noun key)
{
  if ( (u3z_memo_keep == cid) && (u3R->how.fag_w & u3a_flag_cash) ) {
    //  XX jet mismatch in +mice when portable memo hints are nested
    //  XX put +mice behind runtime flag?
    //
    u3a_road* rod_u;
    u3_noun pro = _find_in_roads(cid, key, &rod_u);
    if ( u3_none != pro && rod_u != u3R ) {
      u3h_put(_har(u3R, cid), key, u3k(pro));
    }
    return pro;
  }

  if ( (u3z_memo_toss == cid) || (u3R->how.fag_w & u3a_flag_cash) ) {
    //  we have to check the current road only so that we don't skip portable
    //  memo hints
    //
    return u3h_get(_har(u3R, cid), key);
  }

  return _find_in_roads(cid, key, NULL);
}

/* u3z_find_up(): find in persistent memo cache,
  starting from the current road. Arguments retained
*/
u3_weak
u3z_find_up(u3_noun key)
{
  u3a_road* rod_u = u3R;
  while ( 1 ) {
    u3_weak got = u3h_get(_har(rod_u, u3z_memo_keep), key);
    if ( u3_none != got ) {
      return got;
    }
    if ( rod_u == &(u3H->rod_u) ) {
      return u3_none;
    }
    rod_u = u3to(u3a_road, rod_u->par_p);
  }
}

u3_weak
u3z_find_m(u3z_cid cid, c3_m fun, u3_noun one)
{
  u3_noun key = u3nc(fun, u3k(one));
  u3_weak val;
  val = u3z_find(cid, key);
  u3z(key);

  return val;
}

/* u3z_save(): save in memo cache. TRANSFER key; RETAIN val
*/
u3_noun
u3z_save(u3z_cid cid, u3_noun key, u3_noun val)
{
  u3h_put(_har(u3R, cid), key, u3k(val));
  u3z(key);
  return val;
}

/* u3z_save_m(): save in memo cache. Arguments retained.
*/
u3_noun
u3z_save_m(u3z_cid cid, c3_m fun, u3_noun one, u3_noun val)
{
  u3_noun key = u3nc(fun, u3k(one));
  u3h_put(_har(u3R, cid), key, u3k(val));
  u3z(key);
  return val;
}

/* u3z_save_m_dedup(): save in memo cache, deduplicate. All but val is retained.
*/
u3_noun
u3z_save_m_dedup(u3z_cid cid, c3_m fun, u3_noun one, u3_noun val)
{
  u3_noun key = u3nc(fun, u3k(one));
  if ( u3z_memo_toss != cid ) {
    key = u3m_dedup(key);
    val = u3m_dedup(val);
  }

  u3h_put(_har(u3R, cid), key, u3k(val));
  u3z(key);
  return val;
}

/* u3z_uniq(): uniquify with memo cache. XX not used.
*/
u3_noun
u3z_uniq(u3z_cid cid, u3_noun som)
{
  u3_noun key = u3nc(c3__uniq, u3k(som));
  u3_noun val = u3h_get(_har(u3R, cid), key);
  u3_noun pro;
  if ( u3_none != val ) {
    u3z(som);
    pro = val;
  }
  else {
    u3h_put(_har(u3R, cid), key, u3k(som));
    pro = som;
  }
  u3z(key);
  return pro;
}

/* u3z_reap(): promote memoization cache state.
*/
void
u3z_reap(u3z_cid cid, u3p(u3h_root) har_p)
{
  u3_assert(u3z_memo_toss != cid);

  u3h_uni(_har(u3R, cid), har_p);
  u3h_free(har_p);
}

/* u3z_ut_*(): transient memoization for the +ut jets.  See zave.h.
**
**   Negative tests reach us through +rest, which tests its hold against
**   fan before extending fan with it: +redo only tests a hold it then
**   expands through +rest, or has already expanded in the same call.
**   The extended fan is learnt from the +put:in or +gas:in that makes it
**   (u3z_ut_grow()); +rest builds no set before that.  Positive tests
**   only happen under a non-empty fan, whose noun +has:in recognizes by
**   identity (u3z_ut_test()), as do +put:in and +gas:in.
*/

/* _ut: the innermost executing wrapper frame.
**   pos, neg: tests of its subtree so far, as sets of holds [type hoon]
**   fan: the fan its hoon runs under; u3_none when no frame executes
**   res: it is +rest and has not extended fan yet
**   own: fan is the extended fan, held by us: hoon may drop it before
**        +rest returns
**   kin: its kind (zave.h)
**   opq: its result is valid under the empty fan only: it or a frame it
**        used is a top frame, whose tests are not kept
**   dep: frames executing; at zero the statics hold no nouns, since the
**        road they were made in may be gone by the next computation
**   chk: U3_UT_MEMO_CHECK: -1 unread, 0 off, 1 on, 2 on, inside a recompute
*/
static u3_noun _ut_pos = u3_nul;
static u3_noun _ut_neg = u3_nul;
static u3_weak _ut_fan = u3_none;
static c3_o    _ut_res = c3n;
static c3_o    _ut_own = c3n;
static c3_w    _ut_kin_w = u3z_ut_norm;
static c3_o    _ut_opq = c3n;
static c3_w    _ut_dep_w = 0;
static c3_i    _ut_chk_i = -1;
static c3_d    _ut_hit_d, _ut_bad_d;

/* _ut_tag: head of a saved entry that is not valid under every fan:
**   [tag [pos neg] val], valid where fan agrees with the tests, or
**   [tag ~ val], valid under the empty fan only.
**   No +ut result starts with it: types, [type nock], nock, loobeans.
*/
#define _ut_tag  c3__memo

static void
_ut_report(void)
{
  fprintf(stderr, "ut memo check: %" PRIu64 " hits recomputed, %" PRIu64
                  " mismatches\r\n", _ut_hit_d, _ut_bad_d);
}

/* _ut_part(): is set a a subset of set b?  RETAINS.
*/
static c3_o
_ut_part(u3_noun a, u3_noun b)
{
  while ( u3_nul != a ) {
    u3_noun n_a, l_a, r_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    if ( c3n == u3qdi_has(b, n_a) ) return c3n;
    if ( c3n == _ut_part(l_a, b) )  return c3n;
    a = r_a;
  }
  return c3y;
}

/* _ut_meet(): does any element of set a belong to set b?  RETAINS.
*/
static c3_o
_ut_meet(u3_noun a, u3_noun b)
{
  while ( u3_nul != a ) {
    u3_noun n_a, l_a, r_a;
    u3x_trel(a, &n_a, &l_a, &r_a);
    if ( c3y == u3qdi_has(b, n_a) ) return c3y;
    if ( c3y == _ut_meet(l_a, b) )  return c3y;
    a = r_a;
  }
  return c3n;
}

/* _ut_legs(): the elements of set fen not in set fan.  RETAINS; produces
** a list.
*/
static u3_noun
_ut_legs(u3_noun fen, u3_noun fan)
{
  u3_noun out = u3_nul;

  while ( u3_nul != fen ) {
    u3_noun n_fen, l_fen, r_fen;
    u3x_trel(fen, &n_fen, &l_fen, &r_fen);
    if ( c3n == u3qdi_has(fan, n_fen) ) {
      out = u3nc(u3k(n_fen), out);
    }
    out = u3kb_weld(_ut_legs(l_fen, fan), out);
    fen = r_fen;
  }
  return out;
}

u3_weak
u3z_ut_find(u3_noun key, u3_noun fan, u3_noun cor)
{
  u3_weak ent = u3z_find(u3z_memo_toss, key);
  u3_noun val;

  if ( u3_none == ent ) {
    return u3_none;
  }

  if ( (c3y == u3du(ent)) && (_ut_tag == u3h(ent)) ) {
    u3_noun tes = u3h(u3t(ent));

    if ( u3_nul == tes ) {
      if ( u3_nul != fan ) {
        u3z(ent);
        return u3_none;
      }
      if ( (2 != _ut_chk_i) && _ut_dep_w ) {
        _ut_opq = c3y;
      }
    }
    else {
      u3_noun pos = u3h(tes);
      u3_noun neg = u3t(tes);

      if ( (c3n == _ut_part(pos, fan)) || (c3y == _ut_meet(fan, neg)) ) {
        u3z(ent);
        return u3_none;
      }
      if ( (2 != _ut_chk_i) && _ut_dep_w && (u3z_ut_top != _ut_kin_w) ) {
        _ut_pos = u3kdi_uni(_ut_pos, u3k(pos));
        _ut_neg = u3kdi_uni(_ut_neg, u3k(neg));
      }
    }
    val = u3k(u3t(u3t(ent)));
    u3z(ent);
  }
  else {
    val = ent;
  }

  if ( -1 == _ut_chk_i ) {
    _ut_chk_i = !!getenv("U3_UT_MEMO_CHECK");
    if ( _ut_chk_i ) atexit(_ut_report);
  }
  if ( 1 == _ut_chk_i ) {
    u3_noun re;
    _ut_chk_i = 2;
    re = u3n_nock_on(u3k(cor), u3k(u3x_at(u3x_bat, cor)));
    _ut_chk_i = 1;
    _ut_hit_d++;
    if ( c3n == u3r_sing(re, val) ) {
      _ut_bad_d++;
      fprintf(stderr, "ut memo check: mismatch under %x\r\n", u3h(key));
    }
    u3z(re);
  }
  return val;
}

void
u3z_ut_enter(u3z_ut_frame* fra_u, u3_noun fan, c3_w kin_w)
{
  fra_u->pos = _ut_pos;
  fra_u->neg = _ut_neg;
  fra_u->fan = _ut_fan;
  fra_u->res = _ut_res;
  fra_u->own = _ut_own;
  fra_u->opq = _ut_opq;
  fra_u->kin = _ut_kin_w;
  fra_u->ent = fan;
  fra_u->kiz = kin_w;
  _ut_pos = u3_nul;
  _ut_neg = u3_nul;
  _ut_fan = fan;
  _ut_res = __(u3z_ut_rest == kin_w);
  _ut_own = c3n;
  _ut_opq = c3n;
  _ut_kin_w = kin_w;
  _ut_dep_w++;
}

void
u3z_ut_leave(u3z_ut_frame* fra_u, u3_weak key, u3_noun val)
{
  u3_noun pos = _ut_pos;
  u3_noun neg = _ut_neg;
  c3_o    opq = __((c3y == _ut_opq) || (u3z_ut_top == fra_u->kiz));

  if ( u3z_ut_rest == fra_u->kiz ) {
    //  a +rest frame: _ut_fan is the fan it played under, and the holds
    //  it added are its own.  Tests on them below were made under a fan
    //  holding them, and the frame's own test found them absent.
    //
    u3_noun leg = _ut_legs(_ut_fan, fra_u->ent);
    u3_noun i;

    for ( i = leg; u3_nul != i; i = u3t(i) ) {
      u3_noun old = pos;
      pos = u3qdi_del(old, u3h(i));
      u3z(old);
      neg = u3kdi_put(neg, u3k(u3h(i)));
    }
    u3z(leg);
  }

  if ( u3_none != key ) {
    if ( 2 == _ut_chk_i ) {
      u3z(key);
    }
    else if ( c3y == opq ) {
      //  tests unknown: valid under the empty fan only, if made under it
      //
      if ( u3_nul == fra_u->ent ) {
        u3_noun ent = u3nt(_ut_tag, u3_nul, u3k(val));
        u3z_save(u3z_memo_toss, key, ent);
        u3z(ent);
      }
      else {
        u3z(key);
      }
    }
    else if ( (u3_nul == pos) && (u3_nul == neg) ) {
      u3z_save(u3z_memo_toss, key, val);
    }
    else {
      u3_noun ent = u3nt(_ut_tag, u3nc(u3k(pos), u3k(neg)), u3k(val));
      u3z_save(u3z_memo_toss, key, ent);
      u3z(ent);
    }
  }

  if ( c3y == _ut_own ) {
    u3z(_ut_fan);
  }
  if ( _ut_dep_w ) {
    _ut_dep_w--;
  }
  if ( 0 == _ut_dep_w ) {
    //  the outermost frame: nothing above depends on its tests
    //
    u3z(pos);
    u3z(neg);
    _ut_pos = u3_nul;
    _ut_neg = u3_nul;
    _ut_opq = c3n;
  }
  else if ( u3z_ut_top == fra_u->kin ) {
    //  a top frame keeps no tests
    //
    u3z(pos);
    u3z(neg);
    _ut_pos = fra_u->pos;
    _ut_neg = fra_u->neg;
    _ut_opq = fra_u->opq;
  }
  else {
    _ut_pos = u3kdi_uni(fra_u->pos, pos);
    _ut_neg = u3kdi_uni(fra_u->neg, neg);
    _ut_opq = c3o(fra_u->opq, opq);
  }
  _ut_fan = fra_u->fan;
  _ut_res = fra_u->res;
  _ut_own = fra_u->own;
  _ut_kin_w = fra_u->kin;
}

void
u3z_ut_grow(u3_noun set, u3_noun out)
{
  //  +rest extends fan before anything else builds a set, so the first
  //  growth of its fan, even an empty one, is hoon's extended fan; a
  //  non-empty fan is known by identity, and so is its growth
  //
  if ( (set == _ut_fan) && ((u3_nul != set) || (c3y == _ut_res)) ) {
    if ( c3y == _ut_own ) {
      u3z(_ut_fan);
    }
    _ut_fan = u3k(out);
    _ut_own = c3y;
    _ut_res = c3n;
  }
}

void
u3z_ut_test(u3_noun set, u3_noun key, u3_noun res)
{
  if ( (u3_nul != set) && (set == _ut_fan) && (2 != _ut_chk_i) ) {
    if ( c3y == res ) {
      _ut_pos = u3kdi_put(_ut_pos, u3k(key));
    }
    else {
      _ut_neg = u3kdi_put(_ut_neg, u3k(key));
    }
  }
}

void
u3z_ut_bail(void)
{
  //  the frames were in the road being abandoned, and so were their nouns
  //
  _ut_pos = u3_nul;
  _ut_neg = u3_nul;
  _ut_fan = u3_none;
  _ut_res = c3n;
  _ut_own = c3n;
  _ut_opq = c3n;
  _ut_kin_w = u3z_ut_norm;
  _ut_dep_w = 0;
  if ( 2 == _ut_chk_i ) {
    _ut_chk_i = 1;
  }
}
