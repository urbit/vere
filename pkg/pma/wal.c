/// @file
///
/// This file implements the WAL interface. It checks for corruption using a per
/// page entry checksum, which is a checksum of the entry's page index and page
/// contents, and a global checksum, which is the checksum composed of all per
/// page entry checksums.

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

#ifdef FCNTL_FSYNC
#    define fsync(fd) fcntl(fd, F_FULLFSYNC)
#endif

//==============================================================================
// CONSTANTS

/// Seed value for hashing function used to compute page checksum.
static const size_t kSeed = 0;

/// File stem for a WAL's data file.
static const char kDataStem[] = "data";

/// File stem for a WAL's metadata file.
static const char kMetaStem[] = "meta";

/// File extension for a WAL file.
static const char kWalExt[] = "wal";

/// Size in bytes of _metadata_entry_t's pg_idx field. Must be a macro to
/// ensure kPageIdxSz + kPageSz can be resolved at compile time.
#define kPageIdxSz sizeof(((_metadata_entry_t *)NULL)->pg_idx)

//==============================================================================
// TYPES

/// The header in a WAL's metadata file.
typedef struct _metadata_hdr {
    /// Global checksum.
    uint64_t global_checksum;
} _metadata_hdr_t;

/// An entry in a WAL's metadata file.
typedef struct _metadata_entry {
    /// Page index.
    int64_t pg_idx;
    /// Checksum of page index and page contents.
    uint64_t checksum;
} _metadata_entry_t;

//==============================================================================
// STATIC FUNCTIONS

/// Open a file of the form <path>/<stem>.<suffix>.
///
/// @param[in]  dir        Path to containing directory.
/// @param[in]  stem       File stem.
/// @param[in]  suffix     File suffix.
/// @param[out] path       Populated with the concatenation of base_path and
///                        suffix.
/// @param[out] fd         Populated with an open file descriptor for file at
///                        path.
/// @param[out] len        Populated with length of file at path.
///
/// @return 0   Success.
/// @return -1  Failure.
static int
_open_file(const char *dir,
           const char *stem,
           const char *suffix,
           char      **path,
           int        *fd,
           size_t     *len);

static int
_open_file(const char *dir,
           const char *stem,
           const char *suffix,
           char      **path,
           int        *fd,
           size_t     *len)
{
    assert(dir);
    assert(suffix);
    assert(path);
    assert(fd);
    assert(len);

    int err;

    if (asprintf(path, "%s/%s.%s", dir, stem, suffix) == -1) {
        err = ECANCELED;
        fprintf(stderr, "wal: failed to construct path\r\n");
        goto fail;
    }

    int fd_ = open(*path, O_CREAT | O_RDWR, 0644);
    if (fd_ == -1) {
        err = errno;
        fprintf(stderr, "wal: failed to open %s: %s\r\n", *path, strerror(err));
        goto free_path;
    }

    struct stat buf;
    if (fstat(fd_, &buf) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to determine length of %s: %s\r\n",
                *path,
                strerror(err));
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
    errno = err;
    return -1;
}

//==============================================================================
// FUNCTIONS

