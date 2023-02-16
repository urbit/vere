/// @file

#ifndef PMA_JOURNAL_H
#define PMA_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "page.h"

//==============================================================================
// TYPES

struct journal {
    const char *path;
    int         fd;
    size_t      entry_cnt;
};
typedef struct journal journal_t;

struct journal_entry {
    uint64_t pg_idx;
    char     pg[kPageSz];
};
typedef struct journal_entry journal_entry_t;

//==============================================================================
// FUNCTIONS

int
journal_open(const char *path, journal_t *journal);

int
journal_append(journal_t *journal, const journal_entry_t *entry);

int
journal_sync(const journal_t *journal);

int
journal_apply(journal_t *journal, char *base, bool grows_down);

void
journal_destroy(journal_t *journal);

#endif /* ifndef PMA_JOURNAL_H */
