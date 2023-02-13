#include "pma.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sigsegv.h"

//==============================================================================
// STATIC FUNCTIONS
//==============================================================================

/// Handle a page fault according to the libsigsegv protocol. See
/// sigsegv_handler_t in sigsegv.h for more info.
///
/// @param[in] fault_addr
/// @param[in] serious
static int
handle_page_fault_(void *fault_addr, int serious);

/// @param[in]  path
/// @param[in]  base
/// @param[in]  grows_down
/// @param[out] len
/// @param[out] fd
static int
map_file_(const char *path, void *base, bool grows_down, size_t *len, int *fd);

/// Round `x` up to the nearest multiple of `n`, which must be a power of 2.
///
/// @param[in] x
/// @param[in] n
static inline size_t
round_up_(size_t x, size_t n)
{
    return (x + (n - 1)) & (~(n - 1));
}

static int
handle_page_fault_(void *fault_addr, int serious)
{
    assert((uintptr_t)fault_addr % kPageSz == 0);
    return 0;
}

static int
map_file_(const char *path, void *base, bool grows_down, size_t *len, int *fd)
{
    if (!path) {
        static const size_t kDefaultSz = kPageSz;
        if (grows_down) {
            base = (char *)base - kDefaultSz;
        }
        if (mmap(base,
                 kDefaultSz,
                 PROT_READ | PROT_WRITE,
                 MAP_ANON | MAP_FIXED | MAP_PRIVATE,
                 -1,
                 0)
            == MAP_FAILED)
        {
            fprintf(
                stderr,
                "pma: failed to create %zu-byte anonymous mapping at %p: %s\n",
                kDefaultSz,
                base,
                strerror(errno));
            goto fail;
        }
        *fd  = -1;
        *len = kDefaultSz;
        return 0;
    }

    int fd_ = open(path, O_CREAT | O_RDWR, 0644);
    // A parent directory doesn't exist.
    if (fd_ == -1) {
        fprintf(stderr, "pma: failed to open %s: %s\n", path, strerror(errno));
        goto fail;
    }

    struct stat buf_;
    if (fstat(fd_, &buf_) == -1) {
        fprintf(stderr,
                "pma: failed to determine length of %s: %s\n",
                strerror(errno));
        goto close_fd;
    }

    size_t len_ = round_up_(buf_.st_size, kPageSz);

    // We have to map stacks a page at a time because a stack's backing file has
    // its first page at offset zero, which belongs at the highest address.
    if (grows_down) {
        char *ptr = base;
        for (size_t i_ = 0; i_ < len_ / kPageSz; i_++) {
            size_t offset_ = i_ * kPageSz;
            if (mmap(ptr,
                     kPageSz,
                     PROT_READ | PROT_WRITE,
                     MAP_FIXED | MAP_SHARED,
                     fd_,
                     offset_)
                == MAP_FAILED)
            {
                fprintf(
                    stderr,
                    "pma: failed to create %zu-byte mapping for %s at %p: %s\n",
                    kPageSz,
                    path,
                    ptr,
                    strerror(errno));
                // Unmap already-mapped mappings.
                munmap(ptr + kPageSz, offset_);
                goto close_fd;
            }
            ptr -= kPageSz;
        }
    } else if (mmap(base,
                    len_,
                    PROT_READ | PROT_WRITE,
                    MAP_FIXED | MAP_SHARED,
                    fd_,
                    0)
               == MAP_FAILED)
    {
        fprintf(stderr,
                "pma: failed to create %zu-byte mapping for %s at %p: %s\n",
                len_,
                path,
                base,
                strerror(errno));
        goto close_fd;
    }

    *len = len_;
    *fd  = fd_;
    return 0;

close_fd:
    close(fd_);
fail:
    return -1;
}

//==============================================================================
// FUNCTIONS
//==============================================================================

pma_t *
pma_init(void *base, size_t len, const char *heap_file, const char *stack_file)
{
    assert(kPageSz % sysconf(_SC_PAGESIZE) == 0);
    assert((uintptr_t)base % kPageSz == 0);
    assert(len % kPageSz == 0);

    // If we can't install a handler for SIGSEGV, it's likely because the
    // platform we're running on doesn't support catching SIGSEGV. See sigsegv.h
    // for more info.
    if (sigsegv_install_handler(handle_page_fault_) == -1) {
        fprintf(stderr, "pma: failed to install the page fault handler\n");
        return NULL;
    }

    void  *heap_start = base;
    size_t heap_len;
    int    heap_fd;
    // Failed to map non-NULL heap file.
    if (map_file_(heap_file, heap_start, false, &heap_len, &heap_fd) == -1) {
        return NULL;
    }

    void  *stack_start = (char *)heap_start + len;
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
              .heap_len    = heap_len,
              .stack_len   = stack_len,
              .heap_fd     = heap_fd,
              .stack_fd    = stack_fd,
              .max_sz      = 0,
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
{
    if (!pma) {
        return;
    }

    munmap(pma->heap_start, pma->heap_len);
    if (pma->heap_fd != -1) {
        close(pma->heap_fd);
    }

    munmap(pma->stack_start, pma->stack_len);
    if (pma->stack_fd != -1) {
        close(pma->stack_fd);
    }
}
