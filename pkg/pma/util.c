/// @file

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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
    if (!buf) {
        return -1;
    }
    char *ptr = buf;
    do {
        ssize_t bytes_read = read(fd, ptr, len);
        if (bytes_read == -1) {
            fprintf(stderr,
                    "util: failed to read %zu bytes into %p: %s\n",
                    len,
                    ptr,
                    strerror(errno));
            return -1;
        }
        if (bytes_read == 0) {
            fprintf(stderr,
                    "util: encountered unexpected EOF when reading %zu bytes "
                    "into %p\n",
                    len,
                    ptr);
            return -1;
        }
        len -= bytes_read;
        ptr += bytes_read;
    } while (len > 0);
    return 0;
}

int
write_all(int fd, const void *buf, size_t len)
{
    if (!buf) {
        return -1;
    }
    const char *ptr = buf;
    do {
        ssize_t bytes_written = write(fd, ptr, len);
        if (bytes_written == -1) {
            fprintf(stderr,
                    "util: failed to write %zu bytes from %p: %s\n",
                    len,
                    ptr,
                    strerror(errno));
            return -1;
        }
        len -= bytes_written;
        ptr += bytes_written;
    } while (len > 0);
    return 0;
}
