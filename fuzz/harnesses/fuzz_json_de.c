/// @file fuzz_json_de.c
///
/// H14 — AFL++ harness for the JSON decoder jet
/// (`u3qe_json_de` in `pkg/noun/jets/e/json_de.c`).
///
/// JSON parsing is the highest-yield single target in the codebase
/// because it ingests bytes from the wide network via:
///   - eyre HTTP request bodies
///   - scry results piped through gall agents
///   - external API responses
///
/// The jet takes an atom (cord) of JSON bytes and returns either
/// `[~ parsed]` or `~`. We wrap the call in `u3m_soft` so that
/// invalid JSON paths that bail (via u3m_bail / u3l_log + meme)
/// produce a clean `[%fail trace]` instead of crashing the harness.
/// Real memory-safety bugs still SIGABRT via ASan.
///
/// Build:   fuzz/build.sh fuzz_json_de
/// Corpus:  fuzz/corpus/fuzz_json_de/
/// Dict:    fuzz/dicts/json.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Forward-declare the jet entry. It's in pkg/noun/jets/q.h but
 * including that header pulls in a ton of jet-table machinery; the
 * one-line forward decl is cleaner. */
extern u3_noun u3qe_json_de(u3_atom a);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_de_soft(u3_noun arg)
{
  (void)arg;
  /* Build a cord (atom) from the fuzz bytes and hand it to the
   * decoder. u3i_bytes consumes (len, ptr) and returns an atom. */
  u3_atom cord = u3i_bytes((c3_w)g_len, g_buf);
  return u3qe_json_de(cord);
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _de_soft, u3_nul);
  u3z(pro);

  return 0;
}
