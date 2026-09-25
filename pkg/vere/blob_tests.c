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
#include <stdarg.h>
#include <stdio.h>
#ifndef U3_OS_windows
#include <sys/resource.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Tests for pkg/noun/blob.c — the content-addressed blob store.
**
** Every test names itself and takes a fresh temp pier from _pier_make,
** asserts through _check, and hands the pier back through _pier_done,
** which also insists that no blob hand survived it.  u3C.dir_c points
** at that pier for the whole process, so libnoun's readers find the
** same store the test wrote; u3C.blob_del_f drops a bank record when
** its last bob dies, as mars does, so no test leaves records behind.
** Tests run sequentially and exit nonzero on first failure.
*/

static c3_c        _tmp_pier[1024];
static const c3_c* _nam_c;   //  the running test, for messages
static c3_w        _del_w;   //  deletions requested since _pier_make

/* _fail(): report a failed check in red, naming the running test, and exit.
*/
__attribute__((noreturn)) static void
_fail(const c3_c* fmt_c, ...)
{
  va_list arg_u;

  fprintf(stderr, "\033[31mblob %s: ", _nam_c);
  va_start(arg_u, fmt_c);
  vfprintf(stderr, fmt_c, arg_u);
  va_end(arg_u);
  fprintf(stderr, "\033[0m\r\n");
  exit(1);
}

/* _check(): [con] must hold; the rest is the printf for its failure.
*/
#define _check(con, ...)                                                    \
  do {                                                                      \
    if ( !(con) ) {                                                         \
      _fail(__VA_ARGS__);                                                   \
    }                                                                       \
  } while ( 0 )

/* _blob_del_cb(): the mars end of a bob's death: count the request and
**   drop the bank record.  the file is left to the pier's removal.
*/
static void
_blob_del_cb(c3_h mug_h, c3_h seq_h)
{
  _del_w += 1;
  u3a_blob_drop(mug_h, seq_h);
}

/* _setup(): init the loom and point libnoun at the test pier.
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

  u3C.dir_c      = _tmp_pier;
  u3C.blob_del_f = _blob_del_cb;
}

/* _pier_make(): name the running test and give it a fresh pier with an
**   empty store.
*/
static void
_pier_make(const c3_c* nam_c)
{
  c3_c urb_c[2048];

  _nam_c = nam_c;
  _del_w = 0;

  if ( !c3_tmp_make(_tmp_pier, sizeof(_tmp_pier), "vere-blob-test") ) {
    _fail("c3_tmp_make: %s", strerror(errno));
  }

  //  the disk code creates .urb before the store in production
  //
  snprintf(urb_c, sizeof(urb_c), "%s/.urb", _tmp_pier);
  if ( 0 != mkdir(urb_c, 0700) ) {
    _fail("mkdir %s: %s", urb_c, strerror(errno));
  }

  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);
}

/* _pier_done(): no hand survives a test; remove the pier and report.
*/
static void
_pier_done(void)
{
  _check( 0 == u3_blob_hands(), "%zu hand(s) left open", u3_blob_hands() );
  c3_tmp_kill(_tmp_pier);
  fprintf(stderr, "test blob %s: ok\r\n", _nam_c);
}

/* _path_exists(): true if [pax_c] exists on the filesystem.
*/
static c3_o
_path_exists(const c3_c* pax_c)
{
  struct stat st_u;
  return ( 0 == stat(pax_c, &st_u) ) ? c3y : c3n;
}

/* _bob_is(): the blob at (mug_h, seq_h) denotes the atom that [len_d]
**   bytes of [dat_y] denote, read back through a view.
*/
static c3_o
_bob_is(c3_h mug_h, c3_h seq_h, const c3_y* dat_y, c3_d len_d)
{
  u3_atom bob   = u3i_blob(mug_h, seq_h);
  u3_atom ref   = u3i_bytes((c3_w)len_d, dat_y);
  c3_o    sam_o = u3r_sing(bob, ref);

  u3z(ref);
  u3z(bob);
  return sam_o;
}

/* _blob_save(): install [len_d] bytes of [dat_y] the way a client does:
**   stage them, then install the staging file.  a refused install
**   leaves the staging file to its owner, so it is removed here.
*/
static c3_o
_blob_save(const c3_y* dat_y, c3_d len_d, c3_h* mug_h, c3_h* seq_h)
{
  c3_c stg_c[8192];

  if ( c3n == u3_blob_stage(_tmp_pier, dat_y, len_d, stg_c) ) {
    return c3n;
  }
  if ( c3n == u3_blob_move_stg(_tmp_pier, stg_c, mug_h, seq_h) ) {
    c3_unlink(stg_c);
    return c3n;
  }
  return c3y;
}

/* _hand_write_raw(): write a blob file directly into one bucket, no
**   locking or fsync.  [num_w] distinguishes the content.
*/
static void
_hand_write_raw(c3_h mug_h, c3_h seq_h, c3_w num_w)
{
  c3_c  pax_c[8192];
  FILE* fil_f;

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);
  if ( (0 != mkdir(pax_c, 0700)) && (EEXIST != errno) ) {
    _fail("raw mkdir: %s", strerror(errno));
  }

  u3_blob_path(pax_c, _tmp_pier, mug_h, seq_h);
  if ( !(fil_f = fopen(pax_c, "wb")) ) {
    _fail("raw fopen: %s", strerror(errno));
  }
  fprintf(fil_f, "raw blob %08" PRIc3_w " padded past the minimum length", num_w);
  fclose(fil_f);
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

//  road entry and exit, internal to manage.c
//
void u3m_leap(c3_w pad_w);
void u3m_fall(void);

#ifndef U3_OS_windows
//  a SIGINT handler that only counts, for the critical-section tests
//
static volatile sig_atomic_t _crit_hit;

static void
_crit_handler(int sig)
{
  (void)sig;
  _crit_hit++;
}
#endif

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
  _pier_make("mark+sweep");

  const c3_y dat_y[] = "the bank record must survive a mark and sweep";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  _check( c3y == _blob_save(dat_y, dat_d, &mug_h, &seq_h),
          "save failed" );

  //  hold the bob in arvo state, where the mark reaches it; a C local
  //  would itself be swept as a leak
  //
  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_noun roc = u3A->roc;
  u3A->roc    = bob;

  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  _check( blb_u, "no bank record" );

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
  _check(  (blb_u == u3a_blob_get(mug_h, seq_h))
        && (blb_u->mug_h == mug_h)
        && (blb_u->seq_h == seq_h)
        && (u3a_bob_seq(bob) == seq_h)
        && (u3r_met(3, bob) == (c3_w)dat_d),
          "bank record clobbered by sweep (mug %08" PRIx32 " seq %" PRIc3_h ")",
          blb_u->mug_h, blb_u->seq_h );

  u3A->roc = roc;
  u3z(bob);
  _pier_done();
}

/* _test_init(): u3_disk_blob_init + u3_disk_blob_stg_init create the
**   store's directories, and do so again without error.
*/
static void
_test_init(void)
{
  _pier_make("init");

  c3_c pax_c[2048];
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob", _tmp_pier);
  _check( c3y == _path_exists(pax_c), "%s missing", pax_c );
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/stg", _tmp_pier);
  _check( c3y == _path_exists(pax_c), "%s missing", pax_c );

  u3_disk_blob_init(_tmp_pier);
  u3_disk_blob_stg_init(_tmp_pier);

  _pier_done();
}

/* _test_stg_clean(): u3_disk_blob_stg_init clears leftover staging files.
*/
static void
_test_stg_clean(void)
{
  _pier_make("stg_clean");

  c3_c stg_c[2048];
  snprintf(stg_c, sizeof(stg_c), "%s/.urb/bob/stg/leftover", _tmp_pier);
  FILE* f = fopen(stg_c, "wb");
  _check( f, "setup: %s", strerror(errno) );
  fputs("junk", f);
  fclose(f);
  _check( c3y == _path_exists(stg_c), "file not created" );

  u3_disk_blob_stg_init(_tmp_pier);
  _check( c3n == _path_exists(stg_c), "%s still exists", stg_c );

  _pier_done();
}

/* _test_path(): u3_blob_path produces the expected string.
*/
static void
_test_path(void)
{
  _nam_c = "path";

  c3_c pax_c[8192];
  u3_blob_path(pax_c, "/pier", 0x12345678, 42);

  const c3_c* exp_c = "/pier/.urb/bob/305419896/42";
  _check( 0 == strcmp(pax_c, exp_c), "got %s, expected %s", pax_c, exp_c );

  fprintf(stderr, "test blob %s: ok\r\n", _nam_c);
}

/* _test_save_load(): install bytes, read them back through a bob.
*/
static void
_test_save_load(void)
{
  _pier_make("save+load");

  const c3_y dat_y[] = "the quick brown fox jumps over the lazy dog";
  const c3_d dat_d   = sizeof(dat_y) - 1;  // drop trailing NUL
  c3_h mug_h = 0;
  c3_h seq_h = 0;

  _check( c3y == _blob_save(dat_y, dat_d, &mug_h, &seq_h),
          "save failed" );
  _check( 1 == seq_h, "expected seq=1, got %" PRIc3_h, seq_h );

  c3_c fil_c[8192];
  u3_blob_path(fil_c, _tmp_pier, mug_h, seq_h);
  _check( c3y == _path_exists(fil_c), "%s missing", fil_c );
  _check( c3y == u3_blob_live(_tmp_pier, mug_h, seq_h), "live is false" );
  _check( c3y == _bob_is(mug_h, seq_h, dat_y, dat_d), "byte mismatch" );

  _pier_done();
}

