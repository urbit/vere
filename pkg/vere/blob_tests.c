/// @file

#include "blob.h"
#include "events.h"
#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"
#include "vere.h"
#include "mars.h"
#include "ur/ur.h"
#include "urcrypt.h"

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
#ifndef U3_OS_windows
#include <sys/wait.h>
#endif

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
static c3_c _tmp_bob[8192];   //  epoch 0's store root
static const c3_c* _nam_c;   //  the running test, for messages
static c3_w        _del_w;   //  deletions requested since _pier_make
static c3_w        _hok_w;   //  hook calls
static c3_h        _stg_mug; //  the mug staging reported

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

/* _blob_del_cb(): the mars end of a bob's death, by mars's rule: a
**   record the king or the epoch still holds stays; otherwise count
**   the request and drop the record.  the file is left to the pier's
**   removal.
*/
static void
_blob_del_cb(c3_h mug_h, c3_h seq_h)
{
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);

  _hok_w += 1;

  if ( blb_u && (blb_u->hol_h || (c3y == blb_u->own_o)) ) {
    return;
  }
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
  u3C.bob_c      = _tmp_bob;
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
  _hok_w = 0;

  if ( !c3_tmp_make(_tmp_pier, sizeof(_tmp_pier), "vere-blob-test") ) {
    _fail("c3_tmp_make: %s", strerror(errno));
  }

  //  the disk code creates .urb, the log, and epoch 0 before the store
  //  in production; the store root is epoch 0's
  //
  {
    const c3_c* dir_c[] = { ".urb", ".urb/log", ".urb/log/0i0" };

    for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
      snprintf(urb_c, sizeof(urb_c), "%s/%s", _tmp_pier, dir_c[i_w]);
      if ( 0 != mkdir(urb_c, 0700) ) {
        _fail("mkdir %s: %s", urb_c, strerror(errno));
      }
    }
  }

  u3b_root(_tmp_bob, _tmp_pier, 0);
  if ( 0 != mkdir(_tmp_bob, 0700) ) {
    _fail("mkdir %s: %s", _tmp_bob, strerror(errno));
  }

  u3_disk_blob_init(_tmp_pier);
}

