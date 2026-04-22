/// @file fuzz_jet_l4a_crypto.c
///
/// Differential jet fuzzer for layer-4a cryptographic hashing and
/// checksum jets. Most of these accept raw atoms (and sometimes a
/// length hint) so random bytes produce valid samples easily.
///
/// Covered: `shax` (sha256), `shay` (keyed sha256), `shal` (keyed
/// sha512), `sha1`, `ripe` (ripemd160), `crc32`, `adler32`, `mug`.
///
/// Build:  fuzz/build.sh fuzz_jet_l4a_crypto
/// Corpus: fuzz/corpus/fuzz_jet_l4a_crypto/

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

typedef enum {
  S_ATOM,         /* one atom                          */
  S_WID_DAT,      /* cell (wid=@ud dat=@)              */
} shape_e;

typedef struct {
  const char* nam_c;
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

/* Only hashes exposed at the ivory-pill top level are fuzzable via
 * u3v_wish. `ripe`, `crc32`, `adler32`, `sha1` are not — they live
 * in deeper stdlib cores that the ivory pill doesn't re-export. */
static jet_e _jets[] = {
  { "shax",    S_ATOM,    0 },
  { "shay",    S_WID_DAT, 0 },
  { "shal",    S_WID_DAT, 0 },
  { "mug",     S_ATOM,    0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( !sil_u ) { fprintf(stderr, "l4a: cue init\n"); return 1; }
  u3_weak pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len, u3_Ivory_pill);
  if ( pil == u3_none ) { fprintf(stderr, "l4a: pill cue\n"); return 1; }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) { fprintf(stderr, "l4a: boot\n"); return 1; }

  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) {
      fprintf(stderr, "l4a: u3v_wish('%s') failed — skipping\n", _jets[i].nam_c);
      _jets[i].gate = 0;   /* skip at runtime */
      continue;
    }
    _jets[i].gate = gat;
  }

  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].nam_c);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_INPUT ) {
    return 0;
  }

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  /* Cap payload size to 1 KiB — hash jets are O(len), and the oracle
   * runs the Hoon side which is much slower. Beyond 1 KiB the oracle
   * takes too long and AFL times out. */
  c3_w pay_len = len - 1;
  if ( pay_len > 1024 ) pay_len = 1024;
  const uint8_t* pay_p = buf + 1;

  u3_atom dat = u3i_bytes(pay_len, pay_p);
  u3_noun sam;

  if ( jet->shp_e == S_ATOM ) {
    sam = dat;
  }
  else {
    /* (wid dat) — wid is the declared byte length, which we tie to the
     * actual payload length for a well-formed call. */
    sam = u3nc(u3i_word(pay_len), dat);
  }

  g_gate = jet->gate;
  g_sam  = sam;

  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l4a_crypto: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
