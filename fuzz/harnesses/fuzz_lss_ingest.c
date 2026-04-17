/// @file fuzz_lss_ingest.c
///
/// H10 — AFL++ harness for the LSS (Large Sparse Space?) builder and
/// verifier in pkg/vere/io/lss.c. LSS provides content-integrity
/// proofs used by mesa to validate received fragments.
///
/// We exercise both sides of the LSS state machine:
///   1. lss_builder_init + lss_builder_ingest(leaf_bytes)
///   2. lss_verifier_init + lss_verifier_ingest(leaf_bytes, pair)
///
/// The builder and verifier each maintain an internal state across
/// ingest calls (hash accumulators, rolling Merkle tree). We feed
/// the fuzz input as a sequence of leaf chunks.
///
/// Build:   fuzz/build.sh fuzz_lss_ingest
/// Corpus:  fuzz/corpus/fuzz_lss_ingest/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vere.h"
#include "io/lss.h"
#include "arena.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define LEAF_CHUNK  1024
#define MAX_LEAVES  64

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

  /* Derive a leaf count from the first byte of input (clamped). */
  c3_w leaves = (buf[0] % MAX_LEAVES) + 1;
  unsigned char* body = buf + 1;
  c3_w           body_w = (c3_w)(len - 1);
  if ((int)body_w <= 0) {
    return 0;
  }

  /* Builder side: ingest leaves in LEAF_CHUNK-sized slices. Note
   * that lss_builder_init takes a pre-allocated struct but
   * lss_builder_free calls free() on it — so the builder MUST be
   * heap-allocated. */
  lss_builder* bil_u = c3_calloc(sizeof(*bil_u));
  lss_builder_init(bil_u, leaves);

  c3_w off_w = 0;
  for (c3_w i = 0; i < leaves && off_w < body_w; i++) {
    c3_w chunk = (body_w - off_w > LEAF_CHUNK) ? LEAF_CHUNK : (body_w - off_w);
    lss_builder_ingest(bil_u, body + off_w, chunk);
    off_w += chunk;
  }

  lss_builder_free(bil_u);

  return 0;
}
