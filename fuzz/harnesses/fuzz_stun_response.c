/// @file fuzz_stun_response.c
///
/// H9 — AFL++ harness for the STUN request/response parsers. STUN
/// packets arrive on the same UDP socket as Ames packets and are
/// dispatched first based on a simple format check.
///
/// We call all three parse entry points on each input:
///   - u3_stun_is_request(buf, len)
///   - u3_stun_is_our_response(buf, tid, len)
///   - u3_stun_find_xor_mapped_address(buf, len, &lane)
///
/// The make_* functions are encoders (known good); we don't fuzz them.
/// The public signatures all come from pkg/vere/io/ames/stun.h and
/// are already compiled into zig-out/lib/libvere.a.
///
/// Build:   fuzz/build.sh fuzz_stun_response
/// Corpus:  fuzz/corpus/fuzz_stun_response/
/// Dict:    fuzz/dicts/stun.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vere.h"
#include "io/ames/stun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

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

  /* Fake transaction ID — STUN response checking compares the input
   * against a known tid. Using a zero tid means we rarely pass the
   * "is our response" test but we still exercise the parser's
   * reject path. */
  c3_y tid_y[12] = {0};

  (void)u3_stun_is_request((c3_y*)buf, (c3_w)len);
  (void)u3_stun_is_our_response((c3_y*)buf, tid_y, (c3_w)len);

  u3_lane lan_u = {0};
  (void)u3_stun_find_xor_mapped_address((c3_y*)buf, (c3_w)len, &lan_u);

  return 0;
}
