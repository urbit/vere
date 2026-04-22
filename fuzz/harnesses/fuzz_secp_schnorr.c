/// @file fuzz_secp_schnorr.c
///
/// H29 — AFL++ harness for the secp256k1 Schnorr signature-verification
/// jet (`u3we_sove` in `pkg/noun/jets/e/secp.c`).
///
/// `sove` (schnorr-verify) is the simpler of the two schnorr entry
/// points because it only reads data — no RNG, no private key. Sample:
///   pub (axis 2)  — 32-byte x-coordinate of public key
///   mes (axis 6)  — 32-byte message hash
///   sig (axis 7)  — 64-byte signature
///
/// Axis layout in a gate-shaped core [bat [sam ctx]]:
///   sam = [pub [mes sig]]
///   axis 12  = sam_2 = pub
///   axis 26  = sam_6 = mes
///   axis 27  = sam_7 = sig
///
/// Fixed layout from fuzz buffer (128 bytes):
///   [0..31]   → pub  (32 bytes, secp256k1 x-only pubkey)
///   [32..63]  → mes  (32 bytes, message hash)
///   [64..127] → sig  (64 bytes, Schnorr signature)
///
/// u3je_secp_init() is required before any secp jet.
///
/// Build:   fuzz/build.sh fuzz_secp_schnorr
/// Corpus:  fuzz/corpus/fuzz_secp_schnorr/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3we_sove(u3_noun cor);
extern void    u3je_secp_init(void);

__AFL_FUZZ_INIT();

#define H_MIN_INPUT 128   /* 32 + 32 + 64 */
#define H_MAX_INPUT 4096

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_sove_soft(u3_noun ignored)
{
  (void)ignored;

  u3_atom pub = u3i_bytes(32, g_buf);
  u3_atom mes = u3i_bytes(32, g_buf + 32);
  u3_atom sig = u3i_bytes(64, g_buf + 64);

  /* Build sam = [pub [mes sig]].
   * Axis map:
   *   axis 12 = sam_2 = pub  (head of sam)           ✓
   *   axis 26 = sam_6 = mes  (head of tail of sam)   ✓
   *   axis 27 = sam_7 = sig  (tail of tail of sam)   ✓
   */
  u3_noun sam  = u3nt(pub, mes, sig);
  u3_noun core = u3nc(u3_nul, u3nc(sam, u3_nul));

  u3_noun pro = u3we_sove(core);

  /* pro is %.y / %.n or bailed; either way, free. */
  u3z(pro);
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

  u3_noun pro = u3m_soft(0, _sove_soft, u3_nul);
  u3z(pro);

  return 0;
}
