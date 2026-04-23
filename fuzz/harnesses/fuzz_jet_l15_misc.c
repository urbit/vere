/// @file fuzz_jet_l15_misc.c
///
/// Combined differential ice-oracle fuzzer covering 17 smaller-footprint
/// jets from across the base desk:
///   chacha:       crypt:chacha:crypto, xchacha:chacha:crypto
///   basic math:   bex, xeb, sqt, pow        (single-core, L1 coverage)
///   quad floats:  add/sub/mul/div/lth/equ:rq
///   misc:         og (raw), mug, muk
///   json:         en:json:html, de:json:html
///
/// Uses the same boot-from-snapshot + u3v_wish + u3j_fuzz_arm pattern as
/// fuzz_jet_l11_base.c. Expects a pre-booted brass-pill pier at
/// /tmp/fuzz-pier-zod-v44 so zuse-scope names resolve.
///
/// All shape builders apply aggressive clamps to keep Hoon-side iteration
/// manageable; wrong-shape inputs Hoon-bail (harness ignores) and only
/// c3__fail && u3j_Fuzz_mismatch==c3y aborts.
///
/// Build:  fuzz/build.sh fuzz_jet_l15_misc

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 512
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
  S_ATOM_SMALL,       /* clamped single atom (bex/xeb/sqt)           */
  S_POW_PAIR,         /* [a=@ b=@] both ≤ 8                          */
  S_RQ_PAIR,          /* [a=@rq b=@rq]  each 16 bytes                */
  S_CHACHA,           /* [rounds key nonce [counter msg=octs]]       */
  S_XCHACHA,          /* [rounds=@ud key=@uxI nonce=@ux]             */
  S_ATOM,             /* raw atom (og raw, mug, de:json, xeb)        */
  S_MUK,              /* [syd=@ len=@ key=@]                         */
  S_JSON_EN,          /* simple json noun tree                       */
} shape_e;

