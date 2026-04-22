/// @file fuzz_argon2.c
///
/// H30 — AFL++ harness for the Argon2 key-derivation jet
/// (`u3we_argon2` in `pkg/noun/jets/e/argon2.c`).
///
/// MEMORY-BOMB WARNING: Argon2 allocates `mem_cost` KiB × `threads`
/// working memory. An attacker-supplied noun can request gigabytes.
/// We hard-clamp BEFORE calling into the jet:
///   mem_cost  ≤ 65536   (64 MiB)
///   time_cost ≤ 3
///   threads   ≤ 4
///   out_len   ≤ 64 bytes
///
/// The jet's inner function `_cqe_argon2` is static, so we must reach
/// it through the public wrapper `u3we_argon2`. However, `u3we_argon2`
/// extracts its arguments from a deeply nested core at axis 510, which
/// is impossible to synthesise portably from a harness. Instead we
/// include the jet source directly to get access to the static symbol,
/// and then call `_cqe_argon2` after clamping. We rename potential
/// symbol clashes using the -Wl,-z,muldefs approach already used in
/// fuzz_scot_slaw (the build recipe must add -Wl,-z,muldefs).
///
/// Input layout from fuzz bytes:
///   [0]        → type byte: 0=d, 1=i, 2=id, 3=u (mod 4)
///   [1]        → out_len (1..64, we clamp)
///   [2]        → time_cost (1..3, we clamp)
///   [3]        → threads (1..4, we clamp)
///   [4..5]     → mem_cost as uint16_t LE (clamped to 64 MiB)
///   [6..21]    → key (16 bytes; wik=16)
///   [22..37]   → extra/associated-data (16 bytes; wix=16)
///   [38..53]   → password/data (16 bytes; wid=16)
///   [54..69]   → salt (16 bytes; wis=16)
/// Total minimum: 70 bytes.
///
/// We also allow the fuzzer to supply longer buffers and use the extra
/// bytes as variable-length password (dat) by reading dat from
/// [54..54+dat_len] where dat_len = min(remaining, 256).
///
/// Build:   fuzz/build.sh fuzz_argon2
/// Corpus:  fuzz/corpus/fuzz_argon2/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Pull in the jet source so we can call the static _cqe_argon2.
 * -Wl,-z,muldefs handles the duplicate symbol with libnoun.a. */
#include "jets/e/argon2.c"

__AFL_FUZZ_INIT();

#define H_MIN_INPUT  70
#define H_MAX_INPUT  4096

/* Safety caps */
/* argon2 mem_cost is in KiB. Loom is 64 MiB, and argon2 allocates a
 * scratch buffer plus working state — 8 MiB keeps us well clear of
 * the loom boundary that otherwise segfaults in u3a_walloc. */
#define H_MAX_MEM    8192u   /* 8 MiB in KiB */
#define H_MAX_TIME   3u
#define H_MAX_THREADS 4u
#define H_MAX_OUT    64u
#define H_FIXED_LEN  16u     /* key, extra, pwd, salt fixed fields */

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

/* Map a 0..3 byte to an argon2 type atom (the Hoon term). */
static u3_atom
_type_atom(uint8_t b)
{
  switch (b & 0x03) {
    default:
    case 0: return c3__d;
    case 1: return c3__i;
    case 2: return c3__id;
    case 3: return c3__u;
  }
}

static u3_noun
_argon2_soft(u3_noun ignored)
{
  (void)ignored;

  /* --- parse the fixed header --- */
  uint8_t  type_b    = g_buf[0];
  uint8_t  out_b     = g_buf[1];
  uint8_t  time_b    = g_buf[2];
  uint8_t  thread_b  = g_buf[3];
  uint16_t mem_raw   = (uint16_t)(g_buf[4] | ((uint16_t)g_buf[5] << 8));

  /* Clamp safety parameters */
  c3_w out_w     = (c3_w)(out_b & 0x3f) + 1;           /* 1..64         */
  if (out_w > H_MAX_OUT) out_w = H_MAX_OUT;

  c3_w time_w    = (c3_w)(time_b % H_MAX_TIME) + 1;     /* 1..3          */
  c3_w threads_w = (c3_w)(thread_b % H_MAX_THREADS) + 1;/* 1..4          */
  c3_w mem_w     = (c3_w)(mem_raw ? mem_raw : 8);        /* keep non-zero */
  if (mem_w > H_MAX_MEM) mem_w = H_MAX_MEM;

  /* Fixed 16-byte fields */
  const uint8_t* key_p   = g_buf + 6;   /* wik=16 */
  const uint8_t* extra_p = g_buf + 22;  /* wix=16 */
  const uint8_t* dat_p   = g_buf + 38;  /* password base, wid=16 */
  const uint8_t* sat_p   = g_buf + 54;  /* salt, wis=16          */

  /* Variable-length password: use any extra bytes beyond the header. */
  c3_w dat_len = H_FIXED_LEN;
  if (g_len > (c3_d)H_MIN_INPUT) {
    c3_d extra = g_len - (c3_d)H_MIN_INPUT;
    if (extra > 256) extra = 256;
    dat_len = H_FIXED_LEN + (c3_w)extra;
  }

  /* Build noun atoms */
  u3_atom type_a    = _type_atom(type_b);
  u3_atom version_a = 19;          /* ARGON2_VERSION_13 = 0x13 */
  u3_atom out_a     = out_w;
  u3_atom threads_a = threads_w;
  u3_atom mem_a     = mem_w;
  u3_atom time_a    = time_w;

  /* key and extra (associated data) */
  u3_atom wik_a   = H_FIXED_LEN;
  u3_atom key_a   = u3i_bytes(H_FIXED_LEN, key_p);
  u3_atom wix_a   = H_FIXED_LEN;
  u3_atom extra_a = u3i_bytes(H_FIXED_LEN, extra_p);

  /* password (dat) — may use variable tail */
  u3_atom wid_a;
  u3_atom dat_a;
  if (dat_len <= H_FIXED_LEN) {
    wid_a = H_FIXED_LEN;
    dat_a = u3i_bytes(H_FIXED_LEN, dat_p);
  }
  else {
    /* Allocate a combined buffer for variable-length password. */
    c3_w    vlen   = dat_len;
    uint8_t *vbuf  = malloc(vlen);
    memcpy(vbuf, dat_p, H_FIXED_LEN);
    memcpy(vbuf + H_FIXED_LEN, g_buf + H_MIN_INPUT, vlen - H_FIXED_LEN);
    wid_a = vlen;
    dat_a = u3i_bytes(vlen, vbuf);
    free(vbuf);
  }

  u3_atom wis_a = H_FIXED_LEN;
  u3_atom sat_a = u3i_bytes(H_FIXED_LEN, sat_p);

  /* Call the static inner function directly — no core construction
   * needed since we bypassed the wrapper. */
  u3_noun pro = _cqe_argon2(
      out_a, type_a, version_a,
      threads_a, mem_a, time_a,
      wik_a, key_a, wix_a, extra_a,
      wid_a, dat_a, wis_a, sat_a);

  if (u3_none != pro) u3z(pro);

  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);   /* 64 MiB loom — match worst-case argon2 mem */

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  if (len < H_MIN_INPUT || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _argon2_soft, u3_nul);
  u3z(pro);

  return 0;
}
