/// @file fuzz_jet_l5_float.c
///
/// Differential jet fuzzer for IEEE-754 float jets.
/// Covers rd (double / 64-bit), rs (single / 32-bit), rh (half / 16-bit)
/// arithmetic: add, sub, mul, div, plus comparison (lth, equ).
/// rq (quad / 128-bit) ops left out — less C-jet coverage and need
/// 128-bit atom handling.
///
/// Every 32/64-bit bit pattern is a valid IEEE-754 input, including
/// NaN, ±inf, denormals, and signed zeros. These are rich fuzz
/// targets because float semantics have corners the jet and Hoon
/// could diverge on (rounding mode, NaN propagation, denormal flush).
///
/// Build:  fuzz/build.sh fuzz_jet_l5_float
/// Corpus: fuzz/corpus/fuzz_jet_l5_float/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

typedef enum {
  S_FLOAT64_PAIR,     /* (a=@rd b=@rd) — 8-byte each        */
  S_FLOAT32_PAIR,     /* (a=@rs b=@rs) — 4-byte each        */
  S_FLOAT16_PAIR,     /* (a=@rh b=@rh) — 2-byte each        */
} shape_e;

typedef struct {
  const char* nam_c;   /* wish name, e.g. "add:rd"          */
  const char* cor_c;   /* core name for ice flip, e.g. "add"*/
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "add:rd", "add", S_FLOAT64_PAIR, 0 },
  { "sub:rd", "sub", S_FLOAT64_PAIR, 0 },
  { "mul:rd", "mul", S_FLOAT64_PAIR, 0 },
  { "div:rd", "div", S_FLOAT64_PAIR, 0 },
  { "lth:rd", "lth", S_FLOAT64_PAIR, 0 },
  { "equ:rd", "equ", S_FLOAT64_PAIR, 0 },
  { "add:rs", "add", S_FLOAT32_PAIR, 0 },
  { "sub:rs", "sub", S_FLOAT32_PAIR, 0 },
  { "mul:rs", "mul", S_FLOAT32_PAIR, 0 },
  { "div:rs", "div", S_FLOAT32_PAIR, 0 },
  { "lth:rs", "lth", S_FLOAT32_PAIR, 0 },
  { "equ:rs", "equ", S_FLOAT32_PAIR, 0 },
  { "add:rh", "add", S_FLOAT16_PAIR, 0 },
  { "sub:rh", "sub", S_FLOAT16_PAIR, 0 },
  { "mul:rh", "mul", S_FLOAT16_PAIR, 0 },
  { "div:rh", "div", S_FLOAT16_PAIR, 0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
  u3_cue_xeno* sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( !sil ) return 1;
  u3_weak pil = u3s_cue_xeno_with(sil, u3_Ivory_pill_len, u3_Ivory_pill);
  if ( pil == u3_none ) return 1;
  u3s_cue_xeno_done(sil);
  if ( c3n == u3v_boot_lite(pil) ) return 1;

  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) {
      fprintf(stderr, "l5: u3v_wish('%s') failed\n", _jets[i].nam_c);
      _jets[i].gate = 0;
      continue;
    }
    _jets[i].gate = gat;
  }
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].cor_c);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 3 || len > H_MAX_INPUT ) return 0;

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  const uint8_t* p = buf + 1;
  c3_w rem = len - 1;
  u3_noun sam;

  switch ( jet->shp_e ) {
    default:
    case S_FLOAT64_PAIR: {
      /* Need 16 bytes — 8 for a, 8 for b. Pad with zero if short. */
      uint8_t ab[16] = {0};
      c3_w take = rem > 16 ? 16 : rem;
      memcpy(ab, p, take);
      u3_atom a = u3i_bytes(8, ab);
      u3_atom b = u3i_bytes(8, ab + 8);
      sam = u3nc(a, b);
      break;
    }
    case S_FLOAT32_PAIR: {
      uint8_t ab[8] = {0};
      c3_w take = rem > 8 ? 8 : rem;
      memcpy(ab, p, take);
      u3_atom a = u3i_bytes(4, ab);
      u3_atom b = u3i_bytes(4, ab + 4);
      sam = u3nc(a, b);
      break;
    }
    case S_FLOAT16_PAIR: {
      uint8_t ab[4] = {0};
      c3_w take = rem > 4 ? 4 : rem;
      memcpy(ab, p, take);
      u3_atom a = u3i_bytes(2, ab);
      u3_atom b = u3i_bytes(2, ab + 2);
      sam = u3nc(a, b);
      break;
    }
  }

  g_gate = jet->gate;
  g_sam  = sam;
  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l5_float: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
