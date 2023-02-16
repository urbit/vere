#include "pma.h"

#include <assert.h>

int64_t
addr_to_page_idx_(void *addr, const pma_t *pma);

uint8_t
page_status_(void *addr, const pma_t *pma);

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
        void  *base_ = (void *)0x200000000;
        size_t len_  = 1 << 20;
        pma_t *pma_  = pma_init(base_,
                               len_,
                               "/tmp/nonexistent-heap.bin",
                               "/tmp/nonexistent-stack.bin");
        assert(pma_);
        assert(pma_->heap_start == base_);
        assert(pma_->stack_start == (char *)base_ + len_);
        assert(pma_->heap_len == 0);
        assert(pma_->stack_len == 0);
        assert(pma_->heap_fd != -1);
        assert(pma_->stack_fd != -1);
        assert(pma_->max_sz == 0);

        void *addr_;
        char  ch;

        // Write to the heap.
        addr_ = base_;
        assert(page_status_(addr_, pma_) == PS_UNMAPPED);
        ch             = *(char *)addr_;
        *(char *)addr_ = 'h';
        assert(page_status_(addr_, pma_) == PS_MAPPED_DIRTY);

        // Write to the stack.
        addr_ = (char *)base_ + len_ - 1;
        assert(page_status_(addr_, pma_) == PS_UNMAPPED);
        ch             = *(char *)addr_;
        *(char *)addr_ = 'i';
        // assert(page_status_(addr_, pma_) == PS_MAPPED_DIRTY);

        pma_deinit(pma_);
    }
}

int
main(int argc, char *argv[])
{
    test_pma_();
    test_addr_to_page_idx_();
    return 0;
}
