/// @file fuzz_jet_l2_bits.c
///
/// Differential fuzz harness for Hoon stdlib layer-2 bit/byte jets.
/// These jets build on layer-1 math; we assume layer-1 is verified.
///
/// Layer-2 covered here: `mix`, `dis`, `con`, `mug`, `end`, `lsh`,
/// `rsh`, `cut`, `rep`, `rip`, `rev`, `swp`, `met`, `cat`, `dor`,
/// `gor`, `mor`. (Skipping `can` and `rap` in v1 because their
/// sample is a list — needs valid-shaped seeds.)
///
/// Clamps are tighter here than layer 1 because the Hoon side of
/// `lsh`/`rsh`/`cut`/`rip`/`rep` involves dec chains over the bit
/// count. We cap bloq to 0..4 (1..16 bits per block) and step/len
/// to ≤ 256 so total work per iter stays under a few hundred
/// nock steps.
///
/// Build:  fuzz/build.sh fuzz_jet_l2_bits   (vere-flavor)
/// Corpus: fuzz/corpus/fuzz_jet_l2_bits/

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
  S_ATOM1,          /* one atom                     */
  S_ATOM2,          /* cell [a b]                   */
  S_BLOQ_ATOM,      /* cell [bloq atom]             */
  S_BLOQ_STEP_ATOM, /* cell [[bloq step] atom]      */
  S_CUT_ARGS,       /* [bloq [start len] atom]      */
} shape_e;

typedef struct {
  const char* nam_c;
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "mix", S_ATOM2,          0 },
  { "dis", S_ATOM2,          0 },
  { "con", S_ATOM2,          0 },
  { "mug", S_ATOM1,          0 },
  { "end", S_BLOQ_STEP_ATOM, 0 },
  { "lsh", S_BLOQ_STEP_ATOM, 0 },
  { "rsh", S_BLOQ_STEP_ATOM, 0 },
  { "cut", S_CUT_ARGS,       0 },
  { "rep", S_BLOQ_ATOM,      0 },  /* [bloq list] — we'll fake a 1-elt list */
  { "rip", S_BLOQ_ATOM,      0 },
  { "rev", S_BLOQ_STEP_ATOM, 0 },
  { "swp", S_BLOQ_ATOM,      0 },
  { "met", S_BLOQ_ATOM,      0 },
  { "cat", S_BLOQ_STEP_ATOM, 0 },  /* [bloq a b] */
  { "dor", S_ATOM2,          0 },
  { "gor", S_ATOM2,          0 },
  { "mor", S_ATOM2,          0 },
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

/* Clamp atom size per layer-2 requirements. Most input atoms need to
 * stay small so (a) dec chains in lsh/rsh don't explode and (b) cat
 * output size stays within the loom. 4 bytes = 32-bit max. */
static u3_atom
_atom4(const uint8_t* p, c3_w n_w)
{
  c3_w v = 0;
  c3_w i;
  c3_w take = (n_w > 4) ? 4 : n_w;
  for ( i = 0; i < take; i++ ) {
    v |= ((c3_w)p[i]) << (i * 8);
  }
  return u3i_word(v);
}

static u3_atom
_atom2(const uint8_t* p, c3_w n_w)
{
  c3_w v = 0;
  c3_w i;
  c3_w take = (n_w > 2) ? 2 : n_w;
  for ( i = 0; i < take; i++ ) {
    v |= ((c3_w)p[i]) << (i * 8);
  }
  return u3i_word(v);
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( sil_u == NULL ) { fprintf(stderr, "l2: cue init\n"); return 1; }
  u3_weak pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len, u3_Ivory_pill);
  if ( pil == u3_none ) { fprintf(stderr, "l2: pill cue\n"); return 1; }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) { fprintf(stderr, "l2: boot\n"); return 1; }

  /* Wish every gate BEFORE flipping ice. */
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) {
      fprintf(stderr, "l2: u3v_wish('%s') failed\n", _jets[i].nam_c);
      return 1;
    }
    _jets[i].gate = gat;
  }

  /* Flip ice on layer-2 jets. Layer-1 jets stay ice=c3y (trusted),
   * so the short-circuit in _cj_kick_z's inner calls keeps exec/s
   * reasonable. */
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    if ( c3n == u3j_fuzz_arm(_jets[i].nam_c) ) {
      fprintf(stderr, "l2: failed to flip ice for '%s'\n", _jets[i].nam_c);
      return 1;
    }
  }

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if ( len < 3 || len > H_MAX_INPUT ) {
    return 0;
  }

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];

  /* Clamps: bloq ∈ 0..4, step/len ∈ 0..255. Tight to bound Hoon cost. */
  c3_y bloq_y = buf[1] & 0x7;      /* up to 2^7 = 128 bits; mild */
  if ( bloq_y > 4 ) bloq_y = 4;

  u3_noun sam;
  int off = 2;
  int rem = len - off;

  switch ( jet->shp_e ) {
    default:
    case S_ATOM1: {
      u3_atom a = _atom4(buf + off, rem);
      sam = a;
      break;
    }
    case S_ATOM2: {
      u3_atom a = _atom4(buf + off, (rem < 4) ? rem : 4);
      off += 4; rem = len - off;
      u3_atom b = _atom4(buf + off, rem);
      sam = u3nc(a, b);
      break;
    }
    case S_BLOQ_ATOM: {
      u3_atom a = _atom4(buf + off, rem);
      sam = u3nc((u3_atom)bloq_y, a);
      break;
    }
    case S_BLOQ_STEP_ATOM: {
      c3_y step = buf[off] & 0xff;
      off++; rem = len - off;
      u3_atom step_a = (u3_atom)step;
      u3_atom dat    = _atom4(buf + off, rem);
      sam = u3nc(u3nc((u3_atom)bloq_y, step_a), dat);
      break;
    }
    case S_CUT_ARGS: {
      c3_y start = buf[off] & 0xff;
      off++;
      c3_y lenn  = buf[off] & 0xff;
      off++; rem = len - off;
      u3_atom dat = _atom4(buf + off, rem);
      sam = u3nc((u3_atom)bloq_y,
                 u3nc(u3nc((u3_atom)start, (u3_atom)lenn),
                      dat));
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
      fprintf(stderr, "jet_l2_bits: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
