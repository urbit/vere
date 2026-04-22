/// @file fuzz_jet_l11_base.c
///
/// Ice-oracle fuzzer for base-desk (zuse) jets.
/// Reaches ~26 jets that are not in the ivory pill: crc32,
/// keccak-{224,256,384,512}, blake2b, blake3, hmac-{sha1,sha256,sha512},
/// ed25519 (puck/sign/veri/shar), base16/base58/base64 codecs,
/// xml/urlt codecs.
///
/// Requirement: boot a fake pier with the brass pill first:
///   ./zig-out/x86_64-linux-musl/urbit -F zod \
///     -B fuzz/pills/urbit-v3.5.pill \
///     -c /tmp/fuzz-pier-zod-v44 -l -L -q -d -t
/// (~18 min boot, produces image.bin ~729 MB)
///
/// Key insight: zuse's top-level chapters (crc, crypto, html) are
/// merged directly into the kernel's wish scope. u3v_wish("crc32:crc")
/// works even though "crc32:crc:zuse" bails — the opacity wall on
/// ++zuse blocks access THROUGH a zuse face but not directly to its
/// chapters.
///
/// Build:  fuzz/build.sh fuzz_jet_l11_base   (vere-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define PIER_DIR "/tmp/fuzz-pier-zod-v44"

extern void u3m_init(size_t len_i);
extern void u3m_pave(c3_o nuu_o);
extern void u3t_init(void);
extern c3_w  u3j_boot(c3_o nuu_o);
extern void u3j_ream(void);
extern void u3n_ream(void);
extern c3_o u3e_live(c3_o nuu_o, c3_c* dir_c);
extern c3_o u3e_yolo(void);
extern c3_c* u3m_pier(c3_c* dir_c);

typedef enum {
  S_OCTS,           /* [len=@ud dat=@] */
  S_ATOM,           /* single atom     */
  S_KEY_MSG,        /* [key=@ msg=@]   */
  S_MSG_SEC,        /* [msg=@ sec=@]   */
  S_PUB_SEC,        /* [pub=@ sec=@]   */
  S_SIG_MSG_PUB,    /* [sig=@ msg=@ pub=@] */
  S_BLAKE2B,        /* [msg=octs key=octs out=@ud] */
} shape_e;

