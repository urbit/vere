/// @file fuzz_bytestream.c
///
/// H29 — AFL++ harness for bytestream jets
/// (`pkg/noun/jets/e/bytestream.c`).
///
/// Targets (chosen for OOB/cursor risk):
///
///   u3we_bytestream_read_byte   — reads one byte at cursor pos from an
///     octs buffer; bails if pos+1 > p.octs.  Classic off-by-one target.
///     Sample: [pos=@ octs=[p=@ q=@]]  at [sam_2, sam_3].
///
///   u3we_bytestream_read_octs   — reads n bytes starting at pos; bails
///     if pos+n > p.octs.  Allocates a slab of size n_w — large n with
///     wrong p.octs exercises the slab path.
///     Sample: [n=@ pos=@ octs=[p=@ q=@]]  at [sam_2, sam_6, sam_7].
///
///   u3we_bytestream_find_byte   — linear scan for bat in octs[pos..].
///     Exercises the leading-zero handling path (bat==0).
///     Sample: [bat=@ pos=@ octs=[p=@ q=@]]  at [sam_2, sam_6, sam_7].
///
/// Input layout:
///   byte 0   : op selector (0 => read_byte, 1 => read_octs, 2 => find_byte)
///   bytes 1..: raw bytes used to construct atoms directly (no cue needed).
///
/// Construction strategy:
///   We avoid cue here because the interesting state space is the *numeric*
///   relationship between pos, n, p.octs, and the actual byte length of
///   q.octs — not the noun wire format.  Instead we read small integers
///   directly out of the fuzz buffer and build atoms with u3i_bytes /
///   u3i_word, then assemble the noun sample manually.
///
///   Layout after the op byte:
///     [0..3]  : pos_w  (u32 LE)
///     [4..7]  : n_w    (u32 LE, used by read_octs)
///     [8]     : bat_y  (1 byte, used by find_byte; masked to 0xff)
///     [9..12] : p_octs_w (declared byte-count of the buffer, u32 LE)
///     [13..]  : raw buffer bytes  (q.octs payload)
///
///   We cap p_octs_w and n_w to 4096 to keep allocation bounded.
///
/// Build:   fuzz/build.sh fuzz_bytestream   (noun-flavor)
/// Corpus:  fuzz/corpus/fuzz_bytestream/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3we_bytestream_read_byte(u3_noun cor);
extern u3_noun u3we_bytestream_read_octs(u3_noun cor);
extern u3_noun u3we_bytestream_find_byte(u3_noun cor);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT   65536
/* Cap on declared octs length and n to keep slab allocs cheap. */
#define H_MAX_OCTS    4096u

/*
 * Parsed fields from the fuzz buffer.
 */
static uint8_t        g_op;
static uint32_t       g_pos_w;
static uint32_t       g_n_w;
static uint8_t        g_bat_y;
static uint32_t       g_p_octs;     /* declared byte length */
static const uint8_t* g_buf;        /* raw buffer bytes */
static int            g_buf_len;    /* actual bytes available */

/*
 * _build_octs: make a noun [p_octs q_octs] where q_octs is an atom
 * whose byte representation is g_buf[0..g_buf_len-1].
 */
static u3_noun
_build_octs(void)
{
  u3_atom q_octs;
  if ( g_buf_len == 0 ) {
    q_octs = 0;
  } else {
    q_octs = u3i_bytes((c3_w)g_buf_len, g_buf);
  }
  return u3nc(u3i_word(g_p_octs), q_octs);
}

/*
 * _build_cor_sam: helper to build a gate core with an explicit sample
 * noun placed at the payload head.  The battery and context are ~.
 *
 * Gate core layout: [battery [sample context]]
 *   address 1  = core
 *   address 2  = battery
 *   address 3  = payload
 *   address 6  = sample  (= u3x_sam)
 *   address 7  = context (= u3x_con)
 */
static u3_noun
_build_core(u3_noun sam)
{
  return u3nc(u3_nul, u3nc(sam, u3_nul));
}

