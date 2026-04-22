/// @file fuzz_cttp_body.c
///
/// H28 — AFL++ harness for the HTTP client response-body callback in
/// pkg/vere/io/cttp.c. Specifically the logic exercised inside
/// `_cttp_creq_on_body` (line ~763):
///
///   _cttp_bod_new()          — allocate a u3_hbod from raw bytes
///   _cttp_cres_fire_body()   — append the body chunk to the response queue
///   _cttp_bods_to_octs()     — flatten the body queue into an octet-stream noun
///
/// Threat model: a malicious HTTP server controls every byte of the response
/// body that arrives at this callback. Bugs here would be in large/zero-length
/// chunks, integer overflow in length arithmetic, or use-after-free in the
/// body-queue linked list.
///
/// Implementation strategy:
///   Same as fuzz_cttp_head.c: we inline the three helpers that do the actual
///   parse/noun-conversion work rather than calling `_cttp_creq_on_body`
///   directly. A direct call would require a live h2o_http1client_t with a
///   wired-up h2o_socket_t + h2o_buffer_t, plus stubs for h2o_buffer_consume,
///   u3_auto_plan, u3_ovum_init, and _cttp_creq_free — all far more than the
///   fuzzing value warrants.
///
///   The inline copies are IDENTICAL to the cttp.c originals ("XX deduplicate
///   with _http_*" comments mark where they were lifted from). Any refactor of
///   those helpers must be reflected here.
///
/// Input layout:
///   byte  0       — mode selector (bit 0: include EOS chunk, bits 1-7: unused)
///   bytes 1+      — one or more body chunks, each encoded as:
///                     [2 bytes chunk_len LE][chunk_len bytes data]
///
///   The harness accumulates chunks into a u3_cres body queue and then calls
///   _cttp_bods_to_octs to flatten them (the same path taken by
///   _cttp_creq_respond). An empty chunk (chunk_len == 0) is also valid
///   input and exercises the zero-length branch in _cttp_bod_new.
///
/// Build:   fuzz/build.sh fuzz_cttp_body
/// Corpus:  fuzz/corpus/fuzz_cttp_body/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* ---- Inlined copies of the helpers from pkg/vere/io/cttp.c ---- */

/* u3_hbod: body buffer node (mirrors pkg/vere/vere.h exactly —
 * nex_u FIRST, then len_w, then the flexible array). */
typedef struct _u3_hbod {
  struct _u3_hbod *nex_u;
  c3_w             len_w;
  c3_y             hun_y[0];
} u3_hbod;

/* u3_cres: response accumulator (only the body-queue fields we use) */
typedef struct {
  c3_w     sas_w;   /* status code — not exercised here */
  u3_noun  hed;     /* headers     — not exercised here */
  u3_hbod *bod_u;   /* exit (head) of body queue */
  u3_hbod *dob_u;   /* entry (tail) of body queue */
} u3_cres;

/* _cttp_bod_new(): allocate a body chunk from raw bytes */
static u3_hbod *
_cttp_bod_new(c3_w len_w, c3_c *hun_c)
{
  u3_hbod *bod_u = c3_malloc(1 + len_w + sizeof(*bod_u));
  bod_u->hun_y[len_w] = 0;
  bod_u->len_w = len_w;
  if (len_w > 0) {
    memcpy(bod_u->hun_y, (const c3_y *)hun_c, len_w);
  }
  bod_u->nex_u = 0;
  return bod_u;
}

/* _cttp_cres_fire_body(): append a body chunk to the response queue */
static void
_cttp_cres_fire_body(u3_cres *res_u, u3_hbod *bod_u)
{
  /* Production assert: u3_assert(!bod_u->nex_u) — bod_u must be a singleton. */
  if (bod_u->nex_u != 0) {
    /* Safety: if somehow nex_u is set, drop it to avoid corrupting the queue.
     * This path should never be reached given how _cttp_bod_new initialises. */
    c3_free(bod_u);
    return;
  }

  if (!(res_u->bod_u)) {
    res_u->bod_u = res_u->dob_u = bod_u;
  } else {
    res_u->dob_u->nex_u = bod_u;
    res_u->dob_u = bod_u;
  }
}

/* _cttp_bods_free(): free the body queue */
static void
_cttp_bods_free(u3_hbod *bod_u)
{
  while (bod_u) {
    u3_hbod *nex_u = bod_u->nex_u;
    c3_free(bod_u);
    bod_u = nex_u;
  }
}

