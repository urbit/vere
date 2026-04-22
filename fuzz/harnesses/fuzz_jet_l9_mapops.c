/// @file fuzz_jet_l9_mapops.c
///
/// Differential fuzzer for map/set *batch* operations: `gas`, `uni`,
/// `dif`, `int`, `all`, `any`. L6 covered the single-item ops
/// (get/put/has/del/apt/tap/wyt); this harness targets the set-algebra
/// and reductions.
///
/// Build:  fuzz/build.sh fuzz_jet_l9_mapops
/// Corpus: fuzz/corpus/fuzz_jet_l9_mapops/

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
  S_BY_BY,        /* (by-tree by-tree) — uni/dif/int                  */
  S_BY_LIST,      /* (by-tree list-of-[key val]) — gas                */
  S_IN_IN,        /* (in-tree in-tree)                                */
  S_IN_LIST,      /* (in-tree list-of-key) — gas:in                   */
} shape_e;

typedef struct {
  const char* nam_c;
  const char* cor_c;
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "uni:by", "uni", S_BY_BY,   0 },
  { "dif:by", "dif", S_BY_BY,   0 },
  { "int:by", "int", S_BY_BY,   0 },
  { "gas:by", "gas", S_BY_LIST, 0 },
  { "uni:in", "uni", S_IN_IN,   0 },
  { "dif:in", "dif", S_IN_IN,   0 },
  { "int:in", "int", S_IN_IN,   0 },
  { "gas:in", "gas", S_IN_LIST, 0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

static u3_noun g_put_by = 0;
static u3_noun g_put_in = 0;
static u3_noun g_gate   = 0;
static u3_noun g_sam    = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

/* Build a valid by-tree from `n` bytes (2 bytes per entry: k, v). */
static u3_noun
_build_by(const uint8_t* p, c3_w n)
{
  u3_noun tree = u3_nul;
  c3_w pairs = (n / 2 > 8) ? 8 : n / 2;
  for ( c3_w i = 0; i < pairs; i++ ) {
    u3_atom k = (u3_atom)p[i*2];
    u3_atom v = (u3_atom)p[i*2 + 1];
    u3_noun sam = u3nc(u3k(tree), u3nc(k, v));
    u3z(tree);
    tree = u3n_slam_on(u3k(g_put_by), sam);
  }
  return tree;
}

/* Build a valid in-tree from `n` bytes (1 byte per entry). */
static u3_noun
_build_in(const uint8_t* p, c3_w n)
{
  u3_noun set = u3_nul;
  c3_w steps = n > 8 ? 8 : n;
  for ( c3_w i = 0; i < steps; i++ ) {
    u3_atom k = (u3_atom)p[i];
    u3_noun sam = u3nc(u3k(set), k);
    u3z(set);
    set = u3n_slam_on(u3k(g_put_in), sam);
  }
  return set;
}

/* Build a list of [k v] cells from bytes. */
static u3_noun
_build_kv_list(const uint8_t* p, c3_w n)
{
  c3_w pairs = (n / 2 > 4) ? 4 : n / 2;
  u3_noun lis = u3_nul;
  for ( c3_w i = pairs; i > 0; i-- ) {
    u3_atom k = (u3_atom)p[(i-1)*2];
    u3_atom v = (u3_atom)p[(i-1)*2 + 1];
    lis = u3nc(u3nc(k, v), lis);
  }
  return lis;
}

/* Build a flat list-of-atom from bytes. */
static u3_noun
_build_atom_list(const uint8_t* p, c3_w n)
{
  c3_w take = n > 4 ? 4 : n;
  u3_noun lis = u3_nul;
  for ( c3_w i = take; i > 0; i-- ) {
    lis = u3nc((u3_atom)p[i-1], lis);
  }
  return lis;
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
    u3_noun g = u3v_wish(_jets[i].nam_c);
    _jets[i].gate = (u3_none == g) ? 0 : g;
  }
  g_put_by = u3v_wish("put:by");
  g_put_in = u3v_wish("put:in");

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
  c3_w half = rem / 2;
  u3_noun sam;

  switch ( jet->shp_e ) {
    default:
    case S_BY_BY: {
      u3_noun a = _build_by(p, half);
      u3_noun b = _build_by(p + half, rem - half);
      sam = u3nc(a, b);
      break;
    }
    case S_BY_LIST: {
      u3_noun a = _build_by(p, half);
      u3_noun lis = _build_kv_list(p + half, rem - half);
      sam = u3nc(a, lis);
      break;
    }
    case S_IN_IN: {
      u3_noun a = _build_in(p, half);
      u3_noun b = _build_in(p + half, rem - half);
      sam = u3nc(a, b);
      break;
    }
    case S_IN_LIST: {
      u3_noun a = _build_in(p, half);
      u3_noun lis = _build_atom_list(p + half, rem - half);
      sam = u3nc(a, lis);
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
      fprintf(stderr, "jet_l9_mapops: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
