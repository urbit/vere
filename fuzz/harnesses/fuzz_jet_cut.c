/// @file fuzz_jet_cut.c
///
/// AFL++ harness for `u3qc_cut` (pkg/noun/jets/c/cut.c).
///
/// `cut` extracts a run of blocks from an atom:
///   cut(bloq, start, len, atom)  →  atom
///
/// Signature:
///   u3_noun u3qc_cut(u3_atom a, u3_atom b, u3_atom c, u3_atom d)
///     a = bloq  (block-size log2, 0..31)
///     b = start (block index into d)
///     c = len   (number of blocks to extract)
///     d = atom  (source data)
///
/// Input layout (bytes):
///   [0]      bloq   — clamped to 0..10 (up to 1024-bit blocks is plenty)
///   [1..4]   start  — 4-byte LE word; no clamp needed (jet clamps internally)
///   [5..8]   len    — 4-byte LE word; no clamp needed (jet clamps internally)
///   [9..]    atom   — raw payload bytes
///
/// Minimum input: 9 bytes (empty atom is valid).
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill).
/// Build:   fuzz/build.sh fuzz_jet_cut
/// Corpus:  fuzz/corpus/fuzz_jet_cut/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3qc_cut(u3_atom, u3_atom, u3_atom, u3_atom);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

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
_cut_soft(u3_noun arg)
{
  (void)arg;

  /* bloq: block-size log2. Jet bails if >= 32; clamp to 10 to keep
   * allocations sane (2^10 = 1024-bit blocks). */
  c3_w bloq_w  = g_buf[0] & 0x0f;   /* 0..15; further clamp below */
  if ( bloq_w > 10 ) bloq_w = bloq_w % 11;

  /* start and len: 32-bit LE from bytes 1..8. The jet CLAIMS to clip
   * len internally, but in practice it still allocates
   * len * 2^bloq bits of output before clipping, so a large len at
   * any bloq OOMs the 64 MiB loom. Clamp so total output ≤ 1 MiB. */
  c3_w start_w = _le32(g_buf + 1);
  c3_w len_w   = _le32(g_buf + 5);
  {
    c3_w max_len = (c3_w)((1UL << 23) >> bloq_w);   /* ≤ 1 MiB output */
    if ( len_w > max_len ) len_w = max_len;
    /* start is also used to compute output size in some codepaths */
    if ( start_w > max_len ) start_w = max_len;
  }

  /* Atom: remaining fuzz bytes. */
  c3_d dat_len = g_len - 9;
  u3_atom dat  = u3i_bytes((c3_w)dat_len, g_buf + 9);

  /* u3_atom values ≥ 2^31 are indirect-atom pointers; casting a raw
   * c3_w risks producing a fake pointer that u3r_met dereferences.
   * u3i_word builds a proper direct-or-indirect atom. */
  u3_atom bloq  = bloq_w;        /* 0..10 always fits */
  u3_atom start = u3i_word(start_w);
  u3_atom cnt   = u3i_word(len_w);

  u3_noun pro = u3qc_cut(bloq, start, cnt, dat);
  u3z(start); u3z(cnt);
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
  if ( len < 9 || len > H_MAX_INPUT ) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _cut_soft, u3_nul);
  u3z(pro);

  return 0;
}
