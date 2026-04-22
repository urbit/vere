/// @file fuzz_http_range.c
///
/// H27 — AFL++ harness for the HTTP Range header sub-parser.
///
/// Target: `_parse_range()` at pkg/vere/io/http.c:752. This function
/// accepts a byte slice of the form "X-Y" (the part of a Range header
/// value that follows the "bytes=" prefix), parses two size_t values
/// around the '-' separator, and applies several boundary checks.
///
/// The surrounding caller `_get_range()` (line 779) strips the
/// "bytes=" prefix before passing the remainder to `_parse_range`.
/// We replicate that stripping: if the input starts with "bytes=" we
/// skip those 6 bytes before invoking the parser, so the fuzzer can
/// freely generate both raw "X-Y" slices and full "bytes=X-Y" header
/// values from the same seed corpus.
///
/// `_parse_range` is self-contained — it only calls `memchr` and
/// `h2o_strtosize`. We inline a minimal compatible implementation of
/// `h2o_strtosize` so we can avoid including h2o.h (which pulls in
/// the entire h2o + OpenSSL world and would require the full vere
/// build flavour).  The inlined version is taken verbatim from
/// h2o/lib/common/string.c and is stable/small enough to copy.
///
/// No noun allocation takes place; `u3m_boot_lite` is still called
/// because the harness is compiled against libnoun.a which may assert
/// the loom is initialised.
///
/// Input layout:
///   - If the first 6 bytes equal "bytes=" the leading prefix is
///     stripped before parsing.
///   - Remainder is passed as (txt_c, len_w) to `_parse_range`.
///
/// Build:   fuzz/build.sh fuzz_http_range
/// Corpus:  fuzz/corpus/fuzz_http_range/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "noun.h"

/* ---- Minimal h2o_strtosize clone ------------------------------------
 * h2o_strtosize(s, len): parse a non-negative decimal integer from
 * the first `len` bytes of `s`. Returns SIZE_MAX on failure (no
 * digits, leading spaces, or overflow). This matches h2o's contract
 * exactly so that _parse_range's boundary checks remain correct.
 */
static size_t
_h2o_strtosize(const char* s, size_t len)
{
  size_t result = 0;

  if (len == 0) {
    return SIZE_MAX;
  }

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < '0' || c > '9') {
      return SIZE_MAX;
    }
    size_t prev = result;
    result = result * 10 + (c - '0');
    /* Overflow check: if result wrapped below the previous value we
     * overflowed. Also guard the multiply itself. */
    if (result < prev) {
      return SIZE_MAX;
    }
  }

  return result;
}

/* ---- byte_range type and _parse_range ---------------------------
 * Copied verbatim from pkg/vere/io/http.c so the harness compiles
 * without including http.c (which requires h2o.h / OpenSSL headers
 * from the zig-cache paths that are not in the noun build's include
 * set).  If http.c's _parse_range changes, update this copy.
 */
typedef struct _byte_range {
  size_t beg_z;
  size_t end_z;
} byte_range;

static byte_range
_parse_range(char* txt_c, unsigned int len_w)
{
  char* hep_c = memchr(txt_c, '-', len_w);
  byte_range rng_u;
  rng_u.beg_z = SIZE_MAX;
  rng_u.end_z = SIZE_MAX;

  if ( hep_c ) {
    rng_u.beg_z = _h2o_strtosize(txt_c, (size_t)(hep_c - txt_c));
    rng_u.end_z = _h2o_strtosize(hep_c + 1,
                                  len_w - (size_t)((hep_c + 1) - txt_c));
    /* mirror the "strange" check from the original */
    if (  ((SIZE_MAX == rng_u.beg_z) && (hep_c != txt_c))
       || ((SIZE_MAX == rng_u.end_z) &&
           (len_w - (size_t)((hep_c + 1) - txt_c) > 0))
       || ((SIZE_MAX != rng_u.beg_z) && (rng_u.beg_z > rng_u.end_z)) )
    {
      rng_u.beg_z = SIZE_MAX;
      rng_u.end_z = SIZE_MAX;
    }
  }
  return rng_u;
}

/* ---- Harness -------------------------------------------------------- */

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
/* "bytes=" prefix length */
#define BYTES_PFX_LEN 6

int
main(void)
{
  /* libnoun.a may assert the loom is present; boot with a tiny slab. */
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 1 || len > H_MAX_INPUT) {
    return 0;
  }

  /* Accept both "bytes=X-Y" and bare "X-Y" inputs.  Strip the prefix
   * when present, matching what _get_range() does before it calls us. */
  char*        txt_c = (char*)buf;
  unsigned int txt_w = (unsigned int)len;

  if ( (size_t)len >= BYTES_PFX_LEN
    && 0 == memcmp(buf, "bytes=", BYTES_PFX_LEN) )
  {
    txt_c += BYTES_PFX_LEN;
    txt_w -= BYTES_PFX_LEN;
  }

  if ( txt_w == 0 ) {
    return 0;
  }

  byte_range rng_u = _parse_range(txt_c, txt_w);
  /* Consume result so the compiler can't optimise the call away. */
  (void)rng_u.beg_z;
  (void)rng_u.end_z;

  return 0;
}
