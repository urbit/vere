/// @file fuzz_jet_hew_sew.c
///
/// AFL++ harness for `u3qc_hew` and `u3qc_sew`
/// (pkg/noun/jets/c/hew.c and sew.c).
///
/// hew — structured bit-extract using a width-tree mold:
///   u3qc_hew(boq, sep, vat, sam) → [extracted_tree, boq, total_pos]
///     boq = block-size log2 (0..31)
///     sep = starting block position
///     vat = source atom
///     sam = width noun: a tree of direct atoms (each is a block count
///           to extract) or 0 (null/skip).  We use a flat list of
///           1-byte widths for simplicity.
///
/// sew — bit-insert into an atom:
///   u3qc_sew(a, b, c, d, e) → atom
///     a = bloq    (block-size log2, 0..31)
///     b = start   (block offset into e, 32-bit)
///     c = len     (number of blocks to write, 32-bit; 0 → return e)
///     d = value   (atom to insert)
///     e = dest    (atom to write into)
///
/// Differential test: extract a slice with hew then write it back in
/// the same position with sew — result should equal the original atom.
///   vat' = sew(boq, sep, width_sum, hew_slice, vat)
///   sing(vat', vat) should hold (modulo leading-zero truncation).
///
/// Input layout (bytes):
///   [0]        boq    — low 4 bits, clamped to 0..10
///   [1..4]     sep    — 4-byte LE, block start offset (unmodified)
///   [5]        nwid   — number of width entries, clamped to 0..15
///   [6..6+nwid-1]  widths — one byte each (direct atom width values, 1..15)
///   [6+nwid..] vat    — source atom bytes
///
/// Minimum input: 6 bytes.
///
/// Build flavor: noun (u3m_boot_lite only, no ivory pill).
/// Build:   fuzz/build.sh fuzz_jet_hew_sew
/// Corpus:  fuzz/corpus/fuzz_jet_hew_sew/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3qc_hew(u3_atom, u3_atom, u3_atom, u3_noun);
extern u3_weak u3qc_sew(u3_atom, u3_atom, u3_atom, u3_atom, u3_atom);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT  65536
#define H_MAX_WIDTHS 15

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

/* Build a flat null-terminated list of direct-atom widths.
 * Each width is clamped to 1..15 (0 means skip in hew, which is
 * fine, but 0-width slabs are uninteresting for the round-trip). */
static u3_noun
_width_list(const uint8_t* ws, c3_w n)
{
  u3_noun acc = u3_nul;
  for ( c3_w i = n; i > 0; i-- ) {
    c3_w w = ws[i-1];
    if ( w == 0 ) w = 1;          /* avoid 0-width blocks */
    if ( w > 15 ) w = w % 15 + 1; /* clamp to 1..15 */
    acc = u3nc((u3_atom)w, acc);
  }
  return acc;
}

/* Sum widths in a flat list of direct atoms. */
static c3_w
_sum_widths(u3_noun lst)
{
  c3_w tot = 0;
  while ( u3_nul != lst ) {
    u3_noun h = u3h(lst);
    if ( _(u3a_is_cat(h)) ) tot += (c3_w)h;
    lst = u3t(lst);
  }
  return tot;
}