/* ------------------------------------------------------------------ */

static u3_noun
_read_byte_soft(u3_noun arg)
{
  (void)arg;
  /*
   * read_byte sample at [sam_2, sam_3]:
   *   sam_2 (12) = pos
   *   sam_3 (13) = octs
   * sample = [pos octs]
   */
  u3_noun octs = _build_octs();
  u3_noun sam  = u3nc(u3i_word(g_pos_w), octs);
  u3_noun cor  = _build_core(sam);

  u3_noun pro  = u3we_bytestream_read_byte(cor);
  u3z(pro);
  return u3_nul;
}

static u3_noun
_read_octs_soft(u3_noun arg)
{
  (void)arg;
  /*
   * read_octs sample at [sam_2, sam_6, sam_7]:
   *   sam_2 (12) = n
   *   sam_6 (24 = sam_2*2)   = pos    ... wait, let's verify
   *   sam_7 (25 = sam_2*2+1) = octs
   *
   * u3x_sam_2 = 12, u3x_sam_6 = u3x_sam_2*2 = 24? No:
   *   sam=6, sam_2=12, sam_3=13, sam_6=24, sam_7=25
   * So:
   *   sam_2 = 12 => n      (head of sample)
   *   sam_3 = 13 => [pos octs]  (tail of sample)
   *     sam_6 = 24 = h(sam_3) => pos
   *     sam_7 = 25 = t(sam_3) => octs
   *
   * Therefore sample = [n [pos octs]]
   */
  u3_noun octs    = _build_octs();
  u3_noun pos_octs = u3nc(u3i_word(g_pos_w), octs);
  u3_noun sam     = u3nc(u3i_word(g_n_w), pos_octs);
  u3_noun cor     = _build_core(sam);

  u3_noun pro = u3we_bytestream_read_octs(cor);
  u3z(pro);
  return u3_nul;
}

static u3_noun
_find_byte_soft(u3_noun arg)
{
  (void)arg;
  /*
   * find_byte sample at [sam_2, sam_6, sam_7]:
   *   sam_2 (12) = bat  (the byte to find)
   *   sam_6 (24) = pos
   *   sam_7 (25) = octs
   *
   * sample = [bat [pos octs]]
   */
  u3_noun octs     = _build_octs();
  u3_noun pos_octs = u3nc(u3i_word(g_pos_w), octs);
  u3_noun sam      = u3nc(u3i_word((c3_w)g_bat_y), pos_octs);
  u3_noun cor      = _build_core(sam);

  u3_noun pro = u3we_bytestream_find_byte(cor);
  u3z(pro);
  return u3_nul;
}

/* ------------------------------------------------------------------ */

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* Minimum: op(1) + pos(4) + n(4) + bat(1) + p_octs(4) = 14 bytes */
  if ( len < 14 || len > H_MAX_INPUT ) {
    return 0;
  }

  int off = 0;

  g_op   = buf[off++];

  memcpy(&g_pos_w, buf + off, 4);  off += 4;
  memcpy(&g_n_w,   buf + off, 4);  off += 4;
  g_bat_y = buf[off++];

  uint32_t p_octs_raw;
  memcpy(&p_octs_raw, buf + off, 4);  off += 4;
  /* Clamp declared length — prevents enormous slab allocs. */
  g_p_octs = (p_octs_raw > H_MAX_OCTS) ? H_MAX_OCTS : p_octs_raw;

  /* Also clamp n to prevent huge allocations in read_octs. */
  if ( g_n_w > H_MAX_OCTS ) {
    g_n_w = H_MAX_OCTS;
  }

  g_buf     = (const uint8_t*)(buf + off);
  g_buf_len = len - off;

  u3_funk fun_f;
  switch ( g_op % 3 ) {
    case 0:  fun_f = _read_byte_soft; break;
    case 1:  fun_f = _read_octs_soft; break;
    default: fun_f = _find_byte_soft; break;
  }

  u3_noun pro = u3m_soft(0, fun_f, u3_nul);
  u3z(pro);

  return 0;
}
