/// @file

#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

//==============================================================================
// ARITHMETIC

size_t
max(size_t a, size_t b);

size_t
min(size_t a, size_t b);

size_t
round_down(size_t x, size_t n);

size_t
round_up(size_t x, size_t n);

//==============================================================================
// I/O

int
read_all(int fd, void *buf, size_t len)
{
    int err;
    if (!buf) {
        err = EINVAL;
        goto fail;
    }
    if (len == 0) {
        return 0;
    }
    off_t cur_offset = lseek(fd, 0, SEEK_CUR);
    if (cur_offset == (off_t)-1) {
        err = errno;
        goto fail;
    }
    static const size_t kMaxAttempts = 100;
    size_t              attempts     = 0;
    char               *ptr          = buf;
    do {
        attempts++;
        ssize_t bytes_read = read(fd, ptr, len);
        if (bytes_read == -1) {
            if ((err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
                && attempts <= kMaxAttempts)
            {
                // From the read(2) man page: "On error, -1 is returned, and
                // errno is set to indicate the error. In this case, it is left
                // unspecified whether the file position (if any) changes."
                //
                // This means that we have to restore the file cursor to the
                // position that it was at immediately before the call to read()
                // that failed.
                if (lseek(fd, SEEK_SET, cur_offset) == (off_t)-1) {
                    err = errno;
                    goto fail;
                }
                continue;
            }
            fprintf(stderr,
                    "util: failed to read %zu bytes into %p: %s\r\n",
                    len,
                    ptr,
                    strerror(err));
            goto fail;
        }
        if (bytes_read == 0) {
            err = ECANCELED;
            fprintf(stderr,
                    "util: encountered unexpected EOF when reading %zu bytes "
                    "into %p\r\n",
                    len,
                    ptr);
            goto fail;
        }
        len -= bytes_read;
        ptr += bytes_read;
        cur_offset += bytes_read;
    } while (len > 0);
    return 0;

fail:
    errno = err;
    return -1;
}

int
write_all(int fd, const void *buf, size_t len)
{
    int err;
    if (!buf) {
        err = EINVAL;
        goto fail;
    }
    if (len == 0) {
        return 0;
    }
    static const size_t kMaxAttempts = 100;
    size_t              attempts     = 0;
    const char         *ptr          = buf;
    do {
        attempts++;
        ssize_t bytes_written = write(fd, ptr, len);
        if (bytes_written == -1) {
            err = errno;
            // Attempt to write again if we were interrupted by a signal.
            if ((err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
                && attempts <= kMaxAttempts)
            {
                continue;
            }
            fprintf(stderr,
                    "util: failed to write %zu bytes from %p: %s\r\n",
                    len,
                    ptr,
                    strerror(err));
            goto fail;
        }
        len -= bytes_written;
        ptr += bytes_written;
    } while (len > 0);
    return 0;

fail:
    errno = err;
    return -1;
}
