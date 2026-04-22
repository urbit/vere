/// @file fuzz_ed_veri.c
///
/// H27 — AFL++ harness for the ed25519 signature-verification jet
/// (`u3wee_veri` in `pkg/noun/jets/e/ed_veri.c`).
///
/// ed25519 verify is called every time Arvo needs to authenticate a
/// message from a peer — any ship can craft an arbitrary (sig, msg, pk)
/// triple and have it land here. The crypto library itself is presumably
/// correct, but the urbit glue (byte-width checks, noun unpacking,
/// allocation paths) is the attack surface.
///
/// Input shape: the fuzz bytes are split at fixed offsets into three
/// atoms that are passed as a synthesised 3-cell [sig msg pk]:
///   bytes [0..63]   → signature  (64 bytes, ed25519 sig)
///   bytes [64..95]  → pubkey     (32 bytes, ed25519 public key)
///   bytes [96..end] → message    (variable length)
///
/// We use the `u3qe_`-style inner function (`_cqee_veri`) which is
/// `static`, so we call through the wrapper `u3wee_veri` instead,
/// constructing a fake "core" noun whose sample slots match what
/// u3wee_veri extracts via u3r_mean:
///   u3x_sam_2 → sig
///   u3x_sam_6 → msg
///   u3x_sam_7 → pub
///
/// The core layout expected by u3r_mean at positions 2/6/7 maps to
/// the cell [[[sig [msg pub]] ...]] so we build:
///   sam = [sig [msg pub]]
/// which is a 3-element tuple [sig msg pub] in the standard cell tree:
///   node 2 = sig  (u3h of node 1)
///   node 3 = [msg pub]
///   node 6 = msg  (u3h of node 3)
///   node 7 = pub  (u3t of node 3)
/// The core is then [battery sam context], but u3r_mean on the full
/// core only indexes the sample subtree so we pass [u3_nul sam u3_nul].
///
/// We wrap everything in u3m_soft so bail()s (invalid key format,
/// wrong size, etc.) are caught cleanly.
///
/// Build:   fuzz/build.sh fuzz_ed_veri
/// Corpus:  fuzz/corpus/fuzz_ed_veri/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* The wrapper function declared in jets/w.h; forward-declare to avoid
 * pulling in the entire jet-table machinery. */
extern u3_noun u3wee_veri(u3_noun cor);

__AFL_FUZZ_INIT();

/* Keep the input reasonably small — the message can be up to ~64 KiB;
 * anything beyond that doesn't exercise new code paths. */
#define H_MAX_INPUT 65536
#define H_SIG_LEN   64
#define H_PUB_LEN   32
#define H_FIXED_HDR (H_SIG_LEN + H_PUB_LEN)

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_veri_soft(u3_noun ignored)
{
  (void)ignored;

  /* Require at least the fixed header (sig + pub). The message may be
   * zero bytes. */
  if (g_len < (c3_d)H_FIXED_HDR) {
    return u3_nul;
  }

  c3_d msg_len = g_len - (c3_d)H_FIXED_HDR;

  /* Build atoms from the fixed-layout buffer. */
  u3_atom sig = u3i_bytes(H_SIG_LEN, g_buf);
  u3_atom pub = u3i_bytes(H_PUB_LEN, g_buf + H_SIG_LEN);
  u3_atom msg = u3i_bytes((c3_w)msg_len, g_buf + H_FIXED_HDR);

  /* Construct the sample cell [sig [msg pub]].
   * Position indices in the core noun tree:
   *   core[2] = battery   (we use u3_nul)
   *   core[3] = [sam ctx]
   *   core[6] = sam       = [sig [msg pub]]
   *   core[7] = ctx       (we use u3_nul)
   * u3wee_veri extracts:
   *   u3x_sam_2  (=12) → sig
   *   u3x_sam_6  (=26) → msg
   *   u3x_sam_7  (=27) → pub
   *
   * Working backwards from the axis numbers:
   *   axis 12 = take left(left(right(root))) i.e. root/3/6/12
   * The simplest way: build a minimal core that has the right atoms at
   * the right axes. u3r_mean walks axis paths on the noun, so we need:
   *   axis 2 = sig  → left child of root
   *   axis 6 = msg  → left child of right child of left child of root
   *                  i.e. root = [[[sig [msg pub]] ...] ...]
   * For a gate-shaped core [bat [sam ctx]]:
   *   axis 2  = bat
   *   axis 3  = [sam ctx]
   *   axis 6  = sam
   *   axis 7  = ctx
   *   axis 12 = sam_2 = head of sam
   *   axis 13 = sam_3 = tail of sam
   *   axis 26 = sam_6 = head(tail(sam))
   *   axis 27 = sam_7 = tail(tail(sam))
   *
   * So we need sam = [sig [msg pub]], then:
   *   core = [u3_nul [sam u3_nul]]
   */
  u3_noun sam  = u3nt(sig, msg, pub);  /* [sig msg pub] = [[sig [msg pub]]] */
  u3_noun core = u3nc(u3_nul, u3nc(sam, u3_nul));

  u3_noun pro = u3wee_veri(core);

  /* pro is either c3y (%.y), c3n (%.n), or a bail from u3m_soft. */
  u3z(pro);
  u3z(core);

  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  if (len < (int)H_FIXED_HDR || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _veri_soft, u3_nul);
  u3z(pro);

  return 0;
}
