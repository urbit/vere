/// @file fuzz_fine_wail.c
///
/// H30 — AFL++ harness for _fine_sift_wail.
///
/// _fine_sift_wail parses the body of a FINE wail (request) packet:
///
///   byte 0           tag (must be 0)
///   bytes 1..4       fragment number (little-endian u32)
///   bytes 5..6       path-length field (little-endian u16)
///   bytes 7..7+len-1 ASCII scry path
///
/// The function navigates the packet via pac_u->hun_y (the raw wire
/// buffer) starting at cur_w (the byte offset past header + prelude).
/// It validates total packet length against pac_u->len_w.
///
/// Harness setup: treat the entire fuzz input as the wail body (i.e.
/// cur_w == 0, pac_u->len_w == input length, pac_u->hun_y == buf).
/// This directly exercises all length and content checks inside
/// _fine_sift_wail without needing a full ames pipeline.  If
/// _fine_sift_wail returns c3y it allocates pat_c with c3_calloc;
/// we free it afterward to avoid a leak that would accumulate across
/// persistent-mode iterations (AFL++ fork mode makes this optional
/// but it's cleaner).
///
/// Pattern: fuzz_ames_sift_packet (#include "./io/ames.c" trick).
///
/// Build:   fuzz/build.sh fuzz_fine_wail
/// Corpus:  fuzz/corpus/fuzz_fine_wail/
/// Min:     7 bytes (tag + fra_w + len_s fields)

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

  /* _fine_sift_wail needs at least fra_w (4) + len_s (2) + tag (1)
   * = 7 bytes before it reaches the path-length check. */
  if ( len < 7 || len > H_MAX_INPUT ) {
    return 0;
  }

  /* Synthesize a minimal u3_pact.  We only need the fields that
   * _fine_sift_wail reads:
   *   pac_u->hun_y   — pointer to the packet buffer
   *   pac_u->len_w   — total packet length
   * cur_w is 0 because we are handing the function exactly the
   * wail body starting at offset 0. */
  u3_pact pac_u = {0};
  pac_u.hun_y = (c3_y*)buf;
  pac_u.len_w = (c3_w)len;

  c3_o ok = _fine_sift_wail(&pac_u, 0);

  /* Free path string allocated by c3_calloc inside _fine_sift_wail
   * on success, to keep the process heap tidy across fork iterations. */
  if ( c3y == ok && pac_u.wal_u.pep_u.pat_c ) {
    c3_free(pac_u.wal_u.pep_u.pat_c);
  }

  return 0;
}
