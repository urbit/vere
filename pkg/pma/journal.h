/// @file

#ifndef PMA_JOURNAL_H
#define PMA_JOURNAL_H

#include <stdint.h>

#include "page.h"

//==============================================================================
// TYPES

struct journal {
    const char *path;
    int         fd;
    size_t      offset;
    size_t      len;
};
typedef struct journal journal_t;

struct journal_entry {
    uint64_t pg_idx;
    char     pg[kPageSz];
};
typedef struct journal_entry journal_entry_t;

//==============================================================================
// FUNCTIONS

journal_t *
journal_open(const char *path);

int
journal_append(journal_t *journal, const journal_entry_t *entry);

int
journal_apply(journal_t *journal, char *base);

int
journal_destroy(journal_t *journal);

#endif /* ifndef PMA_JOURNAL_H */
