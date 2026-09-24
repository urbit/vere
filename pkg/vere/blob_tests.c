/// @file

#include "blob.h"
#include "events.h"
#include "jets/q.h"
#include "noun.h"
#include "vere.h"

#include "c3/tmpdir.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#ifndef U3_OS_windows
#include <sys/resource.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Tests for pkg/vere/blob.c — the content-addressed blob store.
**
** Each test creates a fresh temp pier under the system scratch directory
** and tears it down on exit.  Tests run sequentially and exit nonzero
** on first failure.
*/

static c3_c _tmp_pier[1024];

/* _setup(): init loom (for loading blobs), make fresh temp pier.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3t_init();
  u3m_pave(c3y);

  //  the guard page, required to leap onto a child road (u3m_soft_top)
  //
  u3e_init();

  //  hot jet state, required by the meld path (u3j_boot/u3j_ream)
  //
  u3j_boot(c3y);
}

static void
_tmp_make(void)
{
  if ( !c3_tmp_make(_tmp_pier, sizeof(_tmp_pier), "vere-blob-test") ) {
    fprintf(stderr, "blob_tests: c3_tmp_make failed: %s\r\n", strerror(errno));
    exit(1);
  }

  //  blob.c assumes .urb exists (created earlier by disk code in production).
  //  Create it here so u3_disk_blob_init can mkdir .urb/bob.
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
  c3_tmp_kill(_tmp_pier);
}

/* _blob_load(): materialize a blob by id: open a hand, read the file
**   into a zeroed slab, close.
**
**   the path form the tests used before views; u3_none if the file is
**   missing or empty, as u3r_blob_load reports for a bob.
*/
static u3_weak
_blob_load(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  u3_blob_hand* han_u = u3_blob_open(pax_c, mug_h, seq_h);
  u3i_slab      sab_u;
  c3_d          len_d;

  if ( !han_u ) {
    return u3_none;
  }

  len_d = han_u->len_d;
  u3i_slab_init(&sab_u, 3, len_d);

  if ( len_d != u3_blob_read(han_u, 0, sab_u.buf_y, (c3_z)len_d) ) {
    u3i_slab_free(&sab_u);
    u3_blob_close(han_u);
    return u3_none;
  }

  u3_blob_close(han_u);
  return u3i_slab_mint_bytes(&sab_u);
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

/* _test_mark_sweep(): mass/gc marks the bank's u3a_blob records.
**
**   |mass marks the loom and then sweeps, freeing every chunk the mark
**   did not reach.  A bank record the mark skipped is freed under the
**   bank, and every bob atom over it then names a garbage file.  With
**   u3o_leak_crash set, a skipped record aborts the sweep outright.
*/
static void
_test_mark_sweep(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  const c3_y dat_y[] = "the bank record must survive a mark and sweep";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  if ( c3y != u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob mark: save failed\033[0m\r\n");
    exit(1);
  }

  //  hold the bob in arvo state, where the mark reaches it; a C local
  //  would itself be swept as a leak
  //
  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_noun roc = u3A->roc;
  u3A->roc    = bob;

  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  if ( !blb_u ) {
    fprintf(stderr, "\033[31mblob mark: no bank record\033[0m\r\n");
    exit(1);
  }

  {
    c3_h wag_h = u3C.wag_h;
    u3C.wag_h |= u3o_leak_crash;

    u3a_mark_init();
    u3m_quac** qua_u = u3m_mark();
    u3a_sweep();

    for ( c3_w i_w = 0; qua_u[i_w]; i_w++ ) {
      u3a_quac_free(qua_u[i_w]);
    }
    c3_free(qua_u);

    u3C.wag_h = wag_h;
  }

  //  the record is intact and the bob still resolves through it to the file
  //
  if (  (blb_u != u3a_blob_get(mug_h, seq_h))
     || (blb_u->mug_h != mug_h)
     || (blb_u->seq_h != seq_h)
     || (u3a_bob_seq(bob) != seq_h)
     || (u3r_met(3, bob) != (c3_w)dat_d) )
  {
    fprintf(stderr, "\033[31mblob mark: bank record clobbered by sweep "
                    "(mug %08" PRIx32 " seq %" PRIc3_h ")\033[0m\r\n",
            blb_u->mug_h, blb_u->seq_h);
    exit(1);
  }

  u3A->roc = roc;
  u3z(bob);
  _tmp_clean();
  fprintf(stderr, "test blob mark+sweep: ok\r\n");
}

/* _test_init(): u3_disk_blob_init + u3_disk_blob_stg_init create expected dirs.
*/
static void
_test_init(void)
{
  _tmp_make();

  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  _tmp_clean();
  fprintf(stderr, "test blob init: ok\r\n");
}

/* _test_stg_clean(): u3_disk_blob_stg_init clears leftover staging files.
*/
static void
_test_stg_clean(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_weak atm = _blob_load(_tmp_pier, mug_h, seq_h);
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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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

/* _test_save_fd(): u3_blob_save_fd round-trips via the staging mmap.
*/
static void
_test_save_fd(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_weak atm = _blob_load(_tmp_pier, mug_h, seq_h);
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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  u3_weak atm = _blob_load(_tmp_pier, mug_h, seq_h);
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

/* _test_install_stg_trim(): a staging file carrying trailing zero bytes is
**   trimmed to the atom's significant length before it is installed.
**
**   This is the only test that reaches u3_blob_move_stg's ftruncate: every
**   other staging payload is already canonical, so len_d == map_d and the
**   trim is skipped.  It matters most on windows, which refuses to resize a
**   file while a section is open on it -- so a trim attempted under the
**   mapping fails there and takes the whole install with it.
*/
static void
_test_install_stg_trim(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  //  significant bytes, then padding the atom does not include
  //
  c3_y dat_y[4096] = {0};
  const c3_d sig_d = 2048;

  for ( c3_d i_d = 0; i_d < sig_d; i_d++ ) {
    dat_y[i_d] = (c3_y)(1 + (i_d % 255));
  }

  c3_c* stg_c = _write_tmp_file(dat_y, sizeof(dat_y));
  if ( !stg_c ) {
    fprintf(stderr, "\033[31mblob install_stg_trim: _write_tmp_file failed"
                    "\033[0m\r\n");
    exit(1);
  }

  c3_h mug_h = 0;
  c3_h seq_h = 0;
  if ( c3y != u3_blob_move_stg(_tmp_pier, stg_c, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob install_stg_trim: install failed\033[0m\r\n");
    exit(1);
  }

  //  the installed file is the trimmed length, not the staged one
  //
  c3_c fil_c[8192];
  u3_blob_path(fil_c, _tmp_pier, mug_h, seq_h);

  struct stat st_u;
  if ( 0 != stat(fil_c, &st_u) ) {
    fprintf(stderr, "\033[31mblob install_stg_trim: stat %s: %s\033[0m\r\n",
            fil_c, strerror(errno));
    exit(1);
  }
  if ( sig_d != (c3_d)st_u.st_size ) {
    fprintf(stderr, "\033[31mblob install_stg_trim: expected %" PRIc3_d
                    " bytes on disk, got %" PRIc3_d "\033[0m\r\n",
                    sig_d, (c3_d)st_u.st_size);
    exit(1);
  }

  //  and it still denotes the same atom the padded bytes did
  //
  u3_weak atm = _blob_load(_tmp_pier, mug_h, seq_h);
  if ( u3_none == atm ) {
    fprintf(stderr, "\033[31mblob install_stg_trim: load u3_none\033[0m\r\n");
    exit(1);
  }

  u3_atom ref = u3i_bytes((c3_w)sizeof(dat_y), dat_y);
  if ( c3y != u3r_sing(ref, atm) ) {
    fprintf(stderr, "\033[31mblob install_stg_trim: atom mismatch\033[0m\r\n");
    exit(1);
  }

  u3z(ref);
  u3z(atm);
  free(stg_c);
  _tmp_clean();
  fprintf(stderr, "test blob install_stg_trim: ok\r\n");
}

/* _test_install_stg_dedup(): installing a staging file with existing
**   content returns the existing seq and consumes the staging file.
*/
static void
_test_install_stg_dedup(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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

/* _blob_met(): bit-length of a blob by id: open a hand, ask it, close.
**
**   the path form the tests used before hands; 0 if the file is
**   missing or empty, as u3r_blob_met reports for a bob.
*/
static c3_d
_blob_met(const c3_c* pax_c, c3_h mug_h, c3_h seq_h)
{
  u3_blob_hand* han_u = u3_blob_open(pax_c, mug_h, seq_h);
  c3_d          met_d;

  if ( !han_u ) {
    return 0;
  }
  met_d = u3_blob_hand_met(han_u);
  u3_blob_close(han_u);
  return met_d;
}

/* _test_met(): u3_blob_hand_met matches u3r_met on the materialized atom.
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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
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
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
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

/* _test_cue_blob(): blob-aware cue streams large atoms into the store.
**
**   jam a noun holding a large patterned atom (twice — the second
**   occurrence backrefs) and a small atom; cue it with a 1KB threshold
**   and a blob sink.  the large atom must come back as a bob atom
**   (shared via the backref), byte-identical, with the small atom left
**   plain and the bank consistent.
*/
static void
_test_cue_blob(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;
  u3C.blob_del_f = _test_blob_del_cb;
  _test_del_count_w = 0;

  //  ~100KB patterned payload (nonzero tail for exact bit-length)
  //
  const c3_z len_z = 100 * 1024;
  c3_y* dat_y = c3_malloc(len_z);
  for ( c3_z i_z = 0; i_z < len_z; i_z++ ) {
    dat_y[i_z] = (c3_y)(137 + (i_z * 31));
  }
  dat_y[len_z - 1] |= 0x80;

  u3_noun big = u3i_bytes((c3_w)len_z, dat_y);
  u3_noun ref = u3nt(u3k(big), 0x77, big);

  c3_d  jam_d;
  c3_y* jam_y;
  u3s_jam_xeno(ref, &jam_d, &jam_y);

  u3_noun out;
  {
    u3_blob_bsink bsk_u;
    u3_blob_bsink_init(&bsk_u, _tmp_pier);

    u3_cue_xeno* sil_u = u3s_cue_xeno_init();
    u3s_cue_xeno_blob(sil_u, 1024, &bsk_u.snk_u);
    out = u3s_cue_xeno_with(sil_u, jam_d, jam_y);
    u3s_cue_xeno_done(sil_u);

    if ( u3_none == out ) {
      fprintf(stderr, "\033[31mcue blob: cue failed\033[0m\r\n");
      exit(1);
    }
    if ( 1 != bsk_u.num_w ) {
      fprintf(stderr, "\033[31mcue blob: %" PRIc3_w " installs, want 1"
                      "\033[0m\r\n", bsk_u.num_w);
      exit(1);
    }
  }

  u3_noun hed = u3h(out);
  u3_noun mid = u3h(u3t(out));
  u3_noun tel = u3t(u3t(out));

  if (  (c3n == u3a_is_bob(hed))
     || (c3n == u3a_is_bob(tel)) )
  {
    fprintf(stderr, "\033[31mcue blob: large atoms not blobified\033[0m\r\n");
    exit(1);
  }
  if ( hed != tel ) {
    fprintf(stderr, "\033[31mcue blob: backref not shared\033[0m\r\n");
    exit(1);
  }
  if ( 0x77 != mid ) {
    fprintf(stderr, "\033[31mcue blob: small atom mangled\033[0m\r\n");
    exit(1);
  }

  //  bytes must round-trip through the blob file
  //
  {
    c3_y* git_y = c3_malloc(len_z);
    u3r_bytes(0, (c3_w)len_z, git_y, hed);
    if ( 0 != memcmp(git_y, dat_y, len_z) ) {
      fprintf(stderr, "\033[31mcue blob: bytes mangled\033[0m\r\n");
      exit(1);
    }
    c3_free(git_y);
  }

  //  structural equality with the original, and bank consistency:
  //  one live bob atom, no log refs
  //
  if ( c3n == u3r_sing(ref, out) ) {
    fprintf(stderr, "\033[31mcue blob: result != original\033[0m\r\n");
    exit(1);
  }

  {
    c3_h mug_h = u3a_bob_mug(hed);
    c3_h seq_h = u3a_bob_seq(hed);

    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    if ( !blb_u || (1 != blb_u->use_w) ) {
      fprintf(stderr, "\033[31mcue blob: bad bank entry\033[0m\r\n");
      exit(1);
    }

    //  re-cue: the store must dedup to the same (mug, seq)
    //
    {
      u3_blob_bsink bsk_u;
      u3_blob_bsink_init(&bsk_u, _tmp_pier);

      u3_cue_xeno* sil_u = u3s_cue_xeno_init();
      u3s_cue_xeno_blob(sil_u, 1024, &bsk_u.snk_u);
      u3_noun two = u3s_cue_xeno_with(sil_u, jam_d, jam_y);
      u3s_cue_xeno_done(sil_u);

      u3_noun bob = u3h(two);
      if (  (mug_h != u3a_bob_mug(bob))
         || (seq_h != u3a_bob_seq(bob)) )
      {
        fprintf(stderr, "\033[31mcue blob: re-cue did not dedup\033[0m\r\n");
        exit(1);
      }
      if ( 2 != blb_u->use_w ) {
        fprintf(stderr, "\033[31mcue blob: use_w %" PRIc3_w " after re-cue, "
                        "want 2\033[0m\r\n", blb_u->use_w);
        exit(1);
      }
      u3z(two);
    }

    //  cleanup: drop nouns (no log refs, so the last atom death
    //  legitimately requests deletion), then any remaining entry
    //
    u3z(out);
    u3z(ref);

    if ( (blb_u = u3a_blob_get(mug_h, seq_h)) ) {
      blb_u->use_w = 0;
      blb_u->eve_w = 0;
      u3a_blob_drop(mug_h, seq_h);
    }
  }

  c3_free(jam_y);
  c3_free(dat_y);
  u3C.blob_del_f = 0;
  _tmp_clean();
  fprintf(stderr, "test blob cue: ok\r\n");
}

static void
_test_met(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  //  case 1: no trailing zeros, not word-aligned in either bitness
  //
  //  16 bytes, top byte = 0x01 (1 significant bit)
  //  expected met = (16-1)*8 + 1 = 121
  //
  //  Also verifies the loaded atom's trailing
  //  word bytes: u3r_met on the loaded atom must agree with the hand.
  //
  {
    const c3_y dat_y[] = { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x9a,
                           0xbc, 0xde, 0xf0, 0x11, 0x22, 0x33, 0x44, 0x01 };
    const c3_d dat_d   = sizeof(dat_y);
    c3_h mug_h = 0; c3_h seq_h = 0;
    if ( c3y != u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h) ) {
      fprintf(stderr, "\033[31mblob met: dense save failed\033[0m\r\n");
      exit(1);
    }

    c3_d bit_d = _blob_met(_tmp_pier, mug_h, seq_h);
    if ( 121 != bit_d ) {
      fprintf(stderr, "\033[31mblob met: dense got %" PRIc3_d ", expected 121"
                      "\033[0m\r\n", bit_d);
      exit(1);
    }

    u3_weak atm = _blob_load(_tmp_pier, mug_h, seq_h);
    if ( u3_none == atm ) {
      fprintf(stderr, "\033[31mblob met: load u3_none\033[0m\r\n");
      exit(1);
    }
    c3_w ref_w = u3r_met(0, atm);
    u3z(atm);
    if ( bit_d != (c3_d)ref_w ) {
      fprintf(stderr, "\033[31mblob met: hand_met=%" PRIc3_d
                      " != u3r_met=%" PRIc3_w " (load "
                      "not zero-initializing trailing bytes?)\033[0m\r\n",
              bit_d, ref_w);
      exit(1);
    }
  }

  //  case 2: trailing zeros — met should strip them
  //
  {
    const c3_y dat_y[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0x00, 0x00, 0x00, 0x00 };
    const c3_d dat_d   = sizeof(dat_y);
    c3_h mug_h = 0; c3_h seq_h = 0;
    if ( c3y != u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h) ) {
      fprintf(stderr, "\033[31mblob met: trailing-zero save failed\033[0m\r\n");
      exit(1);
    }

    c3_d bit_d = _blob_met(_tmp_pier, mug_h, seq_h);
    //  16 significant bytes; high byte 0xff → 8 bits
    //  total = 15*8 + 8 = 128
    //
    if ( 128 != bit_d ) {
      fprintf(stderr, "\033[31mblob met: trailing-zero got %" PRIc3_d
                      ", expected 128\033[0m\r\n", bit_d);
      exit(1);
    }
  }

  //  case 3: nonexistent blob → 0
  //
  {
    if ( 0 != _blob_met(_tmp_pier, 0xdeadbeef, 999) ) {
      fprintf(stderr, "\033[31mblob met: missing blob should return 0\033[0m\r\n");
      exit(1);
    }
  }

  _tmp_clean();
  fprintf(stderr, "test blob met: ok\r\n");
}

/* _fd_dead(): true if [fid_i] is no longer an open descriptor.
** _fd_live(): true if [fid_i] is still one.
**
**   Neither is checkable through the CRT on Windows (probing a closed fd
**   trips the invalid-parameter handler), so both answer yes there and
**   the hand's own counts carry the assertion.
*/
static c3_o
_fd_dead(c3_i fid_i)
{
#ifdef U3_OS_windows
  (void)fid_i;
  return c3y;
#else
  return ( (-1 == fcntl(fid_i, F_GETFD)) && (EBADF == errno) ) ? c3y : c3n;
#endif
}

static c3_o
_fd_live(c3_i fid_i)
{
#ifdef U3_OS_windows
  (void)fid_i;
  return c3y;
#else
  return ( -1 != fcntl(fid_i, F_GETFD) ) ? c3y : c3n;
#endif
}

/* _test_hand(): u3_blob_open/data/read/met/close round-trip.
*/
static void
_test_hand(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "handle bytes should round-trip exactly";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h);

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  if ( !han_u ) {
    fprintf(stderr, "\033[31mblob hand: open returned NULL\033[0m\r\n");
    exit(1);
  }
  if ( (han_u->len_d != dat_d) || (1 != u3_blob_hands()) ) {
    fprintf(stderr, "\033[31mblob hand: len %" PRIc3_d " hands %zu\033[0m\r\n",
            han_u->len_d, u3_blob_hands());
    exit(1);
  }

  //  bit-length: top byte is the last one, u3r_met(0) semantics
  //
  {
    c3_y top_y = dat_y[dat_d - 1];
    c3_d exp_d = (dat_d - 1) * 8
               + (c3_d)(8 - (__builtin_clz((unsigned int)top_y) - 24));
    c3_d met_d = u3_blob_hand_met(han_u);
    if ( met_d != exp_d ) {
      fprintf(stderr, "\033[31mblob hand: met %" PRIc3_d " != %" PRIc3_d
                      "\033[0m\r\n", met_d, exp_d);
      exit(1);
    }
  }

  //  whole-file mapping, then a window through pread
  //
  {
    const c3_y* buf_y = u3_blob_data(han_u);
    if ( !buf_y || (0 != memcmp(buf_y, dat_y, dat_d)) ) {
      fprintf(stderr, "\033[31mblob hand: data mismatch\033[0m\r\n");
      exit(1);
    }
    if ( buf_y != u3_blob_data(han_u) ) {
      fprintf(stderr, "\033[31mblob hand: data not memoized\033[0m\r\n");
      exit(1);
    }

    c3_y win_y[8];
    if (  (5 != u3_blob_read(han_u, 7, win_y, 5))
       || (0 != memcmp(win_y, dat_y + 7, 5)) )
    {
      fprintf(stderr, "\033[31mblob hand: read window mismatch\033[0m\r\n");
      exit(1);
    }
    //  a read past the end is short, not an error
    //
    if ( 3 != u3_blob_read(han_u, dat_d - 3, win_y, 8) ) {
      fprintf(stderr, "\033[31mblob hand: read past end not short\033[0m\r\n");
      exit(1);
    }
  }

  c3_i fid_i = han_u->fid_i;
  u3_blob_close(han_u);

  if ( (0 != u3_blob_hands()) || (c3n == _fd_dead(fid_i)) ) {
    fprintf(stderr, "\033[31mblob hand: close left hands %zu, fd open\033[0m\r\n",
            u3_blob_hands());
    exit(1);
  }

  //  missing blob: NULL, no bail, nothing left in the table
  //
  if ( u3_blob_open(_tmp_pier, 0xdeadbeef, 999) || u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand: missing should be NULL\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand: ok\r\n");
}

/* _test_hand_dedup(): the home road never shares: two opens of one
**   blob are two hands and two fds, each released by its own close.
*/
static void
_test_hand_dedup(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "one fd per open on the home road";
  c3_h mug_h = 0; c3_h seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  u3_blob_hand* one_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  u3_blob_hand* two_u = u3_blob_open(_tmp_pier, mug_h, seq_h);

  if (  !one_u || !two_u || (one_u == two_u)
     || (one_u->fid_i == two_u->fid_i) || (2 != u3_blob_hands()) )
  {
    fprintf(stderr, "\033[31mblob hand dedup: home road shared\033[0m\r\n");
    exit(1);
  }

  c3_i one_i = one_u->fid_i;
  c3_i two_i = two_u->fid_i;
  u3_blob_close(one_u);

  if ( (1 != u3_blob_hands()) || (c3n == _fd_dead(one_i)) || (c3n == _fd_live(two_i)) ) {
    fprintf(stderr, "\033[31mblob hand dedup: first close disturbed the other"
                    "\033[0m\r\n");
    exit(1);
  }

  {
    c3_y byt_y;
    if ( 1 != u3_blob_read(two_u, 0, &byt_y, 1) ) {
      fprintf(stderr, "\033[31mblob hand dedup: survivor unreadable\033[0m\r\n");
      exit(1);
    }
  }

  u3_blob_close(two_u);

  if ( u3_blob_hands() || (c3n == _fd_dead(two_i)) ) {
    fprintf(stderr, "\033[31mblob hand dedup: last close leaked\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand dedup: ok\r\n");
}

//  shared state for the unwind tests: the blob, a bob atom over it
//  (built on the home road), and the fd the inner road opened.
//
static c3_h    _han_mug_h;
static c3_h    _han_seq_h;
static u3_atom _han_bob;
static c3_i    _han_fid_i;

/* _hand_open_inner(): open the blob twice from the inner road.
**
**   Once directly and once through a u3r_view over the bob atom: the
**   road deduplicates by blob id, so the view lands on the same hand,
**   and both are held when the road unwinds.
*/
static u3_blob_hand*
_hand_open_inner(void)
{
  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  u3_assert( han_u );
  _han_fid_i = han_u->fid_i;

  u3r_view vue_u;
  u3r_view_init(&vue_u, _han_bob);
  u3_assert( vue_u.han_u == han_u );

  return han_u;
}

static u3_noun
_hand_bail_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();
  return u3m_bail(c3__fail);
}

static u3_noun
_hand_signal_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();
  u3m_signal(c3__intr);
  return 0;
}

static u3_noun
_hand_leak_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();
  return 0;   //  returns normally without closing anything
}

/* _hand_unwind_setup(): fresh pier with one blob and a bob atom over it.
*/
static void
_hand_unwind_setup(const c3_c* nam_c)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  const c3_y dat_y[] = "unwinding must release every hand the road held";
  if ( c3y != u3_blob_save(_tmp_pier, dat_y, sizeof(dat_y) - 1,
                           &_han_mug_h, &_han_seq_h) )
  {
    fprintf(stderr, "\033[31mblob hand %s: save failed\033[0m\r\n", nam_c);
    exit(1);
  }
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);
}

/* _hand_unwind_check(): after the inner road is gone, nothing is open.
*/
static void
_hand_unwind_check(const c3_c* nam_c)
{
  if ( u3_blob_hands() || (c3n == _fd_dead(_han_fid_i)) ) {
    fprintf(stderr, "\033[31mblob hand %s: %zu hand(s) survived the road"
                    "\033[0m\r\n", nam_c, u3_blob_hands());
    exit(1);
  }
  u3z(_han_bob);
  _tmp_clean();
  fprintf(stderr, "test blob hand %s: ok\r\n", nam_c);
}

/* _test_hand_bail(): u3m_bail sweeps the bailing road's hands.
*/
static void
_test_hand_bail(void)
{
  _hand_unwind_setup("bail");
  u3z(u3m_soft_top(0, 1 << 12, _hand_bail_cb, 0));
  _hand_unwind_check("bail");
}

/* _test_hand_signal(): a signal unwind sweeps every child road's hands.
*/
static void
_test_hand_signal(void)
{
  _hand_unwind_setup("signal");
  u3z(u3m_soft_top(0, 1 << 12, _hand_signal_cb, 0));
  _hand_unwind_check("signal");
}

/* _test_hand_leak(): a normal return without a close is drained at fall.
*/
static void
_test_hand_leak(void)
{
  _hand_unwind_setup("leak");
  u3z(u3m_soft_top(0, 1 << 12, _hand_leak_cb, 0));
  _hand_unwind_check("leak");
}

/* _test_hand_outer(): a home-road reference survives an inner bail.
*/
static void
_test_hand_outer(void)
{
  _hand_unwind_setup("outer");

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  u3_assert( han_u );

  u3z(u3m_soft_top(0, 1 << 12, _hand_bail_cb, 0));

  //  the inner road opened its own hand on the blob; only that went
  //
  if ( (1 != u3_blob_hands()) || (c3n == _fd_live(han_u->fid_i)) ) {
    fprintf(stderr, "\033[31mblob hand outer: home hand swept "
                    "(hands %zu)\033[0m\r\n", u3_blob_hands());
    exit(1);
  }

  c3_i fid_i = han_u->fid_i;
  u3_blob_close(han_u);
  _han_fid_i = fid_i;
  _hand_unwind_check("outer");
}

/* _test_hand_many(): hundreds of distinct hands open at once, then drained.
*/
static void
_test_hand_many(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  //  write the files directly (no fsync per file) into one bucket
  //
  const c3_h mug_h = 0x1234;
  const c3_w num_w = 300;
  c3_c pax_c[8192];

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);
  if ( 0 != mkdir(pax_c, 0700) ) {
    fprintf(stderr, "\033[31mblob hand many: mkdir failed\033[0m\r\n");
    exit(1);
  }

  for ( c3_w i_w = 1; i_w <= num_w; i_w++ ) {
    u3_blob_path(pax_c, _tmp_pier, mug_h, i_w);
    FILE* fil_f = fopen(pax_c, "wb");
    if ( !fil_f ) {
      fprintf(stderr, "\033[31mblob hand many: fopen failed\033[0m\r\n");
      exit(1);
    }
    fprintf(fil_f, "blob number %08" PRIc3_w " padded past the minimum", i_w);
    fclose(fil_f);
  }

  u3_blob_hand** han_u = c3_malloc(num_w * sizeof(*han_u));

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    han_u[i_w] = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    if ( !han_u[i_w] ) {
      fprintf(stderr, "\033[31mblob hand many: open %" PRIc3_w " failed\033[0m\r\n", i_w);
      exit(1);
    }
  }
  if ( num_w != u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand many: %zu hands, expected %" PRIc3_w
                    "\033[0m\r\n", u3_blob_hands(), num_w);
    exit(1);
  }

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    u3_blob_close(han_u[i_w]);
  }
  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand many: %zu hands left\033[0m\r\n",
            u3_blob_hands());
    exit(1);
  }

  c3_free(han_u);
  _tmp_clean();
  fprintf(stderr, "test blob hand many: ok\r\n");
}