/* _test_dedup(): installing identical content twice reuses the first seq.
*/
static void
_test_dedup(void)
{
  _pier_make("dedup");

  const c3_y dat_y[] = "dedup me, please and thank you";
  const c3_d dat_d   = sizeof(dat_y) - 1;

  c3_h mug1_h, mug2_h;
  c3_h seq1_h, seq2_h;

  _check( c3y == _blob_save(dat_y, dat_d, &mug1_h, &seq1_h),
          "first save failed" );
  _check( c3y == _blob_save(dat_y, dat_d, &mug2_h, &seq2_h),
          "second save failed" );
  _check( mug1_h == mug2_h, "mug changed (%" PRIc3_h " vs %" PRIc3_h ")",
          mug1_h, mug2_h );
  _check( seq1_h == seq2_h, "expected seq reuse, got %" PRIc3_h "+%" PRIc3_h,
          seq1_h, seq2_h );

  //  distinct content → distinct blob slot (may reuse bucket only if mug
  //  collides; overwhelmingly unlikely for ASCII content)
  //
  const c3_y alt_y[] = "a completely different payload";
  const c3_d alt_d   = sizeof(alt_y) - 1;
  c3_h mug3_h = 0;
  c3_h seq3_h = 0;
  _check( c3y == _blob_save(alt_y, alt_d, &mug3_h, &seq3_h),
          "alt save failed" );
  _check( (mug1_h != mug3_h) || (seq1_h != seq3_h),
          "distinct content got same blob" );

  _pier_done();
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
  _pier_make("walk");

  const c3_y one_y[] = "walk blob one";
  const c3_y two_y[] = "walk blob two";
  const c3_y tri_y[] = "walk blob three";
  c3_h mug_h[3] = {0};
  c3_h seq_h[3] = {0};

  _blob_save(one_y, sizeof(one_y) - 1, &mug_h[0], &seq_h[0]);
  _blob_save(two_y, sizeof(two_y) - 1, &mug_h[1], &seq_h[1]);
  _blob_save(tri_y, sizeof(tri_y) - 1, &mug_h[2], &seq_h[2]);

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
  _check( 3 == acc_u.len_z, "expected 3 files, got %zu", acc_u.len_z );

  for ( c3_z i_z = 0; i_z < 3; i_z++ ) {
    _check( c3y == _walk_acc_has(&acc_u, mug_h[i_z], seq_h[i_z]),
            "missing %" PRIc3_h "/%" PRIc3_h, mug_h[i_z], seq_h[i_z] );
  }

  //  after wiping one blob, the walk reports exactly the other two
  //
  u3_blob_wipe(_tmp_pier, mug_h[0], seq_h[0]);

  memset(&acc_u, 0, sizeof(acc_u));
  u3_blob_walk(_tmp_pier, &acc_u, _test_walk_cb);
  _check(  (2 == acc_u.len_z)
        && (c3n == _walk_acc_has(&acc_u, mug_h[0], seq_h[0])),
          "bad post-wipe result" );

  _pier_done();
}

/* _test_delete_empty_bucket(): delete removes file AND empty bucket.
*/
static void
_test_delete_empty_bucket(void)
{
  _pier_make("delete (empty bucket)");

  const c3_y dat_y[] = "ephemeral blob";
  c3_h mug_h = 0;
  c3_h seq_h = 0;
  _blob_save(dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  c3_c fil_c[8192], dir_c[8192];
  u3_blob_path(fil_c, _tmp_pier, mug_h, seq_h);
  snprintf(dir_c, sizeof(dir_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);
  _check( c3y == _path_exists(dir_c), "setup: bucket missing" );

  u3_blob_wipe(_tmp_pier, mug_h, seq_h);

  _check( c3n == _path_exists(fil_c), "file %s still exists", fil_c );
  _check( c3n == u3_blob_live(_tmp_pier, mug_h, seq_h), "live still true" );
  _check( c3n == _path_exists(dir_c), "bucket %s not cleaned", dir_c );

  //  deleting a nonexistent blob is a no-op (no error)
  //
  u3_blob_wipe(_tmp_pier, 0xdeadbeef, 999);

  _pier_done();
}

/* _test_install_stg(): a staging file installs once, is consumed, and
**   keeps its bytes; missing and empty staging files are refused.
*/
static void
_test_install_stg(void)
{
  _pier_make("install_stg");

  const c3_y dat_y[] = "payload installed from staging";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_c stg_c[8192];
  c3_h mug_h = 0;
  c3_h seq_h = 0;

  _check( c3y == u3_blob_stage(_tmp_pier, dat_y, dat_d, stg_c), "stage failed" );
  _check( c3y == _path_exists(stg_c), "staging file missing" );

  _check( c3y == u3_blob_move_stg(_tmp_pier, stg_c, &mug_h, &seq_h),
          "install failed" );
  _check( 1 == seq_h, "expected seq=1, got %" PRIc3_h, seq_h );
  _check( c3n == _path_exists(stg_c), "staging file not consumed" );
  _check( c3y == u3_blob_live(_tmp_pier, mug_h, seq_h),
          "blob not present after install" );
  _check( c3y == _bob_is(mug_h, seq_h, dat_y, dat_d), "byte mismatch" );

  //  a missing staging file and an empty one are refused
  //
  _check( c3n == u3_blob_move_stg(_tmp_pier, "/no/such/path", &mug_h, &seq_h),
          "installed a missing file" );

  _check( c3y == u3_blob_stage(_tmp_pier, dat_y, 0, stg_c), "empty stage failed" );
  _check( c3n == u3_blob_move_stg(_tmp_pier, stg_c, &mug_h, &seq_h),
          "installed an empty file" );
  c3_unlink(stg_c);

  _pier_done();
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
  _pier_make("install_stg_trim");

  //  significant bytes, then padding the atom does not include
  //
  c3_y dat_y[4096] = {0};
  const c3_d sig_d = 2048;

  for ( c3_d i_d = 0; i_d < sig_d; i_d++ ) {
    dat_y[i_d] = (c3_y)(1 + (i_d % 255));
  }

  c3_c stg_c[8192];
  c3_h mug_h = 0;
  c3_h seq_h = 0;
  _check( c3y == u3_blob_stage(_tmp_pier, dat_y, sizeof(dat_y), stg_c),
          "stage failed" );
  _check( c3y == u3_blob_move_stg(_tmp_pier, stg_c, &mug_h, &seq_h),
          "install failed" );

  //  the installed file is the trimmed length, not the staged one
  //
  c3_c fil_c[8192];
  u3_blob_path(fil_c, _tmp_pier, mug_h, seq_h);

  struct stat st_u;
  _check( 0 == stat(fil_c, &st_u), "stat %s: %s", fil_c, strerror(errno) );
  _check( sig_d == (c3_d)st_u.st_size,
          "expected %" PRIc3_d " bytes on disk, got %" PRIc3_d,
          sig_d, (c3_d)st_u.st_size );

  //  and it still denotes the same atom the padded bytes did
  //
  _check( c3y == _bob_is(mug_h, seq_h, dat_y, sizeof(dat_y)), "atom mismatch" );

  _pier_done();
}

/* _test_stage(): both of the king's staging helpers feed
**   u3_blob_move_stg, and the two land on one blob: same mug, same seq
**   by dedup, staging file consumed.
*/
static void
_test_stage(void)
{
  _pier_make("stage");

  const c3_y dat_y[] = "staged by a client, installed by mars, one blob";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  c3_c stg_c[8192];

  //  from a buffer: the staging file lands in the staging directory
  //
  {
    c3_c dir_c[8192];

    u3_blob_stg_dir(dir_c, _tmp_pier);

    _check(  (c3y == u3_blob_stage(_tmp_pier, dat_y, dat_d, stg_c))
          && (0 == strncmp(stg_c, dir_c, strlen(dir_c)))
          && (c3y == _path_exists(stg_c)),
            "buffer stage failed" );
    _check(  (c3y == u3_blob_move_stg(_tmp_pier, stg_c, &mug_h, &seq_h))
          && (1 == seq_h)
          && (c3n == _path_exists(stg_c)),
            "install of a staged buffer failed" );
  }

  //  from a descriptor: the same bytes dedup onto the same blob
  //
  {
    c3_h mug2_h = 0; c3_h seq2_h = 0;
    c3_c src_c[8192];
    c3_i fid_i;

    snprintf(src_c, sizeof(src_c), "%s/source.bin", _tmp_pier);
    fid_i = c3_open(src_c, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    _check( (fid_i >= 0) && ((ssize_t)dat_d == write(fid_i, dat_y, (size_t)dat_d)),
            "source write failed" );
    close(fid_i);
    fid_i = c3_open(src_c, O_RDONLY, 0);

    _check(  (c3y == u3_blob_stage_fd(_tmp_pier, fid_i, dat_d, stg_c))
          && (c3y == u3_blob_move_stg(_tmp_pier, stg_c, &mug2_h, &seq2_h))
          && (mug2_h == mug_h) && (seq2_h == seq_h)
          && (c3n == _path_exists(stg_c)),
            "install of a staged descriptor differs from the buffer's" );
    close(fid_i);

    //  a short source fails and leaves nothing behind
    //
    fid_i = c3_open(src_c, O_RDONLY, 0);
    _check(  (c3n == u3_blob_stage_fd(_tmp_pier, fid_i, dat_d + 1, stg_c))
          && (c3n == _path_exists(stg_c)),
            "short source not refused" );
    close(fid_i);
  }

  _pier_done();
}

/* _test_sane(): u3a_blob_sane catches counter corruption.
*/
static void
_test_sane(void)
{
  _pier_make("sane");

  const c3_y dat_y[] = "sane test blob";
  c3_h mug_h = 0, seq_h = 0;
  _blob_save(dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  //  one live atom in the kernel root, plus a synthetic log ref
  //
  u3_noun bob = u3i_blob(mug_h, seq_h);
  u3_noun old = u3A->roc;
  u3A->roc    = u3nc(bob, u3_nul);

  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  blb_u->eve_w += 1;
  blb_u->use_w += 1;

  _check( c3y == u3a_blob_sane(c3y), "balanced bank reported corrupt" );

  //  cheap tier: use_w below the durable floor
  //
  blb_u->use_w -= 2;
  _check( c3n == u3a_blob_sane(c3n), "use < eve+les not caught" );

  //  deep tier: counters look plausible but cardinality is wrong
  //
  blb_u->use_w += 3;   //  use = eve + les + 2, only 1 live atom
  _check( c3n == u3a_blob_sane(c3y), "cardinality mismatch not caught" );
  blb_u->use_w -= 1;

  //  drop the atom: the log ref keeps the record, and no deletion is asked
  //
  u3z(u3A->roc);
  u3A->roc = old;
  _check( 0 == _del_w, "blob deleted with eve_w held" );

  blb_u = u3a_blob_get(mug_h, seq_h);
  blb_u->eve_w = 0;
  blb_u->use_w = 0;
  u3a_blob_drop(mug_h, seq_h);

  _pier_done();
}

/* _test_meld(): |meld unifies duplicate bob atoms and preserves the bank.
*/
static void
_test_meld(void)
{
  _pier_make("meld");

  const c3_y dat_y[] = "meld test blob";
  c3_h mug_h = 0, seq_h = 0;
  _blob_save(dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

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
    _check( 3 == blb_u->use_w, "setup use_w %" PRIc3_w " != 3", blb_u->use_w );
  }

  (void)u3_meld_all(0, c3n, c3n);

  //  the duplicates must be unified (one atom box), the freed copy's
  //  cardinality decremented, the bank entry and log ref intact, and
  //  the surviving atom's blob pointer valid post-pack
  //
  {
    u3a_cell* cel_u = u3a_to_ptr(u3A->roc);
    _check( cel_u->hed == cel_u->tel, "duplicate bobs not unified" );
  }

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    _check( blb_u, "bank entry lost" );
    _check( (2 == blb_u->use_w) && (1 == blb_u->eve_w),
            "counts use=%" PRIc3_w " eve=%" PRIc3_w " want use=2 eve=1",
            blb_u->use_w, blb_u->eve_w );
    _check(  (mug_h == u3a_bob_mug(u3h(u3A->roc)))
          && (seq_h == u3a_bob_seq(u3h(u3A->roc))),
            "bob blob pointer stale" );
  }

  _check( 0 == _del_w, "blob deleted with refs held" );

  //  drop the last atom: cardinality reaches 0, but eve_w must keep
  //  the file alive (no deletion request)
  //
  u3z(u3A->roc);
  u3A->roc = old;

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    _check( (1 == blb_u->use_w) && (0 == _del_w),
            "post-drop use=%" PRIc3_w " del=%" PRIc3_w, blb_u->use_w, _del_w );

    //  release the log ref: now deletion is legitimate
    //
    blb_u->eve_w = 0;
    blb_u->use_w = 0;
    u3a_blob_drop(mug_h, seq_h);
  }

  _pier_done();
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
  _pier_make("cue");

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

    _check( u3_none != out, "cue failed" );
    _check( 1 == bsk_u.num_w, "%" PRIc3_w " installs, want 1", bsk_u.num_w );
  }

  u3_noun hed = u3h(out);
  u3_noun mid = u3h(u3t(out));
  u3_noun tel = u3t(u3t(out));

  _check( (c3y == u3a_is_bob(hed)) && (c3y == u3a_is_bob(tel)),
          "large atoms not blobified" );
  _check( hed == tel, "backref not shared" );
  _check( 0x77 == mid, "small atom mangled" );

  //  bytes must round-trip through the blob file
  //
  {
    c3_y* git_y = c3_malloc(len_z);
    u3r_bytes(0, (c3_w)len_z, git_y, hed);
    _check( 0 == memcmp(git_y, dat_y, len_z), "bytes mangled" );
    c3_free(git_y);
  }

  //  structural equality with the original, and bank consistency:
  //  one live bob atom, no log refs
  //
  _check( c3y == u3r_sing(ref, out), "result != original" );

  {
    c3_h mug_h = u3a_bob_mug(hed);
    c3_h seq_h = u3a_bob_seq(hed);

    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    _check( blb_u && (1 == blb_u->use_w), "bad bank entry" );

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
      _check( (mug_h == u3a_bob_mug(bob)) && (seq_h == u3a_bob_seq(bob)),
              "re-cue did not dedup" );
      _check( 2 == blb_u->use_w, "use_w %" PRIc3_w " after re-cue, want 2",
              blb_u->use_w );
      u3z(two);
    }
  }

  //  no log refs: the last atom's death requests exactly one deletion,
  //  and the record goes with it
  //
  u3z(out);
  u3z(ref);
  _check( 1 == _del_w, "%" PRIc3_w " deletions requested, want 1", _del_w );

  c3_free(jam_y);
  c3_free(dat_y);
  _pier_done();
}

