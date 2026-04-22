/// @file fuzz_jet_rip_rep.c
///
/// AFL++ harness for `u3qc_rip` and `u3qc_rep` (pkg/noun/jets/c/).
///
/// Tests both jets and their round-trip inverse property:
///   rip(bloq, 1, rep(bloq, 1, list)) == list   (when list elems fit in bloq)
///
/// Signatures:
///   u3_noun u3qc_rip(u3_atom a, u3_atom b, u3_atom c)
///     a = bloq  (block-size log2)
///     b = step  (1 = one-block-at-a-time, the normal case)
///     c = atom  (source to rip apart)
///
///   u3_noun u3qc_rep(u3_atom a, u3_atom b, u3_noun c)
///     a = bloq  (block-size log2)
///     b = step  (1)
///     c = list of atoms (each clamped to fit in one bloq-sized block)
///
/// Input layout (bytes):
///   [0]      bloq   — masked to 0..4 for rip (keeps blocks 32-bit or less,
///                     matching the implemented fast-path; values 5..9 are
///                     also valid for the large-block path — we allow 0..9).
///   [1]      nelem  — number of list elements for rep, clamped to 0..63
///   [2..N]   elem bytes — each element is 1 byte (fits in any bloq >= 3)
///            after elem bytes: remaining bytes become the atom for rip.
///
/// The differential test exercises the round-trip:
///   packed = rep(bloq, 1, list)
///   unpacked = rip(bloq, 1, packed)
///   The re-ripped list should equal the original list after stripping
///   leading zero blocks (rip strips trailing zeros from the atom).
///
/// Minimum input: 2 bytes.
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill).
/// Build:   fuzz/build.sh fuzz_jet_rip_rep
/// Corpus:  fuzz/corpus/fuzz_jet_rip_rep/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3qc_rip(u3_atom, u3_atom, u3_atom);
extern u3_noun u3qc_rep(u3_atom, u3_atom, u3_noun);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define H_MAX_ELEMS 63

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

/* Build a Hoon list (linked cons cells, null-terminated) from an
 * array of c3_w words. */
static u3_noun
_list_of_words(const c3_w* ws, c3_w n)
{
  u3_noun acc = u3_nul;
  /* Build right-to-left so head is first element. */
  for ( c3_w i = n; i > 0; i-- ) {
    acc = u3nc(ws[i-1], acc);
  }
  return acc;
}

/* Count elements in a Hoon null-terminated list. */
static c3_w
_lent(u3_noun lst)
{
  c3_w n = 0;
  while ( u3_nul != lst ) {
    lst = u3t(lst);
    n++;
  }
  return n;
}

/* Compare two null-terminated lists of atoms element-by-element.
 * Returns c3y if equal. */
static c3_o
_list_eq(u3_noun a, u3_noun b)
{
  while ( 1 ) {
    if ( u3_nul == a && u3_nul == b ) return c3y;
    if ( u3_nul == a || u3_nul == b ) return c3n;
    if ( c3n == u3r_sing(u3h(a), u3h(b)) ) return c3n;
    a = u3t(a);
    b = u3t(b);
  }
}

static u3_noun
_rip_rep_soft(u3_noun arg)
{
  (void)arg;

  /* bloq: clamp to 0..9. Values 0..4 hit the small-block fast-path;
   * 5..9 hit the large-block path.  >= 10 means blocks >= 1KB which
   * risks large allocs on tiny atoms — not useful to fuzz. */
  c3_w bloq_w = g_buf[0] & 0x0f;
  if ( bloq_w > 9 ) bloq_w = bloq_w % 10;

  /* Number of list elements for rep, clamped to [0, H_MAX_ELEMS]. */
  c3_w nelem = g_buf[1];
  if ( nelem > H_MAX_ELEMS ) nelem = H_MAX_ELEMS;

  /* Remaining bytes: first nelem bytes are the list elements (one
   * byte each — always fits in a bloq >= 3 block, and we mask to the
   * block width for smaller bloqs).  Bytes after that are the atom
   * to hand directly to rip. */
  c3_d needed  = (c3_d)(2 + nelem);
  if ( g_len < needed ) nelem = (c3_w)(g_len - 2);

  /* --- Part 1: fuzz rip directly --- */
  {
    c3_d dat_off = 2 + (c3_d)nelem;
    c3_d dat_len = (g_len > dat_off) ? (g_len - dat_off) : 0;
    u3_atom rip_atom = (dat_len > 0)
      ? u3i_bytes((c3_w)dat_len, g_buf + dat_off)
      : (u3_atom)0;

    u3_noun rip_pro = u3qc_rip((u3_atom)bloq_w, (u3_atom)1, rip_atom);
    u3z(rip_pro);
    u3z(rip_atom);
  }

  /* --- Part 2: fuzz rep directly, then round-trip --- */
  if ( nelem == 0 ) {
    /* rep on empty list is valid (returns 0). */
    u3_noun rep_pro = u3qc_rep((u3_atom)bloq_w, (u3_atom)1, u3_nul);
    u3z(rep_pro);
    return u3_nul;
  }

  /* Build element array: mask each byte to the block width so values
   * don't exceed the block size (prevents rip stripping more zeros). */
  c3_w elems[H_MAX_ELEMS];
  c3_w block_bits = (c3_w)1 << bloq_w;   /* bits per block */
  c3_w block_mask = (block_bits >= 32) ? 0xffffffffUL : ((1UL << block_bits) - 1);

  for ( c3_w i = 0; i < nelem; i++ ) {
    elems[i] = (c3_w)(g_buf[2 + i]) & block_mask;
  }

  /* Strip trailing zeros from the expected list — rip will drop them
   * because the packed atom loses leading-zero blocks. */
  c3_w nelem_nz = nelem;
  while ( nelem_nz > 0 && elems[nelem_nz - 1] == 0 ) {
    nelem_nz--;
  }

  u3_noun lst      = _list_of_words(elems, nelem);
  u3_noun rep_pro  = u3qc_rep((u3_atom)bloq_w, (u3_atom)1, u3k(lst));

  if ( u3_none == rep_pro ) {
    u3z(lst);
    return u3_nul;
  }

  /* Round-trip: rip the packed atom back and compare to original
   * (sans trailing zero blocks, which rip naturally drops). */
  u3_noun rtrip = u3qc_rip((u3_atom)bloq_w, (u3_atom)1, u3k(rep_pro));

  if ( u3_none != rtrip ) {
    /* Build expected list without trailing zeros for comparison. */
    u3_noun expected = _list_of_words(elems, nelem_nz);
    if ( c3n == _list_eq(rtrip, expected) ) {
      /* Mismatch: rep then rip did not round-trip. This is a bug. */
      u3l_log("fuzz_jet_rip_rep: round-trip mismatch bloq=%u nelem=%u nelem_nz=%u",
              bloq_w, nelem, nelem_nz);
      /* Deliberately abort so AFL++ records this as a crash. */
      abort();
    }
    u3z(expected);
    u3z(rtrip);
  }

  u3z(rep_pro);
  u3z(lst);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_INPUT ) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _rip_rep_soft, u3_nul);
  u3z(pro);

  return 0;
}
