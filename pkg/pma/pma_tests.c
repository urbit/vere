#include "pma.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "page.h"
#include "util.h"

int64_t
addr_to_page_idx_(void *addr, const pma_t *pma);

uint8_t
page_status_(void *addr, const pma_t *pma);

//==============================================================================
// STATIC FUNCTIONS

static void
new_file_(const char *path, char ch, size_t pg_cnt)
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

//==============================================================================
// STATIC FUNCTION TESTS

static void
test_addr_to_page_idx_(void)
{
    void               *base_ = (void *)0x200000000;
    static const size_t kLen  = 1 << 20;
    pma_t              *pma_  = pma_init(base_, kLen, NULL, NULL);
    assert(pma_);

    {
        size_t idx_ = addr_to_page_idx_(base_, pma_);
        assert(idx_ == 0);
    }

    {
        static const int64_t kOffset = 2;
        void                *addr_   = (char *)base_ + kOffset * kPageSz;
        size_t               idx_    = addr_to_page_idx_(addr_, pma_);
        assert(idx_ == kOffset);
    }

    pma_deinit(pma_);
}

//==============================================================================
// FUNCTION TESTS

static void
test_pma_()
{
    // Anonymous arena.
    {
        void  *base_ = (void *)0x200000000;
        size_t len_  = 1 << 20;
        pma_t *pma_  = pma_init(base_, len_, NULL, NULL);
        assert(pma_);
        assert(pma_->heap_start == base_);
        assert(pma_->stack_start == (char *)base_ + len_);
        assert(pma_->heap_len == kPageSz);
        assert(pma_->stack_len == kPageSz);
        assert(pma_->heap_fd == -1);
        assert(pma_->stack_fd == -1);
        assert(pma_->max_sz == 0);

        assert(page_status_((char *)base_ + 2 * kPageSz, pma_) == PS_UNMAPPED);

        void *addr_;
        char  ch;

        // Write to the heap.
        addr_ = base_;
        assert(page_status_(addr_, pma_) == PS_MAPPED_CLEAN);
        ch             = *(char *)addr_;
        *(char *)addr_ = 'h';
        assert(page_status_(addr_, pma_) == PS_MAPPED_DIRTY);

        // Write to the stack.
        addr_ = (char *)base_ + len_ - 1;
        assert(page_status_(addr_, pma_) == PS_MAPPED_CLEAN);
        ch             = *(char *)addr_;
        *(char *)addr_ = 'i';
        assert(page_status_(addr_, pma_) == PS_MAPPED_DIRTY);

        pma_deinit(pma_);
    }

    // File-backed arena with empty files.
    {
        void             *base_        = (void *)0x200000000;
        size_t            len_         = 1 << 20;
        static const char kHeapFile[]  = "/tmp/nonexistent-heap.bin";
        static const char kStackFile[] = "/tmp/nonexistent-stack.bin";
        pma_t            *pma_ = pma_init(base_, len_, kHeapFile, kStackFile);
        assert(pma_);
        assert(pma_->heap_start == base_);
        assert(pma_->stack_start == (char *)base_ + len_);
        assert(pma_->heap_len == 0);
        assert(pma_->stack_len == 0);
        assert(pma_->heap_fd != -1);
        assert(pma_->stack_fd != -1);
        assert(pma_->max_sz == 0);

        void *addr_;
        // Mark as volatile so the compiler doesn't optimize out the assignments
        // to ch below.
        volatile char ch;

        // Write to the heap.
        addr_ = base_;
        assert(page_status_(addr_, pma_) == PS_UNMAPPED);
        ch = *(char *)addr_;
        assert(page_status_(addr_, pma_) == PS_MAPPED_CLEAN);
        *(char *)addr_ = 'h';
        assert(page_status_(addr_, pma_) == PS_MAPPED_DIRTY);

        // Write to the stack.
        addr_ = (char *)base_ + len_ - 1;
        assert(page_status_(addr_, pma_) == PS_UNMAPPED);
        ch = *(char *)addr_;
        assert(page_status_(addr_, pma_) == PS_MAPPED_CLEAN);
        *(char *)addr_ = 'i';
        assert(page_status_(addr_, pma_) == PS_MAPPED_DIRTY);

        assert(pma_sync(pma_, kPageSz, kPageSz) == 0);

        pma_deinit(pma_);
        assert(unlink(kHeapFile) == 0);
        assert(unlink(kStackFile) == 0);
    }

    // File-backed arena with non-empty files.
    {
        void               *base_        = (void *)0x200000000;
        size_t              len_         = GiB(2);
        static const char   kHeapFile[]  = "/tmp/definitely-exists-heap.bin";
        static const char   kStackFile[] = "/tmp/definitely-exists-stack.bin";
        static const size_t kHeapFileSz  = GiB(1);
        static const size_t kStackFileSz = MiB(1);
        new_file_(kHeapFile, 'p', kHeapFileSz / kPageSz);
        new_file_(kStackFile, 'm', kStackFileSz / kPageSz);
        pma_t *pma_ = pma_init(base_, len_, kHeapFile, kStackFile);
        assert(pma_);
        assert(pma_->heap_start == base_);
        assert(pma_->stack_start == (char *)base_ + len_);
        assert(pma_->heap_len == kHeapFileSz);
        assert(pma_->stack_len == kStackFileSz);
        assert(pma_->heap_fd != -1);
        assert(pma_->stack_fd != -1);
        assert(pma_->max_sz == 0);

        char *addr_;

        // Description of heap change #0.
        char  *hpage0           = base_;
        size_t hpage0_change_sz = 1;
        char   hpage0_ch        = 'h';

        // Description of heap change #1.
        static const size_t kNewHeapSz       = 107 * kPageSz;
        char               *hpage1           = base_ + kNewHeapSz - kPageSz;
        size_t              hpage1_change_sz = kPageSz;
        char                hpage1_ch        = 'i';

        // Description of stack change #0.
        char  *spage0           = (char *)base_ + len_ - 1;
        size_t spage0_change_sz = 1;
        char   spage0_ch        = 'a';

        // Description of stack change #1.
        char  *spage1           = (char *)base_ + len_ - 3 * kPageSz;
        size_t spage1_change_sz = 2 * kPageSz;
        char   spage1_ch        = 'y';

        // Make heap change #0.
        assert(page_status_(hpage0, pma_) == PS_MAPPED_CLEAN);
        memset(hpage0, hpage0_ch, hpage0_change_sz);
        assert(page_status_(hpage0, pma_) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < hpage0_change_sz; i++) {
            assert(hpage0[i] == hpage0_ch);
        }

        // Make heap change #1.
        assert(page_status_(hpage1, pma_) == PS_MAPPED_CLEAN);
        memset(hpage1, hpage1_ch, hpage1_change_sz);
        assert(page_status_(hpage1, pma_) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < hpage1_change_sz; i++) {
            assert(hpage1[i] == hpage1_ch);
        }

        // Make stack change #0.
        assert(page_status_(spage0, pma_) == PS_MAPPED_CLEAN);
        memset(spage0, spage0_ch, spage0_change_sz);
        assert(page_status_(spage0, pma_) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < spage0_change_sz; i++) {
            assert(spage0[i] == spage0_ch);
        }

        // Make stack change #1.
        assert(page_status_(spage1, pma_) == PS_MAPPED_CLEAN);
        assert(page_status_(spage1 + kPageSz, pma_) == PS_MAPPED_CLEAN);
        memset(spage1, spage1_ch, spage1_change_sz);
        assert(page_status_(spage1, pma_) == PS_MAPPED_DIRTY);
        assert(page_status_(spage1 + kPageSz, pma_) == PS_MAPPED_DIRTY);
        for (size_t i = 0; i < spage1_change_sz; i++) {
            assert(spage1[i] == spage1_ch);
        }

        // Sync.
        assert(pma_sync(pma_, kNewHeapSz, kPageSz) == 0);

        // Removes all mappings.
        pma_deinit(pma_);
        free(pma_);

        // Re-establishes all mappings.
        pma_ = pma_init(base_, len_, kHeapFile, kStackFile);
        assert(pma_);
        assert(pma_->heap_start == base_);
        assert(pma_->stack_start == (char *)base_ + len_);
        assert(pma_->heap_len == kNewHeapSz);
        assert(pma_->stack_len == kPageSz);
        assert(pma_->heap_fd != -1);
        assert(pma_->stack_fd != -1);
        assert(pma_->max_sz == 0);

        // Check heap change #0.
        assert(page_status_(hpage0, pma_) == PS_MAPPED_CLEAN);
        for (size_t i = 0; i < hpage0_change_sz; i++) {
            assert(hpage0[i] == hpage0_ch);
        }
        assert(page_status_(hpage0, pma_) == PS_MAPPED_CLEAN);

        // Check heap change #1.
        assert(page_status_(hpage1, pma_) == PS_MAPPED_CLEAN);
        for (size_t i = 0; i < hpage1_change_sz; i++) {
            assert(hpage1[i] == hpage1_ch);
        }
        assert(page_status_(hpage1, pma_) == PS_MAPPED_CLEAN);

        // Check stack change #0.
        assert(page_status_(spage0, pma_) == PS_MAPPED_CLEAN);
        for (size_t i = 0; i < spage0_change_sz; i++) {
            assert(spage0[i] == spage0_ch);
        }

        // Check stack change #1.
        assert(page_status_(spage1, pma_) == PS_UNMAPPED);
        assert(page_status_(spage1 + kPageSz, pma_) == PS_UNMAPPED);

        assert(unlink(kHeapFile) == 0);
        assert(unlink(kStackFile) == 0);
    }
}

int
main(int argc, char *argv[])
{
    test_pma_();
    test_addr_to_page_idx_();
    return 0;
}
