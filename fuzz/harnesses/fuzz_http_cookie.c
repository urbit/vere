/// @file fuzz_http_cookie.c
///
/// H28 — AFL++ harness for the HTTP cookie auth scanner.
///
/// Target: the cookie-parsing loop inside `_http_req_is_auth()` at
/// pkg/vere/io/http.c:334. That function:
///   1. Receives a raw "Cookie:" header value (e.g. "a=1; sess=TOKEN; b=2").
///   2. Scans it character-by-character comparing against a fixed auth-key
///      string to locate the key, then copies the value into a 128-byte
///      stack buffer `val_c[128]`.
///   3. Calls u3kdi_has(u3k(fig_u->ses), u3i_bytes(...)) to check the
///      extracted value against the set of valid session tokens.
///
/// The interesting bugs live in steps 2 and 3: the scan uses two
/// cursors (i_i over the header, j_i over the key) with no explicit
/// length guard on j_i, and step 3 converts the extracted bytes to a
/// noun atom and looks them up in a noun set.
///
/// Simplifications:
///   - We provide a fixed non-empty auth key ("sess") so the scanner
///     reaches the value-extraction branch on most inputs.
///   - We build a minimal empty set (u3_nul, the empty ~) as
///     fig_u->ses — u3kdi_has will always return c3n, but the call
///     still exercises u3i_bytes and the set lookup internals.
///   - We skip the h2o_find_header_by_str lookup and pass the fuzz
///     input directly as the cookie string, which is what that call
///     returns anyway.
///
/// Because u3kdi_has allocates nouns we wrap each iteration in
/// u3m_soft so loom errors don't abort the process. The result is
/// always freed.
///
/// Input layout:
///   Raw cookie header value bytes, e.g. "sess=TOKEN; other=val".
///
/// Build:   fuzz/build.sh fuzz_http_cookie
/// Corpus:  fuzz/corpus/fuzz_http_cookie/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* ---- Inlined copy of the cookie-scanning loop ----------------------
 * Extracted from _http_req_is_auth(). The original touches a full
 * u3_hfig* for fig_u->key_c and fig_u->ses. We replace those with
 * local variables so we avoid constructing any of the surrounding
 * server state.
 *
 * Returns: u3_noun result of u3kdi_has lookup (c3y or c3n), or
 *          u3_nul if the cookie header was empty.
 */

/* Fixed auth key the fuzzer tries to find in the cookie string. */
static const char* AUTH_KEY = "sess";

/* Harness globals set per iteration. */
static const uint8_t* g_coo  = NULL;
static size_t         g_len  = 0;

static u3_noun
_cookie_soft(u3_noun ignored)
{
  (void)ignored;

  /* Empty sessions set: the noun ~. */
  u3_noun ses = u3_nul;

  const char* key_c = AUTH_KEY;
  char        val_c[128];
  uint8_t     val_y = 0;
  size_t      i_i   = 0;
  size_t      j_i   = 0;

  while (i_i < g_len) {
    if (key_c[j_i] == '\0' && g_coo[i_i] == '=') {
      i_i++;
      while ( i_i < g_len
           && g_coo[i_i] != ';'
           && val_y < (uint8_t)sizeof(val_c) ) {
        val_c[val_y] = (char)g_coo[i_i];
        val_y++;
        i_i++;
      }
      break;
    }
    else if ((char)g_coo[i_i] == key_c[j_i]) {
      j_i++;
    }
    else {
      j_i = 0;
    }
    i_i++;
  }

  /* Replicate the noun path: convert extracted bytes to atom, look up
   * in the (empty) session set.  The return value is c3y or c3n. */
  u3_noun tok = u3i_bytes((c3_w)val_y, (const c3_y*)val_c);
  u3_noun aut = u3kdi_has(u3k(ses), tok);
  u3z(ses);
  return aut;
}

/* ---- Harness -------------------------------------------------------- */

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

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

  g_coo = (const uint8_t*)buf;
  g_len = (size_t)len;

  u3_noun pro = u3m_soft(0, _cookie_soft, u3_nul);
  u3z(pro);

  return 0;
}
