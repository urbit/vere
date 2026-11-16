/// @file
///
/// Stat-only timing drivers for the `~%`-labeled arms of the SKA lib
/// (nock-compilation.hoon), on with U3NC_PROF_ARMS: each runs the arm's
/// Nock itself and counts the cycles of the outermost call, the outer
/// calls and all calls.  vere's sampling profiler can't label the cores
/// the SKA registers itself.  Off, a driver punts.

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#ifdef U3NC_STAT

#if defined(__x86_64__)
#  include <x86intrin.h>
#  define _prof_tsc()  ((c3_d)__rdtsc())
#else
#  define _prof_tsc()  ((c3_d)0)
#endif

const c3_c* u3we_prof_nam_c[U3WE_PROF_N] = {
  "ska-callgraph", "ska-analyze", "ska-callgraph-iteration", "fol-loop",
  "register-fast", "core-chains", "know", "bake-graph", "poke-scc-loop",
  "eval-finalized", "scc-of", "compile-unary", "huge", "pull", "alias",
  "bake-need", "poke-tarjan", "remove-hops", "compile-scc-in", "optimize",
};

//  per arm: cycles of the outermost calls, outer calls, calls
//
c3_d u3we_prof_d[U3WE_PROF_N][3];
c3_w u3we_prof_dep_w[U3WE_PROF_N];

static c3_i _prof_on_i = -1;

/* _prof(): run the arm of gate .cor, timing it if outermost.
*/
static u3_weak
_prof(c3_w i_w, u3_noun cor)
{
  c3_d    beg_d;
  c3_t    out_t;
  u3_noun pro;

  if ( _prof_on_i < 0 ) {
    _prof_on_i = !!getenv("U3NC_PROF_ARMS");
  }
  if ( !_prof_on_i ) {
    return u3_none;
  }

  beg_d = _prof_tsc();
  u3we_prof_d[i_w][2]++;
  out_t = ( 0 == u3we_prof_dep_w[i_w]++ );
  pro   = u3n_nock_on(u3k(cor), u3k(u3h(cor)));
  u3we_prof_dep_w[i_w]--;

  if ( out_t ) {
    u3we_prof_d[i_w][0] += _prof_tsc() - beg_d;
    u3we_prof_d[i_w][1]++;
  }
  return pro;
}

u3_noun u3we_prof_ska_callgraph(u3_noun cor)           { return _prof(0, cor); }
u3_noun u3we_prof_ska_analyze(u3_noun cor)             { return _prof(1, cor); }
u3_noun u3we_prof_ska_callgraph_iteration(u3_noun cor) { return _prof(2, cor); }
u3_noun u3we_prof_fol_loop(u3_noun cor)                { return _prof(3, cor); }
u3_noun u3we_prof_register_fast(u3_noun cor)           { return _prof(4, cor); }
u3_noun u3we_prof_know(u3_noun cor)                    { return _prof(6, cor); }
u3_noun u3we_prof_bake_graph(u3_noun cor)              { return _prof(7, cor); }
u3_noun u3we_prof_poke_scc_loop(u3_noun cor)           { return _prof(8, cor); }
u3_noun u3we_prof_eval_finalized(u3_noun cor)          { return _prof(9, cor); }
u3_noun u3we_prof_scc_of(u3_noun cor)                  { return _prof(10, cor); }
u3_noun u3we_prof_compile_unary(u3_noun cor)           { return _prof(11, cor); }
u3_noun u3we_prof_huge(u3_noun cor)                    { return _prof(12, cor); }
u3_noun u3we_prof_pull(u3_noun cor)                    { return _prof(13, cor); }
u3_noun u3we_prof_alias(u3_noun cor)                   { return _prof(14, cor); }
u3_noun u3we_prof_bake_need(u3_noun cor)               { return _prof(15, cor); }
u3_noun u3we_prof_poke_tarjan(u3_noun cor)             { return _prof(16, cor); }
u3_noun u3we_prof_remove_hops(u3_noun cor)             { return _prof(17, cor); }
u3_noun u3we_prof_compile_scc_in(u3_noun cor)          { return _prof(18, cor); }
u3_noun u3we_prof_optimize(u3_noun cor)                { return _prof(19, cor); }

#endif
