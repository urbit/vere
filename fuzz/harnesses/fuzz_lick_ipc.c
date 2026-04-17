/// @file fuzz_lick_ipc.c
///
/// H25 — AFL++ harness for the Lick IPC dispatch validation in
/// `_lick_moor_poke` (`pkg/vere/io/lick.c:161`).
///
/// Lick is the named-port IPC mechanism gall agents use to talk to
/// local processes. Simpler surface than conn (H15): cue the input
/// and check `[nam dat]` outer shape. The full dispatch hands off
/// to `u3_auto_plan` and a pier-side ovum, which we skip.
///
/// Like H15 we use `u3s_cue_bytes` instead of `u3s_cue_xeno_with`
/// so the call works inside u3m_soft (xeno asserts home-road).
///
/// Build:   fuzz/build.sh fuzz_lick_ipc
/// Corpus:  fuzz/corpus/fuzz_lick_ipc/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_lick_soft(u3_noun arg)
{
  (void)arg;

  u3_noun put = u3s_cue_bytes(g_len, g_buf);
  u3_noun nam, dat;

  /* _lick_moor_poke's only shape check is `u3r_cell(put, &nam, &dat)`.
   * If it fails, bal_f gets called with -2 / "put-bad". Otherwise
   * the noun is handed off to u3_auto_plan. */
  (void)u3r_cell(put, &nam, &dat);

  u3z(put);
  return u3_nul;
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

  u3_noun pro = u3m_soft(0, _lick_soft, u3_nul);
  u3z(pro);

  return 0;
}