/* _test_met(): u3r_met_d on a bob agrees with u3r_met on the loom atom,
**   at bloq 0 and 3, with and without trailing zeros in the file.
*/
static void
_test_met(void)
{
  _pier_make("met");

  //  16 bytes, top byte = 0x01 (1 significant bit): met = 15*8 + 1 = 121
  //
  {
    const c3_y dat_y[] = { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x9a,
                           0xbc, 0xde, 0xf0, 0x11, 0x22, 0x33, 0x44, 0x01 };
    c3_h mug_h = 0; c3_h seq_h = 0;
    _check( c3y == _blob_save(dat_y, sizeof(dat_y), &mug_h, &seq_h),
            "dense save failed" );

    u3_atom bob = u3i_blob(mug_h, seq_h);
    u3_atom ref = u3i_bytes(sizeof(dat_y), dat_y);

    _check( 121 == u3r_met_d(0, bob), "dense got %" PRIc3_d ", expected 121",
            u3r_met_d(0, bob) );
    _check(  (u3r_met_d(0, bob) == (c3_d)u3r_met(0, ref))
          && (u3r_met_d(3, bob) == (c3_d)u3r_met(3, ref))
          && (16 == u3r_met(3, bob)),
            "dense bob disagrees with loom atom" );

    u3z(bob); u3z(ref);
  }

  //  trailing zeros: 16 significant bytes with a 0xff top byte: 128 bits
  //
  {
    const c3_y dat_y[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0x00, 0x00, 0x00, 0x00 };
    c3_h mug_h = 0; c3_h seq_h = 0;
    _check( c3y == _blob_save(dat_y, sizeof(dat_y), &mug_h, &seq_h),
            "trailing-zero save failed" );

    u3_atom bob = u3i_blob(mug_h, seq_h);
    _check( (128 == u3r_met_d(0, bob)) && (16 == u3r_met_d(3, bob)),
            "trailing-zero got %" PRIc3_d ", expected 128", u3r_met_d(0, bob) );
    u3z(bob);
  }

  _pier_done();
}

/* _test_hand(): u3_blob_open/data/read/met/close round-trip.
*/
static void
_test_hand(void)
{
  _pier_make("hand");

  const c3_y dat_y[] = "handle bytes should round-trip exactly";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  _blob_save(dat_y, dat_d, &mug_h, &seq_h);

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  _check( han_u, "open returned NULL" );
  _check( (han_u->len_d == dat_d) && (1 == u3_blob_hands()),
          "len %" PRIc3_d " hands %zu", han_u->len_d, u3_blob_hands() );

  //  bit-length: top byte is the last one, u3r_met(0) semantics
  //
  {
    c3_y top_y = dat_y[dat_d - 1];
    c3_d exp_d = (dat_d - 1) * 8
               + (c3_d)(8 - (__builtin_clz((unsigned int)top_y) - 24));
    c3_d met_d = u3_blob_hand_met(han_u);
    _check( met_d == exp_d, "met %" PRIc3_d " != %" PRIc3_d, met_d, exp_d );
  }

  //  whole-file mapping, then a window through pread
  //
  {
    const c3_y* buf_y = u3_blob_data(han_u, 0);
    _check( buf_y && (0 == memcmp(buf_y, dat_y, dat_d)), "data mismatch" );
    _check( buf_y == u3_blob_data(han_u, 0), "data not memoized" );

    c3_y win_y[8];
    _check(  (5 == u3_blob_read(han_u, 7, win_y, 5))
          && (0 == memcmp(win_y, dat_y + 7, 5)),
            "read window mismatch" );

    //  a read past the end is short, not an error
    //
    _check( 3 == u3_blob_read(han_u, dat_d - 3, win_y, 8),
            "read past end not short" );
  }

  c3_i fid_i = han_u->fid_i;
  u3_blob_close(han_u);
  _check( (0 == u3_blob_hands()) && (c3y == _fd_dead(fid_i)),
          "close left hands %zu, fd open", u3_blob_hands() );

  //  missing blob: NULL, no bail, nothing left in the table
  //
  _check( !u3_blob_open(_tmp_pier, 0xdeadbeef, 999) && !u3_blob_hands(),
          "missing should be NULL" );

  _pier_done();
}

/* _test_hand_dedup(): the home road never shares: two opens of one
**   blob are two hands and two fds, each released by its own close.
*/
static void
_test_hand_dedup(void)
{
  _pier_make("hand dedup");

  const c3_y dat_y[] = "one fd per open on the home road";
  c3_h mug_h = 0; c3_h seq_h = 0;
  _blob_save(dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h);

  u3_blob_hand* one_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  u3_blob_hand* two_u = u3_blob_open(_tmp_pier, mug_h, seq_h);

  _check(  one_u && two_u && (one_u != two_u)
        && (one_u->fid_i != two_u->fid_i) && (2 == u3_blob_hands()),
          "home road shared" );

  c3_i one_i = one_u->fid_i;
  c3_i two_i = two_u->fid_i;
  u3_blob_close(one_u);

  _check(  (1 == u3_blob_hands())
        && (c3y == _fd_dead(one_i)) && (c3y == _fd_live(two_i)),
          "first close disturbed the other" );

  {
    c3_y byt_y;
    _check( 1 == u3_blob_read(two_u, 0, &byt_y, 1), "survivor unreadable" );
  }

  u3_blob_close(two_u);
  _check( !u3_blob_hands() && (c3y == _fd_dead(two_i)), "last close leaked" );

  _pier_done();
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
  _check( han_u, "inner open failed" );
  _han_fid_i = han_u->fid_i;

  u3r_view vue_u;
  u3r_view_init(&vue_u, _han_bob);
  _check( vue_u.han_u == han_u, "view did not land on the road's hand" );

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
  _pier_make(nam_c);

  const c3_y dat_y[] = "unwinding must release every hand the road held";
  _check( c3y == _blob_save(dat_y, sizeof(dat_y) - 1,
                              &_han_mug_h, &_han_seq_h),
          "save failed" );
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);
}

/* _hand_unwind_check(): after the inner road is gone, nothing is open.
*/
static void
_hand_unwind_check(void)
{
  _check( !u3_blob_hands() && (c3y == _fd_dead(_han_fid_i)),
          "%zu hand(s) survived the road", u3_blob_hands() );
  u3z(_han_bob);
  _pier_done();
}

/* _test_hand_bail(): u3m_bail sweeps the bailing road's hands.
*/
static void
_test_hand_bail(void)
{
  _hand_unwind_setup("hand bail");
  u3z(u3m_soft_top(0, 1 << 12, _hand_bail_cb, 0));
  _hand_unwind_check();
}

/* _test_hand_signal(): a signal unwind sweeps every child road's hands.
*/
static void
_test_hand_signal(void)
{
  _hand_unwind_setup("hand signal");
  u3z(u3m_soft_top(0, 1 << 12, _hand_signal_cb, 0));
  _hand_unwind_check();
}

/* _test_hand_leak(): a normal return without a close is drained at fall.
*/
static void
_test_hand_leak(void)
{
  _hand_unwind_setup("hand leak");
  u3z(u3m_soft_top(0, 1 << 12, _hand_leak_cb, 0));
  _hand_unwind_check();
}

/* _test_hand_outer(): a home-road reference survives an inner bail.
*/
static void
_test_hand_outer(void)
{
  _hand_unwind_setup("hand outer");

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  _check( han_u, "home open failed" );

  u3z(u3m_soft_top(0, 1 << 12, _hand_bail_cb, 0));

  //  the inner road opened its own hand on the blob; only that went
  //
  _check( (1 == u3_blob_hands()) && (c3y == _fd_live(han_u->fid_i)),
          "home hand swept (hands %zu)", u3_blob_hands() );

  c3_i fid_i = han_u->fid_i;
  u3_blob_close(han_u);
  _han_fid_i = fid_i;
  _hand_unwind_check();
}

