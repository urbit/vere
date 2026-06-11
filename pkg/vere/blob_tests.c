/// @file

#include "blob.h"
#include "noun.h"
#include "vere.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Tests for pkg/vere/blob.c — the content-addressed blob store.
**
** Each test creates a fresh temp pier under /tmp/vere-blob-test-XXXXXX
** and tears it down on exit.  Tests run sequentially and exit nonzero
** on first failure.
*/

static c3_c _tmp_pier[1024];

/* _setup(): init loom (for u3_blob_load), make fresh temp pier.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);

  //  hot jet state, required by the meld path (u3j_boot/u3j_ream)
  //
  u3j_boot(c3y);
}

static void
_tmp_make(void)
{
  snprintf(_tmp_pier, sizeof(_tmp_pier), "/tmp/vere-blob-test-XXXXXX");
  if ( !mkdtemp(_tmp_pier) ) {
    fprintf(stderr, "blob_tests: mkdtemp failed: %s\r\n", strerror(errno));
    exit(1);
  }

  //  blob.c assumes .urb exists (created earlier by disk code in production).
  //  Create it here so u3_blob_init can mkdir .urb/bob.
  //
  c3_c urb_c[2048];
  snprintf(urb_c, sizeof(urb_c), "%s/.urb", _tmp_pier);
  if ( 0 != mkdir(urb_c, 0700) ) {
    fprintf(stderr, "blob_tests: mkdir %s failed: %s\r\n", urb_c, strerror(errno));
    exit(1);
  }
}

static void
_tmp_clean(void)
{
  c3_c cmd_c[2048];
  snprintf(cmd_c, sizeof(cmd_c), "rm -rf %s", _tmp_pier);
  (void)system(cmd_c);
}

/* _path_exists(): true if [pax_c] exists on the filesystem.
*/
static c3_o
_path_exists(const c3_c* pax_c)
{
  struct stat st_u;
  return ( 0 == stat(pax_c, &st_u) ) ? c3y : c3n;
}

/* _write_tmp_file(): create a temp file under $pier/.urb/bob/stg/ with
**   [dat_y] bytes; returns path (malloc'd, caller frees).
*/
static c3_c*
_write_tmp_file(const c3_y* dat_y, c3_d len_d)
{
  c3_c tmpl_c[2048];
  snprintf(tmpl_c, sizeof(tmpl_c),
           "%s/.urb/bob/stg/blob-test-XXXXXX", _tmp_pier);
  c3_i fid_i = mkstemp(tmpl_c);
  if ( -1 == fid_i ) {
    fprintf(stderr, "blob_tests: mkstemp(%s) failed: %s\r\n",
            tmpl_c, strerror(errno));
    return 0;
  }
  if ( len_d && (ssize_t)len_d != write(fid_i, dat_y, (size_t)len_d) ) {
    close(fid_i);
    return 0;
  }
  close(fid_i);
  return strdup(tmpl_c);
}

/* _test_init(): u3_blob_init + u3_blob_stg_init create expected dirs.
*/
static void
_test_init(void)
{
  _tmp_make();

  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  c3_c pax_c[2048];
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob", _tmp_pier);
  if ( c3y != _path_exists(pax_c) ) {
    fprintf(stderr, "\033[31mblob init fail: %s missing\033[0m\r\n", pax_c);
    exit(1);
  }
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/stg", _tmp_pier);
  if ( c3y != _path_exists(pax_c) ) {
    fprintf(stderr, "\033[31mblob init fail: %s missing\033[0m\r\n", pax_c);
    exit(1);
  }

  //  idempotent: second call should not error
  //
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  _tmp_clean();
  fprintf(stderr, "test blob init: ok\r\n");
}

