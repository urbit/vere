/// @file fuzz_jet_l13_asym.c
///
/// Ice-oracle fuzzer for secp256k1 + schnorr jet arms on zuse:
///   make-k:secp256k1:secp:crypto          (core=%make)
///   ecdsa-raw-sign:secp256k1:secp:crypto  (core=%sign)
///   ecdsa-raw-recover:secp256k1:secp:crypto (core=%reco)
///   sign:schnorr:secp256k1:secp:crypto    (core=%sosi inside %schnorr)
///   verify:schnorr:secp256k1:secp:crypto  (core=%sove inside %schnorr)
///
/// Requires a pre-booted brass pier with zuse loaded at PIER_DIR;
/// we restore from its snapshot and wish each arm to cache its gate.
///
/// Build:  fuzz/build.sh fuzz_jet_l13_asym   (vere-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define H_CLAMP     128
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
  S_HASH_PRIV,       /* [hash=@uvI priv=@]              32 + 32 bytes */
  S_HASH_SIG,        /* [hash=@ [v=@ r=@ s=@]]          32 + 1 + 32 + 32 */
  S_SCHNORR_SIGN,    /* [sk=@I m=@I a=@I]               32 + 32 + 32 */
  S_SCHNORR_VERIFY,  /* [pk=@I m=@I sig=@J]             32 + 32 + 64 */
} shape_e;

typedef struct {
  const char* nam_c;   /* wish path */
  const char* cor_c;   /* core name for ice flip */
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "make-k:secp256k1:secp:crypto",           "make",  S_HASH_PRIV,      0 },
  { "ecdsa-raw-sign:secp256k1:secp:crypto",   "sign",  S_HASH_PRIV,      0 },
  { "ecdsa-raw-recover:secp256k1:secp:crypto","reco",  S_HASH_SIG,       0 },
  { "sign:schnorr:secp256k1:secp:crypto",     "sosi",  S_SCHNORR_SIGN,   0 },
  { "verify:schnorr:secp256k1:secp:crypto",   "sove",  S_SCHNORR_VERIFY, 0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

/* Extra core names to ice-flip — `schnorr` is the parent door of
** sosi/sove. Flipping it too ensures any path that dispatches via
** the door's battery also exercises the differential oracle. */
static const char* _extra_cores[] = {
  "secp256k1",
  "schnorr",
};
static const c3_w _extra_cores_n =
  sizeof(_extra_cores) / sizeof(_extra_cores[0]);

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

/* Build sample of the right shape from `n` bytes of fuzz input.
**   Inputs are clamped to H_CLAMP (128) bytes and we pad with zeros
**   so every shape gets predictable byte spans. */
static u3_noun
_build_sample(shape_e shp_e, const uint8_t* p, c3_w n)
{
  uint8_t buf[H_CLAMP] = {0};
  c3_w    take = (n > H_CLAMP) ? H_CLAMP : n;
  memcpy(buf, p, take);

  switch ( shp_e ) {
    default:
    case S_HASH_PRIV: {
      /* [hash=@uvI priv=@]  32 bytes hash, 32 bytes priv */
      u3_atom hash = u3i_bytes(32, buf);
      u3_atom priv = u3i_bytes(32, buf + 32);
      return u3nc(hash, priv);
    }
    case S_HASH_SIG: {
      /* [hash=@ sig=[v=@ r=@ s=@]]  32 hash, 1 v, 32 r, 32 s */
      u3_atom hash = u3i_bytes(32, buf);
      u3_atom v    = u3i_bytes(1,  buf + 32);
      u3_atom r    = u3i_bytes(32, buf + 33);
      u3_atom s    = u3i_bytes(32, buf + 65);
      return u3nc(hash, u3nt(v, r, s));
    }
    case S_SCHNORR_SIGN: {
      /* [sk=@I m=@I a=@I]  3 × 32 bytes */
      u3_atom sk = u3i_bytes(32, buf);
      u3_atom m  = u3i_bytes(32, buf + 32);
      u3_atom a  = u3i_bytes(32, buf + 64);
      return u3nt(sk, m, a);
    }
    case S_SCHNORR_VERIFY: {
      /* [pk=@I m=@I sig=@J]  pk 32, m 32, sig 64 */
      u3_atom pk  = u3i_bytes(32, buf);
      u3_atom m   = u3i_bytes(32, buf + 32);
      u3_atom sig = u3i_bytes(64, buf + 64);
      return u3nt(pk, m, sig);
    }
  }
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;

  /* Boot from snapshot. */
  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "l13: pier at %s is empty — boot it first\n", PIER_DIR);
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
  fprintf(stderr, "l13: restored from %s at eve %llu\n",
          PIER_DIR, (unsigned long long)u3A->eve_d);

  /* Wish each target and cache the gate. */
  c3_w ok = 0;
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    g_wish_name = _jets[i].nam_c;
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    if ( 0 != u3h(pro) || u3_none == u3t(pro) ) {
      fprintf(stderr, "l13: skip '%s'\n", _jets[i].nam_c);
      _jets[i].gate = 0;
    } else {
      _jets[i].gate = u3k(u3t(pro));
      ok++;
    }
    u3z(pro);
  }
  fprintf(stderr, "l13: %u/%u jets cached\n", ok, _jets_n);

  /* Flip ice on each cached jet's core (per-arm name). */
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].cor_c);
  }
  /* Also flip the parent door names (%secp256k1 / %schnorr) so
  ** nested dispatch paths go through the differential oracle. */
  for ( c3_w i = 0; i < _extra_cores_n; i++ ) {
    (void)u3j_fuzz_arm(_extra_cores[i]);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 2 || len > H_MAX_INPUT ) return 0;

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  u3_noun sam = _build_sample(jet->shp_e, buf + 1, (c3_w)(len - 1));

  g_gate = jet->gate;
  g_sam  = sam;
  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l13_asym: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
