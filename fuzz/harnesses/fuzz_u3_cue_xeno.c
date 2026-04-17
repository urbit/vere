/// @file fuzz_u3_cue_xeno.c
///
/// H5 — AFL++ harness for u3s_cue_xeno_with, the off-loom cue path
/// used by ames/mesa/conn for incoming network and IPC bytes.
///
/// Differences from H4 (u3s_cue_bytes):
///   - Uses a u3_cue_xeno handle that holds an off-loom backref
///     dictionary (ur_dict32_t). H4 uses on-loom backref state.
///   - Different control flow inside _cs_cue_xeno_next, with a
///     32-bit dict instead of 64-bit.
///   - Returns u3_none on parse failure instead of bailing.
///
/// We share H4's seed corpus and dictionary because both target jam
/// bytes — any input that's interesting for one is interesting for
/// the other.
///
/// Build:   fuzz/build.sh fuzz_u3_cue_xeno
/// Corpus:  fuzz/corpus/fuzz_u3_cue_xeno/
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/* Persistent xeno handle reused across the (forked) process. We init
 * once before the AFL forkserver snapshot. */
static u3_cue_xeno* g_sil = NULL;

int
main(void)
{
  u3m_boot_lite(1 << 24);

  /* xeno handle holds the off-loom dictionary; it persists across
   * child forks because it sits before __AFL_INIT. */
  g_sil = u3s_cue_xeno_init();
  if (g_sil == NULL) {
    return 1;
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  /* u3s_cue_xeno_with returns u3_none on parse failure (no bail).
   * On success it returns the cued noun, which we free. */
  u3_weak out = u3s_cue_xeno_with(g_sil, (c3_d)len, (const c3_y*)buf);
  if (u3_none != out) {
    u3z(out);
  }

  return 0;
}
