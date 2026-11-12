/// @file
///
/// tests for windows loom address staking, in platform/windows/wloom.c.
///
/// windows only. POSIX reservation and collision tests are in loom_tests.c.
///
/// exercised through the public interface, against real address space --
/// the property under test is what the operating system does, and a mock
/// of VirtualAlloc2 would only assert that the mock behaves as expected.

#include "wloom.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

//  high, 64KB-aligned, and far from any loom base. the point is to be
//  somewhere nothing else in the process will have claimed.
//
#define _tst_bas ((void*)0x3A000000000ULL)
#define _tst_alt ((void*)0x3B000000000ULL)
#define _tst_thr ((void*)0x3C000000000ULL)
#define _tst_lom ((void*)0x3D000000000ULL)
#define _tst_len ((size_t)64 << 20)

static int fal_i = 0;

static void
_check(const char* nam_c, int cok_i, const char* det_c)
{
  printf("  [%s] %-46s %s\n", cok_i ? "PASS" : "FAIL", nam_c,
                              det_c ? det_c : "");
  if ( !cok_i ) {
    fal_i++;
  }
}

/* _test_collision_unstaked(): the bug staking exists to prevent.
**
**   an unrelated allocation at a stale loom base makes the migration
**   that wants it fail outright, because windows will not place a
**   mapping over occupied address space.
*/
static void
_test_collision_unstaked(void)
{
  void* squ_v = VirtualAlloc(_tst_bas, _tst_len, MEM_RESERVE, PAGE_NOACCESS);

  if ( !squ_v ) {
    _check("0. squat on the base", 0, "could not reserve the test address");
    return;
  }

  _check("0. squat on the base", 1, "");

  //  NB: fid -1 with a zero image length reserves and maps, without
  //  touching a file. the reservation is the part under test.
  //
  _check("1. unstaked hold fails on a taken address",
         c3n == u3_wnd_loom_hold(_tst_bas, _tst_len, -1, 0),
         "");

  VirtualFree(squ_v, 0, MEM_RELEASE);
}

/* _test_stake_reserves(): a stake actually holds the address.
*/
static void
_test_stake_reserves(void)
{
  void* squ_v;

  _check("2. stake succeeds on a free address",
         c3y == u3_wnd_loom_stake(_tst_bas, _tst_len), "");

  //  the whole point: nothing else can now have it
  //
  squ_v = VirtualAlloc(_tst_bas, _tst_len, MEM_RESERVE, PAGE_NOACCESS);

  _check("3. a staked address cannot be taken", 0 == squ_v, "");

  if ( squ_v ) {
    VirtualFree(squ_v, 0, MEM_RELEASE);
  }
}

/* _test_stake_idempotent(): u3m_init can run twice in a process.
**
**   the slot table is not visible from here, so the duplicate is caught
**   by filling it: a process stakes its loom and its migration sources,
**   which is every slot there is. a duplicate that consumed one would
**   leave the last distinct address without a home.
*/
static void
_test_stake_idempotent(void)
{
  _check("4. staking the same range twice is idempotent",
         c3y == u3_wnd_loom_stake(_tst_bas, _tst_len), "");

  _check("5. a second distinct stake still fits",
         c3y == u3_wnd_loom_stake(_tst_alt, _tst_len),
         "duplicate did not consume a slot");

  _check("6. and so does a third, filling the table",
         c3y == u3_wnd_loom_stake(_tst_thr, _tst_len),
         "duplicate did not consume a slot");
}

/* _test_hold_claims_stake(): the regression this all guards against.
**
**   a stake makes its address occupied, so hold must claim it rather
**   than reserve it again -- reserving over your own placeholder fails
**   exactly as an intruder's would.
*/
static void
_test_hold_claims_stake(void)
{
  _check("7. hold claims a staked address",
         c3y == u3_wnd_loom_hold(_tst_bas, _tst_len, -1, 0), "");

  _check("8. drop releases it",
         c3y == u3_wnd_loom_drop(_tst_bas), "");

  //  and it is genuinely free afterward
  //
  {
    void* got_v = VirtualAlloc(_tst_bas, _tst_len, MEM_RESERVE, PAGE_NOACCESS);

    _check("9. the address is free after drop", 0 != got_v, "");

    if ( got_v ) {
      VirtualFree(got_v, 0, MEM_RELEASE);
    }
  }
}

/* _test_hold_relengths(): a stake of the wrong length must not block.
**
**   the length staked at process start and the length finally reserved
**   can differ -- a migration's destination loom is capped independently
**   of --loom. a stake that cannot be handed over in place has to be
**   released, or the protection becomes the collision it was added to
**   prevent.
*/
static void
_test_hold_relengths(void)
{
  _check("10. hold succeeds on a stake of another length",
         c3y == u3_wnd_loom_hold(_tst_alt, _tst_len >> 1, -1, 0),
         "the stake blocked its own address");

  _check("11. drop releases it",
         c3y == u3_wnd_loom_drop(_tst_alt), "");
}

