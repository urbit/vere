/// @file fuzz_jet_l16_parse.c
///
/// Differential ice-oracle fuzzer for the 12 parser-combinator jets
/// in `pkg/noun/jets/e/parse.c`. Each combinator (cook, cold, easy,
/// fail, just, mask, shim, glue, bend, stew, stir, knee) is itself a
/// gate that CONSTRUCTS a rule. The JETTED arm is `fun` inside the
/// built rule, not the combinator itself.
///
/// To reach those `fun` arms we:
///   1. Wish a Hoon expression that BUILDS a rule from each
///      combinator, e.g. `(easy 0)` or `(cook succ (just 'a'))`.
///      Cache the resulting rule-gate.
///   2. Per fuzz iteration, build a nail from the input bytes and
///      slam the cached rule with that nail — which invokes the
///      `fun` arm.
///
/// `u3j_fuzz_arm(combinator_name)` flips the ice bit on every arm
/// of cores whose `cos_c` matches — the built rule's core `cos_c`
/// is exactly the combinator's name ("easy", "cook", etc.), so
/// flipping e.g. "easy" forces the oracle path for `easy.fun`.
///
/// Notes:
///   - `fail` and `knee` are NOT jetted in vere, so `u3j_fuzz_arm`
///     will report "not found" for them. They are still cached as
///     rules where feasible (so slamming them still exercises the
///     Hoon path, producing no oracle comparison — harmless).
///   - Requires a booted brass-pill pier at PIER_DIR (same as l11).
///
/// Build:  fuzz/build.sh fuzz_jet_l16_parse

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 128
#define H_MAX_TAPE  64
#define PIER_DIR    "/tmp/fuzz-pier-zod-v44"

extern void u3m_init(size_t len_i);
extern void u3m_pave(c3_o nuu_o);
extern void u3t_init(void);
extern c3_w  u3j_boot(c3_o nuu_o);
extern void u3j_ream(void);
extern void u3n_ream(void);
extern c3_o u3e_live(c3_o nuu_o, c3_c* dir_c);
extern c3_o u3e_yolo(void);
extern c3_c* u3m_pier(c3_c* dir_c);

typedef struct {
  const char* nam_c;   /* wish expression yielding a rule gate */
  const char* cor_c;   /* core name for u3j_fuzz_arm            */
  u3_noun     gate;    /* cached rule gate, or 0 if unavailable */
} jet_e;

/* Wish expressions chosen to build a rule from each combinator with
 * trivial arguments, using only commonly-available arms/runes. */
static jet_e _jets[] = {
  /* (cook |=(a=@ a) (easy 0)) — identity poq over a trivial rule */
  { "(cook |=(a=* a) (easy 0))",                        "cook",  0 },
  /* (cold 42 (easy 0)) — constant value over trivial rule */
  { "(cold 42 (easy 0))",                                "cold",  0 },
  /* (easy 0) — the simplest jetted rule */
  { "(easy 0)",                                          "easy",  0 },
  /* fail — bare combinator is already the rule-gate */
  { "fail",                                              "fail",  0 },
  /* (just 'a') — match literal 'a' */
  { "(just 'a')",                                        "just",  0 },
  /* (mask `(list @) ~['a' 'b' 'c']) — match any of the chars */
  { "(mask `(list @)`~['a' 'b' 'c'])",                   "mask",  0 },
  /* (shim 'a' 'z') — match char in range */
  { "(shim 'a' 'z')",                                    "shim",  0 },
  /* (glue ace (just 'a') (just 'b')) — compose two rules with sep */
  { "(glue ace (just 'a') (just 'b'))",                  "glue",  0 },
  /* (bend |=(a=@ `a) (just 'a')) — transform with a unit-returning gate */
  { "(bend |=(a=* `a) (just 'a'))",                      "bend",  0 },
  /* (stew (limo ~[['a' (easy 1)] ['b' (easy 2)]])) — dispatch */
  { "(stew (limo ~[['a' (easy 1)] ['b' (easy 2)]]))",    "stew",  0 },
  /* (stir 0 |=([a=@ b=*] a) (just 'a')) — accumulate */
  { "(stir 0 |=([a=@ b=*] a) (just 'a'))",               "stir",  0 },
  /* (knee *@ |.(~+((just 'a')))) — recursive; gar is a default result */
  { "(knee *@ |.(~+((just 'a'))))",                      "knee",  0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;
static const char* g_wish_name = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

static u3_noun
_wish_soft(u3_noun ignored)
{
  (void)ignored;
  return u3v_wish(g_wish_name);
}

/* Build a nail from `n` fuzz bytes:
 *   nail = [[line=1 col=1] tape]
 *   tape = a chronological list of bytes as atoms (one per byte).
 *
 * Clamp tape length to H_MAX_TAPE to keep parse depth bounded — dec
 * is O(a) and Hoon-side reference implementations iterate the tape.
 */
static u3_noun
_build_nail(const uint8_t* p, c3_w n)
{
  if ( n > H_MAX_TAPE ) n = H_MAX_TAPE;
  u3_noun tape = u3_nul;
  for ( c3_w i = n; i > 0; i-- ) {
    tape = u3nc((u3_atom)p[i - 1], tape);
  }
  return u3nc(u3nc(u3i_word(1), u3i_word(1)), tape);
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;

  /* Boot from snapshot — same flow as l11_base. */
  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "l16: pier at %s is empty — boot it first\n", PIER_DIR);
    return 1;
  }
  u3e_yolo();
  u3C.slog_f = 0;
  u3C.sign_hold_f = 0;
  u3C.sign_move_f = 0;
  u3t_init();
  u3m_pave(nuu_o);
  u3j_boot(nuu_o);
  u3j_ream();
  u3n_ream();
  fprintf(stderr, "l16: restored from %s at eve %llu\n",
          PIER_DIR, (unsigned long long)u3A->eve_d);

  /* Wish each rule-building expression and cache the produced gate. */
  c3_w ok = 0;
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    g_wish_name = _jets[i].nam_c;
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    if ( 0 != u3h(pro) || u3_none == u3t(pro) ) {
      fprintf(stderr, "l16: skip '%s' (wish failed)\n", _jets[i].nam_c);
      _jets[i].gate = 0;
    } else {
      _jets[i].gate = u3k(u3t(pro));
      ok++;
    }
    u3z(pro);
  }
  fprintf(stderr, "l16: %u/%u jets cached\n", ok, _jets_n);

  /* Flip ice on ALL parser-combinator `fun` arms. In the vere jet
   * tree each combinator is registered as `{"cook", ..., arm_u=0,
   * dev_u=_cook_d}` where _cook_d contains `{"fun", ..., arm_u=..}`.
   * Walking for "cook" finds the outer shell (no arms to flip), so
   * instead we walk for "fun" — flipping every `fun` arm in one
   * pass. In the current (140) tree all 13 jetted `fun` arms are
   * parse.c combinators (bend, cold, cook, comp, easy, glue, here,
   * just, mask, shim, stag, stew, stir). The ream step registers
   * additional dynamic arms from the snapshot; in practice the
   * oracle only compares when a jetted C arm exists, so extra flips
   * are harmless. Observed: ~81 arms flipped against brass-pill.
   *
   * Also flip each combinator's outer name for completeness (these
   * are no-ops in practice since outer cores have no arms, but
   * future changes in tree.c might add direct arms). */
  (void)u3j_fuzz_arm("fun");
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].cor_c);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_INPUT ) return 0;

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  u3_noun sam = _build_nail(buf + 1, (c3_w)(len - 1));

  g_gate = jet->gate;
  g_sam  = sam;
  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l16_parse: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