/* _pier_done(): no mapping survives a test; remove the pier and report.
*/
static void
_pier_done(void)
{
  _check( 0 == u3b_maps(), "%zu mapping(s) left", u3b_maps() );
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

/* _file_put(): write [len_z] bytes of [dat_c] to a new file [pax_c].
*/
static c3_o
_file_put(const c3_c* pax_c, const c3_c* dat_c, c3_z len_z)
{
  c3_i fid_i = c3_open(pax_c, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  if ( 0 > fid_i ) {
    return c3n;
  }

  c3_o ret_o = __( (ssize_t)len_z == write(fid_i, dat_c, len_z) );
  close(fid_i);
  return ret_o;
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

  if ( c3n == u3b_stage(_tmp_pier, dat_y, len_d, stg_c, &_stg_mug) ) {
    return c3n;
  }
  if ( c3n == u3b_move_stg(_tmp_bob, stg_c, mug_h, seq_h) ) {
    c3_unlink(stg_c);
    return c3n;
  }
  //  staging reports the mug the install will give the blob: that is
  //  what a stub carries
  //
  _check( _stg_mug == *mug_h, "staging mug %08x, installed %08x",
          _stg_mug, *mug_h );
  return c3y;
}

/* _hand_write_raw(): write a blob file directly into one bucket, no
**   fsync.  [num_w] distinguishes the content.
*/
static void
_hand_write_raw(c3_h mug_h, c3_h seq_h, c3_w num_w)
{
  c3_c  pax_c[8192];
  FILE* fil_f;

  snprintf(pax_c, sizeof(pax_c), "%s/" U3B_MUG_FMT, _tmp_bob, mug_h);
  if ( (0 != mkdir(pax_c, 0700)) && (EEXIST != errno) ) {
    _fail("raw mkdir: %s", strerror(errno));
  }

  u3b_path(pax_c, _tmp_bob, mug_h, seq_h);
  if ( !(fil_f = fopen(pax_c, "wb")) ) {
    _fail("raw fopen: %s", strerror(errno));
  }
  fprintf(fil_f, "raw blob %08" PRIc3_w " padded past the minimum length", num_w);
  fclose(fil_f);
}

#ifndef U3_OS_windows
/* _fd_open(): true if [fid_i] is an open descriptor.
*/
static c3_t
_fd_open(c3_i fid_i)
{
  return -1 != fcntl(fid_i, F_GETFD);
}
#endif

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

/* _grab(): mark the home road and sweep it, as |mass does.
*/
static void
_grab(void)
{
  u3a_mark_init();
  u3m_quac** qua_u = u3m_mark();
  u3a_sweep();

  for ( c3_w i_w = 0; qua_u[i_w]; i_w++ ) {
    u3a_quac_free(qua_u[i_w]);
  }
  c3_free(qua_u);
}

/* _test_sweep_settle(): a sweep that frees a leaked bob takes it off
**   its record, and calls the hook when the last one goes, so a leak
**   cannot pin a file.
*/
static void
_test_sweep_settle(void)
{
  _pier_make("sweep settle");

  const c3_y dat_y[] = "a leaked bob must not pin its file forever";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  _check( c3y == _blob_save(dat_y, dat_d, &mug_h, &seq_h),
          "save failed" );

  //  one bob in arvo state, one leaked: no root reaches it
  //
  u3_noun roc = u3A->roc;
  u3A->roc    = u3i_blob(mug_h, seq_h);
  (void)u3i_blob(mug_h, seq_h);

  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  _check( blb_u && (2 == blb_u->atm_h), "atm=%u, want 2",
          blb_u ? blb_u->atm_h : 0 );

  _grab();
  _check( (blb_u == u3a_blob_get(mug_h, seq_h)) && (1 == blb_u->atm_h)
          && (0 == _hok_w),
          "one leak: atm=%u hook=%u", blb_u->atm_h, _hok_w );
  _check( c3y == u3a_blob_sane(), "bank not sane after the sweep" );

  //  leak the last one: the sweep fires the hook, which drops the
  //  unowned record
  //
  u3A->roc = roc;
  _grab();
  _check( (1 == _hok_w) && (1 == _del_w) && !u3a_blob_get(mug_h, seq_h),
          "last leak: hook=%u del=%u", _hok_w, _del_w );

  _pier_done();
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

/* _test_stg_clean(): u3_disk_blob_init clears leftover staging files.
*/
static void
_test_stg_clean(void)
{
  _pier_make("stg_clean");

  c3_c stg_c[2048];
  snprintf(stg_c, sizeof(stg_c), "%s/.urb/stg/leftover", _tmp_pier);
  FILE* f = fopen(stg_c, "wb");
  _check( f, "setup: %s", strerror(errno) );
  fputs("junk", f);
  fclose(f);
  _check( c3y == _path_exists(stg_c), "file not created" );

  u3_disk_blob_init(_tmp_pier);
  _check( c3n == _path_exists(stg_c), "%s still exists", stg_c );

  _pier_done();
}

/* _test_path(): u3b_path produces the expected string.
*/
static void
_test_path(void)
{
  _nam_c = "path";

  c3_c pax_c[8192];
  c3_c bob_c[8192];

  u3b_root(bob_c, "/pier", 7);
  _check( 0 == strcmp(bob_c, "/pier/.urb/log/0i7/bob"), "root %s", bob_c );

  u3b_path(pax_c, bob_c, 0x12345678, 42);

  const c3_c* exp_c = "/pier/.urb/log/0i7/bob/12345678/42";
  _check( 0 == strcmp(pax_c, exp_c), "got %s, expected %s", pax_c, exp_c );

  fprintf(stderr, "test blob %s: ok\r\n", _nam_c);
}

/* _test_walk(): u3b_walk reports every installed blob exactly once,
**   skipping the staging dir and any name u3b_path would not write.
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
    snprintf(pax_c, sizeof(pax_c), "%s/.urb/stg/leftover", _tmp_pier);
    FILE* fil_u = fopen(pax_c, "w");
    if ( fil_u ) { fputs("junk", fil_u); fclose(fil_u); }

    snprintf(pax_c, sizeof(pax_c), "%s/notanumber", _tmp_bob);
    fil_u = fopen(pax_c, "w");
    if ( fil_u ) { fputs("junk", fil_u); fclose(fil_u); }

    //  and names that parse as numbers but are not how a seq is
    //  spelled: a leading zero and a sign
    //
    const c3_c* odd_c[] = { "07", "+7" };
    for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
      snprintf(pax_c, sizeof(pax_c), "%s/" U3B_MUG_FMT "/%s",
               _tmp_bob, mug_h[0], odd_c[i_w]);
      fil_u = fopen(pax_c, "w");
      if ( fil_u ) { fputs("junk", fil_u); fclose(fil_u); }
    }

    //  and buckets holding a well-named seq whose own names are not
    //  how a mug is spelled: uppercase, seven and nine digits, a prefix
    //
    const c3_c* bad_c[] = { "ABCDEF01", "abcdef0", "0abcdef01", "0xabcdef" };
    for ( c3_w i_w = 0; i_w < 4; i_w++ ) {
      snprintf(pax_c, sizeof(pax_c), "%s/%s", _tmp_bob, bad_c[i_w]);
      _check( 0 == mkdir(pax_c, 0700), "mkdir %s: %s", pax_c, strerror(errno) );
      snprintf(pax_c, sizeof(pax_c), "%s/%s/1", _tmp_bob, bad_c[i_w]);
      fil_u = fopen(pax_c, "w");
      if ( fil_u ) { fputs("junk", fil_u); fclose(fil_u); }
    }
  }

  _walk_acc acc_u = {0};
  u3b_walk(_tmp_bob, &acc_u, _test_walk_cb);
  _check( 3 == acc_u.len_z, "expected 3 files, got %zu", acc_u.len_z );

  for ( c3_z i_z = 0; i_z < 3; i_z++ ) {
    _check( c3y == _walk_acc_has(&acc_u, mug_h[i_z], seq_h[i_z]),
            "missing " U3A_BID_FMT, mug_h[i_z], seq_h[i_z] );
  }

  //  after wiping one blob, the walk reports exactly the other two
  //
  u3b_wipe(_tmp_bob, mug_h[0], seq_h[0]);

  memset(&acc_u, 0, sizeof(acc_u));
  u3b_walk(_tmp_bob, &acc_u, _test_walk_cb);
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
  u3b_path(fil_c, _tmp_bob, mug_h, seq_h);
  snprintf(dir_c, sizeof(dir_c), "%s/" U3B_MUG_FMT, _tmp_bob, mug_h);
  _check( c3y == _path_exists(dir_c), "setup: bucket missing" );

  u3b_wipe(_tmp_bob, mug_h, seq_h);

  _check( c3n == _path_exists(fil_c), "file %s still exists", fil_c );
  _check( c3n == u3b_live(_tmp_bob, mug_h, seq_h), "live still true" );
  _check( c3n == _path_exists(dir_c), "bucket %s not cleaned", dir_c );

  //  deleting a nonexistent blob is a no-op (no error)
  //
  u3b_wipe(_tmp_bob, 0xdeadbeef, 999);

  //  wiping one blob of two leaves the bucket; wiping the last takes
  //  the bucket with it
  //
  {
    _hand_write_raw(0x7777, 1, 1);
    _hand_write_raw(0x7777, 2, 2);
    snprintf(dir_c, sizeof(dir_c), "%s/" U3B_MUG_FMT, _tmp_bob, (c3_h)0x7777);

    u3b_wipe(_tmp_bob, 0x7777, 1);
    _check( c3y == _path_exists(dir_c), "bucket removed with a blob remaining" );
    u3b_wipe(_tmp_bob, 0x7777, 2);
    _check( c3n == _path_exists(dir_c), "bucket not removed with its last blob" );
  }

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

  _check( c3y == u3b_stage(_tmp_pier, dat_y, dat_d, stg_c, &_stg_mug), "stage failed" );
  _check( c3y == _path_exists(stg_c), "staging file missing" );

  _check( c3y == u3b_move_stg(_tmp_bob, stg_c, &mug_h, &seq_h),
          "install failed" );
  _check( 1 == seq_h, "expected seq=1, got %" PRIc3_h, seq_h );
  _check( c3n == _path_exists(stg_c), "staging file not consumed" );
  _check( c3y == u3b_live(_tmp_bob, mug_h, seq_h),
          "blob not present after install" );
  _check( c3y == _bob_is(mug_h, seq_h, dat_y, dat_d), "byte mismatch" );

#ifndef U3_OS_windows
  //  the installed blob is read-only: nothing opens it for writing again
  //
  {
    c3_c fil_c[8192];
    struct stat st_u;
    u3b_path(fil_c, _tmp_bob, mug_h, seq_h);
    _check(  (0 == stat(fil_c, &st_u)) && (0400 == (st_u.st_mode & 0777)),
            "installed mode %o, expected 0400", (unsigned)(st_u.st_mode & 0777) );
  }
#endif

#ifndef U3_OS_windows
  //  a bucket that cannot be read refuses the install instead of
  //  minting a second seq for the same bytes (root reads anything)
  //
  if ( 0 != geteuid() ) {
    c3_c fil_c[8192];
    c3_h mag_h = 0, saq_h = 0;
    u3b_path(fil_c, _tmp_bob, mug_h, seq_h);

    _check( c3y == u3b_stage(_tmp_pier, dat_y, dat_d, stg_c, &_stg_mug), "restage failed" );
    _check( 0 == chmod(fil_c, 0), "chmod failed" );
    _check( c3n == u3b_move_stg(_tmp_bob, stg_c, &mag_h, &saq_h),
            "installed past an unreadable sibling" );
    _check( c3y == _path_exists(stg_c), "refusal consumed the staging file" );
    _check( 0 == chmod(fil_c, 0400), "chmod back failed" );

    //  readable again, the same staging file dedups onto seq 1
    //
    _check(  (c3y == u3b_move_stg(_tmp_bob, stg_c, &mag_h, &saq_h))
          && (mug_h == mag_h) && (1 == saq_h),
            "dedup after refusal got seq %" PRIc3_h, saq_h );
  }
#endif

  //  the bucket says which seqs are taken: with seq 1 holding other
  //  bytes, a reinstall lands on seq 2 and seq 1 is untouched
  //
  {
    c3_c fil_c[8192];
    c3_h mag_h = 0, saq_h = 0;
    struct stat st_u;

    u3b_wipe(_tmp_bob, mug_h, seq_h);
    _hand_write_raw(mug_h, 1, 0xabc);

    _check( c3y == u3b_stage(_tmp_pier, dat_y, dat_d, stg_c, &_stg_mug), "restage failed" );
    _check(  (c3y == u3b_move_stg(_tmp_bob, stg_c, &mag_h, &saq_h))
          && (mug_h == mag_h) && (2 == saq_h),
            "reinstall beside a planted seq 1 got seq %" PRIc3_h, saq_h );
    u3b_path(fil_c, _tmp_bob, mug_h, 1);
    _check( (0 == stat(fil_c, &st_u)) && (dat_d != (c3_d)st_u.st_size),
            "planted seq 1 was replaced" );
    _check( c3y == _bob_is(mug_h, 2, dat_y, dat_d), "seq 2 byte mismatch" );
    u3b_wipe(_tmp_bob, mug_h, 1);
    u3b_wipe(_tmp_bob, mug_h, 2);
  }

  //  a missing staging file and an empty one are refused; staging
  //  refuses the empty one itself, so the install's check needs a file
  //  made by hand
  //
  _check( c3n == u3b_move_stg(_tmp_bob, "/no/such/path", &mug_h, &seq_h),
          "installed a missing file" );

  _check( c3n == u3b_stage(_tmp_pier, dat_y, 0, stg_c, &_stg_mug),
          "staged an empty buffer" );
  u3b_stg_dir(stg_c, _tmp_pier);
  strcat(stg_c, "/empty");
  fclose(fopen(stg_c, "wb"));
  _check( c3n == u3b_move_stg(_tmp_bob, stg_c, &mug_h, &seq_h),
          "installed an empty file" );
  c3_unlink(stg_c);

  _pier_done();
}

/* _test_install_stg_trim(): a staging file carrying trailing zero bytes is
**   trimmed to the atom's significant length before it is installed.
**
**   This is the only test that reaches u3b_move_stg's ftruncate: every
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
  _check( c3y == u3b_stage(_tmp_pier, dat_y, sizeof(dat_y), stg_c, &_stg_mug),
          "stage failed" );
  _check( c3y == u3b_move_stg(_tmp_bob, stg_c, &mug_h, &seq_h),
          "install failed" );

  //  the installed file is the trimmed length, not the staged one
  //
  c3_c fil_c[8192];
  u3b_path(fil_c, _tmp_bob, mug_h, seq_h);

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
**   u3b_move_stg, and the two land on one blob: same mug, same seq
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

    u3b_stg_dir(dir_c, _tmp_pier);

    _check(  (c3y == u3b_stage(_tmp_pier, dat_y, dat_d, stg_c, &_stg_mug))
          && (0 == strncmp(stg_c, dir_c, strlen(dir_c)))
          && (c3y == _path_exists(stg_c)),
            "buffer stage failed" );
    _check(  (c3y == u3b_move_stg(_tmp_bob, stg_c, &mug_h, &seq_h))
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

    _check(  (c3y == u3b_stage_fd(_tmp_pier, fid_i, dat_d, stg_c, &_stg_mug))
          && (c3y == u3b_move_stg(_tmp_bob, stg_c, &mug2_h, &seq2_h))
          && (mug2_h == mug_h) && (seq2_h == seq_h)
          && (c3n == _path_exists(stg_c)),
            "install of a staged descriptor differs from the buffer's" );
    close(fid_i);

    //  a short source fails and leaves nothing behind
    //
    fid_i = c3_open(src_c, O_RDONLY, 0);
    _check(  (c3n == u3b_stage_fd(_tmp_pier, fid_i, dat_d + 1, stg_c, &_stg_mug))
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

  //  one live atom in the kernel root, owned by the epoch
  //
  u3_noun bob = u3i_blob(mug_h, seq_h);
  u3_noun old = u3A->roc;
  u3A->roc    = u3nc(bob, u3_nul);

  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  blb_u->own_o = c3y;

  _check( c3y == u3a_blob_sane(), "balanced bank reported corrupt" );

  //  holds are not atoms: the check ignores them
  //
  blb_u->hol_h += 2;
  _check( c3y == u3a_blob_sane(), "holds read as atoms" );
  blb_u->hol_h -= 2;

  //  an atom count with no atom behind it
  //
  blb_u->atm_h += 1;
  _check( c3n == u3a_blob_sane(), "cardinality mismatch not caught" );
  blb_u->atm_h -= 1;

  //  drop the atom: the epoch keeps the record as a ghost, and no
  //  deletion is asked
  //
  u3z(u3A->roc);
  u3A->roc = old;
  _check( 0 == _del_w, "blob deleted while owned" );

  blb_u = u3a_blob_get(mug_h, seq_h);
  _check( blb_u && (0 == blb_u->atm_h), "ghost not kept" );
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
  //  owned by the epoch: atm = 2
  //
  u3_noun bo1 = u3i_blob(mug_h, seq_h);
  u3_noun bo2 = u3i_blob(mug_h, seq_h);
  u3_noun old = u3A->roc;
  u3A->roc    = u3nc(bo1, bo2);

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    blb_u->own_o = c3y;
    _check( 2 == blb_u->atm_h, "setup atm_h %" PRIc3_h " != 2", blb_u->atm_h );
  }

  (void)u3_meld_all(0, 0);

  //  the duplicates must be unified (one atom box), the freed copy's
  //  cardinality decremented, the bank entry and log ref intact, and
  //  the surviving atom's body still naming the blob post-pack
  //
  {
    u3a_cell* cel_u = u3a_to_ptr(u3A->roc);
    _check( cel_u->hed == cel_u->tel, "duplicate bobs not unified" );
  }

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    _check( blb_u, "bank entry lost" );
    _check( (1 == blb_u->atm_h) && (c3y == blb_u->own_o),
            "counts atm=%" PRIc3_h " own=%u want atm=1 own=1",
            blb_u->atm_h, blb_u->own_o );
    _check(  (mug_h == u3a_bob_mug(u3h(u3A->roc)))
          && (seq_h == u3a_bob_seq(u3h(u3A->roc))),
            "bob blob pointer stale" );
  }

  _check( 0 == _del_w, "blob deleted with refs held" );

  //  drop the last atom: cardinality reaches 0, but the epoch keeps
  //  the file (no deletion request)
  //
  u3z(u3A->roc);
  u3A->roc = old;

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
    _check( blb_u && (0 == blb_u->atm_h) && (0 == _del_w),
            "post-drop del=%" PRIc3_w, _del_w );
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
    u3b_bsink bsk_u;
    u3b_bsink_init(&bsk_u, _tmp_pier, _tmp_bob);

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
    _check( blb_u && (1 == blb_u->atm_h), "bad bank entry" );

    //  re-cue: the store must dedup to the same (mug, seq)
    //
    {
      u3b_bsink bsk_u;
      u3b_bsink_init(&bsk_u, _tmp_pier, _tmp_bob);

      u3_cue_xeno* sil_u = u3s_cue_xeno_init();
      u3s_cue_xeno_blob(sil_u, 1024, &bsk_u.snk_u);
      u3_noun two = u3s_cue_xeno_with(sil_u, jam_d, jam_y);
      u3s_cue_xeno_done(sil_u);

      u3_noun bob = u3h(two);
      _check( (mug_h == u3a_bob_mug(bob)) && (seq_h == u3a_bob_seq(bob)),
              "re-cue did not dedup" );
      _check( 2 == blb_u->atm_h, "atm_h %" PRIc3_h " after re-cue, want 2",
              blb_u->atm_h );
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

/* _test_hand(): the store's mappings on the home road: u3b_map measures
**   a blob and maps it zero to a page past its end or to a width; each
**   mapping is its caller's own until unmap; a padded copy reads its
**   bytes, then zeros.
*/
static void
_test_hand(void)
{
  _pier_make("hand");

  const c3_y dat_y[] = "handle bytes should round-trip exactly";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h mug_h = 0; c3_h seq_h = 0;
  _blob_save(dat_y, dat_d, &mug_h, &seq_h);

  c3_d bid_d = u3a_bid(mug_h, seq_h);
  c3_d pag_d = (c3_d)sysconf(_SC_PAGESIZE);

  //  bit-length: top byte is the last one, u3r_met(0) semantics; the
  //  home road lists nothing
  //
  {
    c3_y     top_y = dat_y[dat_d - 1];
    c3_d     exp_d = (dat_d - 1) * 8
                   + (c3_d)(8 - (__builtin_clz((unsigned int)top_y) - 24));
    c3_d     met_d = 0;
    c3_m     mot_m = 0;
    u3b_flat fla_u;

    _check(  (c3y == u3b_map(&fla_u, _tmp_bob, bid_d, 0, &met_d, &mot_m))
          && (met_d == exp_d) && (1 == u3b_maps())
          && !u3H->rod_u.fla_p,
            "met %" PRIc3_d " != %" PRIc3_d, met_d, exp_d );

    //  the bytes, then zeros to a page past them
    //
    _check( 0 == memcmp(fla_u.map_y, dat_y, dat_d), "bytes mismatch" );
    for ( c3_d i_d = dat_d; i_d < dat_d + pag_d; i_d++ ) {
      _check( !fla_u.map_y[i_d], "tail byte %" PRIc3_d " not zero", i_d );
    }

    u3b_unmap(&fla_u);
    _check( !u3b_maps() && !fla_u.map_y, "unmap left residue" );
  }

  //  two maps of one blob are two mappings
  //
  {
    u3b_flat one_u, two_u;
    c3_d     bit_d;
    c3_m     mot_m;

    _check(  (c3y == u3b_map(&one_u, _tmp_bob, bid_d, 0, &bit_d, &mot_m))
          && (c3y == u3b_map(&two_u, _tmp_bob, bid_d, 0, &bit_d, &mot_m))
          && (one_u.map_y != two_u.map_y) && (2 == u3b_maps()),
            "two maps shared a mapping" );

    u3b_unmap(&one_u);
    _check(  (1 == u3b_maps()) && (0 == memcmp(two_u.map_y, dat_y, dat_d)),
            "first unmap disturbed the other" );
    u3b_unmap(&two_u);
  }

  //  a width far past the file reads zero throughout
  //
  {
    u3b_flat fla_u;
    c3_d     bit_d;
    c3_m     mot_m;
    c3_d     wid_d = dat_d + (3 * pag_d) + 5;

    _check(  (c3y == u3b_map(&fla_u, _tmp_bob, bid_d, wid_d, &bit_d, &mot_m))
          && (fla_u.ext_d >= wid_d),
            "padded map failed" );
    _check( 0 == memcmp(fla_u.map_y, dat_y, dat_d), "padded bytes" );
    for ( c3_d i_d = dat_d; i_d < wid_d; i_d++ ) {
      _check( !fla_u.map_y[i_d], "pad byte %" PRIc3_d " not zero", i_d );
    }
    u3b_unmap(&fla_u);
  }

  //  a padded anonymous copy: its bytes, then zeros, read-only
  //
  {
    u3b_flat fla_u;
    c3_d     wid_d = (2 * pag_d) + 3;

    _check(  (c3y == u3b_pad(&fla_u, dat_y, dat_d, wid_d))
          && (1 == u3b_maps())
          && (0 == memcmp(fla_u.map_y, dat_y, dat_d)),
            "pad copy failed" );
    for ( c3_d i_d = dat_d; i_d < wid_d; i_d++ ) {
      _check( !fla_u.map_y[i_d], "pad copy byte %" PRIc3_d " not zero", i_d );
    }
    u3b_unmap(&fla_u);
    _check( !u3b_maps(), "pad copy left a mapping" );
  }

  //  missing blob: declined with %file, nothing mapped
  //
  {
    u3b_flat fla_u;
    c3_d     bit_d;
    c3_m     mot_m = 0;

    _check(  (c3n == u3b_map(&fla_u, _tmp_bob, u3a_bid(0xdeadbeef, 999), 0,
                             &bit_d, &mot_m))
          && (c3__file == mot_m) && !u3b_maps(),
            "missing should decline with %%file" );
  }

  _pier_done();
}

//  shared state for the unwind tests: the blob, and a bob atom over it
//  (built on the home road).
//
static c3_h    _han_mug_h;
static c3_h    _han_seq_h;
static u3_atom _han_bob;

/* _hand_open_inner(): map the blob from the inner road through two
**   views, which the road lists as two mappings and the caller leaves
**   open for the road's unwind to release.
*/
static void
_hand_open_inner(void)
{
  u3r_view one_u, two_u;

  u3r_view_open(&one_u, _han_bob, 0, c3y);
  u3r_view_open(&two_u, _han_bob, 0, c3y);

  _check(  one_u.byt_y && two_u.byt_y && (one_u.byt_y != two_u.byt_y)
        && (2 == u3b_maps_road(u3R)) && !u3H->rod_u.fla_p,
          "views did not each map on the road" );
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

  const c3_y dat_y[] = "unwinding must release every mapping the road held";
  _check( c3y == _blob_save(dat_y, sizeof(dat_y) - 1,
                              &_han_mug_h, &_han_seq_h),
          "save failed" );
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);
}

/* _hand_unwind_check(): after the inner road is gone, nothing is mapped.
*/
static void
_hand_unwind_check(void)
{
  _check( !u3b_maps(), "%zu mapping(s) survived the road", u3b_maps() );
  _check( !u3H->rod_u.fla_p, "the home road lists a mapping" );
  u3z(_han_bob);
  _pier_done();
}

/* _test_hand_bail(): u3m_bail releases the bailing road's mappings.
*/
static void
_test_hand_bail(void)
{
  _hand_unwind_setup("hand bail");
  u3z(u3m_soft_top(0, 1 << 12, _hand_bail_cb, 0, 0));
  _hand_unwind_check();
}

/* _test_hand_signal(): a signal unwind releases every child road's
**   mappings.
*/
static void
_test_hand_signal(void)
{
  _hand_unwind_setup("hand signal");
  u3z(u3m_soft_top(0, 1 << 12, _hand_signal_cb, 0, 0));
  _hand_unwind_check();
}

/* _test_hand_leak(): a normal return without a close is drained at fall.
*/
static void
_test_hand_leak(void)
{
  _hand_unwind_setup("hand leak");
  u3z(u3m_soft_top(0, 1 << 12, _hand_leak_cb, 0, 0));
  _hand_unwind_check();
}

//  the home road's mapping in the outer test
//
static const c3_y* _han_hom_y;

static u3_noun
_hand_outer_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;

  //  an inner road maps the blob for itself, beside the home road's
  //  mapping, then bails with its view open
  //
  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  _check(  (_han_hom_y != vue_u.byt_y)
        && (1 == u3b_maps_road(u3R)) && (2 == u3b_maps()),
          "inner road did not map for itself" );
  return u3m_bail(c3__fail);
}

/* _test_hand_outer(): an inner road's bail releases its own mapping and
**   leaves the home road's view alone.
*/
static void
_test_hand_outer(void)
{
  _hand_unwind_setup("hand outer");

  u3r_view    vue_u;
  const c3_y* vue_y;

  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  vue_y = vue_u.byt_y;
  _check( vue_y, "home map failed" );
  _han_hom_y = vue_y;

  u3z(u3m_soft_top(0, 1 << 12, _hand_outer_cb, 0, 0));

  _check(  (1 == u3b_maps()) && (vue_y[0] == u3r_byte(0, _han_bob)),
          "home mapping swept (maps %zu)", u3b_maps() );

  u3r_view_done(&vue_u);
  _hand_unwind_check();
}

//  the hook's count, and the mappings live when it last ran
//
static c3_w _hold_hit_w;
static c3_z _hold_map_z;

static void
_hold_del_cb(c3_h mug_h, c3_h seq_h)
{
  _hold_hit_w++;
  _hold_map_z = u3b_maps();
  _blob_del_cb(mug_h, seq_h);
}

/* _test_view_hold(): a view retains its bob on the home road.  the
**   caller's last ref can go while the view is open, and the bob dies,
**   firing the deletion hook that returns the king's holds, only at
**   done, after the mapping is gone.
*/
static void
_test_view_hold(void)
{
  _pier_make("view hold");

  const c3_y dat_y[] = "a view keeps its bob, and the bob its file";
  c3_h       mug_h = 0, seq_h = 0;
  u3r_view   vue_u;
  void       (*old_f)(c3_h, c3_h) = u3C.blob_del_f;

  _check( c3y == _blob_save(dat_y, sizeof(dat_y) - 1, &mug_h, &seq_h),
          "save failed" );

  _hold_hit_w     = 0;
  _hold_map_z     = 0;
  u3C.blob_del_f = _hold_del_cb;

  {
    u3_atom bob = u3i_blob(mug_h, seq_h);

    u3r_view_open(&vue_u, bob, 0, c3y);
    u3z(bob);
  }

  _check(  !_hold_hit_w && (1 == u3b_maps())
        && (0 == memcmp(vue_u.byt_y, dat_y, sizeof(dat_y) - 1)),
          "the bob died under its view (hook %" PRIc3_w ")", _hold_hit_w );

  u3r_view_done(&vue_u);
  _check(  (1 == _hold_hit_w) && !_hold_map_z && !u3b_maps(),
          "done: hook %" PRIc3_w " with %zu mapping(s) live",
          _hold_hit_w, _hold_map_z );

  u3C.blob_del_f = old_f;
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

  //  hold a mapping for the whole spin, and open and close views so the
  //  event timer's SIGVTALRM can land inside a critical section (map,
  //  unmap) as easily as between two.  the timer counts user CPU time,
  //  which the syscalls barely consume, so each round also burns some
  //  arithmetic.  the round cap turns a timer that never fires into a
  //  test failure instead of a hang.
  //
  u3r_view vue_u;
  u3r_view_open(&vue_u, _han_bob, 0, c3y);

  volatile c3_d sum_d = 0;

  for ( c3_w rou_w = 0; rou_w < 2000000; rou_w++ ) {
    u3r_view tmp_u;
    u3r_view_open(&tmp_u, _han_bob, 0, c3y);
    sum_d += tmp_u.byt_y[0];
    u3r_view_done(&tmp_u);

    for ( c3_w i_w = 0; i_w < 20000; i_w++ ) {
      sum_d += i_w;
    }
  }

  u3r_view_done(&vue_u);
  return 0;
}

/* _test_hand_alarm(): a real SIGVTALRM from the event timer, delivered
**   through the production handler while a mapping is live and views
**   open and close, unwinds cleanly, releases the road's mappings, and
**   leaves the store readable.
*/
static void
_test_hand_alarm(void)
{
  _hand_unwind_setup("hand alarm");

  u3_noun gon = u3m_soft_top(20, 1 << 12, _hand_alarm_cb, 0, 0);
  _check( c3__alrm == _hand_mote(gon), "no %%alrm unwind" );
  u3z(gon);

  //  the store reads after the unwind
  //
  {
    u3r_view vue_u;
    _check( c3y == u3r_view_open(&vue_u, _han_bob, 0, c3n),
            "open after unwind failed" );
    u3r_view_done(&vue_u);
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

/* _test_jets_bob(): the atom jets treat a bob the same way they do the
**   materialized atom: the slicers over ranges that touch, cross, and
**   lie beyond the end of the file; the jets that load a bob onto the
**   loom; jam-shax, which streams it; the hashes and ed25519, which
**   borrow it; and lore over a page-long file.
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

  //  rip reads its source a word at a time through u3r_word and sew
  //  loads it, so a bob three bytes over a page exercises a partial
  //  last word on both paths; the steps keep the lists small
  //
  {
    const c3_w odd_w = 4099;
    c3_y*      odd_y = c3_malloc(odd_w);

    for ( c3_w i_w = 0; i_w < odd_w; i_w++ ) {
      odd_y[i_w] = (c3_y)(1 + ((i_w * 13) % 251));
    }

    c3_h om_h = 0; c3_h os_h = 0;
    _check( c3y == _blob_save(odd_y, odd_w, &om_h, &os_h), "odd save failed" );

    u3_atom rob = u3i_blob(om_h, os_h);
    u3_atom rol = u3i_bytes(odd_w, odd_y);

    //  rip: bit, nibble, byte, and word bloqs; unit and odd steps
    //
    {
      const c3_w rip_w[][2] = {
        { 0, 29 }, { 0, 31 }, { 2, 5 }, { 3, 3 }, { 3, 7 }, { 4, 3 },
        { 5, 1 }, { 5, 2 }, { 6, 1 }
      };
      for ( c3_w i_w = 0; i_w < sizeof(rip_w) / sizeof(*rip_w); i_w++ ) {
        _jet_same("rip", rip_w[i_w][0], rip_w[i_w][1], 0,
                  u3qc_rip(rip_w[i_w][0], rip_w[i_w][1], rob),
                  u3qc_rip(rip_w[i_w][0], rip_w[i_w][1], rol));
      }
    }

    //  sew: a patch inside, across, and past the end of the source,
    //  with the source a bob, the patch a bob, and both
    //
    {
      const c3_w sew_w[][3] = {   //  bloq, pos, wid
        { 3, 10, 4 }, { 3, 0, 4099 }, { 3, 4090, 20 }, { 3, 4099, 8 },
        { 0, 13, 100 }, { 0, 32790, 30 }, { 5, 100, 3 }, { 5, 1024, 2 }
      };
      for ( c3_w i_w = 0; i_w < sizeof(sew_w) / sizeof(*sew_w); i_w++ ) {
        c3_w a_w = sew_w[i_w][0], b_w = sew_w[i_w][1], c_w = sew_w[i_w][2];

        _jet_same("sew", a_w, b_w, c_w,
                  u3qc_sew(a_w, b_w, c_w, 0x5a5a5a5a, rob),
                  u3qc_sew(a_w, b_w, c_w, 0x5a5a5a5a, rol));
        _jet_same("sew", a_w, b_w, c_w,
                  u3qc_sew(a_w, b_w, c_w, bob, rob),
                  u3qc_sew(a_w, b_w, c_w, loa, rol));
      }
      _jet_same("sew", 3, 100, 5000,
                u3qc_sew(3, 100, 5000, rob, loa),
                u3qc_sew(3, 100, 5000, rol, loa));
    }

    u3z(rob); u3z(rol);
    c3_free(odd_y);
  }

  //  jets that read an atom's words directly load a bob onto the loom
  //  first: aor, sew (above), add, sub, and the octs scanners; muk
  //  copies its key instead
  //
  {
    u3_atom lob = u3i_bytes(len_w, dat_y);
    ((u3a_atom*)u3a_to_ptr(lob))->buf_w[0] ^= 0x80;   //  differs in byte 0

    _jet_same("aor", 0, 0, 0, u3qc_aor(bob, loa), u3qc_aor(loa, loa));
    _jet_same("aor", 0, 0, 1, u3qc_aor(bob, lob), u3qc_aor(loa, lob));
    _jet_same("aor", 0, 0, 2, u3qc_aor(lob, bob), u3qc_aor(lob, loa));
    _jet_same("aor", 0, 0, 3, u3qc_aor(bob, 0x616263), u3qc_aor(loa, 0x616263));
    u3z(lob);

    _jet_same("add", 0, 0, 0, u3qa_add(bob, 1), u3qa_add(loa, 1));
    _jet_same("add", 0, 0, 1, u3qa_add(bob, bob), u3qa_add(loa, loa));
    _jet_same("add", 0, 0, 2, u3qa_add(7, bob), u3qa_add(7, loa));
    _jet_same("sub", 0, 0, 0, u3qa_sub(bob, 1), u3qa_sub(loa, 1));
    _jet_same("sub", 0, 0, 1, u3qa_sub(bob, loa), u3qa_sub(loa, bob));

    //  muk with the length equal to the key's, which hashes the words
    //  in place for a loom atom, and with a longer length
    //
    _jet_same("muk", len_w, 0, 0,
              u3qc_muk(0xcafe, len_w, bob), u3qc_muk(0xcafe, len_w, loa));
    _jet_same("muk", len_w + 10, 0, 0,
              u3qc_muk(0xcafe, len_w + 10, bob), u3qc_muk(0xcafe, len_w + 10, loa));

    //  can, cat, rap, swp: byte and word bloqs, bob first and second
    //
    _jet_same("cat", 3, 0, 0, u3qc_cat(3, bob, loa), u3qc_cat(3, loa, loa));
    _jet_same("cat", 3, 1, 0, u3qc_cat(3, 0x616263, bob), u3qc_cat(3, 0x616263, loa));
    _jet_same("cat", 5, 0, 0, u3qc_cat(5, bob, 0x11), u3qc_cat(5, loa, 0x11));
    _jet_same("swp", 3, 0, 0, u3qc_swp(3, bob), u3qc_swp(3, loa));
    _jet_same("swp", 0, 0, 0, u3qc_swp(0, bob), u3qc_swp(0, loa));
    {
      u3_noun lib = u3nt(u3nc(len_w, u3k(bob)), u3nc(3, 0x616263), u3_nul);
      u3_noun lil = u3nt(u3nc(len_w, u3k(loa)), u3nc(3, 0x616263), u3_nul);
      _jet_same("can", 3, 0, 0, u3qc_can(3, lib), u3qc_can(3, lil));
      _jet_same("can", 0, 0, 0, u3qc_can(0, lib), u3qc_can(0, lil));
      u3z(lib); u3z(lil);

      lib = u3nt(0x616263, u3k(bob), u3_nul);
      lil = u3nt(0x616263, u3k(loa), u3_nul);
      _jet_same("rap", 3, 0, 0, u3qc_rap(3, lib), u3qc_rap(3, lil));
      _jet_same("rap", 5, 0, 0, u3qc_rap(5, lib), u3qc_rap(5, lil));
      u3z(lib); u3z(lil);
    }

    //  octs scanners: crc32 and adler32 through their gate wrappers
    //
    {
      u3_noun cob = u3nt(0, u3nc(len_w + 3, u3k(bob)), 0);
      u3_noun col = u3nt(0, u3nc(len_w + 3, u3k(loa)), 0);
      _jet_same("crc32", 0, 0, 0, u3we_crc32(cob), u3we_crc32(col));
      _jet_same("adler32", 0, 0, 0, u3we_adler32(cob), u3we_adler32(col));
      u3z(cob); u3z(col);
    }

    //  jam-shax streams a bob without loading it
    //
    _jet_same("jam-shax", 0, 0, 0, u3qe_jam_shax(bob), u3qe_jam_shax(loa));
    {
      u3_noun cel = u3nc(u3k(bob), u3k(bob));
      u3_noun cll = u3nc(u3k(loa), u3k(loa));
      _jet_same("jam-shax", 1, 0, 0, u3qe_jam_shax(cel), u3qe_jam_shax(cll));
      u3z(cel); u3z(cll);
    }
  }

  //  the hashes and ed25519 borrow a bob's bytes through a view: widths
  //  inside the atom (borrowed), at its end, and past it (padded)
  //
  {
    const c3_w wid_w[] = { 0, 100, len_w, len_w + 10 };
    for ( c3_w i_w = 0; i_w < sizeof(wid_w) / sizeof(*wid_w); i_w++ ) {
      u3_noun cob = u3nt(0, u3nc(wid_w[i_w], u3k(bob)), 0);
      u3_noun col = u3nt(0, u3nc(wid_w[i_w], u3k(loa)), 0);
      _jet_same("shay", wid_w[i_w], 0, 0, u3we_shay(cob), u3we_shay(col));
      _jet_same("shal", wid_w[i_w], 0, 0, u3we_shal(cob), u3we_shal(col));
      _jet_same("keccak-256", wid_w[i_w], 0, 0,
                u3we_kecc256(cob), u3we_kecc256(col));
      u3z(cob); u3z(col);
    }

    {
      u3_noun cob = u3nt(0, u3k(bob), 0);
      u3_noun col = u3nt(0, u3k(loa), 0);
      _jet_same("shax", 0, 0, 0, u3we_shax(cob), u3we_shax(col));
      u3z(cob); u3z(col);
    }

    //  a signature over the bob is the one over the loom atom, and
    //  verifies over both
    //
    {
      u3_atom sed = 0x1234567;
      u3_noun cop = u3nt(0, sed, 0);
      u3_atom pub = u3wee_puck(cop);
      u3_noun cob = u3nt(0, u3nc(u3k(bob), sed), 0);
      u3_noun col = u3nt(0, u3nc(u3k(loa), sed), 0);
      u3_atom sig = u3wee_sign(cob);

      _jet_same("ed-sign", 0, 0, 0, u3k(sig), u3wee_sign(col));
      {
        u3_noun vob = u3nt(0, u3nt(u3k(sig), u3k(bob), u3k(pub)), 0);
        u3_noun vol = u3nt(0, u3nt(u3k(sig), u3k(loa), u3k(pub)), 0);
        _check( c3y == u3wee_veri(vob), "ed-veri rejects the bob" );
        _check( c3y == u3wee_veri(vol), "ed-veri rejects the loom atom" );
        u3z(vob); u3z(vol);
      }
      {
        u3_noun vob = u3nt(0, u3nt(u3k(sig), u3nc(len_w, u3k(bob)), u3k(pub)), 0);
        _check( c3y == u3wee_veri_octs(vob), "ed-veri-octs rejects the bob" );
        u3z(vob);
      }
      u3z(sig); u3z(pub);
      u3z(cop); u3z(cob); u3z(col);
    }
  }

  //  widths far past the atom, more than a page of zeros: a bob's view
  //  maps a zero tail, and a loom atom's a padded copy
  //
  {
    const c3_w wid_w = len_w + (3 * 16384) + 5;
    u3_noun    cob   = u3nt(0, u3nc(wid_w, u3k(bob)), 0);
    u3_noun    col   = u3nt(0, u3nc(wid_w, u3k(loa)), 0);

    _jet_same("shay", wid_w, 0, 0, u3we_shay(cob), u3we_shay(col));
    _jet_same("shal", wid_w, 0, 0, u3we_shal(cob), u3we_shal(col));
    _jet_same("keccak-256", wid_w, 0, 0, u3we_kecc256(cob), u3we_kecc256(col));
    u3z(cob); u3z(col);

    //  ed25519 over the padded message: sign-octs agrees, and the
    //  signature verifies over both
    //
    {
      u3_atom sed = 0x1234567;
      u3_noun sob = u3nt(0, u3nc(u3nc(wid_w, u3k(bob)), sed), 0);
      u3_noun sol = u3nt(0, u3nc(u3nc(wid_w, u3k(loa)), sed), 0);
      u3_noun cop = u3nt(0, sed, 0);
      u3_atom sig = u3wee_sign_octs(sob);
      u3_atom pub = u3wee_puck(cop);

      _jet_same("ed-sign-octs", wid_w, 0, 0, u3k(sig), u3wee_sign_octs(sol));
      {
        u3_noun vob = u3nt(0, u3nt(u3k(sig), u3nc(wid_w, u3k(bob)), u3k(pub)), 0);
        _check( c3y == u3wee_veri_octs(vob), "ed-veri-octs rejects the padded bob" );
        u3z(vob);
      }
      u3z(sig); u3z(pub); u3z(sob); u3z(sol); u3z(cop);
    }

    //  blake3: hash and chunk output read their message at its width
    //
    {
      u3_noun ctx = u3nc(0, u3nc(u3nc(0, 0), 0));
      u3_noun hob = u3nt(0, u3nc(32, u3nc(wid_w, u3k(bob))), u3k(ctx));
      u3_noun hol = u3nt(0, u3nc(32, u3nc(wid_w, u3k(loa))), u3k(ctx));
      _jet_same("blake3-hash", wid_w, 0, 0,
                u3we_blake3_hash(hob), u3we_blake3_hash(hol));
      u3z(hob); u3z(hol);

      hob = u3nt(0, u3nc(0, u3nc(wid_w, u3k(bob))), u3k(ctx));
      hol = u3nt(0, u3nc(0, u3nc(wid_w, u3k(loa))), u3k(ctx));
      _jet_same("blake3-chunk", wid_w, 0, 0,
                u3we_blake3_chunk_output(hob), u3we_blake3_chunk_output(hol));
      u3z(hob); u3z(hol);
      u3z(ctx);
    }

    //  scrypt pbkdf: the password and the salt each at their widths
    //
    {
      u3_noun pob = u3nt(0, u3nq(u3k(bob), len_w + 10, u3k(bob), u3nt(len_w + 7, 1, 32)), 0);
      u3_noun pol = u3nt(0, u3nq(u3k(loa), len_w + 10, u3k(loa), u3nt(len_w + 7, 1, 32)), 0);
      _jet_same("scrypt-pbl", len_w + 10, 0, 0, u3wes_pbl(pob), u3wes_pbl(pol));
      u3z(pob); u3z(pol);
    }
  }

  //  the loops that read an atom one element per step open one view of
  //  it: the bitwise and counting jets, rip, the etchers, base16, rub,
  //  fitz, u3r_sing_c, and the legacy cue
  //
  {
    _jet_same("mix", 0, 0, 0, u3qc_mix(0x616263, bob), u3qc_mix(0x616263, loa));
    _jet_same("mix", 1, 0, 0, u3qc_mix(bob, bob), u3qc_mix(loa, loa));
    _jet_same("con", 0, 0, 0, u3qc_con(0x616263, bob), u3qc_con(0x616263, loa));
    _jet_same("dis", 0, 0, 0, u3qc_dis(bob, 0x7f7f7f7f), u3qc_dis(loa, 0x7f7f7f7f));
    _jet_same("dis", 1, 0, 0, u3qc_dis(0x7f7f7f7f, bob), u3qc_dis(0x7f7f7f7f, loa));
    _jet_same("ham", 0, 0, 0, u3qc_ham(bob), u3qc_ham(loa));
    _jet_same("ctz", 0, 0, 0, u3qc_ctz(bob), u3qc_ctz(loa));
    _jet_same("clz", 3, len_w + 4, 0,
              u3qc_clz(3, len_w + 4, bob), u3qc_clz(3, len_w + 4, loa));
    _jet_same("clz", 0, (len_w * 8) - 3, 0,
              u3qc_clz(0, (len_w * 8) - 3, bob), u3qc_clz(0, (len_w * 8) - 3, loa));
    _jet_same("etch-ux", 0, 0, 0, u3s_etch_ux(bob), u3s_etch_ux(loa));
    _jet_same("etch-uv", 0, 0, 0, u3s_etch_uv(bob), u3s_etch_uv(loa));
    _jet_same("etch-uw", 0, 0, 0, u3s_etch_uw(bob), u3s_etch_uw(loa));
    _jet_same("en-base16", len_w + 3, 0, 0,
              u3qe_en_base16(len_w + 3, bob), u3qe_en_base16(len_w + 3, loa));
    _jet_same("rub", 1, 0, 0, u3qe_rub(1, bob), u3qe_rub(1, loa));
    _jet_same("fitz", 0, 0, 0, u3qf_fitz(bob, loa), u3qf_fitz(loa, loa));

    {
      c3_c* str_c = c3_malloc(len_w + 1);
      memcpy(str_c, dat_y, len_w);
      str_c[len_w] = 0;
      _check( c3y == u3r_sing_c(str_c, bob), "sing_c rejects the bob" );
      str_c[17] ^= 1;
      _check( c3n == u3r_sing_c(str_c, bob), "sing_c accepts a mismatch" );
      c3_free(str_c);
    }

    //  de-base16 over hex text of odd and even length
    //
    {
      const c3_w hex_w[] = { 4099, 4100 };

      for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
        c3_y* hex_y = c3_malloc(hex_w[i_w]);

        for ( c3_w j_w = 0; j_w < hex_w[i_w]; j_w++ ) {
          hex_y[j_w] = (c3_y)"0123456789abcdef"[(j_w * 7) % 16];
        }
        hex_y[hex_w[i_w] - 1] = 'a';

        c3_h hm_h = 0; c3_h hs_h = 0;
        _check( c3y == _blob_save(hex_y, hex_w[i_w], &hm_h, &hs_h), "hex save failed" );

        u3_atom hob = u3i_blob(hm_h, hs_h);
        u3_atom hol = u3i_bytes(hex_w[i_w], hex_y);
        _jet_same("de-base16", hex_w[i_w], 0, 0,
                  u3qe_de_base16(hob), u3qe_de_base16(hol));
        u3z(hob); u3z(hol);
        c3_free(hex_y);
      }
    }

    //  the legacy cue reads its tag bits through one view
    //
    {
      u3_noun non = u3nt(u3k(loa), 42, u3k(loa));
      c3_d    jam_d = 0;
      c3_y*   jam_y = 0;
      c3_h    jm_h = 0; c3_h js_h = 0;

      u3s_jam_xeno(non, &jam_d, &jam_y);
      _check( c3y == _blob_save(jam_y, jam_d, &jm_h, &js_h), "jam save failed" );

      u3_atom job = u3i_blob(jm_h, js_h);
      u3_noun cue = u3s_cue(job);
      _check( c3y == u3r_sing(cue, non), "legacy cue of a bob differs" );
      u3z(cue); u3z(job); u3z(non);
      c3_free(jam_y);
    }
  }

  //  lore and leer over a bob exactly one page long whose last byte
  //  is not a newline, the shape whose final read once left the mapping
  //
  {
    const c3_w pag_w = 4096;
    c3_y*      lin_y = c3_malloc(pag_w);

    for ( c3_w i_w = 0; i_w < pag_w; i_w++ ) {
      lin_y[i_w] = ( 99 == (i_w % 100) ) ? '\n' : (c3_y)('a' + (i_w % 26));
    }

    c3_h lm_h = 0; c3_h ls_h = 0;
    _check( c3y == _blob_save(lin_y, pag_w, &lm_h, &ls_h), "lines save failed" );

    u3_atom lin = u3i_blob(lm_h, ls_h);
    u3_atom lil = u3i_bytes(pag_w, lin_y);

    _jet_same("lore", 0, 0, 0, u3qe_lore(lin), u3qe_lore(lil));
    _jet_same("leer", 0, 0, 0, u3qe_leer(lin), u3qe_leer(lil));

    //  lune wants the last byte a newline
    //
    {
      c3_h nm_h = 0; c3_h ns_h = 0;
      lin_y[pag_w - 1] = '\n';
      _check( c3y == _blob_save(lin_y, pag_w, &nm_h, &ns_h), "lune save failed" );

      u3_atom lun = u3i_blob(nm_h, ns_h);
      u3_atom lul = u3i_bytes(pag_w, lin_y);
      _jet_same("lune", 0, 0, 0, u3qe_lune(lun), u3qe_lune(lul));
      u3z(lun); u3z(lul);
    }

    u3z(lin); u3z(lil);
    c3_free(lin_y);
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

  u3m_crit_inn();
  kill(getpid(), SIGINT);
  _han_hit_w = 1;         //  still here: the signal is pending
  u3m_crit_out();       //  delivered here; never returns
  _han_hit_w = 2;
  return 0;
}

/* _test_hand_intr(): a real SIGINT through the production handler
**   unwinds the road and releases its mappings.
*/
static void
_test_hand_intr(void)
{
  _hand_unwind_setup("hand intr");

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_intr_cb, 0, 0);
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

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_intr_crit_cb, 0, 0);
  _check( (c3__intr == _hand_mote(gon)) && (1 == _han_hit_w),
          "hit %" PRIc3_w ", expected the unwind at leave", _han_hit_w );
  u3z(gon);

  //  the hold count is zero: a fresh section delivers at its own leave,
  //  and a nested section only at the outermost
  //
  {
    void (*old_f)(int) = signal(SIGINT, _crit_handler);
    _crit_hit = 0;
    u3m_crit_inn();
    raise(SIGINT);
    _check( 0 == _crit_hit, "hold count not reset" );
    u3m_crit_out();
    _check( 1 == _crit_hit, "section not delivering" );

    _crit_hit = 0;
    u3m_crit_inn();
    u3m_crit_inn();
    raise(SIGINT);
    u3m_crit_out();
    _check( 0 == _crit_hit, "delivered at inner leave" );
    u3m_crit_out();
    _check( 1 == _crit_hit, "not delivered at outer leave" );
    signal(SIGINT, old_f);
  }

  _hand_unwind_check();
}
#endif

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

/* _test_hand_cue(): cue bailing under a live view leaves nothing mapped.
*/
static void
_test_hand_cue(void)
{
  _pier_make("hand cue");

  //  a backref to bit 0, which holds the backref itself: cue bails
  //  %exit on the lookup.  (a run-length prefix too long to read no
  //  longer bails, since +cut is total on wide widths.)  the high byte
  //  keeps the atom 64 bytes wide
  //
  c3_y dat_y[64];
  memset(dat_y, 0, sizeof(dat_y));
  dat_y[0] = 0x07;
  dat_y[sizeof(dat_y) - 1] = 0x80;

  _check( c3y == _blob_save(dat_y, sizeof(dat_y),
                              &_han_mug_h, &_han_seq_h),
          "save failed" );
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_cue_cb, 0, 0);
  _check( c3y == _hand_unwound(gon), "garbage cued" );
  u3z(gon);

  _hand_unwind_check();
}