static c3_o
_tst_state(void* adr_v, size_t len_i, DWORD sat_u)
{
  c3_y* cur_y = (c3_y*)adr_v;
  c3_y* end_y = cur_y + len_i;

  while ( cur_y < end_y ) {
    MEMORY_BASIC_INFORMATION inf_u;

    if (  !VirtualQuery(cur_y, &inf_u, sizeof(inf_u))
       || (sat_u != inf_u.State) )
    {
      return c3n;
    }

    cur_y = (c3_y*)inf_u.BaseAddress + inf_u.RegionSize;
  }

  return c3y;
}

static c3_o
_tst_fill(void* adr_v, size_t len_i, c3_y val_y)
{
  if ( c3n == u3_wnd_loom_commit(adr_v, len_i) ) {
    return c3n;
  }

  memset(adr_v, val_y, len_i);
  return c3y;
}

static c3_o
_tst_is(void* adr_v, size_t len_i, c3_y val_y)
{
  c3_y*  byt_y = (c3_y*)adr_v;
  size_t i_i;

  for ( i_i = 0; i_i < len_i; i_i++ ) {
    if ( val_y != byt_y[i_i] ) {
      return c3n;
    }
  }

  return c3y;
}

static void
_test_remap_releases(void)
{
  const size_t img_i = (size_t)1 << 20;
  const size_t mid_i = (size_t)10 << 20;
  c3_y*        bas_y = (c3_y*)_tst_lom;
  c3_c         dir_c[MAX_PATH], pax_c[MAX_PATH];
  c3_i         fid_i;

  if ( c3n == u3_wnd_loom_init(_tst_lom, _tst_len) ) {
    _check("12. reserve a loom", 0, "");
    return;
  }

  if (  !GetTempPathA(sizeof(dir_c), dir_c)
     || !GetTempFileNameA(dir_c, "wlm", 0, pax_c)
     || (-1 == (fid_i = _open(pax_c, _O_RDWR | _O_BINARY | _O_TRUNC))) )
  {
    _check("12. create an image file", 0, "");
    return;
  }

  {
    static c3_y buf_y[1 << 20];
    memset(buf_y, 0xa5, sizeof(buf_y));

    if ( (int)sizeof(buf_y) != _write(fid_i, buf_y, sizeof(buf_y)) ) {
      _check("12. create an image file", 0, "");
      _close(fid_i);
      DeleteFileA(pax_c);
      return;
    }
  }

  _check("12. fill the loom",
         (c3y == _tst_fill(bas_y, img_i, 0x11))
         && (c3y == _tst_fill(bas_y + img_i, (size_t)8 << 20, 0x22))
         && (c3y == _tst_fill(bas_y + _tst_len - 65536, 65536, 0x33)),
         "");

  _check("13. remap", c3y == u3_wnd_loom_mapf(fid_i, img_i), "");

  _check("14. the image shows the file",
         c3y == _tst_is(bas_y, img_i, 0xa5), "");

  _check("15. the volatile loom is uncommitted",
         c3y == _tst_state(bas_y + img_i, _tst_len - img_i, MEM_RESERVE),
         "the old section's commit was not released");

  _check("16. commit a marker",
         c3y == _tst_fill(bas_y + mid_i, 4096, 0x44), "");

  _check("17. unmap", c3y == u3_wnd_loom_unmapf(), "");

  _check("18. the marker is released by the unmap",
         c3y == _tst_state(bas_y + mid_i, 4096, MEM_RESERVE), "");

  _check("19. remap again", c3y == u3_wnd_loom_mapf(fid_i, img_i), "");

  _check("20. the image still shows the file",
         c3y == _tst_is(bas_y, img_i, 0xa5), "");

  _check("21. fill a range to toss",
         c3y == _tst_fill(bas_y + mid_i, (size_t)1 << 20, 0x55), "");

  _check("22. toss it",
         c3y == u3_wnd_loom_toss(bas_y + mid_i, (size_t)1 << 20), "");

  _check("23. the tossed range is uncommitted",
         c3y == _tst_state(bas_y + mid_i, (size_t)1 << 20, MEM_RESERVE),
         "toss did not return commit");

  _check("24. and reads as zero once committed again",
         (c3y == u3_wnd_loom_commit(bas_y + mid_i, (size_t)1 << 20))
         && (c3y == _tst_is(bas_y + mid_i, (size_t)1 << 20, 0)),
         "");

  u3_wnd_loom_unmapf();
  _close(fid_i);
  DeleteFileA(pax_c);
}

int
main(int argc, char* argv[])
{
  (void)argc; (void)argv;

  _test_collision_unstaked();
  _test_stake_reserves();
  _test_stake_idempotent();
  _test_hold_claims_stake();
  _test_hold_relengths();
  _test_remap_releases();

  if ( fal_i ) {
    fprintf(stderr, "wloom: %d failure%s\r\n", fal_i, (1 == fal_i) ? "" : "s");
    return 1;
  }

  fprintf(stderr, "wloom okeedokee\r\n");
  return 0;
}
