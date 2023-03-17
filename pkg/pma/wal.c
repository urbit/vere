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

#include "murmur3.h"
#include "util.h"

//==============================================================================
// CONSTANTS

/// Seed value for hashing function used to compute page checksum.
static const size_t kSeed = 0;

//==============================================================================
// TYPES

/// An entry in a WAL's metadata file.
typedef struct metadata_entry_ {
    /// Page index.
    uint64_t pg_idx;
    /// Checksum of page contents.
    uint64_t checksum;
} metadata_entry_t_;

//==============================================================================
// STATIC FUNCTIONS

/// Open a file.
///
/// @param[in]  base_path  Path to base file.
/// @param[in]  suffix     Suffix to append to base file path.
/// @param[out] path       Populated with the concatenation of base_path and
///                        suffix.
/// @param[out] fd         Populated with an open file descriptor for file at
///                        path.
/// @param[out] len        Populated with length of file at path.
///
/// @return 0   Success.
/// @return -1  Failure.
static int
open_file_(const char *base_path,
           const char *suffix,
           char      **path,
           int        *fd,
           size_t     *len);

static int
open_file_(const char *base_path,
           const char *suffix,
           char      **path,
           int        *fd,
           size_t     *len)
{
    assert(base_path);
    assert(suffix);
    assert(path);
    assert(fd);
    assert(len);

    if (asprintf(path, "%s.%s", base_path, suffix) == -1) {
        fprintf(stderr,
                "wal: failed to append %s to %s\r\n",
                suffix,
                base_path);
        goto fail;
    }

    int fd_ = open(*path, O_CREAT | O_RDWR, 0644);
    if (fd_ == -1) {
        fprintf(stderr,
                "wal: failed to open %s: %s\r\n",
                *path,
                strerror(errno));
        goto free_path;
    }

    struct stat buf;
    if (fstat(fd_, &buf) == -1) {
        fprintf(stderr,
                "wal: failed to determine length of %s: %s\r\n",
                *path,
                strerror(errno));
        goto close_fd;
    }

    *fd  = fd_;
    *len = buf.st_size;

    return 0;

close_fd:
    close(fd_);
free_path:
    free((void *)*path);
fail:
    return -1;
}

//==============================================================================
// FUNCTIONS

int
wal_open(const char *path, wal_t *wal)
{
    if (!path || !wal) {
        errno = EINVAL;
        goto fail;
    }

    size_t meta_len;
    if (open_file_(path,
                   kWalMetaExt,
                   (char **)&wal->meta_path,
                   &wal->meta_fd,
                   &meta_len)
        == -1)
    {
        fprintf(stderr, "wal: failed to open metadata file\r\n");
        goto close_data_file;
    }

    if (meta_len % sizeof(metadata_entry_t_) != 0) {
        fprintf(stderr,
                "wal: metadata file at %s is corrupt: expected length to be a "
                "multiple of %zu but is %zu\r\n",
                wal->data_path,
                sizeof(metadata_entry_t_),
                meta_len);
    }

    size_t data_len;
    if (open_file_(path,
                   kWalDataExt,
                   (char **)&wal->data_path,
                   &wal->data_fd,
                   &data_len)
        == -1)
    {
        fprintf(stderr, "wal: failed to open data file\r\n");
        goto fail;
    }

    if (data_len % kPageSz != 0) {
        fprintf(stderr,
                "wal: data file at %s is corrupt: expected length to be a "
                "multiple of %zu but is %zu\r\n",
                wal->data_path,
                kPageSz,
                data_len);
        goto close_data_file;
    }

    // Ensure length of data and metadata files make sense.
    size_t entry_cnt  = data_len / kPageSz;
    size_t entry_cnt2 = meta_len / sizeof(metadata_entry_t_);
    if (entry_cnt != entry_cnt2) {
        fprintf(stderr,
                "wal: metadata file (%s) has %zu entries but data file (%s) "
                "has %zu entries\r\n",
                wal->meta_path,
                entry_cnt2,
                wal->data_path,
                entry_cnt);
        goto close_data_file;
    }

    // Verify checksums.
    char              page[kPageSz];
    metadata_entry_t_ entry;
    uint64_t          checksum;
    for (size_t i = 0; i < entry_cnt; i++) {
        if (read_all(wal->data_fd, page, sizeof(page)) == -1) {
            fprintf(stderr,
                    "wal: failed to read page #%zu from data file (%s)\r\n",
                    i,
                    wal->data_path);
            goto close_data_file;
        }
        if (read_all(wal->meta_fd, &entry, sizeof(entry)) == -1) {
            fprintf(
                stderr,
                "wal: failed to read entry #%zu from metadata file (%s)\r\n",
                i,
                wal->meta_path);
            goto close_data_file;
        }
        MurmurHash3_x86_32(page, sizeof(page), kSeed, &checksum);
        if (checksum != entry.checksum) {
            fprintf(stderr,
                    "wal: checksum computed from page #%zu of data file (%s) "
                    "doesn't match checksum from entry #%zu of metadata file "
                    "(%s)\r\n",
                    i,
                    wal->data_path,
                    i,
                    wal->meta_path);
            goto close_data_file;
        }
    }
    wal->entry_cnt = entry_cnt;

    return 0;

close_metadata_file:
    close(wal->meta_fd);
    free((void *)wal->meta_path);
close_data_file:
    close(wal->data_fd);
    free((void *)wal->data_path);
fail:
    return -1;
}

