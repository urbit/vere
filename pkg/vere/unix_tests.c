/// @file

#include <stdbool.h>

#include "noun.h"
#include "vere.h"

//  include the unix driver source to reach its internals.  its
//  exported symbols are renamed to avoid colliding with the copies
//  linked in from the vere library.
//
#define u3_readdir_r              _ut_readdir_r
#define u3_unix_cane              _ut_unix_cane
#define u3_unix_save              _ut_unix_save
#define u3_unix_initial_into_card _ut_unix_initial_into_card
#define u3_unix_ef_dirk           _ut_unix_ef_dirk
#define u3_unix_ef_ergo           _ut_unix_ef_ergo
#define u3_unix_ef_ogre           _ut_unix_ef_ogre
#define u3_unix_ef_wath           _ut_unix_ef_wath
#define u3_unix_ef_wend           _ut_unix_ef_wend
#define u3_unix_ef_hill           _ut_unix_ef_hill
#define u3_unix_ef_look           _ut_unix_ef_look
#define u3_unix_io_init           _ut_unix_io_init
#include "io/unix.c"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 26);
}

/* _ut_mon_init(): initialize a mount point for tests.
*/
static void
_ut_mon_init(u3_umon* mon_u, c3_c* nam_c, c3_c* pax_c)
{
  memset(mon_u, 0, sizeof(*mon_u));
  mon_u->nam_c       = nam_c;
  mon_u->syn_o       = c3n;
  mon_u->dir_u.dir   = c3y;
  mon_u->dir_u.dry   = c3n;
  mon_u->dir_u.pax_c = pax_c;
}

