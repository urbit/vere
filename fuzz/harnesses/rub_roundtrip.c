/// Roundtrip check: does u3qe_rub(0, u3qe_mat(x)) == x for many x?
/// If yes, the C jet is self-consistent on valid jam streams —
/// finding #016 then concerns adversarial-only inputs.
/// If no, this is a real jam/cue-affecting bug.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"

__AFL_FUZZ_INIT();

int main(void) {
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
  u3_cue_xeno* sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  u3_weak pil = u3s_cue_xeno_with(sil, u3_Ivory_pill_len, u3_Ivory_pill);
  u3s_cue_xeno_done(sil);
  u3v_boot_lite(pil);

  int fails = 0;
  int tries = 0;

  /* Sweep all x in [0, 4096) — covers every small-atom bit pattern,
   * and captures the boundaries of the jam encoding's zero-run /
   * payload layout where a byte-level off-by-one would show up. */
  for (int i = 0; i < 4096; i++) {
    tries++;
    u3_atom x = u3i_chub((c3_d)i);
    u3_noun enc = u3qe_mat(u3k(x));    /* [p=bitlen q=bits] */
    u3_atom p = u3h(enc);
    u3_atom q = u3t(enc);

    /* decode back starting at bit 0 */
    u3_noun dec = u3qe_rub(0, u3k(q));  /* [p'=bitlen q'=value] */
    u3_atom p2 = u3h(dec);
    u3_atom q2 = u3t(dec);

    c3_o p_ok = u3r_sing(p, p2);
    c3_o q_ok = u3r_sing(x, q2);

    if (c3n == p_ok || c3n == q_ok) {
      fails++;
      c3_w pv=0, p2v=0, qv=0;
      if (_(u3a_is_cat(p))) pv = (c3_w)p;
      if (_(u3a_is_cat(p2))) p2v = (c3_w)p2;
      if (_(u3a_is_cat(q2))) qv = (c3_w)q2;
      fprintf(stderr, "FAIL x=%d: mat.p=%u rub.p=%u rub.q=%u %s %s\n",
              i, pv, p2v, qv,
              (c3n==p_ok) ? "p-mismatch" : "",
              (c3n==q_ok) ? "q-mismatch" : "");
    }
    u3z(enc); u3z(dec); u3z(x);
  }

  fprintf(stderr, "rub(mat(x)) round-trip: %d/%d OK, %d failures\n",
          tries - fails, tries, fails);
  return fails;
}
