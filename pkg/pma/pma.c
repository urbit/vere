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

#include "page.h"
#include "sigsegv.h"
#include "util.h"

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
static_ const size_t kBitsPerByte = 8;

/// Number of bits required to track the status of a page.
static_ const size_t kBitsPerPage = 2;

/// Number of pages whose status can be tracked in a single byte.
static_ const size_t kPagesPerByte = kBitsPerByte / kBitsPerPage;

/// Number of bytes in a single entry of the pg_status array of pma_t.
static_ const size_t kBytesPerEntry = sizeof(*((pma_t *)NULL)->pg_status);

/// Number of pages whose status can be tracked by a single entry in the
/// pg_status array of pma_t.
static_ const size_t kPagesPerEntry = kBytesPerEntry * kPagesPerByte;

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
addr_to_page_idx_(const void *addr, const pma_t *pma)
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
/// @param[in]  pma
/// @param[out] len
/// @param[out] fd
static_ int
map_file_(const char *path,
          void       *base,
          bool        grows_down,
          pma_t      *pma,
          size_t     *len,
          int        *fd);

/// Get the status of the page surrounding an address. To set rather than get,
/// see set_page_status_().
///
/// @param[in] addr
/// @param[in] pma
static_ inline_ page_status_t
page_status_(const void *addr, const pma_t *pma)
{
    size_t  pg_idx    = addr_to_page_idx_(addr, pma);
    size_t  entry_idx = pg_idx / kPagesPerEntry;
    size_t  bit_idx   = (pg_idx % kPagesPerEntry) * kBitsPerPage;
    uint8_t status    = (pma->pg_status[entry_idx] >> bit_idx) & PS_MASK;
    assert(status != PS_MASK);
    return status;
}

/// Set the status of the page surrounding an address. To get rather than set,
/// see page_status_().
///
/// @param[in] addr
/// @param[in] status
/// @param[in] pma
static_ inline_ void
set_page_status_(const void *addr, page_status_t status, const pma_t *pma)
{
    size_t  pg_idx    = addr_to_page_idx_(addr, pma);
    size_t  entry_idx = pg_idx / kPagesPerEntry;
    size_t  bit_idx   = (pg_idx % kPagesPerEntry) * kBitsPerPage;
    uint8_t entry     = pma->pg_status[entry_idx];
    pma->pg_status[entry_idx]
        = (entry & ~(PS_MASK << bit_idx)) | (status << bit_idx);
}

/// Set the status of the page range starting at an address. To set the page
/// status of a single page, see set_page_status_().
///
/// @param[in] addr
/// @param[in] pg_cnt  Number of pages in the range.
/// @param[in] status
/// @param[in] pma
static_ inline_ void
set_page_status_range_(const void   *addr,
                       size_t        pg_cnt,
                       page_status_t status,
                       const pma_t  *pma)
{
    for (size_t i = 0; i < pg_cnt * kPageSz; i += kPageSz) {
        set_page_status_((char *)addr + i, status, pma);
    }
}

static_ int
handle_page_fault_(void *fault_addr, void *user_arg)
{
    fault_addr = (void *)round_down((uintptr_t)fault_addr, kPageSz);
    pma_t *pma = user_arg;
    switch (page_status_(fault_addr, pma)) {
        case PS_UNMAPPED:
            if (MAP_FAILED
                == mmap(fault_addr,
                        kPageSz,
                        PROT_READ,
                        MAP_ANON | MAP_FIXED | MAP_PRIVATE,
                        -1,
                        0))
            {
                fprintf(stderr,
                        "pma: failed to create %zu-byte anonymous mapping at "
                        "%p: %s\n",
                        kPageSz,
                        fault_addr,
                        strerror(errno));
                return 0;
            }
            set_page_status_(fault_addr, PS_MAPPED_CLEAN, pma);
            break;
        case PS_MAPPED_CLEAN:
            if (mprotect(fault_addr, kPageSz, PROT_READ | PROT_WRITE) == -1) {
                fprintf(stderr,
                        "pma: failed to remove write protections from %zu-byte "
                        "page at %p: %s\n",
                        kPageSz,
                        fault_addr,
                        strerror(errno));
                return 0;
            }
            set_page_status_(fault_addr, PS_MAPPED_DIRTY, pma);
            break;
    }
    return 1;
}

