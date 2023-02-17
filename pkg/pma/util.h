/// @file

#ifndef PMA_UTIL_H
#define PMA_UTIL_H

#include <stddef.h>

//==============================================================================
// ARITHMETIC

/// Convert `x` gibibytes to bytes.
#define GiB(x) ((size_t)(x) << 30)

/// Convert `x` mebibytes to bytes.
#define MiB(x) ((size_t)(x) << 20)

/// Convert `x` kibibytes to bytes.
#define KiB(x) ((size_t)(x) << 10)

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

//==============================================================================
// I/O

/// Read all requested bytes from a file into a buffer.
///
/// @param[in] fd    File descriptor to read from.
/// @param[out] buf  Buffer to read into. Must have enough space for `len`
///                  bytes.
/// @param[in] len   Number of bytes to read into `buf`.
///
/// @return -1  `len` bytes could not be read into `buf`. `errno` is set
///             appropriately.
/// @return  0  `len` bytes read into `buf`.
int
read_all(int fd, void *buf, size_t len);

/// Write all requested bytes from a buffer into a file.
///
/// @param[in] fd   File descriptor to write to.
/// @param[in] buf  Buffer to write from. Must have `len` bytes.
/// @param[in] len  Number of bytes to write from `buf`.
///
/// @return -1  `len` bytes could not be written to `fd`. `errno` is set
///             appropriately.
/// @return  0  `len` bytes written to `fd`.
int
write_all(int fd, const void *buf, size_t len);

#endif /* ifndef PMA_UTIL_H */
