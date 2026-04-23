/// @file fuzz_jet_l17_lists.c
///
/// Ice-oracle fuzzer for 10 list-traversal jet arms on the base desk:
///   turn, roll, reel, skim, skip, sort, levy, lien, zing, weld.
///
/// Each arm takes a list (or list-of-lists / two lists) plus — for the
/// traversal arms — a gate. Gates are wished once at boot and cached.
///
/// Note: u3j_Fuzz_testing in _cj_kick_z suppresses nested oracles, so
/// fuzzing e.g. `turn` only oracle-checks turn itself — inner gate
/// invocations take the fast jet-only path.
///
/// Requires a pre-booted brass pier at PIER_DIR (same pier used by
/// l11/l12/l13 harnesses).
///
/// Build:  fuzz/build.sh fuzz_jet_l17_lists   (vere-flavor)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 128
#define H_MAX_LIST  32
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
  S_LIST_GATE,        /* [list gate]          — turn/roll/reel/skim/skip/sort/levy/lien */
  S_LIST_OF_LISTS,    /* (list (list *))      — zing                                    */
  S_LIST_LIST,        /* [list-a list-b]      — weld                                    */
} shape_e;

typedef enum {
  G_NONE,
  G_INC,      /* |=(a=@ (add a 1))         — turn                   */
  G_ADD,      /* |=([a=@ b=@] (add a b))   — roll/reel              */
  G_EVEN,     /* |=(a=@ =(0 (mod a 2)))    — skim/skip/levy/lien    */
  G_LTH,      /* lth                       — sort                   */
} gate_e;

typedef struct {
  const char* nam_c;   /* wish path          */
  const char* cor_c;   /* core name for ice flip */
  shape_e     shp_e;
  gate_e      gat_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  { "turn", "turn", S_LIST_GATE,     G_INC,  0 },
  { "roll", "roll", S_LIST_GATE,     G_ADD,  0 },
  { "reel", "reel", S_LIST_GATE,     G_ADD,  0 },
  { "skim", "skim", S_LIST_GATE,     G_EVEN, 0 },
  { "skip", "skip", S_LIST_GATE,     G_EVEN, 0 },
  { "sort", "sort", S_LIST_GATE,     G_LTH,  0 },
  { "levy", "levy", S_LIST_GATE,     G_EVEN, 0 },
  { "lien", "lien", S_LIST_GATE,     G_EVEN, 0 },
  { "zing", "zing", S_LIST_OF_LISTS, G_NONE, 0 },
  { "weld", "weld", S_LIST_LIST,     G_NONE, 0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

/* Cached per-shape helper gates. */
static u3_noun g_inc  = 0;
static u3_noun g_add  = 0;
static u3_noun g_even = 0;
static u3_noun g_lth  = 0;

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

/* Build a Hoon list [a0 [a1 [... [an 0]]]] from `n` bytes, each byte
** as a @ud atom. */
static u3_noun
_build_list(const uint8_t* p, c3_w n)
{
  if ( n > H_MAX_LIST ) n = H_MAX_LIST;
  u3_noun lis = u3_nul;
  for ( c3_w i = n; i > 0; i-- ) {
    lis = u3nc(u3i_word((c3_w)p[i - 1]), lis);
  }
  return lis;
}

/* Build a list-of-lists from `n` bytes. Split into roughly sqrt(n)
** sublists, each carrying an equal chunk. Keeps total bytes small. */
static u3_noun
_build_list_of_lists(const uint8_t* p, c3_w n)
{
  if ( n > H_MAX_LIST ) n = H_MAX_LIST;
  if ( n == 0 ) return u3_nul;

  /* 1 sublist per 4 bytes, capped by the list limit. */
  c3_w sublists = (n + 3) / 4;
  if ( sublists > 8 ) sublists = 8;
  c3_w per = n / sublists;
  if ( per == 0 ) per = 1;

  u3_noun outer = u3_nul;
  c3_w consumed = 0;
  /* Build outer list in reverse so final order matches p. */
  for ( c3_w i = sublists; i > 0; i-- ) {
    c3_w take;
    if ( i == 1 ) {
      /* last (front) sublist picks up the head slice */
      take = per;
    } else {
      take = per;
    }
    (void)take;
  }

  /* Simpler: build sublists left-to-right into an array, then cons. */
  u3_noun subs[16];
  c3_w    sub_n = 0;
  c3_w    off   = 0;
  for ( c3_w i = 0; i < sublists && sub_n < 16; i++ ) {
    c3_w take = per;
    if ( i == sublists - 1 ) take = n - off;
    if ( off + take > n ) take = n - off;
    subs[sub_n++] = _build_list(p + off, take);
    off += take;
    if ( off >= n ) break;
  }
  for ( c3_w i = sub_n; i > 0; i-- ) {
    outer = u3nc(subs[i - 1], outer);
  }
  return outer;
}

static u3_noun
_gate_for(gate_e ge)
{
  switch ( ge ) {
    case G_INC:  return g_inc;
    case G_ADD:  return g_add;
    case G_EVEN: return g_even;
    case G_LTH:  return g_lth;
    default:     return 0;
  }
}

/* Wish a gate, retained on success, stderr-logged on failure. */
static u3_noun
_wish_gate(const char* src_c)
{
  g_wish_name = src_c;
  u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
  u3_noun out = 0;
  if ( 0 == u3h(pro) && u3_none != u3t(pro) ) {
    out = u3k(u3t(pro));
  } else {
    fprintf(stderr, "l17: wish('%s') failed\n", src_c);
  }
  u3z(pro);
  return out;
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
    fprintf(stderr, "l17: pier at %s is empty — boot it first\n", PIER_DIR);
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
  fprintf(stderr, "l17: restored from %s at eve %llu\n",
          PIER_DIR, (unsigned long long)u3A->eve_d);

  /* Cache helper gates. */
  g_inc  = _wish_gate("|=(a=@ (add a 1))");
  g_add  = _wish_gate("|=([a=@ b=@] (add a b))");
  g_even = _wish_gate("|=(a=@ =(0 (mod a 2)))");
  g_lth  = _wish_gate("lth");

  /* Wish each target and cache the gate. */
  c3_w ok = 0;
  for ( c3_w i = 0; i < _jets_n; i++ ) {
    g_wish_name = _jets[i].nam_c;
    u3_noun pro = u3m_soft(0, _wish_soft, u3_nul);
    if ( 0 != u3h(pro) || u3_none == u3t(pro) ) {
      fprintf(stderr, "l17: skip '%s'\n", _jets[i].nam_c);
      _jets[i].gate = 0;
    } else {
      _jets[i].gate = u3k(u3t(pro));
      ok++;
    }
    u3z(pro);
  }
  fprintf(stderr, "l17: %u/%u jets cached\n", ok, _jets_n);

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

  const uint8_t* p    = buf + 1;
  c3_w           n    = (c3_w)(len - 1);
  if ( n > H_MAX_LIST ) n = H_MAX_LIST;

  u3_noun sam;
  switch ( jet->shp_e ) {
    default:
    case S_LIST_GATE: {
      u3_noun gat = _gate_for(jet->gat_e);
      if ( 0 == gat ) return 0;
      u3_noun lis = _build_list(p, n);
      sam = u3nc(lis, u3k(gat));
      break;
    }
    case S_LIST_OF_LISTS: {
      sam = _build_list_of_lists(p, n);
      break;
    }
    case S_LIST_LIST: {
      c3_w half = n / 2;
      u3_noun la = _build_list(p, half);
      u3_noun lb = _build_list(p + half, n - half);
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
      fprintf(stderr, "jet_l17_lists: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
