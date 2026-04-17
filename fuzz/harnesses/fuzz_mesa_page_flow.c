/// @file fuzz_mesa_page_flow.c
///
/// H26 — end-to-end mesa page-packet flow harness.
///
/// Purpose: verify whether the theoretical #012 escape (a follow-up
/// page packet with a different `tob_d` than the first packet) is
/// actually reachable from the wire. The unit harness H21 fuzzed
/// `bitset_put/del/has` directly and claimed a remote DoS severity
/// without verifying that there's a call chain from untrusted
/// packet bytes to the vulnerable bitset operation. This harness
/// does that reachability check.
///
/// Flow mirrored from `pkg/vere/io/mesa.c:_mesa_req_pact_done`:
///
///   1. Parse two packets from the fuzz input via
///      `mesa_sift_pact_from_buf`.
///   2. For the first PACT_PAGE packet, initialize a synthetic
///      `req_u` with `was_u.len_w = mesa_num_leaves(pkt1.tob_d)`.
///   3. For each subsequent PACT_PAGE packet, apply the exact guard
///      at mesa.c:1132 — `if (mesa_num_leaves(pkt.tob_d) <= pkt.fra_d) return;`
///   4. Then call `bitset_has(&was_u, pkt.fra_d)` and
///      `bitset_put(&was_u, pkt.fra_d)`.
///
/// This exercises the precise production chain of wire-bytes →
/// parser → guard → bitset ops, with the `req_u` representing
/// request state built from the first packet. If the fuzzer can
/// produce a two-packet sequence that trips the bitset assertion
/// (with the #011/#012 fixes reverted), the escape is real and
/// the fixes are not just "defensive hygiene."
///
/// Running with the fixes applied (the default state of the tree)
/// is expected to find zero crashes. To test reachability, revert
/// the `pkg/vere/io/mesa/bitset.c` patch temporarily and rerun.
///
/// Input layout:
///   byte 0..1    length N of packet 1 (mod the remaining size)
///   bytes 2..N+1 packet 1
///   bytes N+2..  packet 2
///
/// Build:   fuzz/build.sh fuzz_mesa_page_flow
/// Corpus:  fuzz/corpus/fuzz_mesa_page_flow/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"
#include "io/mesa/mesa.h"
#include "io/mesa/bitset.h"
#include "arena.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define ARENA_BYTES (1u << 20)
#define SCRATCH_BYTES 8192

/* Production mesa packets arrive in libuv-malloc'd buffers which
 * are 16-byte aligned. The AFL testcase buffer is byte-aligned, and
 * our length-prefix split gives pkt1/pkt2 at arbitrary offsets,
 * which trips a pre-existing UBSan alignment warning inside
 * ext/murmur3 when mesa verifies the packet checksum. These are
 * false positives w.r.t. real production — we copy both packets
 * into 16-byte-aligned scratch buffers to match the production
 * layout. */
static c3_y g_pkt1_buf[SCRATCH_BYTES] __attribute__((aligned(16)));
static c3_y g_pkt2_buf[SCRATCH_BYTES] __attribute__((aligned(16)));

int
main(void)
{
  /* Minimal u3 + ivory boot, same as H7. */
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if (sil_u == NULL) return 1;
  u3_weak pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len, u3_Ivory_pill);
  if (pil == u3_none) return 1;
  u3s_cue_xeno_done(sil_u);
  if (c3n == u3v_boot_lite(pil)) return 1;

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 8 || len > H_MAX_INPUT) {
    return 0;
  }

  /* Split the fuzz input into two packets. */
  c3_w pkt1_len = (((c3_w)buf[0]) | (((c3_w)buf[1]) << 8));
  c3_w remain = (c3_w)(len - 2);
  if (pkt1_len == 0 || pkt1_len >= remain || pkt1_len > SCRATCH_BYTES) {
    return 0;
  }
  c3_w pkt2_len = remain - pkt1_len;
  if (pkt2_len > SCRATCH_BYTES) {
    return 0;
  }
  memcpy(g_pkt1_buf, buf + 2, pkt1_len);
  memcpy(g_pkt2_buf, buf + 2 + pkt1_len, pkt2_len);

  /* Parse packet 1. Must be a valid PACT_PAGE. */
  u3_mesa_pact pac1 = {0};
  c3_c* err1 = mesa_sift_pact_from_buf(&pac1, g_pkt1_buf, pkt1_len);
  if (err1 != NULL || pac1.hed_u.typ_y != PACT_PAGE) {
    return 0;
  }

  /* Establish synthetic req_u state from packet 1. The real
   * `_mesa_req_pact_done` allocates this per request; we allocate
   * it on the stack and bitset_init it with leaves(pkt1.tob_d). */
  c3_d tof_d = mesa_num_leaves(pac1.pag_u.dat_u.tob_d);
  if (tof_d == 0 || tof_d > (1u << 18)) {
    /* bound the bitset size so we don't allocate gigabytes from a
     * crafted tob_d. 256 k leaves is well beyond any real mesa
     * request. */
    return 0;
  }
  arena are_u = arena_create(ARENA_BYTES);
  if (are_u.dat == NULL) return 0;

  u3_bitset was_u;
  bitset_init(&was_u, (c3_w)tof_d, &are_u);

  /* First-packet processing: guard + bitset ops. This should
   * succeed — the bitset was just sized from THIS packet's tob_d
   * so fra_d is in range by construction (modulo the guard). */
  {
    c3_d fra_d = pac1.pag_u.nam_u.fra_d;
    c3_d leaves_1 = mesa_num_leaves(pac1.pag_u.dat_u.tob_d);
    if (leaves_1 > fra_d) {
      (void)bitset_has(&was_u, (c3_w)fra_d);
      bitset_put(&was_u, (c3_w)fra_d);
    }
  }

  /* Parse packet 2 using the same sifter. This is the crafted
   * follow-up. */
  u3_mesa_pact pac2 = {0};
  c3_c* err2 = mesa_sift_pact_from_buf(&pac2, g_pkt2_buf, pkt2_len);
  if (err2 != NULL || pac2.hed_u.typ_y != PACT_PAGE) {
    free(are_u.dat);
    return 0;
  }

  /* Second-packet processing: exactly the production guard at
   * mesa.c:1132 followed by the bitset ops at 1141, 1159. The
   * critical question is: can a crafted pkt2 with a different
   * tob_d than pkt1 slip a fra_d past the guard that was_u
   * cannot handle? */
  {
    c3_d fra_d = pac2.pag_u.nam_u.fra_d;
    c3_d leaves_2 = mesa_num_leaves(pac2.pag_u.dat_u.tob_d);

    /* exact copy of mesa.c:1132 */
    if (leaves_2 > fra_d) {
      /* exact copy of mesa.c:1141 */
      if (c3y == bitset_has(&was_u, (c3_w)fra_d)) {
        /* duplicate — nothing to do */
      }
      else {
        /* exact copy of mesa.c:1159 or 1181 */
        bitset_put(&was_u, (c3_w)fra_d);
      }
    }
  }

  free(are_u.dat);
  return 0;
}
