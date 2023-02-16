/// @file

#include "journal.h"

//==============================================================================
// FUNCTIONS

journal_t *
journal_open(const char *path)
{
    return NULL;
}

int
journal_append(journal_t *journal, const journal_entry_t *entry)
{
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
