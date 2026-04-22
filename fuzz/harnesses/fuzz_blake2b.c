/// @file fuzz_blake2b.c
///
/// H31 — AFL++ harness for the BLAKE2b hashing jet
/// (`u3we_blake2b` in `pkg/noun/jets/e/blake.c`).
///
/// BLAKE2b is called by Urbit for its internal content-addressing and
/// is reachable by any peer that sends data through gall/arvo. The jet
/// takes three sample fields:
///   msg (axis 2)  — octs cell [wid dat]
///   key (axis 6)  — octs cell [wik dak]  (up to 64 bytes)
///   out (axis 7)  — output length atom (1..64)
///
/// Inner structure:
///   msg = [wid dat]   wid = byte-length of dat
///   key = [wik dak]   wik = byte-length of dak (0 = no key)
///
/// Axis layout in a gate-shaped core [bat [sam ctx]]:
///   sam = [msg [key out]]
///   axis 12 = sam_2 = msg
///   axis 26 = sam_6 = key
///   axis 27 = sam_7 = out
///
/// Input layout from fuzz bytes:
///   [0]        → out_len (1..64, clamped)
///   [1]        → key_len (0..64, clamped; 0 = unkeyed hash)
///   [2..end]   → data payload; first key_len bytes → key,
///                remaining bytes → message data
///
/// Minimum input: 2 bytes.
///
/// Build:   fuzz/build.sh fuzz_blake2b
/// Corpus:  fuzz/corpus/fuzz_blake2b/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3we_blake2b(u3_noun cor);

__AFL_FUZZ_INIT();

#define H_MIN_INPUT  2
#define H_MAX_INPUT  65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_blake2b_soft(u3_noun ignored)
{
  (void)ignored;

  /* Parse header bytes. */
  uint8_t out_raw = g_buf[0];
  uint8_t key_raw = g_buf[1];

  /* Clamp output length to [1, 64]. */
  c3_w out_w = (c3_w)(out_raw & 0x3f) + 1;  /* 1..64 */

  /* Clamp key length to [0, 64]. */
  c3_w wik_w = (c3_w)(key_raw & 0x3f);      /* 0..63 (BLAKE2b max key = 64) */

  /* Remaining bytes after the 2-byte header. */
  c3_d payload_len = (g_len >= 2) ? g_len - 2 : 0;
  const uint8_t* payload = g_buf + 2;

  /* Split payload: first wik_w bytes → key, rest → message. */
  if ((c3_d)wik_w > payload_len) {
    wik_w = (c3_w)payload_len;
  }
  c3_w wid_w = (c3_w)(payload_len - (c3_d)wik_w);

  const uint8_t* key_p = payload;
  const uint8_t* dat_p = payload + wik_w;

  /* Build noun atoms. */
  u3_atom wid_a = wid_w;
  u3_atom dat_a = u3i_bytes(wid_w, dat_p);
  u3_atom wik_a = wik_w;
  u3_atom dak_a = u3i_bytes(wik_w, key_p);
  u3_atom out_a = out_w;

  /* Construct octs cells. */
  u3_noun msg = u3nc(wid_a, dat_a);  /* [wid dat] */
  u3_noun key = u3nc(wik_a, dak_a);  /* [wik dak] */

  /* Build sam = [msg [key out]].
   * Axis map (gate core [bat [sam ctx]]):
   *   axis 12 = sam_2 = msg  (head of sam)           ✓
   *   axis 26 = sam_6 = key  (head of tail of sam)   ✓
   *   axis 27 = sam_7 = out  (tail of tail of sam)   ✓
   */
  u3_noun sam  = u3nt(msg, key, out_a);
  u3_noun core = u3nc(u3_nul, u3nc(sam, u3_nul));

  u3_noun pro = u3we_blake2b(core);

  if (u3_none != pro) u3z(pro);
  u3z(core);

  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  if (len < H_MIN_INPUT || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _blake2b_soft, u3_nul);
  u3z(pro);

  return 0;
}
