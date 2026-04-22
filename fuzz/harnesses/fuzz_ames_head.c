/// @file fuzz_ames_head.c
///
/// H27 — AFL++ harness for _ames_sift_head.
///
/// _ames_sift_head is a 4-byte bit-field parser: it sifts one 32-bit
/// word from the wire into the u3_head struct (req_o, sim_o, ver_y,
/// sac_y, rac_y, mug_l, rel_o). Any byte sequence is structurally
/// valid input; bugs manifest as incorrect field extraction rather
/// than crashes. ASan + UBSan will catch shifts, OOB, or sign issues.
///
/// Pattern: fuzz_ames_sift_packet (#include "./io/ames.c" trick).
/// The harness does NOT link libvere.a — ames.c is compiled directly
/// into this translation unit.  Any ames.c references to symbols not
/// in libnoun (u3_Host, u3_ovum_*, mdns_*) are satisfied by libvere
/// via -z muldefs inside build_vere_harness.
///
/// Build:   fuzz/build.sh fuzz_ames_head
/// Corpus:  fuzz/corpus/fuzz_ames_head/
/// Min:     4 bytes (exactly one header word)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"
/* Pull every static helper from ames.c into this translation unit. */
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

  /* Need exactly 4 bytes for the header word. */
  if ( len < 4 || len > H_MAX_INPUT ) {
    return 0;
  }

  u3_head hed_u = {0};
  _ames_sift_head(&hed_u, (c3_y*)buf);

  return 0;
}
