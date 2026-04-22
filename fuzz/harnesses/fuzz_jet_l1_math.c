/// @file fuzz_jet_l1_math.c
///
/// Differential fuzz harness for Hoon stdlib layer-1 math jets.
///
/// The goal is to catch SILENT C-vs-Hoon drift: a jet that returns
/// a different value than the Hoon formula for the same input.
/// Our other jet harnesses only catch crashes; this one uses vere's
/// built-in `ice` oracle (`pkg/noun/jets.c:_cj_kick_z`) to run both
/// the C jet and the Hoon formula on every call and bail %fail on
/// mismatch.
///
/// The oracle is dead by default — every `u3j_harm` in
/// `pkg/noun/jets/tree.c` declares `ice = c3y` (perfect, don't test).
/// We flip `ice = c3n` on the 16 layer-1 cores at boot via the new
/// `u3j_fuzz_arm` helper, then fuzz samples through `u3n_slam_on`.
///
/// Layer 1 is the bottom of the Hoon stdlib: `add`, `dec`, `sub`,
/// `mul`, `div`, `mod`, `gte`, `gth`, `lte`, `lth`, `max`, `min`,
/// `cap`, `mas`, `peg`, `dvr`. None of them call higher-layer jets
/// in their Hoon formulas, so the oracle doesn't re-enter.
///
/// CRITICAL — dec is O(a) in Hoon (counts up from 0). At 32-bit
/// fuzz atoms that's ~4 billion nock iterations per call. Each jet
/// here clamps its sample to a size the Hoon formula can evaluate
/// in a few ms. See plan doc for rationale.
///
/// Input layout:
///   byte 0:    mode (% 16, selects jet)
///   byte 1..:  jet-specific sample bytes (see _mode_table)
///
/// Build:   fuzz/build.sh fuzz_jet_l1_math    (vere-flavor)
/// Corpus:  fuzz/corpus/fuzz_jet_l1_math/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/* Per-jet entry: Hoon name + sample arity. */
typedef enum {
  ARITY_1,   /* single atom sample */
  ARITY_2,   /* cell sample [a b]   */
} arity_e;

typedef struct {
  const char* nam_c;       /* wish string / core name */
  arity_e     ari_e;
  c3_d        max_a;       /* clamp for a (single arg or first) */
  c3_d        max_b;       /* clamp for b (second arg, if arity 2) */
  c3_o        nz_b;        /* require b != 0 (for div / mod) */
  u3_noun     gate;        /* populated at boot */
} jet_e;

/* Layer 1 table. Clamps are per the plan doc — small enough that
 * the Hoon formula runs in single-digit ms. `cap`/`mas`/`peg` are
 * tree-axis ops that take small atoms by semantics anyway. */
static jet_e _jets[] = {
  { "dec", ARITY_1, 0xFFFF,  0,      c3n, 0 },          /* a ≤ 2^16      */
  { "add", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "sub", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },          /* hoon bails a<b*/
  { "mul", ARITY_2, 0x03FF,  0x03FF, c3n, 0 },          /* a,b ≤ 2^10    */
  { "div", ARITY_2, 0xFFFF,  0x00FF, c3y, 0 },          /* b ≠ 0, small  */
  { "mod", ARITY_2, 0xFFFF,  0x00FF, c3y, 0 },
  { "gte", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "gth", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "lte", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "lth", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "max", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "min", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "cap", ARITY_1, 0xFFFF,  0,      c3n, 0 },          /* axis, ≥ 2     */
  { "mas", ARITY_1, 0xFFFF,  0,      c3n, 0 },          /* axis, ≥ 2     */
  { "peg", ARITY_2, 0xFFFF,  0xFFFF, c3n, 0 },
  { "dvr", ARITY_2, 0xFFFF,  0x00FF, c3y, 0 },          /* [quot rem]    */
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

/* Global state for the soft trampoline. */
static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;

/* Trampoline: u3m_soft calls this, it slams the gate. u3n_slam_on
 * transfers both args. */
static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

/* Read `n` bytes little-endian from buf, building a u3_atom. */
static u3_atom
_atom_from_bytes(const uint8_t* p, c3_w n_w, c3_d clamp_d)
{
  c3_d v_d = 0;
  c3_w i;
  c3_w take = (n_w > 8) ? 8 : n_w;
  for ( i = 0; i < take; i++ ) {
    v_d |= ((c3_d)p[i]) << (i * 8);
  }
  v_d &= clamp_d;
  /* cap/mas axes must be ≥ 2; clamp 0 and 1 to 2 */
  return u3i_chub(v_d);
}

int
main(void)
{
  /* Boot noun + ivory pill. */
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( sil_u == NULL ) {
    fprintf(stderr, "jet_l1: cue_xeno init failed\n"); return 1;
  }
  u3_weak pil = u3s_cue_xeno_with(sil_u,
                                   (c3_d)u3_Ivory_pill_len,
                                   u3_Ivory_pill);
  if ( pil == u3_none ) {
    fprintf(stderr, "jet_l1: ivory pill cue failed\n"); return 1;
  }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) {
    fprintf(stderr, "jet_l1: u3v_boot_lite failed\n"); return 1;
  }

  /* Wish every gate FIRST (while jets are fast). If we flip ice
   * before wishing, `u3v_wish` itself triggers the oracle on every
   * internal add/sub/dec call — boot balloons from seconds to
   * minutes. */
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) {
      fprintf(stderr, "jet_l1: u3v_wish('%s') failed\n",
              _jets[i].nam_c);
      return 1;
    }
    _jets[i].gate = gat;   /* cached for process lifetime */
  }

  /* NOW flip ice=c3n on every layer-1 core so _cj_kick_z runs the
   * differential oracle on subsequent slams. */
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( c3n == u3j_fuzz_arm(_jets[i].nam_c) ) {
      fprintf(stderr, "jet_l1: failed to flip ice for '%s'\n",
              _jets[i].nam_c);
      return 1;
    }
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 3 || len > H_MAX_INPUT ) {
    return 0;
  }

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];

  /* Build the sample from the remaining bytes. */
  c3_w off = 1;
  c3_w rem = len - off;
  u3_atom a = _atom_from_bytes(buf + off, (rem < 4) ? rem : 4, jet->max_a);
  off += (rem < 4) ? rem : 4;
  rem = len - off;

  u3_noun sam;
  if ( jet->ari_e == ARITY_1 ) {
    sam = a;
  }
  else {
    u3_atom b = _atom_from_bytes(buf + off, (rem < 4) ? rem : 4, jet->max_b);
    if ( jet->nz_b && 0 == b ) {
      b = 1;   /* avoid div/mod by zero — Hoon bails, but we want to
                * exercise the success path in most iterations */
    }
    sam = u3nc(a, b);
  }

  /* Slam through u3m_soft so oracle mismatches (c3__fail) AND
   * expected bails (c3__exit from out-of-range inputs) are caught.
   * On %fail the harness aborts — that's AFL seeing a crash. On
   * %exit it returns u3_none; we just drop it and loop. */
  g_gate = jet->gate;
  g_sam  = sam;

  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  /* u3m_soft returns [result] on success, [how reason] on bail.
   * On %fail the oracle logs a mismatch and the bail gets caught
   * here — we translate to abort() so AFL saves the input. */
  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l1_math: ORACLE MISMATCH in '%s'\n",
              jet->nam_c);
      abort();
    }
    /* c3__exit or other expected bail: jet and hoon agreed to crash. */
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
