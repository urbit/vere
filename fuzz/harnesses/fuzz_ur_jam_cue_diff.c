/// @file fuzz_ur_jam_cue_diff.c
///
/// H3 — AFL++ differential harness for cue + jam roundtrip.
///
/// Workflow per iteration:
///   1. cue(input)   -> ref1       (if fails, skip — bogus input)
///   2. jam(ref1)    -> bytes2
///   3. cue(bytes2)  -> ref2
///   4. assert noun_eq(ref1, ref2)
///
/// Why *structural* noun equality instead of `ref1 == ref2`:
///
/// pkg/ur's equality model is nominally "equal nrefs iff equal nouns",
/// which only holds if the hashcons layer canonicalises all
/// representations. In practice ur_cue's large-atom path calls
/// ur_coin_bytes_unsafe, which doesn't promote small post-trim atoms
/// back to direct. That produces non-canonical iatoms for inputs that
/// claim len>62 but carry a value that fits in 62 bits.
///
/// As a result, cue(input) can yield an iatom for what re-cue(jam(…))
/// yields as a direct atom — same *value*, different nref. This isn't
/// a memory-safety bug, just a canonicalisation gap that is benign
/// because ur_cue is not on Vere's production ingest path (u3 has its
/// own cue with proper canonicalisation in pkg/noun/serial.c). See
/// fuzz/findings/002-ur_cue-noncanonical-iatom/ for the writeup.
///
/// To keep H3 useful despite this gap, we compare by structural
/// walk instead of nref identity: recurse into cells, compare atom
/// byte representations. Real jam/cue roundtrip bugs (lost bits,
/// shape mismatch, cue-of-canonical-jam failing) still surface; the
/// known canonicalisation gap no longer creates noise.
///
/// Build:   fuzz/build.sh fuzz_ur_jam_cue_diff
/// Corpus:  fuzz/corpus/fuzz_ur_jam_cue_diff/
/// Dict:    fuzz/dicts/cue.dict

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* read(2) for __AFL_FUZZ_TESTCASE_LEN */

#include "ur.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/*
 * Helpers that avoid pkg/ur's internal accessors (ur_deep, ur_head,
 * ur_tail, ur_bytes) because they are not declared in hashcons.h —
 * calling them without a prototype gets an implicit-int return and
 * corrupts the uint8_t value returned from ur_deep. We use the public
 * ur_nref_tag / ur_nref_idx macros and poke the root struct directly,
 * which is all declared in hashcons.h.
 */

static inline int
is_cell(ur_nref ref)
{
  return (int)ur_nref_tag(ref) == (int)ur_icell;
}

static inline ur_nref
cell_head(ur_root_t* r, ur_nref ref)
{
  return r->cells.heads[ur_nref_idx(ref)];
}

static inline ur_nref
cell_tail(ur_root_t* r, ur_nref ref)
{
  return r->cells.tails[ur_nref_idx(ref)];
}

/*
 * Extract the byte representation of an atom. Handles both direct and
 * iatom refs and normalises them to the same (ptr, len) shape. The
 * direct case writes into a caller-provided 8-byte scratch buffer.
 *
 * Returns the length in bytes; *byt is set to point into either the
 * root's atom table (iatom) or the scratch buffer (direct).
 */
static uint64_t
atom_bytes(ur_root_t* r, ur_nref ref,
           const uint8_t** byt, uint8_t scratch[8])
{
  if ((int)ur_nref_tag(ref) == (int)ur_direct) {
    /* Little-endian serialise the 62-bit value into scratch. */
    for (int i = 0; i < 8; i++) {
      scratch[i] = (uint8_t)((ref >> (8 * i)) & 0xff);
    }
    *byt = scratch;
    return (uint64_t)ur_met3_64(ref);
  }
  /* ur_iatom */
  uint64_t idx = ur_nref_idx(ref);
  *byt = r->atoms.bytes[idx];
  return r->atoms.lens[idx];
}

/*
 * Compare two atom refs for value-equality. Handles all four
 * direct/iatom crossings uniformly. Returns 1 if equal, 0 if not.
 */
