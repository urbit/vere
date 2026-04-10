/// @file

#ifndef U3_NOCK_H
#define U3_NOCK_H

#include <stdio.h>

#include "c3/c3.h"
#include "jets.h"
#include "types.h"
#include "zave.h"

  /** Data structures.
  ***
  **/

  /* u3n_memo: %memo hint space
   */
  typedef struct {
    c3_l    sip_l;
    u3_noun key;
    u3z_cid cid;
  } u3n_memo;

  /* u3n_call: lean kick-site state for the bytecode interpreter.
  **
  **   Mirrors the spirit of nockets' compiled `pullNock` closure.
  **
  **   pog_p caches the called formula's prog.  bat caches u3h(cor)
  **   so polymorphic call sites can detect when the formula at axe
  **   has changed (same battery → same formula → same prog).
  **
  **   lab is u3_none until tracing/profiling needs it.
  */
  struct _u3n_prog;
  typedef struct _u3n_call {
    u3p(struct _u3n_prog) pog_p;      // cached prog of called formula
    u3_noun               axe;        // axis being kicked
    u3_weak               lab;        // tracing label (or u3_none)
    u3_weak               bat;        // last cor's u3h (or u3_none)
  } u3n_call;

  /* u3n_dis_ent: one entry in a per-prog dispatch table.
  **
  **   axe_l is the axis-within-cor at which the formula was
  **   extracted; we filter by it because byte-equal formulas at
  **   multiple arms of one core would otherwise conflate jet
  **   drivers when the per-prog cache is shared across kick sites.
  */
  typedef struct _u3n_dis_ent {
    u3_atom    axe_l;                 // axis to match
    u3j_sten*  ste_u;                 // stencil to match
    u3j_harm*  ham_u;                 // jet to call on match
  } u3n_dis_ent;

