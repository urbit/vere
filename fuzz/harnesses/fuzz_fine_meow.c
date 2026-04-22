/// @file fuzz_fine_meow.c
///
/// H31 — AFL++ harness for _fine_sift_meow.
///
/// _fine_sift_meow parses a signed FINE scry response fragment from a
/// u3 noun atom (the raw wire bytes jammed as a big integer).  Layout:
///
///   bytes 0..63    64-byte host signature
///   bytes 64..67   number-of-fragments (u32, omitted if siz_s == 0)
///   bytes 68..end  data payload (up to FINE_FRAG == 1024 bytes)
///
/// The function operates on the noun via u3r_met / u3r_bytes so it
/// runs inside the u3 loom.  We need the full vere boot sequence
/// (ivory pill) to set up the loom properly.
///
/// Input handling: treat the raw fuzz bytes as the serialised wire
/// fragment.  Wrap them in a noun atom via u3i_bytes, then call
/// _fine_sift_meow.  The function consumes (u3z's) the noun on both
/// success and failure paths, so no extra cleanup is needed.
///
/// If _fine_sift_meow returns c3y it may allocate mew_u->dat_y via
/// c3_calloc; free it to keep the heap tidy across fork iterations.
///
/// Pattern: #include "./io/ames.c" trick (same as H8 / H27-H30) plus
/// the full ivory-pill boot sequence from H7 (fuzz_mesa_sift_pact).
///
/// Build:   fuzz/build.sh fuzz_fine_meow
/// Corpus:  fuzz/corpus/fuzz_fine_meow/
/// Min:     65 bytes (64-byte sig + at least 1 byte body)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"
/* Pull every static helper from ames.c into this translation unit. */
#include "io/ames.c"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

int
main(void)
{
  /* Full boot sequence required: _fine_sift_meow uses u3r_met /
   * u3r_bytes which access the loom. */
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);

  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( sil_u == NULL ) return 1;
  u3_weak pil = u3s_cue_xeno_with(sil_u,
                                   (c3_d)u3_Ivory_pill_len,
                                   u3_Ivory_pill);
  if ( pil == u3_none ) return 1;
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) return 1;

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* _fine_sift_meow requires at least 64 (sig) + 1 = 65 bytes.
   * The internal max_w check is sig_w + num_w + FINE_FRAG = 64+4+1024. */
  if ( len < 65 || len > H_MAX_INPUT ) {
    return 0;
  }

  /* Construct a noun atom from the raw bytes.  This is the same
   * encoding used in production: u3i_bytes(pac_u->len_w, pac_u->hun_y).
   * _fine_sift_meow takes ownership of the noun (calls u3z on it). */
  u3_noun mew = u3i_bytes((c3_w)len, (const c3_y*)buf);

  u3_meow mew_u = {0};
  c3_o ok = _fine_sift_meow(&mew_u, mew);

  /* Free the data payload allocated on success. */
  if ( c3y == ok && mew_u.dat_y ) {
    c3_free(mew_u.dat_y);
  }

  return 0;
}
