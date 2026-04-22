/// @file fuzz_jet_l10_crypto.c
///
/// Crash-finding harness for cryptographic jets whose top-level
/// names are NOT resolvable via u3v_wish against the ivory pill
/// (shax/shay/shal/shas/ripe, etc.). NOT differential — calls the
/// C jet wrapper directly and relies on ASan/UBSan for bug detection.
///
/// Build:  fuzz/build.sh fuzz_jet_l10_crypto    (noun-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

extern u3_noun u3we_shax(u3_noun cor);
extern u3_noun u3we_shay(u3_noun cor);
extern u3_noun u3we_shal(u3_noun cor);
extern u3_noun u3we_shas(u3_noun cor);
extern u3_noun u3we_sha1(u3_noun cor);
extern u3_noun u3we_ripe(u3_noun cor);
extern u3_noun u3we_kecc256(u3_noun cor);
extern u3_noun u3we_kecc512(u3_noun cor);
extern u3_noun u3we_hmac(u3_noun cor);
extern u3_noun u3we_blake2b(u3_noun cor);

typedef u3_noun (*jet_fn_t)(u3_noun);

typedef enum {
  S_ATOM1,       /* u3x_sam = atom                  */
  S_WID_DAT,     /* u3x_sam_2 = wid, u3x_sam_3 = dat*/
} shape_e;

static const struct {
  const char* nam;
  jet_fn_t    fn;
  shape_e     shp;
} _jets[] = {
  { "shax",    u3we_shax,    S_ATOM1   },
  { "shay",    u3we_shay,    S_WID_DAT },
  { "shal",    u3we_shal,    S_WID_DAT },
  { "shas",    u3we_shas,    S_WID_DAT },   /* (sal ruz) */
  { "sha1",    u3we_sha1,    S_WID_DAT },
  { "ripe",    u3we_ripe,    S_WID_DAT },
  { "kecc256", u3we_kecc256, S_ATOM1   },
  { "kecc512", u3we_kecc512, S_ATOM1   },
  { "blake2b", u3we_blake2b, S_WID_DAT }, /* rough — real signature is more complex */
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_cor = 0;
static jet_fn_t g_fn = 0;

static u3_noun
_call_soft(u3_noun ignored)
{
  (void)ignored;
  return g_fn(u3k(g_cor));
}

int
main(void)
{
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_INPUT ) return 0;

  c3_w mode = buf[0] % _jets_n;
  g_fn = _jets[mode].fn;

  c3_w dlen = len - 1;
  if ( dlen > 4096 ) dlen = 4096;
  const uint8_t* p = buf + 1;

  u3_atom dat = u3i_bytes(dlen, p);
  u3_noun sam;
  if ( _jets[mode].shp == S_ATOM1 ) {
    sam = dat;
  }
  else {
    sam = u3nc(u3i_word(dlen), dat);
  }
  /* Build a synthetic core [~ [sam ~]] — u3r_at(u3x_sam, cor) returns sam. */
  g_cor = u3nc(u3_nul, u3nc(sam, u3_nul));

  u3_noun pro = u3m_soft(0, _call_soft, u3_nul);
  u3z(g_cor);
  u3z(pro);
  return 0;
}