static void _hand_write_raw(c3_h mug_h, c3_h seq_h, c3_w num_w);

//  road entry and exit, internal to manage.c
//
void u3m_leap(c3_w pad_w);
void u3m_fall(void);

static u3_noun
_hand_keep_cb(u3_noun arg)
{
  (void)arg;
  const c3_h mug_h = 0x1234;
  const c3_w num_w = 300;

  //  each open is closed at once, so every hand is idle: the road keeps
  //  at most its cap, evicting the oldest
  //
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    if ( !han_u ) {
      fprintf(stderr, "\033[31mblob hand keep: open %" PRIc3_w " failed"
                      "\033[0m\r\n", i_w);
      exit(1);
    }
    u3_blob_close(han_u);
  }

  c3_z kep_z = u3_blob_hands_road(u3R);
  if ( (kep_z >= num_w) || (kep_z < 100) ) {
    fprintf(stderr, "\033[31mblob hand keep: %zu retained\033[0m\r\n", kep_z);
    exit(1);
  }

  //  the newest is still open; the oldest was evicted and reopens
  //
  {
    u3_blob_hand* new_u = u3_blob_open(_tmp_pier, mug_h, num_w);
    u3_blob_hand* old_u = u3_blob_open(_tmp_pier, mug_h, 1);
    if ( !new_u || !old_u || (kep_z != u3_blob_hands_road(u3R)) ) {
      fprintf(stderr, "\033[31mblob hand keep: reopen changed the count"
                      "\033[0m\r\n");
      exit(1);
    }
    u3_blob_close(new_u);
    u3_blob_close(old_u);
  }
  return 0;
}

