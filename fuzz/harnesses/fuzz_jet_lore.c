/// @file fuzz_jet_lore.c
///
/// H — AFL++ harness for the `lore` jet
/// (`u3qe_lore` in `pkg/noun/jets/e/leer.c`).
///
/// `lore` is the legacy line-splitter; unlike `leer` it splits on
/// both newline (0x0a) and null (0x00) and bails with %exit if a
/// null byte appears in the interior of the atom rather than at the
/// terminal position.  The interaction between the two termination
/// conditions is a natural spot for logic bugs (off-by-one on the
/// `(pos_w + meg_w + 1) < len_w` guard, integer wrap on `meg_w`,
/// etc.).
///
/// Input construction: fuzz bytes → atom via u3i_bytes, passed
/// directly to u3qe_lore.  Like leer, the jet takes a plain atom
/// (cord).
///
/// Note: u3qe_lore lives in pkg/noun/jets/e/leer.c (no separate
/// lore.c).  Both symbols are exported from libnoun.
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill needed).
///
/// Build:   fuzz/build.sh fuzz_jet_lore
/// Corpus:  fuzz/corpus/fuzz_jet_lore/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Forward-declare the jet entry; both u3qe_lore and u3qe_leer are
 * exported from libnoun (compiled from jets/e/leer.c). */
extern u3_noun u3qe_lore(u3_atom lub);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_lore_soft(u3_noun arg)
{
  (void)arg;
  /* Build a cord (atom) from the raw fuzz bytes and hand it to
   * u3qe_lore.  We u3k the atom so that u3qe_lore can consume its
   * reference while we retain ours for u3z below. */
  u3_atom lub = u3i_bytes((c3_w)g_len, g_buf);
  u3_noun pro = u3qe_lore(u3k(lub));
  u3z(lub);
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

  u3_noun pro = u3m_soft(0, _lore_soft, u3_nul);
  u3z(pro);

  return 0;
}
