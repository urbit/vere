/// @file

#ifndef PMA_UTIL_H
#define PMA_UTIL_H

#include <stddef.h>

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
