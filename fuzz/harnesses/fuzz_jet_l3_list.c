/// @file fuzz_jet_l3_list.c
///
/// Differential jet fuzzer for layer-3 list jets.
/// Covers: `flop`, `lent`, `snag`, `slag`, `scag`, `reap`, `welp`.
///
/// Lists are cons-shaped nouns `[h [h [h ... 0]]]`. We build them
/// from fuzz bytes: each byte becomes a list element. List length
/// is clamped to 32 — Hoon list ops are O(n) and double over with
/// the oracle running Hoon side, so 32 keeps exec time bounded.
///
/// Build:  fuzz/build.sh fuzz_jet_l3_list
/// Corpus: fuzz/corpus/fuzz_jet_l3_list/

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
#define H_MAX_LIST  32

typedef enum {
  S_LIST,           /* single list               */
  S_IDX_LIST,       /* (@ud list)                */
  S_IDX_ANY,        /* (@ud @) — reap            */
  S_LIST_LIST,      /* (list list) — welp        */
} shape_e;

typedef struct {
  const char* nam_c;
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "flop", S_LIST,      0 },
  { "lent", S_LIST,      0 },
  { "snag", S_IDX_LIST,  0 },
  { "slag", S_IDX_LIST,  0 },
  { "scag", S_IDX_LIST,  0 },
  { "reap", S_IDX_ANY,   0 },
  { "welp", S_LIST_LIST, 0 },
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

/* Build a Hoon list [a0 [a1 [... [an 0]]]] from `n` bytes. */
static u3_noun
_build_list(const uint8_t* p, c3_w n)
{
  u3_noun lis = u3_nul;
  for ( c3_w i = n; i > 0; i-- ) {
    lis = u3nc((u3_atom)p[i - 1], lis);
  }
  return lis;
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( !sil_u ) { fprintf(stderr, "l3: cue init\n"); return 1; }
  u3_weak pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len, u3_Ivory_pill);
  if ( pil == u3_none ) { fprintf(stderr, "l3: pill cue\n"); return 1; }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) { fprintf(stderr, "l3: boot\n"); return 1; }

  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) {
      fprintf(stderr, "l3: u3v_wish('%s') failed — skipping\n", _jets[i].nam_c);
      _jets[i].gate = 0;
      continue;
    }
    _jets[i].gate = gat;
  }
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( 0 == _jets[i].gate ) continue;
    (void)u3j_fuzz_arm(_jets[i].nam_c);
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 3 || len > H_MAX_INPUT ) return 0;

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  c3_y idx_y = buf[1] & 0x1f;    /* 0..31 */
  c3_w  n_w  = (len - 2) > H_MAX_LIST ? H_MAX_LIST : (len - 2);
  const uint8_t* p = buf + 2;

  u3_noun sam;
  switch ( jet->shp_e ) {
    default:
    case S_LIST: {
      sam = _build_list(p, n_w);
      break;
    }
    case S_IDX_LIST: {
      u3_noun lis = _build_list(p, n_w);
      sam = u3nc((u3_atom)idx_y, lis);
      break;
    }
    case S_IDX_ANY: {
      /* reap is `|=  [a=@ b=*]  list` — clamp index tight */
      c3_y idx2 = idx_y & 0x07;   /* up to 7 repeats */
      sam = u3nc((u3_atom)idx2, (u3_atom)p[0]);
      break;
    }
    case S_LIST_LIST: {
      c3_w half = n_w / 2;
      u3_noun la = _build_list(p, half);
      u3_noun lb = _build_list(p + half, n_w - half);
      sam = u3nc(la, lb);
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
      fprintf(stderr, "jet_l3_list: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
