/// @file

#include "wal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

//==============================================================================
// FUNCTIONS

int
wal_open(const char *path, wal_t *wal)
{
    if (!path || !wal) {
        errno = EINVAL;
        goto fail;
    }

    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        fprintf(stderr,
                "wal: failed to open %s: %s\r\n",
                path,
                strerror(errno));
        goto fail;
    }

    struct stat buf;
    if (fstat(fd, &buf) == -1) {
        fprintf(stderr,
                "wal: failed to determine length of %s: %s\r\n",
                path,
                strerror(errno));
        goto close_fd;
    }

    if (buf.st_size % sizeof(wal_entry_t) != 0) {
        fprintf(stderr, "wal: %s is corrupt\r\n", path);
        goto close_fd;
    }

    wal->path      = strdup(path);
    wal->fd        = fd;
    wal->entry_cnt = buf.st_size / sizeof(wal_entry_t);
    return 0;

close_fd:
    close(fd);
fail:
    return -1;
}

int
wal_append(wal_t *wal, const wal_entry_t *entry)
{
    if (!wal || !entry) {
        errno = EINVAL;
        return -1;
    }
    if (write_all(wal->fd, entry, sizeof(*entry)) == -1) {
        return -1;
    }
    wal->entry_cnt++;
    return 0;
}

int
wal_sync(const wal_t *wal)
{
    if (!wal) {
        errno = EINVAL;
        return -1;
    }

    if (fsync(wal->fd) == -1) {
        fprintf(stderr,
                "wal: failed to flush changes to %s: %s\r\n",
                wal->path,
                strerror(errno));
        return -1;
    }

    return 0;
}

int
wal_apply(wal_t *wal, int fd)
{
    if (!wal || wal->fd < 0 || fd < 0) {
        errno = EINVAL;
        return -1;
    }

    if (wal->entry_cnt == 0) {
        return 0;
    }

    if (lseek(wal->fd, 0, SEEK_SET) == (off_t)-1) {
        fprintf(stderr,
                "wal: failed to seek to beginning of %s: %s\r\n",
                wal->path,
                strerror(errno));
        return -1;
    }

    wal_entry_t entry;
    off_t       offset;
    for (size_t i = 0; i < wal->entry_cnt; i++) {
        if (read_all(wal->fd, &entry, sizeof(entry)) == -1) {
            return -1;
        }
        offset = entry.pg_idx * kPageSz;
        if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
            fprintf(stderr,
                    "wal: failed to seek to offset %u of file descriptor "
                    "%d: %s\r\n",
                    offset,
                    fd,
                    strerror(errno));
            return -1;
        }
        if (write_all(fd, entry.pg, sizeof(entry.pg)) == -1) {
            return -1;
        }
    }

    return 0;
}

void
wal_destroy(wal_t *wal)
{
    if (!wal) {
        return;
    }
    assert(close(wal->fd) == 0);
    assert(unlink(wal->path) == 0);
    free((void *)wal->path);
}
