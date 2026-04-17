/// @file fuzz_disk_sift.c
///
/// H11 — AFL++ harness for u3_disk_sift, the event log replay parser.
/// Each persisted event is [4-byte mug][jam bytes]. During replay
/// the disk layer walks the lmdb event log and calls u3_disk_sift on
/// each entry; any crash here means a corrupted or malicious event
/// log aborts the ship at startup.
///
/// The source has a known `// XX u3m_soft?` comment at
/// pkg/vere/disk.c:329 flagging that u3ke_cue can u3m_bail out of
/// this function without a soft wrapper. We paper over that in the
/// harness by wrapping the disk_sift call in u3m_soft.
///
/// Build:   fuzz/build.sh fuzz_disk_sift
/// Corpus:  fuzz/corpus/fuzz_disk_sift/
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static size_t         g_len = 0;

static u3_noun
_sift_soft(u3_noun arg)
{
  (void)arg;
  c3_l    mug_l;
  u3_noun job;

  /* We pass NULL for log_u because u3_disk_sift only touches its
   * `XX check version in log_u` comment — the pointer isn't
   * dereferenced in the current implementation. If that changes
   * we'll need to supply a real u3_disk. */
  (void)u3_disk_sift(NULL, g_len, (c3_y*)g_buf, &mug_l, &job);
  return job;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 5 || len > H_MAX_INPUT) {
    /* disk_sift rejects anything <= 4 bytes; we need at least 5. */
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (size_t)len;

  u3_noun pro = u3m_soft(0, _sift_soft, u3_nul);
  u3z(pro);

  return 0;
}