/* _test_safe(): path validation.
*/
static c3_i
_test_safe()
{
  c3_i ret_i = 1;

  if ( !_ut_unix_cane("/") ||
       !_ut_unix_cane("~.") ||
       !_ut_unix_cane("a") ||
       !_ut_unix_cane("a/b") ||
       !_ut_unix_cane("a/b/c/defg/h/ijklmnop") )
  {
    fprintf(stderr, "_safe fail 1\n");
    ret_i = 0;
  }

  if ( _ut_unix_cane("") ||
       _ut_unix_cane(".") ||
       _ut_unix_cane("..") ||
       _ut_unix_cane("/.") ||
       _ut_unix_cane("a/b/c//") ||
       _ut_unix_cane("a/b/.") ||
       _ut_unix_cane("/././../.") ||
       _ut_unix_cane("/../etc") )
  {
    fprintf(stderr, "_safe fail 2\r\n");
    ret_i = 0;
  }

  if ( !_ut_unix_cane(".a") ||
       !_ut_unix_cane("/.a.b.c/..c") )
  {
    fprintf(stderr, "_safe fail 3\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _test_lod(): mug-cache insert, update, delete; sorted order.
*/
static c3_i
_test_lod()
{
  c3_i     ret_i = 1;
  u3_unix  unx_u;
  u3_umon  mon_u;

  memset(&unx_u, 0, sizeof(unx_u));
  _ut_mon_init(&mon_u, "base", "/p");
  unx_u.pax_c = "/p";
  unx_u.mon_u = &mon_u;

  //  out-of-order inserts land sorted
  //
  _unix_lod_put(&mon_u, "/p/base/b.txt", 2);
  _unix_lod_put(&mon_u, "/p/base/a.txt", 1);
  _unix_lod_put(&mon_u, "/p/base/c.txt", 3);

  if (  (3 != mon_u.lod_w)
     || (0 != strcmp(mon_u.lod_u[0].pax_c, "/p/base/a.txt"))
     || (0 != strcmp(mon_u.lod_u[1].pax_c, "/p/base/b.txt"))
     || (0 != strcmp(mon_u.lod_u[2].pax_c, "/p/base/c.txt"))
     || (1 != mon_u.lod_u[0].mug_w)
     || (2 != mon_u.lod_u[1].mug_w)
     || (3 != mon_u.lod_u[2].mug_w) )
  {
    fprintf(stderr, "_lod fail: insert order\r\n");
    ret_i = 0;
  }

  //  put on an existing path updates in place
  //
  _unix_lod_put(&mon_u, "/p/base/b.txt", 9);

  if ( (3 != mon_u.lod_w) || (9 != mon_u.lod_u[1].mug_w) ) {
    fprintf(stderr, "_lod fail: update\r\n");
    ret_i = 0;
  }

  //  deleting a file removes its entry, keeping order
  //
  {
    u3_ufil fil_u;
    memset(&fil_u, 0, sizeof(fil_u));
    fil_u.dir   = c3n;
    fil_u.pax_c = "/p/base/b.txt";
    fil_u.par_u = &mon_u.dir_u;

    _unix_lod_del(&unx_u, &fil_u);

    if (  (2 != mon_u.lod_w)
       || (0 != strcmp(mon_u.lod_u[0].pax_c, "/p/base/a.txt"))
       || (0 != strcmp(mon_u.lod_u[1].pax_c, "/p/base/c.txt")) )
    {
      fprintf(stderr, "_lod fail: delete\r\n");
      ret_i = 0;
    }

    //  deleting a missing path is a no-op
    //
    fil_u.pax_c = "/p/base/nope.txt";
    _unix_lod_del(&unx_u, &fil_u);

    if ( 2 != mon_u.lod_w ) {
      fprintf(stderr, "_lod fail: missing delete\r\n");
      ret_i = 0;
    }
  }

  _unix_free_lod(&mon_u);
  return ret_i;
}

/* _test_seed(): new file nodes pick up their mug from the cache.
*/
static c3_i
_test_seed()
{
  c3_i     ret_i = 1;
  u3_unix  unx_u;
  u3_umon  mon_u;

  memset(&unx_u, 0, sizeof(unx_u));
  _ut_mon_init(&mon_u, "base", "/p");
  unx_u.pax_c = "/p";
  unx_u.mon_u = &mon_u;

  _unix_lod_put(&mon_u, "/p/base/a.txt", 0xabcd);

  {
    u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
    _unix_watch_file(&unx_u, fil_u, &mon_u.dir_u, "/p/base/a.txt");

    if ( 0xabcd != fil_u->gum_w ) {
      fprintf(stderr, "_seed fail: cached mug\r\n");
      ret_i = 0;
    }
  }

  {
    u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
    _unix_watch_file(&unx_u, fil_u, &mon_u.dir_u, "/p/base/new.txt");

    if ( 0 != fil_u->gum_w ) {
      fprintf(stderr, "_seed fail: uncached mug\r\n");
      ret_i = 0;
    }
  }

  _unix_free_lod(&mon_u);
  return ret_i;
}

/* _test_find_node(): path lookup in the node tree.
*/
static c3_i
_test_find_node()
{
  c3_i     ret_i = 1;
  u3_unix  unx_u;
  u3_umon  mon_u;
  u3_udir* dir_u = c3_malloc(sizeof(u3_udir));
  u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));

  memset(&unx_u, 0, sizeof(unx_u));
  _ut_mon_init(&mon_u, "base", "/p");
  unx_u.pax_c = "/p";
  unx_u.mon_u = &mon_u;

  _unix_watch_dir(dir_u, &mon_u.dir_u, "/p/base");
  _unix_watch_file(&unx_u, fil_u, dir_u, "/p/base/foo.txt");

  if ( (u3_unod*)fil_u != _unix_find_node(&mon_u.dir_u, "/p/base/foo.txt") ) {
    fprintf(stderr, "_find fail: file\r\n");
    ret_i = 0;
  }

  if ( (u3_unod*)dir_u != _unix_find_node(&mon_u.dir_u, "/p/base") ) {
    fprintf(stderr, "_find fail: dir\r\n");
    ret_i = 0;
  }

  if (  _unix_find_node(&mon_u.dir_u, "/p/base/nope.txt")
     || _unix_find_node(&mon_u.dir_u, "/p/nope")
     || _unix_find_node(&mon_u.dir_u, "/p/base/foo.txt.bak") )
  {
    fprintf(stderr, "_find fail: miss\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _test_mug_cache(): sidecar save/load round-trip; corruption safety.
*/
static c3_i
_test_mug_cache()
{
  c3_i ret_i = 1;
  c3_c pir_c[64];

  strcpy(pir_c, "/tmp/ut-unix-XXXXXX");
  if ( !mkdtemp(pir_c) ) {
    fprintf(stderr, "_mug_cache fail: mkdtemp\r\n");
    return 0;
  }

  {
    c3_c urb_c[80];
    snprintf(urb_c, sizeof(urb_c), "%s/.urb", pir_c);
    c3_mkdir(urb_c, 0700);
  }

  u3_unix unx_u;
  memset(&unx_u, 0, sizeof(unx_u));
  unx_u.pax_c = pir_c;

  //  save a cache, load it back, compare
  //
  {
    u3_umon mon_u, won_u;
    c3_c    pax_c[128];

    _ut_mon_init(&mon_u, "base", pir_c);
    _ut_mon_init(&won_u, "base", pir_c);
    unx_u.mon_u = &mon_u;

    snprintf(pax_c, sizeof(pax_c), "%s/base/a.txt", pir_c);
    _unix_lod_put(&mon_u, pax_c, 0x1111);
    snprintf(pax_c, sizeof(pax_c), "%s/base/sub/b.hoon", pir_c);
    _unix_lod_put(&mon_u, pax_c, 0x2222);

    _unix_save_mugs(&unx_u, &mon_u);
    _unix_load_mugs(&unx_u, &won_u);

    if (  (2 != won_u.lod_w)
       || (0x1111 != won_u.lod_u[0].mug_w)
       || (0x2222 != won_u.lod_u[1].mug_w)
       || (0 != strcmp(won_u.lod_u[0].pax_c, mon_u.lod_u[0].pax_c))
       || (0 != strcmp(won_u.lod_u[1].pax_c, mon_u.lod_u[1].pax_c)) )
    {
      fprintf(stderr, "_mug_cache fail: round trip\r\n");
      ret_i = 0;
    }

    //  a second save must atomically replace the first
    //
    _unix_save_mugs(&unx_u, &mon_u);
    _unix_free_lod(&won_u);
    _unix_load_mugs(&unx_u, &won_u);

    if ( 2 != won_u.lod_w ) {
      fprintf(stderr, "_mug_cache fail: re-save\r\n");
      ret_i = 0;
    }

    _unix_free_lod(&mon_u);
    _unix_free_lod(&won_u);
  }

  //  an unrecognized header discards the cache
  //
  {
    u3_umon mon_u;
    c3_c*   pax_c;
    FILE*   fil_u;

    _ut_mon_init(&mon_u, "base", pir_c);
    pax_c = _unix_mug_pax(&unx_u, &mon_u, "");
    fil_u = fopen(pax_c, "w");
    fprintf(fil_u, "not-a-mug-cache\n00000001 base/a.txt\n");
    fclose(fil_u);

    _unix_load_mugs(&unx_u, &mon_u);

    if ( 0 != mon_u.lod_w ) {
      fprintf(stderr, "_mug_cache fail: bad header\r\n");
      ret_i = 0;
    }
    c3_free(pax_c);
  }

  //  a malformed line discards the cache wholesale
  //
  {
    u3_umon mon_u;
    c3_c*   pax_c;
    FILE*   fil_u;

    _ut_mon_init(&mon_u, "base", pir_c);
    pax_c = _unix_mug_pax(&unx_u, &mon_u, "");
    fil_u = fopen(pax_c, "w");
    fprintf(fil_u, "%s\n00000001 base/a.txt\ngarbage\n", SYNC_MUG_HEAD);
    fclose(fil_u);

    _unix_load_mugs(&unx_u, &mon_u);

    if ( 0 != mon_u.lod_w ) {
      fprintf(stderr, "_mug_cache fail: malformed line\r\n");
      ret_i = 0;
    }
    c3_free(pax_c);
  }

  //  a missing cache loads as empty
  //
  {
    u3_umon mon_u;
    _ut_mon_init(&mon_u, "missing", pir_c);
    _unix_load_mugs(&unx_u, &mon_u);

    if ( 0 != mon_u.lod_w ) {
      fprintf(stderr, "_mug_cache fail: missing file\r\n");
      ret_i = 0;
    }
  }

  return ret_i;
}

/* _test_mim_mug(): mugs are computed over the octet-stream at its
**                  declared length, including trailing zero bytes.
*/
static c3_i
_test_mim_mug()
{
  c3_i ret_i = 1;

  //  'ab' followed by two zero bytes: the atom drops them, the
  //  mime length keeps them
  //
  {
    c3_y    dat_y[4] = { 'a', 'b', 0x0, 0x0 };
    u3_noun mim = u3nt(c3__text, u3i_word(4), u3i_bytes(4, dat_y));
    c3_w    mug_w = _unix_mim_mug(mim);

    if ( u3r_mug_bytes(dat_y, 4) != mug_w ) {
      fprintf(stderr, "_mim_mug fail: trailing zeros\r\n");
      ret_i = 0;
    }
    if ( u3r_mug(u3t(u3t(mim))) == mug_w ) {
      fprintf(stderr, "_mim_mug fail: noun mug should differ\r\n");
      ret_i = 0;
    }
    u3z(mim);
  }

  //  no trailing zeros: byte mug at declared length still matches
  //
  {
    c3_y    dat_y[3] = { 'a', 'b', 'c' };
    u3_noun mim = u3nt(c3__text, u3i_word(3), u3i_bytes(3, dat_y));

    if ( u3r_mug_bytes(dat_y, 3) != _unix_mim_mug(mim) ) {
      fprintf(stderr, "_mim_mug fail: plain\r\n");
      ret_i = 0;
    }
    u3z(mim);
  }

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_safe() ) {
    fprintf(stderr, "test unix: failed\r\n");
    exit(1);
  }

  if ( !_test_lod() ) {
    fprintf(stderr, "test unix: lod failed\r\n");
    exit(1);
  }

  if ( !_test_seed() ) {
    fprintf(stderr, "test unix: seed failed\r\n");
    exit(1);
  }

  if ( !_test_find_node() ) {
    fprintf(stderr, "test unix: find-node failed\r\n");
    exit(1);
  }

  if ( !_test_mug_cache() ) {
    fprintf(stderr, "test unix: mug-cache failed\r\n");
    exit(1);
  }

  if ( !_test_mim_mug() ) {
    fprintf(stderr, "test unix: mim-mug failed\r\n");
    exit(1);
  }

  fprintf(stderr, "test unix: ok\r\n");
  return 0;
}