/* _test_hand_keep(): an inner road bounds how many idle hands it retains.
*/
static void
_test_hand_keep(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  for ( c3_w i_w = 1; i_w <= 300; i_w++ ) {
    _hand_write_raw(0x1234, i_w, i_w);
  }

  u3z(u3m_soft_top(0, 1 << 12, _hand_keep_cb, 0));

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand keep: residue\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand keep: ok\r\n");
}

/* _test_hand_comp(): windowed comparison of bobs against bobs and loom atoms.
*/
static void
_test_hand_comp(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  //  three 5000-byte payloads spanning more than one 4 KiB window:
  //  [b] differs from [a] only in its top byte, [c] only in byte 100.
  //
  const c3_w len_w = 5000;
  c3_y* a_y = c3_malloc(len_w);
  c3_y* b_y = c3_malloc(len_w);
  c3_y* c_y = c3_malloc(len_w);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    a_y[i_w] = (c3_y)(1 + (i_w * 7) % 251);
  }
  memcpy(b_y, a_y, len_w);
  memcpy(c_y, a_y, len_w);
  a_y[len_w - 1] = 0x40;
  b_y[len_w - 1] = 0x41;
  c_y[100]       = (c3_y)(a_y[100] + 1);

  c3_h am_h, as_h, bm_h, bs_h, cm_h, cs_h;
  if (  (c3y != u3_blob_save(_tmp_pier, a_y, len_w, &am_h, &as_h))
     || (c3y != u3_blob_save(_tmp_pier, b_y, len_w, &bm_h, &bs_h))
     || (c3y != u3_blob_save(_tmp_pier, c_y, len_w, &cm_h, &cs_h)) )
  {
    fprintf(stderr, "\033[31mblob hand comp: save failed\033[0m\r\n");
    exit(1);
  }

  u3_atom boa = u3i_blob(am_h, as_h);
  u3_atom bob = u3i_blob(bm_h, bs_h);
  u3_atom boc = u3i_blob(cm_h, cs_h);
  u3_atom loa = u3i_bytes(len_w, a_y);

  #define _COMP_CHECK(cond, msg)                                          \
    if ( !(cond) ) {                                                       \
      fprintf(stderr, "\033[31mblob hand comp: %s\033[0m\r\n", msg);       \
      exit(1);                                                             \
    }

  _COMP_CHECK( c3y == u3r_sing(boa, loa),  "bob != equal loom atom" );
  _COMP_CHECK( c3y == u3r_sing(loa, boa),  "loom atom != equal bob" );
  _COMP_CHECK( c3n == u3r_sing(boa, bob),  "top-byte difference missed" );
  _COMP_CHECK( c3n == u3r_sing(boa, boc),  "mid-byte difference missed" );
  _COMP_CHECK( c3n == u3r_sing(loa, bob),  "loom vs bob difference missed" );

  _COMP_CHECK( 0  == u3r_comp(boa, loa),   "comp bob vs equal loom" );
  _COMP_CHECK( -1 == u3r_comp(boa, bob),   "comp top byte a < b" );
  _COMP_CHECK( 1  == u3r_comp(bob, boa),   "comp top byte b > a" );
  _COMP_CHECK( -1 == u3r_comp(boa, boc),   "comp mid byte a < c" );
  _COMP_CHECK( 1  == u3r_comp(bob, loa),   "comp bob vs smaller loom" );

  _COMP_CHECK( 1 == u3r_nord(boa, loa),    "nord equal" );
  _COMP_CHECK( 0 == u3r_nord(boa, bob),    "nord a < b" );
  _COMP_CHECK( 2 == u3r_nord(bob, boa),    "nord b > a" );

  _COMP_CHECK( 0x40     == u3r_byte(len_w - 1, boa), "byte at top" );
  _COMP_CHECK( a_y[100] == u3r_byte(100, boa),       "byte in middle" );
  _COMP_CHECK( 0        == u3r_byte(len_w + 5, boa), "byte past end" );

  #undef _COMP_CHECK

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand comp: %zu hand(s) left open\033[0m\r\n",
            u3_blob_hands());
    exit(1);
  }

  u3z(boa); u3z(bob); u3z(boc); u3z(loa);
  c3_free(a_y); c3_free(b_y); c3_free(c_y);
  _tmp_clean();
  fprintf(stderr, "test blob hand comp: ok\r\n");
}

static u3_noun
_hand_alarm_cb(u3_noun arg)
{
  (void)arg;

  //  hold a view for the whole spin, and churn the road's list so the
  //  event timer's SIGVTALRM can land inside a critical section as
  //  easily as between two.  the timer counts user CPU time, which the
  //  open/close syscalls barely consume, so each round also burns some
  //  arithmetic.  the round cap turns a timer that never fires into a
  //  test failure instead of a hang.
  //
  u3r_view vue_u;
  u3r_view_init(&vue_u, _han_bob);
  _han_fid_i = vue_u.han_u->fid_i;

  volatile c3_d sum_d = 0;

  for ( c3_w rou_w = 0; rou_w < 2000000; rou_w++ ) {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
    u3_blob_close(han_u);

    for ( c3_w i_w = 0; i_w < 20000; i_w++ ) {
      sum_d += i_w;
    }
  }

  u3r_view_done(&vue_u);
  return 0;
}

/* _test_hand_alarm(): a real SIGVTALRM from the event timer, delivered
**   through the production handler while hands are open and the road's list
**   is being mutated, unwinds cleanly and leaves the table consistent.
*/
static void
_test_hand_alarm(void)
{
  _hand_unwind_setup("alarm");

  u3_noun gon = u3m_soft_top(20, 1 << 12, _hand_alarm_cb, 0);
  u3_noun tag, sig;

  if (  (c3n == u3r_cell(gon, &tag, &sig))
     || (3 != tag)
     || (c3n == u3r_cell(sig, &sig, 0))
     || (c3__alrm != sig) )
  {
    fprintf(stderr, "\033[31mblob hand alarm: no %%alrm unwind\033[0m\r\n");
    exit(1);
  }
  u3z(gon);

  //  the table is usable after the unwind
  //
  {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
    if ( !han_u ) {
      fprintf(stderr, "\033[31mblob hand alarm: reopen after unwind failed"
                      "\033[0m\r\n");
      exit(1);
    }
    u3_blob_close(han_u);
  }

  _hand_unwind_check("alarm");
}

/* _test_jam_bob(): both jam encoders stream a bob identically to jamming
**   the materialized atom.
**
**   The blob spans five full 8 KiB windows plus a partial one that is
**   not word-aligned, and its top byte is 0x05 so the bit length is not a
**   multiple of 8: the last window carries a partial byte.  The bob
**   appears twice in the noun so the backref path runs too.
*/
static void
_test_jam_bob(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  const c3_w len_w = (5 * 8192) + 1235;
  c3_y*      dat_y = c3_malloc(len_w);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + ((i_w * 13) % 253));
  }
  dat_y[len_w - 1] = 0x05;

  c3_h mug_h = 0; c3_h seq_h = 0;
  if ( c3y != u3_blob_save(_tmp_pier, dat_y, len_w, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob jam: save failed\033[0m\r\n");
    exit(1);
  }

  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_atom loa = u3i_bytes(len_w, dat_y);
  u3_noun bon = u3nc(u3k(bob), u3nc(42, u3k(bob)));
  u3_noun lon = u3nc(u3k(loa), u3nc(42, u3k(loa)));

  //  fib encoder (loom slab)
  //
  {
    u3i_slab sab_u, sal_u;
    c3_w bib_w = u3s_jam_fib(&sab_u, bon);
    c3_w bil_w = u3s_jam_fib(&sal_u, lon);

    if (  (bib_w != bil_w)
       || (0 != memcmp(sab_u.buf_y, sal_u.buf_y, (bib_w + 7) >> 3)) )
    {
      fprintf(stderr, "\033[31mblob jam: fib encoding differs "
                      "(%" PRIc3_w " vs %" PRIc3_w " bits)\033[0m\r\n",
              bib_w, bil_w);
      exit(1);
    }
    u3i_slab_free(&sab_u);
    u3i_slab_free(&sal_u);
  }

  //  xeno encoder (heap), then cue it back
  //
  {
    c3_d  leb_d = 0, lel_d = 0;
    c3_y* byb_y = 0;
    c3_y* byl_y = 0;
    u3s_jam_xeno(bon, &leb_d, &byb_y);
    u3s_jam_xeno(lon, &lel_d, &byl_y);

    if ( (leb_d != lel_d) || (0 != memcmp(byb_y, byl_y, (size_t)leb_d)) ) {
      fprintf(stderr, "\033[31mblob jam: xeno encoding differs "
                      "(%" PRIc3_d " vs %" PRIc3_d " bytes)\033[0m\r\n",
              leb_d, lel_d);
      exit(1);
    }

    u3_weak cue = u3s_cue_xeno(leb_d, byb_y);
    if ( (u3_none == cue) || (c3n == u3r_sing(cue, lon)) ) {
      fprintf(stderr, "\033[31mblob jam: cue of bob jam != source\033[0m\r\n");
      exit(1);
    }
    u3z(cue);
    c3_free(byb_y);
    c3_free(byl_y);
  }

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob jam: %zu hand(s) left open\033[0m\r\n",
            u3_blob_hands());
    exit(1);
  }

  u3z(bon); u3z(lon); u3z(bob); u3z(loa);
  c3_free(dat_y);
  _tmp_clean();
  fprintf(stderr, "test blob jam: ok\r\n");
}

/* _test_jets_bob(): rsh, end, and cut read ranges from a bob the same
**   way they do from the materialized atom, including ranges that touch,
**   cross, and lie beyond the end of the file.
*/
static void
_test_jets_bob(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + ((i_w * 7) % 251));
  }

  c3_h mug_h = 0; c3_h seq_h = 0;
  if ( c3y != u3_blob_save(_tmp_pier, dat_y, len_w, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob jets: save failed\033[0m\r\n");
    exit(1);
  }

  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_atom loa = u3i_bytes(len_w, dat_y);

  #define _JET_CHECK(nam_c, a_w, b_w, c_w, bob_x, loa_x)                       \
    {                                                                          \
      u3_noun rb = (bob_x);                                                    \
      u3_noun rl = (loa_x);                                                    \
      if ( c3n == u3r_sing(rb, rl) ) {                                         \
        fprintf(stderr, "\033[31mblob jets: %s(%u, %u, %u) differs\033[0m\r\n",\
                nam_c, (unsigned)(a_w), (unsigned)(b_w), (unsigned)(c_w));     \
        exit(1);                                                               \
      }                                                                        \
      u3z(rb); u3z(rl);                                                        \
    }

  //  rsh: byte, word, and bit bloqs; offsets inside, at, and past the end
  //
  {
    const c3_w off_w[] = { 0, 1, 5, 4096, 4999, 5000, 5010 };
    for ( c3_w i_w = 0; i_w < sizeof(off_w) / sizeof(*off_w); i_w++ ) {
      _JET_CHECK("rsh", 3, off_w[i_w], 0,
                 u3qc_rsh(3, off_w[i_w], bob), u3qc_rsh(3, off_w[i_w], loa));
    }
    const c3_w wof_w[] = { 0, 3, 1249, 1250, 1251 };
    for ( c3_w i_w = 0; i_w < sizeof(wof_w) / sizeof(*wof_w); i_w++ ) {
      _JET_CHECK("rsh", 5, wof_w[i_w], 0,
                 u3qc_rsh(5, wof_w[i_w], bob), u3qc_rsh(5, wof_w[i_w], loa));
    }
    _JET_CHECK("rsh", 0, 13, 0, u3qc_rsh(0, 13, bob), u3qc_rsh(0, 13, loa));
  }

  //  end: same shapes
  //
  {
    const c3_w len_y[] = { 0, 1, 7, 4096, 4999, 5000, 5010 };
    for ( c3_w i_w = 0; i_w < sizeof(len_y) / sizeof(*len_y); i_w++ ) {
      _JET_CHECK("end", 3, len_y[i_w], 0,
                 u3qc_end(3, len_y[i_w], bob), u3qc_end(3, len_y[i_w], loa));
    }
    const c3_w wen_w[] = { 1, 1249, 1250, 1251 };
    for ( c3_w i_w = 0; i_w < sizeof(wen_w) / sizeof(*wen_w); i_w++ ) {
      _JET_CHECK("end", 5, wen_w[i_w], 0,
                 u3qc_end(5, wen_w[i_w], bob), u3qc_end(5, wen_w[i_w], loa));
    }
    _JET_CHECK("end", 0, 13, 0, u3qc_end(0, 13, bob), u3qc_end(0, 13, loa));
  }

  //  cut: ranges inside, touching, crossing, and beyond the end
  //
  {
    const c3_w cut_w[][2] = {
      { 0, 0 }, { 0, 1 }, { 0, 5000 }, { 10, 4000 }, { 4095, 2 },
      { 4990, 10 }, { 4990, 20 }, { 5000, 5 }, { 6000, 5 }
    };
    for ( c3_w i_w = 0; i_w < sizeof(cut_w) / sizeof(*cut_w); i_w++ ) {
      _JET_CHECK("cut", 3, cut_w[i_w][0], cut_w[i_w][1],
                 u3qc_cut(3, cut_w[i_w][0], cut_w[i_w][1], bob),
                 u3qc_cut(3, cut_w[i_w][0], cut_w[i_w][1], loa));
    }
    const c3_w wut_w[][2] = { { 0, 1 }, { 1249, 2 }, { 1250, 1 }, { 1251, 3 } };
    for ( c3_w i_w = 0; i_w < sizeof(wut_w) / sizeof(*wut_w); i_w++ ) {
      _JET_CHECK("cut", 5, wut_w[i_w][0], wut_w[i_w][1],
                 u3qc_cut(5, wut_w[i_w][0], wut_w[i_w][1], bob),
                 u3qc_cut(5, wut_w[i_w][0], wut_w[i_w][1], loa));
    }
    _JET_CHECK("cut", 0, 3, 17, u3qc_cut(0, 3, 17, bob), u3qc_cut(0, 3, 17, loa));
  }

  #undef _JET_CHECK

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob jets: %zu hand(s) left open\033[0m\r\n",
            u3_blob_hands());
    exit(1);
  }

  u3z(bob); u3z(loa);
  c3_free(dat_y);
  _tmp_clean();
  fprintf(stderr, "test blob jets: ok\r\n");
}