#ifndef U3_OS_windows
//  blobs for the descriptor tests: mug 0x2345, seqs 1 through 32
//
#define HAND_EMF_W  32

static u3_atom _hand_emf_bob[HAND_EMF_W];

static u3_noun
_hand_emfile_cb(u3_noun arg)
{
  (void)arg;
  const c3_w  hal_w = HAND_EMF_W / 2;
  const c3_y* par_y[HAND_EMF_W / 2];
  u3r_view    par_u[HAND_EMF_W / 2];

  //  a ceiling one descriptor above the lowest free one: a map holds its
  //  fd only while it maps
  //
  struct rlimit old_u, new_u;
  c3_i          fre_i = dup(0);

  close(fre_i);
  getrlimit(RLIMIT_NOFILE, &old_u);
  new_u = old_u;
  new_u.rlim_cur = (rlim_t)fre_i + 1;
  _check( 0 == setrlimit(RLIMIT_NOFILE, &new_u), "setrlimit failed" );

  //  the parent maps half the blobs and keeps every view live
  //
  for ( c3_w i_w = 0; i_w < hal_w; i_w++ ) {
    u3r_view_open(&par_u[i_w], _hand_emf_bob[i_w], 0, c3y);
    par_y[i_w] = par_u[i_w].byt_y;
    _check( par_y[i_w], "parent map %" PRIc3_w " failed", i_w );
  }

  //  a child maps and copies the rest, and reads the parent's
  //
  u3m_leap(512);
  for ( c3_w i_w = hal_w; i_w < HAND_EMF_W; i_w++ ) {
    u3r_view    vue_u;
    const c3_y* vue_y;
    c3_y        win_y[8];

    u3r_view_open(&vue_u, _hand_emf_bob[i_w], 0, c3y);
    vue_y = vue_u.byt_y;
    _check(  vue_y
          && (8 == u3r_view_copy(&vue_u, 0, win_y, 8))
          && (0 == memcmp(win_y, vue_y, 8)),
            "child read %" PRIc3_w " failed", i_w );
  }
  for ( c3_w i_w = 0; i_w < hal_w; i_w++ ) {
    c3_y win_y[8];
    u3r_bytes(0, 8, win_y, _hand_emf_bob[i_w]);
    _check( 0 == memcmp(win_y, par_y[i_w], 8), "parent's blob %" PRIc3_w, i_w );
  }
  _check( hal_w == u3b_maps_road(u3R),
          "%zu child mapping(s)", u3b_maps_road(u3R) );
  u3m_fall();

  setrlimit(RLIMIT_NOFILE, &old_u);

  _check( hal_w == u3b_maps_road(u3R), "the child's fall took the parent's" );

  for ( c3_w i_w = 0; i_w < hal_w; i_w++ ) {
    u3r_view_done(&par_u[i_w]);
  }
  _check( !u3b_maps_road(u3R), "done left a mapping" );
  return 0;
}

