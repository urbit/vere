/// @file
///
/// tests for windows loom address staking, in platform/windows/wloom.c.
///
/// windows only. on POSIX mmap(MAP_FIXED) evicts whatever occupies a fixed
/// address, so a collision at a stale loom base cannot arise and there is
/// nothing here to test.
///
/// exercised through the public interface, against real address space --
/// the property under test is what the operating system does, and a mock
/// of VirtualAlloc2 would only assert that the mock behaves as expected.

#include "wloom.h"

#include <stdio.h>
#include <windows.h>

//  high, 64KB-aligned, and far from any loom base. the point is to be
//  somewhere nothing else in the process will have claimed.
//
#define _tst_bas ((void*)0x3A000000000ULL)
#define _tst_alt ((void*)0x3B000000000ULL)
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
**   with two stake slots, a duplicate that consumed a slot would leave
**   no room for a second distinct address -- which is what this checks,
**   the slot table not being visible from here.
*/
static void
_test_stake_idempotent(void)
{
  _check("4. staking the same range twice is idempotent",
         c3y == u3_wnd_loom_stake(_tst_bas, _tst_len), "");

  _check("5. a second distinct stake still fits",
         c3y == u3_wnd_loom_stake(_tst_alt, _tst_len),
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
  _check("6. hold claims a staked address",
         c3y == u3_wnd_loom_hold(_tst_bas, _tst_len, -1, 0), "");

  _check("7. drop releases it",
         c3y == u3_wnd_loom_drop(_tst_bas), "");

  //  and it is genuinely free afterward
  //
  {
    void* got_v = VirtualAlloc(_tst_bas, _tst_len, MEM_RESERVE, PAGE_NOACCESS);

    _check("8. the address is free after drop", 0 != got_v, "");

    if ( got_v ) {
      VirtualFree(got_v, 0, MEM_RELEASE);
    }
  }
}

int
main(int argc, char* argv[])
{
  (void)argc; (void)argv;

  _test_collision_unstaked();
  _test_stake_reserves();
  _test_stake_idempotent();
  _test_hold_claims_stake();

  if ( fal_i ) {
    fprintf(stderr, "wloom: %d failure%s\r\n", fal_i, (1 == fal_i) ? "" : "s");
    return 1;
  }

  fprintf(stderr, "wloom okeedokee\r\n");
  return 0;
}