/* _test_hand_many(): hundreds of distinct hands open at once, then drained.
*/
static void
_test_hand_many(void)
{
  _pier_make("hand many");

  const c3_h mug_h = 0x1234;
  const c3_w num_w = 300;

  for ( c3_w i_w = 1; i_w <= num_w; i_w++ ) {
    _hand_write_raw(mug_h, i_w, i_w);
  }

  u3_blob_hand** han_u = c3_malloc(num_w * sizeof(*han_u));

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    han_u[i_w] = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    _check( han_u[i_w], "open %" PRIc3_w " failed", i_w );
  }
  _check( num_w == u3_blob_hands(), "%zu hands, expected %" PRIc3_w,
          u3_blob_hands(), num_w );

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    u3_blob_close(han_u[i_w]);
  }
  _check( !u3_blob_hands(), "%zu hands left", u3_blob_hands() );

  c3_free(han_u);
  _pier_done();
}

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
    _check( han_u, "open %" PRIc3_w " failed", i_w );
    u3_blob_close(han_u);
  }

  c3_z kep_z = u3_blob_hands_road(u3R);
  _check( (kep_z < num_w) && (kep_z >= 100), "%zu retained", kep_z );

  //  the newest is still open; the oldest was evicted and reopens
  //
  {
    u3_blob_hand* new_u = u3_blob_open(_tmp_pier, mug_h, num_w);
    u3_blob_hand* old_u = u3_blob_open(_tmp_pier, mug_h, 1);
    _check( new_u && old_u && (kep_z == u3_blob_hands_road(u3R)),
            "reopen changed the count" );
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
  _pier_make("hand keep");

  for ( c3_w i_w = 1; i_w <= 300; i_w++ ) {
    _hand_write_raw(0x1234, i_w, i_w);
  }

  u3z(u3m_soft_top(0, 1 << 12, _hand_keep_cb, 0));

  _pier_done();
}

/* _test_hand_comp(): windowed comparison of bobs against bobs and loom atoms.
*/
static void
_test_hand_comp(void)
{
  _pier_make("hand comp");

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

  c3_h am_h = 0, as_h = 0, bm_h = 0, bs_h = 0, cm_h = 0, cs_h = 0;
  _check(  (c3y == _blob_save(a_y, len_w, &am_h, &as_h))
        && (c3y == _blob_save(b_y, len_w, &bm_h, &bs_h))
        && (c3y == _blob_save(c_y, len_w, &cm_h, &cs_h)),
          "save failed" );

  u3_atom boa = u3i_blob(am_h, as_h);
  u3_atom bob = u3i_blob(bm_h, bs_h);
  u3_atom boc = u3i_blob(cm_h, cs_h);
  u3_atom loa = u3i_bytes(len_w, a_y);

  _check( c3y == u3r_sing(boa, loa),  "bob != equal loom atom" );
  _check( c3y == u3r_sing(loa, boa),  "loom atom != equal bob" );
  _check( c3n == u3r_sing(boa, bob),  "top-byte difference missed" );
  _check( c3n == u3r_sing(boa, boc),  "mid-byte difference missed" );
  _check( c3n == u3r_sing(loa, bob),  "loom vs bob difference missed" );

  _check( 0  == u3r_comp(boa, loa),   "comp bob vs equal loom" );
  _check( -1 == u3r_comp(boa, bob),   "comp top byte a < b" );
  _check( 1  == u3r_comp(bob, boa),   "comp top byte b > a" );
  _check( -1 == u3r_comp(boa, boc),   "comp mid byte a < c" );
  _check( 1  == u3r_comp(bob, loa),   "comp bob vs smaller loom" );

  _check( 1 == u3r_nord(boa, loa),    "nord equal" );
  _check( 0 == u3r_nord(boa, bob),    "nord a < b" );
  _check( 2 == u3r_nord(bob, boa),    "nord b > a" );

  _check( 0x40     == u3r_byte(len_w - 1, boa), "byte at top" );
  _check( a_y[100] == u3r_byte(100, boa),       "byte in middle" );
  _check( 0        == u3r_byte(len_w + 5, boa), "byte past end" );

  u3z(boa); u3z(bob); u3z(boc); u3z(loa);
  c3_free(a_y); c3_free(b_y); c3_free(c_y);
  _pier_done();
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
  _hand_unwind_setup("hand alarm");

  u3_noun gon = u3m_soft_top(20, 1 << 12, _hand_alarm_cb, 0);
  _check( c3__alrm == _hand_mote(gon), "no %%alrm unwind" );
  u3z(gon);

  //  the table is usable after the unwind
  //
  {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
    _check( han_u, "reopen after unwind failed" );
    u3_blob_close(han_u);
  }

  _hand_unwind_check();
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
  _pier_make("jam");

  const c3_w len_w = (5 * 8192) + 1235;
  c3_y*      dat_y = c3_malloc(len_w);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + ((i_w * 13) % 253));
  }
  dat_y[len_w - 1] = 0x05;

  c3_h mug_h = 0; c3_h seq_h = 0;
  _check( c3y == _blob_save(dat_y, len_w, &mug_h, &seq_h),
          "save failed" );

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

    _check(  (bib_w == bil_w)
          && (0 == memcmp(sab_u.buf_y, sal_u.buf_y, (bib_w + 7) >> 3)),
            "fib encoding differs (%" PRIc3_w " vs %" PRIc3_w " bits)",
            bib_w, bil_w );
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

    _check( (leb_d == lel_d) && (0 == memcmp(byb_y, byl_y, (size_t)leb_d)),
            "xeno encoding differs (%" PRIc3_d " vs %" PRIc3_d " bytes)",
            leb_d, lel_d );

    u3_weak cue = u3s_cue_xeno(leb_d, byb_y);
    _check( (u3_none != cue) && (c3y == u3r_sing(cue, lon)),
            "cue of bob jam != source" );
    u3z(cue);
    c3_free(byb_y);
    c3_free(byl_y);
  }

  u3z(bon); u3z(lon); u3z(bob); u3z(loa);
  c3_free(dat_y);
  _pier_done();
}

/* _jet_same(): a jet's results over a bob and over the loom atom are one
**   noun; both are released.
*/
static void
_jet_same(const c3_c* nam_c, c3_w a_w, c3_w b_w, c3_w c_w, u3_noun rb, u3_noun rl)
{
  _check( c3y == u3r_sing(rb, rl),
          "%s(%" PRIc3_w ", %" PRIc3_w ", %" PRIc3_w ") differs",
          nam_c, a_w, b_w, c_w );
  u3z(rb);
  u3z(rl);
}

/* _test_jets_bob(): rsh, end, and cut read ranges from a bob the same
**   way they do from the materialized atom, including ranges that touch,
**   cross, and lie beyond the end of the file.
*/
static void
_test_jets_bob(void)
{
  _pier_make("jets");

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + ((i_w * 7) % 251));
  }

  c3_h mug_h = 0; c3_h seq_h = 0;
  _check( c3y == _blob_save(dat_y, len_w, &mug_h, &seq_h),
          "save failed" );

  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_atom loa = u3i_bytes(len_w, dat_y);

  //  rsh: byte, word, and bit bloqs; offsets inside, at, and past the end
  //
  {
    const c3_w off_w[] = { 0, 1, 5, 4096, 4999, 5000, 5010 };
    for ( c3_w i_w = 0; i_w < sizeof(off_w) / sizeof(*off_w); i_w++ ) {
      _jet_same("rsh", 3, off_w[i_w], 0,
                u3qc_rsh(3, off_w[i_w], bob), u3qc_rsh(3, off_w[i_w], loa));
    }
    const c3_w wof_w[] = { 0, 3, 1249, 1250, 1251 };
    for ( c3_w i_w = 0; i_w < sizeof(wof_w) / sizeof(*wof_w); i_w++ ) {
      _jet_same("rsh", 5, wof_w[i_w], 0,
                u3qc_rsh(5, wof_w[i_w], bob), u3qc_rsh(5, wof_w[i_w], loa));
    }
    _jet_same("rsh", 0, 13, 0, u3qc_rsh(0, 13, bob), u3qc_rsh(0, 13, loa));
  }

  //  end: same shapes
  //
  {
    const c3_w len_y[] = { 0, 1, 7, 4096, 4999, 5000, 5010 };
    for ( c3_w i_w = 0; i_w < sizeof(len_y) / sizeof(*len_y); i_w++ ) {
      _jet_same("end", 3, len_y[i_w], 0,
                u3qc_end(3, len_y[i_w], bob), u3qc_end(3, len_y[i_w], loa));
    }
    const c3_w wen_w[] = { 1, 1249, 1250, 1251 };
    for ( c3_w i_w = 0; i_w < sizeof(wen_w) / sizeof(*wen_w); i_w++ ) {
      _jet_same("end", 5, wen_w[i_w], 0,
                u3qc_end(5, wen_w[i_w], bob), u3qc_end(5, wen_w[i_w], loa));
    }
    _jet_same("end", 0, 13, 0, u3qc_end(0, 13, bob), u3qc_end(0, 13, loa));
  }

  //  cut: ranges inside, touching, crossing, and beyond the end
  //
  {
    const c3_w cut_w[][2] = {
      { 0, 0 }, { 0, 1 }, { 0, 5000 }, { 10, 4000 }, { 4095, 2 },
      { 4990, 10 }, { 4990, 20 }, { 5000, 5 }, { 6000, 5 }
    };
    for ( c3_w i_w = 0; i_w < sizeof(cut_w) / sizeof(*cut_w); i_w++ ) {
      _jet_same("cut", 3, cut_w[i_w][0], cut_w[i_w][1],
                u3qc_cut(3, cut_w[i_w][0], cut_w[i_w][1], bob),
                u3qc_cut(3, cut_w[i_w][0], cut_w[i_w][1], loa));
    }
    const c3_w wut_w[][2] = { { 0, 1 }, { 1249, 2 }, { 1250, 1 }, { 1251, 3 } };
    for ( c3_w i_w = 0; i_w < sizeof(wut_w) / sizeof(*wut_w); i_w++ ) {
      _jet_same("cut", 5, wut_w[i_w][0], wut_w[i_w][1],
                u3qc_cut(5, wut_w[i_w][0], wut_w[i_w][1], bob),
                u3qc_cut(5, wut_w[i_w][0], wut_w[i_w][1], loa));
    }
    _jet_same("cut", 0, 3, 17, u3qc_cut(0, 3, 17, bob), u3qc_cut(0, 3, 17, loa));
  }

  u3z(bob); u3z(loa);
  c3_free(dat_y);
  _pier_done();
}

#ifndef U3_OS_windows
//  set by _hand_intr_crit_cb once the held SIGINT has been survived
//
static c3_w _han_hit_w;

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
  _hand_unwind_setup("hand intr");

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_intr_cb, 0);
  _check( c3__intr == _hand_mote(gon), "no %%intr unwind" );
  u3z(gon);

  _hand_unwind_check();
}