/* _test_hand_emfile(): one descriptor from the ceiling, every read
**   still succeeds: no mapping holds an fd.
*/
static void
_test_hand_emfile(void)
{
  _pier_make("hand emfile");

  for ( c3_w i_w = 0; i_w < HAND_EMF_W; i_w++ ) {
    _hand_write_raw(0x2345, i_w + 1, i_w + 1);
    _hand_emf_bob[i_w] = u3i_blob(0x2345, i_w + 1);
  }

  u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_emfile_cb, 0, 0);
  _check( c3n == _hand_unwound(gon), "a read failed at the ceiling" );
  u3z(gon);

  for ( c3_w i_w = 0; i_w < HAND_EMF_W; i_w++ ) {
    u3z(_hand_emf_bob[i_w]);
  }

  _pier_done();
}

/* _test_hand_nofd(): a live view holds no descriptor.
*/
static void
_test_hand_nofd(void)
{
  _pier_make("hand nofd");

  c3_t     was_t[256];
  u3r_view vue_u[4];
  u3_atom  bob[4];

  for ( c3_i i_i = 0; i_i < 256; i_i++ ) {
    was_t[i_i] = _fd_open(i_i);
  }

  for ( c3_w i_w = 0; i_w < 4; i_w++ ) {
    _hand_write_raw(0x2345, i_w + 1, i_w + 1);
    bob[i_w] = u3i_blob(0x2345, i_w + 1);
    _check( c3y == u3r_view_open(&vue_u[i_w], bob[i_w], 0, c3n),
            "open %" PRIc3_w " failed", i_w );
  }

  for ( c3_i i_i = 0; i_i < 256; i_i++ ) {
    _check( was_t[i_i] || !_fd_open(i_i), "fd %d left open", i_i );
  }

  for ( c3_w i_w = 0; i_w < 4; i_w++ ) {
    u3r_view_done(&vue_u[i_w]);
    u3z(bob[i_w]);
  }

  _pier_done();
}

/* _test_hand_wipe(): a file later installed at a wiped id reads as
**   itself: every open maps the file the path names now.
*/
static void
_test_hand_wipe(void)
{
  _pier_make("hand wipe");

  const c3_h mug_h = 0x3456;
  const c3_h seq_h = 1;
  c3_y       one_y[32];
  c3_y       two_y[32];
  u3_atom    bob   = u3i_blob(mug_h, seq_h);
  u3r_view   vue_u;

  _hand_write_raw(mug_h, seq_h, 1);
  _check( c3y == u3r_view_open(&vue_u, bob, 0, c3n), "first open failed" );
  u3r_view_copy(&vue_u, 0, one_y, 32);
  u3r_view_done(&vue_u);

  u3b_wipe(_tmp_bob, mug_h, seq_h);
  _check( c3n == u3b_live(_tmp_bob, mug_h, seq_h), "wipe left the file" );
  _check(  (c3n == u3r_view_open(&vue_u, bob, 0, c3n)) && !u3b_maps(),
          "opened a wiped blob" );

  //  the id's next file: a reused seq
  //
  _hand_write_raw(mug_h, seq_h, 2);
  _check( c3y == u3r_view_open(&vue_u, bob, 0, c3n), "second open failed" );
  u3r_view_copy(&vue_u, 0, two_y, 32);
  u3r_view_done(&vue_u);
  _check( 0 != memcmp(one_y, two_y, 32), "read the wiped file's bytes" );

  u3z(bob);
  _pier_done();
}

/* _test_hand_trunc(): a file whose last byte is zero is no blob: a map
**   declines it with %file and leaves nothing mapped.
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

  {
    //  blob files are created read-only; rewriting one needs write bits
    //
    c3_c pax_c[8192];
    u3b_path(pax_c, _tmp_bob, mug_h, seq_h);
    dat_y[len_w - 1] = 0;
    _check(  (0 == chmod(pax_c, 0600))
          && (c3y == _file_put(pax_c, (c3_c*)dat_y, len_w)),
            "rewrite failed" );
  }

  {
    u3b_flat fla_u;
    c3_d     bit_d;
    c3_m     mot_m = 0;

    _check(  (c3n == u3b_map(&fla_u, _tmp_bob, u3a_bid(mug_h, seq_h), 0,
                             &bit_d, &mot_m))
          && (c3__file == mot_m) && !u3b_maps(),
            "map did not decline cleanly" );
  }

  c3_free(dat_y);
  _pier_done();
}
#endif

static u3_noun
_hand_gone_view_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;
  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  (void)vue_u.byt_y;
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

/* _hand_gone_expect(): [fun_f] over a bob with no file must bail %file
**   and leave nothing mapped.
*/
static void
_hand_gone_expect(const c3_c* nam_c, u3_funk fun_f)
{
  u3_noun gon = u3m_soft_top(0, 1 << 12, fun_f, 0, 0);
  _check( c3__file == _hand_mote(gon), "%s did not bail %%file", nam_c );
  u3z(gon);
  _check( !u3b_maps(), "%s left residue", nam_c );
}

