/// @file fuzz_secp_reco.c
///
/// H28 — AFL++ harness for the secp256k1 pubkey-recovery jet
/// (`u3we_reco` in `pkg/noun/jets/e/secp.c`).
///
/// Recovery is the cheapest way for any peer to trigger the secp256k1
/// code path: no private key is needed. The sample noun has shape
/// [has [siv [sir sis]]] where:
///   has (axis 2)  — 32-byte hash
///   siv (axis 6)  — recovery id (0-3, single atom)
///   sir (axis 14) — 32-byte r component of signature
///   sis (axis 15) — 32-byte s component of signature
///
/// Axis layout in a gate-shaped core [bat [sam ctx]]:
///   sam = [has [siv [sir sis]]]
///   axis 12  = sam_2  = has
///   axis 13  = sam_3  = [siv [sir sis]]
///   axis 26  = sam_6  = siv
///   axis 27  = sam_7  = [sir sis]
///   axis 54  = sam_14 = sir
///   axis 55  = sam_15 = sis
///
/// Fixed layout from fuzz buffer (128 bytes + 1 byte for siv):
///   [0]        → siv  (recovery id, we AND with 0x03)
///   [1..32]    → has  (32 bytes)
///   [33..64]   → sir  (32 bytes)
///   [65..96]   → sis  (32 bytes)
///
/// u3je_secp_init() must be called before any secp jet; we call it
/// after u3m_boot_lite.
///
/// Build:   fuzz/build.sh fuzz_secp_reco
/// Corpus:  fuzz/corpus/fuzz_secp_reco/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3we_reco(u3_noun cor);
extern void    u3je_secp_init(void);

__AFL_FUZZ_INIT();

#define H_MIN_INPUT 97   /* 1 + 32 + 32 + 32 */
#define H_MAX_INPUT 4096

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_reco_soft(u3_noun ignored)
{
  (void)ignored;

  /* siv must be in [0,3]; AND with 0x03 keeps the fuzzer in range
   * without hard-rejecting inputs — this keeps coverage on the
   * internal recovery logic rather than the early-exit guard. */
  c3_y siv_y = g_buf[0] & 0x03;

  u3_atom has = u3i_bytes(32, g_buf + 1);
  u3_atom sir = u3i_bytes(32, g_buf + 33);
  u3_atom sis = u3i_bytes(32, g_buf + 65);
  u3_atom siv = (u3_atom)siv_y;

  /* Build sam = [has [siv [sir sis]]]
   * Axis map (gate core [bat [sam ctx]]):
   *   axis 12  = sam_2  = has
   *   axis 26  = sam_6  = siv
   *   axis 54  = sam_14 = sir
   *   axis 55  = sam_15 = sis
   * sam = [has [siv [sir sis]]] gives:
   *   head(sam)                 = has   → axis 12 ✓
   *   head(tail(sam))           = siv   → axis 26 ✓
   *   head(tail(tail(sam)))     = sir   → axis 54 ✓
   *   tail(tail(tail(sam)))     = sis   → axis 55 ✓
   */
  u3_noun sam  = u3nq(has, siv, sir, sis); /* [has siv sir sis] 4-tuple */
  u3_noun core = u3nc(u3_nul, u3nc(sam, u3_nul));

  u3_noun pro = u3we_reco(core);

  if (u3_none != pro) u3z(pro);
  u3z(core);

  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);
  u3je_secp_init();

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  if (len < H_MIN_INPUT || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _reco_soft, u3_nul);
  u3z(pro);

  return 0;
}