/* _hand_write_raw(): write a blob file directly into one bucket, no
**   locking or fsync.  [num_w] distinguishes the content.
*/
static void
_hand_write_raw(c3_h mug_h, c3_h seq_h, c3_w num_w)
{
  c3_c pax_c[8192];

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);
  if ( (0 != mkdir(pax_c, 0700)) && (EEXIST != errno) ) {
    fprintf(stderr, "\033[31mblob raw: mkdir failed\033[0m\r\n");
    exit(1);
  }

  u3_blob_path(pax_c, _tmp_pier, mug_h, seq_h);
  FILE* fil_f = fopen(pax_c, "wb");
  if ( !fil_f ) {
    fprintf(stderr, "\033[31mblob raw: fopen failed\033[0m\r\n");
    exit(1);
  }
  fprintf(fil_f, "raw blob %08" PRIc3_w " padded past the minimum length", num_w);
  fclose(fil_f);
}

/* _hand_unwound(): true if [gon] is a bail or signal result, not [0 pro].
*/
static c3_o
_hand_unwound(u3_noun gon)
{
  return ( (c3y == u3du(gon)) && (0 != u3h(gon)) ) ? c3y : c3n;
}

/* _hand_mote(): the mote of a [3 mote tax] result, or 0.
*/
static u3_noun
_hand_mote(u3_noun gon)
{
  u3_noun tag, res;
  if (  (c3n == u3r_cell(gon, &tag, &res))
     || (3 != tag)
     || (c3n == u3r_cell(res, &res, 0)) )
  {
    return 0;
  }
  return res;
}

#ifndef U3_OS_windows
//  set by _hand_intr_crit_cb once the held SIGINT has been survived
//
static c3_w _han_hit_w;

static void
_crit_handler_fwd(int sig)
{
  (void)sig;
  _han_hit_w++;
}

static u3_noun
_hand_intr_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();

  //  delivered to this thread before kill() returns: the production
  //  handler sees a child road and unwinds
  //
  kill(getpid(), SIGINT);
  return 0;
}

static u3_noun
_hand_intr_crit_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();

  u3m_crit_enter();
  kill(getpid(), SIGINT);
  _han_hit_w = 1;         //  still here: the signal is pending
  u3m_crit_leave();       //  delivered here; never returns
  _han_hit_w = 2;
  return 0;
}

/* _test_hand_intr(): a real SIGINT through the production handler
**   unwinds the road and sweeps its hands.
*/
static void
_test_hand_intr(void)
{
  _hand_unwind_setup("intr");

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_intr_cb, 0);
  if ( c3__intr != _hand_mote(gon) ) {
    fprintf(stderr, "\033[31mblob hand intr: no %%intr unwind\033[0m\r\n");
    exit(1);
  }
  u3z(gon);

  _hand_unwind_check("intr");
}

/* _test_hand_intr_crit(): a SIGINT raised inside a critical section is
**   held until the section ends, then unwinds through the production
**   handler; the hold count is back at zero afterwards.
*/
static void
_test_hand_intr_crit(void)
{
  _hand_unwind_setup("intr crit");
  _han_hit_w = 0;

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_intr_crit_cb, 0);
  if ( (c3__intr != _hand_mote(gon)) || (1 != _han_hit_w) ) {
    fprintf(stderr, "\033[31mblob hand intr crit: hit %" PRIc3_w
                    ", expected the unwind at leave\033[0m\r\n", _han_hit_w);
    exit(1);
  }
  u3z(gon);

  //  the hold count is zero: a fresh section delivers at its own leave
  //
  {
    void (*old_f)(int) = signal(SIGINT, _crit_handler_fwd);
    _han_hit_w = 0;
    u3m_crit_enter();
    raise(SIGINT);
    if ( _han_hit_w ) {
      fprintf(stderr, "\033[31mblob hand intr crit: hold count not reset"
                      "\033[0m\r\n");
      exit(1);
    }
    u3m_crit_leave();
    signal(SIGINT, old_f);
    if ( 1 != _han_hit_w ) {
      fprintf(stderr, "\033[31mblob hand intr crit: section not delivering"
                      "\033[0m\r\n");
      exit(1);
    }
  }

  _hand_unwind_check("intr crit");
}
#endif

static u3_noun
_hand_meme_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();

  //  a padded view opens the hand first and then u3a_malloc's the pad:
  //  256 MiB on a 1 MiB loom bails %meme from that exact site
  //
  u3r_view vue_u;
  u3r_view_padd(&vue_u, _han_bob, 1 << 28);
  u3r_view_done(&vue_u);
  return 0;
}

/* _test_hand_meme(): an allocation bail under a live view is swept.
*/
static void
_test_hand_meme(void)
{
  _hand_unwind_setup("meme");

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_meme_cb, 0);
  if ( c3n == _hand_unwound(gon) ) {
    fprintf(stderr, "\033[31mblob hand meme: pad did not bail\033[0m\r\n");
    exit(1);
  }
  u3z(gon);

  _hand_unwind_check("meme");
}

static u3_noun
_hand_cue_cb(u3_noun arg)
{
  (void)arg;
  _hand_open_inner();

  //  cue holds a view across its whole run and allocates as it goes;
  //  the malformed bytes make it bail with the view open
  //
  return u3s_cue(_han_bob);
}

/* _test_hand_cue(): cue bailing under a live view is swept.
*/
static void
_test_hand_cue(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  //  an atom tag followed by a run-length prefix that never terminates
  //
  c3_y dat_y[64];
  memset(dat_y, 0, sizeof(dat_y));
  dat_y[sizeof(dat_y) - 1] = 0x80;

  if ( c3y != u3_blob_save(_tmp_pier, dat_y, sizeof(dat_y),
                           &_han_mug_h, &_han_seq_h) )
  {
    fprintf(stderr, "\033[31mblob hand cue: save failed\033[0m\r\n");
    exit(1);
  }
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_cue_cb, 0);
  if ( c3n == _hand_unwound(gon) ) {
    fprintf(stderr, "\033[31mblob hand cue: garbage cued\033[0m\r\n");
    exit(1);
  }
  u3z(gon);

  _hand_unwind_check("cue");
}

#ifndef U3_OS_windows
/* _test_hand_emfile(): running out of descriptors fails an open cleanly.
*/
static void
_test_hand_emfile(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_h mug_h = 0x2345;
  const c3_w num_w = 32;

  for ( c3_w i_w = 1; i_w <= num_w; i_w++ ) {
    _hand_write_raw(mug_h, i_w, i_w);
  }

  struct rlimit old_u, new_u;
  if ( 0 != getrlimit(RLIMIT_NOFILE, &old_u) ) {
    fprintf(stderr, "\033[31mblob hand emfile: getrlimit failed\033[0m\r\n");
    exit(1);
  }
  new_u = old_u;
  new_u.rlim_cur = 24;
  if ( 0 != setrlimit(RLIMIT_NOFILE, &new_u) ) {
    fprintf(stderr, "\033[31mblob hand emfile: setrlimit failed\033[0m\r\n");
    exit(1);
  }

  u3_blob_hand** han_u = c3_malloc(num_w * sizeof(*han_u));
  c3_w           got_w = 0;

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    han_u[i_w] = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    if ( han_u[i_w] ) {
      got_w++;
    }
  }

  setrlimit(RLIMIT_NOFILE, &old_u);

  if ( (got_w == num_w) || (got_w != u3_blob_hands()) ) {
    fprintf(stderr, "\033[31mblob hand emfile: %" PRIc3_w " opened, %zu in "
                    "table\033[0m\r\n", got_w, u3_blob_hands());
    exit(1);
  }

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    if ( han_u[i_w] ) {
      c3_y byt_y;
      if ( 1 != u3_blob_read(han_u[i_w], 0, &byt_y, 1) ) {
        fprintf(stderr, "\033[31mblob hand emfile: opened hand unreadable"
                        "\033[0m\r\n");
        exit(1);
      }
      u3_blob_close(han_u[i_w]);
    }
  }

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand emfile: residue\033[0m\r\n");
    exit(1);
  }

  c3_free(han_u);
  _tmp_clean();
  fprintf(stderr, "test blob hand emfile: ok\r\n");
}

static u3_noun
_hand_file_cb(u3_noun arg)
{
  (void)arg;
  const c3_h mug_h = 0x2345;
  const c3_w num_w = 32;

  //  idle hands are evicted to make room, so every open succeeds
  //
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    if ( !han_u ) {
      fprintf(stderr, "\033[31mblob hand file: idle eviction did not free a"
                      " descriptor\033[0m\r\n");
      exit(1);
    }
    u3_blob_close(han_u);
  }

  //  held hands cannot be evicted: the road runs out and bails
  //
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    (void)u3_blob_open(_tmp_pier, mug_h, i_w + 1);
  }

  fprintf(stderr, "\033[31mblob hand file: no bail at the ceiling\033[0m\r\n");
  exit(1);
}

/* _test_hand_file(): at the descriptor ceiling an inner road evicts its
**   idle hands, and bails %file once none are idle.
*/
static void
_test_hand_file(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  for ( c3_w i_w = 1; i_w <= 32; i_w++ ) {
    _hand_write_raw(0x2345, i_w, i_w);
  }

  struct rlimit old_u, new_u;
  getrlimit(RLIMIT_NOFILE, &old_u);
  new_u = old_u;
  new_u.rlim_cur = 24;
  if ( 0 != setrlimit(RLIMIT_NOFILE, &new_u) ) {
    fprintf(stderr, "\033[31mblob hand file: setrlimit failed\033[0m\r\n");
    exit(1);
  }

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_file_cb, 0);

  setrlimit(RLIMIT_NOFILE, &old_u);

  if ( c3__file != _hand_mote(gon) ) {
    fprintf(stderr, "\033[31mblob hand file: no %%file unwind\033[0m\r\n");
    exit(1);
  }
  u3z(gon);

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand file: residue\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand file: ok\r\n");
}

/* _test_hand_wipe(): wiping a blob under a live hand unlinks the file,
**   warns, and leaves the reader on the surviving inode.
*/
static void
_test_hand_wipe(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_y dat_y[] = "wiped under a live hand, still readable";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, dat_d, &mug_h, &seq_h);

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  if ( !han_u ) {
    fprintf(stderr, "\033[31mblob hand wipe: open failed\033[0m\r\n");
    exit(1);
  }
  c3_i fid_i = han_u->fid_i;

  u3_blob_wipe(_tmp_pier, mug_h, seq_h);

  if ( c3y == u3_blob_live(_tmp_pier, mug_h, seq_h) ) {
    fprintf(stderr, "\033[31mblob hand wipe: file survived\033[0m\r\n");
    exit(1);
  }

  //  posix keeps the inode for the open descriptor
  //
  {
    c3_y buf_y[64];
    if (  (dat_d != u3_blob_read(han_u, 0, buf_y, (c3_z)dat_d))
       || (0 != memcmp(buf_y, dat_y, (size_t)dat_d)) )
    {
      fprintf(stderr, "\033[31mblob hand wipe: read after wipe failed"
                      "\033[0m\r\n");
      exit(1);
    }
  }

  u3_blob_close(han_u);

  if ( u3_blob_hands() || (c3n == _fd_dead(fid_i)) ) {
    fprintf(stderr, "\033[31mblob hand wipe: close leaked\033[0m\r\n");
    exit(1);
  }

  //  a second open finds nothing
  //
  if ( u3_blob_open(_tmp_pier, mug_h, seq_h) ) {
    fprintf(stderr, "\033[31mblob hand wipe: reopened a wiped blob\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand wipe: ok\r\n");
}
#endif

/* _test_hand_stop(): u3_blob_stop releases every home-road hand; the
**   list can be initialized again afterwards.
*/
static void
_test_hand_stop(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_h mug_h = 0x3456;
  c3_i       fid_i[3];

  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    _hand_write_raw(mug_h, i_w + 1, i_w);
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    if ( !han_u ) {
      fprintf(stderr, "\033[31mblob hand stop: open failed\033[0m\r\n");
      exit(1);
    }
    fid_i[i_w] = han_u->fid_i;
  }

  u3_blob_stop();

  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    if ( c3n == _fd_dead(fid_i[i_w]) ) {
      fprintf(stderr, "\033[31mblob hand stop: fd %" PRIc3_w " survived"
                      "\033[0m\r\n", i_w);
      exit(1);
    }
  }

  u3_blob_init();

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand stop: residue after init\033[0m\r\n");
    exit(1);
  }

  {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, 1);
    if ( !han_u ) {
      fprintf(stderr, "\033[31mblob hand stop: open after init failed"
                      "\033[0m\r\n");
      exit(1);
    }
    u3_blob_close(han_u);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand stop: ok\r\n");
}