int
wal_open(const char *path, wal_t *wal)
{
    int err;

    if (!path || !wal) {
        err = EINVAL;
        goto fail;
    }

    size_t meta_len;
    if (_open_file(path,
                   kMetaStem,
                   kWalExt,
                   (char **)&wal->meta_path,
                   &wal->meta_fd,
                   &meta_len)
        == -1)
    {
        err = errno;
        fprintf(stderr, "wal: failed to open metadata file\r\n");
        goto fail;
    }

    // Don't include the header length in the entry count calculation.
    if (meta_len > 0) {
      meta_len -= sizeof(_metadata_hdr_t);
    }

    if (meta_len % sizeof(_metadata_entry_t) != 0) {
        err = ENOTRECOVERABLE;
        fprintf(stderr,
                "wal: metadata file at %s is corrupt: expected length to be a "
                "multiple of %zu but is %zu\r\n",
                wal->data_path,
                sizeof(_metadata_entry_t),
                meta_len);
        goto close_metadata_file;
    }

    size_t data_len;
    if (_open_file(path,
                   kDataStem,
                   kWalExt,
                   (char **)&wal->data_path,
                   &wal->data_fd,
                   &data_len)
        == -1)
    {
        err = errno;
        fprintf(stderr, "wal: failed to open data file\r\n");
        goto close_metadata_file;
    }

    if (data_len % kPageSz != 0) {
        err = ENOTRECOVERABLE;
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
    size_t entry_cnt2 = meta_len / sizeof(_metadata_entry_t);
    if (entry_cnt != entry_cnt2) {
        err = ENOTRECOVERABLE;
        fprintf(stderr,
                "wal: metadata file (%s) has %zu entries but data file (%s) "
                "has %zu entries\r\n",
                wal->meta_path,
                entry_cnt2,
                wal->data_path,
                entry_cnt);
        goto close_data_file;
    }

    if (entry_cnt == 0) {
        wal->checksum = 0;
        if (write_all(wal->meta_fd, &wal->checksum, sizeof(wal->checksum))
            == -1)
        {
            int err = errno;
            fprintf(stderr,
                    "wal: failed to write global checksum %llx to %s: %s\r\n",
                    wal->checksum,
                    wal->meta_path,
                    strerror(err));
            goto close_data_file;
        }
    } else {
        // Verify checksums.
        _metadata_hdr_t hdr;
        if (read_all(wal->meta_fd, &hdr, sizeof(hdr)) == -1) {
          err = errno;
          fprintf(stderr,
                  "wal: failed to read header from metadata file (%s): %s\r\n",
                  wal->meta_path,
                  strerror(err));
          goto fail;
        }
        wal->checksum = hdr.global_checksum;
        char              page[kPageIdxSz + kPageSz];
        _metadata_entry_t entry;
        uint64_t          checksums[]     = {0, 0};
        uint64_t         *global_checksum = checksums;
        uint64_t         *checksum        = checksums + 1;
        // i is unused since read_all() implicitly updates the underlying file
        // offset for the given file descriptor.
        for (size_t i = 0; i < entry_cnt; i++) {
            if (read_all(wal->data_fd, page + kPageIdxSz, kPageSz) == -1) {
                err = errno;
                fprintf(
                    stderr,
                    "wal: failed to read page #%zu from data file (%s): %s\r\n",
                    i,
                    wal->data_path,
                    strerror(err));
                goto close_data_file;
            }
            if (read_all(wal->meta_fd, &entry, sizeof(entry)) == -1) {
                err = errno;
                fprintf(
                    stderr,
                    "wal: failed to read entry #%zu from metadata file (%s): "
                    "%s\r\n",
                    i,
                    wal->meta_path,
                    strerror(err));
                goto close_data_file;
            }
            memcpy(page, &entry.pg_idx, kPageIdxSz);
            *checksum = 0;
            MurmurHash3_x86_32(page, sizeof(page), kSeed, checksum);
            if (*checksum != entry.checksum) {
                err = ENOTRECOVERABLE;
                fprintf(stderr,
                        "wal: checksum %llx computed from page #%zu of data "
                        "file (%s) "
                        "doesn't match checksum %llx from entry #%zu of "
                        "metadata file "
                        "(%s)\r\n",
                        *checksum,
                        i,
                        wal->data_path,
                        entry.checksum,
                        i,
                        wal->meta_path);
                goto close_data_file;
            }
            // Compute the new global checksum using the existing global
            // checksum and this entry's checksum.
            uint64_t tmp = 0;
            MurmurHash3_x86_32(checksums, sizeof(checksums), kSeed, &tmp);
            *global_checksum = tmp;
        }
        if (*global_checksum != wal->checksum) {
            err = ENOTRECOVERABLE;
            fprintf(stderr,
                    "wal: global checksum %llx computed from data file (%s) "
                    "doesn't match global checksum %llx from metadata file "
                    "(%s)\r\n",
                    *global_checksum,
                    wal->data_path,
                    wal->checksum,
                    wal->meta_path);
            goto close_data_file;
        }
    }
    wal->entry_cnt = entry_cnt;

    return 0;

close_data_file:
    close(wal->data_fd);
    free((void *)wal->data_path);
close_metadata_file:
    close(wal->meta_fd);
    free((void *)wal->meta_path);
fail:
    errno = err;
    return -1;
}

int
wal_append(wal_t *wal, ssize_t pg_idx, const char pg[kPageSz])
{
    if (!wal || !pg) {
        errno = EINVAL;
        return -1;
    }
    if (write_all(wal->data_fd, pg, kPageSz) == -1) {
        return -1;
    }

    _metadata_entry_t entry;
    entry.pg_idx = pg_idx;
    entry.checksum = 0;
    MurmurHash3_x86_32(pg, kPageSz, kSeed, &entry.checksum);
    if (write_all(wal->meta_fd, &entry, sizeof(entry)) == -1) {
        return -1;
    }

    // Compute new global checksum using the previous global checksum and this
    // entry's checksum.
    uint64_t checksums[] = {
        wal->checksum,
        entry.checksum,
    };
    uint64_t tmp = 0;
    MurmurHash3_x86_32(checksums, sizeof(checksums), kSeed, &tmp);
    wal->checksum = tmp;

    wal->entry_cnt++;
    return 0;
}

int
wal_sync(const wal_t *wal)
{
    int err;

    if (!wal) {
        err = EINVAL;
        goto fail;
    }

    if (lseek(wal->meta_fd, 0, SEEK_SET) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to seek to beginning of %s: %s\r\n",
                wal->meta_path,
                strerror(err));
        goto fail;
    }

    if (write_all(wal->meta_fd, &wal->checksum, sizeof(wal->checksum)) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to write global checksum %llx to %s: %s\r\n",
                wal->checksum,
                wal->meta_path,
                strerror(err));
        goto fail;
    }

    if (fsync(wal->data_fd) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to flush changes to %s: %s\r\n",
                wal->data_path,
                strerror(err));
        goto fail;
    }

    if (fsync(wal->meta_fd) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to flush changes to %s: %s\r\n",
                wal->meta_path,
                strerror(err));
        goto fail;
    }

    return 0;