/* _test_hand_gone(): a bob whose file is missing fails at every entry
**   point the way its contract says: bail %file for readers, a declined
**   open for a view opened without bal_o.
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

  //  an open without bal_o reports the missing file without bailing
  //
  {
    u3r_view win_u;
    _check( (c3n == u3r_view_open(&win_u, _han_bob, 0, c3n)) && !u3b_maps(),
            "open did not decline" );
  }

  u3z(_han_bob);
  _pier_done();
}

#if !defined(VERE64) && !defined(U3_OS_windows)
/* _test_jam_wide(): a bob of 2^32 bits or more jams through xeno,
**   whose stream counts in chubs: the jam is an atom tag, the length,
**   and the bits, all zero but the last; jam-shax hashes those bytes.
**   the file is sparse: 512 MiB of hole and a last byte.
*/
static void
_test_jam_wide(void)
{
  _pier_make("jam wide");

  _han_mug_h = 0x7777;
  _han_seq_h = 1;
  {
    c3_c dir_c[8192], fil_c[8192];
    snprintf(dir_c, sizeof(dir_c), "%s/" U3B_MUG_FMT, _tmp_bob, _han_mug_h);
    _check( 0 == mkdir(dir_c, 0700), "mkdir %s: %s", dir_c, strerror(errno) );
    u3b_path(fil_c, _tmp_bob, _han_mug_h, _han_seq_h);

    c3_i fid_i = open(fil_c, O_WRONLY | O_CREAT | O_EXCL, 0600);
    c3_y one_y = 1;
    _check( -1 != fid_i, "open %s: %s", fil_c, strerror(errno) );
    _check( 1 == pwrite(fid_i, &one_y, 1, (off_t)1 << 29), "pwrite: %s",
            strerror(errno) );
    close(fid_i);
  }
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);

  c3_d met_d = u3r_met_d(0, _han_bob);
  _check( met_d > c3_w_max, "not wider than a word" );

  c3_d  len_d = 0;
  c3_y* byt_y = 0;
  u3s_jam_xeno(_han_bob, &len_d, &byt_y);

  {
    ur_bsr_t     red_u = {0};
    ur_cue_tag_e tag_e;
    uint64_t     hed_d = 0;

    _check( ur_cue_good == ur_bsr_init(&red_u, len_d, byt_y), "bsr init" );
    _check(  (ur_cue_good == ur_bsr_tag(&red_u, &tag_e))
          && (ur_jam_atom == tag_e),
            "jam is not an atom" );
    _check(  (ur_cue_good == ur_bsr_rub_len(&red_u, &hed_d))
          && (met_d == hed_d),
            "length %" PRIu64 " != %" PRIu64, hed_d, met_d );

    c3_d pos_d = red_u.bits;
    c3_d end_d = pos_d + met_d - 1;   //  the atom's top bit
    c3_o zer_o = c3y;

    _check( len_d == ((end_d >> 3) + 1), "jam is %" PRIu64 " bytes", len_d );
    _check( (byt_y[end_d >> 3] >> (end_d & 7)) == 1, "top bit lost" );

    for ( c3_d i_d = pos_d; i_d < end_d; ) {
      if ( (i_d & 7) || ((end_d - i_d) < 8) ) {
        if ( (byt_y[i_d >> 3] >> (i_d & 7)) & 1 ) { zer_o = c3n; break; }
        i_d++;
      }
      else {
        if ( byt_y[i_d >> 3] ) { zer_o = c3n; break; }
        i_d += 8;
      }
    }
    _check( c3y == zer_o, "a hole bit is set" );
  }

  //  jam-shax hashes the same encoding, a chub-long mat prefix included
  //
  {
    c3_y    wan_y[32], hav_y[32];
    u3_atom has = u3qe_jam_shax(_han_bob);

    urcrypt_shay(byt_y, (size_t)len_d, wan_y);
    u3r_bytes(0, 32, hav_y, has);
    _check( 0 == memcmp(wan_y, hav_y, 32), "jam-shax is not shax of the jam" );
    u3z(has);
  }
  c3_free(byt_y);

  u3z(_han_bob);
  _pier_done();
}
#endif

#ifndef U3_OS_windows
/* _test_units_wide(): u3r_halfs, u3r_chubs and u3r_words read a bob
**   at a unit offset whose byte offset is past 4 GiB, which a 32-bit
**   byte offset would wrap to the start.  the file is sparse: 4 GiB of
**   hole and a last chub.
*/
static void
_test_units_wide(void)
{
  _pier_make("units wide");

  const c3_y las_y[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  const c3_d off_d    = (c3_d)1 << 32;

  _han_mug_h = 0x7778;
  _han_seq_h = 1;
  {
    c3_c dir_c[8192], fil_c[8192];
    snprintf(dir_c, sizeof(dir_c), "%s/" U3B_MUG_FMT, _tmp_bob, _han_mug_h);
    _check( 0 == mkdir(dir_c, 0700), "mkdir %s: %s", dir_c, strerror(errno) );
    u3b_path(fil_c, _tmp_bob, _han_mug_h, _han_seq_h);

    c3_i fid_i = open(fil_c, O_WRONLY | O_CREAT | O_EXCL, 0600);
    _check( -1 != fid_i, "open %s: %s", fil_c, strerror(errno) );
    _check( 8 == pwrite(fid_i, las_y, 8, (off_t)off_d), "pwrite: %s",
            strerror(errno) );
    close(fid_i);
  }
  _han_bob = u3i_blob(_han_mug_h, _han_seq_h);

  //  each reader takes the last chub and the zero chub past the end
  //
  {
    c3_y wan_y[16] = {0};
    memcpy(wan_y, las_y, 8);

    {
      c3_h hav_h[4];
      u3r_halfs(off_d >> u3a_half_bytes_shift, 4, hav_h, _han_bob);
      _check( 0 == memcmp(wan_y, hav_h, 16), "u3r_halfs misread" );
    }
    {
      c3_d hav_d[2];
      u3r_chubs(off_d >> u3a_chub_bytes_shift, 2, hav_d, _han_bob);
      _check( 0 == memcmp(wan_y, hav_d, 16), "u3r_chubs misread" );
    }
    {
      c3_w hav_w[16 >> u3a_word_bytes_shift];
      u3r_words(off_d >> u3a_word_bytes_shift, 16 >> u3a_word_bytes_shift,
                hav_w, _han_bob);
      _check( 0 == memcmp(wan_y, hav_w, 16), "u3r_words misread" );
    }
  }

  u3z(_han_bob);
  _pier_done();
}
#endif

#ifndef U3_OS_windows
/* _hand_grab_cb(): on an inner road, keep a mapping past its view,
**   then mark and sweep the road with leaks fatal.
*/
static u3_noun
_hand_grab_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;
  c3_y     fir_y;

  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  fir_y = vue_u.byt_y[0];
  _check( 1 == u3b_maps_road(u3R), "no mapping listed" );

  u3C.wag_h |= u3o_leak_crash;
  u3a_mark_init();
  {
    u3m_quac** qua_u = u3m_mark();
    for ( c3_w i_w = 0; qua_u[i_w]; i_w++ ) {
      u3a_quac_free(qua_u[i_w]);
    }
    c3_free(qua_u);
  }
  u3a_sweep();

  //  the sweep kept the record: the list still reaches it, and done
  //  unlists and frees it
  //
  _check(  (1 == u3b_maps_road(u3R)) && (fir_y == vue_u.byt_y[0]),
          "the sweep took a live mapping's record" );
  u3r_view_done(&vue_u);
  _check( !u3b_maps_road(u3R), "done left the record listed" );
  return 0;
}

/* _test_hand_grab(): a sweep on an inner road, as -g's u3m_grab runs
**   at every event, finds no leak while a view is open: u3b_mark marks
**   the mapping records the road keeps on its heap.  a leak aborts, so
**   the sweep runs in a child.
*/
static void
_test_hand_grab(void)
{
  _pier_make("hand grab");

  c3_h mug_h, seq_h;
  _check( c3y == _blob_save((const c3_y*)"kept idle across a sweep", 24,
                            &mug_h, &seq_h),
          "save failed" );
  _han_bob = u3i_blob(mug_h, seq_h);

  pid_t pid_i = fork();
  _check( -1 != pid_i, "fork: %s", strerror(errno) );
  if ( 0 == pid_i ) {
    u3_noun gon = u3m_soft_top(0, 1 << 12, _hand_grab_cb, 0, 0);
    _exit( (c3y == _hand_unwound(gon)) ? 2 : 0 );
  }

  c3_i sat_i = 0;
  _check( pid_i == waitpid(pid_i, &sat_i, 0), "waitpid: %s", strerror(errno) );
  _check( WIFEXITED(sat_i) && (0 == WEXITSTATUS(sat_i)),
          "the sweep failed (status %d)", sat_i );

  u3z(_han_bob);
  _pier_done();
}
#endif

/* _test_sift_bob(): the %ud parser behind slaw reads a bob through a
**   view and agrees with the loom atom.
*/
static void
_test_sift_bob(void)
{
  _pier_make("sift bob");

  const c3_y dat_y[] = "1.234.567";
  c3_h mug_h, seq_h;
  _check( c3y == _blob_save(dat_y, 9, &mug_h, &seq_h), "save failed" );

  u3_atom bob = u3i_blob(mug_h, seq_h);
  u3_atom loa = u3i_bytes(9, dat_y);
  u3_weak pob = u3s_sift_ud(bob);
  u3_weak pol = u3s_sift_ud(loa);

  _check( (1234567 == pol) && (c3y == u3r_sing(pob, pol)),
          "sift_ud on a bob differs from the loom atom" );

  u3z(pob); u3z(pol); u3z(bob); u3z(loa);
  _pier_done();
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
  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  const c3_y* vue_y = vue_u.byt_y;

  c3_w met_w = u3r_met(3, _han_bob);
  c3_y byt_y = u3r_byte(3, _han_bob);

  _check(  (1 == u3b_maps_road(u3R))
        && (met_w == vue_u.byt_d) && (byt_y == vue_y[3]),
          "second open under a view" );

  //  beside the view's mapping, the fixed-width readers each map and
  //  release their own: inside the file they agree with the view, across
  //  the end they are zero-filled past the file's own length
  //
  {
    c3_y win_y[16];
    c3_y exp_y[16];
    c3_w off_w = met_w - 5;

    memset(exp_y, 0, sizeof(exp_y));
    memcpy(exp_y, vue_y + off_w, 5);

    u3r_bytes(off_w, sizeof(win_y), win_y, _han_bob);

    _check(  (1 == u3b_maps_road(u3R))
          && (0 == memcmp(win_y, exp_y, sizeof(win_y)))
          && (u3r_chub(0, _han_bob) == *(c3_d*)vue_y)
          && (0 == u3r_chub(1 + (met_w >> 3), _han_bob))
          && (0 == u3r_byte(met_w + 100000, _han_bob)),
            "readers under a mapping" );
  }

  u3r_view_done(&vue_u);
  return 0;
}

/* _test_hand_share(): on an inner road, met and byte reads beside a
**   live view agree with its mapping and leave no mapping of their own.
*/
static void
_test_hand_share(void)
{
  _hand_unwind_setup("hand share");
  u3z(u3m_soft_top(0, 1 << 12, _hand_share_cb, 0, 0));
  _hand_unwind_check();
}

static u3_noun
_hand_reuse_cb(u3_noun arg)
{
  (void)arg;

  //  a trap's thousand views each map the blob and release it at done:
  //  the road never holds more than the open view's
  //
  for ( c3_w i_w = 0; i_w < 1000; i_w++ ) {
    u3r_view vue_u;

    u3r_view_open(&vue_u, _han_bob, 0, c3y);
    _check(  vue_u.byt_y && (1 == u3b_maps_road(u3R)),
            "open %" PRIc3_w " left %zu mapping(s)", i_w, u3b_maps_road(u3R) );
    u3r_view_done(&vue_u);
    _check( !u3b_maps_road(u3R), "done %" PRIc3_w " kept its mapping", i_w );
  }
  return 0;
}

/* _test_hand_reuse(): no mapping outlives its view: each done on an
**   inner road unmaps at once.
*/
static void
_test_hand_reuse(void)
{
  _hand_unwind_setup("hand reuse");
  u3z(u3m_soft_top(0, 1 << 12, _hand_reuse_cb, 0, 0));
  _hand_unwind_check();
}

static u3_noun
_hand_deep_cb(u3_noun arg)
{
  (void)arg;
  u3r_view    par_u;
  const c3_y* par_y;

  u3r_view_open(&par_u, _han_bob, 0, c3y);
  par_y = par_u.byt_y;

  //  a grandchild maps for itself, and its done releases only its own
  //
  u3m_leap(512);
  {
    u3r_view kid_u;
    u3r_view_open(&kid_u, _han_bob, 0, c3y);
    _check(  (par_y != kid_u.byt_y) && (par_y[0] == kid_u.byt_y[0])
          && (1 == u3b_maps_road(u3R)),
            "child shared the parent's mapping" );
    u3r_view_done(&kid_u);
    _check( !u3b_maps_road(u3R), "child's done kept its mapping" );
  }
  u3m_fall();

  _check(  (1 == u3b_maps_road(u3R)) && (par_y[0] == u3r_byte(0, _han_bob)),
          "child's fall took the parent's mapping" );
  u3r_view_done(&par_u);
  return 0;
}

/* _test_hand_deep(): a nested road maps for itself beside an inner
**   ancestor's mapping, and leaves the ancestor's alone.
*/
static void
_test_hand_deep(void)
{
  _hand_unwind_setup("hand deep");
  u3z(u3m_soft_top(0, 1 << 12, _hand_deep_cb, 0, 0));
  _hand_unwind_check();
}

static u3_atom _hand_fall_bob;

static u3_noun
_hand_fall_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;

  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  (void)vue_u.byt_y;

  //  a grandchild maps a blob its parent never did, and falls with the
  //  view open
  //
  u3m_leap(512);
  {
    u3r_view kid_u;
    u3r_view_open(&kid_u, _hand_fall_bob, 0, c3y);
    (void)kid_u.byt_y;
    _check( 1 == u3b_maps_road(u3R), "child did not map its own" );
  }
  u3m_fall();

  _check(  (1 == u3b_maps_road(u3R)) && (1 == u3b_maps()),
          "fall left %zu mapping(s), the parent %zu",
          u3b_maps(), u3b_maps_road(u3R) );
  return 0;
}

/* _test_hand_fall(): a road's fall releases its own mappings, and only
**   those.
*/
static void
_test_hand_fall(void)
{
  _hand_unwind_setup("hand fall");
  _hand_write_raw(0x2345, 1, 1);
  _hand_fall_bob = u3i_blob(0x2345, 1);
  u3z(u3m_soft_top(0, 1 << 12, _hand_fall_cb, 0, 0));
  u3z(_hand_fall_bob);
  _hand_unwind_check();
}

#ifndef U3_OS_windows
static u3_noun
_hand_done_off_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;

  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  u3m_leap(512);
  u3r_view_done(&vue_u);
  _exit(0);
}

static u3_noun
_hand_read_off_cb(u3_noun arg)
{
  (void)arg;
  u3r_view vue_u;
  c3_y     byt_y;

  u3m_leap(512);
  u3r_view_open(&vue_u, _han_bob, 0, c3y);
  u3m_fall();
  u3r_view_copy(&vue_u, 0, &byt_y, 1);
  _exit(0);
}

/* _hand_off_expect(): [fun_f], run in a child process, must abort on
**   the view's road assertion.
*/
static void
_hand_off_expect(const c3_c* nam_c, u3_funk fun_f)
{
  pid_t pid_i = fork();
  _check( -1 != pid_i, "fork: %s", strerror(errno) );

  if ( 0 == pid_i ) {
    //  the assertion's own report is the expected outcome
    //
    freopen("/dev/null", "w", stderr);
    u3z(u3m_soft_top(0, 1 << 12, fun_f, 0, 0));
    _exit(0);
  }

  c3_i sat_i = 0;
  _check( pid_i == waitpid(pid_i, &sat_i, 0), "waitpid: %s", strerror(errno) );
  _check( WIFSIGNALED(sat_i) && (SIGABRT == WTERMSIG(sat_i)),
          "%s: no abort (status %d)", nam_c, sat_i );
}

/* _test_hand_road(): a view is closed on the road that opened it, and
**   read only there.
*/
static void
_test_hand_road(void)
{
  _hand_unwind_setup("hand road");
  _hand_off_expect("done on a nested road", _hand_done_off_cb);
  _hand_off_expect("read after its road fell", _hand_read_off_cb);
  u3z(_han_bob);
  _pier_done();
}
#endif

