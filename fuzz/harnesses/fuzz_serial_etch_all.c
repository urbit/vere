/// @file fuzz_serial_etch_all.c
///
/// AFL++ harness for the four atom-to-text etch functions in
/// `pkg/noun/serial.c`:
///   - u3s_etch_ud  (decimal)
///   - u3s_etch_ux  (hex)
///   - u3s_etch_uv  (base-32)
///   - u3s_etch_uw  (base-64)
///
/// Most aura fuzzers target parsers (sift direction). Etch functions
/// are less obviously attack-surface, but they have their own risks:
///
///   - u3s_etch_ud_smol uses a 26-byte stack buffer. Arithmetic
///     pointing past the buffer would clobber the stack.
///   - Large indirect atoms go through `mpz_get_*` conversions and
///     `u3i_slab_bare` allocations that depend on `_cs_etch_*_size`
///     returning accurate length estimates.
///   - uv / uw radix conversions with block padding are fiddly.
///
/// Input layout:
///   [0]   mode byte (% 4 selects etch variant)
///   [1..] atom bytes (up to 512 to keep output text bounded)
///
/// Build: fuzz/build.sh fuzz_serial_etch_all   (noun-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define H_MAX_ATOM_BYTES 512

/* Declare etch prototypes manually — serial.h doesn't expose etch_uv
 * / etch_uw at all public-header level; they live in serial.c. */
extern u3_atom u3s_etch_ud(u3_atom a);
extern u3_atom u3s_etch_ux(u3_atom a);
extern u3_atom u3s_etch_uv(u3_atom a);
extern u3_atom u3s_etch_uw(u3_atom a);

static unsigned char* g_buf = NULL;
static int            g_len = 0;
static uint8_t        g_mode = 0;

static u3_noun
_etch_soft(u3_noun arg)
{
  (void)arg;

  u3_atom a = u3i_bytes((c3_w)g_len, (c3_y*)g_buf);
  u3_atom cord;

  switch ( g_mode & 0x03 ) {
    default:
    case 0: cord = u3s_etch_ud(a); break;
    case 1: cord = u3s_etch_ux(a); break;
    case 2: cord = u3s_etch_uv(a); break;
    case 3: cord = u3s_etch_uw(a); break;
  }

  /* Force a read of the output bytes so ASan sees any OOB produced
   * by the etch. u3r_met walks the atom's word buffer. */
  (void)u3r_met(3, cord);

  u3z(a);
  u3z(cord);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_ATOM_BYTES + 1 ) {
    return 0;
  }

  g_mode = buf[0];
  g_buf  = buf + 1;
  g_len  = len - 1;

  u3_noun pro = u3m_soft(0, _etch_soft, u3_nul);
  u3z(pro);

  return 0;
}