#ifndef U3_OS_windows
/* _test_hand_trunc(): every reader reports a file shortened under it.
*/
static void
_test_hand_trunc(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + (i_w % 200));
  }

  c3_h mug_h = 0; c3_h seq_h = 0;
  u3_blob_save(_tmp_pier, dat_y, len_w, &mug_h, &seq_h);

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  if ( !han_u ) {
    fprintf(stderr, "\033[31mblob hand trunc: open failed\033[0m\r\n");
    exit(1);
  }

  {
    //  blob files are created read-only; shortening one needs write bits
    //
    c3_c pax_c[8192];
    u3_blob_path(pax_c, _tmp_pier, mug_h, seq_h);
    if ( (0 != chmod(pax_c, 0600)) || (0 != truncate(pax_c, 100)) ) {
      fprintf(stderr, "\033[31mblob hand trunc: truncate failed\033[0m\r\n");
      exit(1);
    }
  }

  {
    c3_y* buf_y = c3_malloc(len_w);
    if ( 100 != u3_blob_read(han_u, 0, buf_y, len_w) ) {
      fprintf(stderr, "\033[31mblob hand trunc: read not short\033[0m\r\n");
      exit(1);
    }
    c3_free(buf_y);
  }

  if ( u3_blob_data(han_u) || han_u->map_y ) {
    fprintf(stderr, "\033[31mblob hand trunc: data did not fail cleanly"
                    "\033[0m\r\n");
    exit(1);
  }

  if ( 0 != u3_blob_hand_met(han_u) ) {
    fprintf(stderr, "\033[31mblob hand trunc: met not zero\033[0m\r\n");
    exit(1);
  }

  u3_blob_close(han_u);
  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand trunc: residue\033[0m\r\n");
    exit(1);
  }

  c3_free(dat_y);
  _tmp_clean();
  fprintf(stderr, "test blob hand trunc: ok\r\n");
}
#endif

static u3_noun
_hand_gone_view_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;
  u3r_view_init(&vue_u, _han_bob);
  u3r_view_done(&vue_u);
  return 0;
}

static u3_noun
_hand_gone_met_cb(u3_noun arg)
{
  (void)arg;
  return u3r_met(3, _han_bob);
}

static u3_noun
_hand_gone_xeno_cb(u3_noun arg)
{
  (void)arg;
  c3_d  len_d = 0;
  c3_y* byt_y = 0;
  u3s_jam_xeno(_han_bob, &len_d, &byt_y);
  c3_free(byt_y);
  return 0;
}

static u3_noun
_hand_gone_byte_cb(u3_noun arg)
{
  (void)arg;
  return u3r_byte(0, _han_bob);
}

static u3_noun
_hand_gone_fib_cb(u3_noun arg)
{
  (void)arg;
  u3i_slab sab_u;
  u3s_jam_fib(&sab_u, _han_bob);
  u3i_slab_free(&sab_u);
  return 0;
}

/* _hand_gone_expect(): [fun_f] over a bob with no file must bail %fail
**   and leave no hand open.
*/
static void
_hand_gone_expect(const c3_c* nam_c, u3_funk fun_f)
{
  u3_noun gon = u3m_soft_top(0, 1 << 12, fun_f, 0);
  if ( c3__fail != _hand_mote(gon) ) {
    fprintf(stderr, "\033[31mblob hand gone: %s did not bail %%fail"
                    "\033[0m\r\n", nam_c);
    exit(1);
  }
  u3z(gon);

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand gone: %s left residue\033[0m\r\n",
            nam_c);
    exit(1);
  }
}

/* _test_hand_gone(): a bob whose file is missing fails at every entry
**   point the way its contract says: bail %fail for readers, u3_none
**   for load, 0 for met.
*/
static void
_test_hand_gone(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  _han_mug_h = 0x4567;
  _han_seq_h = 1;
  _han_bob   = u3i_blob(_han_mug_h, _han_seq_h);

  _hand_gone_expect("view", _hand_gone_view_cb);
  _hand_gone_expect("met",  _hand_gone_met_cb);
  _hand_gone_expect("xeno", _hand_gone_xeno_cb);
  _hand_gone_expect("fib",  _hand_gone_fib_cb);
  _hand_gone_expect("byte", _hand_gone_byte_cb);

  //  a windowed view reports the missing file without bailing
  //
  {
    u3r_view vue_u;
    if ( (c3n != u3r_view_open(&vue_u, _han_bob)) || u3_blob_hands() ) {
      fprintf(stderr, "\033[31mblob hand gone: open did not decline"
                      "\033[0m\r\n");
      exit(1);
    }
  }

  if (  (u3_none != _blob_load(_tmp_pier, _han_mug_h, _han_seq_h))
     || (0 != _blob_met(_tmp_pier, _han_mug_h, _han_seq_h))
     || u3_blob_hands() )
  {
    fprintf(stderr, "\033[31mblob hand gone: load/met contract\033[0m\r\n");
    exit(1);
  }

  u3z(_han_bob);
  _tmp_clean();
  fprintf(stderr, "test blob hand gone: ok\r\n");
}

static u3_noun
_hand_nest_cb(u3_noun arg)
{
  (void)arg;
  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  u3_assert( han_u && (1 == han_u->use_w) );
  u3_blob_close(han_u);
  u3_assert( 0 == han_u->use_w );
  return 0;
}

/* _test_hand_nest(): a child road that opens and closes normally leaves
**   the home hand exactly as it found it.
*/
static void
_test_hand_nest(void)
{
  _hand_unwind_setup("nest");

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  u3_assert( han_u );
  c3_i fid_i = han_u->fid_i;

  u3z(u3m_soft_top(0, 1 << 12, _hand_nest_cb, 0));

  if (  (1 != u3_blob_hands())
     || (fid_i != han_u->fid_i) || (c3n == _fd_live(fid_i)) )
  {
    fprintf(stderr, "\033[31mblob hand nest: home hand disturbed"
                    "\033[0m\r\n");
    exit(1);
  }

  u3_blob_close(han_u);
  _han_fid_i = fid_i;
  _hand_unwind_check("nest");
}

/* _test_hand_edge(): met and comparison at the 4 KiB window boundaries.
*/
static void
_test_hand_edge(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  //  met: the top byte sits at the very end of, or just past, a window
  //
  {
    const c3_w len_w[] = { 4095, 4096, 4097, 8192, 8193 };

    for ( c3_w i_w = 0; i_w < sizeof(len_w) / sizeof(*len_w); i_w++ ) {
      c3_y* dat_y = c3_malloc(len_w[i_w]);
      memset(dat_y, 0x11, len_w[i_w]);
      dat_y[len_w[i_w] - 1] = 0x03;

      c3_h mug_h = 0; c3_h seq_h = 0;
      u3_blob_save(_tmp_pier, dat_y, len_w[i_w], &mug_h, &seq_h);

      u3_weak atm = _blob_load(_tmp_pier, mug_h, seq_h);
      c3_d    met_d = _blob_met(_tmp_pier, mug_h, seq_h);

      if ( (u3_none == atm) || (met_d != (c3_d)u3r_met(0, atm)) ) {
        fprintf(stderr, "\033[31mblob hand edge: met at %" PRIc3_w
                        " bytes\033[0m\r\n", len_w[i_w]);
        exit(1);
      }
      u3z(atm);
      c3_free(dat_y);
    }
  }

  //  compare: 8192-byte bobs differing exactly at a window boundary
  //
  {
    const c3_w len_w = 8192;
    c3_y* a_y = c3_malloc(len_w);
    c3_y* b_y = c3_malloc(len_w);
    c3_y* c_y = c3_malloc(len_w);

    memset(a_y, 0x22, len_w);
    memcpy(b_y, a_y, len_w);
    memcpy(c_y, a_y, len_w);
    b_y[4095] = 0x23;
    c_y[4096] = 0x21;

    c3_h am_h, as_h, bm_h, bs_h, cm_h, cs_h;
    u3_blob_save(_tmp_pier, a_y, len_w, &am_h, &as_h);
    u3_blob_save(_tmp_pier, b_y, len_w, &bm_h, &bs_h);
    u3_blob_save(_tmp_pier, c_y, len_w, &cm_h, &cs_h);

    u3_atom boa = u3i_blob(am_h, as_h);
    u3_atom bob = u3i_blob(bm_h, bs_h);
    u3_atom boc = u3i_blob(cm_h, cs_h);
    u3_atom loa = u3i_bytes(len_w, a_y);

    if (  (c3y != u3r_sing(boa, loa))
       || (c3n != u3r_sing(boa, bob))
       || (c3n != u3r_sing(boa, boc))
       || (-1 != u3r_comp(boa, bob))     //  b larger at byte 4095
       || (1  != u3r_comp(boa, boc))     //  c smaller at byte 4096
       || (1  != u3r_comp(bob, boc)) )
    {
      fprintf(stderr, "\033[31mblob hand edge: window-boundary compare"
                      "\033[0m\r\n");
      exit(1);
    }

    u3z(boa); u3z(bob); u3z(boc); u3z(loa);
    c3_free(a_y); c3_free(b_y); c3_free(c_y);
  }

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob hand edge: residue\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand edge: ok\r\n");
}

static u3_noun
_hand_share_cb(u3_noun arg)
{
  (void)arg;

  u3r_view vue_u;
  u3r_view_init(&vue_u, _han_bob);

  u3_blob_hand* han_u = vue_u.han_u;
  c3_i          fid_i = han_u->fid_i;
  _han_fid_i          = fid_i;

  c3_w met_w = u3r_met(3, _han_bob);
  c3_y byt_y = u3r_byte(3, _han_bob);

  if (  (1 != u3_blob_hands_road(u3R)) || (1 != han_u->use_w)
     || (fid_i != han_u->fid_i)
     || (met_w != vue_u.len_w) || (byt_y != vue_u.byt_y[3]) )
  {
    fprintf(stderr, "\033[31mblob hand share: second open under a view"
                    "\033[0m\r\n");
    exit(1);
  }

  //  with the view's mapping in place, the fixed-width readers copy
  //  out of it: inside the file they agree with the mapping, across
  //  the end they are zero-filled past the file's own length
  //
  {
    c3_y win_y[16];
    c3_y exp_y[16];
    c3_w off_w = met_w - 5;

    memset(exp_y, 0, sizeof(exp_y));
    memcpy(exp_y, vue_u.byt_y + off_w, 5);

    u3r_bytes(off_w, sizeof(win_y), win_y, _han_bob);

    if (  han_u->map_y != vue_u.byt_y
       || (0 != memcmp(win_y, exp_y, sizeof(win_y)))
       || (u3r_chub(0, _han_bob) != *(c3_d*)vue_u.byt_y)
       || (0 != u3r_chub(1 + (met_w >> 3), _han_bob))
       || (0 != u3r_byte(met_w + 100000, _han_bob)) )
    {
      fprintf(stderr, "\033[31mblob hand share: readers under a mapping"
                      "\033[0m\r\n");
      exit(1);
    }
  }

  u3r_view_done(&vue_u);
  return 0;
}

/* _test_hand_share(): on an inner road, met and byte reads under a live
**   view reuse its hand and fd instead of opening a second one.
*/
static void
_test_hand_share(void)
{
  _hand_unwind_setup("share");
  u3z(u3m_soft_top(0, 1 << 12, _hand_share_cb, 0));
  _hand_unwind_check("share");
}

static u3_noun
_hand_reuse_cb(u3_noun arg)
{
  (void)arg;

  //  a trap's open, close, open lands on the one retained hand
  //
  u3_blob_hand* one_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  c3_i          fid_i = one_u->fid_i;
  _han_fid_i          = fid_i;
  u3_blob_close(one_u);

  if ( (0 != one_u->use_w) || (c3n == _fd_live(fid_i)) ) {
    fprintf(stderr, "\033[31mblob hand reuse: close released the hand"
                    "\033[0m\r\n");
    exit(1);
  }

  for ( c3_w i_w = 0; i_w < 1000; i_w++ ) {
    u3_blob_hand* two_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
    if ( (two_u != one_u) || (fid_i != two_u->fid_i) || (1 != two_u->use_w) ) {
      fprintf(stderr, "\033[31mblob hand reuse: reopened\033[0m\r\n");
      exit(1);
    }
    u3_blob_close(two_u);
  }

  if ( 1 != u3_blob_hands_road(u3R) ) {
    fprintf(stderr, "\033[31mblob hand reuse: %zu hands on the road"
                    "\033[0m\r\n", u3_blob_hands_road(u3R));
    exit(1);
  }
  return 0;
}

/* _test_hand_reuse(): an inner road keeps a closed hand for its next
**   open, and the fall releases it.
*/
static void
_test_hand_reuse(void)
{
  _hand_unwind_setup("reuse");
  u3z(u3m_soft_top(0, 1 << 12, _hand_reuse_cb, 0));
  _hand_unwind_check("reuse");
}

static u3_noun
_hand_deep_cb(u3_noun arg)
{
  (void)arg;

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  c3_i          fid_i = han_u->fid_i;
  _han_fid_i          = fid_i;
  u3_blob_close(han_u);

  //  a grandchild borrows the parent's hand without listing it, and
  //  its fall leaves the parent's hand open
  //
  u3m_leap(512);
  {
    u3_blob_hand* kid_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);

    if (  (kid_u != han_u) || (fid_i != kid_u->fid_i)
       || (0 != u3_blob_hands_road(u3R)) || (0 != kid_u->use_w) )
    {
      fprintf(stderr, "\033[31mblob hand deep: child did not borrow"
                      "\033[0m\r\n");
      exit(1);
    }
    u3_blob_close(kid_u);
  }
  u3m_fall();

  if (  (c3n == _fd_live(fid_i)) || (1 != u3_blob_hands_road(u3R))
     || (han_u != u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h)) )
  {
    fprintf(stderr, "\033[31mblob hand deep: child's fall took the parent's"
                    " hand\033[0m\r\n");
    exit(1);
  }
  return 0;
}

