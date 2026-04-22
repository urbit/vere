/// @file fuzz_jet_trip.c
///
/// H — AFL++ harness for the `trip` jet
/// (`u3qe_trip` in `pkg/noun/jets/e/trip.c`).
///
/// `trip` converts a UTF-8 atom (cord) into a tape — a list of @c
/// codepoints produced by `rip(3, 1, a)`.  Adversarial bytes can
/// expose integer overflows in the bit-rip logic or malformed
/// multi-byte codepoint handling.
///
/// Input construction: fuzz bytes → atom via u3i_bytes, passed
/// directly to u3qe_trip.  The jet expects a plain atom (cord), so
/// no additional wrapping is needed.
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill needed).
///
/// Build:   fuzz/build.sh fuzz_jet_trip
/// Corpus:  fuzz/corpus/fuzz_jet_trip/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Forward-declare the jet entry rather than pulling in the full
 * jet-table header. */
extern u3_noun u3qe_trip(u3_atom a);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_trip_soft(u3_noun arg)
{
  (void)arg;
  /* Build a cord (atom) from the raw fuzz bytes and hand it to the
   * trip jet.  u3qe_trip owns the reference; it does not u3z its
   * argument, so we u3z(a) below after the result is consumed. */
  u3_atom a   = u3i_bytes((c3_w)g_len, g_buf);
  u3_noun pro = u3qe_trip(u3k(a));
  u3z(a);
  return pro;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _trip_soft, u3_nul);
  u3z(pro);

  return 0;
}
