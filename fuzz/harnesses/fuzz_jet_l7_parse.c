/// @file fuzz_jet_l7_parse.c
///
/// Differential jet fuzzer for Hoon parser combinators in
/// `pkg/noun/jets/e/parse.c`. These jets take a `nail` (parsing
/// state = `[[line=@ud col=@ud] txt=tape]`) and/or another parser
/// and return an `edge` (`[nail (unit [result nail])]`).
///
/// Covers the combinators whose sample is just a `nail` or that
/// reduce to one — specifically the "atomic" parsers:
///   - `easy` : returns a default, consumes nothing
///   - `fail` : always fails
///   - `here` : captures the position
/// Plus the COMPOSITIONAL combinators that take an edge directly:
///   - `pfix`, `plug`, `pose`, `sfix` — take `(vex=edge sab=rule)`
///
/// For the compositional ones we supply a trivial `sab` rule (easy
/// or fail) so the oracle focuses on the combinator's own logic.
///
/// Build:  fuzz/build.sh fuzz_jet_l7_parse
/// Corpus: fuzz/corpus/fuzz_jet_l7_parse/

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
#define H_MAX_TAPE 32

typedef enum {
  S_NAIL,         /* nail = [[line col] tape]           */
  S_EDGE_RULE,    /* vex=edge sab=rule                  */
} shape_e;

typedef struct {
  const char* nam_c;
  const char* cor_c;
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

static jet_e _jets[] = {
  /* simpler arity */
  { "here", "here", S_NAIL,      0 },
  /* binary combinators take (vex, sab) */
  { "pfix", "pfix", S_EDGE_RULE, 0 },
  { "plug", "plug", S_EDGE_RULE, 0 },
  { "pose", "pose", S_EDGE_RULE, 0 },
  { "sfix", "sfix", S_EDGE_RULE, 0 },
};
static const c3_w _jets_n = sizeof(_jets) / sizeof(_jets[0]);

/* Cached `easy`/`fail` gates used as trivial `sab` arguments. */
static u3_noun g_easy_gate = 0;

static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

/* Build a tape (list of codepoints) from the next n bytes. */
static u3_noun
_build_tape(const uint8_t* p, c3_w n)
{
  u3_noun t = u3_nul;
  for ( c3_w i = n; i > 0; i-- ) {
    t = u3nc((u3_atom)p[i - 1], t);
  }
  return t;
}

/* Build a nail: [[line=0 col=i] txt=tape] */
static u3_noun
_build_nail(const uint8_t* p, c3_w n)
{
  c3_y col = (n > 0) ? p[0] : 0;
  const uint8_t* tp = (n > 0) ? p + 1 : p;
  c3_w tn = (n > 0) ? n - 1 : 0;
  if ( tn > H_MAX_TAPE ) tn = H_MAX_TAPE;
  u3_noun tape = _build_tape(tp, tn);
  return u3nc(u3nc(0, (u3_atom)col), tape);
}

/* Build a minimal edge `[p q]` where
 *   p = nail (farthest-progress marker)
 *   q = unit result — here we use `~` (no result)
 * This satisfies `pfix`/`plug`/`pose`/`sfix`'s edge input shape. */
static u3_noun
_build_edge(const uint8_t* p, c3_w n)
{
  u3_noun nail = _build_nail(p, n);
  /* q = [~ ~] (failure) OR [~ [val new-nail]] (success) */
  /* Use the "no result" variant: q = ~ */
  return u3nc(nail, u3_nul);
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
    if ( u3_none == gat ) { _jets[i].gate = 0; continue; }
    _jets[i].gate = gat;
  }
  g_easy_gate = u3v_wish("easy");

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
    case S_NAIL: {
      sam = _build_nail(p, rem);
      break;
    }
    case S_EDGE_RULE: {
      /* sam = (vex=edge sab=rule-gate)
       * Use `easy` applied to a trivial value as a no-op sab. */
      u3_noun edge = _build_edge(p, rem);
      sam = u3nc(edge, u3k(g_easy_gate));
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
      fprintf(stderr, "jet_l7_parse: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
