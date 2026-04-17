/// @file fuzz_ce_patch_control.c
///
/// H12 — AFL++ harness for the snapshot patch verifier
/// (`_ce_patch_read_control` + `_ce_patch_verify` from
/// pkg/noun/events.c).
///
/// **Two-mode harness**: the verifier has checksum gates that random
/// fuzz mutations cannot pass without help. We split the input space
/// in half via byte 0:
///
///   byte 0 high bit set (>= 0x80)  — RAW MODE
///     Write bytes 1..N directly as control.bin and split into
///     control / memory at byte 1's value. Tests the file-size
///     parsing, the integer-overflow corner case in
///     `pgs_w * sizeof(u3e_line)`, and the early validation paths.
///
///   byte 0 high bit clear (< 0x80) — SMART MODE
///     The harness constructs a structured u3_ce_patch with valid
///     checksums:
///       byte 1: clamped pgs_w (0..MAX_PAGES)
///       bytes 2..: page content
///     Computes per-page murmur3 and meta murmur3, writes valid
///     control.bin / memory.bin, then runs the verifier. The fuzzer
///     mutates page content; checksums are recomputed each time.
///     Exercises the post-gate code: the page entry walk, pread
///     offset arithmetic, per-page hashing.
///
/// Build:   fuzz/build.sh fuzz_ce_patch_control
/// Corpus:  fuzz/corpus/fuzz_ce_patch_control/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>

#include "noun.h"
#include "events.h"
#include "events.c"

__AFL_FUZZ_INIT();

#define MAX_PAGES   4
#define H_MAX_INPUT (1 + MAX_PAGES * _ce_page)

static c3_c g_ctl_path[64];
static c3_c g_mem_path[64];

static void
_write_file(const c3_c* path, const void* data, size_t len)
{
  c3_i fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    fprintf(stderr, "open(%s) failed\n", path);
    _exit(1);
  }
  size_t off = 0;
  while (off < len) {
    ssize_t n = write(fd, (const c3_y*)data + off, len - off);
    if (n <= 0) break;
    off += (size_t)n;
  }
  close(fd);
}

int
main(void)
{
  u3m_boot_lite(1 << 24);

  snprintf(g_ctl_path, sizeof(g_ctl_path),
           "/tmp/fuzz_h12_ctl_%d.bin", (int)getpid());
  snprintf(g_mem_path, sizeof(g_mem_path),
           "/tmp/fuzz_h12_mem_%d.bin", (int)getpid());

  __AFL_INIT();

  unsigned char* buf = __AFL_FUZZ_TESTCASE_BUF;
  int            len = __AFL_FUZZ_TESTCASE_LEN;
  if (len < 2) {
    return 0;
  }

  if (buf[0] & 0x80) {
    /* RAW MODE: split bytes 2..N at offset buf[1] into ctl/mem. */
    int body_len = len - 2;
    int split = (buf[1] * body_len) / 256;
    if (split < 0) split = 0;
    if (split > body_len) split = body_len;
    _write_file(g_ctl_path, buf + 2, (size_t)split);
    _write_file(g_mem_path, buf + 2 + split, (size_t)(body_len - split));
  }
  else {
    /* SMART MODE: build valid frame with computed checksums. */
    if (len < 2) return 0;
    c3_w pgs_w = (c3_w)(buf[1] % (MAX_PAGES + 1));

    size_t pages_bytes = (size_t)pgs_w * (size_t)_ce_page;
    c3_y*  pages = (c3_y*)c3_calloc(pages_bytes ? pages_bytes : 1);
    size_t avail = (size_t)(len - 2);
    size_t copy  = avail < pages_bytes ? avail : pages_bytes;
    if (copy > 0) memcpy(pages, buf + 2, copy);

    size_t       con_sz = sizeof(u3e_control) + pgs_w * sizeof(u3e_line);
    u3e_control* con_u  = (u3e_control*)c3_calloc(con_sz);
    con_u->ver_w = U3P_VERLAT;
    con_u->tot_w = pgs_w;
    con_u->pgs_w = pgs_w;

    for (c3_w i = 0; i < pgs_w; i++) {
      con_u->mem_u[i].pag_w = i;
      con_u->mem_u[i].has_w = _ce_muk_page(pages + i * _ce_page);
    }

    {
      size_t off = offsetof(u3e_control, tot_w);
      con_u->has_w = _ce_muk_buf((c3_w)(con_sz - off),
                                 (c3_y*)con_u + off);
    }

    _write_file(g_ctl_path, con_u, con_sz);
    _write_file(g_mem_path, pages, pages_bytes);

    c3_free(con_u);
    c3_free(pages);
  }

  /* Open and run the real verifier. */
  c3_i ctl_i = open(g_ctl_path, O_RDONLY);
  c3_i mem_i = open(g_mem_path, O_RDONLY);
  if (ctl_i < 0 || mem_i < 0) {
    if (ctl_i >= 0) close(ctl_i);
    if (mem_i >= 0) close(mem_i);
    return 0;
  }

  u3_ce_patch pat_u = {0};
  pat_u.ctl_i = ctl_i;
  pat_u.mem_i = mem_i;

  if (c3y == _ce_patch_read_control(&pat_u)) {
    (void)_ce_patch_verify(&pat_u);
    c3_free(pat_u.con_u);
  }

  close(ctl_i);
  close(mem_i);

  return 0;
}
