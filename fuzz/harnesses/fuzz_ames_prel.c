/// @file fuzz_ames_prel.c
///
/// H28 — AFL++ harness for _ames_sift_prel.
///
/// _ames_sift_prel parses the ames packet prelude that follows the
/// 4-byte header: an optional 6-byte origin lane (when rel_o is set),
/// one life-tick byte, a sender ship of 2<<sac_y bytes, and a
/// receiver ship of 2<<rac_y bytes.  The total prelude length is
/// computed by _ames_prel_size from the header fields.
///
/// Fuzzer strategy: use the first 4 bytes as the header, compute the
/// expected prelude size from the parsed header, and call
/// _ames_sift_prel on the remainder.  Reject inputs too short to hold
/// the prelude.
///
/// Pattern: fuzz_ames_sift_packet (#include "./io/ames.c" trick).
///
/// Build:   fuzz/build.sh fuzz_ames_prel
/// Corpus:  fuzz/corpus/fuzz_ames_prel/
/// Min:     5 bytes (4 header + at least 1 prelude byte)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"
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

  if ( len < 4 || len > H_MAX_INPUT ) {
    return 0;
  }

  /* Parse the header to determine how many bytes the prelude needs. */
  u3_head hed_u = {0};
  _ames_sift_head(&hed_u, (c3_y*)buf);

  c3_y pre_w = _ames_prel_size(&hed_u);

  /* sac_y and rac_y are 2-bit fields (0-3), so pre_w is at most
   * 6 + 1 + 32 + 32 = 71 bytes.  Reject if the remaining bytes are
   * not enough. */
  if ( (size_t)len < (size_t)(4 + pre_w) ) {
    return 0;
  }

  u3_prel pre_u = {0};
  _ames_sift_prel(&hed_u, &pre_u, (c3_y*)buf + 4);

  return 0;
}
