/// @file

#include "pma.c"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "page.h"
#include "util.h"

//==============================================================================
// GLOBAL VARIABLES

static size_t _heap_len;
static size_t _stack_len;

//==============================================================================
// STATIC FUNCTIONS

static int
_len_getter(size_t *heap_len, size_t *stack_len)
{
    *heap_len  = _heap_len;
    *stack_len = _stack_len;
    return 0;
}

static void
_new_file(const char *path, char ch, size_t pg_cnt)
{
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    assert(fd != -1);

    char buf[kPageSz];
    memset(buf, ch, sizeof(buf));
    for (size_t i = 0; i < pg_cnt; i++) {
        assert(write_all(fd, buf, sizeof(buf)) == 0);
    }

    memset(buf, 0, sizeof(buf));

    assert(lseek(fd, 0, SEEK_SET) == 0);
    for (size_t i = 0; i < pg_cnt; i++) {
        assert(read_all(fd, buf, sizeof(buf)) == 0);
        for (size_t j = 0; j < sizeof(buf); j++) {
            assert(buf[j] == ch);
        }
    }

    assert(close(fd) == 0);
}

static void
_oom_handler(void *fault_addr)
{}

//==============================================================================
// STATIC FUNCTION TESTS

static void
_test_addr_to_page_idx(void)
{
    void               *base = (void *)0x200000000;
    static const size_t kLen = 1 << 20;
    _heap_len                = kPageSz;
    _stack_len               = kPageSz;
    pma_t *pma = pma_load(base, kLen, NULL, NULL, _len_getter, _oom_handler);
    assert(pma);

    {
        size_t idx_ = _addr_to_page_idx(base, pma);
        assert(idx_ == 0);
    }

    {
        static const int64_t kOffset = 2;
        void                *addr    = (char *)base + kOffset * kPageSz;
        size_t               idx_    = _addr_to_page_idx(addr, pma);
        assert(idx_ == kOffset);
    }

    pma_unload(pma);
}

//==============================================================================
// FUNCTION TESTS

