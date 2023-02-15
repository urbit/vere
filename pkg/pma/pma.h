#ifndef PMA_PMA_H
#define PMA_PMA_H

#include <stddef.h>
#include <stdint.h>

//==============================================================================
// CONSTANTS
//==============================================================================

static const size_t kPageSz = 16 << 10;

static const size_t kDefaultSz = 0xffffffff;

//==============================================================================
// TYPES
//==============================================================================

struct pma {
    void *heap_start;

    void *stack_start;

    /// Number of bytes of the heap are mapped into memory. Guaranteed to be a
    /// multiple of kPageSz.
    size_t heap_len;

    /// Number of bytes of the stack mapped into memory. Guaranteed to be a
    /// multiple of kPageSz.
    size_t stack_len;

    int    heap_fd;

    int    stack_fd;

    /// Total number of pages.
    size_t num_pgs;

    /// Bit map for tracking pages, which can be in one of three states:
    /// unmapped, mapped and clean, or mapped and dirty. Guaranteed to have at
    /// least (2 * num_pgs) bits.
    uint8_t *pg_status;

    size_t   max_sz;

    /// libsigsegv ticket.
    void *sigsegv_ticket;
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