/* _test_met_cache(): the home road's first measure of a bob fills the
**   bank's bit length, and later measures read it without the file.
*/
static void
_test_met_cache(void)
{
  _pier_make("met cache");

  const c3_y dat_y[] = "measured once on the home road";
  const c3_d dat_d   = sizeof(dat_y) - 1;
  c3_h       mug_h   = 0;
  c3_h       seq_h   = 0;

  _check( c3y == _blob_save(dat_y, dat_d, &mug_h, &seq_h), "save failed" );

  u3_atom   bob   = u3i_blob(mug_h, seq_h);
  u3_atom   ref   = u3i_bytes((c3_w)dat_d, dat_y);
  u3a_blob* blb_u = u3a_blob_get(mug_h, seq_h);
  c3_d      exp_d = u3r_met_d(0, ref);

  _check( blb_u && !blb_u->bit_d, "fresh record already measured" );
  _check(  (exp_d == u3r_met_d(0, bob)) && (exp_d == blb_u->bit_d)
        && !u3b_maps(),
          "met did not fill the cache (%" PRIc3_d ")", blb_u->bit_d );

  //  with the file gone, the cache still answers
  //
  {
    c3_c pax_c[8192];
    u3b_path(pax_c, _tmp_bob, mug_h, seq_h);
    _check( 0 == unlink(pax_c), "unlink failed" );
  }
  _check(  (exp_d == u3r_met_d(0, bob))
        && ((exp_d + 7) >> 3 == u3r_met_d(3, bob)),
          "met opened the file" );

  u3z(bob); u3z(ref);
  _pier_done();
}

/* _test_hand_empty(): a zero-byte blob file opens as nothing.
*/
static void
_test_hand_empty(void)
{
  _pier_make("hand empty");

  const c3_h mug_h = 0x5678;
  c3_c pax_c[8192];

  snprintf(pax_c, sizeof(pax_c), "%s/" U3B_MUG_FMT, _tmp_bob, mug_h);
  mkdir(pax_c, 0700);
  u3b_path(pax_c, _tmp_bob, mug_h, 1);
  fclose(fopen(pax_c, "wb"));

  u3_atom  bob = u3i_blob(mug_h, 1);
  u3r_view win_u;
  u3b_flat fla_u;
  c3_d     bit_d;
  c3_m     mot_m = 0;

  _check(  (c3n == u3b_map(&fla_u, _tmp_bob, u3a_bid(mug_h, 1), 0,
                           &bit_d, &mot_m))
        && (c3__file == mot_m)
        && (c3n == u3r_view_open(&win_u, bob, 0, c3n))
        && !u3b_maps(),
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

  //  flat views: a bob's bytes are a mapping; a loom atom is viewed in
  //  place; a direct atom's value is kept in the view
  //
  {
    u3r_view vue_u;

    u3r_view_open(&vue_u, bob, 0, c3y);
    const c3_y* vue_y = vue_u.byt_y;
    _check(  vue_u.fla_u && (len_w == vue_u.byt_d)
          && (vue_y == vue_u.byt_y) && (0 == memcmp(vue_y, dat_y, len_w))
          && (1 == u3b_maps()),
            "flat bob" );
    u3r_view_done(&vue_u);
    _check( 0 == u3b_maps(), "flat bob done" );

    {
      u3_atom sma = u3i_string("small loom atom, wider than a word");
      c3_w    met_w = u3r_met(3, sma);

      u3r_view_open(&vue_u, sma, 0, c3y);
      vue_y = vue_u.byt_y;
      _check( !vue_u.fla_u && (met_w == vue_u.byt_d), "flat loom" );
      {
        u3_atom cop = u3i_bytes(met_w, vue_y);
        _check( c3y == u3r_sing(sma, cop), "flat loom bytes" );
        u3z(cop);
      }
      u3r_view_done(&vue_u);

      u3r_view_open(&vue_u, 0x44332211, 0, c3y);
      vue_y = vue_u.byt_y;
      _check(  !vue_u.fla_u && (4 == vue_u.byt_d)
            && (vue_y == (const c3_y*)&vue_u.raw_d)
            && (0x11 == vue_y[0]) && (0x44 == vue_y[3]),
              "init cat" );
      u3r_view_done(&vue_u);

      //  windows: a bob's view copies from its mapping, zero past the
      //  end; a loom atom and a direct atom read the same way from their
      //  own bytes.  met agrees with the atom's at every bloq
      //
      {
        u3r_view win_u;
        c3_y     win_y[64];
        c3_y     exp_y[64];

        _check( c3y == u3r_view_open(&win_u, bob, 0, c3n), "open bob" );
        _check(  win_u.fla_u && win_u.byt_y
              && (len_w == win_u.byt_d)
              && (u3r_met(0, loa) == win_u.bit_d)
              && (u3r_met(5, loa) == ((win_u.bit_d + 31) >> 5))
              && (1 == u3b_maps()),
                "open bob shape" );

        memset(exp_y, 0, sizeof(exp_y));
        memcpy(exp_y, dat_y + len_w - 40, 40);
        _check(  (40 == u3r_view_copy(&win_u, len_w - 40, win_y, sizeof(win_y)))
              && (0 == memcmp(win_y, exp_y, sizeof(win_y))),
                "read bob tail" );
        _check(  (0 == u3r_view_copy(&win_u, (c3_d)len_w << 20, win_y, 8))
              && (0 == memcmp(win_y, exp_y + 40, 8)),
                "read bob past" );
        u3r_view_done(&win_u);
        _check( 0 == u3b_maps(), "open bob done" );

        _check( c3y == u3r_view_open(&win_u, sma, 0, c3n), "open loom" );
        _check(  !win_u.fla_u && (met_w == win_u.byt_d)
              && (3 == u3r_view_copy(&win_u, met_w - 3, win_y, 16))
              && (0 == memcmp(win_y, win_u.byt_y + met_w - 3, 3))
              && (0 == memcmp(win_y + 3, exp_y + 40, 13)),
                "read loom" );
        u3r_view_done(&win_u);

        _check( c3y == u3r_view_open(&win_u, 0x44332211, 0, c3n), "open cat" );
        _check(  (win_u.byt_y == (const c3_y*)&win_u.raw_d)
              && (31 == win_u.bit_d)
              && (2 == u3r_view_copy(&win_u, 2, win_y, 4))
              && (0x33 == win_y[0]) && (0x44 == win_y[1])
              && (0 == win_y[2]) && (0 == win_y[3]),
                "read cat" );
        u3r_view_done(&win_u);
      }

      u3z(sma);
    }
  }

  u3z(bob); u3z(loa);
  c3_free(dat_y);
  _pier_done();
}

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
  u3b_wipe(_tmp_bob, mug1_h, seq1_h);
  u3b_wipe(_tmp_bob, mug2_h, seq2_h);
  _check(  (c3n == u3b_live(_tmp_bob, mug1_h, seq1_h))
        && (c3n == u3b_live(_tmp_bob, mug2_h, seq2_h)),
          "blobs still present after delete" );

  _pier_done();
}

/* _log_open(): open an LMDB env in the pier's log directory.
*/
static MDB_env*
_log_open(void)
{
  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);

  MDB_env* env_u = u3_lmdb_init(log_c, 1ULL << 30);
  _check( env_u, "lmdb init failed" );
  return env_u;
}

/* _log_env(): a fresh pier with an LMDB env for a log test.
*/
static MDB_env*
_log_env(const c3_c* nam_c)
{
  _pier_make(nam_c);

  return _log_open();
}

/* _log_reopen(): close and reopen the env — stands in for a crash +
**   restart, exercising on-disk durability.
*/
static MDB_env*
_log_reopen(MDB_env* env_u)
{
  u3_lmdb_exit(env_u);
  return _log_open();
}

/* _test_blobs_persist(): an event and its BLOBS row, written in one
**   transaction, survive a close + reopen together.
*/
static void
_test_blobs_persist(void)
{
  MDB_env* env_u = _log_env("blobs persist");

  c3_d bid_a = ((c3_d)0x0a11ce << 32) | 7;
  c3_d bid_b = ((c3_d)0x000b0b << 32) | 9;

  {
    c3_d   ids_d[2] = { bid_a, bid_b };
    c3_y   byt_y[4] = { 1, 2, 3, 4 };
    void*  byt_p[1] = { byt_y };
    size_t siz_i[1] = { sizeof(byt_y) };
    c3_d*  bid_p[1] = { ids_d };
    c3_z   bid_z[1] = { 2 };
    _check( c3y == u3_lmdb_save(env_u, 42, 1, byt_p, siz_i, bid_p, bid_z),
            "event + blobs save failed" );
  }

  //  "crash" and restart
  //
  env_u = _log_reopen(env_u);

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
    u3b_path(fil_c, _tmp_bob, bar_mug_h, bar_seq_h);
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

/* _epoc_env(): an LMDB env for epoch [epo_d] under the pier's log dir.
*/
static MDB_env*
_epoc_env(c3_d epo_d)
{
  c3_c epo_c[2048];
  snprintf(epo_c, sizeof(epo_c), "%s/.urb/log/0i%" PRIu64, _tmp_pier, epo_d);
  _check( (0 == mkdir(epo_c, 0700)) || (EEXIST == errno),
          "mkdir %s: %s", epo_c, strerror(errno) );

  MDB_env* env_u = u3_lmdb_init(epo_c, 1ULL << 30);
  _check( env_u, "lmdb init %s failed", epo_c );
  return env_u;
}

/* _epoc_event(): write event [eve_d] into [env_u] with a BLOBS row
**   naming [len_z] blob ids.
*/
static void
_epoc_event(MDB_env* env_u, c3_d eve_d, c3_d* ids_d, c3_z len_z)
{
  c3_y   byt_y[4] = { 1, 2, 3, 4 };
  void*  byt_p[1] = { byt_y };
  size_t siz_i[1] = { sizeof(byt_y) };
  c3_d*  bid_p[1] = { ids_d };
  c3_z   bid_z[1] = { len_z };
  _check( c3y == u3_lmdb_save(env_u, eve_d, 1, byt_p, siz_i, bid_p, bid_z),
          "event %" PRIu64 " save failed", eve_d );
}

/* _test_live(): u3a_blob_live is the distinct ids reachable from the
**   home road's roots, ascending: two atoms over one id under the state,
**   shared structure walked once, one atom under a memo cache; a stub
**   and an atom no root reaches are absent.
*/
static void
_test_live(void)
{
  _pier_make("live");

  const c3_c* dat_c[] = { "under the state, twice",
                          "under a memo cache",
                          "reachable from no root" };
  c3_h mug_h[3], seq_h[3];
  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    _check( c3y == _blob_save((const c3_y*)dat_c[i_w], strlen(dat_c[i_w]),
                              &mug_h[i_w], &seq_h[i_w]),
            "save %" PRIc3_w " failed", i_w );
  }

  u3_noun par = u3nc(u3i_blob(mug_h[0], seq_h[0]), u3i_blob(mug_h[0], seq_h[0]));
  u3_noun old = u3A->roc;
  u3A->roc    = u3nt(par, u3k(par), u3i_blob(mug_h[1], 0));

  u3_noun key = u3i_string("live");
  u3h_put(u3R->cax.per_p, key, u3i_blob(mug_h[1], seq_h[1]));

  u3_noun los = u3i_blob(mug_h[2], seq_h[2]);

  c3_z  len_z = 0;
  c3_d* bid_d = u3a_blob_live(&len_z);
  c3_d  a_d   = ((c3_d)mug_h[0] << 32) | seq_h[0];
  c3_d  b_d   = ((c3_d)mug_h[1] << 32) | seq_h[1];

  _check( 2 == len_z, "live set has %" PRIc3_z " ids, want 2", len_z );
  _check( bid_d && (bid_d[0] == c3_min(a_d, b_d)) && (bid_d[1] == c3_max(a_d, b_d)),
          "live set not the two reachable ids, ascending" );
  c3_free(bid_d);

  u3z(los);
  u3h_del(u3R->cax.per_p, key);
  u3z(key);
  u3z(u3A->roc);
  u3A->roc = old;
  _pier_done();
}

/* _test_roll_link(): roll carries every blob the bank holds into the
**   new epoch's store and leaves the rest behind: an atom under the
**   state and a held record are linked (one inode, two names), a
**   record nothing holds is not; the state's id alone is owned and is
**   the new epoch's first BLOBS row; the old files stay for the old
**   epoch; the new store dedups against what it was handed; the
**   process now installs there.
*/
static void
_test_roll_link(void)
{
  _pier_make("roll link");

  //  roll copies the checkpoint image; an empty one is a valid image
  //
  {
    c3_c chk_c[2048];
    snprintf(chk_c, sizeof(chk_c), "%s/.urb/chk", _tmp_pier);
    _check( 0 == mkdir(chk_c, 0700), "mkdir %s: %s", chk_c, strerror(errno) );
    snprintf(chk_c, sizeof(chk_c), "%s/.urb/chk/image.bin", _tmp_pier);
    FILE* fil_f = fopen(chk_c, "wb");
    _check( fil_f, "image: %s", strerror(errno) );
    fclose(fil_f);
  }

  //  a: held by a live atom.  b: held by nothing.  c: held by the king.
  //
  const c3_c* dat_c[] = { "carried by a live atom",
                          "left behind, held by nothing",
                          "carried by a hold the king has" };
  c3_h mug_h[3], seq_h[3];
  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    _check( c3y == _blob_save((const c3_y*)dat_c[i_w], strlen(dat_c[i_w]),
                              &mug_h[i_w], &seq_h[i_w]),
            "save %" PRIc3_w " failed", i_w );
  }
  u3_atom   bob_a = u3i_blob(mug_h[0], seq_h[0]);
  u3a_blob* blb_b = u3a_blob_new(mug_h[1], seq_h[1]);
  u3a_blob* blb_c = u3a_blob_new(mug_h[2], seq_h[2]);
  blb_b->own_o  = c3y;   //  a ghost: the old epoch's input, nothing holds it
  blb_c->hol_h += 1;

  //  d: held, no root reaches it, and its file is gone.  the new epoch
  //  can do without it, so the roll reports it and goes on
  //
  u3a_blob* blb_d = u3a_blob_new(0x5eed, 1);
  blb_d->hol_h += 1;

  //  the image holds a: the roll owns what a root reaches
  //
  u3_noun old = u3A->roc;
  u3A->roc    = u3nc(bob_a, u3k(old));

  //  epoch 0 with two events and its meta, as roll requires
  //
  MDB_env* env_u = _epoc_env(0);
  {
    u3_meta met_u = { .ver_h = 1, .who_d = { 0, 0 }, .fak_o = c3y, .lif_h = 1 };
    c3_d    non_d = 0;
    _epoc_event(env_u, 1, &non_d, 0);
    _epoc_event(env_u, 2, &non_d, 0);
    _check( c3y == u3_disk_save_meta(env_u, &met_u), "meta save failed" );
  }

  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);
  u3_dire dir_u = { .pax_c = _tmp_pier };
  u3_dire com_u = { .pax_c = log_c };
  u3_disk log_u = { .dir_u = &dir_u, .com_u = &com_u,
                    .mdb_u = env_u, .epo_d = 0, .dun_d = 2 };

  u3_disk_roll(&log_u, 2);
  _check( 2 == log_u.epo_d, "epoch not rolled" );

  //  the epoch has its image, and no patch once the roll stands
  //
  {
    c3_c fil_c[2048];
    struct stat buf_u;
    snprintf(fil_c, sizeof(fil_c), "%s/0i2/image.bin", log_c);
    _check( 0 == stat(fil_c, &buf_u), "no epoch image" );
    snprintf(fil_c, sizeof(fil_c), "%s/0i2/image.tmp", log_c);
    _check( 0 != stat(fil_c, &buf_u), "epoch image left under its temporary name" );
    snprintf(fil_c, sizeof(fil_c), "%s/0i2/control.bin", log_c);
    _check( 0 != stat(fil_c, &buf_u), "roll left its patch" );
  }

  c3_c new_c[8192];
  u3b_root(new_c, _tmp_pier, 2);
  _check( u3C.bob_c && (0 == strcmp(u3C.bob_c, new_c)),
          "store root not moved: %s", u3C.bob_c ? u3C.bob_c : "(null)" );

  //  the bank is the new epoch's: what an atom holds is owned, what
  //  only a hold keeps is not, and the ghost is gone
  //
  _check( c3y == u3a_blob_get(mug_h[0], seq_h[0])->own_o, "atom-held not owned" );
  _check( c3n == u3a_blob_get(mug_h[2], seq_h[2])->own_o, "hold-only owned" );
  _check( !u3a_blob_get(mug_h[1], seq_h[1]), "ghost survived the roll" );

  //  the live set is the new epoch's first BLOBS row, at the event the
  //  image is the state of: a alone, not the hold-only c, not the ghost
  //
  {
    c3_d* row_d = 0;
    c3_z  row_z = 0;
    _check( c3y == u3_lmdb_read_blobs(log_u.mdb_u, 2, &row_d, &row_z),
            "no image row at the epoch's first key" );
    _check( (1 == row_z) && row_d
            && (row_d[0] == (((c3_d)mug_h[0] << 32) | seq_h[0])),
            "image row is not the one state-held id" );
    c3_free(row_d);
  }

  //  a and c carried as links, b left behind; the old store untouched
  //
  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    c3_c old_f[8192], new_f[8192];
    struct stat old_u, new_u;
    u3b_path(old_f, _tmp_bob, mug_h[i_w], seq_h[i_w]);
    u3b_path(new_f, new_c, mug_h[i_w], seq_h[i_w]);
    _check( 0 == stat(old_f, &old_u), "old %" PRIc3_w " gone", i_w );

    if ( 1 == i_w ) {
      _check( 0 != stat(new_f, &new_u), "unheld blob carried" );
    }
    else {
      _check( (0 == stat(new_f, &new_u)) && (old_u.st_ino == new_u.st_ino),
              "blob %" PRIc3_w " not linked", i_w );
    }
  }

  //  the new store knows what it was handed: the same content dedups
  //  to the carried seq instead of taking a new one
  //
  {
    c3_c stg_c[8192];
    c3_h mag_h = 0, saq_h = 0;
    _check( c3y == u3b_stage(_tmp_pier, (const c3_y*)dat_c[0],
                             strlen(dat_c[0]), stg_c, &_stg_mug),
            "restage failed" );
    _check(  (c3y == u3b_move_stg(new_c, stg_c, &mag_h, &saq_h))
          && (mag_h == mug_h[0]) && (saq_h == seq_h[0]),
            "carried blob not deduped: got seq %" PRIc3_h, saq_h );
  }

  u3_lmdb_exit(log_u.mdb_u);
  u3C.bob_c = _tmp_bob;
  u3a_blob_get(mug_h[0], seq_h[0])->own_o = c3n;
  u3z(u3A->roc);
  u3A->roc = old;
  blb_c->hol_h = 0;
  u3a_blob_drop(mug_h[2], seq_h[2]);
  _check( u3a_blob_get(0x5eed, 1), "the fileless hold-only record went" );
  u3a_blob_get(0x5eed, 1)->hol_h = 0;
  u3a_blob_drop(0x5eed, 1);
  _pier_done();
}

