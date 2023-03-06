/// @file

#ifndef PMA_JOURNAL_H
#define PMA_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "page.h"

//==============================================================================
// TYPES

/// Write-ahead log for updating a PMA.
struct wal {
    /// Path to the write-ahead log.
    const char *path;
    /// File descriptor for the open write-ahead log.
    int fd;
    /// Number of entries in the write-ahead log.
    size_t entry_cnt;
};
typedef struct wal wal_t;

/// Write-ahead log entry representing a single PMA page.
struct wal_entry {
    /// Page index.
    uint64_t pg_idx;
    /// Page contents.
    char pg[kPageSz];
};
typedef struct wal_entry wal_entry_t;

//==============================================================================
// FUNCTIONS

/// Open a WAL.
///
/// @param[in]  path  Path to the WAL. Must not be NULL.
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
/// @param[in] wal    WAL handle. Must not be NULL.
/// @param[in] entry  Entry to append. Must not be NULL.
///
/// @return 0   Success.
/// @return -1  wal or entry were NULL.
/// @return -1  Failed to write entry to WAL file.
int
wal_append(wal_t *wal, const wal_entry_t *entry);

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
