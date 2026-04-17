/// @file fuzz_u3_cue_bytes.c
///
/// H4 — AFL++ harness for u3s_cue_bytes, u3's noun-level cue.
///
/// Unlike H1-H3, this harness needs a u3 loom: u3m_boot_lite sets up
/// the allocator, road, and jet system before we can call any u3
/// functions. The signal handler install is gated out at compile time
/// by -DU3_FUZZ (propagated through -Dfuzz in build.zig), so ASan
/// traps reach us directly.
///
/// cue failures call u3m_bail, which longjmps back to a setjmp set
/// up by u3m_soft. We wrap the target call in u3m_soft so that
/// malformed inputs return a [%fail trace] pair instead of crashing
/// the harness. Actual memory-safety bugs still SIGABRT via ASan,
/// not via longjmp.
///
/// State management:
///   - u3m_boot_lite is called ONCE, before the AFL forkserver snapshot.
///   - We use deferred forkserver (__AFL_INIT) WITHOUT __AFL_LOOP, so
///     each test case runs in a fresh forked child from the post-boot
///     snapshot. The alternative (persistent-mode loop with
///     u3m_reclaim between iterations) accumulates loom state — cue
///     walks past u3m_reclaim's cache-clearing and eventually trips
///     the loom guard page, which with U3_FUZZ has no handler to
///     service the growth. Fork mode is ~10x slower but reliably
///     stateless.
///
/// This is the first hybrid-build harness: it links against
/// zig-out/lib/libnoun.a (built with -DU3_FUZZ, no AFL instrumentation)
/// plus our own afl-clang-fast build of pkg/ur. We lose coverage
/// feedback inside pkg/noun code, but ASan still catches memory bugs
/// there via shadow-memory on the heap. If noun-level coverage becomes
/// the bottleneck we can upgrade to full afl instrumentation of
/// pkg/noun/*.c later.
///
/// Build:   fuzz/build.sh fuzz_u3_cue_bytes
/// Corpus:  fuzz/corpus/fuzz_u3_cue_bytes/
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* read(2) for __AFL_FUZZ_TESTCASE_LEN */

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/*
 * Funk adapter for u3m_soft. We stash the fuzz input in a thread-local
 * and the soft wrapper just calls u3s_cue_bytes(len, byt) on it.
 */
static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_cue_soft(u3_noun arg)
{
  /* arg is unused — u3m_soft hands us an atom we don't need. */
  u3z(arg);
  return u3s_cue_bytes(g_len, g_buf);
}

int
main(void)
{
  /* One-time u3 setup before AFL forks. 16 MiB loom matches
   * pkg/noun/serial_tests.c. */
  u3m_boot_lite(1 << 24);

  /* Deferred forkserver: AFL snapshots after boot. The input still
   * comes through the shared-memory channel set up by
   * __AFL_FUZZ_INIT(); we read it via __AFL_FUZZ_TESTCASE_BUF/LEN
   * rather than via stdin for consistency with H1-H3. */
  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = buf;
  g_len = (c3_d)len;

  /* u3m_soft wraps the call in a setjmp. On cue failure, u3m_bail
   * longjmps back and u3m_soft returns [%fail trace]. On success
   * it returns [%blip result]. */
  u3_noun pro = u3m_soft(0, _cue_soft, u3_nul);
  u3z(pro);

  return 0;
}
