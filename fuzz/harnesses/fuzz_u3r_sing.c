/// @file fuzz_u3r_sing.c
///
/// H22 — AFL++ harness for `u3r_sing`, u3's structural noun
/// equality. Called on every u3h (hashtable) lookup, and it's the
/// walker that unifies noun references across the loom. Bugs here
/// would be in the handling of hash-cons edge cases, cell-descent
/// boundaries, and atom byte comparisons.
///
/// Harness design: split the fuzz input in two at a selector byte,
/// cue each half into a noun, then call `u3r_sing` on the pair.
/// The fuzzer is incentivised (via coverage) to construct inputs
/// where both cues succeed — most mutations are rejected quickly,
/// so the interesting state is a small slice of the mutation space.
///
/// Build:   fuzz/build.sh fuzz_u3r_sing
/// Corpus:  fuzz/corpus/fuzz_u3r_sing/

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
static int            g_split = 0;

static u3_noun
_sing_soft(u3_noun arg)
{
  (void)arg;

  /* Split the input at g_split into two chunks; cue each. */
  c3_d len_a = (c3_d)g_split;
  c3_d len_b = g_len - len_a;
  if (len_a == 0 || len_b == 0) {
    return u3_nul;
  }

  u3_noun a = u3s_cue_bytes(len_a, g_buf);
  u3_noun b = u3s_cue_bytes(len_b, g_buf + len_a);

  /* If either cue bailed, the soft wrapper caught it and we'd not
   * be here. Both should be valid nouns at this point. */
  (void)u3r_sing(a, b);

  /* Also call the reverse direction — should be symmetric; if
   * ever asymmetric, that's a bug. */
  (void)u3r_sing(b, a);

  u3z(a);
  u3z(b);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 3 || len > H_MAX_INPUT) {
    return 0;
  }

  /* Byte 0 picks the split point. Scale it so splits are distributed
   * across the input. */
  int payload_len = len - 1;
  int split = ((int)buf[0] * payload_len) / 256;
  if (split <= 0) split = 1;
  if (split >= payload_len) split = payload_len - 1;

  g_buf   = (const uint8_t*)(buf + 1);
  g_len   = (c3_d)payload_len;
  g_split = split;

  u3_noun pro = u3m_soft(0, _sing_soft, u3_nul);
  u3z(pro);

  return 0;
}
