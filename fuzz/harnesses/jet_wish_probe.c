/// @file jet_wish_probe.c
///
/// One-shot probe: attempt u3v_wish on a batch of candidate paths against
/// the booted brass-pill pier. Prints OK/FAIL per path so we can plan
/// which to wire into differential harnesses next.
///
/// Usage: ./fuzz/out/jet_wish_probe.afl

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ur/ur.h"
#include "vere.h"

extern void u3m_init(size_t len_i);
extern void u3m_pave(c3_o nuu_o);
extern void u3t_init(void);
extern c3_w  u3j_boot(c3_o nuu_o);
extern void u3j_ream(void);
extern void u3n_ream(void);
extern c3_o u3e_live(c3_o nuu_o, c3_c* dir_c);
extern c3_o u3e_yolo(void);
extern c3_c* u3m_pier(c3_c* dir_c);

#define PIER_DIR "/tmp/fuzz-pier-zod-v44"

static const char* g_wish_name = 0;

static u3_noun
_wish_soft(u3_noun ignored)
{
  (void)ignored;
  return u3v_wish(g_wish_name);
}

/* Candidate paths grouped by suspected target. Anything not resolvable
 * will print FAIL — the user can then suggest alternative paths. */
static const char* _candidates[] = {
  /* === AES door (symmetric cipher) === */
  "en:siva:aes:crypto",   /* SIV 128-bit encrypt */
  "de:siva:aes:crypto",
  "en:sivb:aes:crypto",
  "de:sivb:aes:crypto",
  "en:sivc:aes:crypto",
  "de:sivc:aes:crypto",
  "en:cbca:aes:crypto",   /* CBC-128 */
  "de:cbca:aes:crypto",
  "en:cbcb:aes:crypto",
  "de:cbcb:aes:crypto",
  "en:cbcc:aes:crypto",
  "de:cbcc:aes:crypto",
  "en:ecba:aes:crypto",   /* ECB-128 */
  "de:ecba:aes:crypto",
  "en:ecbb:aes:crypto",
  "de:ecbb:aes:crypto",
  "en:ecbc:aes:crypto",
  "de:ecbc:aes:crypto",

  /* === secp256k1 (using Hoon arm names, not C-side %sign etc.) === */
  "make-k:secp256k1:secp:crypto",
  "ecdsa-raw-sign:secp256k1:secp:crypto",
  "ecdsa-raw-recover:secp256k1:secp:crypto",
  "priv-to-pub:secp256k1:secp:crypto",
  "sign:schnorr:secp256k1:secp:crypto",
  "verify:schnorr:secp256k1:secp:crypto",

  /* === KDFs & symmetric primitives === */
  "argon2:crypto",
  "pbkdf:crypto",
  "pbk:scr:crypto",               /* scrypt pbkdf */
  "pbl:scr:crypto",
  "hsh:scr:crypto",               /* scrypt mixing */
  "hsl:scr:crypto",

  /* === Hashes === */
  "ripemd-160:ripemd:crypto",
  "hmac:crypto",
  "sha1:sha",                     /* lone jetted arm */
  "sha-256l:sha",
  "sha-512l:sha",
  "shax", "shay", "shas", "shal",

  /* === ChaCha20 === */
  "crypt:chacha:crypto",
  "xchacha:chacha:crypto",

  /* === Parser combinators (top-level or shoe:parse) === */
  "cook",
  "cold",
  "easy",
  "fail",
  "just",
  "mask",
  "shim",
  "fuse",
  "glue",
  "bend",
  "stew",
  "stir",
  "knee",

  /* === List ops === */
  "turn",
  "roll",
  "reel",
  "skim",
  "skip",
  "sort",
  "levy",
  "lien",
  "zing",
  "weld",

  /* === Math === */
  "bex",
  "xeb",
  "sqt",
  "pow",

  /* === Floats rq === */
  "add:rq",
  "sub:rq",
  "mul:rq",
  "div:rq",
  "lth:rq",
  "equ:rq",

  /* === Other === */
  "og",                         /* random generator */
  "mug",                        /* hash */
  "muk",                        /* low-level mug */
  "jam",                        /* serialization */
  "cue",                        /* deserialization */
  "en:json:html",               /* JSON encode */
  "de:json:html",               /* JSON decode */

  0
};

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "pier %s empty\n", PIER_DIR);
    return 1;
  }
  u3e_yolo();
  u3C.slog_f = 0;
  u3C.sign_hold_f = 0;
  u3C.sign_move_f = 0;
  u3t_init();
  u3m_pave(nuu_o);
  u3j_boot(nuu_o);
  u3j_ream();
  u3n_ream();
  fprintf(stderr, "probe: booted eve %llu\n",
          (unsigned long long)u3A->eve_d);

  c3_w ok = 0, fail = 0;
  for ( c3_w i = 0; _candidates[i]; i++ ) {
    g_wish_name = _candidates[i];
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    u3_noun how = u3h(pro);
    u3_noun res = u3t(pro);
    if ( 0 == how && u3_none != res ) {
      fprintf(stderr, "OK   %s\n", _candidates[i]);
      ok++;
    } else {
      fprintf(stderr, "FAIL %s\n", _candidates[i]);
      fail++;
    }
    u3z(pro);
  }
  fprintf(stderr, "\nprobe: %u OK, %u FAIL\n", ok, fail);
  return 0;
}
