/// @file fuzz_nock_mink.c
///
/// H27 — AFL++ harness for the Nock interpreter jet `u3we_mink`
/// (`pkg/noun/jets/e/mink.c`).
///
/// This is the highest-risk target: a peer can supply an arbitrary
/// (subject, formula, scry_gate) triple and the jet will execute it
/// under the full Nock interpreter.  Crafted inputs can:
///   - loop forever on tail-call cycles
///   - allocate enormous noun trees
///   - recurse until stack overflow
///
/// Mitigations applied here:
///   1. u3m_soft() with a 64 MiB stack budget (mil_w = 64<<20) so
///      runaway stack use bails cleanly rather than SIGSEGV.
///   2. AFL's default 2000 ms (or -t 1000 at fuzz time) kills hangs;
///      the harness itself does not need a timer.
///   3. The formula is decoded with u3s_cue_bytes from the fuzz input.
///      If cue fails u3_none is returned and we exit quickly.
///   4. The subject is hardcoded to u3_nul (0) and the scry gate to
///      u3_nul to avoid giving the interpreter a hook it can call into
///      unknown code.
///
/// Input layout (all bytes, no framing needed):
///   [0..3]  : 4-byte little-endian length prefix for the formula bytes.
///             Clamped to remaining input length.
///   [4..4+n]: formula, decoded via u3s_cue_bytes.
///   The subject is always 0 (~) and scry gate is always ~ (u3_nul).
///
/// Build:   fuzz/build.sh fuzz_nock_mink   (noun-flavor)
/// Corpus:  fuzz/corpus/fuzz_nock_mink/
/// Fuzz:    afl-fuzz -t 1000 ... -- ./fuzz_nock_mink

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* The mink sample sits at:
 *   u3x_sam_4 (24) => bus (subject)
 *   u3x_sam_5 (25) => fol (formula)
 *   u3x_sam_3 (13) => gul (scry gate)
 * u3we_mink expects a gate core where the sample is [bus fol gul].
 * Rather than fabricating a full gate core we call u3n_nock_et
 * directly (which is what the jet does internally) wrapped in
 * u3m_soft for the bail fence. */
extern u3_noun u3n_nock_et(u3_noun gul, u3_noun bus, u3_noun fol);

__AFL_FUZZ_INIT();

/* 64 KiB cap on raw input keeps cue decode fast. */
#define H_MAX_INPUT   65536
/* 64 MiB soft stack budget — generous enough for real code but
 * small enough to bail before an OOM kills the process. */
#define H_STACK_BUDGET (64u << 20)

static const uint8_t* g_buf;
static c3_w           g_fol_len;

/*
 * _mink_soft: called inside u3m_soft's bail fence.
 *
 * arg is unused; we use globals to avoid loom-allocation of the
 * raw byte buffer before the noun is constructed.
 */
static u3_noun
_mink_soft(u3_noun arg)
{
  (void)arg;

  /* Decode the formula from the fuzz bytes. */
  u3_noun fol = u3s_cue_bytes(g_fol_len, g_buf);
  if ( u3_none == fol ) {
    /* Invalid cue — not interesting, bail cleanly. */
    return u3m_bail(c3__exit);
  }

  /* subject = 0 (u3_nul), scry gate = 0 (u3_nul). */
  u3_noun bus = u3_nul;
  u3_noun gul = u3_nul;

  /* u3n_nock_et returns a ++toon: [%0 res] | [%1 ~] | [%2 trace]
   * Any bail inside will be caught by u3m_soft above. */
  u3_noun toon = u3n_nock_et(u3k(gul), u3k(bus), u3k(fol));

  u3z(toon);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* Need at least a 4-byte length prefix and 1 byte of payload. */
  if ( len < 5 || len > H_MAX_INPUT ) {
    return 0;
  }

  /* Read formula length from the first 4 bytes (little-endian). */
  uint32_t fol_len;
  memcpy(&fol_len, buf, sizeof(fol_len));
  /* Clamp to remaining bytes. */
  uint32_t avail = (uint32_t)(len - 4);
  if ( fol_len == 0 || fol_len > avail ) {
    fol_len = avail;
  }

  g_buf     = (const uint8_t*)(buf + 4);
  g_fol_len = (c3_w)fol_len;

  /* u3m_soft mil_w=H_STACK_BUDGET — bails are not crashes. */
  u3_noun pro = u3m_soft(H_STACK_BUDGET, _mink_soft, u3_nul);
  u3z(pro);

  return 0;
}