static u3_noun
_hand_wide_cb(u3_noun arg)
{
  (void)arg;

  u3r_view one_u, two_u;
  c3_y     zer_y[64];
  memset(zer_y, 0, sizeof(zer_y));

  //  a lone view widens the hand's mapping in place
  //
  u3r_view_init(&one_u, _han_bob);
  {
    u3_blob_hand* han_u = one_u.han_u;
    c3_w          wid_w = (c3_w)u3_blob_hand_pad(han_u) + 4096;
    _han_fid_i = han_u->fid_i;

    u3r_view_done(&one_u);
    u3r_view_padd(&one_u, _han_bob, wid_w);

#ifndef U3_OS_windows
    if (  (u3r_view_blob != one_u.kin_e) || (one_u.han_u != han_u)
       || (han_u->map_d < wid_w)
       || (0 != memcmp(one_u.byt_y + wid_w - 64, zer_y, 64)) )
    {
      fprintf(stderr, "\033[31mblob hand wide: lone view did not widen"
                      "\033[0m\r\n");
      exit(1);
    }

    //  with that view live, a pad past the mapping cannot remap under
    //  it and is filled into a loom pad instead; the first view and the
    //  mapping are untouched
    //
    c3_d old_d = han_u->map_d;
    c3_w big_w = (c3_w)old_d + 8;

    u3r_view_padd(&two_u, _han_bob, big_w);
    if (  (u3r_view_heap != two_u.kin_e) || (big_w != two_u.len_w)
       || (han_u->map_d != old_d) || (han_u->map_y != one_u.byt_y)
       || (0 != memcmp(two_u.byt_y, one_u.byt_y, wid_w))
       || (0 != memcmp(two_u.byt_y + big_w - 8, zer_y, 8))
       || (1 != han_u->use_w) )
    {
      fprintf(stderr, "\033[31mblob hand wide: held hand remapped or pad "
                      "wrong\033[0m\r\n");
      exit(1);
    }
    u3r_view_done(&two_u);
#endif
    u3r_view_done(&one_u);
  }
  return 0;
}

/* _test_hand_wide(): a pad past the file's pages widens the hand's
**   mapping, unless another live view holds the hand.
*/
static void
_test_hand_wide(void)
{
  _hand_unwind_setup("wide");
  u3z(u3m_soft_top(0, 1 << 12, _hand_wide_cb, 0));
  _hand_unwind_check("wide");
}

/* _test_hand_deep(): a nested road borrows an inner ancestor's hand.
*/
static void
_test_hand_deep(void)
{
  _hand_unwind_setup("deep");
  u3z(u3m_soft_top(0, 1 << 12, _hand_deep_cb, 0));
  _hand_unwind_check("deep");
}

/* _test_hand_empty(): a zero-byte blob file opens as nothing.
*/
static void
_test_hand_empty(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  const c3_h mug_h = 0x5678;
  c3_c pax_c[8192];

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);
  mkdir(pax_c, 0700);
  u3_blob_path(pax_c, _tmp_pier, mug_h, 1);
  fclose(fopen(pax_c, "wb"));

  if (  u3_blob_open(_tmp_pier, mug_h, 1)
     || (u3_none != _blob_load(_tmp_pier, mug_h, 1))
     || (0 != _blob_met(_tmp_pier, mug_h, 1))
     || u3_blob_hands() )
  {
    fprintf(stderr, "\033[31mblob hand empty: empty file not rejected"
                    "\033[0m\r\n");
    exit(1);
  }

  _tmp_clean();
  fprintf(stderr, "test blob hand empty: ok\r\n");
}

/* _test_hand_access(): every fixed-width and range reader on a bob agrees
**   with the materialized atom, including reads past the end, and none of
**   them leaves a hand open.
*/
static void
_test_hand_access(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + ((i_w * 11) % 241));
  }

  c3_h mug_h = 0; c3_h seq_h = 0;
  if ( c3y != u3_blob_save(_tmp_pier, dat_y, len_w, &mug_h, &seq_h) ) {
    fprintf(stderr, "\033[31mblob access: save failed\033[0m\r\n");
    exit(1);
  }

  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_atom loa = u3i_bytes(len_w, dat_y);

  #define _ACC_CHECK(cond, nam_c, i_w)                                        \
    if ( !(cond) ) {                                                          \
      fprintf(stderr, "\033[31mblob access: %s at %u\033[0m\r\n",             \
              nam_c, (unsigned)(i_w));                                        \
      exit(1);                                                                \
    }

  //  fixed-width readers, inside and past the end
  //
  {
    const c3_w bit_w[]  = { 0, 7, 8, 12345, 39999, 40000, 40008, 100000 };
    for ( c3_w i_w = 0; i_w < sizeof(bit_w) / sizeof(*bit_w); i_w++ ) {
      _ACC_CHECK( u3r_bit(bit_w[i_w], bob) == u3r_bit(bit_w[i_w], loa),
                  "bit", bit_w[i_w] );
    }
    const c3_w sho_w[]  = { 0, 1, 2499, 2500, 2600 };
    for ( c3_w i_w = 0; i_w < sizeof(sho_w) / sizeof(*sho_w); i_w++ ) {
      _ACC_CHECK( u3r_short(sho_w[i_w], bob) == u3r_short(sho_w[i_w], loa),
                  "short", sho_w[i_w] );
    }
    const c3_w haf_w[]  = { 0, 1, 1249, 1250, 1300 };
    for ( c3_w i_w = 0; i_w < sizeof(haf_w) / sizeof(*haf_w); i_w++ ) {
      _ACC_CHECK( u3r_half(haf_w[i_w], bob) == u3r_half(haf_w[i_w], loa),
                  "half", haf_w[i_w] );
      _ACC_CHECK( u3r_word(haf_w[i_w], bob) == u3r_word(haf_w[i_w], loa),
                  "word", haf_w[i_w] );
    }
    const c3_w chb_w[]  = { 0, 1, 624, 625, 700 };
    for ( c3_w i_w = 0; i_w < sizeof(chb_w) / sizeof(*chb_w); i_w++ ) {
      _ACC_CHECK( u3r_chub(chb_w[i_w], bob) == u3r_chub(chb_w[i_w], loa),
                  "chub", chb_w[i_w] );
    }
  }

  //  range readers.  the scratch buffers must hold the widest read: on
  //  a 64-bit loom a word is eight bytes, so 1250 words is 10000 bytes.
  //
  {
    const c3_z buf_z = 16384;
    c3_y* ba_y = c3_malloc(buf_z);
    c3_y* bl_y = c3_malloc(buf_z);

    const c3_w byt_w[][2] = { {0, 5000}, {10, 20}, {4990, 20}, {5000, 10}, {6000, 4} };
    for ( c3_w i_w = 0; i_w < sizeof(byt_w) / sizeof(*byt_w); i_w++ ) {
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_bytes(byt_w[i_w][0], byt_w[i_w][1], ba_y, bob);
      u3r_bytes(byt_w[i_w][0], byt_w[i_w][1], bl_y, loa);
      _ACC_CHECK( 0 == memcmp(ba_y, bl_y, buf_z), "bytes", byt_w[i_w][0] );
    }

    const c3_w hfs_w[][2] = { {0, 1250}, {1249, 3}, {1250, 2}, {1300, 4} };
    for ( c3_w i_w = 0; i_w < sizeof(hfs_w) / sizeof(*hfs_w); i_w++ ) {
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_halfs(hfs_w[i_w][0], hfs_w[i_w][1], (c3_h*)ba_y, bob);
      u3r_halfs(hfs_w[i_w][0], hfs_w[i_w][1], (c3_h*)bl_y, loa);
      _ACC_CHECK( 0 == memcmp(ba_y, bl_y, buf_z), "halfs", hfs_w[i_w][0] );
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_words(hfs_w[i_w][0], hfs_w[i_w][1], (c3_w*)ba_y, bob);
      u3r_words(hfs_w[i_w][0], hfs_w[i_w][1], (c3_w*)bl_y, loa);
      _ACC_CHECK( 0 == memcmp(ba_y, bl_y, buf_z), "words", hfs_w[i_w][0] );
    }

    const c3_w chs_w[][2] = { {0, 625}, {624, 3}, {625, 2}, {700, 4} };
    for ( c3_w i_w = 0; i_w < sizeof(chs_w) / sizeof(*chs_w); i_w++ ) {
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_chubs(chs_w[i_w][0], chs_w[i_w][1], (c3_d*)ba_y, bob);
      u3r_chubs(chs_w[i_w][0], chs_w[i_w][1], (c3_d*)bl_y, loa);
      _ACC_CHECK( 0 == memcmp(ba_y, bl_y, buf_z), "chubs", chs_w[i_w][0] );
    }

    c3_free(ba_y);
    c3_free(bl_y);
  }

  //  chop: bit, byte, and word bloqs at aligned and unaligned offsets,
  //  into a destination that already carries bits
  //
  {
    const c3_w chp_w[][4] = {   //  met, fum, wid, tou
      { 0, 0, 40000, 0 }, { 0, 13, 1000, 7 }, { 0, 39990, 50, 3 },
      { 3, 10, 4000, 3 }, { 3, 4990, 20, 0 }, { 3, 5000, 8, 1 },
      { 5, 0, 1250, 0 }, { 5, 1249, 2, 1 }, { 5, 1250, 4, 0 },
      { 6, 3, 100, 2 }
    };
    for ( c3_w i_w = 0; i_w < sizeof(chp_w) / sizeof(*chp_w); i_w++ ) {
      c3_g met_g = chp_w[i_w][0];
      c3_w wid_w = chp_w[i_w][2];
      c3_w tou_w = chp_w[i_w][3];
      c3_w siz_w = ((tou_w + wid_w) << met_g) + 128;

      u3i_slab sa_u, sl_u;
      u3i_slab_init(&sa_u, 0, siz_w);
      u3i_slab_init(&sl_u, 0, siz_w);
      sa_u.buf_w[0] = 0x5; sl_u.buf_w[0] = 0x5;

      u3r_chop(met_g, chp_w[i_w][1], wid_w, tou_w, sa_u.buf_w, bob);
      u3r_chop(met_g, chp_w[i_w][1], wid_w, tou_w, sl_u.buf_w, loa);

      _ACC_CHECK( 0 == memcmp(sa_u.buf_y, sl_u.buf_y,
                              (size_t)sa_u.len_w * u3a_word_bytes),
                  "chop", i_w );
      u3i_slab_free(&sa_u);
      u3i_slab_free(&sl_u);
    }
  }

  //  gmp import and mug
  //
  {
    mpz_t ma_mp, ml_mp;
    u3r_mp(ma_mp, bob);
    u3r_mp(ml_mp, loa);
    _ACC_CHECK( 0 == mpz_cmp(ma_mp, ml_mp), "mp", 0 );
    mpz_clear(ma_mp);
    mpz_clear(ml_mp);
    _ACC_CHECK( u3r_mug(bob) == u3r_mug(loa), "mug", 0 );
  }

  //  padded views: a pad shorter than the bob truncates the flat view;
  //  one inside the mapping's last page is the mapping itself, whose
  //  tail reads as zero; only one past that is heap-backed
  //
  {
    u3r_view vue_u;
    c3_y     zer_y[1000];
    c3_w     pad_w;

    memset(zer_y, 0, sizeof(zer_y));

    {
      u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
      pad_w = (c3_w)u3_blob_hand_pad(han_u);
      u3_blob_close(han_u);
    }
    _ACC_CHECK( (pad_w >= len_w) && (0 == u3_blob_hands()), "pad", pad_w );

    u3r_view_padd(&vue_u, bob, 100);
    _ACC_CHECK( (u3r_view_blob == vue_u.kin_e) && (100 == vue_u.len_w)
             && (0 == memcmp(vue_u.byt_y, dat_y, 100)) && (1 == u3_blob_hands()),
                "padd short", 100 );
    u3r_view_done(&vue_u);

    //  a pad inside the hand's own zero tail is the mapping; on a
    //  platform whose pad is only the word tail it is a loom pad
    //
    {
      c3_o map_o = ( pad_w >= 6000 ) ? c3y : c3n;

      u3r_view_padd(&vue_u, bob, 6000);
      _ACC_CHECK( (vue_u.kin_e == ((c3y == map_o) ? u3r_view_blob : u3r_view_heap))
               && (6000 == vue_u.len_w)
               && (0 == memcmp(vue_u.byt_y, dat_y, len_w))
               && (0 == memcmp(vue_u.byt_y + len_w, zer_y, 1000))
               && (u3_blob_hands() == ((c3y == map_o) ? 1 : 0)),
                  "padd mapped", 6000 );
      u3r_view_done(&vue_u);
    }

    //  past the file's pages the hand widens its mapping with anonymous
    //  zero pages, so even a very wide pad allocates nothing; windows
    //  cannot fix a mapping and falls back to a loom pad
    //
    {
      c3_w wid_w = pad_w + 100000;
      u3r_view_padd(&vue_u, bob, wid_w);
#ifndef U3_OS_windows
      _ACC_CHECK( (u3r_view_blob == vue_u.kin_e) && (1 == u3_blob_hands())
               && (vue_u.han_u->map_d >= wid_w),
                  "padd wide kind", wid_w );
#else
      _ACC_CHECK( (u3r_view_heap == vue_u.kin_e) && (0 == u3_blob_hands()),
                  "padd wide kind", wid_w );
#endif
      _ACC_CHECK( (wid_w == vue_u.len_w)
               && (0 == memcmp(vue_u.byt_y, dat_y, len_w))
               && (0 == memcmp(vue_u.byt_y + pad_w, zer_y, 1000))
               && (0 == memcmp(vue_u.byt_y + wid_w - 1000, zer_y, 1000)),
                  "padd wide bytes", wid_w );
      u3r_view_done(&vue_u);
    }

    //  loom atoms: a pad wider than the atom is a loom pad; a direct
    //  atom is viewed in place and padded the same way
    //
    {
      u3_atom sma = u3i_string("small loom atom, wider than a word");
      c3_w    met_w = u3r_met(3, sma);

      u3r_view_padd(&vue_u, sma, 64);
      _ACC_CHECK( (u3r_view_heap == vue_u.kin_e) && (64 == vue_u.len_w)
               && (0 == memcmp(vue_u.byt_y + met_w, zer_y, 64 - met_w)),
                  "padd loom", 64 );
      {
        u3_atom cop = u3i_bytes(met_w, vue_u.byt_y);
        _ACC_CHECK( c3y == u3r_sing(sma, cop), "padd loom bytes", met_w );
        u3z(cop);
      }
      u3r_view_done(&vue_u);

      u3r_view_init(&vue_u, 0x44332211);
      _ACC_CHECK( (u3r_view_flat == vue_u.kin_e) && (4 == vue_u.len_w)
               && (vue_u.byt_y == (const c3_y*)&vue_u.raw_d)
               && (0x11 == vue_u.byt_y[0]) && (0x44 == vue_u.byt_y[3]),
                  "init cat", 4 );
      u3r_view_done(&vue_u);

      u3r_view_padd(&vue_u, 0x44332211, 16);
      _ACC_CHECK( (u3r_view_heap == vue_u.kin_e) && (16 == vue_u.len_w)
               && (0x11 == vue_u.byt_y[0]) && (0x44 == vue_u.byt_y[3])
               && (0 == memcmp(vue_u.byt_y + 4, zer_y, 12)),
                  "padd cat", 16 );
      u3r_view_done(&vue_u);

      u3r_view_padd(&vue_u, sma, 4);
      _ACC_CHECK( (u3r_view_loom == vue_u.kin_e) && (4 == vue_u.len_w),
                  "padd truncate", 4 );
      u3r_view_done(&vue_u);

      //  windowed views: a bob opens without mapping and reads windows
      //  straight from the file, zero past the end; a loom atom and a
      //  direct atom read the same way from their own bytes
      //
      {
        c3_y win_y[64];
        c3_y exp_y[64];

        _ACC_CHECK( c3y == u3r_view_open(&vue_u, bob), "open bob", 0 );
        _ACC_CHECK( (u3r_view_blob == vue_u.kin_e) && (0 == vue_u.byt_y)
                 && (len_w == vue_u.len_w) && (0 == vue_u.han_u->map_y)
                 && (1 == u3_blob_hands()),
                    "open bob shape", len_w );

        memset(exp_y, 0, sizeof(exp_y));
        memcpy(exp_y, dat_y + len_w - 40, 40);
        _ACC_CHECK( (40 == u3r_view_read(&vue_u, len_w - 40, win_y, sizeof(win_y)))
                 && (0 == memcmp(win_y, exp_y, sizeof(win_y)))
                 && (0 == vue_u.han_u->map_y),
                    "read bob tail", len_w - 40 );
        _ACC_CHECK( (0 == u3r_view_read(&vue_u, (c3_d)len_w << 20, win_y, 8))
                 && (0 == memcmp(win_y, exp_y + 40, 8)),
                    "read bob past", 0 );
        u3r_view_done(&vue_u);
        _ACC_CHECK( 0 == u3_blob_hands(), "open bob done", 0 );

        _ACC_CHECK( c3y == u3r_view_open(&vue_u, sma), "open loom", 0 );
        _ACC_CHECK( (u3r_view_loom == vue_u.kin_e)
                 && (3 == u3r_view_read(&vue_u, met_w - 3, win_y, 16))
                 && (0 == memcmp(win_y, vue_u.byt_y + met_w - 3, 3))
                 && (0 == memcmp(win_y + 3, exp_y + 40, 13)),
                    "read loom", met_w );
        u3r_view_done(&vue_u);

        _ACC_CHECK( c3y == u3r_view_open(&vue_u, 0x44332211), "open cat", 0 );
        _ACC_CHECK( (u3r_view_flat == vue_u.kin_e)
                 && (2 == u3r_view_read(&vue_u, 2, win_y, 4))
                 && (0x33 == win_y[0]) && (0x44 == win_y[1])
                 && (0 == win_y[2]) && (0 == win_y[3]),
                    "read cat", 2 );
        u3r_view_done(&vue_u);
      }

      u3z(sma);
    }
  }

  #undef _ACC_CHECK

  if ( u3_blob_hands() ) {
    fprintf(stderr, "\033[31mblob access: %zu hand(s) left open\033[0m\r\n",
            u3_blob_hands());
    exit(1);
  }

  u3z(bob); u3z(loa);
  c3_free(dat_y);
  _tmp_clean();
  fprintf(stderr, "test blob hand access: ok\r\n");
}

