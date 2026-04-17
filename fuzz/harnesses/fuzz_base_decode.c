/// @file fuzz_base_decode.c
///
/// H20 — AFL++ harness for the base-16 decoder jet
/// (`u3qe_de_base16` in `pkg/noun/jets/e/base.c`).
///
/// Hex parsing is small but easy to off-by-one: there's a
/// special-cased odd vs even byte-length code path
/// (`_of_hex_odd` / `_of_hex_even`) which has historically been
/// the kind of code where bit-shifting bugs hide. The decoder
/// returns `~` on invalid input, so the harness just calls and
/// frees.
///
/// Build:   fuzz/build.sh fuzz_base_decode
/// Corpus:  fuzz/corpus/fuzz_base_decode/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3qe_de_base16(u3_atom inp);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_de_soft(u3_noun arg)
{
  (void)arg;
  u3_atom inp = u3i_bytes((c3_w)g_len, g_buf);
  u3_noun res = u3qe_de_base16(inp);
  u3z(res);
  u3z(inp);
  return u3_nul;
}

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

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _de_soft, u3_nul);
  u3z(pro);

  return 0;
}
