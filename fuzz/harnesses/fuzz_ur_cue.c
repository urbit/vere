/// @file fuzz_ur_cue.c
///
/// H1 — AFL++ harness for ur_cue, the off-loom cue (bit-stream
/// deserialisation) parser in pkg/ur.
///
/// This is the simplest possible Vere fuzz target:
///   - no u3 loom
///   - no signal handlers
///   - no setjmp/longjmp
///
/// Known caveats addressed by this harness:
///
///   * ur_root_t holds an interned-atoms table that grows with every
///     cue call. Sharing it across persistent-mode iterations causes
///     AFL++ coverage instability (the same input takes different
///     branches depending on whether an atom is already interned).
///     Fix: free+reinit the root each iteration.
///
///   * cue blindly trusts the length prefix inside the jam stream.
///     A 9-byte input can legitimately claim "allocate 8 PB" and
///     _oom() will abort via calloc-returns-NULL. The fuzzer found
///     this within 10 seconds; see
///     fuzz/findings/001-ur_cue-alloc-size-too-big/ for the report.
///
///     Vere's loom maxes out at 16 GiB, so any cue input that requests
///     a larger allocation is unambiguously bogus. We bound ASan's
///     max_allocation_size_mb to 16384 (set by run.sh) so anything
///     above that threshold crashes loudly as a real bug.
///
///     The harness still caps input length to avoid each iteration
///     pulling in megabytes of input, which would crater throughput.
///
/// Build:   fuzz/build.sh fuzz_ur_cue
/// Corpus:  fuzz/corpus/ur_cue/
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* read(2) for __AFL_FUZZ_TESTCASE_LEN */

#include "ur.h"

__AFL_FUZZ_INIT();

/* Cap inputs at 64 KiB — packet-sized, roomy enough for any realistic
 * jammed noun worth fuzzing, but bounded so bogus length claims don't
 * tank throughput. */
#define H_MAX_INPUT 65536

int
main(void)
{
  /* Deferred forkserver — AFL takes its snapshot here, after libc
   * startup is done but before we start touching state. */
  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;

  while (__AFL_LOOP(10000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    if (len < 1) continue;
    if (len > H_MAX_INPUT) continue;

    /* Fresh root per iteration — ur_cue_with wipes the backref dict but
     * not the hashcons table on the root. Cheap: ~microseconds. */
    ur_root_t* r = ur_root_init();
    if (r == NULL) continue;

    ur_cue_t* c = ur_cue_init(r);
    if (c == NULL) {
      ur_root_free(r);
      continue;
    }

    ur_nref ref;
    (void)ur_cue_with(c, (uint64_t)len, buf, &ref);

    ur_cue_done(c);
    ur_root_free(r);
  }

  return 0;
}