#ifndef U3_OS_windows
static volatile sig_atomic_t _crit_hit;

static void
_crit_handler(int sig)
{
  (void)sig;
  _crit_hit++;
}
#endif

/* _test_crit(): a signal raised inside a critical section is delivered
**   at the outermost leave, not before.
**
**   POSIX only: the Windows emulation delivers through rsignal_raise()
**   from another thread, which this single-threaded test cannot drive.
*/
static void
_test_crit(void)
{
#ifndef U3_OS_windows
  void (*old_f)(int) = signal(SIGINT, _crit_handler);

  _crit_hit = 0;
  u3m_crit_enter();
  raise(SIGINT);
  if ( _crit_hit ) {
    fprintf(stderr, "\033[31mcrit: delivered inside section\033[0m\r\n");
    exit(1);
  }
  u3m_crit_leave();
  if ( 1 != _crit_hit ) {
    fprintf(stderr, "\033[31mcrit: not delivered at leave\033[0m\r\n");
    exit(1);
  }

  _crit_hit = 0;
  u3m_crit_enter();
  u3m_crit_enter();
  raise(SIGINT);
  u3m_crit_leave();
  if ( _crit_hit ) {
    fprintf(stderr, "\033[31mcrit: delivered at inner leave\033[0m\r\n");
    exit(1);
  }
  u3m_crit_leave();
  if ( 1 != _crit_hit ) {
    fprintf(stderr, "\033[31mcrit: not delivered at outer leave\033[0m\r\n");
    exit(1);
  }

  signal(SIGINT, old_f);
  fprintf(stderr, "test crit: ok\r\n");
#else
  fprintf(stderr, "test crit: skipped on windows\r\n");
#endif
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
**        each bob atom through u3r_blob_load → u3r_view_read → pread.
**
**   Also exercises the ram encoder's backref path for bob atoms (same bob
**   appearing multiple times in a noun) and the dedup path through
**   install_stg (two bob atoms resolving to the same on-disk blob).
*/
static void
_test_lifecycle(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

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
  //  exercises u3r_bytes → u3r_blob_load → u3r_view_read (pread from disk).
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
  //  u3r_bytes → u3r_blob_load → u3r_view_read → pread.
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
    u3_weak mat   = _blob_load(_tmp_pier, mug1_h, seq1_h);
    if ( u3_none == mat ) {
      fprintf(stderr, "\033[31mlifecycle: load failed\033[0m\r\n");
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

/* _lease_seen / _lease_collect_cb: accumulator for u3_lmdb_walk_leases.
*/
typedef struct { c3_d bid_d; c3_d exp_d; c3_d lea_d; } _lease_seen;
static _lease_seen _lease_arr[64];
static c3_z        _lease_num;

static void
_lease_collect_cb(void* ptr_v, c3_d bid_d, c3_d exp_d, c3_d lea_d)
{
  (void)ptr_v;
  if ( _lease_num < 64 ) {
    _lease_arr[_lease_num].bid_d = bid_d;
    _lease_arr[_lease_num].exp_d = exp_d;
    _lease_arr[_lease_num].lea_d = lea_d;
  }
  _lease_num += 1;
}

static c3_z
_lease_count(MDB_env* env_u)
{
  _lease_num = 0;
  u3_lmdb_walk_leases(env_u, 0, _lease_collect_cb);
  return _lease_num;
}

/* _lease_has(): does the accumulator hold a row matching (bid, exp, lea)?
*/
static c3_o
_lease_has(c3_d bid_d, c3_d exp_d, c3_d lea_d)
{
  for ( c3_z i_z = 0; i_z < _lease_num && i_z < 64; i_z++ ) {
    if (  (_lease_arr[i_z].bid_d == bid_d)
       && (_lease_arr[i_z].exp_d == exp_d)
       && (_lease_arr[i_z].lea_d == lea_d) )
    {
      return c3y;
    }
  }
  return c3n;
}

/* _test_lease(): durable LEASES table — multiple leases per bid
**   (MDB_DUPSORT), exact-row delete, idempotency, missing-table walk.
*/
static void
_test_lease(void)
{
  _tmp_make();

  //  u3_lmdb_init opens an env at an existing directory
  //
  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);
  if ( 0 != mkdir(log_c, 0700) ) {
    fprintf(stderr, "lease: mkdir %s failed: %s\r\n", log_c, strerror(errno));
    exit(1);
  }

  MDB_env* env_u = u3_lmdb_init(log_c, 1ULL << 30);
  if ( !env_u ) {
    fprintf(stderr, "lease: lmdb init failed\r\n");
    exit(1);
  }

  //  walking a not-yet-created table yields nothing
  //
  if ( 0 != _lease_count(env_u) ) {
    fprintf(stderr, "lease: empty table not empty\r\n");
    exit(1);
  }

  c3_d bid1 = ((c3_d)0xdeadbeef << 32) | 1;
  c3_d bid2 = ((c3_d)0xdeadbeef << 32) | 2;

  //  two live leases on one blob (duplicate key) + one on another
  //
  if (  (c3n == u3_lmdb_save_lease(env_u, bid1, 100, 1))
     || (c3n == u3_lmdb_save_lease(env_u, bid1, 200, 2))
     || (c3n == u3_lmdb_save_lease(env_u, bid2, 300, 3)) )
  {
    fprintf(stderr, "lease: save failed\r\n");
    exit(1);
  }

  if ( 3 != _lease_count(env_u) ) {
    fprintf(stderr, "lease: expected 3 rows, got %zu\r\n", _lease_num);
    exit(1);
  }
  if (  (c3n == _lease_has(bid1, 100, 1))
     || (c3n == _lease_has(bid1, 200, 2))
     || (c3n == _lease_has(bid2, 300, 3)) )
  {
    fprintf(stderr, "lease: missing a row after save\r\n");
    exit(1);
  }

  //  delete one specific duplicate; its sibling under bid1 survives
  //
  if ( c3n == u3_lmdb_delete_lease(env_u, bid1, 100, 1) ) {
    fprintf(stderr, "lease: delete failed\r\n");
    exit(1);
  }
  if ( 2 != _lease_count(env_u) ) {
    fprintf(stderr, "lease: expected 2 rows after delete, got %zu\r\n",
            _lease_num);
    exit(1);
  }
  if (  (c3y == _lease_has(bid1, 100, 1))
     || (c3n == _lease_has(bid1, 200, 2))
     || (c3n == _lease_has(bid2, 300, 3)) )
  {
    fprintf(stderr, "lease: wrong duplicate deleted\r\n");
    exit(1);
  }

  //  deleting a now-absent row is idempotent success
  //
  if ( c3n == u3_lmdb_delete_lease(env_u, bid1, 100, 1) ) {
    fprintf(stderr, "lease: idempotent delete reported failure\r\n");
    exit(1);
  }
  if ( 2 != _lease_count(env_u) ) {
    fprintf(stderr, "lease: idempotent delete changed table\r\n");
    exit(1);
  }

  //  drain
  //
  u3_lmdb_delete_lease(env_u, bid1, 200, 2);
  u3_lmdb_delete_lease(env_u, bid2, 300, 3);
  if ( 0 != _lease_count(env_u) ) {
    fprintf(stderr, "lease: table not drained\r\n");
    exit(1);
  }

  u3_lmdb_exit(env_u);
  _tmp_clean();
  fprintf(stderr, "test blob lease: ok\r\n");
}

/* _lease_env(): make a fresh pier + LMDB env for a lease test.
*/
static MDB_env*
_lease_env(void)
{
  _tmp_make();

  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);
  if ( 0 != mkdir(log_c, 0700) ) {
    fprintf(stderr, "lease: mkdir %s failed: %s\r\n", log_c, strerror(errno));
    exit(1);
  }

  MDB_env* env_u = u3_lmdb_init(log_c, 1ULL << 30);
  if ( !env_u ) {
    fprintf(stderr, "lease: lmdb init failed\r\n");
    exit(1);
  }
  return env_u;
}

/* _lease_reopen(): close and reopen the env — stands in for a crash +
**   restart, exercising on-disk durability.
*/
static MDB_env*
_lease_reopen(MDB_env* env_u)
{
  u3_lmdb_exit(env_u);

  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);

  MDB_env* new_u = u3_lmdb_init(log_c, 1ULL << 30);
  if ( !new_u ) {
    fprintf(stderr, "lease: lmdb reopen failed\r\n");
    exit(1);
  }
  return new_u;
}

