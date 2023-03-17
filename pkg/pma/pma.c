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
#include "wal.h"

#ifdef PMA_TEST
#    define inline_
#    define static_
#else
#    define inline_ inline
#    define static_ static
#endif

#define PMA_DEBUG

//==============================================================================
// CONSTANTS

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

/// Extension appended to a backing file to create that file's wal name.
static_ const char kWriteAheadLogExtension[] = ".wal";

//==============================================================================
// GLOBAL VARIABLES

/// Global libsigsegv dispatcher.
static_ sigsegv_dispatcher dispatcher;

//==============================================================================
// STATIC FUNCTIONS

/// Determine the 0-based page index of an address within the bounds of a PMA
/// relative to the start of the heap.
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

static_ int
center_guard_page_(pma_t *pma);

/// Handle a page fault within the bounds of a PMA according to the libsigsegv
/// protocol. See sigsegv_area_handler_t in sigsegv.h for more info.
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

/// Map a file into memory at a specific address. Can also be used to create an
/// anonymous, rather than file-backed, mapping by passing in a NULL path.
///
/// @param[in]     path        Path to the backing file. If NULL, an anonymous
///                            mapping is created.
/// @param[in]     base        Base address of the new mapping.
/// @param[in]     grows_down  true if the mapping should grow downward in
///                            memory.
/// @param[in]     pma         PMA this mapping will belong to.
/// @param[in,out] len         Populated with the length of the new mapping. If
///                            path is NULL, this parameter can also be used to
///                            supply a non-default length for an anonymous
///                            mapping.
/// @param[out]    fd          Populated with the file descriptor of the opened
///                            backing file. -1 if path is NULL.
///
/// @return 0   Successfully created a new mapping.
/// @return -1  Failed to create a new mapping.
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
/// @param[in] addr  Address within the page in question. Must be within the
///                  bounds of the PMA.
/// @param[in] pma   PMA the page in question belongs to.
static_ inline_ page_status_t
page_status_(const void *addr, const pma_t *pma)
{
    size_t  pg_idx    = addr_to_page_idx_(addr, pma);
    size_t  entry_idx = pg_idx / kPagesPerEntry;
    size_t  bit_idx   = (pg_idx % kPagesPerEntry) * kBitsPerPage;
    uint8_t status    = (pma->pg_status[entry_idx] >> bit_idx) & PS_MASK;
    return status;
}

/// Set the status of the page surrounding an address. To get rather than set,
/// see page_status_().
///
/// @param[in] addr    Address within the page in question. Must be within the
///                    bounds of the PMA.
/// @param[in] status  New status of the page in question. Must not be PS_MASK.
/// @param[in] pma     PMA the page in question belongs to.
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
/// @param[in] addr    Address within the first page in question. Must be within
///                    the bounds of the PMA.
/// @param[in] pg_cnt  Number of pages in the range. The entirety of the page
///                    range must be within the bounds of the PMA.
/// @param[in] status  New status of the page range in question. Must not be
///                    PS_MASK.
/// @param[in] pma     PMA the page range in question belongs to.
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

/// Sync changes made to either the heap or stack since the last call to
/// pma_sync().
///
/// @param[in] path        Path to backing file. If NULL, this function is a
///                        no-op.
/// @param[in] base        Base address of the file-backed mapping.
/// @param[in] grows_down  true if the mapping grows downward in memory.
/// @param[in] pma         PMA this mapping belongs to.
/// @param[in] len         Number of bytes at the beginning of the mapping to be
///                        synced to the backing file. Must be a multiple of
///                        kPageSz.
/// @param[in] fd          Open file descriptor of the backing file.
///
/// @return 0   Synced changes successfully.
/// @return -1  Failed to sync changes.
static_ int
sync_file_(const char *path,
           void       *base,
           bool        grows_down,
           pma_t      *pma,
           size_t      len,
           int         fd);

/// Get the total length in bytes of a PMA.
///
/// @param[in] pma
static_ inline_ size_t
total_len_(const pma_t *pma)
{
    assert(pma);
    return (size_t)(pma->stack_start - pma->heap_start);
}

