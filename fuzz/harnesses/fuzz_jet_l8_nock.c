/// @file fuzz_jet_l8_nock.c
///
/// Differential jet fuzzer for Nock interpreters (`mink`, `mule`,
/// `mole`). These jets ARE the nock virtual machine — they take a
/// subject and a formula and evaluate it. For the ice oracle this
/// means: the C jet interprets nock, and the Hoon formula (itself
/// a nock AST) also interprets nock. Both should agree.
///
/// A naive "compile fuzz bytes and compare" is slightly circular
/// because mink's Hoon formula CALLS mink recursively via %=
/// reconvergence. With our nested-test short-circuit
/// (u3j_Fuzz_testing) that's fine — inner mink calls take the jet
/// fast path while we compare the outer.
///
/// Samples are extremely structured:
///   - mink: [[subject=* formula=*] scry=gate]
///   - mule: [try-gate=gate]
///   - mole: [gate]
///
/// For mink we cue fuzz bytes as `formula` (allowing all nock-valid
/// trees to emerge over time) and use a fixed subject=0 and
/// scry=~. We clamp formula size via the cue input length.
///
/// Build:  fuzz/build.sh fuzz_jet_l8_nock
/// Corpus: fuzz/corpus/fuzz_jet_l8_nock/

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
#define H_MAX_CUE_BYTES 512   /* bound the formula we feed mink */

typedef struct {
  const char* nam_c;
  const char* cor_c;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "mink", "mink", 0 },
  /* mule and mole are slammable-gate-wrappers whose sample is a
   * whole gate noun; harder to construct from fuzz bytes. Start
   * with mink only. */
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;
static const uint8_t* g_cue_buf = 0;
static c3_d           g_cue_len = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

static u3_noun
_cue_soft(u3_noun ignored)
{
  (void)ignored;
  return u3s_cue_bytes(g_cue_len, g_cue_buf);
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
  u3_cue_xeno* sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( !sil ) return 1;
  u3_weak pil = u3s_cue_xeno_with(sil, u3_Ivory_pill_len, u3_Ivory_pill);
  if ( pil == u3_none ) return 1;
  u3s_cue_xeno_done(sil);
  if ( c3n == u3v_boot_lite(pil) ) return 1;

  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) { _jets[i].gate = 0; continue; }
    _jets[i].gate = gat;
  }
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].cor_c);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 4 || len > H_MAX_INPUT ) return 0;

  /* First 4 bytes: length prefix for cue input (little-endian u32).
   * Remaining bytes: candidate jam stream, clamped to H_MAX_CUE_BYTES. */
  c3_w cue_len = (c3_w)buf[0]
              | ((c3_w)buf[1] << 8)
              | ((c3_w)buf[2] << 16)
              | ((c3_w)buf[3] << 24);
  cue_len %= (len - 3);
  if ( cue_len == 0 ) return 0;
  if ( cue_len > H_MAX_CUE_BYTES ) cue_len = H_MAX_CUE_BYTES;

  /* Wrap cue in u3m_soft — a malformed jam can trip a %meme bail
   * (unbounded allocation) which is NOT an oracle mismatch; it's
   * just the decoder rejecting input, so we drop it silently. */
  g_cue_buf = buf + 4;
  g_cue_len = (c3_d)cue_len;
  u3_noun cue_result = u3m_soft(0, _cue_soft, u3_nul);
  if ( 0 != u3h(cue_result) ) { u3z(cue_result); return 0; }
  u3_weak formula = u3k(u3t(cue_result));
  u3z(cue_result);
  if ( u3_none == formula ) return 0;

  /* Build mink sample: [[subject formula] scry-gate]
   * subject = ~, scry = ~ (no scry). */
  u3_noun sub_form = u3nc(u3_nul, formula);
  u3_noun sam      = u3nc(sub_form, u3_nul);

  g_gate = _jets[0].gate;
  g_sam  = sam;
  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l8_nock: ORACLE MISMATCH in 'mink'\n");
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
