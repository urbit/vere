#include "pma.h"

#include <assert.h>

static void
test_pma_init_()
{
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
        // Ensure we can write to the heap.
        *(char *)base_ = 'h';
        // Ensure we can write to the stack.
        *((char *)base_ + len_ - 1) = 'i';
        pma_deinit(pma_);
    }
}

int
main(int argc, char *argv[])
{
    test_pma_init_();
    return 0;
}
