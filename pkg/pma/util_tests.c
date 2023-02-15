/// @file

#include "util.h"

#include <assert.h>

static const size_t kPageSz = 16 << 10;

static void
test_round_down_(void)
{
    assert(round_down(4, 0) == 0);
    assert(round_down(1, 1) == 1);
    assert(round_down(2, 1) == 2);
    assert(round_down(2, 2) == 2);
    assert(round_down(3, 2) == 2);
    assert(round_down(7, 4) == 4);
    assert(round_down(43, 8) == 40);
    assert(round_down(63, 16) == 48);
    assert(round_down(kPageSz - 1, kPageSz) == 0);
    assert(round_down(kPageSz, kPageSz) == kPageSz);
}

static void
test_round_up_(void)
{
    assert(round_up(3, 0) == 0);
    assert(round_up(6, 1) == 6);
    assert(round_up(7, 2) == 8);
    assert(round_up(11, 4) == 12);
    assert(round_up(17, 8) == 24);
    assert(round_up(42, 16) == 48);
    assert(round_up(670384, kPageSz) == (41 * kPageSz));
    assert(round_up(0, kPageSz) == 0);
    assert(round_up(kPageSz, kPageSz) == kPageSz);
}

int
main(int argc, char *argv[])
{
    test_round_down_();
    test_round_up_();
    return 0;
}