/* _test_hand_intr_crit(): a SIGINT raised inside a critical section is
**   held until the section ends, then unwinds through the production
**   handler; the hold count is back at zero afterwards.
*/
static void
_test_hand_intr_crit(void)
{
  _hand_unwind_setup("hand intr crit");
  _han_hit_w = 0;

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_intr_crit_cb, 0);
  _check( (c3__intr == _hand_mote(gon)) && (1 == _han_hit_w),
          "hit %" PRIc3_w ", expected the unwind at leave", _han_hit_w );
  u3z(gon);

  //  the hold count is zero: a fresh section delivers at its own leave
  //
  {
    void (*old_f)(int) = signal(SIGINT, _crit_handler);
    _crit_hit = 0;
    u3m_crit_enter();
    raise(SIGINT);
    _check( 0 == _crit_hit, "hold count not reset" );
    u3m_crit_leave();
    signal(SIGINT, old_f);
    _check( 1 == _crit_hit, "section not delivering" );
  }

  _hand_unwind_check();
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
  _hand_unwind_setup("hand meme");

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_meme_cb, 0);
  _check( c3y == _hand_unwound(gon), "pad did not bail" );
  u3z(gon);

  _hand_unwind_check();
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
  _pier_make("hand cue");

  //  an atom tag followed by a run-length prefix that never terminates
  //
  c3_y dat_y[64];
  memset(dat_y, 0, sizeof(dat_y));
  dat_y[sizeof(dat_y) - 1] = 0x80;

  _check( c3y == _blob_save(dat_y, sizeof(dat_y),
                              &_han_mug_h, &_han_seq_h),
          "save failed" );
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_cue_cb, 0);
  _check( c3y == _hand_unwound(gon), "garbage cued" );
  u3z(gon);

  _hand_unwind_check();
}

#ifndef U3_OS_windows
/* _test_hand_emfile(): running out of descriptors fails an open cleanly.
*/
static void
_test_hand_emfile(void)
{
  _pier_make("hand emfile");

  const c3_h mug_h = 0x2345;
  const c3_w num_w = 32;

  for ( c3_w i_w = 1; i_w <= num_w; i_w++ ) {
    _hand_write_raw(mug_h, i_w, i_w);
  }

  struct rlimit old_u, new_u;
  _check( 0 == getrlimit(RLIMIT_NOFILE, &old_u), "getrlimit failed" );
  new_u = old_u;
  new_u.rlim_cur = 24;
  _check( 0 == setrlimit(RLIMIT_NOFILE, &new_u), "setrlimit failed" );

  u3_blob_hand** han_u = c3_malloc(num_w * sizeof(*han_u));
  c3_w           got_w = 0;

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    han_u[i_w] = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    if ( han_u[i_w] ) {
      got_w++;
    }
  }

  setrlimit(RLIMIT_NOFILE, &old_u);

  _check( (got_w != num_w) && (got_w == u3_blob_hands()),
          "%" PRIc3_w " opened, %zu in table", got_w, u3_blob_hands() );

  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    if ( han_u[i_w] ) {
      c3_y byt_y;
      _check( 1 == u3_blob_read(han_u[i_w], 0, &byt_y, 1),
              "opened hand unreadable" );
      u3_blob_close(han_u[i_w]);
    }
  }

  c3_free(han_u);
  _pier_done();
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
    _check( han_u, "idle eviction did not free a descriptor" );
    u3_blob_close(han_u);
  }

  //  held hands cannot be evicted: the road runs out and bails
  //
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    (void)u3_blob_open(_tmp_pier, mug_h, i_w + 1);
  }

  _fail("no bail at the ceiling");
  return 0;
}

/* _test_hand_file(): at the descriptor ceiling an inner road evicts its
**   idle hands, and bails %file once none are idle.
*/
static void
_test_hand_file(void)
{
  _pier_make("hand file");

  for ( c3_w i_w = 1; i_w <= 32; i_w++ ) {
    _hand_write_raw(0x2345, i_w, i_w);
  }

  struct rlimit old_u, new_u;
  getrlimit(RLIMIT_NOFILE, &old_u);
  new_u = old_u;
  new_u.rlim_cur = 24;
  _check( 0 == setrlimit(RLIMIT_NOFILE, &new_u), "setrlimit failed" );

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_file_cb, 0);

  setrlimit(RLIMIT_NOFILE, &old_u);

  _check( c3__file == _hand_mote(gon), "no %%file unwind" );
  u3z(gon);

  _pier_done();
}

/* _test_hand_wipe(): wiping a blob under a live hand unlinks the file,
**   warns, and leaves the reader on the surviving inode.
*/
static void
_test_hand_wipe(void)
{
  _pier_make("hand wipe");

  const c3_y dat_y[] = "wiped under a live hand, still readable";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  _blob_save(dat_y, dat_d, &mug_h, &seq_h);

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  _check( han_u, "open failed" );
  c3_i fid_i = han_u->fid_i;

  u3_blob_wipe(_tmp_pier, mug_h, seq_h);
  _check( c3n == u3_blob_live(_tmp_pier, mug_h, seq_h), "file survived" );

  //  posix keeps the inode for the open descriptor
  //
  {
    c3_y buf_y[64];
    _check(  (dat_d == u3_blob_read(han_u, 0, buf_y, (c3_z)dat_d))
          && (0 == memcmp(buf_y, dat_y, (size_t)dat_d)),
            "read after wipe failed" );
  }

  u3_blob_close(han_u);
  _check( !u3_blob_hands() && (c3y == _fd_dead(fid_i)), "close leaked" );

  //  a second open finds nothing
  //
  _check( !u3_blob_open(_tmp_pier, mug_h, seq_h), "reopened a wiped blob" );

  _pier_done();
}
#endif

/* _test_hand_stop(): u3_blob_stop releases every home-road hand and
**   leaves the list usable.
*/
static void
_test_hand_stop(void)
{
  _pier_make("hand stop");

  const c3_h mug_h = 0x3456;
  c3_i       fid_i[3];

  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    _hand_write_raw(mug_h, i_w + 1, i_w);
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, i_w + 1);
    _check( han_u, "open failed" );
    fid_i[i_w] = han_u->fid_i;
  }

  u3_blob_stop();

  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    _check( c3y == _fd_dead(fid_i[i_w]), "fd %" PRIc3_w " survived", i_w );
  }
  _check( !u3_blob_hands(), "residue after stop" );

  {
    u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, 1);
    _check( han_u, "open after stop failed" );
    u3_blob_close(han_u);
  }

  _pier_done();
}

#ifndef U3_OS_windows
/* _test_hand_trunc(): every reader reports a file shortened under it.
*/
static void
_test_hand_trunc(void)
{
  _pier_make("hand trunc");

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + (i_w % 200));
  }

  c3_h mug_h = 0; c3_h seq_h = 0;
  _blob_save(dat_y, len_w, &mug_h, &seq_h);

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, mug_h, seq_h);
  _check( han_u, "open failed" );

  {
    //  blob files are created read-only; shortening one needs write bits
    //
    c3_c pax_c[8192];
    u3_blob_path(pax_c, _tmp_pier, mug_h, seq_h);
    _check( (0 == chmod(pax_c, 0600)) && (0 == truncate(pax_c, 100)),
            "truncate failed" );
  }

  {
    c3_y* buf_y = c3_malloc(len_w);
    _check( 100 == u3_blob_read(han_u, 0, buf_y, len_w), "read not short" );
    c3_free(buf_y);
  }

  _check( !u3_blob_data(han_u, 0) && !han_u->map_y, "data did not fail cleanly" );
  _check( 0 == u3_blob_hand_met(han_u), "met not zero" );

  u3_blob_close(han_u);

  c3_free(dat_y);
  _pier_done();
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
  _check( c3__fail == _hand_mote(gon), "%s did not bail %%fail", nam_c );
  u3z(gon);
  _check( !u3_blob_hands(), "%s left residue", nam_c );
}

/* _test_hand_gone(): a bob whose file is missing fails at every entry
**   point the way its contract says: bail %fail for readers, u3_none
**   for load, a declined open for a windowed view.
*/
static void
_test_hand_gone(void)
{
  _pier_make("hand gone");

  _han_mug_h = 0x4567;
  _han_seq_h = 1;
  _han_bob   = u3i_blob(_han_mug_h, _han_seq_h);

  _hand_gone_expect("view", _hand_gone_view_cb);
  _hand_gone_expect("met",  _hand_gone_met_cb);
  _hand_gone_expect("xeno", _hand_gone_xeno_cb);
  _hand_gone_expect("fib",  _hand_gone_fib_cb);
  _hand_gone_expect("byte", _hand_gone_byte_cb);

  //  a windowed view and a load report the missing file without bailing
  //
  {
    u3r_view vue_u;
    _check( (c3n == u3r_view_open(&vue_u, _han_bob)) && !u3_blob_hands(),
            "open did not decline" );
    _check( (u3_none == u3r_blob_load(_han_bob)) && !u3_blob_hands(),
            "load did not decline" );
  }

  u3z(_han_bob);
  _pier_done();
}

static u3_noun
_hand_nest_cb(u3_noun arg)
{
  (void)arg;
  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  _check( han_u && (1 == han_u->use_w), "child open failed" );
  u3_blob_close(han_u);
  _check( 0 == han_u->use_w, "child close left the hand held" );
  return 0;
}

/* _test_hand_nest(): a child road that opens and closes normally leaves
**   the home hand exactly as it found it.
*/
static void
_test_hand_nest(void)
{
  _hand_unwind_setup("hand nest");

  u3_blob_hand* han_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
  _check( han_u, "home open failed" );
  c3_i fid_i = han_u->fid_i;

  u3z(u3m_soft_top(0, 1 << 12, _hand_nest_cb, 0));

  _check(  (1 == u3_blob_hands())
        && (fid_i == han_u->fid_i) && (c3y == _fd_live(fid_i)),
          "home hand disturbed" );

  u3_blob_close(han_u);
  _han_fid_i = fid_i;
  _hand_unwind_check();
}

