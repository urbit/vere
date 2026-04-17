/// @file fuzz_http_request.c
///
/// H24 — AFL++ harness for the h2o-to-noun request conversion glue
/// in `pkg/vere/io/http.c`. Specifically, the three helpers that
/// turn an `h2o_req_t`'s iovec fields into nouns:
///
///   _http_vec_to_meth  — method string to mote
///   _http_vec_to_atom  — iovec bytes to cord
///   _http_vec_to_octs  — iovec bytes to (unit octs)
///   _http_heds_to_noun — header array to (list (pair cord cord))
///
/// These are called on every inbound HTTP request via
/// `_http_rec_to_httq`, which runs for every request that hits
/// eyre. The bugs would be in byte-length handling, UTF-8 edges,
/// header-value parsing.
///
/// Rather than #include "io/http.c" (which drags in h2o + openssl
/// transitively), we duplicate the tiny helpers inline. They're
/// ~30 lines and stable — if vere's copies change we'll notice
/// via a build break or a diff in the regression results. Any
/// refactor should update both.
///
/// Input layout:
///   byte 0   — selector: which helper to hit (mod 4)
///   bytes 1+ — payload
///
/// Build:   fuzz/build.sh fuzz_http_request
/// Corpus:  fuzz/corpus/fuzz_http_request/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "noun.h"

/* Minimal local copy of h2o_iovec_t + h2o_header_t so we don't
 * have to include h2o.h. Layout must match but we only ever
 * construct these ourselves, so that's not a constraint. */
typedef struct {
  char*  base;
  size_t len;
} h2o_iovec_t;

typedef struct {
  h2o_iovec_t* name;
  h2o_iovec_t* orig_name;
  h2o_iovec_t  value;
  c3_i         flags;
} h2o_header_t;

/* ---- Inlined copies of the helpers from pkg/vere/io/http.c ---- */

static u3_weak
_http_vec_to_meth(h2o_iovec_t vec_u)
{
  return ( 0 == strncmp(vec_u.base, "GET",     vec_u.len) ) ? u3i_string("GET") :
         ( 0 == strncmp(vec_u.base, "PUT",     vec_u.len) ) ? u3i_string("PUT")  :
         ( 0 == strncmp(vec_u.base, "POST",    vec_u.len) ) ? u3i_string("POST") :
         ( 0 == strncmp(vec_u.base, "HEAD",    vec_u.len) ) ? u3i_string("HEAD") :
         ( 0 == strncmp(vec_u.base, "CONNECT", vec_u.len) ) ? u3i_string("CONNECT") :
         ( 0 == strncmp(vec_u.base, "DELETE",  vec_u.len) ) ? u3i_string("DELETE") :
         ( 0 == strncmp(vec_u.base, "OPTIONS", vec_u.len) ) ? u3i_string("OPTIONS") :
         ( 0 == strncmp(vec_u.base, "TRACE",   vec_u.len) ) ? u3i_string("TRACE") :
         u3_none;
}

static u3_noun
_http_vec_to_atom(h2o_iovec_t vec_u)
{
  return u3i_bytes(vec_u.len, (const c3_y*)vec_u.base);
}

static u3_noun
_http_vec_to_octs(h2o_iovec_t vec_u)
{
  if ( 0 == vec_u.len ) {
    return u3_nul;
  }
  return u3nt(u3_nul, u3i_chubs(1, (const c3_d*)&vec_u.len),
                      _http_vec_to_atom(vec_u));
}

static u3_noun
_http_heds_to_noun(h2o_header_t* hed_u, c3_d hed_d)
{
  u3_noun hed = u3_nul;
  c3_d dex_d  = hed_d;

  h2o_header_t deh_u;

  while ( 0 < dex_d ) {
    deh_u = hed_u[--dex_d];
    hed = u3nc(u3nc(_http_vec_to_atom(*deh_u.name),
                    _http_vec_to_atom(deh_u.value)), hed);
  }

  return hed;
}

/* ---- Harness glue ---- */

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536
#define MAX_HEADERS 32

static const uint8_t* g_buf = NULL;
static c3_d           g_len = 0;
static uint8_t        g_mode = 0;

static u3_noun
_http_soft(u3_noun arg)
{
  (void)arg;

  switch (g_mode & 3) {
    case 0: {
      /* _http_vec_to_meth: full input is the method string */
      h2o_iovec_t vec = { (char*)g_buf, (size_t)g_len };
      u3_weak res = _http_vec_to_meth(vec);
      if (u3_none != res) u3z(res);
      break;
    }
    case 1: {
      /* _http_vec_to_atom: full input is the cord body */
      h2o_iovec_t vec = { (char*)g_buf, (size_t)g_len };
      u3_noun res = _http_vec_to_atom(vec);
      u3z(res);
      break;
    }
    case 2: {
      /* _http_vec_to_octs: full input is the octet stream */
      h2o_iovec_t vec = { (char*)g_buf, (size_t)g_len };
      u3_noun res = _http_vec_to_octs(vec);
      u3z(res);
      break;
    }
    case 3: {
      /* _http_heds_to_noun: split input into (name, value) pairs.
       * Each pair uses 1 byte for name_len + name + 1 byte for
       * value_len + value. Cap header count. */
      h2o_header_t headers[MAX_HEADERS] = {0};
      h2o_iovec_t  names[MAX_HEADERS]   = {0};
      c3_d         count = 0;
      c3_d         off = 0;

      while (count < MAX_HEADERS && off + 2 <= g_len) {
        uint8_t nlen = g_buf[off++];
        if (off + nlen >= g_len) break;
        names[count].base = (char*)(g_buf + off);
        names[count].len  = nlen;
        off += nlen;

        uint8_t vlen = g_buf[off++];
        if (off + vlen > g_len) break;
        headers[count].name       = &names[count];
        headers[count].orig_name  = &names[count];
        headers[count].value.base = (char*)(g_buf + off);
        headers[count].value.len  = vlen;
        off += vlen;

        count++;
      }

      u3_noun res = _http_heds_to_noun(headers, count);
      u3z(res);
      break;
    }
  }

  return u3_nul;
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 2 || len > H_MAX_INPUT) {
    return 0;
  }

  g_mode = buf[0];
  g_buf  = (const uint8_t*)(buf + 1);
  g_len  = (c3_d)(len - 1);

  u3_noun pro = u3m_soft(0, _http_soft, u3_nul);
  u3z(pro);

  return 0;
}
