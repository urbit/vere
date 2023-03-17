/// @file

#include "wal.c"

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "page.h"

//==============================================================================
// FUNCTION TESTS

static void
test_wal_(void)
{
    // Empty write-ahead log.
    {
        wal_t wal;
        assert(wal_open("/tmp/wal-empty-test", &wal) == 0);

        char pg[kPageSz];

        char start = 'a';
        char end   = 'z';
        assert(start <= end);
        for (char ch = start; ch <= end; ch++) {
            memset(pg, ch, sizeof(pg));
            assert(wal_append(&wal, ch - start, pg) == 0);
        }
        assert(wal_sync(&wal) == 0);

        static const char kTargetPath[] = "/tmp/wal-empty-test-target";
        int fd = open(kTargetPath, O_CREAT | O_RDWR | O_TRUNC, 0644);
        assert(wal_apply(&wal, fd) == 0);

        assert(lseek(fd, 0, SEEK_SET) == 0);

        for (char ch = start; ch <= end; ch++) {
            assert(read_all(fd, pg, sizeof(pg)) == 0);
            for (size_t i = 0; i < sizeof(pg); i++) {
                assert(pg[i] == ch);
            }
        }

        wal_destroy(&wal);
        close(fd);
        unlink(kTargetPath);
    }

    // Corrupt data file of write-ahead log.
    {
        wal_t      wal;
        const char kPath[] = "/tmp/wal-corrupt-data-test";
        assert(wal_open(kPath, &wal) == 0);

        char pg[kPageSz];
        char start = '0';
        char end   = '9';
        assert(start <= end);
        for (char ch = start; ch <= end; ch++) {
            memset(pg, ch, sizeof(pg));
            assert(wal_append(&wal, ch - start, pg) == 0);
        }
        assert(wal_sync(&wal) == 0);

        int fd = open(wal.data_path, O_RDWR, 0644);
        assert(fd != -1);
        char ch = end + 1;
        assert(lseek(fd, 0, SEEK_SET) == 0);
        assert(write_all(fd, &ch, sizeof(ch)) == 0);
        assert(fsync(fd) == 0);

        // Can't use wal_destroy() here because it'll remove the WAL files.
        close(wal.data_fd);
        close(wal.meta_fd);
        free((void *)wal.data_path);
        free((void *)wal.meta_path);

        assert(wal_open(kPath, &wal) == -1);
        assert(errno == ENOTRECOVERABLE);

        char buf[512];

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalDataExt);
        assert(unlink(buf) == 0);

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalMetaExt);
        assert(unlink(buf) == 0);
    }

    // Corrupt metadata file of write-ahead log.
    {
        wal_t      wal;
        const char kPath[] = "/tmp/wal-corrupt-metadata-test";
        assert(wal_open(kPath, &wal) == 0);

        char pg[kPageSz];
        char start = '0';
        char end   = '9';
        assert(start <= end);
        for (char ch = start; ch <= end; ch++) {
            memset(pg, ch, sizeof(pg));
            assert(wal_append(&wal, ch - start, pg) == 0);
        }
        assert(wal_sync(&wal) == 0);

        int fd = open(wal.meta_path, O_RDWR, 0644);
        assert(fd != -1);
        uint64_t bad_pg_idx = -1;
        assert(lseek(fd, 0, SEEK_SET) == 0);
        assert(write_all(fd, &bad_pg_idx, sizeof(bad_pg_idx)) == 0);
        assert(fsync(fd) == 0);

        // Can't use wal_destroy() here because it'll remove the WAL files.
        close(wal.data_fd);
        close(wal.meta_fd);
        free((void *)wal.data_path);
        free((void *)wal.meta_path);

        assert(wal_open(kPath, &wal) == -1);
        assert(errno == ENOTRECOVERABLE);

        char buf[512];

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalDataExt);
        assert(unlink(buf) == 0);

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalMetaExt);
        assert(unlink(buf) == 0);
    }
}

int
main(int argc, char *argv[])
{
    test_wal_();
    return 0;
}