/* _test_hand_edge(): met and comparison at the 4 KiB window boundaries.
*/
static void
_test_hand_edge(void)
{
  _pier_make("hand edge");

  //  met: the top byte sits at the very end of, or just past, a window
  //
  {
    const c3_w len_w[] = { 4095, 4096, 4097, 8192, 8193 };

    for ( c3_w i_w = 0; i_w < sizeof(len_w) / sizeof(*len_w); i_w++ ) {
      c3_y* dat_y = c3_malloc(len_w[i_w]);
      memset(dat_y, 0x11, len_w[i_w]);
      dat_y[len_w[i_w] - 1] = 0x03;

      c3_h mug_h = 0; c3_h seq_h = 0;
      _blob_save(dat_y, len_w[i_w], &mug_h, &seq_h);

      u3_atom bob = u3i_blob(mug_h, seq_h);
      u3_atom ref = u3i_bytes(len_w[i_w], dat_y);
      _check( u3r_met_d(0, bob) == (c3_d)u3r_met(0, ref),
              "met at %" PRIc3_w " bytes", len_w[i_w] );
      u3z(bob); u3z(ref);
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
    _blob_save(a_y, len_w, &am_h, &as_h);
    _blob_save(b_y, len_w, &bm_h, &bs_h);
    _blob_save(c_y, len_w, &cm_h, &cs_h);

    u3_atom boa = u3i_blob(am_h, as_h);
    u3_atom bob = u3i_blob(bm_h, bs_h);
    u3_atom boc = u3i_blob(cm_h, cs_h);
    u3_atom loa = u3i_bytes(len_w, a_y);

    _check(  (c3y == u3r_sing(boa, loa))
          && (c3n == u3r_sing(boa, bob))
          && (c3n == u3r_sing(boa, boc))
          && (-1 == u3r_comp(boa, bob))     //  b larger at byte 4095
          && (1  == u3r_comp(boa, boc))     //  c smaller at byte 4096
          && (1  == u3r_comp(bob, boc)),
            "window-boundary compare" );

    u3z(boa); u3z(bob); u3z(boc); u3z(loa);
    c3_free(a_y); c3_free(b_y); c3_free(c_y);
  }

  _pier_done();
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

  _check(  (1 == u3_blob_hands_road(u3R)) && (1 == han_u->use_w)
        && (fid_i == han_u->fid_i)
        && (met_w == vue_u.len_w) && (byt_y == vue_u.byt_y[3]),
          "second open under a view" );

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

    _check(  (han_u->map_y == vue_u.byt_y)
          && (0 == memcmp(win_y, exp_y, sizeof(win_y)))
          && (u3r_chub(0, _han_bob) == *(c3_d*)vue_u.byt_y)
          && (0 == u3r_chub(1 + (met_w >> 3), _han_bob))
          && (0 == u3r_byte(met_w + 100000, _han_bob)),
            "readers under a mapping" );
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
  _hand_unwind_setup("hand share");
  u3z(u3m_soft_top(0, 1 << 12, _hand_share_cb, 0));
  _hand_unwind_check();
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

  _check( (0 == one_u->use_w) && (c3y == _fd_live(fid_i)),
          "close released the hand" );

  for ( c3_w i_w = 0; i_w < 1000; i_w++ ) {
    u3_blob_hand* two_u = u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h);
    _check( (two_u == one_u) && (fid_i == two_u->fid_i) && (1 == two_u->use_w),
            "reopened" );
    u3_blob_close(two_u);
  }

  _check( 1 == u3_blob_hands_road(u3R), "%zu hands on the road",
          u3_blob_hands_road(u3R) );
  return 0;
}

/* _test_hand_reuse(): an inner road keeps a closed hand for its next
**   open, and the fall releases it.
*/
static void
_test_hand_reuse(void)
{
  _hand_unwind_setup("hand reuse");
  u3z(u3m_soft_top(0, 1 << 12, _hand_reuse_cb, 0));
  _hand_unwind_check();
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

    _check(  (kid_u == han_u) && (fid_i == kid_u->fid_i)
          && (0 == u3_blob_hands_road(u3R)) && (0 == kid_u->use_w),
            "child did not borrow" );
    u3_blob_close(kid_u);
  }
  u3m_fall();

  _check(  (c3y == _fd_live(fid_i)) && (1 == u3_blob_hands_road(u3R))
        && (han_u == u3_blob_open(_tmp_pier, _han_mug_h, _han_seq_h)),
          "child's fall took the parent's hand" );
  return 0;
}

/* _test_hand_deep(): a nested road borrows an inner ancestor's hand.
*/
static void
_test_hand_deep(void)
{
  _hand_unwind_setup("hand deep");
  u3z(u3m_soft_top(0, 1 << 12, _hand_deep_cb, 0));
  _hand_unwind_check();
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
    _check(  (u3r_view_blob == one_u.kin_e) && (one_u.han_u == han_u)
          && (han_u->map_d >= wid_w)
          && (0 == memcmp(one_u.byt_y + wid_w - 64, zer_y, 64)),
            "lone view did not widen" );

    //  with that view live, a pad past the mapping cannot remap under
    //  it and is filled into a loom pad instead; the first view and the
    //  mapping are untouched
    //
    c3_d old_d = han_u->map_d;
    c3_w big_w = (c3_w)old_d + 8;

    u3r_view_padd(&two_u, _han_bob, big_w);
    _check(  (u3r_view_heap == two_u.kin_e) && (big_w == two_u.len_w)
          && (han_u->map_d == old_d) && (han_u->map_y == one_u.byt_y)
          && (0 == memcmp(two_u.byt_y, one_u.byt_y, wid_w))
          && (0 == memcmp(two_u.byt_y + big_w - 8, zer_y, 8))
          && (1 == han_u->use_w),
            "held hand remapped or pad wrong" );
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
  _hand_unwind_setup("hand wide");
  u3z(u3m_soft_top(0, 1 << 12, _hand_wide_cb, 0));
  _hand_unwind_check();
}

/* _test_hand_empty(): a zero-byte blob file opens as nothing.
*/
static void
_test_hand_empty(void)
{
  _pier_make("hand empty");

  const c3_h mug_h = 0x5678;
  c3_c pax_c[8192];

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h, _tmp_pier, mug_h);
  mkdir(pax_c, 0700);
  u3_blob_path(pax_c, _tmp_pier, mug_h, 1);
  fclose(fopen(pax_c, "wb"));

  u3_atom  bob = u3i_blob(mug_h, 1);
  u3r_view vue_u;

  _check(  !u3_blob_open(_tmp_pier, mug_h, 1)
        && (u3_none == u3r_blob_load(bob))
        && (c3n == u3r_view_open(&vue_u, bob))
        && !u3_blob_hands(),
          "empty file not rejected" );

  u3z(bob);
  _pier_done();
}

/* _test_hand_access(): every fixed-width and range reader on a bob agrees
**   with the materialized atom, including reads past the end, and none of
**   them leaves a hand open.
*/
static void
_test_hand_access(void)
{
  _pier_make("hand access");

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + ((i_w * 11) % 241));
  }

  c3_h mug_h = 0; c3_h seq_h = 0;
  _check( c3y == _blob_save(dat_y, len_w, &mug_h, &seq_h),
          "save failed" );

  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_atom loa = u3i_bytes(len_w, dat_y);

  //  fixed-width readers, inside and past the end
  //
  {
    const c3_w bit_w[]  = { 0, 7, 8, 12345, 39999, 40000, 40008, 100000 };
    for ( c3_w i_w = 0; i_w < sizeof(bit_w) / sizeof(*bit_w); i_w++ ) {
      _check( u3r_bit(bit_w[i_w], bob) == u3r_bit(bit_w[i_w], loa),
              "bit at %" PRIc3_w, bit_w[i_w] );
    }
    const c3_w sho_w[]  = { 0, 1, 2499, 2500, 2600 };
    for ( c3_w i_w = 0; i_w < sizeof(sho_w) / sizeof(*sho_w); i_w++ ) {
      _check( u3r_short(sho_w[i_w], bob) == u3r_short(sho_w[i_w], loa),
              "short at %" PRIc3_w, sho_w[i_w] );
    }
    const c3_w haf_w[]  = { 0, 1, 1249, 1250, 1300 };
    for ( c3_w i_w = 0; i_w < sizeof(haf_w) / sizeof(*haf_w); i_w++ ) {
      _check( u3r_half(haf_w[i_w], bob) == u3r_half(haf_w[i_w], loa),
              "half at %" PRIc3_w, haf_w[i_w] );
      _check( u3r_word(haf_w[i_w], bob) == u3r_word(haf_w[i_w], loa),
              "word at %" PRIc3_w, haf_w[i_w] );
    }
    const c3_w chb_w[]  = { 0, 1, 624, 625, 700 };
    for ( c3_w i_w = 0; i_w < sizeof(chb_w) / sizeof(*chb_w); i_w++ ) {
      _check( u3r_chub(chb_w[i_w], bob) == u3r_chub(chb_w[i_w], loa),
              "chub at %" PRIc3_w, chb_w[i_w] );
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
      _check( 0 == memcmp(ba_y, bl_y, buf_z), "bytes at %" PRIc3_w, byt_w[i_w][0] );
    }

    const c3_w hfs_w[][2] = { {0, 1250}, {1249, 3}, {1250, 2}, {1300, 4} };
    for ( c3_w i_w = 0; i_w < sizeof(hfs_w) / sizeof(*hfs_w); i_w++ ) {
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_halfs(hfs_w[i_w][0], hfs_w[i_w][1], (c3_h*)ba_y, bob);
      u3r_halfs(hfs_w[i_w][0], hfs_w[i_w][1], (c3_h*)bl_y, loa);
      _check( 0 == memcmp(ba_y, bl_y, buf_z), "halfs at %" PRIc3_w, hfs_w[i_w][0] );
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_words(hfs_w[i_w][0], hfs_w[i_w][1], (c3_w*)ba_y, bob);
      u3r_words(hfs_w[i_w][0], hfs_w[i_w][1], (c3_w*)bl_y, loa);
      _check( 0 == memcmp(ba_y, bl_y, buf_z), "words at %" PRIc3_w, hfs_w[i_w][0] );
    }

    const c3_w chs_w[][2] = { {0, 625}, {624, 3}, {625, 2}, {700, 4} };
    for ( c3_w i_w = 0; i_w < sizeof(chs_w) / sizeof(*chs_w); i_w++ ) {
      memset(ba_y, 0xee, buf_z); memset(bl_y, 0xee, buf_z);
      u3r_chubs(chs_w[i_w][0], chs_w[i_w][1], (c3_d*)ba_y, bob);
      u3r_chubs(chs_w[i_w][0], chs_w[i_w][1], (c3_d*)bl_y, loa);
      _check( 0 == memcmp(ba_y, bl_y, buf_z), "chubs at %" PRIc3_w, chs_w[i_w][0] );
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

      _check( 0 == memcmp(sa_u.buf_y, sl_u.buf_y,
                          (size_t)sa_u.len_w * u3a_word_bytes),
              "chop case %" PRIc3_w, i_w );
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
    _check( 0 == mpz_cmp(ma_mp, ml_mp), "mp" );
    mpz_clear(ma_mp);
    mpz_clear(ml_mp);
    _check( u3r_mug(bob) == u3r_mug(loa), "mug" );
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
    _check( (pad_w >= len_w) && (0 == u3_blob_hands()), "pad %" PRIc3_w, pad_w );

    u3r_view_padd(&vue_u, bob, 100);
    _check(  (u3r_view_blob == vue_u.kin_e) && (100 == vue_u.len_w)
          && (0 == memcmp(vue_u.byt_y, dat_y, 100)) && (1 == u3_blob_hands()),
            "padd short" );
    u3r_view_done(&vue_u);

    //  a pad inside the hand's own zero tail is the mapping; on a
    //  platform whose pad is only the word tail it is a loom pad
    //
    {
      c3_o map_o = ( pad_w >= 6000 ) ? c3y : c3n;

      u3r_view_padd(&vue_u, bob, 6000);
      _check(  (vue_u.kin_e == ((c3y == map_o) ? u3r_view_blob : u3r_view_heap))
            && (6000 == vue_u.len_w)
            && (0 == memcmp(vue_u.byt_y, dat_y, len_w))
            && (0 == memcmp(vue_u.byt_y + len_w, zer_y, 1000))
            && (u3_blob_hands() == ((c3y == map_o) ? 1 : 0)),
              "padd mapped" );
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
      _check(  (u3r_view_blob == vue_u.kin_e) && (1 == u3_blob_hands())
            && (vue_u.han_u->map_d >= wid_w),
              "padd wide kind at %" PRIc3_w, wid_w );
#else
      _check( (u3r_view_heap == vue_u.kin_e) && (0 == u3_blob_hands()),
              "padd wide kind at %" PRIc3_w, wid_w );
#endif
      _check(  (wid_w == vue_u.len_w)
            && (0 == memcmp(vue_u.byt_y, dat_y, len_w))
            && (0 == memcmp(vue_u.byt_y + pad_w, zer_y, 1000))
            && (0 == memcmp(vue_u.byt_y + wid_w - 1000, zer_y, 1000)),
              "padd wide bytes at %" PRIc3_w, wid_w );
      u3r_view_done(&vue_u);
    }

    //  loom atoms: a pad wider than the atom is a loom pad; a direct
    //  atom is viewed in place and padded the same way
    //
    {
      u3_atom sma = u3i_string("small loom atom, wider than a word");
      c3_w    met_w = u3r_met(3, sma);

      u3r_view_padd(&vue_u, sma, 64);
      _check(  (u3r_view_heap == vue_u.kin_e) && (64 == vue_u.len_w)
            && (0 == memcmp(vue_u.byt_y + met_w, zer_y, 64 - met_w)),
              "padd loom" );
      {
        u3_atom cop = u3i_bytes(met_w, vue_u.byt_y);
        _check( c3y == u3r_sing(sma, cop), "padd loom bytes" );
        u3z(cop);
      }
      u3r_view_done(&vue_u);

      u3r_view_init(&vue_u, 0x44332211);
      _check(  (u3r_view_flat == vue_u.kin_e) && (4 == vue_u.len_w)
            && (vue_u.byt_y == (const c3_y*)&vue_u.raw_d)
            && (0x11 == vue_u.byt_y[0]) && (0x44 == vue_u.byt_y[3]),
              "init cat" );
      u3r_view_done(&vue_u);

      u3r_view_padd(&vue_u, 0x44332211, 16);
      _check(  (u3r_view_heap == vue_u.kin_e) && (16 == vue_u.len_w)
            && (0x11 == vue_u.byt_y[0]) && (0x44 == vue_u.byt_y[3])
            && (0 == memcmp(vue_u.byt_y + 4, zer_y, 12)),
              "padd cat" );
      u3r_view_done(&vue_u);

      u3r_view_padd(&vue_u, sma, 4);
      _check( (u3r_view_loom == vue_u.kin_e) && (4 == vue_u.len_w),
              "padd truncate" );
      u3r_view_done(&vue_u);

      //  windowed views: a bob opens without mapping and reads windows
      //  straight from the file, zero past the end; a loom atom and a
      //  direct atom read the same way from their own bytes
      //
      {
        c3_y win_y[64];
        c3_y exp_y[64];

        _check( c3y == u3r_view_open(&vue_u, bob), "open bob" );
        _check(  (u3r_view_blob == vue_u.kin_e) && (0 == vue_u.byt_y)
              && (len_w == vue_u.len_w) && (0 == vue_u.han_u->map_y)
              && (1 == u3_blob_hands()),
                "open bob shape" );

        memset(exp_y, 0, sizeof(exp_y));
        memcpy(exp_y, dat_y + len_w - 40, 40);
        _check(  (40 == u3r_view_read(&vue_u, len_w - 40, win_y, sizeof(win_y)))
              && (0 == memcmp(win_y, exp_y, sizeof(win_y)))
              && (0 == vue_u.han_u->map_y),
                "read bob tail" );
        _check(  (0 == u3r_view_read(&vue_u, (c3_d)len_w << 20, win_y, 8))
              && (0 == memcmp(win_y, exp_y + 40, 8)),
                "read bob past" );
        u3r_view_done(&vue_u);
        _check( 0 == u3_blob_hands(), "open bob done" );

        _check( c3y == u3r_view_open(&vue_u, sma), "open loom" );
        _check(  (u3r_view_loom == vue_u.kin_e)
              && (3 == u3r_view_read(&vue_u, met_w - 3, win_y, 16))
              && (0 == memcmp(win_y, vue_u.byt_y + met_w - 3, 3))
              && (0 == memcmp(win_y + 3, exp_y + 40, 13)),
                "read loom" );
        u3r_view_done(&vue_u);

        _check( c3y == u3r_view_open(&vue_u, 0x44332211), "open cat" );
        _check(  (u3r_view_flat == vue_u.kin_e)
              && (2 == u3r_view_read(&vue_u, 2, win_y, 4))
              && (0x33 == win_y[0]) && (0x44 == win_y[1])
              && (0 == win_y[2]) && (0 == win_y[3]),
                "read cat" );
        u3r_view_done(&vue_u);
      }

      u3z(sma);
    }
  }

  u3z(bob); u3z(loa);
  c3_free(dat_y);
  _pier_done();
}