static void
_test_pma(void)
{
    // Anonymous arena.
    {
        void  *base = (void *)0x200000000;
        size_t len  = 1 << 20;
        pma_t *pma = pma_load(base, len, NULL, NULL, _len_getter, _oom_handler);
        assert(pma);
        assert(pma->heap_start == base);
        assert(pma->stack_start == (char *)base + len);
        assert(pma->heap_len == kPageSz);
        assert(pma->stack_len == kPageSz);
        assert(pma->heap_fd == -1);
        assert(pma->stack_fd == -1);

        assert(_page_status((char *)base + kPageSz, pma) == PS_UNMAPPED);

        void *addr;
        char  ch;

        // Write to the heap.
        addr = base;
        assert(_page_status(addr, pma) == PS_MAPPED_CLEAN);
        ch            = *(char *)addr;
        *(char *)addr = 'h';
        assert(_page_status(addr, pma) == PS_MAPPED_DIRTY);

        // Write to the stack.
        addr = (char *)base + len - 1;
        assert(_page_status(addr, pma) == PS_MAPPED_CLEAN);
        ch            = *(char *)addr;
        *(char *)addr = 'i';
        assert(_page_status(addr, pma) == PS_MAPPED_DIRTY);

        pma_unload(pma);
    }

    // File-backed arena with empty files.
    {
        void             *base         = (void *)0x200000000;
        size_t            len          = 1 << 20;
        static const char kHeapFile[]  = "/tmp/nonexistent-heap.bin";
        static const char kStackFile[] = "/tmp/nonexistent-stack.bin";
        pma_t            *pma          = pma_load(base,
                              len,
                              kHeapFile,
                              kStackFile,
                              _len_getter,
                              _oom_handler);
        assert(pma);
        assert(pma->heap_start == base);
        assert(pma->stack_start == (char *)base + len);
        assert(pma->heap_len == 0);
        assert(pma->stack_len == 0);
        assert(pma->heap_fd != -1);
        assert(pma->stack_fd != -1);

        void *addr;
        // Mark as volatile so the compiler doesn't optimize out the assignments
        // to ch below.
        volatile char ch;

        // Write to the heap.
        addr = base;
        assert(_page_status(addr, pma) == PS_UNMAPPED);
        ch = *(char *)addr;
        assert(_page_status(addr, pma) == PS_MAPPED_DIRTY);
        *(char *)addr = 'h';
        assert(_page_status(addr, pma) == PS_MAPPED_DIRTY);

        // Write to the stack.
        addr = (char *)base + len - 1;
        assert(_page_status(addr, pma) == PS_UNMAPPED);
        ch = *(char *)addr;
        assert(_page_status(addr, pma) == PS_MAPPED_DIRTY);
        *(char *)addr = 'i';
        assert(_page_status(addr, pma) == PS_MAPPED_DIRTY);

        _heap_len  = kPageSz;
        _stack_len = kPageSz;
        assert(pma_sync(pma) == 0);

        pma_unload(pma);
        assert(unlink(kHeapFile) == 0);
        assert(unlink(kStackFile) == 0);
    }

    // File-backed arena with non-empty files.
    {
        void               *base         = (void *)0x200000000;
        size_t              len          = GiB(2);
        static const char   kHeapFile[]  = "/tmp/definitely-exists-heap.bin";
        static const char   kStackFile[] = "/tmp/definitely-exists-stack.bin";
        static const size_t kHeapFileSz  = GiB(1);
        static const size_t kStackFileSz = MiB(1);
        _new_file(kHeapFile, 'p', kHeapFileSz / kPageSz);
        _new_file(kStackFile, 'm', kStackFileSz / kPageSz);
        _heap_len  = kHeapFileSz;
        _stack_len = kStackFileSz;
        pma_t *pma = pma_load(base,
                              len,
                              kHeapFile,
                              kStackFile,
                              _len_getter,
                              _oom_handler);
        assert(pma);
        assert(pma->heap_start == base);
        assert(pma->stack_start == (char *)base + len);
        assert(pma->heap_len == kHeapFileSz);
        assert(pma->stack_len == kStackFileSz);
        assert(pma->heap_fd != -1);
        assert(pma->stack_fd != -1);

        char *addr;

        // Description of heap change #0.
        char  *hpage0           = base;
        size_t hpage0_change_sz = 1;
        char   hpage0_ch        = 'h';

        // Description of heap change #1.
        static const size_t kNewHeapSz       = 107 * kPageSz;
        char               *hpage1           = base + kNewHeapSz - kPageSz;
        size_t              hpage1_change_sz = kPageSz;
        char                hpage1_ch        = 'i';

        // Description of stack change #0.
        char  *spage0           = (char *)base + len - 1;
        size_t spage0_change_sz = 1;
        char   spage0_ch        = 'a';

        // Description of stack change #1.
        char  *spage1           = (char *)base + len - 3 * kPageSz;
        size_t spage1_change_sz = 2 * kPageSz;
        char   spage1_ch        = 'y';

        // Make heap change #0.
        assert(_page_status(hpage0, pma) == PS_MAPPED_CLEAN);
        memset(hpage0, hpage0_ch, hpage0_change_sz);
        assert(_page_status(hpage0, pma) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < hpage0_change_sz; i++) {
            assert(hpage0[i] == hpage0_ch);
        }

        // Make heap change #1.
        assert(_page_status(hpage1, pma) == PS_MAPPED_CLEAN);
        memset(hpage1, hpage1_ch, hpage1_change_sz);
        assert(_page_status(hpage1, pma) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < hpage1_change_sz; i++) {
            assert(hpage1[i] == hpage1_ch);
        }

        // Make stack change #0.
        assert(_page_status(spage0, pma) == PS_MAPPED_CLEAN);
        memset(spage0, spage0_ch, spage0_change_sz);
        assert(_page_status(spage0, pma) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < spage0_change_sz; i++) {
            assert(spage0[i] == spage0_ch);
        }

        // Make stack change #1.
        assert(_page_status(spage1, pma) == PS_MAPPED_CLEAN);
        assert(_page_status(spage1 + kPageSz, pma) == PS_MAPPED_CLEAN);
        memset(spage1, spage1_ch, spage1_change_sz);
        assert(_page_status(spage1, pma) == PS_MAPPED_DIRTY);
        assert(_page_status(spage1 + kPageSz, pma) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < spage1_change_sz; i++) {
            assert(spage1[i] == spage1_ch);
        }

        // Sync.
        _heap_len  = kNewHeapSz;
        _stack_len = kPageSz;
        assert(pma_sync(pma) == 0);

        // Removes all mappings.
        pma_unload(pma);

        // Re-establishes all mappings.
        pma = pma_load(base,
                       len,
                       kHeapFile,
                       kStackFile,
                       _len_getter,
                       _oom_handler);
        assert(pma);
        assert(pma->heap_start == base);
        assert(pma->stack_start == (char *)base + len);
        assert(pma->heap_len == kNewHeapSz);
        assert(pma->stack_len == kPageSz);
        assert(pma->heap_fd != -1);
        assert(pma->stack_fd != -1);

        // Check heap change #0.
        assert(_page_status(hpage0, pma) == PS_MAPPED_CLEAN);
        for (size_t i = 0; i < hpage0_change_sz; i++) {
            assert(hpage0[i] == hpage0_ch);
        }
        assert(_page_status(hpage0, pma) == PS_MAPPED_CLEAN);

        // Check heap change #1.
        assert(_page_status(hpage1, pma) == PS_MAPPED_CLEAN);
        for (size_t i = 0; i < hpage1_change_sz; i++) {
            assert(hpage1[i] == hpage1_ch);
        }
        assert(_page_status(hpage1, pma) == PS_MAPPED_CLEAN);

        // Check stack change #0.
        assert(_page_status(spage0, pma) == PS_MAPPED_CLEAN);
        for (size_t i = 0; i < spage0_change_sz; i++) {
            assert(spage0[i] == spage0_ch);
        }

        // Check stack change #1.
        assert(_page_status(spage1, pma) == PS_UNMAPPED);
        assert(_page_status(spage1 + kPageSz, pma) == PS_UNMAPPED);

        assert(unlink(kHeapFile) == 0);
        assert(unlink(kStackFile) == 0);
    }
}

int
main(int argc, char *argv[])
{
    _test_addr_to_page_idx();
    _test_pma();
    return 0;
}
