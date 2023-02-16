/// @file

#include "journal.h"

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

journal_t *
journal_open(const char *path)
{
    if (!path) {
        fprintf(stderr, "journal: NULL is an invalid path\n");
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

    size_t len = buf.st_size;
    if (len % sizeof(journal_entry_t) != 0) {
        fprintf(stderr, "journal: %s is corrupt\n", path);
        goto close_fd;
    }

    journal_t *journal = malloc(sizeof(*journal));
    journal->path      = strdup(path);
    journal->fd        = fd;
    journal->offset    = 0;
    journal->len       = len;
    return journal;

close_fd:
    close(fd);
fail:
    return NULL;
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
    journal->offset += sizeof(*entry);
    return 0;
}

int
journal_apply(journal_t *journal, char *base)
{
    return 0;
}

int
journal_destroy(journal_t *journal)
{
    return 0;
}
