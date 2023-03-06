#ifndef PMA_PMA_H
#define PMA_PMA_H

#include <stddef.h>
#include <stdint.h>

//==============================================================================
// TYPES

/// Status of a page in a PMA.
enum page_status {
    /// Page hasn't been read or written.
    PS_UNMAPPED = 0x0,
    /// Page has been read but not written.
    PS_MAPPED_CLEAN = 0x1,
    /// Page has been written.
    PS_MAPPED_DIRTY = 0x2,
    PS_MASK         = 0x3,
};
typedef enum page_status page_status_t;

/// Persistent memory arena handle.
struct pma {
    /// Path to file backing the heap. NULL if there is no backing file.
    const char *heap_file;

    /// Path to file backing the stack. NULL if there is no backing file.
    const char *stack_file;

    /// Base address of the heap.
    void *heap_start;

    /// Base address of the stack. Points to one byte past the end of the PMA.
    void *stack_start;

    /// Number of bytes of the heap mapped into memory. Guaranteed to be a
    /// multiple of kPageSz.
    size_t heap_len;

    /// Number of bytes of the stack mapped into memory. Guaranteed to be a
    /// multiple of kPageSz.
    size_t stack_len;

    /// File descriptor for the open backing heap file. -1 is there is no
    /// backing heap file or if the backing heap file isn't open.
    int heap_fd;

    /// File descriptor for the open backing stack file. -1 is there is no
    /// backing stack file or if the backing stack file isn't open.
    int stack_fd;

    /// Total number of pages in the PMA. Each page is `kPageSz` bytes.
    size_t num_pgs;

    /// Bit map for tracking pages, which can be in one of three states:
    /// unmapped, mapped and clean, or mapped and dirty (see page_status_t).
    /// Guaranteed to have at least (2 * num_pgs) bits.
    uint8_t *pg_status;

    size_t   max_sz;

    /// libsigsegv ticket.
    void *sigsegv_ticket;
};
typedef struct pma pma_t;

//==============================================================================
// FUNCTIONS

/// @param[in] base        Base address to create the PMA at.
/// @param[in] len         Length in bytes of the PMA.
/// @param[in] heap_file   Optional backing file for heap.
/// @param[in] stack_file  Optional backing file for stack.
pma_t *
pma_load(void *base, size_t len, const char *heap_file, const char *stack_file);

int
pma_sync(pma_t *pma, size_t heap_len, size_t stack_len);

void
pma_unload(pma_t *pma);

#endif /* ifndef PMA_PMA_H */
