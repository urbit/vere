/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/*  u3wfu_nest_dext(): memoizing wrapper for +dext:nest:ut.
**
**    +dext is the recursive worker of +nest.  Besides the pair being
**    compared, [sut ref], and the crash flag [tel], its result depends
**    on three sets threaded down the recursion:
**
**      seg: sut holds expanded on the current path    (hit: %.n)
**      reg: ref holds expanded on the current path    (hit: %.y)
**      gil: pairs assumed to nest on the current path (hit: %.y)
**
**    A hit ends the recursion with a fixed answer, so a result reached
**    through one is conditional on a hypothesis that an enclosing frame
**    is still in the middle of establishing.
**
**    We memoize on [sut ref] alone (plus the compiler battery), so that
**    a comparison done inside one +nest is reusable by any later one.
**    Such an entry must not depend on the sets.  Every combinator in
**    +dext (?&, ?|, +lien, +levy, +meet, +deem, +deep) is monotone in
**    its arguments and each set pushes in one direction only, so:
**
**      - a seg hit can only turn a %.y into a %.n: a %.n is unconditional
**        unless its subtree hit a seg entry made by an enclosing frame;
**      - a reg or gil hit can only turn a %.n into a %.y: a %.y is
**        unconditional unless its subtree hit a reg or gil entry made by
**        an enclosing frame.
**
**    For seg we keep the coarse test used since 2016: the set was empty
**    on entry, so no enclosing frame can have added to it.  Tracking seg
**    hits exactly, as below, saves some 465k more %.n results per
**    hoon.hoon compile, but with the default transient cache of 50k
**    entries that only costs time (1.4s of 17), and with a 500k cache it
**    gains 0.4s.
**
**    Reg and gil hits are tracked exactly.  +dext only ever consults a
**    set for its own sut, ref or pair, so a frame whose sut is in seg,
**    ref in reg or pair in gil on entry is a hit, and the frame that made
**    that entry (the nearest enclosing frame with the same sut, ref or
**    pair that is not itself a hit) is the one whose return discharges
**    it.  Each evaluating frame therefore collects the reg and gil hits
**    of its subtree in _nest_reg and _nest_gil, removes those it
**    discharges when it returns, and:
**
**      - returning %.y, hands them to its parent (they are on hypotheses
**        of enclosing frames, which its answer now depends on) and is
**        saved only if there are none;
**      - returning %.n, drops them, and is saved if seg was empty.
**
**    A %.y saved this way was derived with every hypothesis it used
**    discharged by a %.y inside its own subtree, i.e. by a closed
**    coinductive argument, so it holds in any context.  A saved %.n
**    means the pair is not in the greatest fixed point of the nesting
**    rules; +dext under hypotheses could only say %.y for it by using a
**    hypothesis that is itself false, which the frame that introduced
**    that hypothesis then rejects, so the enclosing +nest is unaffected
**    and no %.n saved on the way is wrong.  Cached entries are thus safe
**    to return in every context.  A frame that is itself a hit does not
**    consult the cache: hoon answers it from the hypothesis.
**
**    From 2016 the C port saved %.y on the coarse condition that reg was
**    empty on entry, and ignored gil: every hold expansion adds its
**    pair, so gil is non-empty below nearly every comparison.  Inside a
**    core comparison +deep extends gil with the pair of cores and checks
**    the arm products under that hypothesis; a sub-pair that bottomed
**    out in the hypothesis yielded %.y and was cached as unconditional.
**    When the comparison then failed on another arm without crashing
**    (any +nest with tel=|, in particular the +lien over a %fork), the
**    entry for the core pair itself was overwritten by the failing
**    enclosing result, but every intermediate pair stayed poisoned, and
**    a later cast between two of them compiled:
**
**      =>  ~  =+  [a=|.([[. .] 0]) b=|.([[. .] [0 0]])]
**      =+  ^+(?:(& a b) b)  ^+(-:$:a -:$:b)
**
**    tel is not part of the key: it never changes the boolean, only
**    whether a %.n crashes with a nest-fail trace.  A cached %.n is
**    therefore not returned when tel is set; the arm is re-evaluated,
**    which fails exactly where hoon would.
**
**    The result also depends on fan, through the holds expanded below;
**    u3z_ut_*() tracks that.
*/