static_ int
center_guard_page_(pma_t *pma)
{
    size_t heap_len  = 0;
    size_t stack_len = 0;
    if (pma->len_getter(&heap_len, &stack_len) == -1) {
        fprintf(stderr,
                "pma: failed to determine length of heap and stack\r\n");
        return -1;
    }

    size_t free_len   = total_len_(pma) - (heap_len + stack_len);
    char  *free_start = (char *)pma->heap_start + pma->heap_len;
    void  *guard_pg   = free_start + round_up(free_len / 2, kPageSz);

    if (mmap(guard_pg,
             kPageSz,
             PROT_NONE,
             MAP_ANON | MAP_FIXED | MAP_PRIVATE,
             -1,
             0)
        == MAP_FAILED)
    {
        fprintf(stderr,
                "pma: failed to place %zu-byte guard page at %p: %s\r\n",
                kPageSz,
                guard_pg,
                strerror(errno));
        return -1;
    }
    set_page_status_(guard_pg, PS_MAPPED_INACCESSIBLE, pma);
    pma->guard_pg = guard_pg;
    return 0;
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
                        PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_FIXED | MAP_PRIVATE,
                        -1,
                        0))
            {
                fprintf(stderr,
                        "pma: failed to create %zu-byte anonymous mapping at "
                        "%p: %s\r\n",
                        kPageSz,
                        fault_addr,
                        strerror(errno));
                return 0;
            }
            set_page_status_(fault_addr, PS_MAPPED_DIRTY, pma);
            break;
        case PS_MAPPED_CLEAN:
            if (mprotect(fault_addr, kPageSz, PROT_READ | PROT_WRITE) == -1) {
                fprintf(stderr,
                        "pma: failed to remove write protections from %zu-byte "
                        "page at %p: %s\r\n",
                        kPageSz,
                        fault_addr,
                        strerror(errno));
                return 0;
            }
            set_page_status_(fault_addr, PS_MAPPED_DIRTY, pma);
            break;
        case PS_MAPPED_INACCESSIBLE:
            if (center_guard_page_(pma) == -1) {
                fprintf(stderr,
                        "pma: hit guard page at %p: out of memory\r\n",
                        fault_addr);
                if (pma->oom_handler) {
                    pma->oom_handler(fault_addr);
                }
                return 0;
            }
            if (mprotect(fault_addr, kPageSz, PROT_READ) == -1) {
                fprintf(stderr,
                        "pma: failed to grant read protections to %zu-byte "
                        "page at %p: %s\r\n",
                        kPageSz,
                        fault_addr,
                        strerror(errno));
                return 0;
            }
            set_page_status_(fault_addr, PS_MAPPED_CLEAN, pma);
            break;
    }
    return 1;
}

