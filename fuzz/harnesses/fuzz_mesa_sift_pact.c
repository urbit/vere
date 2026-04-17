/// @file fuzz_mesa_sift_pact.c
///
/// H7 — AFL++ harness for mesa_sift_pact_from_buf, the entry point
/// for parsing a mesa packet from raw UDP bytes. Mesa is the newer
/// transport (replacing/augmenting Ames) and has been the source of
/// recent OOB read bugs (see git log: "mesa: fix out of bounds read
/// in packet parsing").
///
/// Setup mirrors pkg/vere/io/mesa/pact_test.c::_setup() exactly:
///   1. u3C.wag_w |= u3o_hashless          (skip hashcons in cue)
///   2. u3m_boot_lite(1 << 26)              (64 MiB loom)
///   3. u3s_cue_xeno_with(ivory_pill, ...) (cue the ivory pill)
///   4. u3v_boot_lite(pil)                  (boot the kernel)
///
/// Per iteration: mesa_sift_pact_from_buf(&pac, buf, len) and
/// mesa_free_pact(&pac). We use fork mode rather than persistent
/// because mesa pact parsing allocates and a clean per-iter state is
/// safer than risking accumulation in the slab pools.
///
/// Build:   fuzz/build.sh fuzz_mesa_sift_pact
/// Corpus:  fuzz/corpus/fuzz_mesa_sift_pact/
/// Dict:    fuzz/dicts/mesa.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"
#include "io/mesa/mesa.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

int
main(void)
{
  /* Same boot sequence as pkg/vere/io/mesa/pact_test.c::_setup. */
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if (sil_u == NULL) {
    fprintf(stderr, "fuzz_mesa: cue_xeno init failed\n");
    return 1;
  }

  u3_weak pil = u3s_cue_xeno_with(sil_u,
                                   (c3_d)u3_Ivory_pill_len,
                                   u3_Ivory_pill);
  if (pil == u3_none) {
    fprintf(stderr, "fuzz_mesa: ivory pill cue failed\n");
    return 1;
  }
  u3s_cue_xeno_done(sil_u);

  if (c3n == u3v_boot_lite(pil)) {
    fprintf(stderr, "fuzz_mesa: u3v_boot_lite failed\n");
    return 1;
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  u3_mesa_pact pac_u;
  memset(&pac_u, 0, sizeof(pac_u));

  /* mesa_sift_pact_from_buf returns NULL on success or an error
   * string on parse failure. We treat both as "ok, not a crash" —
   * real bugs surface via ASan. */
  c3_c* err_c = mesa_sift_pact_from_buf(&pac_u, (c3_y*)buf, (c3_w)len);
  (void)err_c;

  /* mesa_free_pact() is declared in mesa.h but never defined (see
   * pkg/vere/io/mesa/pact.c:930 — only referenced inside the
   * MESA_ROUNDTRIP conditional). We're in fork mode so per-iter
   * allocations get GC'd when the child exits anyway. */

  return 0;
}
