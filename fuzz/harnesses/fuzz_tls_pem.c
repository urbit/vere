/// @file fuzz_tls_pem.c
///
/// H29 — AFL++ harness for the TLS PEM cert + key loader.
///
/// Target: the OpenSSL PEM parsing sequence inside `_http_init_tls()`
/// at pkg/vere/io/http.c:2359. That function:
///   1. Creates an SSL_CTX via SSLv23_server_method().
///   2. BIO_new_mem_buf(key) → PEM_read_bio_PrivateKey → SSL_CTX_use_PrivateKey.
///   3. BIO_new_mem_buf(cert) → PEM_read_bio_X509_AUX → SSL_CTX_use_certificate.
///   4. Loops PEM_read_bio_X509 for additional chain certs.
///   5. Returns the SSL_CTX* (or NULL with OpenSSL error info printed on
///      failure).
///
/// OpenSSL's PEM parsers are themselves fuzz-tested upstream, but
/// vere's error-handling wrapper is not. The fuzz target is the
/// sequence of calls and the branching on NULL / error return values:
/// specifically, that NULL pky_u / xer_u don't crash the code (e.g.
/// via EVP_PKEY_free(NULL) or X509_free(NULL) — both safe in modern
/// OpenSSL, but worth confirming under ASan) and that the early-return
/// paths correctly free all resources without leaks that ASan's
/// leak-checker would flag.
///
/// Simplifications vs the original:
///   - We omit the H2O_USE_ALPN branch (h2o_ssl_register_alpn_protocols)
///     because it requires the h2o library headers and is not the fuzzing
///     target. The SSL_CTX options and ciphers are reproduced faithfully.
///   - Error logging uses fprintf(stderr) instead of u3l_log /
///     u3_term_io_hija/loja, so we need neither libvere.a nor h2o.
///   - We split the fuzz input into a "key half" and a "cert half" at
///     a 1-byte split-point encoded in buf[0], matching the pattern
///     used by fuzz_ur_jam_cue_diff for dual-region inputs.
///
/// Input layout:
///   byte  0    — split point as a fraction of len: split_off =
///                1 + (buf[0] * (len-1)) / 256. Bytes [1..split_off)
///                are treated as the PEM private-key blob; bytes
///                [split_off..len) as the PEM certificate blob.
///
/// Build:   fuzz/build.sh fuzz_tls_pem
/// Corpus:  fuzz/corpus/fuzz_tls_pem/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OpenSSL headers — available via zig-out/include/openssl/ */
#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/err.h"
#include "openssl/evp.h"

__AFL_FUZZ_INIT();

#define H_MAX_INPUT 65536

/* ---- Inline replica of _http_init_tls() ----------------------------
 * Key difference: logging goes to stderr so we need no libvere symbols.
 * Everything else matches the original semantics.
 */
static SSL_CTX*
_try_init_tls(const char* key_p, int key_l, const char* cer_p, int cer_l)
{
  if ( key_l <= 0 ) {
    return NULL;
  }

  /* XX: upstream uses SSLv23_server_method() with TLS 1.0 suppressed. */
  SSL_CTX* tls_u = SSL_CTX_new(SSLv23_server_method());
  if ( NULL == tls_u ) {
    return NULL;
  }

  SSL_CTX_set_options(tls_u,
                      SSL_OP_NO_SSLv2 |
                      SSL_OP_NO_SSLv3 |
                      SSL_OP_NO_COMPRESSION);
  SSL_CTX_set_session_cache_mode(tls_u, SSL_SESS_CACHE_OFF);
  /* ignore return — cipher string may be unsupported by old OpenSSL */
  (void)SSL_CTX_set_cipher_list(tls_u,
    "ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:"
    "ECDH+AES128:DH+AES:ECDH+3DES:DH+3DES:RSA+AESGCM:"
    "RSA+AES:RSA+3DES:!aNULL:!MD5:!DSS");

  /* --- Private key -------------------------------------------------- */
  {
    BIO*      bio_u = BIO_new_mem_buf(key_p, key_l);
    EVP_PKEY* pky_u = PEM_read_bio_PrivateKey(bio_u, NULL, NULL, NULL);
    int       sas_i = SSL_CTX_use_PrivateKey(tls_u, pky_u);

    EVP_PKEY_free(pky_u);   /* safe if NULL */
    BIO_free(bio_u);

    if ( 0 == sas_i ) {
      /* Error path: drain the OpenSSL error queue so it doesn't bleed
       * across iterations, then tear down. */
      ERR_clear_error();
      SSL_CTX_free(tls_u);
      return NULL;
    }
  }

  /* --- Certificate chain -------------------------------------------- */
  if ( cer_l > 0 ) {
    BIO*  bio_u = BIO_new_mem_buf(cer_p, cer_l);
    X509* xer_u = PEM_read_bio_X509_AUX(bio_u, NULL, NULL, NULL);
    int   sas_i = SSL_CTX_use_certificate(tls_u, xer_u);

    X509_free(xer_u);       /* safe if NULL */

    if ( 0 == sas_i ) {
      ERR_clear_error();
      BIO_free(bio_u);
      SSL_CTX_free(tls_u);
      return NULL;
    }

    /* Additional chain certs — mirrors the original while() loop. */
    X509* extra_u;
    while ( NULL != (extra_u = PEM_read_bio_X509(bio_u, NULL, NULL, NULL)) ) {
      /* SSL_CTX_add_extra_chain_cert takes ownership on success; on
       * failure (returns 0) we must free manually. */
      if ( 0 == SSL_CTX_add_extra_chain_cert(tls_u, extra_u) ) {
        X509_free(extra_u);
      }
    }
    ERR_clear_error();      /* drain end-of-chain "no more data" error */

    BIO_free(bio_u);
  }

  return tls_u;
}

/* ---- Harness -------------------------------------------------------- */

int
main(void)
{
  /* OpenSSL global init (idempotent in OpenSSL 1.1+). */
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;

  /* Need at least the split byte + 1 byte of content. */
  if ( len < 2 || len > H_MAX_INPUT ) {
    return 0;
  }

  /* Compute split offset.  buf[0] is a 0-255 fraction; scale into
   * [1 .. len-1] so both halves are non-empty unless the fuzzer
   * specifically sends a 0 or 255 split byte. */
  uint8_t frac    = buf[0];
  int     payload = len - 1;
  int     key_len = 1 + ((int)frac * payload) / 256;
  if ( key_len >= payload ) {
    key_len = payload;      /* cert half may be empty — that's fine */
  }

  const char* key_p = (const char*)(buf + 1);
  int         key_l = key_len;
  const char* cer_p = (const char*)(buf + 1 + key_len);
  int         cer_l = payload - key_len;

  SSL_CTX* ctx = _try_init_tls(key_p, key_l, cer_p, cer_l);
  if ( ctx ) {
    SSL_CTX_free(ctx);
  }

  return 0;
}