/* _nest_reg, _nest_gil: hits by frames of the +dext call tree currently
** executing, not yet discharged by the frame that introduced them: refs
** and [sut ref] pairs.  u3_nul between calls.  +nest
** never virtualizes, so a bail unwinds the whole tree; the next root
** frame (empty gil) resets them.
*/
static u3_noun _nest_reg = u3_nul;
static u3_noun _nest_gil = u3_nul;

/* _nest_drop(): remove every element equal to som from a hit list.
** RETAINS som, TRANSFERS and produces the list.
*/
static u3_noun
_nest_drop(u3_noun hit, u3_noun som)
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
u3wfu_nest_dext(u3_noun dext_core)
{
  u3_noun nest_in_core, nest_core;
  u3_noun bat, sut, tel, ref, van, seg, reg, gil, fan;

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
     || (u3_none == (fan = u3r_at(u3qfu_van_fan, van))) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    u3_weak vet   = u3r_at(u3qfu_van_vet, van);
    c3_m    fun_m = 141 + c3__dext + ((!!vet) << 8);
    u3_noun par   = u3nc(u3k(sut), u3k(ref));
    //  this frame's own hits
    //
    c3_t    seg_t = (u3_nul != seg) && (c3y == u3qdi_has(seg, sut));
    c3_t    reg_t = (u3_nul != reg) && (c3y == u3qdi_has(reg, ref));
    c3_t    gil_t = (u3_nul != gil) && (c3y == u3qdi_has(gil, par));
    u3_weak key   = u3_none;
    u3_weak pro   = u3_none;

    if ( !(seg_t || reg_t || gil_t) ) {
      key = u3z_key_3(fun_m, sut, ref, bat);
      pro = u3z_ut_find(key, fan, dext_core);

      //  a cached %.n is not returned when tel is set; hoon crashes there
      //
      if ( (u3_none != pro) && !((c3n == pro) && (c3y == tel)) ) {
        u3z(key);
        u3z(par);
        return pro;
      }
    }

    {
      //  out_*: undischarged hits of the frames above this one
      //
      u3_noun out_reg = _nest_reg, out_gil = _nest_gil;
      u3_noun hit_reg, hit_gil;
      c3_t    sav_t;
      u3z_ut_frame fra_u;

      if ( u3_nul == gil ) {
        //  root frame: nothing above can hold hits
        //
        out_reg = out_gil = u3_nul;
      }
      _nest_reg = _nest_gil = u3_nul;

      u3z_ut_enter(&fra_u, fan, (u3_nul == fan) ? u3z_ut_top : u3z_ut_norm);
      pro = u3n_nock_on(u3k(dext_core), u3k(u3x_at(u3x_bat, dext_core)));

      //  add this frame's hits, or discharge the hypotheses it introduced
      //
      hit_reg = reg_t ? u3nc(u3k(ref), _nest_reg) : _nest_drop(_nest_reg, ref);
      hit_gil = gil_t ? u3nc(u3k(par), _nest_gil) : _nest_drop(_nest_gil, par);

      if ( c3y == pro ) {
        sav_t = (u3_nul == hit_reg) && (u3_nul == hit_gil);
      }
      else {
        //  a %.n owes nothing to reg or gil hits
        //
        u3z(hit_reg);
        u3z(hit_gil);
        hit_reg = hit_gil = u3_nul;
        sav_t   = (u3_nul == seg);
      }

      if ( (u3_none != key) && !sav_t ) {
        u3z(key);
        key = u3_none;
      }
      u3z_ut_leave(&fra_u, key, pro);

      if ( u3_nul == gil ) {
        //  root frame: its hits are all discharged; keep no nouns around
        //
        u3z(hit_reg);
        u3z(hit_gil);
        _nest_reg = _nest_gil = u3_nul;
      }
      else {
        _nest_reg = u3kb_weld(hit_reg, out_reg);
        _nest_gil = u3kb_weld(hit_gil, out_gil);
      }

      u3z(par);
      return pro;
    }
  }
}
