/// @file
///
/// This file defines the interface for a write-ahead log (WAL). The required
/// pattern of usage is:
/// - Open a WAL (either new or existing) with wal_open(). If opening an
///   existing WAL, wal_open() will fail if the WAL is corrupt.
/// - Append page-sized entries to the WAL using wal_append().
/// - Flush all changes to the WAL to disk using wal_sync().
/// - Apply the changes in the WAL to another file using wal_apply().
/// - Destroy the WAL using wal_destroy(), which removes it from the file
///   system.

#ifndef PMA_WAL_H
#define PMA_WAL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "page.h"

//==============================================================================
// CONSTANTS

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
/// @param[in]  path  Path to the directory containing the WAL. Must not be
///                   NULL.
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
/// @param[in] pg_idx  Page index of heap or stack page. Index 0 is the first
///                    page of the heap, index 1 is the second page of the heap,
///                    index -1 is the first page of the stack, index -2 is the
///                    second page of the stack, etc.
/// @param[in] pg      Page to append. Must not be NULL.
///
/// @return 0   Success.
/// @return -1  wal or entry were NULL.
/// @return -1  Failed to write entry to WAL file.
int
wal_append(wal_t *wal, ssize_t pg_idx, const char pg[kPageSz]);

/// Sync a WAL to disk.
///
/// @param[in] wal  WAL handle. Must not be NULL.
///
/// @return 0   Success.
/// @return -1  wal was NULL.
/// @return -1  Failure.
int
wal_sync(const wal_t *wal);

/// Apply a WAL to a heap and stack.
///
/// Only call this function if all necessary entries have been appended to the
/// WAL via wal_append(). Once a WAL is applied, wal_append() must not be
/// called.
///
/// @param[in] wal       WAL handle. Must not be NULL.
/// @param[in] heap_fd   File descriptor to heap to apply WAL to.
/// @param[in] stack_fd  File descriptor to stack to apply WAL to.
///
/// @return 0   Success.
/// @return -1  Failure.
int
wal_apply(wal_t *wal, int heap_fd, int stack_fd);

/// Dispose of a WAL's resources and remove the WAL from the file system.
///
/// @param[in] wal  WAL handle. If NULL, this function is a no-op.
void
wal_destroy(wal_t *wal);

#endif /* ifndef PMA_WAL_H */
