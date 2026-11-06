/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


/*
  +nest:ut takes two types, `sut` and `ref`, and checks whether `ref` nests
  under `sut`. Its worker +dext threads three sets down the recursion:
    - seg: %hold `sut`s expanded by outer frames, cleared by %cell and %core,
    - reg: %hold `ref`s expanded by outer frames, cleared by %cell and %core,
    - gil: pairs [sut ref] assumed to nest by outer frames, never cleared.

  `seg` and `reg` stop a %hold from expanding to itself without passing through
  a %cell or a %core. Finding `sut` in `seg` answers %.n and finding `ref` in
  `reg` answers %.y. Those are the neutral answers for +lien and +levy over a
  %fork, so that path contributes nothing and the other branches decide. `seg`
  can only turn %.y into %.n, and `reg` and `gil` can only turn %.n into %.y.

  `gil` proves nesting by coinduction: in %core and %hold we check if the pair
  [sut ref] is already in `gil`, and if it's not, we descend into the type by
  either expanding the %hold or producing the types of the arms of the core,
  assuming that they nest by pushing [sut ref] into `gil`. Both can then produce
  the same [sut ref] comparison, which will be found in `gil` and answer %.y.
  That way we only check the parts of the types that do not expand infinitely,
  verifying or disproving that they nest.

  Clearing on both %cell and %core confines `seg` and `reg` to chains of %hold,
  %fork, %face and %hint. There `sut` stops changing once `ref` starts
  expanding, so a `reg` hit is on the same pair as the frame that pushed it, and
  a `seg` hit only skips branches that frame walks anyway. A pair therefore has
  one answer, the one it gets with all three sets empty.

  The memo key omits the sets, so a saved answer must not depend on entries
  pushed above its frame, whether the frame or one below it hit them:
    - A %.y is saved only if it has no `reg` or `gil` hits left. We keep track
      of those hits, and the frame that pushed an entry discharges the hits on
      it.
    - A %.n is saved only if `seg` was empty on entry, so that every entry it
      could've hit was pushed below it. Tracking `seg` hits exactly would save
      some 465k more %.n per compile of hoon.hoon, but with the default
      transient cache of 50k entries that only costs time.

  Reusing a saved answer has its own rule, see the lookup below.
*/


//  hits on `reg` and `gil` not discharged yet: unordered lists of `ref`s and
//  pairs, which may repeat
//
static u3_noun _hits_reg = u3_nul;
static u3_noun _hits_gil = u3_nul;

//  drop every `som` from unordered list `hit`
static u3_noun
_set_drop(u3_noun hit, u3_noun som)
{
  u3_noun out, i;

  if ( u3_nul == hit ) {
    return hit;
  }

  for ( out = u3_nul, i = hit; u3_nul != i; i = u3t(i) ) {
    if ( c3n == u3r_sing(u3h(i), som) ) {
      out = u3nc(u3k(u3h(i)), out);
    }
  }

  u3z(hit);
  return out;
}

u3_noun
u3wfu_nest_dext2(u3_noun dext_core)
{
  u3_noun nest_in_core, nest_core;
  u3_noun bat, sut, ref, van, seg, reg, gil, tel, vet;

  if (  (u3_none == (nest_in_core = u3r_at(3, dext_core)))
     || (c3n == u3r_mean(nest_in_core, {u3x_sam_2, &seg},
                                       {u3x_sam_6, &reg},
                                       {u3x_sam_7, &gil},
                                       {u3x_con, &nest_core}))
     || (c3n == u3r_mean(nest_core, {u3x_sam_2, &tel},
                                    {u3x_sam_3, &ref},
                                    {u3x_con, &van}))
     || (u3_none == (bat = u3r_at(u3x_bat, van)))
     || (u3_none == (sut = u3r_at(u3x_sam, van)))
     || (u3_none == (vet = u3r_at(u3qfu_van_vet, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    c3_t roo_t = !(u3R->how.fag_w & u3a_flag_nest);
    if ( roo_t ) {
      _hits_reg = u3_nul;
      _hits_gil = u3_nul;
      u3R->how.fag_w |= u3a_flag_nest;
    }

    c3_m  fun_m = 141 + c3__dext + ((!!vet) << 8);
    u3_noun key = u3z_key_3(fun_m, sut, ref, bat);
    u3_noun par   = u3nc(u3k(sut), u3k(ref));
    u3_weak pro;

    c3_t reg_t = (u3_nul != reg) && (c3y == u3qdi_has(reg, ref));
    c3_t gil_t = (u3_nul != gil) && (c3y == u3qdi_has(gil, par));

    //  A saved answer can be reused under any sets. This frame may then answer
    //  differently from Nock, but the verdict of the whole +nest does not
    //  change:
    //    - A saved %.y had every hypothesis it used discharged within its own
    //      frame, so the pair nests. Nock could only answer %.n here through a
    //      `seg` hit below, and the frame that pushed that entry walks the
    //      branch that proved this %.y anyway.
    //    - A saved %.n means the pair does not nest. Nock could only answer %.y
    //      here through a false hypothesis, and the outermost frame that pushed
    //      a false hypothesis fails.
    //
    pro = u3z_find(u3z_memo_toss, key);

    if ( (u3_none != pro) && !((c3n == pro) && (c3y == tel)) ) {
      u3z(key);
      u3z(par);
      if ( roo_t ) {
        u3R->how.fag_w &= ~(c3_w)u3a_flag_nest;
      }
      return pro;
    }

    u3_noun stash_reg = _hits_reg, stash_gil = _hits_gil;
    _hits_reg = _hits_gil = u3_nul;

    u3_noun here_reg, here_gil;
    c3_t    sav_t;

    pro = u3n_nock_on(u3k(dext_core), u3k(u3x_at(u3x_bat, dext_core)));

    //  A frame that could've hit a set records its own hit. Otherwise every hit
    //  below on its `ref` or pair was on an entry pushed at or below this
    //  frame, so it is discharged.
    //
    here_reg = reg_t ? u3nc(u3k(ref), _hits_reg) : _set_drop(_hits_reg, ref);
    here_gil = gil_t ? u3nc(u3k(par), _hits_gil) : _set_drop(_hits_gil, par);

    if ( c3y == pro ) {
      //  `reg` and `gil` could've turned %.n into %.y, so we only memoize if
      //  no hit on an entry pushed above this frame is left.
      //
      sav_t = (u3_nul == here_reg) && (u3_nul == here_gil);
    }
    else {
      //  `seg` could've turned %.y into %.n, so we only memoize if it was empty
      //  on entry. `reg` and `gil` hits only turn %.n into %.y, so this answer
      //  owes them nothing and we drop them.
      //
      u3z(here_reg);
      u3z(here_gil);
      here_reg = here_gil = u3_nul;
      sav_t   = (u3_nul == seg);
    }

    if ( sav_t ) {
      u3z_save(u3z_memo_toss, key, pro);
    }
    else {
      u3z(key);
    }

    if ( roo_t ) {
      //  root frame: no outer frame to hand hits to; keep no nouns around
      //
      u3z(here_reg);
      u3z(here_gil);
      _hits_reg = _hits_gil = u3_nul;
      u3R->how.fag_w &= ~(c3_w)u3a_flag_nest;
    }
    else {
      //  hand undischarged hits to the outer frames
      //
      _hits_reg = u3kb_weld(here_reg, stash_reg);
      _hits_gil = u3kb_weld(here_gil, stash_gil);
    }

    u3z(par);
    return pro;

  }
}