fail:
    errno = err;
    return -1;
}

int
wal_apply(wal_t *wal, int heap_fd, int stack_fd)
{
    int err;

    if (!wal || heap_fd < 0 || stack_fd < 0) {
        err = EINVAL;
        goto fail;
    }

    if (wal->entry_cnt == 0) {
        return 0;
    }

    if (lseek(wal->data_fd, 0, SEEK_SET) == (off_t)-1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to seek to beginning of %s: %s\r\n",
                wal->data_path,
                strerror(err));
        goto fail;
    }

    // Seek past the global checksum to the first metdata entry.
    if (lseek(wal->meta_fd, sizeof(wal->checksum), SEEK_SET) == (off_t)-1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to seek to beginning of %s: %s\r\n",
                wal->meta_path,
                strerror(err));
        goto fail;
    }

    char              pg[kPageSz];
    _metadata_entry_t entry;
    size_t            pg_idx;
    int               fd;
    off_t             offset;
    for (size_t i = 0; i < wal->entry_cnt; i++) {
        if (read_all(wal->data_fd, pg, sizeof(pg)) == -1) {
            err = errno;
            goto fail;
        }
        if (read_all(wal->meta_fd, &entry, sizeof(entry)) == -1) {
            err = errno;
            goto fail;
        }
        // Convert a 1-based negative page index to a 0-based positive one.
        if (entry.pg_idx < 0) {
            pg_idx = -(entry.pg_idx + 1);
            fd     = stack_fd;
        } else {
            pg_idx = entry.pg_idx;
            fd     = heap_fd;
        }
        offset = pg_idx * kPageSz;
        if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
            err = errno;
            fprintf(stderr,
                    "wal: failed to seek to offset %lld of file descriptor "
                    "%d: %s\r\n",
                    offset,
                    fd,
                    strerror(err));
            goto fail;
        }
        if (write_all(fd, pg, sizeof(pg)) == -1) {
            err = errno;
            goto fail;
        }
    }

    if (fsync(heap_fd) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to flush changes to heap file with fd %d: %s\r\n",
                heap_fd,
                strerror(err));
        goto fail;
    }

    if (fsync(stack_fd) == -1) {
        err = errno;
        fprintf(stderr,
                "wal: failed to flush changes to stack file with fd %d: %s\r\n",
                stack_fd,
                strerror(err));
        goto fail;
    }

    return 0;

fail:
    errno = err;
    return -1;
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
