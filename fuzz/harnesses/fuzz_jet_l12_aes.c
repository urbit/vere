/// @file fuzz_jet_l12_aes.c
///
/// Ice-oracle fuzzer for AES jet arms on the base desk (zuse).
/// Covers 18 arms across 9 cores:
///   en/de : siva/sivb/sivc : aes:crypto   (SIV modes, 128/192/256)
///   en/de : cbca/cbcb/cbcc : aes:crypto   (CBC modes, 128/192/256)
///   en/de : ecba/ecbb/ecbc : aes:crypto   (ECB modes, 128/192/256)
///
/// One ice-flip per core (siva/sivb/sivc/cbca/cbcb/cbcc/ecba/ecbb/ecbc)
/// covers both en and de arms inside each core.
///
/// The AES door-samples (keys) default to 0 when wished bare — that's
/// fine, the jet code paths still exercise. Fuzz bytes drive the
/// arm-sample (blk / txt / siv-tuple).
///
/// Requirement: pre-booted brass-pill pier with zuse loaded at
///   /tmp/fuzz-pier-zod-v44  (see fuzz_jet_l11_base.c for boot recipe).
///
/// Build:  fuzz/build.sh fuzz_jet_l12_aes   (vere-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define H_MAX_PAYLOAD 512
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
  S_ATOM,           /* plain atom — for ECB blk=@H and CBC txt=@ */
  S_SIV_EN,         /* [key=@H vec=(list @) txt=@]               */
  S_SIV_DE,         /* [key=@H vec=(list @) len=@ud txt=@]       */
} shape_e;

typedef struct {
  const char* nam_c;   /* wish path, e.g. "en:siva:aes:crypto" */
  const char* cor_c;   /* core name for ice flip               */
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  /* SIV modes — arm en/de have different shapes */
  { "en:siva:aes:crypto", "siva", S_SIV_EN, 0 },
  { "de:siva:aes:crypto", "siva", S_SIV_DE, 0 },
  { "en:sivb:aes:crypto", "sivb", S_SIV_EN, 0 },
  { "de:sivb:aes:crypto", "sivb", S_SIV_DE, 0 },
  { "en:sivc:aes:crypto", "sivc", S_SIV_EN, 0 },
  { "de:sivc:aes:crypto", "sivc", S_SIV_DE, 0 },
  /* CBC modes — arm takes txt=@ */
  { "en:cbca:aes:crypto", "cbca", S_ATOM, 0 },
  { "de:cbca:aes:crypto", "cbca", S_ATOM, 0 },
  { "en:cbcb:aes:crypto", "cbcb", S_ATOM, 0 },
  { "de:cbcb:aes:crypto", "cbcb", S_ATOM, 0 },
  { "en:cbcc:aes:crypto", "cbcc", S_ATOM, 0 },
  { "de:cbcc:aes:crypto", "cbcc", S_ATOM, 0 },
  /* ECB modes — arm takes blk=@H */
  { "en:ecba:aes:crypto", "ecba", S_ATOM, 0 },
  { "de:ecba:aes:crypto", "ecba", S_ATOM, 0 },
  { "en:ecbb:aes:crypto", "ecbb", S_ATOM, 0 },
  { "de:ecbb:aes:crypto", "ecbb", S_ATOM, 0 },
  { "en:ecbc:aes:crypto", "ecbc", S_ATOM, 0 },
  { "de:ecbc:aes:crypto", "ecbc", S_ATOM, 0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

/* Core-name walk matches against the ARM name in the dev_u tree, not
 * the outer AES mode core. "en" and "de" together cover every en/de
 * arm across AES modes (and incidentally across base16/58/64/json too,
 * which is harmless — oracle fires on ALL en/de arms). */
static const char* _cores[] = { "en", "de" };
static const c3_w _cores_n = sizeof(_cores) / sizeof(_cores[0]);

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
  /* clamp tightly — AES Hoon is slow with large inputs */
  if ( n > H_MAX_PAYLOAD ) n = H_MAX_PAYLOAD;

  switch ( shp_e ) {
    default:
    case S_ATOM: {
      return u3i_bytes(n, p);
    }
    case S_SIV_EN: {
      /* [key=@H vec=(list @) txt=@]
       * key=0 (door-sample default), vec=~ (empty list),
       * txt = fuzz bytes as atom. */
      return u3nt(u3i_word(0), u3_nul, u3i_bytes(n, p));
    }
    case S_SIV_DE: {
      /* [key=@H vec=(list @) len=@ud txt=@]
       * key=0, vec=~, len = n (claimed plaintext len), txt = fuzz atom. */
      return u3nq(u3i_word(0), u3_nul, u3i_word(n), u3i_bytes(n, p));
    }
  }
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;

  /* Boot from snapshot (same flow as l11). */
  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "l12: pier at %s is empty — boot it first\n", PIER_DIR);
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
  fprintf(stderr, "l12: restored from %s at eve %llu\n",
          PIER_DIR, (unsigned long long)u3A->eve_d);

  /* Wish each arm and cache the gate. */
  c3_w ok = 0;
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    g_wish_name = _jets[i].nam_c;
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    if ( 0 != u3h(pro) || u3_none == u3t(pro) ) {
      fprintf(stderr, "l12: skip '%s'\n", _jets[i].nam_c);
      _jets[i].gate = 0;
    } else {
      _jets[i].gate = u3k(u3t(pro));
      ok++;
    }
    u3z(pro);
  }
  fprintf(stderr, "l12: %u/%u jets cached\n", ok, _jets_n);

  /* Flip ice on each unique core (one ice-flip covers both en and de). */
  for ( c3_w i = 0; i < _cores_n; i++ ) {
    (void)u3j_fuzz_arm(_cores[i]);
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
      fprintf(stderr, "jet_l12_aes: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
