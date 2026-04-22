/// @file fuzz_jet_l4b_encode.c
///
/// Differential jet fuzzer for layer-4b text / encoding jets.
/// Covers: `en_base16`, `de_base16`, `fein_ob`, `fynd_ob`,
/// `scot`, `scow`, `slaw`, `mat`, `rub`.
///
/// Deliberately excludes `jam`/`cue` — they'd recurse through
/// the ice oracle during `u3v_wish`/`u3n_nock_on` since the
/// interpreter uses cue internally.
///
/// Build:  fuzz/build.sh fuzz_jet_l4b_encode
/// Corpus: fuzz/corpus/fuzz_jet_l4b_encode/

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
  S_ATOM,             /* one atom (cord / data)    */
  S_WID_DAT,          /* (wid=@ud dat=@)           */
  S_AURA_ATOM,        /* (aura=@ta value=@)        */
  S_AURA_CORD,        /* (aura=@ta cord=@ta)       */
  S_OFF_BITS,         /* (offset=@ud bits=@) — rub */
} shape_e;

typedef struct {
  const char* nam_c;   /* u3v_wish name (e.g. "fein:ob")            */
  const char* cor_c;   /* core name for ice flip (e.g. "fein")      */
  shape_e     shp_e;
  u3_noun     gate;
} jet_e;

/* `en_base16` / `de_base16` aren't top-level in the ivory pill.
 * fein/fynd resolve under the `ob` door via `fein:ob` / `fynd:ob`. */
static jet_e _jets[] = {
  { "fein:ob", "fein", S_ATOM,      0 },
  { "fynd:ob", "fynd", S_ATOM,      0 },
  { "scot",    "scot", S_AURA_ATOM, 0 },
  { "scow",    "scow", S_AURA_ATOM, 0 },
  { "slaw",    "slaw", S_AURA_CORD, 0 },
  { "mat",     "mat",  S_ATOM,      0 },
  { "rub",     "rub",  S_OFF_BITS,  0 },
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

/* Common auras for scot/scow/slaw testing. Term atoms built
 * lazily via c3_s-style 4-byte packs where the mote macros aren't
 * exposed. */
static u3_atom
_pick_aura(uint8_t b)
{
  switch ( b % 7 ) {
    default:
    case 0: return c3__ud;
    case 1: return c3__ux;
    case 2: return c3__uv;
    case 3: return c3__uw;
    case 4: return c3__p;
    case 5: return c3__da;   /* date */
    case 6: return c3_s1('t');   /* @t — ASCII cord */
  }
}

int
main(void)
{
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( !sil_u ) { fprintf(stderr, "l4b: cue init\n"); return 1; }
  u3_weak pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len, u3_Ivory_pill);
  if ( pil == u3_none ) { fprintf(stderr, "l4b: pill cue\n"); return 1; }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) { fprintf(stderr, "l4b: boot\n"); return 1; }

  for ( c3_w i = 0; i < _jets_n; i++ ) {
    u3_noun gat = u3v_wish(_jets[i].nam_c);
    if ( u3_none == gat ) {
      fprintf(stderr, "l4b: u3v_wish('%s') failed — skipping\n", _jets[i].nam_c);
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
  if ( len < 2 || len > H_MAX_INPUT ) return 0;

  c3_w   mode = buf[0] % _jets_n;
  jet_e* jet  = &_jets[mode];
  if ( 0 == jet->gate ) return 0;

  /* Clamp payload to 256 bytes — keeps Hoon-side runtime under the
   * AFL timeout for even the slowest encoders (slaw text parsers
   * are particularly expensive). */
  c3_w pay_len = len - 2;
  if ( pay_len > 256 ) pay_len = 256;
  const uint8_t* pay_p = buf + 2;
  uint8_t aura_sel = buf[1];

  u3_atom dat = u3i_bytes(pay_len, pay_p);
  u3_noun sam;

  switch ( jet->shp_e ) {
    default:
    case S_ATOM:       sam = dat;                                       break;
    case S_WID_DAT:    sam = u3nc(u3i_word(pay_len), dat);              break;
    case S_AURA_ATOM:  sam = u3nc(_pick_aura(aura_sel), dat);           break;
    case S_AURA_CORD:  sam = u3nc(_pick_aura(aura_sel), dat);           break;
    case S_OFF_BITS:
      /* rub: [a=@ b=@] — a is starting bit offset, b is the bitstream
       * atom. Keep offset 0 so rub always starts reading at the
       * beginning (simplest valid call). */
      sam = u3nc(0, dat);
      break;
  }

  g_gate = jet->gate;
  g_sam  = sam;

  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);

  u3_noun how = u3h(pro);
  if ( 0 != how ) {
    c3_m bail_tag = u3r_word(0, how);
    if ( c3__fail == bail_tag && c3y == u3j_Fuzz_mismatch ) {
      fprintf(stderr, "jet_l4b_encode: ORACLE MISMATCH in '%s'\n", jet->nam_c);
      abort();
    }
  }

  u3z(sam);
  u3z(pro);
  return 0;
}