typedef struct {
  const char* nam_c;   /* wish path                                  */
  const char* cor_c;   /* core name for ice flip                     */
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  /* chacha */
  { "crypt:chacha:crypto",      "crypt",    S_CHACHA,     0 },
  { "xchacha:chacha:crypto",    "xchacha",  S_XCHACHA,    0 },
  /* basic math */
  { "bex",                      "bex",      S_ATOM_SMALL, 0 },
  { "xeb",                      "xeb",      S_ATOM_SMALL, 0 },
  { "sqt",                      "sqt",      S_ATOM_SMALL, 0 },
  { "pow",                      "pow",      S_POW_PAIR,   0 },
  /* quad-precision floats — flip by arm name (each is registered as
     a sub-core with axe .2 under rq); this also flips rd/rs/rh arms
     of the same name, which is harmless */
  { "add:rq",                   "add",      S_RQ_PAIR,    0 },
  { "sub:rq",                   "sub",      S_RQ_PAIR,    0 },
  { "mul:rq",                   "mul",      S_RQ_PAIR,    0 },
  { "div:rq",                   "div",      S_RQ_PAIR,    0 },
  { "lth:rq",                   "lth",      S_RQ_PAIR,    0 },
  { "equ:rq",                   "equ",      S_RQ_PAIR,    0 },
  /* misc */
  { "raw:og",                   "raw",      S_ATOM_SMALL, 0 },
  { "mug",                      "mug",      S_ATOM,       0 },
  { "muk",                      "muk",      S_MUK,        0 },
  /* json */
  { "en:json:html",             "en",       S_JSON_EN,    0 },
  { "de:json:html",             "de",       S_ATOM,       0 },
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
  if ( n > 256 ) n = 256;

  switch ( shp_e ) {
    default:
    case S_ATOM: {
      return u3i_bytes(n, p);
    }

    case S_ATOM_SMALL: {
      /* clamp to ≤ 2^20 so bex/xeb/sqt/og-raw don't explode */
      c3_w a = 0;
      if ( n >= 1 ) a |= (c3_w)p[0];
      if ( n >= 2 ) a |= ((c3_w)p[1]) << 8;
      if ( n >= 3 ) a |= ((c3_w)(p[2] & 0x0f)) << 16;
      return u3i_word(a);
    }

    case S_POW_PAIR: {
      /* [a=@ b=@], both ≤ 8 */
      c3_w a = (n >= 1) ? (p[0] & 7) : 0;
      c3_w b = (n >= 2) ? (p[1] & 7) : 0;
      return u3nc(u3i_word(a), u3i_word(b));
    }

    case S_RQ_PAIR: {
      /* two 128-bit atoms */
      uint8_t ab[32] = {0};
      c3_w take = (n > 32) ? 32 : n;
      memcpy(ab, p, take);
      u3_atom a = u3i_bytes(16, ab);
      u3_atom b = u3i_bytes(16, ab + 16);
      return u3nc(a, b);
    }

    case S_CHACHA: {
      /* [rounds=@ud key=@uxI nonce=@uxG counter=@udG msg=octs] */
      uint8_t buf[46] = {0};
      c3_w take = (n > 46) ? 46 : n;
      memcpy(buf, p, take);
      /* even rounds 0..30 */
      u3_noun rnd = u3i_word(((buf[0] & 0x1f) / 2) * 2);
      u3_noun key = u3i_bytes(32, buf + 1);
      u3_noun non = u3i_bytes(12, buf + 33);
      u3_noun ctr = u3i_word(buf[45]);
      c3_w msg_n = (n > 46) ? (n - 46) : 0;
      if ( msg_n > 128 ) msg_n = 128;
      u3_noun msg = u3nc(u3i_word(msg_n),
                         u3i_bytes(msg_n, (n > 46) ? (p + 46) : buf));
      /* shape: [rnd key non ctr msg] as nested right-associated tuple */
      return u3nq(rnd, key, non, u3nc(ctr, msg));
    }

    case S_XCHACHA: {
      /* [rounds=@ud key=@uxI nonce=@ux] — nonce is 24 bytes for xchacha */
      uint8_t buf[1 + 32 + 24] = {0};
      c3_w take = (n > sizeof(buf)) ? sizeof(buf) : n;
      memcpy(buf, p, take);
      u3_noun rnd = u3i_word(((buf[0] & 0x1f) / 2) * 2);
      u3_noun key = u3i_bytes(32, buf + 1);
      u3_noun non = u3i_bytes(24, buf + 33);
      return u3nt(rnd, key, non);
    }

    case S_MUK: {
      /* [syd=@ len=@ key=@], clamp len ≤ 64 */
      c3_w syd = (n >= 1) ? p[0] : 0;
      c3_w ln  = (n >= 2) ? (p[1] & 0x3f) : 0;
      c3_w rem = (n > 2) ? (n - 2) : 0;
      if ( rem > 128 ) rem = 128;
      u3_atom key = u3i_bytes(rem, (n > 2) ? (p + 2) : p);
      return u3nt(u3i_word(syd), u3i_word(ln), key);
    }

    case S_JSON_EN: {
      /* Pass a simple json tagged-union: [%s 'abc...'] */
      c3_w take = (n > 32) ? 32 : n;
      u3_atom txt = u3i_bytes(take, p);
      return u3nc(c3__s, txt);
    }
  }
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;

  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "l15: pier at %s is empty — boot it first\n", PIER_DIR);
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
  fprintf(stderr, "l15: restored from %s at eve %llu\n",
          PIER_DIR, (unsigned long long)u3A->eve_d);

  /* Wish each target and cache the gate. */
  c3_w ok = 0;
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    g_wish_name = _jets[i].nam_c;
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    if ( 0 != u3h(pro) || u3_none == u3t(pro) ) {
      fprintf(stderr, "l15: skip '%s'\n", _jets[i].nam_c);
      _jets[i].gate = 0;
    } else {
      _jets[i].gate = u3k(u3t(pro));
      ok++;
    }
    u3z(pro);
  }
  fprintf(stderr, "l15: %u/%u jets cached\n", ok, _jets_n);

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
      fprintf(stderr, "jet_l15_misc: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