static_ int
handle_sigsegv_(void *fault_addr, int serious)
{
    int rc = sigsegv_dispatch(&dispatcher, fault_addr);
#ifdef PMA_DEBUG
    if (rc == 0) {
        volatile int unused = rc;
    }
#endif
    return rc;
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
        *len = *len == 0 ? kDefaultSz : round_up(*len, kPageSz);
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
            fprintf(stderr,
                    "pma: failed to create %zu-byte anonymous mapping at %p: "
                    "%s\r\n",
                    kDefaultSz,
                    base,
                    strerror(errno));
            goto fail;
        }
        size_t pg_cnt = round_up(kDefaultSz, kPageSz) / kPageSz;
        set_page_status_range_(base, pg_cnt, PS_MAPPED_CLEAN, pma);
        *fd = -1;
        return 0;
    }

    int fd_ = open(path, O_CREAT | O_RDWR, 0644);
    // A parent directory doesn't exist.
    if (fd_ == -1) {
        fprintf(stderr,
                "pma: failed to open %s: %s\r\n",
                path,
                strerror(errno));
        goto fail;
    }

    struct stat buf_;
    if (fstat(fd_, &buf_) == -1) {
        fprintf(stderr,
                "pma: failed to determine length of %s: %s\r\n",
                path,
                strerror(errno));
        goto close_fd;
    }

    if (buf_.st_size == 0) {
        *len = 0;
        *fd  = fd_;
        return 0;
    }
    size_t len_ = round_up(buf_.st_size, kPageSz);

    {
        // If there's a write-ahead log for this file, which can happen if a
        // crash occurs during pma_sync() after the write-ahead log has been
        // created but before it can be applied, we need to apply it to the file
        // before mapping the file.
        char wal_file[strlen(path) + sizeof(kWriteAheadLogExtension)];
        snprintf(wal_file,
                 sizeof(wal_file),
                 "%s%s",
                 path,
                 kWriteAheadLogExtension);
        wal_t wal;
        if (wal_open(wal_file, &wal) == -1 && errno != EEXIST) {
            fprintf(stderr, "pma: failed to open wal file at %s\r\n", wal_file);
            goto close_fd;
        }

        if (wal_apply(&wal, fd_) == -1) {
            fprintf(stderr,
                    "pma: failed to apply wal at %s to %s\r\n",
                    wal_file,
                    path);
            // We're leaking the fd in wal here.
            goto close_fd;
        }

        wal_destroy(&wal);
    }

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
                fprintf(stderr,
                        "pma: failed to create %zu-byte mapping for %s at %p: "
                        "%s\r\n",
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
            fprintf(
                stderr,
                "pma: failed to create %zu-byte mapping for %s at %p: %s\r\n",
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

static_ int
sync_file_(const char *path,
           void       *base,
           bool        grows_down,
           pma_t      *pma,
           size_t      len,
           int         fd)
{
    if (!path) {
        return 0;
    }
    assert(base);
    assert(pma);

    char wal_file[strlen(path) + sizeof(kWriteAheadLogExtension)];
    snprintf(wal_file, sizeof(wal_file), "%s%s", path, kWriteAheadLogExtension);
    wal_t wal;
    if (wal_open(wal_file, &wal) == -1) {
        fprintf(stderr, "pma: failed to open wal file at %s\r\n", wal_file);
        return -1;
    }

    // Determine largest possible page index.
    size_t total = total_len_(pma);
    assert(total % kPageSz == 0);
    size_t  max_idx = (total / kPageSz) - 1;

    char   *ptr  = grows_down ? base - kPageSz : base;
    ssize_t step = grows_down ? -kPageSz : kPageSz;
    for (size_t i = 0; i < len; i += kPageSz) {
        page_status_t status = page_status_(ptr, pma);
        assert(status != PS_UNMAPPED);
        if (status == PS_MAPPED_DIRTY) {
            size_t pg_idx = addr_to_page_idx_(ptr, pma);
            pg_idx        = grows_down ? max_idx - pg_idx : pg_idx;
            if (wal_append(&wal, pg_idx, ptr) == -1) {
                fprintf(stderr,
                        "pma: failed to append %zu-byte page with index %zu "
                        "and address %p to wal at %s\r\n",
                        kPageSz,
                        pg_idx,
                        ptr,
                        wal_file);
                return -1;
            }
        }
        ptr += step;
    }

    if (wal_sync(&wal) == -1) {
        fprintf(stderr, "pma: failed to sync wal for %s\r\n", path);
        return -1;
    }

    if (wal_apply(&wal, fd) == -1) {
        fprintf(stderr,
                "pma: failed to apply wal at %s to %s\r\n",
                wal_file,
                path);
        return -1;
    }

    wal_destroy(&wal);

    return 0;
}

//==============================================================================
// FUNCTIONS

pma_t *
pma_load(void         *base,
         size_t        len,
         const char   *heap_file,
         const char   *stack_file,
         len_getter_t  len_getter,
         oom_handler_t oom_handler)
{
#ifndef HAVE_SIGSEGV_RECOVERY
    fprintf(stderr, "pma: this platform doesn't support handling SIGSEGV\r\n");
    return NULL;
#endif
    assert(kPageSz % sysconf(_SC_PAGESIZE) == 0);
    if (!len_getter) {
        return NULL;
    }
    if ((uintptr_t)base % kPageSz != 0) {
        fprintf(stderr,
                "pma: base address %p is not a multiple of %zu\r\n",
                base,
                kPageSz);
        return NULL;
    }
    len = round_up(len, kPageSz);

    pma_t *pma = malloc(sizeof(*pma));

    if (heap_file) {
        pma->heap_file = strdup(heap_file);
    }
    if (stack_file) {
        pma->stack_file = strdup(stack_file);
    }

    void *heap_start  = base;
    void *stack_start = (char *)heap_start + len;
    pma->heap_start   = heap_start;
    pma->stack_start  = stack_start;

    size_t num_pgs      = round_up(len, kPageSz) / kPageSz;
    size_t bits_needed  = round_up(kBitsPerPage * num_pgs, kBitsPerByte);
    size_t bytes_needed = bits_needed / kBitsPerByte;
    pma->num_pgs        = num_pgs;
    pma->pg_status      = calloc(bytes_needed, sizeof(*pma->pg_status));

    pma->heap_len = 0;
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

    pma->stack_len = 0;
    // Failed to map non-NULL stack file.
    if (map_file_(stack_file,
                  stack_start,
                  true,
                  pma,
                  &pma->stack_len,
                  &pma->stack_fd)
        == -1)
    {
        goto unmap_heap;
    }

    if ((pma->heap_len + pma->stack_len) > len) {
        fprintf(stderr, "pma: heap and stack overlap\r\n");
        goto unmap_stack;
    }

    sigsegv_init(&dispatcher);
    // This should never fail when HAVE_SIGSEGV_RECOVERY is defined.
    assert(sigsegv_install_handler(handle_sigsegv_) == 0);
    pma->sigsegv_ticket = sigsegv_register(&dispatcher,
                                           base,
                                           len,
                                           handle_page_fault_,
                                           (void *)pma);
    pma->len_getter     = len_getter;
    pma->guard_pg       = NULL;
    pma->max_sz         = 0;

    if (center_guard_page_(pma) == -1) {
        fprintf(stderr, "pma: failed to initialize the guard page\r\n");
        goto unmap_stack;
    }

    return pma;
unmap_stack:
    munmap((char *)pma->stack_start - pma->stack_len, pma->stack_len);
    close(pma->stack_fd);
unmap_heap:
    munmap(pma->heap_start, pma->heap_len);
    close(pma->heap_fd);
free_pma:
    if (heap_file) {
        free((void *)pma->heap_file);
    }
    if (stack_file) {
        free((void *)pma->stack_file);
    }
    free(pma);
    return NULL;
}

int
pma_sync(pma_t *pma)
{
    if (!pma) {
        errno = EINVAL;
        return -1;
    }

    size_t heap_len  = 0;
    size_t stack_len = 0;
    if (pma->len_getter(&heap_len, &stack_len) == -1) {
        fprintf(stderr,
                "pma: failed to determine length of heap and stack\r\n");
        return -1;
    }

    heap_len  = round_up(heap_len, kPageSz);
    stack_len = round_up(stack_len, kPageSz);

    if (pma->heap_fd != -1) {
        if (sync_file_(pma->heap_file,
                       pma->heap_start,
                       false,
                       pma,
                       heap_len,
                       pma->heap_fd)
            == -1)
        {
            fprintf(stderr,
                    "pma: failed to sync heap changes to %s\r\n",
                    pma->heap_file);
            return -1;
        }

        // The heap shrunk, so shrink the backing file.
        if (heap_len < pma->heap_len) {
            if (ftruncate(pma->heap_fd, heap_len) == -1) {
                fprintf(stderr,
                        "pma: failed to truncate %s from %zu bytes to %zu "
                        "bytes\r\n",
                        pma->heap_file,
                        pma->heap_len,
                        heap_len);
                return -1;
            }
        }
        pma->heap_len = heap_len;
        close(pma->heap_fd);
        pma->heap_fd = -1;
    }

    if (pma->stack_fd != -1) {
        if (sync_file_(pma->stack_file,
                       pma->stack_start,
                       true,
                       pma,
                       stack_len,
                       pma->stack_fd)
            == -1)
        {
            fprintf(stderr,
                    "pma: failed to sync stack changes to %s\r\n",
                    pma->stack_file);
            return -1;
        }

        // The stack shrunk, so shrink the backing file.
        if (stack_len < pma->stack_len) {
            if (ftruncate(pma->stack_fd, stack_len) == -1) {
                fprintf(stderr,
                        "pma: failed to truncate %s from %zu bytes to %zu "
                        "bytes\r\n",
                        pma->stack_file,
                        pma->stack_len,
                        stack_len);
                return -1;
            }
        }
        pma->stack_len = stack_len;
        close(pma->stack_fd);
        pma->stack_fd = -1;
    }

    // Remap heap.
    if (map_file_(pma->heap_file,
                  pma->heap_start,
                  false,
                  pma,
                  &pma->heap_len,
                  &pma->heap_fd)
        == -1)
    {
        fprintf(stderr, "pma: failed to remap %s\r\n", pma->heap_file);
        return -1;
    }

    // Remap stack.
    if (map_file_(pma->stack_file,
                  pma->stack_start,
                  true,
                  pma,
                  &pma->stack_len,
                  &pma->stack_fd)
        == -1)
    {
        fprintf(stderr, "pma: failed to remap %s\r\n", pma->stack_file);
        munmap(pma->heap_start, pma->heap_len);
        close(pma->heap_fd);
        pma->heap_fd = -1;
        return -1;
    }

    return 0;
}

void
pma_unload(pma_t *pma)
{
    if (!pma) {
        return;
    }

    munmap(pma->heap_start, total_len_(pma));

    if (pma->heap_fd != -1) {
        close(pma->heap_fd);
    }

    if (pma->stack_fd != -1) {
        close(pma->stack_fd);
    }

    free(pma->pg_status);

    sigsegv_unregister(&dispatcher, pma->sigsegv_ticket);

    // TODO: free pma.
}

#undef inline_
#undef static_
