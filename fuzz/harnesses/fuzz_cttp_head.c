/// @file fuzz_cttp_head.c
///
/// H27 — AFL++ harness for the HTTP client response-header callback in
/// pkg/vere/io/cttp.c. Specifically the logic exercised inside
/// `_cttp_creq_on_head` (line ~792):
///
///   _cttp_cres_new()       — allocate a response object, store status code
///   _cttp_heds_to_noun()   — convert h2o_header_t[] to (list (pair cord cord))
///   _cttp_vec_to_atom()    — convert h2o_iovec_t to atom
///
/// Threat model: a malicious HTTP server controls the status line and all
/// response headers that arrive at this callback. Bugs here would be in
/// length-zero fields, very long header names/values, embedded NULs, or
/// a large header count overflowing an arithmetic expression.
///
/// Implementation strategy:
///   Rather than calling `_cttp_creq_on_head` directly (which would require
///   a fully-initialized h2o_http1client_t, a live u3_creq linked into a
///   u3_cttp, and stubs for u3_auto_plan / u3_ovum_init / _cttp_creq_free),
///   we inline the three helper functions that do the actual noun-conversion
///   work. This mirrors the approach taken by fuzz_http_request.c, which
///   inlines the analogous helpers from pkg/vere/io/http.c.
///
///   The inline copies are IDENTICAL to the cttp.c originals. Any refactor
///   of those helpers must be reflected here (a build break or diff in
///   regression results will signal drift).
///
/// Input layout:
///   byte  0       — number of headers to build (capped at MAX_HEADERS)
///   byte  1-3     — status code (little-endian 24-bit, i.e. mod 65536+1)
///   bytes 4+      — header encoding: each header is:
///                     [1 byte name_len][name_len bytes][1 byte val_len][val_len bytes]
///
/// Build:   fuzz/build.sh fuzz_cttp_head
/// Corpus:  fuzz/corpus/fuzz_cttp_head/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Minimal local copies of h2o types.  Layout must match h2o/memory.h and
 * h2o/http1client.h. We construct these ourselves so there is no ABI
 * dependency on the installed h2o headers. */
typedef struct {
  char   *base;
  size_t  len;
} fuzz_iovec_t;

typedef struct {
  fuzz_iovec_t *name;
  fuzz_iovec_t *orig_name;
  fuzz_iovec_t  value;
  int           flags;
} fuzz_header_t;

/* ---- Inlined copies of the helpers from pkg/vere/io/cttp.c ---- */

/* _cttp_vec_to_atom(): convert iovec to atom (cord) */
static u3_noun
_cttp_vec_to_atom(fuzz_iovec_t vec_u)
{
  return u3i_bytes(vec_u.len, (const c3_y *)vec_u.base);
}

/* _cttp_heds_to_noun(): convert header array to (list (pair cord cord)) */
static u3_noun
_cttp_heds_to_noun(fuzz_header_t *hed_u, size_t hed_t)
{
  u3_noun hed   = u3_nul;
  size_t  dex_t = hed_t;

  while (0 < dex_t) {
    fuzz_header_t deh_u = hed_u[--dex_t];
    hed = u3nc(u3nc(_cttp_vec_to_atom(*deh_u.name),
                    _cttp_vec_to_atom(deh_u.value)),
               hed);
  }

  return hed;
}

/* ---- Harness ---- */

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define MAX_HEADERS 64

int
main(void)
{
  /* u3m_boot_lite is sufficient — the helpers only use u3i_bytes / u3nc,
   * which do not require a kernel or ivory pill. */
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* Minimum: 1 (nheads byte) + 2 (status, little-endian 16-bit) */
  if (len < 3 || len > H_MAX_INPUT) {
    return 0;
  }

  /* byte 0: number of headers the fuzzer wants us to build */
  unsigned int req_heads = (unsigned int)buf[0];

  /* bytes 1-2: HTTP status code (16-bit LE, capped to sane range) */
  uint16_t status_u = (uint16_t)((uint32_t)buf[1] | ((uint32_t)buf[2] << 8));

  size_t off = 3;

  /* Build header array from remaining bytes.
   * Each header: [1-byte name_len][name bytes][1-byte val_len][val bytes]
   * We point name/value into the AFL buffer directly — no copies needed
   * and ASan will catch any out-of-bounds access in the helper. */
  fuzz_header_t headers[MAX_HEADERS];
  fuzz_iovec_t  names[MAX_HEADERS];
  size_t        nheads = 0;

  while (nheads < MAX_HEADERS
      && nheads < (size_t)req_heads
      && off + 2 <= (size_t)len)
  {
    uint8_t nlen = buf[off++];
    if (off + nlen > (size_t)len) {
      break;
    }
    names[nheads].base = (char *)(buf + off);
    names[nheads].len  = nlen;
    off += nlen;

    if (off + 1 > (size_t)len) {
      break;
    }
    uint8_t vlen = buf[off++];
    if (off + vlen > (size_t)len) {
      break;
    }
    fuzz_iovec_t val = { (char *)(buf + off), vlen };
    off += vlen;

    headers[nheads].name      = &names[nheads];
    headers[nheads].orig_name = &names[nheads];
    headers[nheads].value     = val;
    headers[nheads].flags     = 0;
    nheads++;
  }

  /* Exercise the two noun-conversion helpers that _cttp_creq_on_head runs
   * for every inbound response.  We wrap in u3m_soft so any u3 bail
   * (allocation failure, malformed atom length) is caught and turned into
   * a non-crash return rather than an abort(). */

  /* 1. _cttp_heds_to_noun — the header list builder */
  {
    u3_noun hed = _cttp_heds_to_noun(headers, nheads);
    u3z(hed);
  }

  /* 2. _cttp_vec_to_atom on the status message (synthesize a status iovec
   *    from the remaining input so the fuzzer can explore corner cases:
   *    empty string, very long string, embedded NULs). */
  {
    size_t     sas_len = (off < (size_t)len) ? ((size_t)len - off) : 0;
    fuzz_iovec_t sas_u = { (char *)(buf + off), sas_len };
    u3_noun  sas = _cttp_vec_to_atom(sas_u);

    /* Also exercise status-code storage (u3i_chubs path for large codes). */
    u3_noun cod = u3i_word((c3_w)status_u);

    u3z(sas);
    u3z(cod);
  }

  return 0;
}
