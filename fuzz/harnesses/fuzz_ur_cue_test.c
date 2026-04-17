/// @file fuzz_ur_cue_test.c
///
/// H2 — AFL++ harness for ur_cue_test, the parse-only variant of cue.
///
/// Differences from H1:
///   - No hashcons root, no allocation (ur_cue_test just validates
///     the bit-stream without building the resulting noun).
///   - Reusable ur_cue_test_t handle; no per-iteration teardown.
///   - ~5x the throughput of H1.
///
/// Why fuzz this separately from H1:
///   ur_cue_test exercises the same bit-stream reader logic (rub, tag,
///   backref offsets) but skips atom allocation. Bugs that live in the
///   bitstream layer are found faster here, and bugs whose symptoms
///   are masked by allocation ordering in H1 (e.g., heap-invariant
///   drift before a crash eventually fires) show up cleaner.
///
/// Build:   fuzz/build.sh fuzz_ur_cue_test
/// Corpus:  fuzz/corpus/fuzz_ur_cue_test/  (shares shape with H1)
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* read(2) for __AFL_FUZZ_TESTCASE_LEN */

#include "ur.h"

__AFL_FUZZ_INIT();

/* Same cap as H1 — packet-sized, bounded per-iteration work. */
#define H_MAX_INPUT 65536

int
main(void)
{
  /* ur_cue_test has no root; just a handle holding a backref dict and
   * a small stack. The handle is reusable across iterations and there
   * is no leaking state (ur_cue_test_with wipes both at the end). */
  ur_cue_test_t* t = ur_cue_test_init();
  if (t == NULL) {
    return 1;
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;

  while (__AFL_LOOP(10000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    if (len < 1) continue;
    if (len > H_MAX_INPUT) continue;

    (void)ur_cue_test_with(t, (uint64_t)len, buf);
  }

  ur_cue_test_done(t);
  return 0;
}