static int
atom_eq(ur_root_t* r, ur_nref a, ur_nref b)
{
  uint8_t scratch_a[8];
  uint8_t scratch_b[8];
  const uint8_t* ba;
  const uint8_t* bb;
  uint64_t la = atom_bytes(r, a, &ba, scratch_a);
  uint64_t lb = atom_bytes(r, b, &bb, scratch_b);
  if (la != lb) return 0;
  if (la == 0) return 1;
  return memcmp(ba, bb, (size_t)la) == 0;
}

/*
 * Iterative structural noun equality over a shared ur_root_t.
 *
 * Uses a dynamically-grown stack of ref pairs to avoid unbounded
 * recursion depth for deeply-nested cells. Returns 1 equal, 0 not
 * equal, -1 on allocation failure (which we treat as rejected input,
 * not a crash).
 */
typedef struct {
  ur_nref a;
  ur_nref b;
} ref_pair_t;

static int
noun_eq(ur_root_t* r, ur_nref a, ur_nref b)
{
  /* Fast path: identical refs are equal by hashcons. */
  if (a == b) return 1;

  ref_pair_t* stack = NULL;
  size_t      fill  = 0;
  size_t      cap   = 0;

#define PUSH(x, y) do {                                               \
    if (fill == cap) {                                                \
      size_t next = cap ? cap * 2 : 64;                               \
      ref_pair_t* tmp = realloc(stack, next * sizeof(ref_pair_t));    \
      if (!tmp) { free(stack); return -1; }                           \
      stack = tmp; cap = next;                                        \
    }                                                                 \
    stack[fill].a = (x);                                              \
    stack[fill].b = (y);                                              \
    fill++;                                                           \
} while (0)

  PUSH(a, b);

  while (fill > 0) {
    fill--;
    ur_nref x = stack[fill].a;
    ur_nref y = stack[fill].b;

    if (x == y) continue;

    int x_cell = is_cell(x);
    int y_cell = is_cell(y);

    if (x_cell != y_cell) {
      /* One is a cell, the other an atom: not structurally equal. */
      free(stack);
      return 0;
    }

    if (x_cell) {
      /* Both cells — queue up head and tail for comparison. */
      PUSH(cell_tail(r, x), cell_tail(r, y));
      PUSH(cell_head(r, x), cell_head(r, y));
      continue;
    }

    /* Both atoms — compare by value. */
    if (!atom_eq(r, x, y)) {
      free(stack);
      return 0;
    }
  }

  free(stack);
  return 1;
#undef PUSH
}

static void
diff_fail(const char* msg, ur_nref ref1, ur_nref ref2)
{
  fprintf(stderr,
          "H3 diff fail: %s ref1=0x%016lx ref2=0x%016lx\n",
          msg, (unsigned long)ref1, (unsigned long)ref2);
  abort();
}

int
main(void)
{
  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;

  while (__AFL_LOOP(10000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    if (len < 1) continue;
    if (len > H_MAX_INPUT) continue;

    ur_root_t* r = ur_root_init();
    if (r == NULL) continue;

    ur_cue_t* c = ur_cue_init(r);
    if (c == NULL) {
      ur_root_free(r);
      continue;
    }

    ur_nref ref1;
    ur_cue_res_e cr1 = ur_cue_with(c, (uint64_t)len, buf, &ref1);
    if (cr1 != ur_cue_good) {
      ur_cue_done(c);
      ur_root_free(r);
      continue;
    }

    uint64_t jlen = 0;
    uint8_t* jbyt = NULL;
    (void)ur_jam(r, ref1, &jlen, &jbyt);
    if (jbyt == NULL || jlen == 0) {
      diff_fail("jam returned empty buffer", ref1, 0);
    }

    ur_nref ref2;
    ur_cue_res_e cr2 = ur_cue_with(c, jlen, jbyt, &ref2);
    if (cr2 != ur_cue_good) {
      diff_fail("cue(jam(x)) failed", ref1, 0);
    }

    int eq = noun_eq(r, ref1, ref2);
    if (eq == 0) {
      diff_fail("noun_eq(ref1, ref2) = false", ref1, ref2);
    }
    /* eq == -1 means allocation failure inside noun_eq; treat as
     * rejected input (same policy as the other "skip" paths). */

    free(jbyt);
    ur_cue_done(c);
    ur_root_free(r);
  }

  return 0;
}
