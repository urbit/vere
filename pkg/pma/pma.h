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
    /// Page is mapped but can't be read or written.
    PS_MAPPED_INACCESSIBLE = 0x3,
    PS_MASK                = 0x3,
};
typedef enum page_status page_status_t;

/// Out-of-memory handler.
typedef void (*oom_handler_t)(void *fault_addr, void *ctx);

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

    /// Hanlder to run when out of memory.
    oom_handler_t oom_handler;

    size_t        max_sz;

    /// libsigsegv ticket.
    void *sigsegv_ticket;
};
typedef struct pma pma_t;

//==============================================================================
// FUNCTIONS

/// Load a new or existing PMA into memory.
///
/// @param[in] base         Base address to create the PMA at.
/// @param[in] len          Length in bytes of the PMA. Must be greater than the
///                         sum of the lengths of the backing heap and stack
///                         files.
/// @param[in] heap_file    Optional backing file for heap. If NULL, changes to
///                         the heap will not be persistent.
/// @param[in] stack_file   Optional backing file for stack. If NULL, changes to
///                         the stack will not be persistent.
/// @param[in] oom_handler  Function to run if the PMA runs out of memory.
///
/// @return PMA handle  Success. When finished, call pma_unload() to dispose of
///                     the PMA's resources.
/// @return NULL        Failed to set up heap.
/// @return NULL        Failed to set up stack.
pma_t *
pma_load(void         *base,
         size_t        len,
         const char   *heap_file,
         const char   *stack_file,
         oom_handler_t oom_handler);

/// Sync changes to a PMA to disk.
///
/// @param[in] pma        PMA handle. Must not be NULL.
/// @param[in] heap_len   Length in bytes of the heap to synchronize. The range
///                       [heap_start, heap_start + heap_len) is synchronized to
///                       disk. If there is no backing heap file, no heap
///                       changes are synced to disk.
/// @param[in] stack_len  Length in bytes of the stack to synchronize. The
///                       range [stack_start - stack_len, stack_start) is
///                       synchronized to disk. If there is no backing stack
///                       file, no stack changes are synced to disk.
///
/// @return 0   Success.
/// @return -1  pma was NULL.
/// @return -1  Failed to sync changes to heap.
/// @return -1  Failed to sync changes to stack.
int
pma_sync(pma_t *pma, size_t heap_len, size_t stack_len);

/// Unload a PMA from memory and dispose of its resources, including freeing the
/// PMA handle itself.
///
/// @param[in] pma  PMA handle. If NULL, this function is a no-op.
void
pma_unload(pma_t *pma);

#endif /* ifndef PMA_PMA_H */
