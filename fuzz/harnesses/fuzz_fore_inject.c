/// @file fuzz_fore_inject.c
///
/// H23 — AFL++ harness for the `_fore_inject` shape-validation
/// logic in `pkg/vere/io/fore.c`. Vere's `-I` flag injects a jam'd
/// event file from disk; a malicious file crashes the ship at
/// startup.
///
/// _fore_inject calls `u3ke_cue(u3m_file(...))` to parse the file,
/// then walks the result expecting `[[tar wir] cad]` shape where:
///   - tar is an atom ≤ 4 bytes
///   - wir is a cell / some shape
///   - cad is [tag dat]
///
/// We skip `u3m_file` (that's just file I/O) and feed the fuzz bytes
/// directly to `u3ke_cue`, then run the same shape-validation
/// sequence the real code does. u3m_soft catches cue bails.
///
/// Build:   fuzz/build.sh fuzz_fore_inject
/// Corpus:  fuzz/corpus/fuzz_fore_inject/
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_inject_soft(u3_noun arg)
{
  (void)arg;

  /* Cue the fuzz bytes as if they came from u3m_file. u3s_cue_bytes
   * (on-loom) is what u3ke_cue internally calls; we use it directly
   * so the harness can run inside u3m_soft (xeno requires the home
   * road). */
  u3_noun ovo = u3s_cue_bytes(g_len, g_buf);

  u3_noun riw, cad, tar, wir;

  /* Mirror _fore_inject's shape validation exactly. */
  if (c3n == u3r_cell(ovo, &riw, &cad)) {
    /* "invalid ovum in -I" */
  }
  else if ((c3n == u3a_is_cell(cad)) ||
           (c3n == u3a_is_atom(u3h(cad)))) {
    /* "invalid card in -I ovum" */
  }
  else if (c3n == u3r_cell(riw, &tar, &wir)) {
    /* "invalid wire in -I ovum" */
  }
  else if ((c3n == u3a_is_atom(tar)) ||
           (4 < u3r_met(3, tar))) {
    /* "invalid target in -I wire" */
  }
  else {
    /* Would call u3do("spat", ...) and u3_auto_plan here, but
     * neither is available without a real pier. The validation
     * we just did is the actual bug surface; the pier-side work
     * beyond is covered by its own harnesses. */
  }

  u3z(ovo);
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

  u3_noun pro = u3m_soft(0, _inject_soft, u3_nul);
  u3z(pro);

  return 0;
}
