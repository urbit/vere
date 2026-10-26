/// @file
///
/// tests for the migration of a checkpoint to loom version 6: the SKA
/// state at the end of the road, and the registrations for SKA rebuilt
/// from the cold state.

#include "noun.h"
#include "steel.h"
#include "ur/ur.h"
#include "vere.h"

#include "../noun/migrate.h"
#include "direct.h"
#include "ivory.h"
#include "nock-compile.h"

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _CHECK(cond, what)                                                    \
  if ( !(cond) ) {                                                            \
    fprintf(stderr, "*** %s: %s\r\n", __func__, what);                        \
    ret_i = 0;                                                                \
  }

/* _tmpdir(): mkdtemp wrapper, aborts on failure.  caller frees.
*/
static c3_c*
_tmpdir(void)
{
  char  tmpl[] = "/tmp/migrate-test-XXXXXX";
  c3_c* dir_c  = mkdtemp(tmpl);

  if ( !dir_c ) {
    fprintf(stderr, "*** mkdtemp: %s\r\n", strerror(errno));
    exit(1);
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

/* _cue(): cue an embedded pill.
*/
static u3_noun
_cue(const c3_y* byt_y, c3_d len_d)
{
  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  u3_weak      pil   = u3s_cue_xeno_with(sil_u, len_d, byt_y);

  u3s_cue_xeno_done(sil_u);

  if ( u3_none == pil ) {
    fprintf(stderr, "*** cue failed\r\n");
    exit(1);
  }

  return pil;
}

/* _boot(): boot the kernel from the ivory pill in u3n, which registers
**          its cores.
*/
static void
_boot(void)
{
  if ( c3n == u3v_boot_lite(_cue(u3_Ivory_pill, u3_Ivory_pill_len)) ) {
    fprintf(stderr, "*** boot failed\r\n");
    exit(1);
  }
}

/* _patch_v5(): make the home struct in the snapshot image version 5, as
**              an old develop snapshot has it: no SKA state after the
**              road, and no tables version 5 added over its lifetime.
*/
static void
_patch_v5(c3_c* dir_c)
{
  c3_c      pax_c[8192];
  c3_z      pag_z = ((c3_z)1 << u3a_page) * sizeof(c3_w);
  c3_y*     pag_y = c3_malloc(pag_z);
  u3v_home* hom_u = (void*)pag_y;
  c3_i      fid_i;

  snprintf(pax_c, sizeof(pax_c), "%s/.urb/chk/image.bin", dir_c);

  if (  (-1 == (fid_i = open(pax_c, O_RDWR)))
     || (pag_z != pread(fid_i, pag_y, pag_z, 0)) )
  {
    fprintf(stderr, "*** image %s: %s\r\n", pax_c, strerror(errno));
    exit(1);
  }

  u3_assert( U3V_VER6 == hom_u->ver_d );

  memset(&(hom_u->rod_u.ska), 0, sizeof(hom_u->rod_u.ska));
  hom_u->rod_u.lop_p     = 0;
  hom_u->rod_u.cax.for_p = 0;
  hom_u->rod_u.dup_p     = 0;
  hom_u->ver_d = U3V_VER5;

  if ( pag_z != pwrite(fid_i, pag_y, pag_z, 0) ) {
    fprintf(stderr, "*** image %s: %s\r\n", pax_c, strerror(errno));
    exit(1);
  }

  close(fid_i);
  c3_free(pag_y);
}

/* _tem(): the registrations for SKA: how many, and the sum of their
**         mugs, the same for the same set.
*/
typedef struct {
  c3_w num_w;
  c3_w mug_w;
} _tem_sum;

static void
_tem_cb(u3_noun kev, void* ptr_v)
{
  _tem_sum* sum_u = ptr_v;
  u3_noun   i;

  for ( i = u3t(kev); u3_nul != i; i = u3t(i) ) {
    sum_u->num_w++;
    sum_u->mug_w += u3r_mug(u3h(i));
  }
}

static _tem_sum
_tem(void)
{
  _tem_sum sum_u = {0};
  u3h_walk_with(u3R->ska.tem_p, _tem_cb, &sum_u);
  return sum_u;
}

/* _nock_n(), _nock_nc(): .*(bus fol) by each interpreter, for u3m_soft().
*/
static u3_noun
_nock_n(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3n_nock_on(bus, fol);
}

static u3_noun
_nock_nc(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3nc_nock_on(bus, fol);
}

/* _test_ska(): the SKA core, installed after the migration, is fed the
**              rebuilt registrations: a gate calling the kernel computes
**              the same in both interpreters.
*/
static c3_i
_test_ska(void)
{
  u3_noun gat = u3v_wish("|=  n=@\n^-  @\n?:  =(0 n)  0\n+($(n (dec n)))\n");
  u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(1000, u3k(u3t(u3t(gat)))));
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_noun a, b;
  c3_i    ret_i = 1;

  u3d_boot(_cue(u3_Steel_pill, u3_Steel_pill_len));

  a = u3m_soft(0, _nock_n, u3nc(u3k(cor), u3k(fol)));
  b = u3m_soft(0, _nock_nc, u3nc(cor, fol));

  _CHECK( 0 == u3h(a), "u3n run" );
  _CHECK( c3y == u3r_sing(a, b), "u3nc agrees with u3n" );

  u3z(gat); u3z(a); u3z(b);
  return ret_i;
}

/* _test_pier(): a pier's checkpoint, made version 5, migrates in place
**               and its registrations for SKA come back from the cold
**               state.
*/
static c3_i
_test_pier(void)
{
  c3_c*    dir_c = _tmpdir();
  _tem_sum bef_u, aft_u;
  c3_i     ret_i = 1;
  c3_w     i_w;

  u3C.wag_h |= u3o_hashless;

  //  a pier at version 6: the kernel boot registers its cores
  //
  u3m_boot(dir_c, 1ULL << 33);
  _boot();
  bef_u = _tem();
  _CHECK( bef_u.num_w > 0, "cores registered at boot" );
  u3m_save();
  u3m_stop();

  //  its checkpoint, as version 5 left it: cold state, no SKA state
  //
  _patch_v5(dir_c);

  //  migrated in place, as disk.c does before booting
  //
  _CHECK( c3y == u3_disk_migrate_home(dir_c, 1ULL << 33, 0), "migrated" );
  u3m_stop();

  u3m_boot(dir_c, 1ULL << 33);

  _CHECK( U3V_VER6 == u3H->ver_d, "version after restore" );
  _CHECK( u3_nul == u3R->ska.new, "no pending registrations" );
  _CHECK( u3_nul == u3R->ska.cor, "no SKA core" );
  _CHECK( u3R->lop_p && u3R->cax.for_p && u3R->dup_p, "older tables made" );
  for ( i_w = 0; i_w < c3_array_len(u3R->fut_w); i_w++ ) {
    _CHECK( 0 == u3R->fut_w[i_w], "futureproof buffer clear" );
  }

  aft_u = _tem();
  _CHECK( bef_u.num_w == aft_u.num_w, "as many registrations as before" );
  _CHECK( bef_u.mug_w == aft_u.mug_w, "the same registrations as before" );
  fprintf(stderr, "test migrate: %u registrations rebuilt\r\n", aft_u.num_w);

  if ( !_test_ska() ) {
    ret_i = 0;
  }

  u3m_stop();
  _rmtree(dir_c);
  c3_free(dir_c);
  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  if ( !_test_pier() ) {
    fprintf(stderr, "test migrate: pier: failed\r\n");
    exit(1);
  }

  fprintf(stderr, "test migrate: ok\r\n");
  return 0;
}
