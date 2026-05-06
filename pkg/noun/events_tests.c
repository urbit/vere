/// @file
///
/// tests for the loom snapshot patching system in events.{c,h}.

#include "events.h"
#include "manage.h"

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "./events.c"

/* _tmpdir(): mkdtemp wrapper, aborts on failure.  caller frees.
*/
static c3_c*
_tmpdir(void)
{
  char tmpl[] = "/tmp/events-test-XXXXXX";
  c3_c* dir_c = mkdtemp(tmpl);
  if ( !dir_c ) {
    fprintf(stderr, "*** mkdtemp: %s\r\n", strerror(errno));
    u3_assert(0);
  }
  return strdup(dir_c);
}

/* _rmtree_one(): nftw callback, removes one entry.
*/
static c3_i
_rmtree_one(const char* pax_c, const struct stat* sat_u,
            int typ_i, struct FTW* ftw_u)
{
  (void)sat_u; (void)typ_i; (void)ftw_u;
  if ( 0 != remove(pax_c) ) {
    fprintf(stderr, "*** remove %s: %s\r\n", pax_c, strerror(errno));
  }
  return 0;
}

/* _rmtree(): recursive remove.
*/
static void
_rmtree(c3_c* dir_c)
{
  if ( 0 != nftw(dir_c, _rmtree_one, 64, FTW_DEPTH | FTW_PHYS) ) {
    fprintf(stderr, "*** nftw %s: %s\r\n", dir_c, strerror(errno));
  }
}

/* _build_patch(): write a valid patch (control.bin + memory.bin) into [dir_c]
**                 with [n_w] pages.  page i is filled with byte (0xa0 + i).
**                 page numbers are arbitrary but distinct (i*2 + 1).
**                 [pat_u] receives the open fds and the in-memory u3e_control.
**                 the caller frees pat_u via _close_patch.
*/
static void
_build_patch(c3_c* dir_c, u3_ce_patch* pat_u, c3_w n_w)
{
  c3_c pax_c[8192];
  c3_z len_z = sizeof(u3e_control) + (n_w * sizeof(u3e_line));
  u3e_control* con_u = c3_malloc(len_z);
  c3_y page_y[_ce_page];

  con_u->ver_h = U3P_VERLAT;
  con_u->tot_w = n_w + 4;          //  arbitrary total > n_w
  con_u->pgs_w = n_w;

  snprintf(pax_c, sizeof(pax_c), "%s/memory.bin", dir_c);
  c3_i mem_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != mem_i);

  for ( c3_w i_w = 0; i_w < n_w; i_w++ ) {
    memset(page_y, 0xa0 + (c3_y)i_w, _ce_page);
    u3_assert(_ce_page == pwrite(mem_i, page_y, _ce_page, _ce_len(i_w)));
    con_u->mem_u[i_w].pag_w = (i_w * 2) + 1;
    con_u->mem_u[i_w].has_h = _ce_muk_page(page_y);
  }

  {
    c3_z off_z = offsetof(u3e_control, tot_w);
    con_u->has_h = _ce_muk_buf(len_z - off_z, (c3_y*)con_u + off_z);
  }

  snprintf(pax_c, sizeof(pax_c), "%s/control.bin", dir_c);
  c3_i ctl_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != ctl_i);

  pat_u->ctl_i = ctl_i;
  pat_u->mem_i = mem_i;
  pat_u->con_u = con_u;
  pat_u->sip_w = 0;
  _ce_patch_write_control(pat_u);
}

/* _close_patch(): release everything in [pat_u].
*/
static void
_close_patch(u3_ce_patch* pat_u)
{
  if ( pat_u->con_u ) c3_free(pat_u->con_u);
  if ( -1 != pat_u->ctl_i ) close(pat_u->ctl_i);
  if ( -1 != pat_u->mem_i ) close(pat_u->mem_i);
  pat_u->con_u = 0;
  pat_u->ctl_i = -1;
  pat_u->mem_i = -1;
}

/* _open_patch(): reopen a patch [dir_c] for verify-style tests.
*/
static void
_open_patch(c3_c* dir_c, u3_ce_patch* pat_u)
{
  c3_c pax_c[8192];

  pat_u->con_u = 0;
  pat_u->sip_w = 0;

  snprintf(pax_c, sizeof(pax_c), "%s/control.bin", dir_c);
  pat_u->ctl_i = c3_open(pax_c, O_RDWR);
  u3_assert(-1 != pat_u->ctl_i);

  snprintf(pax_c, sizeof(pax_c), "%s/memory.bin", dir_c);
  pat_u->mem_i = c3_open(pax_c, O_RDWR);
  u3_assert(-1 != pat_u->mem_i);
}

