/// @file fuzz_jet_lsh.c
///
/// AFL++ harness for `u3qc_lsh` (pkg/noun/jets/c/lsh.c).
///
/// `lsh` left-shifts an atom by `b` blocks of size `2^a` bits:
///   lsh(bloq, count, atom)  →  atom * 2^(count * 2^bloq)
///
/// Signature:
///   u3_noun u3qc_lsh(u3_atom a, u3_atom b, u3_atom c)
///     a = bloq  (block-size log2, 0..31)
///     b = count (number of blocks to shift left, must be direct atom)
///     c = atom  (value to shift)
///
/// Allocation size: (count + met(bloq, atom)) blocks, so a large count
/// combined with a large atom can allocate enormous slabs.  We clamp:
///   - bloq  to 0..10  (1024-bit blocks at most)
///   - count to 0..1048575  (< 1M blocks; at bloq=10 that is 128 MiB max)
///
/// In practice the loom is 64 MiB so the jet will bail via u3m_bail
/// (caught by u3m_soft) before overflowing, but clamping count keeps
/// the fuzzer from wasting iterations on out-of-memory cases.
///
/// Input layout (bytes):
///   [0]      bloq   — low 4 bits, then clamped to 0..10
///   [1..4]   count  — 4-byte LE; clamped to 0..1048575
///   [5..]    atom   — raw payload bytes
///
/// Minimum input: 5 bytes (empty atom is valid).
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill).
/// Build:   fuzz/build.sh fuzz_jet_lsh
/// Corpus:  fuzz/corpus/fuzz_jet_lsh/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3qc_lsh(u3_atom, u3_atom, u3_atom);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT   65536
/* Output atom needs count * 2^bloq bits. Cap at ~1 MiB = 2^23 bits
 * worth of pure zero padding; exceeding the 64 MiB loom caused a
 * segfault inside u3qc_lsh before u3m_bail could fire. */
#define H_MAX_OUT_BITS (1UL << 23)

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static inline c3_w
_le32(const uint8_t* p)
{
  return (c3_w)p[0]
       | ((c3_w)p[1] << 8)
       | ((c3_w)p[2] << 16)
       | ((c3_w)p[3] << 24);
}

static u3_noun
_lsh_soft(u3_noun arg)
{
  (void)arg;

  /* bloq: 0..10 keeps block sizes from 1 bit up to 1024 bits. */
  c3_w bloq_w = g_buf[0] & 0x0f;
  if ( bloq_w > 10 ) bloq_w = bloq_w % 11;

  /* count: clamp so count * 2^bloq ≤ H_MAX_OUT_BITS. */
  c3_w count_w = _le32(g_buf + 1);
  c3_w max_cnt = (c3_w)(H_MAX_OUT_BITS >> bloq_w);
  if ( count_w > max_cnt ) count_w = max_cnt;

  /* Atom: remaining fuzz bytes. */
  c3_d dat_len = g_len - 5;
  u3_atom dat  = (dat_len > 0)
    ? u3i_bytes((c3_w)dat_len, g_buf + 5)
    : (u3_atom)0;

  u3_noun pro = u3qc_lsh((u3_atom)bloq_w, (u3_atom)count_w, dat);
  u3z(dat);
  return pro;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 5 || len > H_MAX_INPUT ) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _lsh_soft, u3_nul);
  u3z(pro);

  return 0;
}