#ifndef U3_OS_windows
/* _test_crit(): a signal raised inside a critical section is delivered
**   at the outermost leave, not before.
**
**   POSIX only: the Windows emulation delivers through rsignal_raise()
**   from another thread, which this single-threaded test cannot drive.
*/
static void
_test_crit(void)
{
  _nam_c = "crit";

  void (*old_f)(int) = signal(SIGINT, _crit_handler);

  _crit_hit = 0;
  u3m_crit_enter();
  raise(SIGINT);
  _check( 0 == _crit_hit, "delivered inside section" );
  u3m_crit_leave();
  _check( 1 == _crit_hit, "not delivered at leave" );

  _crit_hit = 0;
  u3m_crit_enter();
  u3m_crit_enter();
  raise(SIGINT);
  u3m_crit_leave();
  _check( 0 == _crit_hit, "delivered at inner leave" );
  u3m_crit_leave();
  _check( 1 == _crit_hit, "not delivered at outer leave" );

  signal(SIGINT, old_f);
  fprintf(stderr, "test blob %s: ok\r\n", _nam_c);
}
#endif

/* _test_lifecycle(): a noun holding bobs survives the event-log framing.
**
**   mars frames every event with u3s_ram_xeno before it reaches the log
**   or the newt pipe, and u3s_tap_xeno reads it back on replay.  a bob
**   crosses as its (mug, seq), not its bytes, and the same bob appears
**   twice so the encoder's backref path runs for bobs too.  after the
**   round trip the readers reach the files through views: u3r_bytes
**   against the saved bytes, u3r_met_d against u3r_met on the loom atom.
*/
static void
_test_lifecycle(void)
{
  _pier_make("lifecycle");

  const c3_y dat1_y[] = "lifecycle: first blob contents";
  const c3_d dat1_d   = sizeof(dat1_y) - 1;
  const c3_y dat2_y[] = "lifecycle: entirely different payload here";
  const c3_d dat2_d   = sizeof(dat2_y) - 1;

  c3_h mug1_h = 0, mug2_h = 0;
  c3_h seq1_h = 0, seq2_h = 0;

  _check( c3y == _blob_save(dat1_y, dat1_d, &mug1_h, &seq1_h),
          "save1 failed" );
  _check( c3y == _blob_save(dat2_y, dat2_d, &mug2_h, &seq2_h),
          "save2 failed" );

  //  shape: [%blob [bob1 bob2] bob1 42]
  //
  u3_noun bob1 = u3i_blob(mug1_h, seq1_h);
  u3_noun bob2 = u3i_blob(mug2_h, seq2_h);
  u3_noun ref  = u3nq(c3__blob,
                      u3nc(u3k(bob1), u3k(bob2)),
                      u3k(bob1),
                      42);
  u3z(bob1);
  u3z(bob2);

  //  encode via ram (what mars writes to the event log / newt)
  //
  c3_d  len_d = 0;
  c3_y* byt_y = 0;
  u3s_ram_xeno(ref, &len_d, &byt_y);

  //  validate header: "RAM\0" + 0x01 (the disk/newt framing)
  //
  _check(  (len_d >= 5)
        && (byt_y[0] == 'R')
        && (byt_y[1] == 'A')
        && (byt_y[2] == 'M')
        && (byt_y[3] == 0x00)
        && (byt_y[4] == 0x01),
          "ram header invalid" );

  //  decode via tap (what mars does on replay / newt receive)
  //
  u3_weak out = u3s_tap_xeno(len_d, byt_y);
  c3_free(byt_y);
  _check( u3_none != out, "tap returned u3_none" );

  //  structural equality: mug+seq preserved for bob atoms; cat/indirect
  //  atoms compared by value
  //
  _check( c3y == u3r_sing(ref, out), "decoded noun differs from ref" );

  u3_noun tag, cel, b2, rst;
  _check( c3y == u3r_cell(out, &tag, &cel), "decoded root not cell" );
  _check( tag == c3__blob, "wrong tag" );

  u3_noun bobs, bob1_d, bob2_d;
  u3x_trel(cel, &bobs, &b2, &rst);
  u3x_cell(bobs, &bob1_d, &bob2_d);

  //  every occurrence is a bob naming its (mug, seq); whether tap
  //  reuses one loom slot for the backref is an implementation detail
  //
  _check(  (c3y == u3a_is_bob(bob1_d))
        && (c3y == u3a_is_bob(bob2_d))
        && (c3y == u3a_is_bob(b2)),
          "decoded non-bob where bob expected" );
  _check( (u3a_bob_mug(bob1_d) == mug1_h) && (u3a_bob_seq(bob1_d) == seq1_h),
          "bob1 mug/seq mismatch" );
  _check( (u3a_bob_mug(b2) == mug1_h) && (u3a_bob_seq(b2) == seq1_h),
          "backref bob1 mug/seq mismatch" );
  _check( (u3a_bob_mug(bob2_d) == mug2_h) && (u3a_bob_seq(bob2_d) == seq2_h),
          "bob2 mug/seq mismatch" );

  //  the bytes come back from the files
  //
  {
    c3_y* buf_y = c3_malloc(dat2_d);
    u3r_bytes(0, (c3_w)dat1_d, buf_y, bob1_d);
    _check( 0 == memcmp(buf_y, dat1_y, dat1_d), "bob1 bytes mismatch" );
    u3r_bytes(0, (c3_w)dat2_d, buf_y, bob2_d);
    _check( 0 == memcmp(buf_y, dat2_y, dat2_d), "bob2 bytes mismatch" );
    c3_free(buf_y);
  }

  //  and the bit length from the file agrees with the loom atom's
  //
  {
    u3_atom lom = u3i_bytes((c3_w)dat1_d, dat1_y);
    _check( u3r_met_d(0, bob1_d) == (c3_d)u3r_met(0, lom),
            "met_d=%" PRIc3_d " vs loom met=%" PRIc3_w,
            u3r_met_d(0, bob1_d), u3r_met(0, lom) );
    u3z(lom);
  }

  u3z(ref);
  u3z(out);

  //  tear down and confirm blob files are deleted cleanly
  //
  u3_blob_wipe(_tmp_pier, mug1_h, seq1_h);
  u3_blob_wipe(_tmp_pier, mug2_h, seq2_h);
  _check(  (c3n == u3_blob_live(_tmp_pier, mug1_h, seq1_h))
        && (c3n == u3_blob_live(_tmp_pier, mug2_h, seq2_h)),
          "blobs still present after delete" );

  _pier_done();
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

/* _lease_open(): open an LMDB env in the pier's log directory.
*/
static MDB_env*
_lease_open(void)
{
  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);

  MDB_env* env_u = u3_lmdb_init(log_c, 1ULL << 30);
  _check( env_u, "lmdb init failed" );
  return env_u;
}