/* _setup(): boot the loom in dirty state (no on-disk pier).
*/
static void
_setup(size_t len_i)
{
  u3m_init((size_t)1 << len_i);
  u3e_init();
  u3m_pave(c3y);
}

/* _test_muk_determinism(): _ce_muk_buf / _ce_muk_page produce the same hash
**   on repeated calls and match the underlying murmur3.
*/
static void
_test_muk_determinism(void)
{
  c3_y     buf_y[16] = "abcdefghijklmnop";
  c3_h     ref_h     = _ce_muk_buf(16, buf_y);
  uint32_t direct_w;

  for ( c3_w i_w = 0; i_w < 1000; i_w++ ) {
    u3_assert( ref_h == _ce_muk_buf(16, buf_y) );
  }

  //  no garbage upper bits, no contamination from c3_w return-type widening
  //
  MurmurHash3_x86_32(buf_y, 16, 0xcafebabeU, &direct_w);
  u3_assert( ref_h == (c3_h)direct_w );

  {
    c3_y page_y[_ce_page];
    memset(page_y, 0xab, _ce_page);
    u3_assert( _ce_muk_page(page_y) == _ce_muk_page(page_y) );
  }

  fprintf(stderr, "test_muk_determinism: ok\r\n");
}

/* _test_muk_distinct(): different inputs hash distinctly.
*/
static void
_test_muk_distinct(void)
{
  c3_y a_y[_ce_page], b_y[_ce_page];

  memset(a_y, 0x00, _ce_page);
  memset(b_y, 0xff, _ce_page);
  u3_assert( _ce_muk_page(a_y) != _ce_muk_page(b_y) );

  memset(a_y, 0xab, _ce_page);
  memcpy(b_y, a_y, _ce_page);
  b_y[0] ^= 1;
  u3_assert( _ce_muk_page(a_y) != _ce_muk_page(b_y) );

  fprintf(stderr, "test_muk_distinct: ok\r\n");
}

/* _test_image_stat_sizes(): _ce_image_stat over empty / aligned / unaligned.
*/
static void
_test_image_stat_sizes(void)
{
  c3_c* dir_c = _tmpdir();
  c3_c  pax_c[8192];
  c3_w  pgs_w;
  u3e_image img_u = { .nam_c = "test", .pgs_w = 0 };

  snprintf(pax_c, sizeof(pax_c), "%s/empty.bin", dir_c);
  img_u.fid_i = c3_open(pax_c, O_RDWR | O_CREAT, 0600);
  u3_assert(-1 != img_u.fid_i);
  u3_assert(_ce_img_good == _ce_image_stat(&img_u, &pgs_w));
  u3_assert(0 == pgs_w);
  close(img_u.fid_i);

  snprintf(pax_c, sizeof(pax_c), "%s/three.bin", dir_c);
  img_u.fid_i = c3_open(pax_c, O_RDWR | O_CREAT, 0600);
  u3_assert(-1 != img_u.fid_i);
  u3_assert(0 == ftruncate(img_u.fid_i, _ce_len(3)));
  u3_assert(_ce_img_good == _ce_image_stat(&img_u, &pgs_w));
  u3_assert(3 == pgs_w);
  close(img_u.fid_i);

  snprintf(pax_c, sizeof(pax_c), "%s/odd.bin", dir_c);
  img_u.fid_i = c3_open(pax_c, O_RDWR | O_CREAT, 0600);
  u3_assert(-1 != img_u.fid_i);
  u3_assert(0 == ftruncate(img_u.fid_i, _ce_len(3) + 1));
  u3_assert(_ce_img_size == _ce_image_stat(&img_u, &pgs_w));
  close(img_u.fid_i);

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_image_stat_sizes: ok\r\n");
}