/* _test_stg_clean(): u3_blob_stg_init clears leftover staging files.
*/
static void
_test_stg_clean(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  //  drop a dummy file in staging
  //
  c3_c stg_c[2048];
  snprintf(stg_c, sizeof(stg_c), "%s/.urb/bob/stg/leftover", _tmp_pier);
  FILE* f = fopen(stg_c, "wb");
  if ( !f ) {
    fprintf(stderr, "\033[31mblob stg_clean setup fail\033[0m\r\n");
    exit(1);
  }
  fputs("junk", f);
  fclose(f);

  if ( c3y != _path_exists(stg_c) ) {
    fprintf(stderr, "\033[31mblob stg_clean: file not created\033[0m\r\n");
    exit(1);
  }

  //  re-init should clean it
  //
  u3_blob_stg_init(_tmp_pier);

  if ( c3y == _path_exists(stg_c) ) {
    fprintf(stderr, "\033[31mblob stg_clean fail: %s still exists\033[0m\r\n",
            stg_c);
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob stg_clean: ok\r\n");
}

/* _test_path(): u3_blob_path produces expected string.
*/
static void
_test_path(void)
{
  c3_c pax_c[8192];
  u3_blob_path(pax_c, "/pier", 0x12345678, 42);

  const c3_c* exp_c = "/pier/.urb/bob/305419896/42";
  if ( 0 != strcmp(pax_c, exp_c) ) {
    fprintf(stderr, "\033[31mblob path fail: got %s, expected %s\033[0m\r\n",
            pax_c, exp_c);
    exit(1);
  }
  fprintf(stderr, "test blob path: ok\r\n");
}

/* _test_save_load(): save bytes, load, verify content.
*/
static void
_test_save_load(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "the quick brown fox jumps over the lazy dog";
  const c3_d dat_d   = sizeof(dat_y) - 1;  // drop trailing NUL
  c3_h mug_h = 0;
  c3_h seq_h = 0;

  if ( c3y != u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob save fail\033[0m\r\n");
    exit(1);
  }
  if ( 1 != seq_h ) {
    fprintf(stderr, "\033[31mblob save: expected seq=1, got %" PRIc3_h "\033[0m\r\n",
            seq_h);
    exit(1);
  }

  //  file should exist at computed path
  //
  c3_c fil_c[8192];
  u3_blob_path(fil_c, _tmp_pier, mug_h, seq_h);
  if ( c3y != _path_exists(fil_c) ) {
    fprintf(stderr, "\033[31mblob save: %s missing\033[0m\r\n", fil_c);
    exit(1);
  }

  if ( c3y != u3_blob_live(_tmp_pier, mug_h, seq_h) ) {
    fprintf(stderr, "\033[31mblob exists fail\033[0m\r\n");
    exit(1);
  }

  //  load and verify bytes
  //
  u3_weak atm = u3_blob_load(_tmp_pier, mug_h, seq_h);
  if ( u3_none == atm ) {
    fprintf(stderr, "\033[31mblob load: u3_none\033[0m\r\n");
    exit(1);
  }
  if ( dat_d != u3r_met(3, atm) ) {
    fprintf(stderr, "\033[31mblob load: met mismatch\033[0m\r\n");
    exit(1);
  }
  c3_y* buf_y = c3_malloc(dat_d);
  u3r_bytes(0, (c3_w)dat_d, buf_y, atm);
  if ( 0 != memcmp(buf_y, dat_y, dat_d) ) {
    fprintf(stderr, "\033[31mblob load: byte mismatch\033[0m\r\n");
    exit(1);
  }
  c3_free(buf_y);
  u3z(atm);

  _tmp_clean();
  fprintf(stderr, "test blob save+load: ok\r\n");
}

/* _test_dedup(): saving identical content twice reuses the first seq.
*/
static void
_test_dedup(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "dedup me, please and thank you";
  const c3_d dat_d   = sizeof(dat_y) - 1;

  c3_h mug1_h, mug2_h;
  c3_h seq1_h, seq2_h;

  if ( c3y != u3_blob_save(_tmp_pier, dat_y, dat_d, &mug1_h, &seq1_h) ) {
    fprintf(stderr, "\033[31mblob dedup: first save failed\033[0m\r\n");
    exit(1);
  }
  if ( c3y != u3_blob_save(_tmp_pier, dat_y, dat_d, &mug2_h, &seq2_h) ) {
    fprintf(stderr, "\033[31mblob dedup: second save failed\033[0m\r\n");
    exit(1);
  }

  if ( mug1_h != mug2_h ) {
    fprintf(stderr, "\033[31mblob dedup: mug changed (%" PRIc3_h
                    " vs %" PRIc3_h ")\033[0m\r\n", mug1_h, mug2_h);
    exit(1);
  }
  if ( seq1_h != seq2_h ) {
    fprintf(stderr, "\033[31mblob dedup: expected seq reuse, "
                    "got %" PRIc3_h "+%" PRIc3_h "\033[0m\r\n", seq1_h, seq2_h);
    exit(1);
  }

  //  distinct content → distinct blob slot (may reuse bucket only if mug
  //  collides; overwhelmingly unlikely for ASCII content)
  //
  const c3_y alt_y[] = "a completely different payload";
  const c3_d alt_d   = sizeof(alt_y) - 1;
  c3_h mug3_h = 0;
  c3_h seq3_h = 0;
  if ( c3y != u3_blob_save(_tmp_pier, alt_y, alt_d, &mug3_h, &seq3_h) ) {
    fprintf(stderr, "\033[31mblob dedup: alt save failed\033[0m\r\n");
    exit(1);
  }
  if (  mug1_h == mug3_h
     && seq1_h == seq3_h )
  {
    fprintf(stderr, "\033[31mblob dedup: distinct content got same blob\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob dedup: ok\r\n");
}

/* _test_save_fd(): u3_blob_save_fd round-trips via mmap path.
*/
static void
_test_save_fd(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "content delivered via file descriptor";
  const c3_d dat_d   = sizeof(dat_y) - 1;

  //  write a real file and re-open for reading
  //
  c3_c src_c[2048];
  snprintf(src_c, sizeof(src_c), "%s/src", _tmp_pier);
  FILE* f = fopen(src_c, "wb");
  fwrite(dat_y, 1, (size_t)dat_d, f);
  fclose(f);

  c3_i fid_i = open(src_c, O_RDONLY);
  if ( -1 == fid_i ) {
    fprintf(stderr, "\033[31mblob save_fd: open failed\033[0m\r\n");
    exit(1);
  }

  c3_h mug_h = 0;
  c3_h seq_h = 0;
  c3_o ret_o = u3_blob_save_fd(_tmp_pier, fid_i, dat_d, &mug_h, &seq_h);
  close(fid_i);

  if ( c3y != ret_o ) {
    fprintf(stderr, "\033[31mblob save_fd failed\033[0m\r\n");
    exit(1);
  }

  //  u3_blob_save_fd rejects empty files
  //
  FILE* ef = fopen(src_c, "wb");
  fclose(ef);  //  truncate to zero
  c3_i efid_i = open(src_c, O_RDONLY);
  c3_h emh = 0;
  c3_h esh = 0;
  if ( c3n != u3_blob_save_fd(_tmp_pier, efid_i, 0, &emh, &esh) ) {
    fprintf(stderr, "\033[31mblob save_fd: should reject empty\033[0m\r\n");
    exit(1);
  }
  close(efid_i);

  //  verify loaded content matches
  //
  u3_weak atm = u3_blob_load(_tmp_pier, mug_h, seq_h);
  if ( u3_none == atm ) {
    fprintf(stderr, "\033[31mblob save_fd: load u3_none\033[0m\r\n");
    exit(1);
  }
  c3_y* buf_y = c3_malloc(dat_d);
  u3r_bytes(0, (c3_w)dat_d, buf_y, atm);
  if ( 0 != memcmp(buf_y, dat_y, dat_d) ) {
    fprintf(stderr, "\033[31mblob save_fd: byte mismatch\033[0m\r\n");
    exit(1);
  }
  c3_free(buf_y);
  u3z(atm);

  _tmp_clean();
  fprintf(stderr, "test blob save_fd: ok\r\n");
}

/* _test_walk(): u3_blob_walk reports every installed blob exactly once,
**   skipping the staging dir, lockfiles, and junk entries.
*/
typedef struct {
  c3_d bid_d[8];
  c3_z len_z;
} _walk_acc;

static void
_test_walk_cb(void* ptr_v, c3_h mug_h, c3_h seq_h)
{
  _walk_acc* acc_u = ptr_v;
  if ( acc_u->len_z < 8 ) {
    acc_u->bid_d[acc_u->len_z] = ((c3_d)mug_h << 32) | (c3_d)seq_h;
  }
  acc_u->len_z++;
}

static c3_o
_walk_acc_has(_walk_acc* acc_u, c3_h mug_h, c3_h seq_h)
{
  c3_d bid_d = ((c3_d)mug_h << 32) | (c3_d)seq_h;
  for ( c3_z i_z = 0; i_z < acc_u->len_z && i_z < 8; i_z++ ) {
    if ( acc_u->bid_d[i_z] == bid_d ) return c3y;
  }
  return c3n;
}

static void
_test_walk(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y one_y[] = "walk blob one";
  const c3_y two_y[] = "walk blob two";
  const c3_y tri_y[] = "walk blob three";
  c3_h mug_h[3] = {0};
  c3_h seq_h[3] = {0};

  u3_blob_save(_tmp_pier, one_y, sizeof(one_y) - 1, &mug_h[0], &seq_h[0]);
  u3_blob_save(_tmp_pier, two_y, sizeof(two_y) - 1, &mug_h[1], &seq_h[1]);
  u3_blob_save(_tmp_pier, tri_y, sizeof(tri_y) - 1, &mug_h[2], &seq_h[2]);

  //  plant junk the walk must ignore: a leftover staging file and a
  //  non-numeric entry in the store root
  //
  {
    c3_c pax_c[8192];
    snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/stg/leftover", _tmp_pier);
    FILE* fil_u = fopen(pax_c, "w");
    if ( fil_u ) { fputs("junk", fil_u); fclose(fil_u); }

    snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/notanumber", _tmp_pier);
    fil_u = fopen(pax_c, "w");
    if ( fil_u ) { fputs("junk", fil_u); fclose(fil_u); }
  }

  _walk_acc acc_u = {0};
  u3_blob_walk(_tmp_pier, &acc_u, _test_walk_cb);

  if ( 3 != acc_u.len_z ) {
    fprintf(stderr, "\033[31mblob walk: expected 3 files, got %zu\033[0m\r\n",
            acc_u.len_z);
    exit(1);
  }

  for ( c3_z i_z = 0; i_z < 3; i_z++ ) {
    if ( c3n == _walk_acc_has(&acc_u, mug_h[i_z], seq_h[i_z]) ) {
      fprintf(stderr, "\033[31mblob walk: missing %" PRIc3_h "/%" PRIc3_h
                      "\033[0m\r\n", mug_h[i_z], seq_h[i_z]);
      exit(1);
    }
  }

  //  after wiping one blob, the walk reports exactly the other two
  //
  u3_blob_wipe(_tmp_pier, mug_h[0], seq_h[0]);

  memset(&acc_u, 0, sizeof(acc_u));
  u3_blob_walk(_tmp_pier, &acc_u, _test_walk_cb);

  if (  (2 != acc_u.len_z)
     || (c3y == _walk_acc_has(&acc_u, mug_h[0], seq_h[0])) )
  {
    fprintf(stderr, "\033[31mblob walk: bad post-wipe result\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob walk: ok\r\n");
}

/* _test_delete_empty_bucket(): delete removes file AND empty bucket.
*/
static void
_test_delete_empty_bucket(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "ephemeral blob";
  c3_h mug_h = 0;
  c3_h seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  c3_c fil_c[8192], dir_c[8192];
  u3_blob_path(fil_c, _tmp_pier, mug_h, seq_h);
  snprintf(dir_c, sizeof(dir_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);

  if ( c3y != _path_exists(dir_c) ) {
    fprintf(stderr, "\033[31mblob delete setup: bucket missing\033[0m\r\n");
    exit(1);
  }

  u3_blob_wipe(_tmp_pier, mug_h, seq_h);

  if ( c3y == _path_exists(fil_c) ) {
    fprintf(stderr, "\033[31mblob delete: file %s still exists\033[0m\r\n",
            fil_c);
    exit(1);
  }
  if ( c3y == u3_blob_live(_tmp_pier, mug_h, seq_h) ) {
    fprintf(stderr, "\033[31mblob delete: exists() still true\033[0m\r\n");
    exit(1);
  }
  if ( c3y == _path_exists(dir_c) ) {
    fprintf(stderr, "\033[31mblob delete: bucket %s not cleaned\033[0m\r\n",
            dir_c);
    exit(1);
  }

  //  deleting a nonexistent blob is a no-op (no error)
  //
  u3_blob_wipe(_tmp_pier, 0xdeadbeef, 999);

  _tmp_clean();
  fprintf(stderr, "test blob delete (empty bucket): ok\r\n");
}

/* _test_install_stg(): stage a file → install → rename, blob exists.
*/
static void
_test_install_stg(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "payload installed from staging";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_c* stg_c = _write_tmp_file(dat_y, dat_d);
  if ( !stg_c ) {
    fprintf(stderr, "\033[31mblob install_stg: _write_tmp_file failed\033[0m\r\n");
    exit(1);
  }

  //  staging file is present before install
  //
  if ( c3y != _path_exists(stg_c) ) {
    fprintf(stderr, "\033[31mblob install_stg: staging file missing\033[0m\r\n");
    exit(1);
  }

  c3_h mug_h = 0;
  c3_h seq_h = 0;
  if ( c3y != u3_blob_move_stg(_tmp_pier, stg_c, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob install_stg failed\033[0m\r\n");
    exit(1);
  }
  if ( 1 != seq_h ) {
    fprintf(stderr, "\033[31mblob install_stg: expected seq=1, got %" PRIc3_h
                    "\033[0m\r\n", seq_h);
    exit(1);
  }

  //  staging file is consumed after install
  //
  if ( c3y == _path_exists(stg_c) ) {
    fprintf(stderr, "\033[31mblob install_stg: staging file not consumed\033[0m\r\n");
    exit(1);
  }

  if ( c3y != u3_blob_live(_tmp_pier, mug_h, seq_h) ) {
    fprintf(stderr, "\033[31mblob install_stg: blob not present after install\033[0m\r\n");
    exit(1);
  }

  //  content preserved
  //
  u3_weak atm = u3_blob_load(_tmp_pier, mug_h, seq_h);
  if ( u3_none == atm ) {
    fprintf(stderr, "\033[31mblob install_stg: load u3_none\033[0m\r\n");
    exit(1);
  }
  c3_y* buf_y = c3_malloc(dat_d);
  u3r_bytes(0, (c3_w)dat_d, buf_y, atm);
  if ( 0 != memcmp(buf_y, dat_y, dat_d) ) {
    fprintf(stderr, "\033[31mblob install_stg: byte mismatch\033[0m\r\n");
    exit(1);
  }
  c3_free(buf_y);
  u3z(atm);

  free(stg_c);
  _tmp_clean();
  fprintf(stderr, "test blob install_stg: ok\r\n");
}

/* _test_install_stg_dedup(): installing a staging file with existing
**   content returns the existing seq and consumes the staging file.
*/
static void
_test_install_stg_dedup(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "shared bytes across save + install_stg";
  const c3_d dat_d   = sizeof(dat_y) - 1;

  //  first save via u3_blob_save
  //
  c3_h mug1_h = 0;
  c3_h seq1_h = 0;
  u3_blob_save(_tmp_pier, dat_y, dat_d, &mug1_h, &seq1_h);

  //  then stage same content and install
  //
  c3_c* stg_c = _write_tmp_file(dat_y, dat_d);

  c3_h mug2_h = 0;
  c3_h seq2_h = 0;
  if ( c3y != u3_blob_move_stg(_tmp_pier, stg_c, &mug2_h, &seq2_h) ) {
    fprintf(stderr, "\033[31mblob install_stg dedup: install failed\033[0m\r\n");
    exit(1);
  }

  if ( mug1_h != mug2_h || seq1_h != seq2_h ) {
    fprintf(stderr, "\033[31mblob install_stg dedup: expected %"
                    PRIc3_h "/%" PRIc3_h ", got %" PRIc3_h "/%" PRIc3_h
                    "\033[0m\r\n", mug1_h, seq1_h, mug2_h, seq2_h);
    exit(1);
  }

  //  staging file consumed even on dedup
  //
  if ( c3y == _path_exists(stg_c) ) {
    fprintf(stderr, "\033[31mblob install_stg dedup: staging file not consumed\033[0m\r\n");
    exit(1);
  }

  //  reject missing and empty staging files
  //
  c3_h m = 0; c3_h s = 0;
  if ( c3n != u3_blob_move_stg(_tmp_pier, "/no/such/path", &m, &s) ) {
    fprintf(stderr, "\033[31mblob install_stg: should reject missing file\033[0m\r\n");
    exit(1);
  }

  c3_c* empty_c = _write_tmp_file((const c3_y*)"", 0);
  if ( c3n != u3_blob_move_stg(_tmp_pier, empty_c, &m, &s) ) {
    fprintf(stderr, "\033[31mblob install_stg: should reject empty\033[0m\r\n");
    exit(1);
  }
  unlink(empty_c);
  free(empty_c);

  free(stg_c);
  _tmp_clean();
  fprintf(stderr, "test blob install_stg dedup: ok\r\n");
}

/* _test_met(): u3_blob_met matches u3r_met on the materialized atom.
*/

/* _test_blob_del_cb(): test blob_del_f — count deletion requests.
*/
static c3_w _test_del_count_w;

static void
_test_blob_del_cb(c3_h mug_h, c3_h seq_h)
{
  (void)mug_h; (void)seq_h;
  _test_del_count_w += 1;
}

/* _test_sane(): u3a_blob_sane catches counter corruption.
*/
static void
_test_sane(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;
  u3C.blob_del_f = _test_blob_del_cb;

  const c3_y dat_y[] = "sane test blob";
  c3_h mug_h = 0, seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  //  one live atom in the kernel root, plus a synthetic log ref
  //
  u3_noun bob = u3i_blob(mug_h, seq_h);
  u3_noun old = u3A->roc;
  u3A->roc    = u3nc(bob, u3_nul);

  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  blb_u->eve_w += 1;
  blb_u->use_w += 1;

  if ( c3y != u3a_blob_sane(c3y) ) {
    fprintf(stderr, "\033[31msane: balanced bank reported corrupt\033[0m\r\n");
    exit(1);
  }

  //  cheap tier: use_w below the durable floor
  //
  blb_u->use_w -= 2;
  if ( c3n != u3a_blob_sane(c3n) ) {
    fprintf(stderr, "\033[31msane: use < eve+les not caught\033[0m\r\n");
    exit(1);
  }

  //  deep tier: counters look plausible but cardinality is wrong
  //
  blb_u->use_w += 3;   //  use = eve + les + 2, only 1 live atom
  if ( c3n != u3a_blob_sane(c3y) ) {
    fprintf(stderr, "\033[31msane: cardinality mismatch not caught\033[0m\r\n");
    exit(1);
  }
  blb_u->use_w -= 1;

  //  cleanup: drop the atom, then the entry
  //
  u3z(u3A->roc);
  u3A->roc = old;

  if ( _test_del_count_w ) {
    fprintf(stderr, "\033[31msane: blob deleted with eve_w held\033[0m\r\n");
    exit(1);
  }

  blb_u = u3a_blob_get(mug_h, seq_h);
  blb_u->eve_w = 0;
  blb_u->use_w = 0;
  u3a_blob_drop(mug_h, seq_h);

  u3C.blob_del_f = 0;
  _tmp_clean();
  fprintf(stderr, "test blob sane: ok\r\n");
}

/* _test_meld(): |meld unifies duplicate bob atoms and preserves the bank.
*/
static void
_test_meld(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;
  u3C.blob_del_f = _test_blob_del_cb;
  _test_del_count_w = 0;

  const c3_y dat_y[] = "meld test blob";
  c3_h mug_h = 0, seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  //  two distinct bob atoms for the same bid, both in the kernel root,
  //  plus a synthetic log ref: use = eve(1) + cardinality(2) = 3
  //
  u3_noun bo1 = u3i_blob(mug_h, seq_h);
  u3_noun bo2 = u3i_blob(mug_h, seq_h);
  u3_noun old = u3A->roc;
  u3A->roc    = u3nc(bo1, bo2);

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    blb_u->eve_w += 1;
    blb_u->use_w += 1;

    if ( 3 != blb_u->use_w ) {
      fprintf(stderr, "\033[31mmeld: setup use_w %" PRIc3_w " != 3\033[0m\r\n",
              blb_u->use_w);
      exit(1);
    }
  }

  (void)u3_meld_all(0, c3n, c3n);

  //  the duplicates must be unified (one atom box), the freed copy's
  //  cardinality decremented, the bank entry and log ref intact, and
  //  the surviving atom's blob pointer valid post-pack
  //
  {
    u3a_cell* cel_u = u3a_to_ptr(u3A->roc);
    if ( cel_u->hed != cel_u->tel ) {
      fprintf(stderr, "\033[31mmeld: duplicate bobs not unified\033[0m\r\n");
      exit(1);
    }
  }

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    if ( !blb_u ) {
      fprintf(stderr, "\033[31mmeld: bank entry lost\033[0m\r\n");
      exit(1);
    }
    if ( (2 != blb_u->use_w) || (1 != blb_u->eve_w) ) {
      fprintf(stderr, "\033[31mmeld: counts use=%" PRIc3_w " eve=%" PRIc3_w
                      " want use=2 eve=1\033[0m\r\n",
              blb_u->use_w, blb_u->eve_w);
      exit(1);
    }
    if (  (mug_h != u3a_bob_mug(u3h(u3A->roc)))
       || (seq_h != u3a_bob_seq(u3h(u3A->roc))) )
    {
      fprintf(stderr, "\033[31mmeld: bob blob pointer stale\033[0m\r\n");
      exit(1);
    }
  }

  if ( _test_del_count_w ) {
    fprintf(stderr, "\033[31mmeld: blob deleted with refs held\033[0m\r\n");
    exit(1);
  }

  //  drop the last atom: cardinality reaches 0, but eve_w must keep
  //  the file alive (no deletion request)
  //
  u3z(u3A->roc);
  u3A->roc = old;

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    if ( (1 != blb_u->use_w) || _test_del_count_w ) {
      fprintf(stderr, "\033[31mmeld: post-drop use=%" PRIc3_w " del=%" PRIc3_w
                      "\033[0m\r\n", blb_u->use_w, _test_del_count_w);
      exit(1);
    }

    //  release the log ref: now deletion is legitimate
    //
    blb_u->eve_w = 0;
    blb_u->use_w = 0;
    u3a_blob_drop(mug_h, seq_h);
  }

  u3C.blob_del_f = 0;
  _tmp_clean();
  fprintf(stderr, "test blob meld: ok\r\n");
}

static void
_test_met(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  //  case 1: no trailing zeros, not word-aligned (4 bytes, < 1 VERE64 word)
  //
  //  bytes: { 0xab, 0xcd, 0xef, 0x01 }
  //  last nonzero byte at pos 4, top byte = 0x01 (1 significant bit)
  //  expected met = (4-1)*8 + 1 = 25
  //
  //  Also verifies u3_blob_load zero-initializes the loom atom's trailing
  //  word bytes: u3r_met on the loaded atom must agree with u3_blob_met.
  //
  {
    const c3_y dat_y[] = { 0xab, 0xcd, 0xef, 0x01 };
    const c3_d dat_d   = sizeof(dat_y);
    c3_h mug_h = 0; c3_h seq_h = 0;
    u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h);

    c3_d bit_d = u3_blob_met(_tmp_pier, mug_h, seq_h);
    if ( 25 != bit_d ) {
      fprintf(stderr, "\033[31mblob met: dense got %" PRIc3_d ", expected 25"
                      "\033[0m\r\n", bit_d);
      exit(1);
    }

    u3_weak atm = u3_blob_load(_tmp_pier, mug_h, seq_h);
    if ( u3_none == atm ) {
      fprintf(stderr, "\033[31mblob met: load u3_none\033[0m\r\n");
      exit(1);
    }
    c3_w ref_w = u3r_met(0, atm);
    u3z(atm);
    if ( bit_d != (c3_d)ref_w ) {
      fprintf(stderr, "\033[31mblob met: blob_met=%" PRIc3_d
                      " != u3r_met=%" PRIc3_w " (u3_blob_load "
                      "not zero-initializing trailing bytes?)\033[0m\r\n",
              bit_d, ref_w);
      exit(1);
    }
  }

  //  case 2: trailing zeros — met should strip them
  //
  {
    const c3_y dat_y[] = { 0xff, 0xff, 0x00, 0x00, 0x00 };
    const c3_d dat_d   = sizeof(dat_y);
    c3_h mug_h = 0; c3_h seq_h = 0;
    u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h);

    c3_d bit_d = u3_blob_met(_tmp_pier, mug_h, seq_h);
    //  16 significant bits; high byte 0xff → 8 bits
    //  total = 1*8 + 8 = 16
    //
    if ( 16 != bit_d ) {
      fprintf(stderr, "\033[31mblob met: trailing-zero got %" PRIc3_d
                      ", expected 16\033[0m\r\n", bit_d);
      exit(1);
    }
  }

  //  case 3: nonexistent blob → 0
  //
  {
    if ( 0 != u3_blob_met(_tmp_pier, 0xdeadbeef, 999) ) {
      fprintf(stderr, "\033[31mblob met: missing blob should return 0\033[0m\r\n");
      exit(1);
    }
  }

  _tmp_clean();
  fprintf(stderr, "test blob met: ok\r\n");
}

/* _test_map(): u3_blob_mmap returns byte-accurate pointer.
*/
static void
_test_map(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "mapped bytes should round-trip exactly";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h);

  c3_d mlen_d = 0;
  const c3_y* map_y = u3_blob_mmap(_tmp_pier, mug_h, seq_h, &mlen_d);
  if ( !map_y ) {
    fprintf(stderr, "\033[31mblob map: returned NULL\033[0m\r\n");
    exit(1);
  }
  if ( mlen_d != dat_d ) {
    fprintf(stderr, "\033[31mblob map: len %" PRIc3_d " != %" PRIc3_d
                    "\033[0m\r\n", mlen_d, dat_d);
    exit(1);
  }
  if ( 0 != memcmp(map_y, dat_y, dat_d) ) {
    fprintf(stderr, "\033[31mblob map: byte mismatch\033[0m\r\n");
    exit(1);
  }
  u3_blob_umap(map_y, mlen_d);

  //  mapping nonexistent returns NULL
  //
  c3_d dlen_d = 0;
  const c3_y* miss_y = u3_blob_mmap(_tmp_pier, 0xdeadbeef, 999, &dlen_d);
  if ( miss_y ) {
    fprintf(stderr, "\033[31mblob map: missing should be NULL\033[0m\r\n");
    u3_blob_umap(miss_y, dlen_d);
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob map: ok\r\n");
}

/* _test_lifecycle(): end-to-end exercise of the blob-as-atom pipeline.
**
**   Simulates the production flow:
**     1. Earth writes bytes to the blob store (save or install_stg).
**     2. Mars constructs bob atoms referencing those blobs.
**     3. The event containing the bob atoms is serialized with u3s_ram_xeno
**        (as happens when writing to the event log or sending over newt IPC).
**     4. On replay (or IPC receive), u3s_tap_xeno reconstructs the bob atoms.
**     5. Arvo reads the atoms' bytes via u3r_bytes, which re-materializes
**        each bob atom through u3r_blob_load → u3_blob_load → mmap.
**
**   Also exercises the ram encoder's backref path for bob atoms (same bob
**   appearing multiple times in a noun) and the dedup path through
**   install_stg (two bob atoms resolving to the same on-disk blob).
*/
static void
_test_lifecycle(void)
{
  _tmp_make();
  u3_blob_init(_tmp_pier);
  u3_blob_stg_init(_tmp_pier);

  //  u3s_ram_xeno calls u3r_blob_met on bob atoms during encoding, which
  //  reads the blob file at $u3C.dir_c/.urb/bob/<mug>/<seq>.  Set it now
  //  so encode, decode, and u3r_bytes all find the same store.
  //
  u3C.dir_c = _tmp_pier;

  //  write two distinct blobs directly, plus a third via install_stg
  //  that should dedup against the first.
  //
  const c3_y dat1_y[] = "lifecycle: first blob contents";
  const c3_d dat1_d   = sizeof(dat1_y) - 1;
  const c3_y dat2_y[] = "lifecycle: entirely different payload here";
  const c3_d dat2_d   = sizeof(dat2_y) - 1;

  c3_h mug1_h = 0, mug2_h = 0, mug3_h = 0;
  c3_h seq1_h = 0, seq2_h = 0, seq3_h = 0;

  if ( c3y != u3_blob_save(_tmp_pier, dat1_y, dat1_d, &mug1_h, &seq1_h) ) {
    fprintf(stderr, "\033[31mlifecycle: save1 failed\033[0m\r\n");
    exit(1);
  }
  if ( c3y != u3_blob_save(_tmp_pier, dat2_y, dat2_d, &mug2_h, &seq2_h) ) {
    fprintf(stderr, "\033[31mlifecycle: save2 failed\033[0m\r\n");
    exit(1);
  }

  //  install a staging file with same content as blob 1 → dedup
  //
  {
    c3_c* stg_c = _write_tmp_file(dat1_y, dat1_d);
    if ( c3y != u3_blob_move_stg(_tmp_pier, stg_c, &mug3_h, &seq3_h) ) {
      fprintf(stderr, "\033[31mlifecycle: install_stg failed\033[0m\r\n");
      exit(1);
    }
    free(stg_c);
  }
  if ( mug1_h != mug3_h || seq1_h != seq3_h ) {
    fprintf(stderr, "\033[31mlifecycle: install_stg should dedup; "
                    "got %" PRIc3_h "/%" PRIc3_h " vs %" PRIc3_h "/%"
                    PRIc3_h "\033[0m\r\n", mug3_h, seq3_h, mug1_h, seq1_h);
    exit(1);
  }

  //  build a noun with both blobs, bob1 appearing twice (to exercise ram's
  //  backref path for bob atoms).  this mirrors a realistic Arvo event
  //  that references the same large binary in multiple places.
  //
  //  shape: [%blob-evt [bob1 bob2] bob1 42]
  //
  u3_noun bob1 = u3i_blob(mug1_h, seq1_h);
  u3_noun bob2 = u3i_blob(mug2_h, seq2_h);
  u3_noun ref  = u3nq(c3__blob,
                      u3nc(u3k(bob1), u3k(bob2)),
                      u3k(bob1),
                      42);
  u3z(bob1);
  u3z(bob2);

  //  encode via ram (what mars would write to the event log / newt)
  //
  c3_d  len_d = 0;
  c3_y* byt_y = 0;
  u3s_ram_xeno(ref, &len_d, &byt_y);

  //  validate header: "RAM\0" + 0x01 (the disk/newt framing)
  //
  if (  (len_d < 5)
     || (byt_y[0] != 'R')
     || (byt_y[1] != 'A')
     || (byt_y[2] != 'M')
     || (byt_y[3] != 0x00)
     || (byt_y[4] != 0x01) )
  {
    fprintf(stderr, "\033[31mlifecycle: ram header invalid\033[0m\r\n");
    exit(1);
  }

  //  decode via tap (what mars would do on replay / newt receive)
  //
  u3_weak out = u3s_tap_xeno(len_d, byt_y);
  free(byt_y);
  if ( u3_none == out ) {
    fprintf(stderr, "\033[31mlifecycle: tap returned u3_none\033[0m\r\n");
    exit(1);
  }

  //  structural equality: mug+seq preserved for bob atoms; cat/indirect
  //  atoms compared by value
  //
  if ( c3n == u3r_sing(ref, out) ) {
    fprintf(stderr, "\033[31mlifecycle: decoded noun differs from ref\033[0m\r\n");
    exit(1);
  }

  //  walk the decoded noun and pull bytes out of each bob atom.  this
  //  exercises u3r_bytes → u3r_blob_load → u3_blob_load (mmap from disk).
  //
  u3_noun tag, cel, b2, rst;
  if ( c3n == u3r_cell(out, &tag, &cel) ) {
    fprintf(stderr, "\033[31mlifecycle: decoded root not cell\033[0m\r\n");
    exit(1);
  }
  if ( tag != c3__blob ) {
    fprintf(stderr, "\033[31mlifecycle: wrong tag\033[0m\r\n");
    exit(1);
  }

  u3_noun bobs, bob1_d, bob2_d;
  u3x_trel(cel, &bobs, &b2, &rst);
  u3x_cell(bobs, &bob1_d, &bob2_d);

  //  the occurrences of bob1 should all be bob atoms pointing at the
  //  same (mug, seq).  whether tap reuses the same loom slot for
  //  backrefs is an implementation detail — we only assert semantic
  //  equality here.
  //
  if (  c3n == u3a_is_bob(bob1_d)
     || c3n == u3a_is_bob(bob2_d)
     || c3n == u3a_is_bob(b2) )
  {
    fprintf(stderr, "\033[31mlifecycle: decoded non-bob where bob expected\033[0m\r\n");
    exit(1);
  }
  if (  u3a_bob_mug(bob1_d) != mug1_h
     || u3a_bob_seq(bob1_d) != seq1_h )
  {
    fprintf(stderr, "\033[31mlifecycle: bob1 mug/seq mismatch\033[0m\r\n");
    exit(1);
  }
  if (  u3a_bob_mug(b2) != mug1_h
     || u3a_bob_seq(b2) != seq1_h )
  {
    fprintf(stderr, "\033[31mlifecycle: backref bob1 mug/seq mismatch\033[0m\r\n");
    exit(1);
  }
  if (  u3a_bob_mug(bob2_d) != mug2_h
     || u3a_bob_seq(bob2_d) != seq2_h )
  {
    fprintf(stderr, "\033[31mlifecycle: bob2 mug/seq mismatch\033[0m\r\n");
    exit(1);
  }

  //  materialize each bob to bytes and compare to original input.
  //  u3r_bytes → u3r_blob_load → u3_blob_load → mmap.
  //
  {
    c3_y* buf_y = c3_malloc(dat1_d);
    u3r_bytes(0, (c3_w)dat1_d, buf_y, bob1_d);
    if ( 0 != memcmp(buf_y, dat1_y, dat1_d) ) {
      fprintf(stderr, "\033[31mlifecycle: bob1 bytes mismatch\033[0m\r\n");
      exit(1);
    }
    c3_free(buf_y);
  }
  {
    c3_y* buf_y = c3_malloc(dat2_d);
    u3r_bytes(0, (c3_w)dat2_d, buf_y, bob2_d);
    if ( 0 != memcmp(buf_y, dat2_y, dat2_d) ) {
      fprintf(stderr, "\033[31mlifecycle: bob2 bytes mismatch\033[0m\r\n");
      exit(1);
    }
    c3_free(buf_y);
  }

  //  u3r_met (which materializes) should agree with u3r_blob_met
  //  (which reads the file directly) — exercises the cross-module
  //  invariant between retrieve.c and blob.c.
  //
  {
    c3_d    bit_d = u3r_blob_met(bob1_d);
    u3_weak mat   = u3_blob_load(_tmp_pier, mug1_h, seq1_h);
    if ( u3_none == mat ) {
      fprintf(stderr, "\033[31mlifecycle: u3_blob_load failed\033[0m\r\n");
      exit(1);
    }
    c3_w ref_w = u3r_met(0, mat);
    u3z(mat);
    if ( bit_d != (c3_d)ref_w ) {
      fprintf(stderr, "\033[31mlifecycle: blob_met=%" PRIc3_d
                      " vs materialized met=%" PRIc3_w "\033[0m\r\n",
              bit_d, ref_w);
      exit(1);
    }
  }

  u3z(ref);
  u3z(out);

  //  tear down and confirm blob files are deleted cleanly
  //
  u3_blob_wipe(_tmp_pier, mug1_h, seq1_h);
  u3_blob_wipe(_tmp_pier, mug2_h, seq2_h);
  if (  c3y == u3_blob_live(_tmp_pier, mug1_h, seq1_h)
     || c3y == u3_blob_live(_tmp_pier, mug2_h, seq2_h) )
  {
    fprintf(stderr, "\033[31mlifecycle: blobs still present after delete\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob lifecycle: ok\r\n");
}

/* main(): run all blob tests.
*/
int
main(int argc, char* argv[])
{
  (void)argc; (void)argv;
  _setup();

  _test_path();            //  no filesystem
  _test_init();
  _test_stg_clean();
  _test_save_load();
  _test_dedup();
  _test_save_fd();
  _test_delete_empty_bucket();
  _test_walk();
  _test_install_stg();
  _test_install_stg_dedup();
  _test_sane();
  _test_meld();
  _test_met();
  _test_map();
  _test_lifecycle();

  fprintf(stderr, "test blob: ok\r\n");
  return 0;
}
