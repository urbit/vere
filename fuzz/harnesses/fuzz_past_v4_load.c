/// @file fuzz_past_v4_load.c
///
/// H13 — AFL++ harness for u3_v4_load + u3a_v4_ream, the past v4
/// snapshot loader. Vere uses these to read pre-3.0 snapshots
/// during boot migration. A crafted on-disk snapshot crashes the
/// ship at startup.
///
/// Architectural challenge: u3_v4_load and friends read from a
/// fixed loom address `u3_Loom_v4 = u3_Loom + (1 << u3a_bits_max)`
/// (16 GiB above the live loom). The harness mmap's this region
/// MAP_FIXED at the expected address, copies the fuzz input into
/// it, then calls the loader. Both u3_Loom and u3_Loom_v4 sit in
/// ASan's HighMem range so MAP_FIXED should succeed without
/// clobbering ASan shadow memory.
///
/// **Known limitation**: this harness is "needs corpus" rather than
/// productive out-of-the-box. The functions we target only do
/// interesting work when the v4 loom contains a valid
/// `u3v_v4_home` + `u3a_v4_road` graph with internal pointer
/// offsets that resolve back inside the loom region. Random fuzz
/// bytes look like garbage allocator metadata and `u3a_v4_ream`
/// either does nothing or follows a bogus pointer (crashing only
/// in the "interesting" case which is rare with random input).
///
/// To make H13 productive, seed `fuzz/corpus/fuzz_past_v4_load/`
/// with a real v4 snapshot (or a synthetically-constructed minimal
/// one). The fuzzer's mutations will then rotate around a valid
/// shape and find real bugs. Without seeds, this harness mainly
/// proves the MAP_FIXED + loader plumbing works; coverage stays
/// near 0%.
///
/// Build:   fuzz/build.sh fuzz_past_v4_load
/// Corpus:  fuzz/corpus/fuzz_past_v4_load/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "noun.h"
#include "past/v4.h"
#include "past/migrate.h"

__AFL_FUZZ_INIT();

/* The v4 loom region needs to be word-aligned and at least a few
 * pages — we use 1 MiB which is more than enough for any 64 KiB
 * fuzz input. */
#define V4_LOOM_BYTES (1u << 20)
#define V4_LOOM_WORDS (V4_LOOM_BYTES / sizeof(c3_w))

#define H_MAX_INPUT 65536

static c3_w* g_v4_loom = NULL;

int
main(void)
{
  u3m_boot_lite(1 << 24);

  /* Map the v4 loom at the address u3_v4_load expects. If MAP_FIXED
   * fails (e.g. because ASan reserved this region), we can't run
   * H13 — abort cleanly so the user knows. */
  void* want = (void*)u3_Loom_v4;
  void* got = mmap(want, V4_LOOM_BYTES,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                   -1, 0);
  if (got == MAP_FAILED || got != want) {
    fprintf(stderr,
            "fuzz_past_v4_load: mmap MAP_FIXED at %p failed (got %p)\n",
            want, got);
    return 1;
  }
  g_v4_loom = (c3_w*)got;

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 4 || len > H_MAX_INPUT) {
    return 0;
  }

  /* Lay out the v4 loom: zero everything, then copy the fuzz input
   * into the start. Stamp the version word at the end so
   * u3_v4_load's `assert(U3V_VER4 == *(u3_Loom_v4 + wor_i - 1))`
   * passes — otherwise we just trip the assert immediately and
   * never exercise the loader proper. */
  memset(g_v4_loom, 0, V4_LOOM_BYTES);
  memcpy(g_v4_loom, buf, (size_t)len);

  c3_z wor_i = V4_LOOM_WORDS;
  g_v4_loom[wor_i - 1] = U3V_VER4;

  /* Run the loader. It sets u3H_v4 and u3R_v4 to point into the
   * loom region. If it returns successfully, we proceed to ream. */
  u3_v4_load(wor_i);

  /* u3a_v4_ream walks the noun graph in the v4 loom and rebuilds
   * pointer state. This is where bogus pointers in the loom would
   * surface as crashes. */
  u3a_v4_ream();

  return 0;
}