/* _test_loom_track_bitmap(): _ce_loom_track + _ce_loom_track_sane on the
**   always-64-bit dit_d, straddling word boundaries.
*/
static void
_test_loom_track_bitmap(void)
{
  c3_w sav_pag_w     = u3P.pag_w;
  c3_w sav_img_pgs_w = u3P.img_u.pgs_w;
  c3_w n_w = 70;     //  > 64, lands in word 1
  c3_w m_w = 130;    //  spans two more 64-bit words

  u3_assert(n_w + m_w <= sav_pag_w);

  //  shift literal must produce a real 64-bit value on both bitnesses
  //
  u3_assert( ((c3_d)1 << 63) != 0 );

  u3e_foul();
  _ce_loom_track(n_w, m_w);
  u3P.img_u.pgs_w = n_w;
  u3P.pag_w       = n_w + m_w;

  u3_assert( c3y == _ce_loom_track_sane() );

  //  flip a bit that should be clean
  //
  u3P.dit_d[0] |= ((c3_d)1 << 5);
  u3_assert( c3n == _ce_loom_track_sane() );
  u3P.dit_d[0] &= ~((c3_d)1 << 5);
  u3_assert( c3y == _ce_loom_track_sane() );

  //  flip a bit that should be dirty
  //
  {
    c3_w pag_w = n_w + 50;
    c3_w blk_w = pag_w >> u3a_chub_bits_log;
    c3_w bit_w = pag_w & (u3a_chub_bits - 1);

    u3P.dit_d[blk_w] &= ~((c3_d)1 << bit_w);
    u3_assert( c3n == _ce_loom_track_sane() );
    u3P.dit_d[blk_w] |= ((c3_d)1 << bit_w);
    u3_assert( c3y == _ce_loom_track_sane() );
  }

  u3P.pag_w       = sav_pag_w;
  u3P.img_u.pgs_w = sav_img_pgs_w;
  u3e_foul();

  fprintf(stderr, "test_loom_track_bitmap: ok\r\n");
}

/* _test_patch_control_roundtrip(): write a u3e_control and read it back.
*/
static void
_test_patch_control_roundtrip(void)
{
  c3_c* dir_c = _tmpdir();
  u3_ce_patch wri_u;

  _build_patch(dir_c, &wri_u, 3);
  _close_patch(&wri_u);

  {
    u3_ce_patch rdo_u;
    _open_patch(dir_c, &rdo_u);

    u3_assert( c3y == _ce_patch_read_control(&rdo_u) );
    u3_assert( U3P_VERLAT == rdo_u.con_u->ver_h );
    u3_assert( 3 == rdo_u.con_u->pgs_w );
    u3_assert( 7 == rdo_u.con_u->tot_w );    //  n_w + 4

    for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
      c3_y page_y[_ce_page];
      memset(page_y, 0xa0 + (c3_y)i_w, _ce_page);
      u3_assert( ((i_w * 2) + 1) == rdo_u.con_u->mem_u[i_w].pag_w );
      u3_assert( _ce_muk_page(page_y) == rdo_u.con_u->mem_u[i_w].has_h );
    }

    _close_patch(&rdo_u);
  }

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_control_roundtrip: ok\r\n");
}

/* _test_patch_verify_happy(): a freshly-composed patch verifies clean.
*/
static void
_test_patch_verify_happy(void)
{
  c3_c* dir_c = _tmpdir();
  u3_ce_patch wri_u;

  _build_patch(dir_c, &wri_u, 2);
  _close_patch(&wri_u);

  {
    u3_ce_patch ver_u;
    _open_patch(dir_c, &ver_u);
    u3_assert( c3y == _ce_patch_read_control(&ver_u) );
    u3_assert( c3y == _ce_patch_verify(&ver_u) );
    _close_patch(&ver_u);
  }

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_verify_happy: ok\r\n");
}

/* _test_patch_verify_meta_corruption(): flip a bit in the meta checksum.
*/
static void
_test_patch_verify_meta_corruption(void)
{
  c3_c* dir_c = _tmpdir();
  u3_ce_patch wri_u;

  _build_patch(dir_c, &wri_u, 2);

  {
    c3_h bad_h = wri_u.con_u->has_h ^ (c3_h)1;
    c3_z off_z = offsetof(u3e_control, has_h);
    u3_assert( sizeof(c3_h) == pwrite(wri_u.ctl_i, &bad_h, sizeof(c3_h), off_z) );
  }

  _close_patch(&wri_u);

  {
    u3_ce_patch ver_u;
    _open_patch(dir_c, &ver_u);
    u3_assert( c3y == _ce_patch_read_control(&ver_u) );
    u3_assert( c3n == _ce_patch_verify(&ver_u) );
    _close_patch(&ver_u);
  }

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_verify_meta_corruption: ok\r\n");
}

