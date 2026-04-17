/// @file fuzz_scot_slaw.c
///
/// H16 — AFL++ harness for Hoon's atom-text parsers in
/// `pkg/noun/jets/e/slaw.c`. These convert text representations
/// like `~zod`, `1.234`, `~2020.1.1`, `foo-bar` into atoms.
/// Hoon code calls them whenever it parses a ship name, a number
/// from text, a date from text, etc. Adversarial-by-construction
/// input flows through them constantly.
///
/// We dispatch to one of several sub-parsers based on byte 0:
///
///   0  → _parse_p     (ship names: @p)
///   1  → _parse_tas   (terms: @tas)
///   2  → u3s_sift_ud  (decimal numbers: @ud)
///   3  → _parse_da    (dates: @da)  — calls u3j_cook which
///        needs a Hoon core; we wrap in u3m_soft and accept the
///        bail as a no-op.
///
/// The parsers are static in slaw.c; we bring them in via
/// `#include "jets/e/slaw.c"`. The vere harness build's
/// -Wl,-z,muldefs handles the symbol collision with libnoun.a's
/// copy.
///
/// Build:   fuzz/build.sh fuzz_scot_slaw
/// Corpus:  fuzz/corpus/fuzz_scot_slaw/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"
/* Pull in the static parsers. */
#include "jets/e/slaw.c"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 4096

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;
static uint8_t        g_mode = 0;

static u3_noun
_parse_soft(u3_noun arg)
{
  (void)arg;

  /* Build a cord (atom) from the fuzz bytes. */
  u3_atom txt = u3i_bytes((c3_w)g_len, g_buf);

  switch (g_mode) {
    case 0: {
      /* _parse_p needs a `cor` parameter even though it doesn't
       * strictly require Hoon. We pass u3_nul as a placeholder; the
       * parser only uses it indirectly. */
      u3_noun res = _parse_p(u3_nul, txt);
      u3z(res);
      break;
    }
    case 1: {
      u3_noun res = _parse_tas(txt);
      u3z(res);
      break;
    }
    case 2: {
      u3_weak res = u3s_sift_ud(txt);
      if (u3_none != res) u3z(res);
      break;
    }
    case 3: {
      /* _parse_da calls u3j_cook + u3n_slam_on which need a real
       * Hoon core. With cor=u3_nul this will bail; u3m_soft catches
       * it. We get the parsing branches before the cook attempt. */
      u3_noun res = _parse_da(u3_nul, txt);
      u3z(res);
      break;
    }
  }

  u3z(txt);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 2 || len > H_MAX_INPUT) {
    return 0;
  }

  g_mode = buf[0] & 0x03;
  g_buf  = (const uint8_t*)(buf + 1);
  g_len  = (c3_d)(len - 1);

  u3_noun pro = u3m_soft(0, _parse_soft, u3_nul);
  u3z(pro);

  return 0;
}
