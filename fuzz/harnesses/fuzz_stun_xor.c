/// @file fuzz_stun_xor.c
///
/// H32 — AFL++ harness for u3_stun_find_xor_mapped_address.
///
/// u3_stun_find_xor_mapped_address scans a STUN response buffer for
/// the XOR-MAPPED-ADDRESS TLV attribute (type 0x0020, length 0x0008)
/// and extracts IP:port after XOR-ing with the magic cookie.
///
/// The parser uses memmem to locate the 4-byte type+length tag within
/// the attribute chain starting at byte 20 (after the 20-byte STUN
/// header).  It then reads the 2-byte family, 2-byte XOR-port, and
/// 4-byte XOR-IP without bounds-checking the individual field accesses
/// beyond the initial len_w < 40 guard.
///
/// This dedicated harness provides much denser coverage than the
/// combined fuzz_stun_response (H9) because the fuzzer's entire byte
/// budget goes toward inputs that pass the 40-byte minimum and have
/// the XOR-MAPPED-ADDRESS tag somewhere in the attribute chain.
///
/// Pattern: vere-flavor (links libvere.a; u3_stun_find_xor_mapped_address
/// is a public symbol declared in io/ames/stun.h).
///
/// Build:   fuzz/build.sh fuzz_stun_xor
/// Corpus:  fuzz/corpus/fuzz_stun_xor/
/// Dict:    fuzz/dicts/stun.dict
/// Min:     40 bytes

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

  /* The function has an early-return guard at len_w < 40.  Enforce
   * a minimum here to keep the fuzzer focused on the interesting
   * attribute-scanning code rather than the trivial rejection path. */
  if ( len < 40 || len > H_MAX_INPUT ) {
    return 0;
  }

  u3_lane lan_u = {0};
  (void)u3_stun_find_xor_mapped_address((c3_y*)buf, (c3_w)len, &lan_u);

  return 0;
}