/* _test_patch_verify_page_corruption(): flip a byte in memory.bin.
**   meta checksum still passes (control unchanged) so the failure is on
**   the per-page checksum branch.
*/
static void
_test_patch_verify_page_corruption(void)
{
  c3_c* dir_c = _tmpdir();
  u3_ce_patch wri_u;

  _build_patch(dir_c, &wri_u, 2);

  {
    c3_y bad_y = 0x5a;
    u3_assert( 1 == pwrite(wri_u.mem_i, &bad_y, 1, 100) );
  }

  _close_patch(&wri_u);

  {
    u3_ce_patch ver_u;
    _open_patch(dir_c, &ver_u);
    u3_assert( c3y == _ce_patch_read_control(&ver_u) );
    u3_assert( c3n == _ce_patch_verify(&ver_u) );
    _close_patch(&ver_u);
  }

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_verify_page_corruption: ok\r\n");
}

/* _test_patch_verify_version_mismatch(): wrong ver_h with valid meta checksum.
*/
static void
_test_patch_verify_version_mismatch(void)
{
  c3_c* dir_c = _tmpdir();
  c3_c  pax_c[8192];
  c3_w  n_w = 1;
  c3_z  len_z = sizeof(u3e_control) + (n_w * sizeof(u3e_line));
  u3e_control* con_u = c3_malloc(len_z);
  c3_y  page_y[_ce_page];
  c3_i  mem_i, ctl_i;

  memset(page_y, 0xab, _ce_page);

  con_u->ver_h = U3P_VERLAT + 1;             //  bad version
  con_u->tot_w = 5;
  con_u->pgs_w = n_w;
  con_u->mem_u[0].pag_w = 0;
  con_u->mem_u[0].has_h = _ce_muk_page(page_y);

  {
    c3_z off_z = offsetof(u3e_control, tot_w);
    con_u->has_h = _ce_muk_buf(len_z - off_z, (c3_y*)con_u + off_z);
  }

  snprintf(pax_c, sizeof(pax_c), "%s/memory.bin", dir_c);
  mem_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != mem_i);
  u3_assert( _ce_page == pwrite(mem_i, page_y, _ce_page, 0) );

  snprintf(pax_c, sizeof(pax_c), "%s/control.bin", dir_c);
  ctl_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != ctl_i);

  {
    u3_ce_patch wri_u = { .ctl_i = ctl_i, .mem_i = mem_i,
                          .sip_w = 0,     .con_u = con_u };
    _ce_patch_write_control(&wri_u);
    _close_patch(&wri_u);
  }

  {
    u3_ce_patch ver_u;
    _open_patch(dir_c, &ver_u);
    u3_assert( c3y == _ce_patch_read_control(&ver_u) );
    u3_assert( c3n == _ce_patch_verify(&ver_u) );
    _close_patch(&ver_u);
  }

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_verify_version_mismatch: ok\r\n");
}

/* _test_patch_verify_truncated_control(): control.bin shorter than declared.
*/
static void
_test_patch_verify_truncated_control(void)
{
  c3_c* dir_c = _tmpdir();
  u3_ce_patch wri_u;

  _build_patch(dir_c, &wri_u, 3);
  u3_assert( 0 == ftruncate(wri_u.ctl_i, sizeof(u3e_control)) );
  _close_patch(&wri_u);

  {
    c3_c pax_c[8192];
    u3_ce_patch ver_u = { .ctl_i = -1, .mem_i = -1, .sip_w = 0, .con_u = 0 };
    snprintf(pax_c, sizeof(pax_c), "%s/control.bin", dir_c);
    ver_u.ctl_i = c3_open(pax_c, O_RDWR);
    u3_assert(-1 != ver_u.ctl_i);
    u3_assert( c3n == _ce_patch_read_control(&ver_u) );
    close(ver_u.ctl_i);
  }

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_verify_truncated_control: ok\r\n");
}

