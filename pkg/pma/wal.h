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

int
wal_open(const char *path, wal_t *wal);

int
wal_append(wal_t *wal, const wal_entry_t *entry);

int
wal_sync(const wal_t *wal);

int
wal_apply(wal_t *wal, int fd);

void
wal_destroy(wal_t *wal);

#endif /* ifndef PMA_JOURNAL_H */
