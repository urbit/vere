#include "pma.h"

#include <assert.h>
#include <fcntl.h>
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

static int
new_file_(const char *path, char ch, size_t pg_cnt)
{
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    assert(fd != -1);

    char buf[pg_cnt * kPageSz];
    memset(buf, ch, sizeof(buf));
    assert(write_all(fd, buf, sizeof(buf)) == 0);

    memset(buf, 0, sizeof(buf));

    assert(lseek(fd, 0, SEEK_SET) == 0);
    assert(read_all(fd, buf, sizeof(buf)) == 0);
    for (size_t i = 0; i < sizeof(buf); i++) {
        assert(buf[i] == ch);
    }

    return fd;
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
}

int
main(int argc, char *argv[])
{
    test_pma_();
    test_addr_to_page_idx_();
    return 0;
}
