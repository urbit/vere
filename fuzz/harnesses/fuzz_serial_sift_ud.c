/// @file fuzz_serial_sift_ud.c
///
/// AFL++ harness for `u3s_sift_ud_bytes` (pkg/noun/serial.c:1360) —
/// the C-level parser for the @ud (decimal unsigned) aura. This is
/// the underlying parser that the Hoon %ud slaw jet dispatches to,
/// and it's directly reachable with attacker-controlled bytes
/// whenever a peer can get us to parse a decimal cord (which is
/// routine in HTTP URLs, Arvo events, JSON numbers routed through
/// Hoon, etc.).
///
/// The parser has several sub-loops:
///   - +ape:ag / +ted:ab handling the 1–3 leading digits
///   - a small-integer fast path (atom ≤ 2^63, no gmp)
///   - a gmp-backed big-integer path with mpz_mul_ui/mpz_add_ui
///   - dot-prefixed 3-digit block scanning
/// Off-by-one, overflow, or malformed-input bugs here would directly
/// crash anything that calls `(slaw %ud text)`.
///
/// Input: raw bytes, passed verbatim to u3s_sift_ud_bytes.
///
/// Build: fuzz/build.sh fuzz_serial_sift_ud   (noun-flavor)
/// Corpus: fuzz/corpus/fuzz_serial_sift_ud/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static unsigned char* g_buf = NULL;
static int            g_len = 0;

static u3_noun
_sift_soft(u3_noun arg)
{
  (void)arg;
  u3_weak res = u3s_sift_ud_bytes((c3_w)g_len, (c3_y*)g_buf);
  if ( u3_none != res ) {
    u3z(res);
  }
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 1 || len > H_MAX_INPUT ) {
    return 0;
  }

  g_buf = buf;
  g_len = len;

  u3_noun pro = u3m_soft(0, _sift_soft, u3_nul);
  u3z(pro);

  return 0;
}