/* _test_patch_apply_roundtrip(): _ce_patch_apply writes the right bytes
**   into the right pages of image.bin.
*/
static void
_test_patch_apply_roundtrip(void)
{
  c3_c*     dir_c = _tmpdir();
  c3_c      pax_c[8192];
  u3e_image sav_u = u3P.img_u;
  c3_i      fid_i;
  c3_y      page_a[_ce_page], page_b[_ce_page];
  c3_y      zero_y[_ce_page];
  c3_y      check_y[_ce_page];

  memset(page_a, 0x11, _ce_page);
  memset(page_b, 0x22, _ce_page);
  memset(zero_y, 0x00, _ce_page);

  snprintf(pax_c, sizeof(pax_c), "%s/image.bin", dir_c);
  fid_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != fid_i);
  u3_assert( 0 == ftruncate(fid_i, _ce_len(4)) );

  u3P.img_u.nam_c = "image";
  u3P.img_u.fid_i = fid_i;
  u3P.img_u.pgs_w = 4;

  //  build a patch updating pages 1 and 3
  //
  c3_w n_w = 2;
  c3_z len_z = sizeof(u3e_control) + (n_w * sizeof(u3e_line));
  u3e_control* con_u = c3_malloc(len_z);
  con_u->ver_h = U3P_VERLAT;
  con_u->tot_w = 4;
  con_u->pgs_w = n_w;
  con_u->mem_u[0].pag_w = 1;
  con_u->mem_u[0].has_h = _ce_muk_page(page_a);
  con_u->mem_u[1].pag_w = 3;
  con_u->mem_u[1].has_h = _ce_muk_page(page_b);
  {
    c3_z off_z = offsetof(u3e_control, tot_w);
    con_u->has_h = _ce_muk_buf(len_z - off_z, (c3_y*)con_u + off_z);
  }

  snprintf(pax_c, sizeof(pax_c), "%s/memory.bin", dir_c);
  c3_i mem_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != mem_i);
  u3_assert( _ce_page == pwrite(mem_i, page_a, _ce_page, _ce_len(0)) );
  u3_assert( _ce_page == pwrite(mem_i, page_b, _ce_page, _ce_len(1)) );

  snprintf(pax_c, sizeof(pax_c), "%s/control.bin", dir_c);
  c3_i ctl_i = c3_open(pax_c, O_RDWR | O_CREAT | O_EXCL, 0600);
  u3_assert(-1 != ctl_i);

  {
    u3_ce_patch pat_u = { .ctl_i = ctl_i, .mem_i = mem_i,
                          .sip_w = 0,     .con_u = con_u };
    _ce_patch_apply(&pat_u);
  }

  u3_assert( 4 == u3P.img_u.pgs_w );

  u3_assert( _ce_page == pread(fid_i, check_y, _ce_page, _ce_len(0)) );
  u3_assert( 0 == memcmp(check_y, zero_y, _ce_page) );
  u3_assert( _ce_page == pread(fid_i, check_y, _ce_page, _ce_len(1)) );
  u3_assert( 0 == memcmp(check_y, page_a, _ce_page) );
  u3_assert( _ce_page == pread(fid_i, check_y, _ce_page, _ce_len(2)) );
  u3_assert( 0 == memcmp(check_y, zero_y, _ce_page) );
  u3_assert( _ce_page == pread(fid_i, check_y, _ce_page, _ce_len(3)) );
  u3_assert( 0 == memcmp(check_y, page_b, _ce_page) );

  c3_free(con_u);
  close(ctl_i);
  close(mem_i);
  close(fid_i);

  u3P.img_u = sav_u;

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_patch_apply_roundtrip: ok\r\n");
}

/* _test_save_dryrun(): u3e_save under u3o_dryrun is a no-op.
*/
static void
_test_save_dryrun(void)
{
  c3_c*  dir_c     = _tmpdir();
  c3_c   pax_c[8192];
  c3_h   sav_wag_h = u3C.wag_h;
  c3_c*  sav_dir_c = u3P.dir_c;
  struct stat buf_u;

  u3P.dir_c   = dir_c;
  u3C.wag_h  |= u3o_dryrun;

  u3e_save(0, 0);

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/chk/control.bin", dir_c);
  u3_assert( -1 == stat(pax_c, &buf_u) );
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/chk/memory.bin", dir_c);
  u3_assert( -1 == stat(pax_c, &buf_u) );

  u3C.wag_h = sav_wag_h;
  u3P.dir_c = sav_dir_c;

  _rmtree(dir_c);
  c3_free(dir_c);
  fprintf(stderr, "test_save_dryrun: ok\r\n");
}

int
main(int argc, char* argv[])
{
  (void)argc; (void)argv;

#ifdef VERE64
  _setup(32);   //  4 GiB virtual loom (8-byte words)
#else
  _setup(28);   //  1 GiB virtual loom (4-byte words)
#endif

  _test_muk_determinism();
  _test_muk_distinct();
  _test_image_stat_sizes();
  _test_loom_track_bitmap();
  _test_patch_control_roundtrip();
  _test_patch_verify_happy();
  _test_patch_verify_meta_corruption();
  _test_patch_verify_page_corruption();
  _test_patch_verify_version_mismatch();
  _test_patch_verify_truncated_control();
  _test_patch_apply_roundtrip();
  _test_save_dryrun();

  fprintf(stderr, "events okeedokee\r\n");
  return 0;
}