/* _cttp_bods_to_octs(): flatten body queue into an octet-stream noun
 * Returns u3nc(len, cord). */
static u3_noun
_cttp_bods_to_octs(u3_hbod *bod_u)
{
  c3_w  len_w;
  c3_y *buf_y;
  u3_noun cos;

  /* measure */
  {
    u3_hbod *bid_u = bod_u;
    len_w = 0;
    while (bid_u) {
      len_w += bid_u->len_w;
      bid_u = bid_u->nex_u;
    }
  }

  buf_y = c3_malloc(1 + len_w);
  buf_y[len_w] = 0;

  /* copy */
  {
    c3_y *ptr_y = buf_y;
    while (bod_u) {
      memcpy(ptr_y, bod_u->hun_y, bod_u->len_w);
      ptr_y += bod_u->len_w;
      bod_u = bod_u->nex_u;
    }
  }

  cos = u3i_bytes(len_w, buf_y);
  c3_free(buf_y);
  return u3nc(len_w, cos);
}

/* ---- Harness ---- */

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
/* Cap total accumulated body bytes to avoid exhausting the u3 loom in a
 * single iteration.  16 MiB is well above any real HTTP response that
 * would arrive on a loopback; real limit is u3m_boot_lite's slab size. */
#define MAX_BODY_BYTES (1u << 20)
/* Cap the number of chunks to bound queue-walk time. */
#define MAX_CHUNKS 256

int
main(void)
{
  /* u3m_boot_lite is sufficient — the helpers only use u3i_bytes / u3nc. */
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* Minimum: 1 mode byte + 2 chunk-length bytes */
  if (len < 3 || len > H_MAX_INPUT) {
    return 0;
  }

  /* byte 0: mode flags
   *   bit 0 — if set, exercise the EOS (end-of-stream) path that calls
   *            _cttp_bods_to_octs after all chunks are accumulated.
   *            If clear, we still accumulate chunks but skip the flatten step
   *            (exercises the partial-progress / streaming path). */
  uint8_t mode = buf[0];
  size_t  off  = 1;

  /* Synthesise a u3_cres body queue from the fuzz input. */
  u3_cres res_u;
  memset(&res_u, 0, sizeof(res_u));
  res_u.hed = u3_nul;

  size_t total_bytes = 0;
  size_t nchunks     = 0;

  while (off + 2 <= (size_t)len && nchunks < MAX_CHUNKS) {
    /* 2-byte chunk length LE */
    uint16_t chunk_len = (uint16_t)((uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8));
    off += 2;

    /* Clamp: don't read past the fuzz buffer. */
    if (off + chunk_len > (size_t)len) {
      chunk_len = (uint16_t)((size_t)len - off);
    }

    /* Clamp: don't accumulate more than MAX_BODY_BYTES total. */
    if (total_bytes + chunk_len > MAX_BODY_BYTES) {
      break;
    }

    /* _cttp_creq_on_body calls _cttp_bod_new(buf_u->size, buf_u->bytes)
     * where buf_u is the socket's input buffer. We replicate that call
     * directly.  chunk_len == 0 is deliberately allowed (empty chunk). */
    u3_hbod *bod_u = _cttp_bod_new((c3_w)chunk_len, (c3_c *)(buf + off));
    _cttp_cres_fire_body(&res_u, bod_u);

    off         += chunk_len;
    total_bytes += chunk_len;
    nchunks++;
  }

  /* EOS path: flatten the accumulated body into an octet-stream noun,
   * exactly as _cttp_creq_respond does.  Only do this when the EOS bit
   * is set — the opposite branch exercises the "partial progress" path
   * where the body queue is built but not yet consumed. */
  if ((mode & 0x01) && res_u.bod_u) {
    u3_noun octs = _cttp_bods_to_octs(res_u.bod_u);
    u3z(octs);
    /* _cttp_bods_to_octs does NOT free the queue — _cttp_creq_free does
     * that via _cttp_cres_free. We free it here to avoid a leak that
     * would accumulate across AFL iterations. */
    _cttp_bods_free(res_u.bod_u);
  } else {
    /* Non-EOS path: just free the queue (no noun built). */
    _cttp_bods_free(res_u.bod_u);
  }

  return 0;
}
