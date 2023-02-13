#include "pma.h"

#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

//==============================================================================
// CONSTANTS
//==============================================================================

static const size_t kDefaultSz = 0xffffffff;

//==============================================================================
// STATIC FUNCTIONS
//==============================================================================

int
map_file_(const char *path, void *base, bool grows_down, size_t *len, int *fd)
{
    return 0;
}

//==============================================================================
// FUNCTIONS
//==============================================================================

pma_t *
pma_init(void *base, const char *heap_file, const char *stack_file)
{
    void  *heap_start = base;
    size_t heap_len;
    int    heap_fd;
    // Failed to map non-NULL heap file.
    if (map_file_(heap_file, heap_start, false, &heap_len, &heap_fd) == -1) {
        return NULL;
    }

    size_t max_sz = kDefaultSz;

    void  *stack_start = (char *)heap_start + max_sz;
    size_t stack_len;
    int    stack_fd;
    // Failed to map non-NULL stack file.
    if (map_file_(stack_file, stack_start, true, &stack_len, &stack_fd) == -1) {
        munmap(heap_start, heap_len);
        close(heap_fd);
        return NULL;
    }

    pma_t *pma = malloc(sizeof(*pma));
    *pma       = (pma_t){
              .heap_start  = heap_start,
              .stack_start = stack_start,
              .heap_fd     = heap_fd,
              .stack_fd    = stack_fd,
              .max_sz      = max_sz,
    };

    return pma;
}

int
pma_sync(pma_t *pma, size_t heap_sz, size_t stack_sz)
{
    return 0;
}

void
pma_deinit(pma_t *pma)
{}
