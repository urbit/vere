/// @file fuzz_jet_leer.c
///
/// H — AFL++ harness for the `leer` jet
/// (`u3qe_leer` in `pkg/noun/jets/e/leer.c`).
///
/// `leer` takes an atom (cord) and splits it into a list of lines on
/// newline (0x0a) boundaries.  The implementation walks bytes with
/// `u3r_byte` and builds sub-atoms with `_leer_cut`; adversarial
/// input can expose off-by-one errors, length arithmetic bugs, or
/// allocator edge cases in `u3i_defcons` / `u3i_slab_mint_bytes`.
///
/// Input construction: fuzz bytes → atom via u3i_bytes, passed
/// directly to u3qe_leer.  The jet takes a plain atom (cord), not a
/// tape, so no conversion through `trip` is required.
///
/// Both u3qe_leer and u3qe_lore live in leer.c.
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill needed).
///
/// Build:   fuzz/build.sh fuzz_jet_leer
/// Corpus:  fuzz/corpus/fuzz_jet_leer/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Forward-declare the jet entry rather than pulling in the full
 * jet-table header. */
extern u3_noun u3qe_leer(u3_atom txt);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_leer_soft(u3_noun arg)
{
  (void)arg;
  /* Build a cord (atom) from the raw fuzz bytes and hand it to
   * u3qe_leer.  We u3k the atom so that u3qe_leer can consume its
   * reference while we retain ours for u3z below. */
  u3_atom txt = u3i_bytes((c3_w)g_len, g_buf);
  u3_noun pro = u3qe_leer(u3k(txt));
  u3z(txt);
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

  u3_noun pro = u3m_soft(0, _leer_soft, u3_nul);
  u3z(pro);

  return 0;
}
