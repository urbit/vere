/// @file fuzz_lane_decode.c
///
/// H29 — AFL++ harness for lane atom deserializers.
///
/// Covers two functions in one harness:
///
///   u3_ames_decode_lane(u3_atom)  — public, from libvere.a.
///     Input: an atom encoding a 48-bit IP:port value as
///     (port_s << 32) | ip_w.  Returns u3_lane{pip_w, por_s}.
///     Invalid atoms (> 48 significant bits) yield 0.0.0.0:0.
///
///   _mesa_decode_lane_local(u3_atom) — u3_mesa_decode_lane is
///     *static* in mesa.c and returns sockaddr_in.  Rather than
///     pulling in the 2700-line mesa.c (which drags vast state), we
///     inline the function verbatim here.  The logic is identical;
///     the only production difference is that the real function
///     remaps localhost when ops_u.net is c3n — we keep net as c3y
///     (the default zero-init) so we exercise the straight-through
///     path that production traffic actually takes.
///
/// Input layout:
///   bytes 0..N-1   raw fuzz bytes → jammed as a big-integer atom via
///                  u3i_bytes.  The same atom is passed to both
///                  decoders so a single corpus covers both.
///
/// Pattern: vere-flavor build (links libvere.a for u3_ames_decode_lane
/// and the noun runtime; no extra source files).
///
/// Build:   fuzz/build.sh fuzz_lane_decode
/// Corpus:  fuzz/corpus/fuzz_lane_decode/
/// Min:     1 byte

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef U3_OS_windows
#  include <arpa/inet.h>
#endif

#include "vere.h"

/* Inline replica of the static u3_mesa_decode_lane from mesa.c.
 * Returns a sockaddr_in populated with host-byte-order IP and port.
 * We always take the "net == c3y" branch (standard production path).
 * Keeping this inline avoids including the entire 2700-line mesa.c. */
static struct sockaddr_in
_mesa_decode_lane_local(u3_atom lan)
{
  struct sockaddr_in adr_u = {0};
  c3_d lan_d;

  if ( c3n == u3r_safe_chub(lan, &lan_d) || (lan_d >> 48) != 0 ) {
    u3z(lan);
    return adr_u;
  }

  u3z(lan);

  /* net == c3y path: use the raw IP from the atom. */
  adr_u.sin_addr.s_addr = htonl((c3_w)lan_d);
  adr_u.sin_port        = htons((c3_s)(lan_d >> 32));
  return adr_u;
}

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

int
main(void)
{
  /* Both targets are pure atom-reading functions — no Arvo kernel
   * needed. Ivory pill boot was a holdover from the H7 template and
   * was slowing this harness ~6x vs peers (per audit). Plain
   * u3m_boot_lite is enough for u3r_safe_chub / u3i_bytes. */
  u3m_boot_lite(1 << 26);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  if ( len < 1 || len > H_MAX_INPUT ) {
    return 0;
  }

  /* Construct a big-integer atom from the raw fuzz bytes.  This
   * mirrors how production code receives an encoded lane from Arvo:
   * as an atom whose bytes are the little-endian IP:port encoding. */
  u3_atom lan_a = u3i_bytes((c3_w)len, (const c3_y*)buf);
  u3_atom lan_b = u3k(lan_a);   /* keep a second reference for mesa */

  /* 1. u3_ames_decode_lane — consumes (zaps) lan_a. */
  u3_lane ames_lane = u3_ames_decode_lane(lan_a);
  (void)ames_lane;

  /* 2. inline mesa variant — consumes lan_b. */
  struct sockaddr_in mesa_sa = _mesa_decode_lane_local(lan_b);
  (void)mesa_sa;

  return 0;
}
