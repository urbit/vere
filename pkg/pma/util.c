/// @file

#include "util.h"

//==============================================================================
// ARITHMETIC

inline size_t
max(size_t a, size_t b)
{
    return a < b ? b : a;
}

inline size_t
min(size_t a, size_t b)
{
    return a < b ? a : b;
}

inline size_t
round_down(size_t x, size_t n)
{
    return x & ~(n - 1);
}

inline size_t
round_up(size_t x, size_t n)
{
    return (x + (n - 1)) & (~(n - 1));
}

//==============================================================================
// I/O

int
read_all(int fd, void *buf, size_t len)
{
    return -1;
}

int
write_all(int fd, const void *buf, size_t len)
{
    return -1;
}