static u3_noun
_hew_sew_soft(u3_noun arg)
{
  (void)arg;

  /* boq: 0..10. */
  c3_w boq_w = g_buf[0] & 0x0f;
  if ( boq_w > 10 ) boq_w = boq_w % 11;

  /* sep: starting block offset. Clamp so (sep + sum(widths)) << boq
   * stays within ~1 MiB of output — avoids OOMs on 64 MiB loom. */
  c3_w sep_w = _le32(g_buf + 1);
  sep_w &= 0x0fff;   /* 0..4095 */

  /* nwid: number of width entries, 0..H_MAX_WIDTHS. */
  c3_w nwid = g_buf[5];
  if ( nwid > H_MAX_WIDTHS ) nwid = H_MAX_WIDTHS;

  /* Adjust for available bytes. */
  c3_d vat_off = (c3_d)(6 + nwid);
  if ( (c3_d)g_len < vat_off ) {
    /* Not enough bytes even for the header; use 0 widths. */
    nwid    = 0;
    vat_off = 6;
  }

  /* vat: source atom from remaining bytes. */
  c3_d vat_len = (g_len > vat_off) ? (g_len - vat_off) : 0;
  u3_atom vat  = (vat_len > 0)
    ? u3i_bytes((c3_w)vat_len, g_buf + vat_off)
    : (u3_atom)0;

  /* --- Part 1: fuzz sew directly with simple inputs --- */
  {
    /* Use boq, sep as start, nwid as len, vat as both value and dest.
     * This exercises sew without needing hew. */
    u3_atom sep_a = u3i_word(sep_w);
    u3_weak sew_pro = u3qc_sew((u3_atom)boq_w,
                                sep_a,
                                (u3_atom)nwid,
                                u3k(vat),
                                u3k(vat));
    u3z(sep_a);
    if ( u3_none != sew_pro ) {
      u3z(sew_pro);
    }
  }

  /* --- Part 2: fuzz hew then round-trip with sew --- */
  if ( nwid == 0 ) {
    /* hew with a null sam is valid: extracts nothing. */
    u3_noun hew_pro = u3qc_hew((u3_atom)boq_w,
                                (u3_atom)sep_w,
                                u3k(vat),
                                u3_nul);
    u3z(hew_pro);
    u3z(vat);
    return u3_nul;
  }

  u3_noun sam = _width_list(g_buf + 6, nwid);

  /* Guard: total blocks extracted is sep + sum(widths). If sep is
   * huge and there are many wide blocks, hew still works (it just
   * reads zeros beyond the atom).  No extra clamp needed — hew
   * detects overflow and bails via u3m_bail which u3m_soft catches. */

  u3_noun hew_pro = u3qc_hew((u3_atom)boq_w,
                              (u3_atom)sep_w,
                              u3k(vat),
                              u3k(sam));

  /* hew_pro is [extracted_tree, boq, total_pos].  The round-trip
   * works only for a flat list (one element) since sew is scalar.
   * When nwid == 1, extracted_tree is a single atom; we use that. */
  if ( u3_none != hew_pro && _(u3a_is_cell(hew_pro)) ) {
    u3_noun ext_tree = u3h(hew_pro);  /* extracted value(s) */
    u3_noun boq_out  = u3h(u3t(hew_pro));
    u3_noun pos_out  = u3t(u3t(hew_pro));

    /* Only attempt round-trip when the extracted result is a simple
     * atom (nwid==1 case). ext_tree can still be a cell depending on
     * hew's output shape — u3qc_sew's u3r_chop would assert on a
     * cell source. Guard with is_atom. */
    if ( nwid == 1
         && _(u3a_is_cat(boq_out)) && _(u3a_is_cat(pos_out))
         && _(u3a_is_atom(ext_tree)) ) {
      c3_w total_pos = (c3_w)pos_out;
      c3_w wid1      = total_pos - sep_w; /* blocks extracted */

      /* sew the extracted slice back at the same position. */
      u3_weak sew_back = u3qc_sew((u3_atom)boq_w,
                                   (u3_atom)sep_w,
                                   (u3_atom)wid1,
                                   u3k(ext_tree),
                                   u3k(vat));

      if ( u3_none != sew_back ) {
        /* The result should be structurally equal to vat provided the
         * region [sep, sep+wid1) lies within met(boq, vat).
         * We verify only when sep+wid1 <= met(boq, vat). */
        c3_w met_w = u3r_met((c3_g)boq_w, vat);
        if ( sep_w + wid1 <= met_w ) {
          if ( c3n == u3r_sing(sew_back, vat) ) {
            u3l_log("fuzz_jet_hew_sew: round-trip mismatch boq=%u sep=%u wid=%u",
                    boq_w, sep_w, wid1);
            abort();
          }
        }
        u3z(sew_back);
      }
    }
    u3z(hew_pro);
  } else if ( u3_none == hew_pro ) {
    /* hew returned u3_none (unexpected, but don't crash the harness). */
  }

  u3z(sam);
  u3z(vat);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 6 || len > H_MAX_INPUT ) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _hew_sew_soft, u3_nul);
  u3z(pro);

  return 0;
}
