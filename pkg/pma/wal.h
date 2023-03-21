/// @file

#ifndef PMA_JOURNAL_H
#define PMA_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "page.h"

//==============================================================================
// CONSTANTS

/// File extension for a WAL's data file.
static const char kWalDataExt[] = "data";

/// File extension for a WAL's metadata file.
static const char kWalMetaExt[] = "meta";

//==============================================================================
// TYPES

/// Write-ahead log (WAL) for updating a PMA.
///
/// A WAL is comprised of two separate files: a metadata file and a data file.
/// The data file contains the raw contents of the pages in the WAL, and the
/// metadata file contains the page index and page checksum for each page in the
/// data file.
typedef struct wal {
    /// Path to the write-ahead log's data file.
    const char *data_path;

    /// Path to the write-ahead log's metadata file.
    const char *meta_path;

    /// File descriptor for the open data file of the write-ahead log.
    int data_fd;

    /// File descriptor for the open metadata file of the write-ahead log.
    int meta_fd;

    /// Number of entries in the write-ahead log.
    size_t entry_cnt;

    /// In-progress global checksum.
    uint64_t checksum;
} wal_t;

//==============================================================================
// FUNCTIONS

/// Open a WAL.
///
/// @param[in]  path  Path to the WAL. kWalDataExt and kWalMetaExt will be
///                   appended to the end of this path to generate the data and
///                   metadata file paths, respectively, for the WAL. Must not
///                   be NULL.
/// @param[out] wal   Populated with the resulting WAL handle. Must not be NULL.
///
/// @return 0   Success.
/// @return -1  path or wal were NULL.
/// @return -1  Failed to open file at path.
/// @return -1  File at path is corrupt.
int
wal_open(const char *path, wal_t *wal);

/// Append a new entry to a WAL.
///
/// @param[in] wal     WAL handle. Must not be NULL.
/// @param[in] pg_idx  Zero-based page index of page.
/// @param[in] pg      Page to append. Must not be NULL.
///
/// @return 0   Success.
/// @return -1  wal or entry were NULL.
/// @return -1  Failed to write entry to WAL file.
int
wal_append(wal_t *wal, size_t pg_idx, const char pg[kPageSz]);

/// Sync a WAL to disk.
///
/// @param[in] wal  WAL handle. Must not be NULL.
///
/// @return 0   Success.
/// @return -1  wal was NULL.
/// @return -1  Failure.
int
wal_sync(const wal_t *wal);

/// Apply a WAL to a (PMA) file.
///
/// @param[in] wal  WAL handle. Must not be NULL.
/// @param[in] fd   File descriptor to file to apply WAL to.
///
/// @return 0   Success.
/// @return -1  Failure.
int
wal_apply(wal_t *wal, int fd);

/// Dispose of a WAL's resources and remove the WAL from the file system.
///
/// @param[in] wal  WAL handle. If NULL, this function is a no-op.
void
wal_destroy(wal_t *wal);

#endif /* ifndef PMA_JOURNAL_H */