/* _lease_env(): a fresh pier with an LMDB env for a lease test.
*/
static MDB_env*
_lease_env(const c3_c* nam_c)
{
  _pier_make(nam_c);

  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);
  _check( 0 == mkdir(log_c, 0700), "mkdir %s: %s", log_c, strerror(errno) );

  return _lease_open();
}

/* _lease_reopen(): close and reopen the env — stands in for a crash +
**   restart, exercising on-disk durability.
*/
static MDB_env*
_lease_reopen(MDB_env* env_u)
{
  u3_lmdb_exit(env_u);
  return _lease_open();
}

/* _test_lease(): the durable LEASES table: several leases per bid
**   (MDB_DUPSORT), exact-row deletes that leave the siblings untouched,
**   idempotent delete, and a walk over a table not yet created.
*/
static void
_test_lease(void)
{
  MDB_env* env_u = _lease_env("lease");

  //  walking a not-yet-created table yields nothing
  //
  _check( 0 == _lease_count(env_u), "empty table not empty" );

  c3_d bid1 = ((c3_d)0xdeadbeef << 32) | 1;
  c3_d bid2 = ((c3_d)0xdeadbeef << 32) | 2;

  //  five leases on one blob (duplicate key) + one on another
  //
  for ( c3_d i_d = 0; i_d < 5; i_d++ ) {
    _check( c3y == u3_lmdb_save_lease(env_u, bid1, 100 + i_d, 1 + i_d),
            "save %" PRIc3_d " failed", i_d );
  }
  _check( c3y == u3_lmdb_save_lease(env_u, bid2, 300, 3), "save failed" );

  _check( 6 == _lease_count(env_u), "expected 6 rows, got %zu", _lease_num );
  for ( c3_d i_d = 0; i_d < 5; i_d++ ) {
    _check( c3y == _lease_has(bid1, 100 + i_d, 1 + i_d),
            "row %" PRIc3_d " missing after save", i_d );
  }
  _check( c3y == _lease_has(bid2, 300, 3), "second blob's row missing" );

  //  delete the first and last duplicates; the middle three and the
  //  other blob's row survive
  //
  _check(  (c3y == u3_lmdb_delete_lease(env_u, bid1, 100, 1))
        && (c3y == u3_lmdb_delete_lease(env_u, bid1, 104, 5)),
          "delete failed" );
  _check( 4 == _lease_count(env_u), "expected 4 rows after delete, got %zu",
          _lease_num );
  _check(  (c3n == _lease_has(bid1, 100, 1))
        && (c3y == _lease_has(bid1, 101, 2))
        && (c3y == _lease_has(bid1, 102, 3))
        && (c3y == _lease_has(bid1, 103, 4))
        && (c3n == _lease_has(bid1, 104, 5))
        && (c3y == _lease_has(bid2, 300, 3)),
          "wrong rows survived" );

  //  deleting a now-absent row is idempotent success
  //
  _check( c3y == u3_lmdb_delete_lease(env_u, bid1, 100, 1),
          "idempotent delete reported failure" );
  _check( 4 == _lease_count(env_u), "idempotent delete changed table" );

  //  drain
  //
  u3_lmdb_delete_lease(env_u, bid1, 101, 2);
  u3_lmdb_delete_lease(env_u, bid1, 102, 3);
  u3_lmdb_delete_lease(env_u, bid1, 103, 4);
  u3_lmdb_delete_lease(env_u, bid2, 300, 3);
  _check( 0 == _lease_count(env_u), "table not drained" );

  u3_lmdb_exit(env_u);
  _pier_done();
}

/* _test_lease_persist(): leases (and their deletions) survive a close +
**   reopen, and coexist with the BLOBS table in the same env.
*/
static void
_test_lease_persist(void)
{
  MDB_env* env_u = _lease_env("lease persist");

  c3_d bid_a = ((c3_d)0x0a11ce << 32) | 7;
  c3_d bid_b = ((c3_d)0x000b0b << 32) | 9;

  //  two leases on bid_a (duplicate key) + one on bid_b
  //
  _check(  (c3y == u3_lmdb_save_lease(env_u, bid_a, 1000, 10))
        && (c3y == u3_lmdb_save_lease(env_u, bid_a, 2000, 11))
        && (c3y == u3_lmdb_save_lease(env_u, bid_b, 3000, 12)),
          "save failed" );

  //  an unrelated BLOBS row, to prove the tables are independent and
  //  maxdbs accommodates both
  //
  {
    c3_d ids_d[2] = { bid_a, bid_b };
    _check( c3y == u3_lmdb_save_blobs(env_u, 42, ids_d, 2), "blobs save failed" );
  }

  //  "crash" and restart
  //
  env_u = _lease_reopen(env_u);

  //  every lease survived, values intact
  //
  _check( 3 == _lease_count(env_u), "expected 3 rows after reopen, got %zu",
          _lease_num );
  _check(  (c3y == _lease_has(bid_a, 1000, 10))
        && (c3y == _lease_has(bid_a, 2000, 11))
        && (c3y == _lease_has(bid_b, 3000, 12)),
          "a lease did not survive reopen" );

  //  the BLOBS row survived independently
  //
  {
    c3_d* out_d = 0;
    c3_z  out_z = 0;
    _check(  (c3y == u3_lmdb_read_blobs(env_u, 42, &out_d, &out_z))
          && (2 == out_z)
          && (bid_a == out_d[0])
          && (bid_b == out_d[1]),
            "blobs row corrupt after reopen" );
    c3_free(out_d);
  }

  //  a deletion is durable too
  //
  _check( c3y == u3_lmdb_delete_lease(env_u, bid_a, 1000, 10), "delete failed" );
  env_u = _lease_reopen(env_u);

  _check( 2 == _lease_count(env_u), "deletion did not persist (%zu rows)",
          _lease_num );
  _check( c3n == _lease_has(bid_a, 1000, 10), "deleted row reappeared" );

  u3_lmdb_exit(env_u);
  _pier_done();
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
  _pier_make("canon");

  //  same atom, three spellings.  the payload is over 8 bytes so that the
  //  atom it denotes is indirect in both bitnesses: u3r_sing's bob branch
  //  is only reached for indirect atoms, and a real blob (> U3_BLOB_THRESH)
  //  is always indirect.
  //
  const c3_y bar_y[] = "canonical bytes";
  const c3_y pad_y[] = "canonical bytes\0";
  const c3_y pud_y[] = "canonical bytes\0\0\0\0\0\0\0\0";

  c3_h bar_mug_h = 0, bar_seq_h = 0;
  c3_h pad_mug_h = 0, pad_seq_h = 0;
  c3_h pud_mug_h = 0, pud_seq_h = 0;

  _check(  (c3y == _blob_save(bar_y, sizeof(bar_y),
                                &bar_mug_h, &bar_seq_h))
        && (c3y == _blob_save(pad_y, sizeof(pad_y),
                                &pad_mug_h, &pad_seq_h))
        && (c3y == _blob_save(pud_y, sizeof(pud_y),
                                &pud_mug_h, &pud_seq_h)),
          "save failed" );

  _check(  (bar_mug_h == pad_mug_h) && (bar_seq_h == pad_seq_h)
        && (bar_mug_h == pud_mug_h) && (bar_seq_h == pud_seq_h),
          "trailing zeros made distinct blobs (%" PRIc3_h "/%" PRIc3_h
          ", %" PRIc3_h "/%" PRIc3_h ", %" PRIc3_h "/%" PRIc3_h ")",
          bar_mug_h, bar_seq_h, pad_mug_h, pad_seq_h, pud_mug_h, pud_seq_h );

  //  what landed on disk is the atom's bytes, not the caller's buffer
  //  (every fixture above carries at least the string literal's own NUL)
  //
  const c3_d sig_d = strlen((const c3_c*)bar_y);
  {
    c3_c fil_c[8192];
    struct stat st_u;
    u3_blob_path(fil_c, _tmp_pier, bar_mug_h, bar_seq_h);
    _check( (0 == stat(fil_c, &st_u)) && (sig_d == (c3_d)st_u.st_size),
            "stored %lld bytes, wanted %" PRIc3_d,
            (long long)st_u.st_size, sig_d );
  }

  //  the core invariant: a bob atom mugs like the loom atom it denotes
  //
  {
    u3_atom bob = u3i_blob(bar_mug_h, bar_seq_h);
    u3_atom lom = u3i_bytes(sizeof(pud_y), pud_y);

    _check( u3r_mug(bob) == u3r_mug(lom),
            "bob mug %" PRIc3_h " != loom mug %" PRIc3_h,
            u3r_mug(bob), u3r_mug(lom) );
    _check( c3y == u3r_sing(bob, lom), "bob != loom atom" );

    //  and two bobs for one atom are the same bob, so bob-vs-bob sing
    //  (which compares mug and blob identity only) agrees
    //
    u3_atom tob = u3i_blob(pud_mug_h, pud_seq_h);
    _check( c3y == u3r_sing(bob, tob), "bob != bob for one atom" );

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

    _check( c3n == _blob_save(nil_y, sizeof(nil_y),
                                &nil_mug_h, &nil_seq_h),
            "saved an all-zero blob" );
    _check( c3n == _blob_save(sml_y, sizeof(sml_y),
                                &sml_mug_h, &sml_seq_h),
            "saved a direct-atom blob (%zu bytes)", sizeof(sml_y) );

    //  one byte more is always indirect, and is accepted
    //
    const c3_y big_y[U3_BLOB_MIN] = {[U3_BLOB_MIN - 1] = 0xff};
    c3_h big_mug_h = 0, big_seq_h = 0;

    _check( c3y == _blob_save(big_y, sizeof(big_y),
                                &big_mug_h, &big_seq_h),
            "refused a %zu-byte blob", sizeof(big_y) );
    {
      u3_atom bob = u3i_blob(big_mug_h, big_seq_h);
      u3_atom lom = u3i_bytes(sizeof(big_y), big_y);

      _check( c3y == u3a_is_pug(lom), "U3_BLOB_MIN bytes still direct in the loom" );
      _check( c3y == u3r_sing(bob, lom), "floor-sized bob != loom" );
      u3z(bob); u3z(lom);
    }
  }

  _pier_done();
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
  _test_delete_empty_bucket();
  _test_walk();
  _test_install_stg();
  _test_install_stg_trim();
  _test_stage();
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
#ifndef U3_OS_windows
  _test_crit();
#endif
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

  fprintf(stderr, "test blob: ok\r\n");
  return 0;
}
