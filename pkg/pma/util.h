/// @file

#ifndef PMA_UTIL_H
#define PMA_UTIL_H

#include <stddef.h>

/// Determine the max of `a` and `b`.
///
/// @param[in] a
/// @param[in] b
extern size_t
max(size_t a, size_t b);

/// Determine the min of `a` and `b`.
///
/// @param[in] a
/// @param[in] b
extern size_t
min(size_t a, size_t b);

/// Round `x` down to the nearest multiple of `n`, which must be a power of 2.
///
/// @param[in] x
/// @param[in] n
extern size_t
round_down(size_t x, size_t n);

/// Round `x` up to the nearest multiple of `n`, which must be a power of 2.
///
/// @param[in] x
/// @param[in] n
extern size_t
round_up(size_t x, size_t n);

#endif /* ifndef PMA_UTIL_H */
