/// @file fuzz_serial_etch_roundtrip.c
///
/// AFL++ differential harness for @ud etch/sift round-trip.
///
/// Takes arbitrary fuzz bytes, builds an atom `a`, runs:
///   cord = u3s_etch_ud(a)   ← format as decimal ASCII
///   a2   = u3s_sift_ud(cord) ← parse decimal ASCII back
/// and asserts `a == a2` via u3r_sing. Any inequality is a real
/// encoder/decoder drift bug: either etch produced text that sift
/// rejects (parser over-strict), or sift accepted text that doesn't
/// round-trip to the original value (parser wrong).
///
/// This is the cheapest possible bug-finding density for serial.c:
/// two functions, one oracle, no state to set up.
///
/// Edge cases the fuzzer should find:
///   - leading zeros handling
///   - dot-separator placement at non-3-digit boundaries
///   - the small-int / mpz boundary crossing (values near 2^63)
///   - empty / length-1 cords
///
/// Build: fuzz/build.sh fuzz_serial_etch_roundtrip   (noun-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define H_MAX_ATOM_BYTES 512     /* keeps etch text under ~1.5k chars */

static unsigned char* g_buf = NULL;
static int            g_len = 0;

static u3_noun
_roundtrip_soft(u3_noun arg)
{
  (void)arg;

  /* Build source atom from fuzz bytes. */
  u3_atom a = u3i_bytes((c3_w)g_len, (c3_y*)g_buf);

  /* Etch to @ud cord. Never returns u3_none for valid atoms. */
  u3_atom cord = u3s_etch_ud(a);

  /* Sift the cord back. Should always succeed for cords we just
   * produced; u3_none here is a bug. */
  u3_weak a2 = u3s_sift_ud(u3k(cord));

  if ( u3_none == a2 ) {
    u3l_log("fuzz_serial_etch_roundtrip: sift rejected own etch output");
    abort();
  }

  if ( c3n == u3r_sing(a, a2) ) {
    u3l_log("fuzz_serial_etch_roundtrip: round-trip value mismatch");
    abort();
  }

  u3z(a); u3z(cord); u3z(a2);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 1 || len > H_MAX_ATOM_BYTES ) {
    return 0;
  }

  g_buf = buf;
  g_len = len;

  u3_noun pro = u3m_soft(0, _roundtrip_soft, u3_nul);
  u3z(pro);

  return 0;
}
