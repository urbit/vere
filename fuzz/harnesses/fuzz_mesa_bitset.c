/// @file fuzz_mesa_bitset.c
///
/// H21 — AFL++ harness for the mesa fragment-bitmap API
/// (`pkg/vere/io/mesa/bitset.c`). Small file, tight bit math,
/// network-reachable via mesa page packets.
///
/// Input layout (fuzzer-controlled):
///   bytes [0..1] — bitset size (mod 4096 + 1)
///   bytes [2..]  — sequence of ops, each 1 byte op + 2 bytes arg:
///     op 0: put(arg)
///     op 1: del(arg)
///     op 2: has(arg)
///     op 3: wyt()  (ignores arg)
///
/// We use a single `arena` block and reset it each iteration so
/// persistent mode is safe.
///
/// Build:   fuzz/build.sh fuzz_mesa_bitset
/// Corpus:  fuzz/corpus/fuzz_mesa_bitset/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"
#include "arena.h"
#include "io/mesa/bitset.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define BS_MAX_LEN  4096
#define ARENA_BYTES (64 * 1024)

static arena g_are = {0};

int
main(void)
{
  /* We don't need u3 state but the vere-harness build path already
   * boots the loom; harmless. */
  u3m_boot_lite(1 << 24);

  g_are = arena_create(ARENA_BYTES);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 2 || len > H_MAX_INPUT) {
    return 0;
  }

  /* Reset arena so we can re-allocate the bitset. */
  g_are.beg = g_are.dat;

  c3_w bs_len = ((c3_w)buf[0] | ((c3_w)buf[1] << 8)) % BS_MAX_LEN + 1;
  u3_bitset bit_u;
  bitset_init(&bit_u, bs_len, &g_are);

  int i = 2;
  while (i + 3 <= len) {
    uint8_t op = buf[i] & 0x3;
    c3_w    arg = ((c3_w)buf[i+1] | ((c3_w)buf[i+2] << 8));
    i += 3;

    switch (op) {
      case 0: bitset_put(&bit_u, arg); break;
      case 1: bitset_del(&bit_u, arg); break;
      case 2: (void)bitset_has(&bit_u, arg); break;
      case 3: (void)bitset_wyt(&bit_u); break;
    }
  }

  return 0;
}