typedef struct {
  const char* nam_c;   /* wish path, e.g. "crc32:crc"      */
  const char* cor_c;   /* core name for ice flip           */
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "crc32:crc",                  "crc32",       S_OCTS,        0 },
  { "keccak-256:keccak:crypto",   "keccak-256",  S_OCTS,        0 },
  { "keccak-512:keccak:crypto",   "keccak-512",  S_OCTS,        0 },
  { "keccak-224:keccak:crypto",   "keccak-224",  S_OCTS,        0 },
  { "keccak-384:keccak:crypto",   "keccak-384",  S_OCTS,        0 },
  { "blake3:blake:crypto",        "blake3",      S_OCTS,        0 },
  { "blake2b:blake:crypto",       "blake2b",     S_BLAKE2B,     0 },
  { "hmac-sha256:hmac:crypto",    "hmac-sha256", S_KEY_MSG,     0 },
  { "hmac-sha512:hmac:crypto",    "hmac-sha512", S_KEY_MSG,     0 },
  { "hmac-sha1:hmac:crypto",      "hmac-sha1",   S_KEY_MSG,     0 },
  { "puck:ed:crypto",             "puck",        S_ATOM,        0 },
  { "sign:ed:crypto",             "sign",        S_MSG_SEC,     0 },
  { "veri:ed:crypto",             "veri",        S_SIG_MSG_PUB, 0 },
  { "shar:ed:crypto",             "shar",        S_PUB_SEC,     0 },
  { "en:base16:mimes:html",       "en-base16",   S_OCTS,        0 },
  { "de:base16:mimes:html",       "de-base16",   S_ATOM,        0 },
  { "en-base58:mimes:html",       "en-base58",   S_ATOM,        0 },
  { "de-base58:mimes:html",       "de-base58",   S_ATOM,        0 },
  { "en:base64:mimes:html",       "en-base64",   S_OCTS,        0 },
  { "de:base64:mimes:html",       "de-base64",   S_ATOM,        0 },
  { "en-urlt:html",               "en-urlt",     S_ATOM,        0 },
  { "de-urlt:html",               "de-urlt",     S_ATOM,        0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;
static const char* g_wish_name = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

static u3_noun
_wish_soft(u3_noun ignored)
{
  (void)ignored;
  return u3v_wish(g_wish_name);
}

/* Build sample of the right shape from `n` bytes of fuzz input. */
static u3_noun
_build_sample(shape_e shp_e, const uint8_t* p, c3_w n)
{
  /* clamp to keep Hoon-side iteration manageable */
  if ( n > 1024 ) n = 1024;

  switch ( shp_e ) {
    default:
    case S_OCTS: {
      u3_atom dat = u3i_bytes(n, p);
      return u3nc(u3i_word(n), dat);
    }
    case S_ATOM: {
      return u3i_bytes(n, p);
    }
    case S_KEY_MSG: {
      c3_w half = n / 2;
      u3_atom k = u3i_bytes(half, p);
      u3_atom m = u3i_bytes(n - half, p + half);
      return u3nc(k, m);
    }
    case S_MSG_SEC: {
      /* sign:ed — (pair @ @) msg, sec. sec is 32+32 bytes of secret. */
      c3_w msg_len = n > 64 ? n - 64 : 1;
      u3_atom msg = u3i_bytes(msg_len, p);
      u3_atom sec = u3i_bytes(64, (n > 64) ? p + msg_len : p);
      /* fall back if too short */
      if ( n <= 64 ) sec = u3i_bytes(n, p);
      return u3nc(msg, sec);
    }
    case S_PUB_SEC: {
      /* shar:ed — (pair pub=@I sec=@I), each 32 bytes. */
      uint8_t buf[64] = {0};
      c3_w take = n > 64 ? 64 : n;
      memcpy(buf, p, take);
      u3_atom pub = u3i_bytes(32, buf);
      u3_atom sec = u3i_bytes(32, buf + 32);
      return u3nc(pub, sec);
    }
    case S_SIG_MSG_PUB: {
      /* veri:ed — (trel sig=@I msg=@ pub=@I). sig=64, pub=32. */
      uint8_t buf[128] = {0};
      c3_w take = n > 128 ? 128 : n;
      memcpy(buf, p, take);
      u3_atom sig = u3i_bytes(64, buf);
      u3_atom pub = u3i_bytes(32, buf + 64);
      u3_atom msg = u3i_bytes((n > 128) ? n - 128 : 1,
                               (n > 128) ? p + 128 : buf + 96);
      return u3nt(sig, msg, pub);
    }
    case S_BLAKE2B: {
      /* (trel msg=octs key=octs out=@ud) */
      c3_w third = n / 3;
      u3_atom msg_dat = u3i_bytes(third, p);
      u3_noun msg = u3nc(u3i_word(third), msg_dat);
      u3_atom key_dat = u3i_bytes(third, p + third);
      u3_noun key = u3nc(u3i_word(third), key_dat);
      /* clamp out to 1..64 (blake2b limit) */
      c3_w out_w = (p[0] % 64) + 1;
      return u3nt(msg, key, u3i_word(out_w));
    }
  }
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;

  /* Boot from snapshot (same flow as the inline u3m_boot replica). */
  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "l11: pier at %s is empty — boot it first\n", PIER_DIR);
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
  fprintf(stderr, "l11: restored from %s at eve %llu\n",
          PIER_DIR, (unsigned long long)u3A->eve_d);

  /* Wish each target and cache the gate. */
  c3_w ok = 0;
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    g_wish_name = _jets[i].nam_c;
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    if ( 0 != u3h(pro) || u3_none == u3t(pro) ) {
      fprintf(stderr, "l11: skip '%s'\n", _jets[i].nam_c);
      _jets[i].gate = 0;
    } else {
      _jets[i].gate = u3k(u3t(pro));
      ok++;
    }
    u3z(pro);
  }
  fprintf(stderr, "l11: %u/%u jets cached\n", ok, _jets_n);

  /* Flip ice on each cached jet's core. */
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].cor_c);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_INPUT ) return 0;

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  u3_noun sam = _build_sample(jet->shp_e, buf + 1, len - 1);

  g_gate = jet->gate;
  g_sam  = sam;
  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l11_base: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
