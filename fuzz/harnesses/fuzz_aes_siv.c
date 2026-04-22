/// @file fuzz_aes_siv.c
///
/// H32 — AFL++ harness for the AES-SIV authenticated encryption jet
/// (`u3wea_siva_en` in `pkg/noun/jets/e/aes_siv.c`).
///
/// AES-SIV (AES-128-SIV, 32-byte key) is used by Urbit for encrypted
/// ames packet payloads — any peer can send ciphertext that lands here.
/// We fuzz the ENCRYPT direction (`siva-en`) which:
///   - exercises the full key-schedule and SIV MAC path
///   - avoids the bail-on-auth-failure issue present in the decrypt path
///     (decrypt bails with %evil on MAC mismatch, which u3m_soft catches
///      but wastes fuzzer cycles; encrypt never bails for bad crypto data)
///
/// The jet is a curried gate: the outer gate captures `key` and `ads`
/// (associated data list) in its context; the inner gate receives `txt`.
/// `u3wea_siva_en` reads:
///   u3x_sam         (axis  6)  → txt   (plaintext atom)
///   u3x_con_sam_2   (axis 60)  → key   (atom, ≤ 32 bytes)
///   u3x_con_sam_3   (axis 61)  → ads   (list of atoms, may be ~)
///
/// Core tree layout to satisfy those axes:
///   core[2]  = bat (u3_nul)
///   core[3]  = [sam ctx]
///   core[6]  = sam = txt
///   core[7]  = ctx
///   core[14] = ctx_bat (u3_nul)
///   core[15] = ctx_payload = [ctx_sam ctx_ctx]
///   core[30] = ctx_sam = [key ads]
///   core[31] = ctx_ctx (u3_nul)
///   core[60] = key
///   core[61] = ads
///
/// So the core noun is:
///   [u3_nul [txt [u3_nul [[key ads] u3_nul]]]]
///
/// Input layout from fuzz bytes:
///   [0..31]   → key    (32 bytes, AES-128-SIV uses 32-byte key)
///   [32]      → n_ads  (0..3; number of associated-data items to include)
///   [33..48]  → aad1   (16 bytes, used if n_ads >= 1)
///   [49..64]  → aad2   (16 bytes, used if n_ads >= 2)
///   [65..80]  → aad3   (16 bytes, used if n_ads >= 3)
///   [81..end] → plaintext (variable length)
///
/// Minimum input: 82 bytes (32 key + 1 count + 48 aad + 1 byte plaintext).
/// We relax to minimum 33 bytes (key + count byte) and allow zero-length
/// plaintext and zero aad items so the fuzzer can explore boundary cases.
///
/// Build:   fuzz/build.sh fuzz_aes_siv
/// Corpus:  fuzz/corpus/fuzz_aes_siv/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

extern u3_noun u3wea_siva_en(u3_noun cor);

__AFL_FUZZ_INIT();

#define H_MIN_INPUT  33   /* 32 key + 1 n_ads byte */
#define H_MAX_INPUT  65536

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

static u3_noun
_siva_en_soft(u3_noun ignored)
{
  (void)ignored;

  /* Parse key (32 bytes). */
  u3_atom key = u3i_bytes(32, g_buf);

  /* Parse n_ads: clamp to 0..3. */
  uint8_t n_ads = g_buf[32] % 4;

  /* Parse associated data items.
   * Each item is 16 bytes from the fixed region [33..80]. */
  static const c3_w AAD_ITEM_LEN = 16;
  static const c3_d AAD_OFFSET   = 33;

  /* Build the ads noun as a (list @) = ~, [aad1 ~], [aad1 [aad2 ~]], etc.
   * The list is cons-terminated with u3_nul (~ = 0). */
  u3_noun ads = u3_nul;
  for (int i = (int)n_ads - 1; i >= 0; i--) {
    c3_d item_off = AAD_OFFSET + (c3_d)i * (c3_d)AAD_ITEM_LEN;
    u3_atom item;
    if (item_off + (c3_d)AAD_ITEM_LEN <= g_len) {
      item = u3i_bytes(AAD_ITEM_LEN, g_buf + item_off);
    }
    else {
      /* Not enough input for this item; use a zero atom. */
      item = 0;
    }
    ads = u3nc(item, ads);
  }

  /* Parse plaintext: everything after the 81-byte fixed header. */
  c3_d txt_off = AAD_OFFSET + 3 * (c3_d)AAD_ITEM_LEN;  /* 33 + 48 = 81 */
  c3_w txt_len = 0;
  u3_atom txt;
  if (g_len > txt_off) {
    txt_len = (c3_w)(g_len - txt_off);
    txt = u3i_bytes(txt_len, g_buf + txt_off);
  }
  else {
    txt = 0;   /* zero-length plaintext: atom 0 */
  }

  /* Construct the curried-gate core.
   *
   * We need:
   *   core[6]  = txt        (u3x_sam)
   *   core[60] = key        (u3x_con_sam_2)
   *   core[61] = ads        (u3x_con_sam_3)
   *
   * Core layout: [bat [sam ctx]]
   *   bat      = u3_nul   (axis 2)
   *   sam      = txt       (axis 6)
   *   ctx      = [ctx_bat [ctx_sam ctx_ctx]]   (axis 7)
   *   ctx_bat  = u3_nul   (axis 14)
   *   ctx_sam  = [key ads] (axis 30 = u3x_con_sam)
   *   ctx_ctx  = u3_nul   (axis 31)
   *   key      = core[60]
   *   ads      = core[61]
   *
   * Building bottom-up:
   */
  u3_noun ctx_sam     = u3nc(key, ads);                     /* [key ads]        */
  u3_noun ctx_payload = u3nc(ctx_sam, u3_nul);              /* [[key ads] ~]    */
  u3_noun ctx         = u3nc(u3_nul, ctx_payload);          /* [~ [[key ads] ~]]*/
  u3_noun core_pay    = u3nc(txt, ctx);                     /* [txt ctx]        */
  u3_noun core        = u3nc(u3_nul, core_pay);             /* [~ [txt ctx]]    */

  u3_noun pro = u3wea_siva_en(core);

  if (u3_none != pro) u3z(pro);
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

  if (len < H_MIN_INPUT || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _siva_en_soft, u3_nul);
  u3z(pro);

  return 0;
}