static_ int
handle_sigsegv_(void *fault_addr, int serious)
{
    return sigsegv_dispatch(&dispatcher, fault_addr);
}

static_ int
map_file_(const char *path,
          void       *base,
          bool        grows_down,
          pma_t      *pma,
          size_t     *len,
          int        *fd)
{
    if (!path) {
        static const size_t kDefaultSz = kPageSz;
        if (grows_down) {
            base = (char *)base - kDefaultSz;
        }
        if (mmap(base,
                 kDefaultSz,
                 PROT_READ,
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
        size_t pg_cnt = round_up(kDefaultSz, kPageSz) / kPageSz;
        set_page_status_range_(base, pg_cnt, PS_MAPPED_CLEAN, pma);
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

    if (buf_.st_size == 0) {
        *len = 0;
        *fd  = fd_;
        return 0;
    }
    size_t len_ = round_up(buf_.st_size, kPageSz);

    // We have to map stacks a page at a time because a stack's backing file has
    // its first page at offset zero, which belongs at the highest address.
    if (grows_down) {
        char *ptr = base;
        for (size_t i_ = 0; i_ < len_ / kPageSz; i_++) {
            ptr -= kPageSz;
            size_t offset_ = i_ * kPageSz;
            if (mmap(ptr,
                     kPageSz,
                     PROT_READ,
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
            set_page_status_(ptr, PS_MAPPED_CLEAN, pma);
        }
    } else {
        if (mmap(base, len_, PROT_READ, MAP_FIXED | MAP_PRIVATE, fd_, 0)
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
        set_page_status_range_(base, len_ / kPageSz, PS_MAPPED_CLEAN, pma);
    }

    *len = len_;
    *fd  = fd_;
    return 0;

close_fd:
    close(fd_);
fail:
    // TODO: mark all pages as unmapped.
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

    pma_t *pma = malloc(sizeof(*pma));

    void  *heap_start  = base;
    void  *stack_start = (char *)heap_start + len;
    pma->heap_start    = heap_start;
    pma->stack_start   = stack_start;

    size_t num_pgs      = round_up(len, kPageSz) / kPageSz;
    size_t bits_needed  = round_up(kBitsPerPage * num_pgs, kBitsPerByte);
    size_t bytes_needed = bits_needed / kBitsPerByte;
    pma->num_pgs        = num_pgs;
    pma->pg_status      = calloc(bytes_needed, sizeof(*pma->pg_status));

    // Failed to map non-NULL heap file.
    if (map_file_(heap_file,
                  heap_start,
                  false,
                  pma,
                  &pma->heap_len,
                  &pma->heap_fd)
        == -1)
    {
        goto free_pma;
    }

    // Failed to map non-NULL stack file.
    if (map_file_(stack_file,
                  stack_start,
                  true,
                  pma,
                  &pma->stack_len,
                  &pma->stack_fd)
        == -1)
    {
        munmap(pma->heap_start, pma->heap_len);
        close(pma->heap_fd);
        goto free_pma;
    }

    sigsegv_init(&dispatcher);
    // This should never fail when HAVE_SIGSEGV_RECOVERY is defined.
    assert(sigsegv_install_handler(handle_sigsegv_) == 0);
    pma->sigsegv_ticket = sigsegv_register(&dispatcher,
                                           base,
                                           len,
                                           handle_page_fault_,
                                           (void *)pma);
    pma->max_sz         = 0;

    return pma;
free_pma:
    free(pma);
    return NULL;
}

int
pma_sync(pma_t *pma, size_t heap_len, size_t stack_len)
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
