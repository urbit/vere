/// @file fuzz_ames_sift_packet.c
///
/// H8 — AFL++ harness for the ames classic UDP packet parser.
///
/// We use the same trick as pkg/vere/ames_tests.c:
///
///     #include "./io/ames.c"
///
/// This brings every static helper in ames.c into the harness's
/// compilation unit so we can call them directly. We target the
/// low-level sifters (_ames_sift_head, _ames_sift_prel) rather than
/// _ames_hear so the harness doesn't need a full u3_ames struct.
///
/// Link: the fuzz build provides libnoun.a etc.; libvere.a is NOT
/// linked for this harness because doing so would duplicate every
/// ames.c symbol. Anything ames.c references that isn't in libnoun
/// needs a weak stub below.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"
/* include ames.c directly to expose static helpers */
#include "io/ames.c"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 4 || len > H_MAX_INPUT) {
    return 0;
  }

  /* Parse the 4-byte header. The sifter doesn't touch fields beyond
   * the first 4 bytes. */
  u3_head hed_u = {0};
  _ames_sift_head(&hed_u, (c3_y*)buf);

  /* Parse the prelude if there's enough room. Prelude length is a
   * function of the header's sac/rac/rel fields. */
  c3_w pre_w = _ames_prel_size(&hed_u);
  if ((size_t)len < 4 + pre_w) {
    return 0;
  }
  u3_prel pre_u = {0};
  _ames_sift_prel(&hed_u, &pre_u, (c3_y*)buf + 4);

  return 0;
}
