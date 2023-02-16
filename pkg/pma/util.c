/// @file

#include "util.h"

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
