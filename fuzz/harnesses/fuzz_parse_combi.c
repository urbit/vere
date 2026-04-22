/// @file fuzz_parse_combi.c
///
/// H28 — AFL++ harness for Hoon parser-combinator jets.
///
/// Targets (all from pkg/noun/jets/e/parse.c):
///   u3we_pfix  — prefix-consuming combinator: discards first parse result,
///               returns second.  Takes sample [vex sab] where vex is a
///               pre-computed $edge and sab is a rule gate.  Calls
///               u3n_slam_on(sab, quq_vex).
///   u3we_plug  — sequence combinator: applies sab to the nail left by
///               vex, pairs both parse values.  Same sample shape.
///   u3we_pose  — ordered-choice combinator: if vex succeeded return it,
///               otherwise kick sab (u3n_kick_on).  Same sample shape.
///
/// These three are the most attack-relevant:
///   - They take (vex, sab) directly at u3x_sam_2 / u3x_sam_3 — no
///     extra gate-context layer — so the harness can build the core
///     cheaply.
///   - They slam/kick user-supplied gate nouns: crafted sab with
///     pathological formulas can expose mismatch between the $edge
///     shape assumptions and what the jet reads.
///   - pfix and plug walk into quq_vex (the continuation nail) via
///     u3t(u3t(q_vex)), so an unusual $edge shape exercises the
///     u3x_cell descent checks.
///
/// Input layout:
///   byte 0   : op selector (0 => pfix, 1 => plug, 2 => pose, else skip)
///   bytes 1..split_vex: cue'd $edge   (vex)
///   bytes split_vex..: cue'd rule-gate (sab)
///   byte 1+1        : split point as fraction of payload length
///
/// A $edge is: [p=$hair q=(unit [p=* q=$nail])]
///   $hair = [line=@ col=@]
///   $nail = [p=$hair q=$tape]   where $tape = (list @)
///
/// The fuzzer will explore the valid-shape space through coverage;
/// invalid shapes bail cleanly through u3m_bail / u3m_soft.
///
/// Build:   fuzz/build.sh fuzz_parse_combi   (noun-flavor)
/// Corpus:  fuzz/corpus/fuzz_parse_combi/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3we_pfix(u3_noun cor);
extern u3_noun u3we_plug(u3_noun cor);
extern u3_noun u3we_pose(u3_noun cor);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/*
 * Globals for the soft wrapper.
 */
static const uint8_t* g_vex_buf;
static c3_w           g_vex_len;
static const uint8_t* g_sab_buf;
static c3_w           g_sab_len;
static uint8_t        g_op;

/*
 * _build_cor: construct a minimal gate core whose sample is [vex sab].
 *
 * The combinator jets read:
 *   u3x_sam_2 (12) => vex
 *   u3x_sam_3 (13) => sab
 * A gate core is [[battery] [sample] [context]].
 * address layout:  core=1, battery=2, payload=3, sample=6, context=7.
 * We build: [battery [vex sab] context] where battery and context are ~.
 */
static u3_noun
_build_cor(u3_noun vex, u3_noun sab)
{
  /* payload = [sample context] = [[vex sab] ~] */
  u3_noun sample  = u3nc(vex, sab);
  u3_noun payload = u3nc(sample, u3_nul);
  /* core = [battery payload] = [~ payload] */
  return u3nc(u3_nul, payload);
}

static u3_noun
_combi_soft(u3_noun arg)
{
  (void)arg;

  /* Decode vex ($edge) */
  u3_noun vex = u3s_cue_bytes(g_vex_len, g_vex_buf);
  if ( u3_none == vex ) {
    return u3m_bail(c3__exit);
  }

  /* Decode sab (rule gate) */
  u3_noun sab = u3s_cue_bytes(g_sab_len, g_sab_buf);
  if ( u3_none == sab ) {
    u3z(vex);
    return u3m_bail(c3__exit);
  }

  u3_noun cor = _build_cor(u3k(vex), u3k(sab));
  u3_noun pro;

  switch ( g_op % 3 ) {
    case 0:  pro = u3we_pfix(cor); break;
    case 1:  pro = u3we_plug(cor); break;
    default: pro = u3we_pose(cor); break;
  }

  u3z(pro);
  u3z(sab);
  u3z(vex);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* Need at least: op(1) + split(1) + 2 bytes payload */
  if ( len < 4 || len > H_MAX_INPUT ) {
    return 0;
  }

  g_op = buf[0];

  /* byte 1: split fraction for vex / sab boundary */
  int payload_len = len - 2;
  int split = ((int)buf[1] * payload_len) / 256;
  if ( split <= 0 ) split = 1;
  if ( split >= payload_len ) split = payload_len - 1;

  g_vex_buf = (const uint8_t*)(buf + 2);
  g_vex_len = (c3_w)split;
  g_sab_buf = (const uint8_t*)(buf + 2 + split);
  g_sab_len = (c3_w)(payload_len - split);

  u3_noun pro = u3m_soft(0, _combi_soft, u3_nul);
  u3z(pro);

  return 0;
}