int
wal_append(wal_t *wal, size_t pg_idx, const char pg[kPageSz])
{
    if (!wal || !pg) {
        errno = EINVAL;
        return -1;
    }
    if (write_all(wal->data_fd, pg, kPageSz) == -1) {
        return -1;
    }

    metadata_entry_t_ entry;
    entry.pg_idx = pg_idx;
    MurmurHash3_x86_32(pg, kPageSz, kSeed, &entry.checksum);
    if (write_all(wal->meta_fd, &entry, sizeof(entry)) == -1) {
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

    if (fsync(wal->data_fd) == -1) {
        fprintf(stderr,
                "wal: failed to flush changes to %s: %s\r\n",
                wal->data_path,
                strerror(errno));
        return -1;
    }

    if (fsync(wal->meta_fd) == -1) {
        fprintf(stderr,
                "wal: failed to flush changes to %s: %s\r\n",
                wal->meta_path,
                strerror(errno));
        return -1;
    }

    return 0;
}

int
wal_apply(wal_t *wal, int fd)
{
    if (!wal || fd < 0) {
        errno = EINVAL;
        return -1;
    }

    if (wal->entry_cnt == 0) {
        return 0;
    }

    if (lseek(wal->data_fd, 0, SEEK_SET) == (off_t)-1) {
        fprintf(stderr,
                "wal: failed to seek to beginning of %s: %s\r\n",
                wal->data_path,
                strerror(errno));
        return -1;
    }

    if (lseek(wal->meta_fd, 0, SEEK_SET) == (off_t)-1) {
        fprintf(stderr,
                "wal: failed to seek to beginning of %s: %s\r\n",
                wal->meta_path,
                strerror(errno));
        return -1;
    }

    char              pg[kPageSz];
    metadata_entry_t_ entry;
    off_t             offset;
    for (size_t i = 0; i < wal->entry_cnt; i++) {
        if (read_all(wal->data_fd, pg, sizeof(pg)) == -1) {
            return -1;
        }
        if (read_all(wal->meta_fd, &entry, sizeof(entry)) == -1) {
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
        if (write_all(fd, pg, sizeof(pg)) == -1) {
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
    close(wal->data_fd);
    close(wal->meta_fd);
    if (unlink(wal->data_path) == -1) {
        fprintf(stderr,
                "wal: failed to remove data file (%s): %s\r\n",
                wal->data_path,
                strerror(errno));
    }
    if (unlink(wal->meta_path) == -1) {
        fprintf(stderr,
                "wal: failed to remove metadata file (%s): %s\r\n",
                wal->meta_path,
                strerror(errno));
    }
    free((void *)wal->data_path);
    free((void *)wal->meta_path);
}
