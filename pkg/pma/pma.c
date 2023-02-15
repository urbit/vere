#include "pma.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sigsegv.h"

#ifdef PMA_TEST
#    define inline_
#    define static_
#else
#    define inline_ inline
#    define static_ static
#endif

//==============================================================================
// CONSTANTS
//==============================================================================

/// Number of bits in a byte.
static const size_t kBitsPerByte = 8;

/// Number of bits required to track the status of a page.
static const size_t kBitsPerPage = 2;

/// Number of pages whose status can be tracked in a single byte.
static const size_t kPagesPerByte = kBitsPerByte / kBitsPerPage;

/// Number of bytes in a single entry of the pg_status array of pma_t.
static const size_t kBytesPerEntry = sizeof(*((pma_t *)NULL)->pg_status);

/// Number of pages whose status can be tracked by a single entry in the
/// pg_status array of pma_t.
static const size_t kPagesPerEntry = kBytesPerEntry * kPagesPerByte;

//==============================================================================
// TYPES
//==============================================================================

enum page_status {
    PS_UNMAPPED     = 0x0,
    PS_MAPPED_CLEAN = 0x1,
    PS_MAPPED_DIRTY = 0x2,
    PS_MASK         = 0x3,
};
typedef enum page_status page_status_t;

//==============================================================================
// GLOBAL VARIABLES
//==============================================================================

/// Global libsigsegv dispatcher.
static_ sigsegv_dispatcher dispatcher;

//==============================================================================
// STATIC FUNCTIONS
//==============================================================================

/// Determine the 0-based page index of an address relative to the start of the
/// heap.
///
/// @param[in] addr  Address to determine page index for.
/// @param[in] pma
static_ inline_ size_t
addr_to_page_idx_(void *addr, const pma_t *pma)
{
    assert(pma);
    assert(pma->heap_start <= addr && addr < pma->stack_start);
    return (addr - pma->heap_start) / kPageSz;
}

/// Handle a page fault according to the libsigsegv protocol. See
/// sigsegv_area_handler_t in sigsegv.h for more info.
///
/// @param[in] fault_addr
/// @param[in] user_arg
static_ int
handle_page_fault_(void *fault_addr, void *user_arg);

/// Handle SIGSEGV according to the libsigsegv protocol. See sigsegv_handler_t
/// in sigsegv.h for more info.
///
/// @param[in] fault_addr
/// @param[in] serious
static_ int
handle_sigsegv_(void *fault_addr, int serious);

/// @param[in]  path
/// @param[in]  base
/// @param[in]  grows_down
/// @param[out] len
/// @param[out] fd
static_ int
map_file_(const char *path, void *base, bool grows_down, size_t *len, int *fd);

/// Get the status of the page surrounding an address.
///
/// @param[in] addr
/// @param[in] pma
static_ inline_ uint8_t
page_status_(void *addr, const pma_t *pma)
{
    size_t  pg_idx    = addr_to_page_idx_(addr, pma);
    size_t  entry_idx = pg_idx / kPagesPerEntry;
    size_t  bit_idx   = (pg_idx % kPagesPerEntry) * kBitsPerPage;
    uint8_t status    = (pma->pg_status[entry_idx] >> bit_idx) & PS_MASK;
    assert(status != PS_MASK);
    return status;
}

/// Round `x` up to the nearest multiple of `n`, which must be a power of 2.
///
/// @param[in] x
/// @param[in] n
static_ inline_ size_t
round_up_(size_t x, size_t n)
{
    return (x + (n - 1)) & (~(n - 1));
}

static_ int
handle_page_fault_(void *fault_addr, void *user_arg)
{
    assert((uintptr_t)fault_addr % kPageSz == 0);
    if (MAP_FAILED
        == mmap(fault_addr,
                kPageSz,
                PROT_READ | PROT_WRITE,
                MAP_ANON | MAP_FIXED | MAP_PRIVATE,
                -1,
                0))
    {
        fprintf(stderr,
                "pma: failed to create %zu-byte anonymous mapping at %p: %s\n",
                kPageSz,
                fault_addr,
                strerror(errno));
        return 0;
    }
    return 1;
}

static_ int
handle_sigsegv_(void *fault_addr, int serious)
{
    return sigsegv_dispatch(&dispatcher, fault_addr);
}

static_ int
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
                     MAP_FIXED | MAP_PRIVATE,
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
                    MAP_FIXED | MAP_PRIVATE,
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
#ifndef HAVE_SIGSEGV_RECOVERY
    fprintf(stderr, "pma: this platform doesn't support handling SIGSEGV\n");
    return NULL;
#endif
    assert(kPageSz % sysconf(_SC_PAGESIZE) == 0);
    assert((uintptr_t)base % kPageSz == 0);
    assert(len % kPageSz == 0);

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

    size_t   num_pgs      = round_up_(len, kPageSz) / kPageSz;
    size_t   bits_needed  = round_up_(kBitsPerPage * num_pgs, kBitsPerByte);
    size_t   bytes_needed = bits_needed / kBitsPerByte;
    uint8_t *pg_status    = calloc(bytes_needed, sizeof(*pg_status));

    pma_t   *pma = malloc(sizeof(*pma));

    sigsegv_init(&dispatcher);
    // This should never fail when HAVE_SIGSEGV_RECOVERY is defined.
    assert(sigsegv_install_handler(handle_sigsegv_) == 0);
    void *ticket = sigsegv_register(&dispatcher,
                                    base,
                                    len,
                                    handle_page_fault_,
                                    (void *)pma);
    *pma         = (pma_t){
                .heap_start     = heap_start,
                .stack_start    = stack_start,
                .heap_len       = heap_len,
                .stack_len      = stack_len,
                .heap_fd        = heap_fd,
                .stack_fd       = stack_fd,
                .num_pgs        = num_pgs,
                .pg_status      = pg_status,
                .max_sz         = 0,
                .sigsegv_ticket = ticket,
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

    free(pma->pg_status);

    sigsegv_unregister(&dispatcher, pma->sigsegv_ticket);

    // TODO: free pma.
}

#undef static_
