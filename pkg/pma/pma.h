#ifndef PMA_PMA_H
#define PMA_PMA_H

#include <stddef.h>

//==============================================================================
// CONSTANTS
//==============================================================================

static const size_t kDefaultSz = 0xffffffff;

//==============================================================================
// TYPES
//==============================================================================

struct pma {
    void  *heap_start;

    void  *stack_start;

    int    heap_fd;

    int    stack_fd;

    size_t max_sz;
};
typedef struct pma pma_t;

//==============================================================================
// FUNCTIONS
//==============================================================================

/// @param[in] base        Base address to create the PMA at.
/// @param[in] len         Length in bytes of the PMA.
/// @param[in] heap_file   Optional backing file for heap.
/// @param[in] stack_file  Optional backing file for stack.
pma_t *
pma_init(void *base, size_t len, const char *heap_file, const char *stack_file);

int
pma_sync(pma_t *pma, size_t heap_sz, size_t stack_sz);

void
pma_deinit(pma_t *pma);

#endif /* ifndef PMA_PMA_H */
