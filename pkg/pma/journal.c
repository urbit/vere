/// @file

#include "journal.h"

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
journal_open(const char *path, journal_t *journal)
{
    if (!path || !journal) {
        errno = EINVAL;
        goto fail;
    }

    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        fprintf(stderr,
                "journal: failed to open %s: %s\n",
                path,
                strerror(errno));
        goto fail;
    }

    struct stat buf;
    if (fstat(fd, &buf) == -1) {
        fprintf(stderr,
                "journal: failed to determine length of %s: %s\n",
                path,
                strerror(errno));
        goto close_fd;
    }

    if (buf.st_size % sizeof(journal_entry_t) != 0) {
        fprintf(stderr, "journal: %s is corrupt\n", path);
        goto close_fd;
    }

    journal->path      = strdup(path);
    journal->fd        = fd;
    journal->entry_cnt = buf.st_size / sizeof(journal_entry_t);
    return 0;

close_fd:
    close(fd);
fail:
    return -1;
}

int
journal_append(journal_t *journal, const journal_entry_t *entry)
{
    if (!journal || !entry) {
        errno = EINVAL;
        return -1;
    }
    if (write_all(journal->fd, entry, sizeof(*entry)) == -1) {
        return -1;
    }
    journal->entry_cnt++;
    return 0;
}

int
journal_apply(journal_t *journal, char *base, bool grows_down)
{
    if (!journal || !base) {
        errno = EINVAL;
        return -1;
    }

    if (lseek(journal->fd, 0, SEEK_SET) == (off_t)-1) {
        fprintf(stderr,
                "journal: failed to seek to beginning of %s: %s\n",
                journal->path,
                strerror(errno));
        return -1;
    }

    if (grows_down) {
        base -= kPageSz;
    }
    int             direction = grows_down ? -1 : 1;

    journal_entry_t entry;
    for (size_t i = 0; i < journal->entry_cnt; i++) {
        if (read_all(journal->fd, &entry, sizeof(entry)) == -1) {
            return -1;
        }
        char *dst = base + direction * entry.pg_idx;
        memcpy(dst, entry.pg, sizeof(entry.pg));
    }

    return 0;
}

void
journal_destroy(journal_t *journal)
{
    if (!journal) {
        return;
    }
    assert(close(journal->fd) == 0);
    assert(unlink(journal->path) == 0);
    free((void *)journal->path);
    free(journal);
}