/* _test_lease_persist(): leases (and their deletions) survive a close +
**   reopen, and coexist with the BLOBS table in the same env.
*/
static void
_test_lease_persist(void)
{
  MDB_env* env_u = _lease_env();

  c3_d bid_a = ((c3_d)0x0a11ce << 32) | 7;
  c3_d bid_b = ((c3_d)0x000b0b << 32) | 9;

  //  two leases on bid_a (duplicate key) + one on bid_b
  //
  if (  (c3n == u3_lmdb_save_lease(env_u, bid_a, 1000, 10))
     || (c3n == u3_lmdb_save_lease(env_u, bid_a, 2000, 11))
     || (c3n == u3_lmdb_save_lease(env_u, bid_b, 3000, 12)) )
  {
    fprintf(stderr, "lease persist: save failed\r\n");
    exit(1);
  }

  //  an unrelated BLOBS row, to prove the tables are independent and
  //  maxdbs accommodates both
  //
  {
    c3_d ids_d[2] = { bid_a, bid_b };
    if ( c3n == u3_lmdb_save_blobs(env_u, 42, ids_d, 2) ) {
      fprintf(stderr, "lease persist: blobs save failed\r\n");
      exit(1);
    }
  }

  //  "crash" and restart
  //
  env_u = _lease_reopen(env_u);

  //  every lease survived, values intact
  //
  if ( 3 != _lease_count(env_u) ) {
    fprintf(stderr, "lease persist: expected 3 rows after reopen, got %zu\r\n",
            _lease_num);
    exit(1);
  }
  if (  (c3n == _lease_has(bid_a, 1000, 10))
     || (c3n == _lease_has(bid_a, 2000, 11))
     || (c3n == _lease_has(bid_b, 3000, 12)) )
  {
    fprintf(stderr, "lease persist: a lease did not survive reopen\r\n");
    exit(1);
  }

  //  the BLOBS row survived independently
  //
  {
    c3_d* out_d = 0;
    c3_z  out_z = 0;
    if (  (c3n == u3_lmdb_read_blobs(env_u, 42, &out_d, &out_z))
       || (2 != out_z)
       || (bid_a != out_d[0])
       || (bid_b != out_d[1]) )
    {
      fprintf(stderr, "lease persist: blobs row corrupt after reopen\r\n");
      exit(1);
    }
    c3_free(out_d);
  }

  //  a deletion is durable too
  //
  if ( c3n == u3_lmdb_delete_lease(env_u, bid_a, 1000, 10) ) {
    fprintf(stderr, "lease persist: delete failed\r\n");
    exit(1);
  }
  env_u = _lease_reopen(env_u);

  if ( 2 != _lease_count(env_u) ) {
    fprintf(stderr, "lease persist: deletion did not persist (%zu rows)\r\n",
            _lease_num);
    exit(1);
  }
  if ( c3y == _lease_has(bid_a, 1000, 10) ) {
    fprintf(stderr, "lease persist: deleted row reappeared\r\n");
    exit(1);
  }

  u3_lmdb_exit(env_u);
  _tmp_clean();
  fprintf(stderr, "test blob lease persist: ok\r\n");
}

/* _test_lease_dups(): many leases on one blob, deleting specific rows
**   leaves the others untouched (the multi-lease-per-bid invariant).
*/
static void
_test_lease_dups(void)
{
  MDB_env* env_u = _lease_env();

  c3_d bid_d = ((c3_d)0xd00d << 32) | 3;

  //  five concurrent leases on the same blob
  //
  for ( c3_d i_d = 0; i_d < 5; i_d++ ) {
    if ( c3n == u3_lmdb_save_lease(env_u, bid_d, 5000 + i_d, 100 + i_d) ) {
      fprintf(stderr, "lease dups: save %" PRIu64 " failed\r\n", i_d);
      exit(1);
    }
  }

  if ( 5 != _lease_count(env_u) ) {
    fprintf(stderr, "lease dups: expected 5 rows, got %zu\r\n", _lease_num);
    exit(1);
  }

  //  drop the first and last; the middle three remain
  //
  if (  (c3n == u3_lmdb_delete_lease(env_u, bid_d, 5000, 100))
     || (c3n == u3_lmdb_delete_lease(env_u, bid_d, 5004, 104)) )
  {
    fprintf(stderr, "lease dups: delete failed\r\n");
    exit(1);
  }

  if ( 3 != _lease_count(env_u) ) {
    fprintf(stderr, "lease dups: expected 3 rows after delete, got %zu\r\n",
            _lease_num);
    exit(1);
  }
  if (  (c3y == _lease_has(bid_d, 5000, 100))
     || (c3n == _lease_has(bid_d, 5001, 101))
     || (c3n == _lease_has(bid_d, 5002, 102))
     || (c3n == _lease_has(bid_d, 5003, 103))
     || (c3y == _lease_has(bid_d, 5004, 104)) )
  {
    fprintf(stderr, "lease dups: wrong rows survived\r\n");
    exit(1);
  }

  u3_lmdb_exit(env_u);
  _tmp_clean();
  fprintf(stderr, "test blob lease dups: ok\r\n");
}

/* _test_canon(): blobs store the atom's bytes, not the caller's buffer.
**
**   The mug a blob gets is memoized by u3i_blob() as the bob atom's
**   mug_w, so it must equal the mug the same atom gets in the loom.
**   Since an atom has no trailing zeros, content that differs only in
**   trailing zeros is one atom and must become one blob.
*/
static void
_test_canon(void)
{
  _tmp_make();
  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
  u3C.dir_c = _tmp_pier;

  //  same atom, three spellings.  the payload is over 8 bytes so that the
  //  atom it denotes is indirect in both bitnesses: u3r_sing's bob branch
  //  is only reached for indirect atoms, and a real blob (> U3_BLOB_THRESH)
  //  is always indirect.
  //
  const c3_y bar_y[] = "canonical bytes";
  const c3_y pad_y[] = "canonical bytes\0";
  const c3_y pud_y[] = "canonical bytes\0\0\0\0\0\0\0\0";

  c3_h bar_mug_h, bar_seq_h;
  c3_h pad_mug_h, pad_seq_h;
  c3_h pud_mug_h, pud_seq_h;

  if (  (c3y != u3_blob_save(_tmp_pier, bar_y, sizeof(bar_y),
                             &bar_mug_h, &bar_seq_h))
     || (c3y != u3_blob_save(_tmp_pier, pad_y, sizeof(pad_y),
                             &pad_mug_h, &pad_seq_h))
     || (c3y != u3_blob_save(_tmp_pier, pud_y, sizeof(pud_y),
                             &pud_mug_h, &pud_seq_h)) )
  {
    fprintf(stderr, "\033[31mblob canon: save failed\033[0m\r\n");
    exit(1);
  }

  if (  (bar_mug_h != pad_mug_h) || (bar_seq_h != pad_seq_h)
     || (bar_mug_h != pud_mug_h) || (bar_seq_h != pud_seq_h) )
  {
    fprintf(stderr, "\033[31mblob canon: trailing zeros made distinct blobs "
                    "(%" PRIc3_h "/%" PRIc3_h ", %" PRIc3_h "/%" PRIc3_h
                    ", %" PRIc3_h "/%" PRIc3_h ")\033[0m\r\n",
            bar_mug_h, bar_seq_h, pad_mug_h, pad_seq_h, pud_mug_h, pud_seq_h);
    exit(1);
  }

  //  what landed on disk is the atom's bytes, not the caller's buffer
  //  (every fixture above carries at least the string literal's own NUL)
  //
  const c3_d sig_d = strlen((const c3_c*)bar_y);
  {
    c3_c fil_c[8192];
    struct stat st_u;
    u3_blob_path(fil_c, _tmp_pier, bar_mug_h, bar_seq_h);
    if ( (0 != stat(fil_c, &st_u)) || (sig_d != (c3_d)st_u.st_size) ) {
      fprintf(stderr, "\033[31mblob canon: stored %lld bytes, wanted %" PRIc3_d
                      "\033[0m\r\n", (long long)st_u.st_size, sig_d);
      exit(1);
    }
  }

  //  the core invariant: a bob atom mugs like the loom atom it denotes
  //
  {
    u3_atom bob = u3i_blob(bar_mug_h, bar_seq_h);
    u3_atom lom = u3i_bytes(sizeof(pud_y), pud_y);

    if ( u3r_mug(bob) != u3r_mug(lom) ) {
      fprintf(stderr, "\033[31mblob canon: bob mug %" PRIc3_h
                      " != loom mug %" PRIc3_h "\033[0m\r\n",
              u3r_mug(bob), u3r_mug(lom));
      exit(1);
    }
    if ( c3y != u3r_sing(bob, lom) ) {
      fprintf(stderr, "\033[31mblob canon: bob != loom atom\033[0m\r\n");
      exit(1);
    }

    //  and two bobs for one atom are the same bob, so bob-vs-bob sing
    //  (which compares mug and blob identity only) agrees
    //
    u3_atom tob = u3i_blob(pud_mug_h, pud_seq_h);
    if ( c3y != u3r_sing(bob, tob) ) {
      fprintf(stderr, "\033[31mblob canon: bob != bob for one atom\033[0m\r\n");
      exit(1);
    }

    u3z(bob); u3z(lom); u3z(tob);
  }

  //  a blob must denote an atom the loom would also make indirect, else
  //  u3r_sing compares a pug against a cat and takes the early c3n in
  //  _cr_sing_atom.  all-zero content (atom 0) is the degenerate case.
  //
  {
    const c3_y nil_y[64] = {0};
    const c3_y sml_y[U3_BLOB_MIN - 1] = {[U3_BLOB_MIN - 2] = 0xff};
    c3_h nil_mug_h = 0, nil_seq_h = 0;
    c3_h sml_mug_h = 0, sml_seq_h = 0;

    if ( c3n != u3_blob_save(_tmp_pier, nil_y, sizeof(nil_y),
                             &nil_mug_h, &nil_seq_h) )
    {
      fprintf(stderr, "\033[31mblob canon: saved an all-zero blob\033[0m\r\n");
      exit(1);
    }
    if ( c3n != u3_blob_save(_tmp_pier, sml_y, sizeof(sml_y),
                             &sml_mug_h, &sml_seq_h) )
    {
      fprintf(stderr, "\033[31mblob canon: saved a direct-atom blob "
                      "(%zu bytes)\033[0m\r\n", sizeof(sml_y));
      exit(1);
    }

    //  one byte more is always indirect, and is accepted
    //
    const c3_y big_y[U3_BLOB_MIN] = {[U3_BLOB_MIN - 1] = 0xff};
    c3_h big_mug_h = 0, big_seq_h = 0;

    if ( c3y != u3_blob_save(_tmp_pier, big_y, sizeof(big_y),
                             &big_mug_h, &big_seq_h) )
    {
      fprintf(stderr, "\033[31mblob canon: refused a %zu-byte blob\033[0m\r\n",
              sizeof(big_y));
      exit(1);
    }
    {
      u3_atom bob = u3i_blob(big_mug_h, big_seq_h);
      u3_atom lom = u3i_bytes(sizeof(big_y), big_y);

      if ( c3n == u3a_is_pug(lom) ) {
        fprintf(stderr, "\033[31mblob canon: U3_BLOB_MIN bytes still "
                        "direct in the loom\033[0m\r\n");
        exit(1);
      }
      if ( c3y != u3r_sing(bob, lom) ) {
        fprintf(stderr, "\033[31mblob canon: floor-sized bob != loom\033[0m\r\n");
        exit(1);
      }
      u3z(bob); u3z(lom);
    }
    u3a_blob_drop(big_mug_h, big_seq_h);
  }

  //  move_stg trims too, so the staged path stores the same blob
  //
  {
    c3_c* stg_c = _write_tmp_file(pud_y, sizeof(pud_y));
    c3_h  stg_mug_h = 0;
    c3_h  stg_seq_h = 0;

    if ( c3y != u3_blob_move_stg(_tmp_pier, stg_c, &stg_mug_h, &stg_seq_h) ) {
      fprintf(stderr, "\033[31mblob canon: move_stg failed\033[0m\r\n");
      exit(1);
    }
    if ( (stg_mug_h != bar_mug_h) || (stg_seq_h != bar_seq_h) ) {
      fprintf(stderr, "\033[31mblob canon: move_stg made a distinct blob "
                      "(%" PRIc3_h "/%" PRIc3_h ")\033[0m\r\n",
              stg_mug_h, stg_seq_h);
      exit(1);
    }
    c3_free(stg_c);
  }

  //  our bobs went to zero; with no blob_del_f installed nothing drops the
  //  entry, so clear it by hand and leave the shared bank as we found it
  //
  u3a_blob_drop(bar_mug_h, bar_seq_h);

  _tmp_clean();
  fprintf(stderr, "test blob canon: ok\r\n");
}

/* main(): run all blob tests.
*/
int
main(int argc, char* argv[])
{
  (void)argc; (void)argv;
  _setup();

  _test_path();            //  no filesystem
  _test_mark_sweep();      //  first: the sweep must see a clean process
  _test_init();
  _test_stg_clean();
  _test_save_load();
  _test_dedup();
  _test_canon();
  _test_save_fd();
  _test_delete_empty_bucket();
  _test_walk();
  _test_install_stg();
  _test_install_stg_trim();
  _test_install_stg_dedup();
  _test_sane();
  _test_meld();
  _test_cue_blob();
  _test_met();
  _test_hand();
  _test_hand_dedup();
  _test_hand_bail();
  _test_hand_signal();
  _test_hand_leak();
  _test_hand_outer();
  _test_hand_many();
  _test_hand_keep();
  _test_hand_comp();
  _test_crit();
  _test_hand_alarm();
  _test_jam_bob();
  _test_jets_bob();
#ifndef U3_OS_windows
  _test_hand_intr();
  _test_hand_intr_crit();
#endif
  _test_hand_meme();
  _test_hand_cue();
#ifndef U3_OS_windows
  _test_hand_emfile();
  _test_hand_file();
  _test_hand_wipe();
#endif
  _test_hand_stop();
#ifndef U3_OS_windows
  _test_hand_trunc();
#endif
  _test_hand_gone();
  _test_hand_nest();
  _test_hand_edge();
  _test_hand_share();
  _test_hand_reuse();
  _test_hand_deep();
  _test_hand_wide();
  _test_hand_empty();
  _test_hand_access();
  _test_lifecycle();
  _test_lease();
  _test_lease_persist();
  _test_lease_dups();

  fprintf(stderr, "test blob: ok\r\n");
  return 0;
}