/* _test_link_copy(): when the link is refused and the copy cannot
**   read its source, the link fails and leaves no file at the target:
**   a directory where the blob should be is refused by link(2) and
**   fails read(2) with EISDIR.
*/
static void
_test_link_copy(void)
{
  _pier_make("link copy");

  c3_c new_c[8192], src_c[8192], dst_c[8192];
  {
    c3_c epo_c[2048];
    snprintf(epo_c, sizeof(epo_c), "%s/.urb/log/0i5", _tmp_pier);
    _check( 0 == mkdir(epo_c, 0700), "mkdir %s: %s", epo_c, strerror(errno) );
  }
  u3b_root(new_c, _tmp_pier, 5);
  _check( 0 == mkdir(new_c, 0700), "mkdir %s: %s", new_c, strerror(errno) );

  u3b_path(src_c, _tmp_bob, 0x1234, 1);
  {
    c3_c dir_c[8192];
    snprintf(dir_c, sizeof(dir_c), "%s/" U3B_MUG_FMT, _tmp_bob, (c3_h)0x1234);
    _check( 0 == mkdir(dir_c, 0700), "mkdir %s: %s", dir_c, strerror(errno) );
  }
  _check( 0 == mkdir(src_c, 0700), "mkdir %s: %s", src_c, strerror(errno) );

  _check( c3n == u3b_link(_tmp_bob, new_c, 0x1234, 1),
          "a failed copy read as a link" );

  struct stat sat_u;
  u3b_path(dst_c, new_c, 0x1234, 1);
  _check( 0 != stat(dst_c, &sat_u), "a failed copy left %s", dst_c );

  rmdir(src_c);
  _pier_done();
}

#ifndef U3_OS_windows
/* _test_roll_same(): a roll into the current epoch (an empty epoch
**   meeting a new binary) that fails late removes nothing: the store,
**   its blob, and the epoch's markers are the live epoch's.  the roll
**   exits on failure, so it runs in a child.
*/
static void
_test_roll_same(void)
{
  _pier_make("roll same");

  //  the current epoch is 2: its store holds a blob, its marker is
  //  written.  no checkpoint, so the roll fails at the image copy
  //
  c3_c cur_c[8192], epv_c[2048];
  u3b_root(cur_c, _tmp_pier, 2);
  snprintf(epv_c, sizeof(epv_c), "%s/.urb/log/0i2/epoc.txt", _tmp_pier);

  MDB_env* env_u = _epoc_env(0);
  {
    c3_c epo_c[2048];
    snprintf(epo_c, sizeof(epo_c), "%s/.urb/log/0i2", _tmp_pier);
    _check( 0 == mkdir(epo_c, 0700), "mkdir %s: %s", epo_c, strerror(errno) );
    _check( 0 == mkdir(cur_c, 0700), "mkdir %s: %s", cur_c, strerror(errno) );

    FILE* fil_f = fopen(epv_c, "w");
    _check( fil_f, "epoc.txt: %s", strerror(errno) );
    fprintf(fil_f, "3");
    fclose(fil_f);

    c3_d non_d = 0;
    _epoc_event(env_u, 1, &non_d, 0);
    _epoc_event(env_u, 2, &non_d, 0);
  }

  u3C.bob_c = cur_c;
  c3_h mug_h, seq_h;
  {
    c3_c stg_c[8192];
    _check( c3y == u3b_stage(_tmp_pier, (const c3_y*)"owned by the live epoch",
                             23, stg_c, &_stg_mug),
            "stage failed" );
    _check( c3y == u3b_move_stg(cur_c, stg_c, &mug_h, &seq_h),
            "install failed" );
  }
  u3_atom bob = u3i_blob(mug_h, seq_h);

  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);
  u3_dire dir_u = { .pax_c = _tmp_pier };
  u3_dire com_u = { .pax_c = log_c };
  u3_disk log_u = { .dir_u = &dir_u, .com_u = &com_u,
                    .mdb_u = env_u, .epo_d = 2, .dun_d = 2 };

  pid_t pid_i = fork();
  _check( -1 != pid_i, "fork: %s", strerror(errno) );
  if ( 0 == pid_i ) {
    u3_disk_roll(&log_u, 2);
    _exit(0);
  }

  c3_i sat_i = 0;
  _check( pid_i == waitpid(pid_i, &sat_i, 0), "waitpid: %s", strerror(errno) );
  _check( WIFEXITED(sat_i) && (1 == WEXITSTATUS(sat_i)),
          "the roll did not fail (status %d)", sat_i );

  {
    c3_c fil_c[8192];
    struct stat sat_u;
    u3b_path(fil_c, cur_c, mug_h, seq_h);
    _check( 0 == stat(fil_c, &sat_u), "the live epoch's blob was deleted" );
    _check( 0 == stat(epv_c, &sat_u), "the live epoch's marker was deleted" );
  }

  u3z(bob);
  u3_lmdb_exit(env_u);
  u3C.bob_c = _tmp_bob;
  _pier_done();
}
#endif

/* _test_grants(): the encoder and the decoder of its output collect
**   the same distinct blob ids, one per id however many atoms or
**   repeats name it; a hold keeps the record past its last atom, the
**   hook fires then, and returning the holds frees it.
*/
static void
_test_grants(void)
{
  _pier_make("grants");

  c3_h mug_h[2], seq_h[2];
  _check( c3y == _blob_save((const c3_y*)"named by two atoms and a repeat", 31,
                            &mug_h[0], &seq_h[0]),
          "save 0 failed" );
  _check( c3y == _blob_save((const c3_y*)"named once", 10, &mug_h[1], &seq_h[1]),
          "save 1 failed" );

  //  x and y are two atoms over one id; x appears twice
  //
  u3_atom x = u3i_blob(mug_h[0], seq_h[0]);
  u3_atom y = u3i_blob(mug_h[0], seq_h[0]);
  u3_atom z = u3i_blob(mug_h[1], seq_h[1]);
  u3_noun non = u3nq(x, y, u3k(x), z);

  c3_d     len_d;
  c3_y*    byt_y;
  u3s_bids out_u = {0};
  u3s_bids inn_u = {0};

  u3s_ram_xeno_bids(non, &len_d, &byt_y, &out_u);
  u3_weak tap = u3s_tap_xeno_bids(len_d, byt_y, &inn_u);
  free(byt_y);

  _check( u3_none != tap, "tap failed" );
  _check( (2 == out_u.len_w) && (2 == inn_u.len_w)
          && (out_u.bid_d[0] == inn_u.bid_d[0])
          && (out_u.bid_d[1] == inn_u.bid_d[1])
          && (out_u.bid_d[0] < out_u.bid_d[1]),
          "sets differ: %u out, %u in", out_u.len_w, inn_u.len_w );
  c3_free(out_u.bid_d);

  //  the receiving side counts one hold per id; the tapped noun's
  //  atoms are alive, so the hook cannot have fired
  //
  u3a_blob_hold(inn_u.bid_d, inn_u.len_w);
  c3_free(inn_u.bid_d);
  u3z(tap);

  {
    u3a_blob* blb_u = u3a_blob_get(mug_h[0], seq_h[0]);
    _check( (1 == blb_u->hol_h) && (2 == blb_u->atm_h),
            "id 0: les=%u atm=%u", blb_u->hol_h, blb_u->atm_h );
  }

  //  the last atoms go: the hook fires once per id, and the holds keep
  //  the records
  //
  _hok_w = 0;
  u3z(non);
  _check( 2 == _hok_w, "hook fired %u time(s) for two ids", _hok_w );
  {
    u3a_blob* blb_u = u3a_blob_get(mug_h[0], seq_h[0]);
    _check( blb_u && (1 == blb_u->hol_h) && (0 == blb_u->atm_h) && (0 == _del_w),
            "held record not kept" );
  }

  //  the holds come back: the %blrl arithmetic, clamped
  //
  for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
    u3a_blob* blb_u = u3a_blob_get(mug_h[i_w], seq_h[i_w]);
    c3_h      num_h = c3_min(5, blb_u->hol_h);
    blb_u->hol_h -= num_h;
    _check( 0 == blb_u->hol_h, "id %u not released", i_w );
    _blob_del_cb(mug_h[i_w], seq_h[i_w]);
  }
  _check( (2 == _del_w) && !u3a_blob_get(mug_h[0], seq_h[0]),
          "released records not deleted" );

  _pier_done();
}

/* _test_stub_fill(): a stub touches no bank; u3_mars_blob_fill puts
**   the installed bob in every stub's place, by mug, sharing none and
**   losing nothing, and refuses a stub with no installed match.
*/
static void
_test_stub_fill(void)
{
  _pier_make("stub fill");

  c3_h mug_h[2], seq_h[2];
  _check( c3y == _blob_save((const c3_y*)"installed for the first stub", 28,
                            &mug_h[0], &seq_h[0]),
          "save 0 failed" );
  _check( c3y == _blob_save((const c3_y*)"installed for the second stub", 29,
                            &mug_h[1], &seq_h[1]),
          "save 1 failed" );

  u3_atom stb_a = u3i_blob(mug_h[0], 0);
  u3_atom stb_b = u3i_blob(mug_h[1], 0);
  _check( !u3a_blob_get(mug_h[0], 0) && !u3a_blob_get(mug_h[1], 0),
          "a stub made a record" );
  _check( (mug_h[0] == u3r_mug(stb_a)) && (0 == u3a_bob_seq(stb_a)),
          "stub mug or seq wrong" );

  //  a stub equals no loom atom, even one with its content, and the
  //  compare opens nothing: ram meets this whenever a stub and an
  //  indirect atom share a backref slot
  //
  {
    const c3_c* dat_c = "installed for the first stub";
    u3_atom     loa   = u3i_bytes(28, (const c3_y*)dat_c);
    u3_atom     cor   = u3i_string("an indirect cord beside the stub");
    _check( c3n == u3r_sing(stb_a, loa), "stub equal to its content" );
    _check( c3n == u3r_sing(loa, stb_a), "content equal to its stub" );
    _check( c3n == u3r_sing(stb_a, cor), "stub equal to a cord" );
    _check( 0 == u3b_maps(), "a stub compare mapped a blob" );
    u3z(loa);
    u3z(cor);
  }

  //  [now [[tar wir] [%into stub-a 42 stub-b stub-a]]]: two stubs of
  //  one mug and one of another, in a list spine
  //
  u3_noun job = u3nc(0xbeef,
                     u3nc(u3nc(c3__c, u3nc(c3__sync, u3_nul)),
                          u3nq(c3__into, stb_a, 42, u3nc(stb_b, u3nc(u3k(stb_a), u3_nul)))));

  c3_d bid_d[2] = { ((c3_d)mug_h[0] << 32) | seq_h[0],
                    ((c3_d)mug_h[1] << 32) | seq_h[1] };

  u3_weak new = u3_mars_blob_fill(job, bid_d, 2);
  _check( u3_none != new, "fill refused" );

  {
    u3_noun cad = u3t(u3t(new));
    u3_noun one = u3h(u3t(cad));
    u3_noun two = u3h(u3t(u3t(u3t(cad))));
    u3_noun tre = u3h(u3t(u3t(u3t(u3t(cad)))));

    _check( (c3__into == u3h(cad)) && (42 == u3h(u3t(u3t(cad)))),
            "shape changed" );
    _check( (c3y == u3a_is_bob(one)) && (seq_h[0] == u3a_bob_seq(one))
            && (mug_h[0] == u3a_bob_mug(one)),
            "first stub not filled" );
    _check( (c3y == u3a_is_bob(two)) && (seq_h[1] == u3a_bob_seq(two)),
            "second stub not filled" );
    _check( (c3y == u3a_is_bob(tre)) && (seq_h[0] == u3a_bob_seq(tre)),
            "repeated stub not filled" );
    _check( u3_nul == u3t(u3t(u3t(u3t(u3t(cad))))), "list end lost" );
    _check( 0xbeef == u3h(new), "head lost" );
  }

  //  three bobs over two records
  //
  _check( (2 == u3a_blob_get(mug_h[0], seq_h[0])->atm_h)
          && (1 == u3a_blob_get(mug_h[1], seq_h[1])->atm_h),
          "fill miscounted" );

  //  a stub with no installed match refuses the whole fill
  //
  _check( u3_none == u3_mars_blob_fill(job, bid_d, 1), "unmatched stub filled" );

  //  the king rams the event with its stubs in it: a stub is written
  //  as its id without being measured (it has no file to measure),
  //  comes back a stub, makes no record, and grants nothing
  //
  {
    c3_d     len_d;
    c3_y*    byt_y;
    u3s_bids out_u = {0};
    u3s_bids inn_u = {0};

    u3s_ram_xeno_bids(job, &len_d, &byt_y, &out_u);
    u3_weak tap = u3s_tap_xeno_bids(len_d, byt_y, &inn_u);
    free(byt_y);

    _check( u3_none != tap, "stubbed event did not tap" );
    _check( (0 == out_u.len_w) && (0 == inn_u.len_w), "a stub granted a hold" );
    _check( (3 == out_u.nub_w) && (3 == inn_u.nub_w),
            "stubs met %" PRIc3_w " and %" PRIc3_w ", not 3",
            out_u.nub_w, inn_u.nub_w );
    {
      u3_noun cad = u3t(u3t(tap));
      u3_noun one = u3h(u3t(cad));
      _check( (c3y == u3a_is_bob(one)) && (0 == u3a_bob_seq(one))
              && (mug_h[0] == u3a_bob_mug(one)),
              "stub did not survive ram and tap" );
    }
    _check( !u3a_blob_get(mug_h[0], 0), "a tapped stub made a record" );
    u3z(tap);
  }

  u3z(new);
  u3z(job);
  _pier_done();
}

