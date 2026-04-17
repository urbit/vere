/// @file fuzz_conn_poke.c
///
/// H15 — AFL++ harness for the Khan IPC dispatch validation in
/// `_conn_moor_poke` (`pkg/vere/io/conn.c:593`).
///
/// The full `_conn_moor_poke` calls into pier-side machinery on
/// success (`u3_pier_peek`, `u3_pier_meld`, `u3_auto_plan`, etc.).
/// We don't have a real pier in the harness. Instead we mirror the
/// **pre-pier validation logic** — cue the bytes, check the
/// `[rid [tag dat]]` shape, walk into each tag's specific sub-shape
/// (e.g. for `%ovum`, check `dat` is a triple). This is where the
/// actual parser-style bug surface lives; the dispatch beyond it
/// hands off to bigger systems we cover in their own harnesses
/// (cue is H4/H5, ovum injection would be a fore.c harness).
///
/// Build:   fuzz/build.sh fuzz_conn_poke
/// Corpus:  fuzz/corpus/fuzz_conn_poke/
/// Dict:    fuzz/dicts/cue.dict (re-used)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/* H4/H5 already cover u3s_cue_bytes / u3s_cue_xeno; H15's unique
 * value is the *post-cue* structural validation. We use
 * u3s_cue_bytes (not xeno) so we can run inside u3m_soft — xeno's
 * `u3R == home` assert is incompatible with the child-road soft
 * frame. The bug surface we care about (the [rid [tag dat]] shape
 * checks and tag dispatch) is identical between the two paths. */
static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;

/* Mote constants for the dispatch tags — same as conn.c. */
#define MOTE_FYRD c3_s4('f','y','r','d')
#define MOTE_PEEK c3_s4('p','e','e','k')
#define MOTE_PEEL c3_s4('p','e','e','l')
#define MOTE_OVUM c3_s4('o','v','u','m')
#define MOTE_URTH c3_s4('u','r','t','h')
#define MOTE_MELD c3_s4('m','e','l','d')
#define MOTE_PACK c3_s4('p','a','c','k')

static u3_noun
_validate_soft(u3_noun arg)
{
  (void)arg;

  /* Step 1: cue the input. Mirrors the `u3s_cue_xeno_with` call in
   * _conn_moor_poke (using u3s_cue_bytes — see the comment on
   * g_buf above for why). On a parse error u3s_cue_bytes bails via
   * u3m_bail; u3m_soft's setjmp catches it and we return cleanly. */
  u3_noun jar = u3s_cue_bytes(g_len, g_buf);

  /* Step 2: outer cell — [rid can]. */
  u3_noun rid, can;
  if (c3n == u3r_cell(jar, &rid, &can)) {
    u3z(jar);
    return u3_nul;
  }

  /* Step 3: rid must be an atom. */
  if (c3n == u3ud(rid)) {
    u3z(jar);
    return u3_nul;
  }

  /* Step 4: can must be a cell — [tag dat]. */
  u3_noun tag, dat;
  if (c3n == u3r_cell(can, &tag, &dat)) {
    u3z(jar);
    return u3_nul;
  }

  /* Step 5: dispatch by tag. We don't actually call the pier
   * action — we just do the shape checks the real code does
   * before that call. */
  switch (tag) {
    case MOTE_FYRD:
    case MOTE_PEEK:
      /* fyrd/peek take any noun in `dat` — no extra shape check. */
      break;

    case MOTE_PEEL: {
      /* _conn_read_peel walks a [head tail] structure with
       * specific tag values inside. We mirror its outer shape
       * checks: dat must be a cell or u3_nul. */
      u3_noun i_dat, t_dat;
      if (c3y == u3r_cell(dat, &i_dat, &t_dat)) {
        if (u3_nul != t_dat) {
          u3_noun it_dat, tt_dat;
          if (c3y == u3r_cell(t_dat, &it_dat, &tt_dat)) {
            (void)it_dat;
            (void)tt_dat;
          }
        }
      }
    } break;

    case MOTE_OVUM: {
      /* dat must be [tar wir cad] — three-element cell. */
      u3_noun tar, wir, cad;
      (void)u3r_trel(dat, &tar, &wir, &cad);
    } break;

    case MOTE_URTH: {
      /* dat must be %meld or %pack. */
      switch (dat) {
        case MOTE_MELD:
        case MOTE_PACK:
          break;
        default:
          break;
      }
    } break;

    default:
      /* unknown tag */
      break;
  }

  u3z(jar);
  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  g_buf = (const uint8_t*)buf;
  g_len = (c3_d)len;

  u3_noun pro = u3m_soft(0, _validate_soft, u3_nul);
  u3z(pro);

  return 0;
}
