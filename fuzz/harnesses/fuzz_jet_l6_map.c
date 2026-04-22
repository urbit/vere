/// @file fuzz_jet_l6_map.c
///
/// Differential jet fuzzer for maps (`by`) and sets (`in`).
/// These take STRUCTURED tree nouns — the Hoon `+by` / `+in` trees
/// are `(tree (pair key val))` and `(tree *)` respectively, with
/// an ordering invariant on keys.
///
/// We fuzz two distinct patterns:
///
/// 1. **Validator fuzz** (`apt:by`, `apt:in`): these are the tree
///    invariant checkers. They accept ANY tree shape and return a
///    loobean. Random bytes trivially produce random trees, so this
///    is dense coverage.
///
/// 2. **Op fuzz** (`put:by` / `get:by` / `has:by` / `del:by` and
///    in-set equivalents): we build a valid starting tree by
///    inserting a handful of (key, val) pairs, then fuzz one more
///    operation against it.
///
/// Build:  fuzz/build.sh fuzz_jet_l6_map
/// Corpus: fuzz/corpus/fuzz_jet_l6_map/

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
  S_APT_BY,         /* validator on by-tree — random tree input */
  S_APT_IN,         /* validator on in-tree — random tree input */
  S_BY_GET,         /* (key tree) — lookup                      */
  S_BY_HAS,         /* (key tree)                               */
  S_BY_PUT,         /* ([key val] tree) — insert                */
  S_BY_DEL,         /* (key tree)                               */
  S_IN_HAS,         /* (key tree)                               */
  S_IN_PUT,         /* (key tree)                               */
  S_IN_DEL,         /* (key tree)                               */
  S_IN_TAP,         /* (tree) — enumerate                       */
  S_IN_WYT,         /* (tree) — size                            */
} shape_e;

typedef struct {
  const char* nam_c;
  const char* cor_c;
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "apt:by", "apt", S_APT_BY, 0 },
  { "apt:in", "apt", S_APT_IN, 0 },
  { "get:by", "get", S_BY_GET, 0 },
  { "has:by", "has", S_BY_HAS, 0 },
  { "put:by", "put", S_BY_PUT, 0 },
  { "del:by", "del", S_BY_DEL, 0 },
  { "has:in", "has", S_IN_HAS, 0 },
  { "put:in", "put", S_IN_PUT, 0 },
  { "del:in", "del", S_IN_DEL, 0 },
  { "tap:in", "tap", S_IN_TAP, 0 },
  { "wyt:in", "wyt", S_IN_WYT, 0 },
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

/* Build a random tree of depth ≤ 3 from `n` bytes. Each cell node
 * is [value left right] per Hoon convention. Leaf is ~ (u3_nul=0).
 * For `in`, tree nodes are [key left right]. For `by`, they are
 * [[key val] left right]. */
static u3_noun
_build_in_tree(const uint8_t* p, c3_w* idx_p, c3_w n, c3_w depth)
{
  if ( depth == 0 || *idx_p >= n ) return u3_nul;
  uint8_t tag = p[*idx_p]; (*idx_p)++;
  if ( (tag & 0x1) == 0 ) return u3_nul;
  uint8_t key = (*idx_p < n) ? p[*idx_p] : 0; (*idx_p)++;
  u3_noun l = _build_in_tree(p, idx_p, n, depth - 1);
  u3_noun r = _build_in_tree(p, idx_p, n, depth - 1);
  return u3nt((u3_atom)key, l, r);
}

static u3_noun
_build_by_tree(const uint8_t* p, c3_w* idx_p, c3_w n, c3_w depth)
{
  if ( depth == 0 || *idx_p >= n ) return u3_nul;
  uint8_t tag = p[*idx_p]; (*idx_p)++;
  if ( (tag & 0x1) == 0 ) return u3_nul;
  uint8_t key = (*idx_p < n) ? p[*idx_p] : 0; (*idx_p)++;
  uint8_t val = (*idx_p < n) ? p[*idx_p] : 0; (*idx_p)++;
  u3_noun kv = u3nc((u3_atom)key, (u3_atom)val);
  u3_noun l = _build_by_tree(p, idx_p, n, depth - 1);
  u3_noun r = _build_by_tree(p, idx_p, n, depth - 1);
  return u3nt(kv, l, r);
}

/* Build a valid (well-ordered) by-tree via repeated put:by calls. */
static u3_noun
_build_valid_by(u3_noun put_gate, const uint8_t* p, c3_w n)
{
  u3_noun tree = u3_nul;
  c3_w steps = n > 8 ? 8 : n;
  for ( c3_w i = 0; i < steps; i++ ) {
    u3_atom k = (u3_atom)p[i];
    u3_atom v = (u3_atom)(p[i] ^ 0x5A);
    u3_noun sam = u3nc(u3k(tree), u3nc(k, v));
    u3z(tree);
    tree = u3n_slam_on(u3k(put_gate), sam);
  }
  return tree;
}

static u3_noun
_build_valid_in(u3_noun put_gate, const uint8_t* p, c3_w n)
{
  u3_noun set = u3_nul;
  c3_w steps = n > 8 ? 8 : n;
  for ( c3_w i = 0; i < steps; i++ ) {
    u3_atom k = (u3_atom)p[i];
    u3_noun sam = u3nc(u3k(set), k);
    u3z(set);
    set = u3n_slam_on(u3k(put_gate), sam);
  }
  return set;
}

/* Cached "put" gates for valid-tree construction. */
static u3_noun g_put_by = 0;
static u3_noun g_put_in = 0;

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
      fprintf(stderr, "l6: u3v_wish('%s') failed\n", _jets[i].nam_c);
      _jets[i].gate = 0;
      continue;
    }
    _jets[i].gate = gat;
  }
  g_put_by = u3v_wish("put:by");
  g_put_in = u3v_wish("put:in");

  /* Flip ice AFTER wishing. */
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
    case S_APT_BY: {
      c3_w i = 0;
      sam = _build_by_tree(p, &i, rem, 3);
      break;
    }
    case S_APT_IN: {
      c3_w i = 0;
      sam = _build_in_tree(p, &i, rem, 3);
      break;
    }
    case S_BY_GET:
    case S_BY_HAS:
    case S_BY_DEL: {
      /* (tree key) — put tree at sam_2, key at sam_3 */
      uint8_t key = p[0];
      u3_noun tree = _build_valid_by(g_put_by, p + 1, rem - 1);
      sam = u3nc(tree, (u3_atom)key);
      break;
    }
    case S_BY_PUT: {
      /* (tree [key val]) */
      uint8_t key = p[0];
      uint8_t val = rem > 1 ? p[1] : 0;
      u3_noun tree = _build_valid_by(g_put_by, p + 2, rem > 2 ? rem - 2 : 0);
      sam = u3nc(tree, u3nc((u3_atom)key, (u3_atom)val));
      break;
    }
    case S_IN_HAS:
    case S_IN_DEL:
    case S_IN_PUT: {
      uint8_t key = p[0];
      u3_noun set = _build_valid_in(g_put_in, p + 1, rem - 1);
      sam = u3nc(set, (u3_atom)key);
      break;
    }
    case S_IN_TAP:
    case S_IN_WYT: {
      u3_noun set = _build_valid_in(g_put_in, p, rem);
      sam = set;
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
      fprintf(stderr, "jet_l6_map: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
