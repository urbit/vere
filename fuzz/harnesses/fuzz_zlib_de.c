/// @file fuzz_zlib_de.c
///
/// H19 — AFL++ harness for the zlib/gzip decompression jets in
/// `pkg/noun/jets/e/zlib.c`. These are wrappers around upstream
/// `inflate`, but the urbit glue does its own buffer management
/// (`u3i_slab_grow`, leading-zero handling, the position-tracking
/// dance) which has plenty of room for off-by-one bugs.
///
/// Public entries:
///   u3qe_decompress_gzip(pos, octs) — gzip wrapper (window_bits=31)
///   u3qe_decompress_zlib(pos, octs) — zlib wrapper (window_bits=15)
///
/// Both take a position atom and an `octs = [len bytes]` cell. We
/// construct them from the fuzz input: byte 0 selects gzip/zlib,
/// the rest is the compressed stream.
///
/// Build:   fuzz/build.sh fuzz_zlib_de
/// Corpus:  fuzz/corpus/fuzz_zlib_de/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3qe_decompress_gzip(u3_atom pos, u3_noun octs);
extern u3_noun u3qe_decompress_zlib(u3_atom pos, u3_noun octs);

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;
static uint8_t        g_mode = 0;

static u3_noun
_de_soft(u3_noun arg)
{
  (void)arg;

  /* Build octs = [len content_atom]. The content atom is the fuzz
   * bytes; len is the bit-length matching what the decompressor
   * expects (we use byte_len * 8). */
  u3_atom content = u3i_bytes((c3_w)g_len, g_buf);
  u3_atom byte_len = u3i_word((c3_w)g_len);
  u3_noun octs = u3nc(byte_len, content);
  u3_atom pos = 0;

  u3_noun res;
  if (g_mode & 1) {
    res = u3qe_decompress_gzip(pos, octs);
  }
  else {
    res = u3qe_decompress_zlib(pos, octs);
  }

  if (u3_none != res) u3z(res);
  u3z(octs);
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

  g_mode = buf[0];
  g_buf  = (const uint8_t*)(buf + 1);
  g_len  = (c3_d)(len - 1);

  u3_noun pro = u3m_soft(0, _de_soft, u3_nul);
  u3z(pro);

  return 0;
}