/* _test_chop_kill(): chop deletes an epoch's store with the epoch,
**   dotfiles included, removes a link without following it, and
**   leaves the retained epochs, their stores, and the bank alone.
*/
static void
_test_chop_kill(void)
{
  _pier_make("chop kill");

  c3_c log_c[2048];
  snprintf(log_c, sizeof(log_c), "%s/.urb/log", _tmp_pier);

  //  epochs 0 to 3; a blob in epoch 0's store, an empty store in epoch
  //  1's, and a blob in epoch 3's held by an atom.  the process runs
  //  in epoch 3; chop deletes epochs 0 and 1.
  //
  c3_h mug_h = 0, seq_h = 0;
  _check( c3y == _blob_save((const c3_y*)"dies with epoch 0", 17, &mug_h, &seq_h),
          "save failed" );

  MDB_env* env_u;
  c3_c     emp_c[8192];
  env_u = _epoc_env(0); u3_lmdb_exit(env_u);
  env_u = _epoc_env(1); u3_lmdb_exit(env_u);
  u3b_root(emp_c, _tmp_pier, 1);
  _check( 0 == mkdir(emp_c, 0700), "mkdir %s: %s", emp_c, strerror(errno) );
  env_u = _epoc_env(2); u3_lmdb_exit(env_u);
  env_u = _epoc_env(3);

  c3_c new_c[8192];
  u3b_root(new_c, _tmp_pier, 3);
  _check( 0 == mkdir(new_c, 0700), "mkdir %s: %s", new_c, strerror(errno) );
  u3C.bob_c = new_c;

  c3_c stg_c[8192];
  c3_h mag_h = 0, saq_h = 0;
  _check( c3y == u3b_stage(_tmp_pier, (const c3_y*)"lives on in epoch 2", 19, stg_c, &_stg_mug)
          && (c3y == u3b_move_stg(new_c, stg_c, &mag_h, &saq_h)),
          "save in epoch 2 failed" );
  u3_atom bob = u3i_blob(mag_h, saq_h);

  u3_dire dir_u = { .pax_c = _tmp_pier };
  u3_dire com_u = { .pax_c = log_c };
  u3_disk log_u = { .dir_u = &dir_u, .com_u = &com_u,
                    .mdb_u = env_u, .epo_d = 3 };

  //  dotfiles in epoch 0's root and bucket must not block the rmdirs
  //
  {
    c3_c dot_c[8192];
    snprintf(dot_c, sizeof(dot_c), "%s/.DS_Store", _tmp_bob);
    _check( c3y == _file_put(dot_c, "x", 1), "plant %s", dot_c );
    snprintf(dot_c, sizeof(dot_c), "%s/" U3B_MUG_FMT "/.DS_Store", _tmp_bob, mug_h);
    _check( c3y == _file_put(dot_c, "x", 1), "plant %s", dot_c );
  }

#ifndef U3_OS_windows
  //  a link in epoch 1's store is removed, not followed
  //
  c3_c out_c[8192], kep_c[8192];
  {
    c3_c lin_c[8192];
    snprintf(out_c, sizeof(out_c), "%s/outside", _tmp_pier);
    _check( 0 == mkdir(out_c, 0700), "mkdir %s: %s", out_c, strerror(errno) );
    snprintf(kep_c, sizeof(kep_c), "%s/1", out_c);
    _check( c3y == _file_put(kep_c, "keep", 4), "plant %s", kep_c );
    snprintf(lin_c, sizeof(lin_c), "%s/7", emp_c);
    _check( 0 == symlink(out_c, lin_c), "symlink: %s", strerror(errno) );
  }
#endif

  u3_disk_chop(&log_u, 0);

#ifndef U3_OS_windows
  _check( c3y == _path_exists(kep_c), "kill followed a link out of the store" );
#endif

  {
    c3_c epo_c[2048];
    snprintf(epo_c, sizeof(epo_c), "%s/0i0", log_c);
    _check( c3n == _path_exists(epo_c), "epoch 0 not deleted" );
    _check( c3n == _path_exists(_tmp_bob), "epoch 0's store not deleted" );
    snprintf(epo_c, sizeof(epo_c), "%s/0i1", log_c);
    _check( c3n == _path_exists(epo_c), "epoch 1, with an empty store, not deleted" );
    snprintf(epo_c, sizeof(epo_c), "%s/0i2", log_c);
    _check( c3y == _path_exists(epo_c), "epoch 2 deleted" );
  }
  _check( c3y == u3b_live(new_c, mag_h, saq_h), "epoch 3's blob gone" );
  _check( 1 == u3a_blob_get(mag_h, saq_h)->atm_h, "bank disturbed" );

  u3_lmdb_exit(env_u);
  u3C.bob_c = _tmp_bob;
  u3z(bob);
  _pier_done();
}

/* _test_gc_own(): the boot gc wipes what nothing holds and no epoch
**   owns, keeps an owned ghost (record and file), and wipes a file the
**   bank never heard of.
*/
static void
_test_gc_own(void)
{
  _pier_make("gc own");

  const c3_c* dat_c[] = { "unowned and unheld: garbage",
                          "owned by the epoch, held by nothing: a ghost",
                          "no record at all: an orphan" };
  c3_h mug_h[3], seq_h[3];
  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    _check( c3y == _blob_save((const c3_y*)dat_c[i_w], strlen(dat_c[i_w]),
                              &mug_h[i_w], &seq_h[i_w]),
            "save %" PRIc3_w " failed", i_w );
  }
  u3a_blob_new(mug_h[0], seq_h[0]);
  u3a_blob_new(mug_h[1], seq_h[1])->own_o = c3y;

  u3_disk log_u = {0};
  u3_disk_blob_gc();

  _check( !u3a_blob_get(mug_h[0], seq_h[0])
          && (c3n == u3b_live(_tmp_bob, mug_h[0], seq_h[0])),
          "garbage survived" );
  _check( u3a_blob_get(mug_h[1], seq_h[1])
          && (c3y == u3b_live(_tmp_bob, mug_h[1], seq_h[1])),
          "ghost wiped" );
  _check( c3n == u3b_live(_tmp_bob, mug_h[2], seq_h[2]), "orphan survived" );

  u3a_blob_drop(mug_h[1], seq_h[1]);
  _pier_done();
}

/* _test_blob_own(): u3_disk_blob_own marks what the epoch's rows name,
**   the image's row at the epoch's first key and an event's row alike,
**   making a ghost for a named blob the bank lost; an atom no row
**   names is not owned, nor is a hold; a named blob whose file is gone
**   fails the mark.
*/
static void
_test_blob_own(void)
{
  _pier_make("blob own");

  const c3_c* dat_c[] = { "in the image's row, held by an atom",
                          "named by an event, no record",
                          "held by the king only",
                          "held by an atom, named by no row" };
  c3_h mug_h[4], seq_h[4];
  for ( c3_w i_w = 0; i_w < 4; i_w++ ) {
    _check( c3y == _blob_save((const c3_y*)dat_c[i_w], strlen(dat_c[i_w]),
                              &mug_h[i_w], &seq_h[i_w]),
            "save %" PRIc3_w " failed", i_w );
  }
  u3_atom   bob_a = u3i_blob(mug_h[0], seq_h[0]);
  u3_atom   bob_d = u3i_blob(mug_h[3], seq_h[3]);
  u3a_blob* blb_c = u3a_blob_new(mug_h[2], seq_h[2]);
  blb_c->hol_h += 1;

  //  epoch 5: the image's row at 5, one event at 6
  //
  MDB_env* env_u = _epoc_env(5);
  {
    c3_d img_d = ((c3_d)mug_h[0] << 32) | seq_h[0];
    c3_d bid_d = ((c3_d)mug_h[1] << 32) | seq_h[1];
    _check( c3y == u3_lmdb_save_blobs(env_u, 5, &img_d, 1),
            "image row save failed" );
    _epoc_event(env_u, 6, &bid_d, 1);
  }

  u3_disk log_u = { .mdb_u = env_u, .epo_d = 5 };
  _check( c3y == u3_disk_blob_own(&log_u, 5, 6), "mark failed" );

  _check( c3y == u3a_blob_get(mug_h[0], seq_h[0])->own_o,
          "image-row blob not owned" );
  _check( u3a_blob_get(mug_h[1], seq_h[1])
          && (c3y == u3a_blob_get(mug_h[1], seq_h[1])->own_o)
          && (0 == u3a_blob_get(mug_h[1], seq_h[1])->atm_h),
          "event-named blob not a ghost" );
  _check( c3n == blb_c->own_o, "hold-only owned" );
  _check( c3n == u3a_blob_get(mug_h[3], seq_h[3])->own_o, "unnamed atom owned" );

  //  a named file gone: an id owned in this process is not stat'd
  //  again, but the next boot's mark, with the flags zeroed as
  //  _find_home zeroes them, fails and says so
  //
  u3b_wipe(_tmp_bob, mug_h[1], seq_h[1]);
  _check( c3y == u3_disk_blob_own(&log_u, 5, 6), "owned id stat'd again" );
  u3a_blob_get(mug_h[0], seq_h[0])->own_o = c3n;
  u3a_blob_get(mug_h[1], seq_h[1])->own_o = c3n;
  _check( c3n == u3_disk_blob_own(&log_u, 5, 6), "mark passed a missing file" );

  u3_lmdb_exit(env_u);
  u3a_blob_get(mug_h[0], seq_h[0])->own_o = c3n;
  u3z(bob_a);
  u3z(bob_d);
  u3a_blob_drop(mug_h[1], seq_h[1]);
  blb_c->hol_h = 0;
  u3a_blob_drop(mug_h[2], seq_h[2]);
  _pier_done();
}

/* _stag_ran: what each staging job reported.
*/
static c3_w     _stag_num_w;
static u3_stag  _stag_got_u[4];

static void
_stag_don_cb(u3_stag* tag_u)
{
  _check( _stag_num_w < 4, "too many reports" );
  _stag_got_u[_stag_num_w++] = *tag_u;
}

/* _stg_count(): the files in .urb/stg.
*/
static c3_w
_stg_count(void)
{
  c3_c           stg_c[8192];
  DIR*           dir_u;
  struct dirent* ent_u;
  c3_w           num_w = 0;

  u3b_stg_dir(stg_c, _tmp_pier);
  _check( 0 != (dir_u = opendir(stg_c)), "opendir %s", stg_c );
  while ( (ent_u = readdir(dir_u)) ) {
    if ( '.' != ent_u->d_name[0] ) num_w++;
  }
  closedir(dir_u);
  return num_w;
}

/* _test_stag(): the staging queue runs one job at a time, in order:
**   a buffer stages and reports its mug; a file whose mug is the one
**   given is skipped; a queued job dropped never runs; a running job
**   dropped is not reported and its staged file is removed.
*/
static void
_test_stag(void)
{
  _pier_make("stag");

  if ( !u3_Host.lup_u ) {
    u3_Host.lup_u = uv_default_loop();
  }

  const c3_w len_w = 5000;
  c3_y*      dat_y = c3_malloc(len_w);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    dat_y[i_w] = (c3_y)(1 + (i_w % 253));
  }
  c3_h mug_h = u3r_mug_bytes(dat_y, len_w);

  c3_c fil_c[8192];
  snprintf(fil_c, sizeof(fil_c), "%s/source", _tmp_pier);
  _check( c3y == _file_put(fil_c, (const c3_c*)dat_y, len_w), "plant source" );

  c3_w a_w, b_w, c_w, d_w;
  u3_stag* tag_u[4];
  void*    own_v[4] = { &a_w, &b_w, &c_w, &d_w };

  for ( c3_w i_w = 0; i_w < 4; i_w++ ) {
    tag_u[i_w] = u3_stag_new(own_v[i_w], _stag_don_cb);
    tag_u[i_w]->pax_c = _tmp_pier;
  }
  tag_u[0]->buf_y = c3_malloc(len_w);
  memcpy(tag_u[0]->buf_y, dat_y, len_w);
  tag_u[0]->len_d = len_w;
  tag_u[1]->buf_y = c3_malloc(len_w);
  memcpy(tag_u[1]->buf_y, dat_y, len_w);
  tag_u[1]->len_d = len_w;
  tag_u[2]->nam_c = strdup(fil_c);
  tag_u[2]->gum_h = mug_h;
  tag_u[3]->nam_c = strdup(fil_c);

  _stag_num_w = 0;
  for ( c3_w i_w = 0; i_w < 4; i_w++ ) {
    u3_stag_plan(tag_u[i_w]);
  }

  //  a is on the pool, the rest wait.  drop a (running) and c (queued)
  //
  u3_stag_drop(&a_w);
  u3_stag_drop(&c_w);

  uv_run(u3L, UV_RUN_DEFAULT);

  _check( 2 == _stag_num_w, "%u report(s), want 2", _stag_num_w );
  _check(  (&b_w == _stag_got_u[0].ptr_v)
        && (c3y == _stag_got_u[0].ok_o)
        && (mug_h == _stag_got_u[0].mug_h),
          "the buffer job" );
  _check(  (&d_w == _stag_got_u[1].ptr_v)
        && (c3y == _stag_got_u[1].ok_o)
        && (c3n == _stag_got_u[1].sam_o)
        && (len_w == _stag_got_u[1].len_d)
        && (mug_h == _stag_got_u[1].mug_h),
          "the file job" );
  _check( 2 == _stg_count(), "%u staged file(s), want 2", _stg_count() );

  //  the same file with its mug known stages nothing
  //
  {
    u3_stag* sam_u = u3_stag_new(&c_w, _stag_don_cb);
    sam_u->pax_c = _tmp_pier;
    sam_u->nam_c = strdup(fil_c);
    sam_u->gum_h = mug_h;
    u3_stag_plan(sam_u);
    uv_run(u3L, UV_RUN_DEFAULT);
  }
  _check(  (3 == _stag_num_w)
        && (c3y == _stag_got_u[2].sam_o)
        && (c3n == _stag_got_u[2].ok_o),
          "an unchanged file was staged" );
  _check( 2 == _stg_count(), "%u staged file(s), want 2", _stg_count() );

  c3_free(dat_y);
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
  _test_stg_clean();
  _test_canon();
  _test_delete_empty_bucket();
  _test_walk();
  _test_install_stg();
  _test_install_stg_trim();
  _test_stage();
  _test_sane();
  _test_meld();
  _test_cue_blob();
  _test_hand();
  _test_hand_bail();
  _test_hand_signal();
  _test_hand_leak();
  _test_hand_outer();
  _test_view_hold();
  _test_sweep_settle();
  _test_hand_comp();
  _test_hand_alarm();
  _test_jam_bob();
  _test_jets_bob();
#ifndef U3_OS_windows
  _test_hand_intr();
  _test_hand_intr_crit();
#endif
  _test_hand_cue();
#ifndef U3_OS_windows
  _test_hand_emfile();
  _test_hand_nofd();
  _test_hand_wipe();
#endif
#ifndef U3_OS_windows
  _test_hand_trunc();
#endif
  _test_hand_gone();
#if !defined(VERE64) && !defined(U3_OS_windows)
  _test_jam_wide();
#endif
#ifndef U3_OS_windows
  _test_units_wide();
  _test_hand_grab();
#endif
  _test_hand_edge();
  _test_sift_bob();
  _test_hand_share();
  _test_hand_reuse();
  _test_hand_deep();
  _test_hand_fall();
#ifndef U3_OS_windows
  _test_hand_road();
#endif
  _test_hand_empty();
  _test_met_cache();
  _test_hand_access();
  _test_lifecycle();
  _test_blobs_persist();
  _test_live();
  _test_roll_link();
  _test_link_copy();
#ifndef U3_OS_windows
  _test_roll_same();
#endif
  _test_chop_kill();
  _test_gc_own();
  _test_blob_own();
  _test_grants();
  _test_stub_fill();
  _test_stag();

  fprintf(stderr, "test blob: ok\r\n");
  return 0;
}