#  define U3N_DIS_INLINE 4              // # of inline dispatch entries
#  define U3N_DIS_POLY   0xFFFFFFFFU    // sentinel: too many stencils

  struct _u3n_prog;

  /* u3n_dis_f: per-prog jet dispatcher.  Returns u3_none on miss
  ** (caller falls back to bytecode); transferred jet result otherwise.
  ** RETAINS cor on miss, TRANSFERS on hit.
  **
  ** axe is the call site's axis being kicked — used by templates to
  ** disambiguate entries when the same prog has multiple registered
  ** jet drivers (one per arm) and they share a stencil.
  */
  typedef u3_weak (*u3n_dis_f)(u3_noun cor,
                               struct _u3n_prog* pog_u,
                               u3_atom axe);

  /* u3n_prog: program compiled from nock
   */
  typedef struct _u3n_prog {
    struct {
      c3_o      own_o;                // program owns ops_y?
      c3_w      len_w;                // length of bytecode (bytes)
      c3_y*     ops_y;                // actual array of bytes
    } byc_u;                          // bytecode
    struct {
      c3_w      len_w;                // number of literals
      u3_noun*  non;                  // array of literals
    } lit_u;                          // literals
    struct {
      c3_w      len_w;                // number of memo slots
      u3n_memo* sot_u;                // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_w       len_w;               // number of call sites
      u3n_call*  cls_u;               // array of lean kick sites
    } cal_u;                          // call site data
    struct {
      c3_w      len_w;                // number of registration sites
      u3j_rite* rit_u;                // array of sites
    } reg_u;                          // registration site data
    struct {                          // per-prog dispatch table
      u3n_dis_f    dis_f;             // specialized dispatcher (or NULL)
      c3_w         ent_w;             // # entries, or U3N_DIS_POLY to disable
      u3n_dis_ent  ent_v[U3N_DIS_INLINE]; // inline (sten, jet) pairs
    } dis_u;                          // shared across call sites kicking this formula
  } u3n_prog;

  /**  Functions.
  **/
    /* u3n_nock_on(): produce .*(bus fol).
    */
      u3_noun
      u3n_nock_on(u3_noun bus, u3_noun fol);

    /* u3n_dis_install(): cache (axe_l, ste_u, ham_u) on prog for
    **                    future kicks.  axe_l is the axis at which
    **                    cor was kicked when ham_u was resolved —
    **                    used to disambiguate byte-equal formulas at
    **                    multiple arms of one core.  If the table is
    **                    full, marks the prog polymorphic.  ste_u
    **                    and ham_u are RETAINED.
    */
      void
      u3n_dis_install(u3p(u3n_prog) pog_p,
                      u3_atom       axe_l,
                      u3j_sten*     ste_u,
                      u3j_harm*     ham_u);

    /* u3n_dis_mark_nojet(): set the prog's dispatcher to a "no jet"
    **                      template so subsequent kicks short-circuit.
    **                      No-op if a real dispatcher is already set.
    */
      void
      u3n_dis_mark_nojet(u3p(u3n_prog) pog_p);

    /* u3n_dis_is_nojet(): is this prog's dispatcher the no-jet
    **                    sentinel?  Used by _n_kick to skip the
    **                    slow path entirely.
    */
      c3_o
      u3n_dis_is_nojet(u3n_prog* pog_u);

    /* u3n_find_lookup(): return cached prog for fol on the current
    **                    road, or 0 if not yet compiled.  Does NOT
    **                    trigger compilation.  RETAIN fol.
    */
      u3p(u3n_prog)
      u3n_find_lookup(u3_noun fol);

    /* u3n_call_kick(): slow path called when the per-prog dispatcher
    **                  misses.  Resolves cor's location fresh, finds
    **                  the jet driver, dispatches it, and installs
    **                  per-prog dispatch on every arm of the core so
    **                  future kicks hit the per-prog fast path.
    **
    **                  cor is RETAINED on miss (returns u3_none),
    **                  TRANSFERRED on hit.
    */
      u3_weak
      u3n_call_kick(u3_noun cor, u3n_call* cal_u);

    /* u3n_call_take(): copy junior call-site references to a fresh
    **                  senior call site.  RETAINS src; dst is
    **                  uninitialized on entry.
    */
      void
      u3n_call_take(u3n_call* dst_u, u3n_call* src_u);

    /* u3n_call_merge(): copy call-site references from src to dst,
    **                   losing dst's old references.
    */
      void
      u3n_call_merge(u3n_call* dst_u, u3n_call* src_u);

    /* u3n_call_ream(): refresh u3n_call after restoring from a
    **                  checkpoint.  Clears the cached pog_p so the
    **                  next kick re-derives it from cor.
    */
      void
      u3n_call_ream(u3n_call* cal_u);

    /* u3n_call_lose(): release references held by u3n_call.
    */
      void
      u3n_call_lose(u3n_call* cal_u);

    /* u3n_call_mark(): mark u3n_call references for gc.
    */
      c3_w
      u3n_call_mark(u3n_call* cal_u);

    /* u3n_find(): return prog for given formula,
     *             split by key (u3_nul for none). RETAIN.
     */
      u3p(u3n_prog)
      u3n_find(u3_noun key, u3_noun fol);

    /* u3n_burn(): execute u3n_prog with bus as subject.
     */
      u3_noun
      u3n_burn(u3p(u3n_prog) pog_p, u3_noun bus);

    /* u3n_slam_on(): produce (gat sam).
    */
      u3_noun
      u3n_slam_on(u3_noun gat, u3_noun sam);

    /* u3n_kick_on(): fire `gat` without changing the sample.
    */
      u3_noun
      u3n_kick_on(u3_noun gat);

    /* u3n_nock_in(): produce .*(bus fol), as ++toon, in namespace.
    */
      u3_noun
      u3n_nock_in(u3_noun fly, u3_noun bus, u3_noun fol);

    /* u3n_nock_it(): produce .*(bus fol), as ++toon, in namespace.
    */
      u3_noun
      u3n_nock_it(u3_noun sea, u3_noun bus, u3_noun fol);

    /* u3n_nock_et(): produce .*(bus fol), as ++toon, in namespace.
    */
      u3_noun
      u3n_nock_et(u3_noun gul, u3_noun bus, u3_noun fol);

    /* u3n_slam_in(): produce (gat sam), as ++toon, in namespace.
    */
      u3_noun
      u3n_slam_in(u3_noun fly, u3_noun gat, u3_noun sam);

    /* u3n_slam_it(): produce (gat sam), as ++toon, in namespace.
    */
      u3_noun
      u3n_slam_it(u3_noun sea, u3_noun gat, u3_noun sam);

    /* u3n_slam_et(): produce (gat sam), as ++toon, in namespace.
    */
      u3_noun
      u3n_slam_it(u3_noun gul, u3_noun gat, u3_noun sam);

    /* u3n_nock_an(): as slam_in(), but with empty fly.
    */
      u3_noun
      u3n_nock_an(u3_noun bus, u3_noun fol);

    /* u3n_reap(): promote bytecode state.
     */
      void
      u3n_reap(u3p(u3h_root) har_p);

    /* u3n_take(): copy junior bytecode state.
     */
      u3p(u3h_root)
      u3n_take(u3p(u3h_root) har_p);

    /* u3n_mark(): mark bytecode cache.
     */
      u3m_quac*
      u3n_mark();

    /* u3n_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3n_reclaim(void);

    /* u3n_rewrite_compact(): rewrite bytecode cache for compaction.
     */
      void
      u3n_rewrite_compact(void);

    /* u3n_free(): free bytecode cache.
     */
      void
      u3n_free(void);

    /* u3n_ream(): refresh after restoring from checkpoint.
    */
      void
      u3n_ream(void);

#endif /* ifndef U3_NOCK_H */
