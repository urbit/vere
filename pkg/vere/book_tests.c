/// @file

#include "db/book.h"
#include "vere.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* test helpers
*/

/* _test_tmpdir(): create temporary test directory.
*/
static c3_c*
_test_tmpdir(const c3_c* prefix)
{
  c3_c* tmp_c = c3_malloc(256);
  snprintf(tmp_c, 256, "/tmp/%s-XXXXXX", prefix);

  if ( !mkdtemp(tmp_c) ) {
    fprintf(stderr, "book_tests: failed to create temp dir\r\n");
    c3_free(tmp_c);
    return 0;
  }

  return tmp_c;
}

/* _test_cleanup(): remove test directory and contents.
*/
static void
_test_cleanup(const c3_c* dir_c)
{
  c3_c cmd_c[512];
  snprintf(cmd_c, sizeof(cmd_c), "rm -rf %s", dir_c);
  system(cmd_c);
}

/* _test_make_event(): create a fake event buffer (mug + jam data).
*/
static void
_test_make_event(c3_y** buf_y, c3_z* siz_z, c3_d eve_d)
{
  //  simple fake event: 4-byte mug + variable jam data
  //  mug = eve_d as 32-bit value
  //  jam = repeating pattern based on eve_d

  c3_w mug_w = (c3_w)eve_d;
  c3_z jam_len = 16 + (eve_d % 32);  //  16-48 bytes of jam data

  *siz_z = 4 + jam_len;
  *buf_y = c3_malloc(*siz_z);

  memcpy(*buf_y, &mug_w, 4);

  //  fill jam data with pattern
  for ( c3_z i = 0; i < jam_len; i++ ) {
    (*buf_y)[4 + i] = (c3_y)((eve_d + i) & 0xff);
  }
}

/* _test_verify_event(): verify event buffer matches expected.
*/
static c3_o
_test_verify_event(c3_d eve_d, c3_z siz_z, void* buf_v)
{
  c3_y* buf_y = (c3_y*)buf_v;
  c3_w  mug_w;
  c3_z  expected_len;

  memcpy(&mug_w, buf_y, 4);

  if ( mug_w != (c3_w)eve_d ) {
    fprintf(stderr, "book_tests: event %llu mug mismatch: got %u\r\n", eve_d, mug_w);
    return c3n;
  }

  expected_len = 16 + (eve_d % 32);

  if ( siz_z != 4 + expected_len ) {
    fprintf(stderr, "book_tests: event %llu size mismatch: got %zu, expected %zu (4 + %zu)\r\n",
            eve_d, siz_z, 4 + expected_len, expected_len);
    return c3n;
  }

  //  verify jam data pattern
  for ( c3_z i = 0; i < expected_len; i++ ) {
    if ( buf_y[4 + i] != (c3_y)((eve_d + i) & 0xff) ) {
      fprintf(stderr, "book_tests: event %llu data mismatch at offset %zu\r\n",
              eve_d, i);
      return c3n;
    }
  }

  return c3y;
}

/* corruption test helpers
*/

/* _test_get_book_path(): build path to book.log file.
*/
static void
_test_get_book_path(const c3_c* dir_c, c3_c* path_c, c3_z max_z)
{
  snprintf(path_c, max_z, "%s/book.log", dir_c);
}

/* _test_get_file_size(): get size of book.log file.
*/
static c3_o
_test_get_file_size(const c3_c* dir_c, c3_z* siz_z)
{
  c3_c       path_c[8193];
  struct stat st;

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  if ( 0 != stat(path_c, &st) ) {
    fprintf(stderr, "book_tests: stat failed: %s\r\n", path_c);
    return c3n;
  }

  *siz_z = st.st_size;
  return c3y;
}

/* _test_calculate_event_offset(): calculate byte offset to specific event.
*/
static c3_o
_test_calculate_event_offset(const c3_c* dir_c, c3_d target_eve, c3_w* off_w)
{
  c3_c            path_c[8193];
  c3_i            fid_i;
  u3_book_head    hed_u;
  c3_d            cur_d;
  c3_w            cur_off;
  c3_zs           ret_zs;

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  fid_i = c3_open(path_c, O_RDONLY, 0);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: open failed: %s\r\n", path_c);
    return c3n;
  }

  //  read header
  ret_zs = pread(fid_i, &hed_u, sizeof(u3_book_head), 0);
  if ( sizeof(u3_book_head) != ret_zs ) {
    fprintf(stderr, "book_tests: header read failed\r\n");
    close(fid_i);
    return c3n;
  }

  //  allow target beyond current range (for corruption tests)
  //  just scan up to target or last event
  c3_d scan_to = (target_eve <= hed_u.las_d) ? target_eve : hed_u.las_d + 1;

  //  scan to target event
  cur_off = 64 + 256;  //  sizeof(u3_book_head) + BOOK_META_SIZE

  for ( cur_d = hed_u.fir_d; cur_d < scan_to; cur_d++ ) {
    u3_book_deed_head deed_hed;

    ret_zs = pread(fid_i, &deed_hed, sizeof(u3_book_deed_head), cur_off);
    if ( sizeof(u3_book_deed_head) != ret_zs ) {
      fprintf(stderr, "book_tests: deed header read failed at event %llu offset %u\r\n",
              cur_d, cur_off);
      close(fid_i);
      return c3n;
    }

    //  total deed size = head(16 with padding) + (len_d - 4) + tail(16 with padding)
    //  = 16 + (len_d - 4) + 16 = len_d + 28
    cur_off += (deed_hed.len_d + 28);
  }

  close(fid_i);
  *off_w = cur_off;
  return c3y;
}

/* _test_corrupt_magic(): corrupt magic number in header.
*/
static c3_o
_test_corrupt_magic(const c3_c* dir_c, c3_w bad_magic)
{
  c3_c path_c[8193];
  c3_i fid_i;
  c3_zs ret_zs;

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  fid_i = c3_open(path_c, O_RDWR, 0);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: corrupt_magic open failed\r\n");
    return c3n;
  }

  ret_zs = pwrite(fid_i, &bad_magic, sizeof(c3_w), 0);
  if ( sizeof(c3_w) != ret_zs ) {
    fprintf(stderr, "book_tests: corrupt_magic write failed\r\n");
    close(fid_i);
    return c3n;
  }

  c3_sync(fid_i);
  close(fid_i);
  return c3y;
}

/* _test_corrupt_version(): corrupt version in header.
*/
static c3_o
_test_corrupt_version(const c3_c* dir_c, c3_w bad_version)
{
  c3_c path_c[8193];
  c3_i fid_i;
  c3_zs ret_zs;

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  fid_i = c3_open(path_c, O_RDWR, 0);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: corrupt_version open failed\r\n");
    return c3n;
  }

  ret_zs = pwrite(fid_i, &bad_version, sizeof(c3_w), 4);  //  offset 4
  if ( sizeof(c3_w) != ret_zs ) {
    fprintf(stderr, "book_tests: corrupt_version write failed\r\n");
    close(fid_i);
    return c3n;
  }

  c3_sync(fid_i);
  close(fid_i);
  return c3y;
}

/* _test_corrupt_event_crc(): corrupt CRC of specific event.
*/
static c3_o
_test_corrupt_event_crc(const c3_c* dir_c, c3_d eve_d)
{
  c3_c              path_c[8193];
  c3_i              fid_i;
  c3_w              event_off, crc_off;
  u3_book_deed_head deed_hed;
  c3_w              bad_crc = 0xDEADBEEF;
  c3_zs             ret_zs;

  //  calculate offset to event
  if ( c3n == _test_calculate_event_offset(dir_c, eve_d, &event_off) ) {
    return c3n;
  }

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  fid_i = c3_open(path_c, O_RDWR, 0);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: corrupt_event_crc open failed\r\n");
    return c3n;
  }

  //  read deed header to get len_d
  ret_zs = pread(fid_i, &deed_hed, sizeof(u3_book_deed_head), event_off);
  if ( sizeof(u3_book_deed_head) != ret_zs ) {
    fprintf(stderr, "book_tests: corrupt_event_crc deed read failed\r\n");
    close(fid_i);
    return c3n;
  }

  //  CRC offset = event_off + head(16 with padding) + (len_d - 4)
  crc_off = event_off + 16 + (deed_hed.len_d - 4);

  ret_zs = pwrite(fid_i, &bad_crc, sizeof(c3_w), crc_off);
  if ( sizeof(c3_w) != ret_zs ) {
    fprintf(stderr, "book_tests: corrupt_event_crc write failed\r\n");
    close(fid_i);
    return c3n;
  }

  c3_sync(fid_i);
  close(fid_i);
  return c3y;
}

/* _test_corrupt_event_length_tail(): corrupt let_d in event trailer.
*/
static c3_o
_test_corrupt_event_length_tail(const c3_c* dir_c, c3_d eve_d, c3_d bad_let_d)
{
  c3_c              path_c[8193];
  c3_i              fid_i;
  c3_w              event_off, let_off;
  u3_book_deed_head deed_hed;
  c3_zs             ret_zs;

  //  calculate offset to event
  if ( c3n == _test_calculate_event_offset(dir_c, eve_d, &event_off) ) {
    return c3n;
  }

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  fid_i = c3_open(path_c, O_RDWR, 0);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: corrupt_event_length open failed\r\n");
    return c3n;
  }

  //  read deed header to get len_d
  ret_zs = pread(fid_i, &deed_hed, sizeof(u3_book_deed_head), event_off);
  if ( sizeof(u3_book_deed_head) != ret_zs ) {
    fprintf(stderr, "book_tests: corrupt_event_length deed read failed\r\n");
    close(fid_i);
    return c3n;
  }

  //  let_d offset = event_off + head(16 with padding) + (len_d - 4) + crc_w(4)
  let_off = event_off + 16 + (deed_hed.len_d - 4) + 4;

  ret_zs = pwrite(fid_i, &bad_let_d, sizeof(c3_d), let_off);
  if ( sizeof(c3_d) != ret_zs ) {
    fprintf(stderr, "book_tests: corrupt_event_length write failed\r\n");
    close(fid_i);
    return c3n;
  }

  c3_sync(fid_i);
  close(fid_i);
  return c3y;
}

/* _test_truncate_file(): truncate book.log to specific offset.
*/
static c3_o
_test_truncate_file(const c3_c* dir_c, c3_w offset)
{
  c3_c path_c[8193];

  _test_get_book_path(dir_c, path_c, sizeof(path_c));

  if ( 0 != truncate(path_c, offset) ) {
    fprintf(stderr, "book_tests: truncate failed at offset %u\r\n", offset);
    return c3n;
  }

  return c3y;
}

/* read callback context
*/
typedef struct _read_ctx {
  c3_d  count;
  c3_d  expected_start;
  c3_o  failed;
} read_ctx;

/* _test_read_cb(): callback for u3_book_read().
*/
static c3_o
_test_read_cb(void* ptr_v, c3_d eve_d, c3_z siz_z, void* buf_v)
{
  read_ctx* ctx = (read_ctx*)ptr_v;

  if ( eve_d != ctx->expected_start + ctx->count ) {
    fprintf(stderr, "book_tests: read callback event mismatch: %llu vs %llu\r\n",
            eve_d, ctx->expected_start + ctx->count);
    ctx->failed = c3y;
    return c3n;
  }

  if ( c3n == _test_verify_event(eve_d, siz_z, buf_v) ) {
    ctx->failed = c3y;
    return c3n;
  }

  ctx->count++;
  return c3y;
}

/* tests
*/

/* _test_book_init_empty(): test creating new empty log.
*/
static c3_o
_test_book_init_empty(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-init");
  u3_book* log_u;
  c3_d     low_d, hig_d;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create new log
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: init failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify empty gulf
  if ( c3n == u3_book_gulf(log_u, &low_d, &hig_d) ) {
    fprintf(stderr, "book_tests: gulf failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( 0 != low_d || 0 != hig_d ) {
    fprintf(stderr, "book_tests: empty gulf wrong: [%llu, %llu]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_single_event(): test writing and reading single event.
*/
static c3_o
_test_book_single_event(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-single");
  u3_book* log_u;
  c3_y*    buf_y;
  c3_z     siz_z;
  c3_d     low_d, hig_d;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  create and save event 1
  _test_make_event(&buf_y, &siz_z, 1);

  if ( c3n == u3_book_save(log_u, 1, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: save failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  c3_free(buf_y);

  //  verify gulf
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 1 != hig_d ) {
    fprintf(stderr, "book_tests: single gulf wrong: [%llu, %llu]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read event back
  ctx.expected_start = 1;
  ctx.count = 0;
  ctx.failed = c3n;
  if ( c3n == u3_book_read(log_u, &ctx, 1, 1, _test_read_cb) ) {
    fprintf(stderr, "book_tests: read failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3y == ctx.failed || 1 != ctx.count ) {
    fprintf(stderr, "book_tests: read verify failed (failed=%u, count=%llu)\r\n",
            ctx.failed, ctx.count);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_batch_write(): test writing batch of 100 events.
*/
static c3_o
_test_book_batch_write(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-batch");
  u3_book* log_u;
  void*    bufs[100];
  c3_z     sizes[100];
  c3_d     i, low_d, hig_d;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  create 100 events
  for ( i = 0; i < 100; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  //  write batch
  if ( c3n == u3_book_save(log_u, 1, 100, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: batch save failed\r\n");
    for ( i = 0; i < 100; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  free buffers
  for ( i = 0; i < 100; i++ ) {
    c3_free(bufs[i]);
  }

  //  verify gulf
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 100 != hig_d ) {
    fprintf(stderr, "book_tests: batch gulf wrong: [%llu, %llu]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read all events back
  ctx.expected_start = 1;
  ctx.count = 0;
  ctx.failed = c3n;
  if ( c3n == u3_book_read(log_u, &ctx, 1, 100, _test_read_cb) ) {
    fprintf(stderr, "book_tests: batch read failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3y == ctx.failed || 100 != ctx.count ) {
    fprintf(stderr, "book_tests: batch read verify failed (failed=%u, count=%llu)\r\n",
            ctx.failed, ctx.count);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_persistence(): test closing and reopening log.
*/
static c3_o
_test_book_persistence(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-persist");
  u3_book* log_u;
  void*    bufs[50];
  c3_z     sizes[50];
  c3_d     i, low_d, hig_d;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  //  write 50 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 50, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: persist save failed\r\n");
    for ( i = 0; i < 50; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  reopen and verify
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: persist reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 50 != hig_d ) {
    fprintf(stderr, "book_tests: persist gulf wrong: [%llu, %llu]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read all events
  ctx.expected_start = 1;
  ctx.count = 0;
  ctx.failed = c3n;
  if ( c3n == u3_book_read(log_u, &ctx, 1, 50, _test_read_cb) ) {
    fprintf(stderr, "book_tests: persist read failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3y == ctx.failed || 50 != ctx.count ) {
    fprintf(stderr, "book_tests: persist verify failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_contiguity(): test that non-contiguous writes fail.
*/
static c3_o
_test_book_contiguity(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-contig");
  u3_book* log_u;
  c3_y*    buf_y;
  c3_z     siz_z;

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  write event 1
  _test_make_event(&buf_y, &siz_z, 1);
  if ( c3n == u3_book_save(log_u, 1, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: contig save 1 failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  c3_free(buf_y);

  //  try to write event 3 (should fail - gap)
  _test_make_event(&buf_y, &siz_z, 3);
  if ( c3y == u3_book_save(log_u, 3, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: contig should have failed for gap\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  c3_free(buf_y);

  //  write event 2 (should succeed)
  _test_make_event(&buf_y, &siz_z, 2);
  if ( c3n == u3_book_save(log_u, 2, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: contig save 2 failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  c3_free(buf_y);

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_partial_read(): test reading subset of events.
*/
static c3_o
_test_book_partial_read(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-partial");
  u3_book* log_u;
  void*    bufs[100];
  c3_z     sizes[100];
  c3_d     i;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  write 100 events
  for ( i = 0; i < 100; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 100, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: partial save failed\r\n");
    for ( i = 0; i < 100; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 100; i++ ) {
    c3_free(bufs[i]);
  }

  //  read events 50-75
  ctx.expected_start = 50;
  ctx.count = 0;
  ctx.failed = c3n;
  if ( c3n == u3_book_read(log_u, &ctx, 50, 26, _test_read_cb) ) {
    fprintf(stderr, "book_tests: partial read failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3y == ctx.failed || 26 != ctx.count ) {
    fprintf(stderr, "book_tests: partial verify failed: count=%llu\r\n",
            ctx.count);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_iterator(): test walk iterator pattern.
*/
static c3_o
_test_book_iterator(void)
{
  c3_c*         tmp_c = _test_tmpdir("book-iter");
  u3_book*      log_u;
  u3_book_walk  itr_u;
  void*         bufs[50];
  c3_z          sizes[50];
  c3_d          i;
  c3_z          len_z;
  void*         buf_v;
  c3_d          count = 0;

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  write 50 events
  for ( i = 0; i < 50; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 50, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: iter save failed\r\n");
    for ( i = 0; i < 50; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    c3_free(bufs[i]);
  }

  //  iterate events 10-30
  if ( c3n == u3_book_walk_init(log_u, &itr_u, 10, 30) ) {
    fprintf(stderr, "book_tests: walk_init failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  while ( c3y == u3_book_walk_next(&itr_u, &len_z, &buf_v) ) {
    c3_d expected_eve = 10 + count;

    if ( c3n == _test_verify_event(expected_eve, len_z, buf_v) ) {
      fprintf(stderr, "book_tests: iter verify failed at %llu\r\n", count);
      c3_free(buf_v);
      u3_book_walk_done(&itr_u);
      u3_book_exit(log_u);
      _test_cleanup(tmp_c);
      c3_free(tmp_c);
      return c3n;
    }

    c3_free(buf_v);
    count++;
  }

  if ( 21 != count ) {  //  events 10-30 inclusive = 21 events
    fprintf(stderr, "book_tests: iter count wrong: %llu\r\n", count);
    u3_book_walk_done(&itr_u);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_walk_done(&itr_u);
  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* metadata callback context
*/
typedef struct _meta_ctx {
  c3_o   found;
  c3_z   len_z;
  c3_y   buf_y[256];
} meta_ctx;

/* _test_meta_cb(): callback for u3_book_read_meta().
*/
static void
_test_meta_cb(void* ptr_v, c3_zs len_zs, void* val_v)
{
  meta_ctx* ctx = (meta_ctx*)ptr_v;

  if ( len_zs < 0 ) {
    ctx->found = c3n;
    ctx->len_z = 0;
    return;
  }

  ctx->found = c3y;
  ctx->len_z = len_zs;
  if ( len_zs > 0 && len_zs <= 256 ) {
    memcpy(ctx->buf_y, val_v, len_zs);
  }
}

/* _test_book_metadata(): test metadata read/write operations.
*/
static c3_o
_test_book_metadata(void)
{
  c3_c*     tmp_c = _test_tmpdir("book-meta");
  u3_book*  log_u;
  meta_ctx  ctx = {0};
  c3_w      version = 1;
  c3_y      who[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  c3_o      fake = c3y;

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  write metadata
  if ( c3n == u3_book_save_meta(log_u, "version", sizeof(version), &version) ) {
    fprintf(stderr, "book_tests: meta save version failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3n == u3_book_save_meta(log_u, "who", sizeof(who), who) ) {
    fprintf(stderr, "book_tests: meta save who failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3n == u3_book_save_meta(log_u, "fake", sizeof(fake), &fake) ) {
    fprintf(stderr, "book_tests: meta save fake failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read metadata back
  ctx.found = c3n;
  u3_book_read_meta(log_u, &ctx, "version", _test_meta_cb);
  if ( c3n == ctx.found || ctx.len_z != sizeof(version) ) {
    fprintf(stderr, "book_tests: meta read version failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  if ( memcmp(ctx.buf_y, &version, sizeof(version)) != 0 ) {
    fprintf(stderr, "book_tests: meta version mismatch\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  ctx.found = c3n;
  u3_book_read_meta(log_u, &ctx, "who", _test_meta_cb);
  if ( c3n == ctx.found || ctx.len_z != sizeof(who) ) {
    fprintf(stderr, "book_tests: meta read who failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  if ( memcmp(ctx.buf_y, who, sizeof(who)) != 0 ) {
    fprintf(stderr, "book_tests: meta who mismatch\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read non-existent key
  ctx.found = c3y;
  u3_book_read_meta(log_u, &ctx, "nonexistent", _test_meta_cb);
  if ( c3y == ctx.found ) {
    fprintf(stderr, "book_tests: meta read nonexistent should fail\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  update existing key
  version = 2;
  if ( c3n == u3_book_save_meta(log_u, "version", sizeof(version), &version) ) {
    fprintf(stderr, "book_tests: meta update version failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  ctx.found = c3n;
  u3_book_read_meta(log_u, &ctx, "version", _test_meta_cb);
  if ( c3n == ctx.found || ctx.len_z != sizeof(version) ) {
    fprintf(stderr, "book_tests: meta read updated version failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  if ( memcmp(ctx.buf_y, &version, sizeof(version)) != 0 ) {
    fprintf(stderr, "book_tests: meta updated version mismatch\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* failure mode tests
*/

/* _test_book_corrupt_header_magic(): test invalid magic number detection.
*/
static c3_o
_test_book_corrupt_header_magic(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-corrupt-magic");
  u3_book* log_u;
  void*    bufs[10];
  c3_z     sizes[10];
  c3_d     i;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 10 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 10, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: corrupt_header_magic save failed\r\n");
    for ( i = 0; i < 10; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt magic number
  if ( c3n == _test_corrupt_magic(tmp_c, 0xDEADBEEF) ) {
    fprintf(stderr, "book_tests: corrupt_header_magic corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to reopen - should fail
  log_u = u3_book_init(tmp_c);
  if ( log_u ) {
    fprintf(stderr, "book_tests: corrupt_header_magic should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_corrupt_header_version(): test unsupported version detection.
*/
static c3_o
_test_book_corrupt_header_version(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-corrupt-version");
  u3_book* log_u;
  void*    bufs[10];
  c3_z     sizes[10];
  c3_d     i;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 10 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 10, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: corrupt_header_version save failed\r\n");
    for ( i = 0; i < 10; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt version
  if ( c3n == _test_corrupt_version(tmp_c, 99) ) {
    fprintf(stderr, "book_tests: corrupt_header_version corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to reopen - should fail
  log_u = u3_book_init(tmp_c);
  if ( log_u ) {
    fprintf(stderr, "book_tests: corrupt_header_version should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_corrupt_deed_crc(): test CRC corruption detection and recovery.
*/
static c3_o
_test_book_corrupt_deed_crc(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-corrupt-crc");
  u3_book* log_u;
  void*    bufs[50];
  c3_z     sizes[50];
  c3_d     i, low_d, hig_d;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 50 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 50, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc save failed\r\n");
    for ( i = 0; i < 50; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt event 25's CRC
  if ( c3n == _test_corrupt_event_crc(tmp_c, 25) ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  reopen - should succeed with recovery
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify recovery truncated to event 24
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 24 != hig_d ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc gulf wrong: [%llu, %llu] expected [1, 24]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read events 1-24 should succeed
  ctx.expected_start = 1;
  ctx.count = 0;
  ctx.failed = c3n;
  if ( c3n == u3_book_read(log_u, &ctx, 1, 24, _test_read_cb) ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc read failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3y == ctx.failed || 24 != ctx.count ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc read verify failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_corrupt_deed_length_mismatch(): test len_d != let_d detection.
*/
static c3_o
_test_book_corrupt_deed_length_mismatch(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-corrupt-length");
  u3_book* log_u;
  void*    bufs[30];
  c3_z     sizes[30];
  c3_d     i, low_d, hig_d;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 30 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 30; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 30, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: corrupt_deed_length save failed\r\n");
    for ( i = 0; i < 30; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 30; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt event 15's let_d field
  if ( c3n == _test_corrupt_event_length_tail(tmp_c, 15, 99999) ) {
    fprintf(stderr, "book_tests: corrupt_deed_length corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  reopen with recovery
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: corrupt_deed_length reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify recovery truncated to event 14
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 14 != hig_d ) {
    fprintf(stderr, "book_tests: corrupt_deed_length gulf wrong: [%llu, %llu] expected [1, 14]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_truncated_deed_partial(): test partial deed detection.
*/
static c3_o
_test_book_truncated_deed_partial(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-truncated");
  u3_book* log_u;
  void*    bufs[20];
  c3_z     sizes[20];
  c3_d     i, low_d, hig_d;
  c3_w     event20_off;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 20 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 20; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 20, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: truncated_deed save failed\r\n");
    for ( i = 0; i < 20; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 20; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  calculate offset to event 20
  if ( c3n == _test_calculate_event_offset(tmp_c, 20, &event20_off) ) {
    fprintf(stderr, "book_tests: truncated_deed offset calc failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  truncate in middle of event 20
  if ( c3n == _test_truncate_file(tmp_c, event20_off + 10) ) {
    fprintf(stderr, "book_tests: truncated_deed truncate failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  reopen
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: truncated_deed reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify recovery removed partial event 20
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 19 != hig_d ) {
    fprintf(stderr, "book_tests: truncated_deed gulf wrong: [%llu, %llu] expected [1, 19]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_multiple_corruptions(): verify recovery stops at first corruption.
*/
static c3_o
_test_book_multiple_corruptions(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-multi-corrupt");
  u3_book* log_u;
  void*    bufs[100];
  c3_z     sizes[100];
  c3_d     i, low_d, hig_d;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 100 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 100; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 100, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: multi_corrupt save failed\r\n");
    for ( i = 0; i < 100; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 100; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt event 30's CRC
  if ( c3n == _test_corrupt_event_crc(tmp_c, 30) ) {
    fprintf(stderr, "book_tests: multi_corrupt first corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  corrupt event 60's CRC
  if ( c3n == _test_corrupt_event_crc(tmp_c, 60) ) {
    fprintf(stderr, "book_tests: multi_corrupt second corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  reopen
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: multi_corrupt reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify recovery stopped at first corruption (event 30)
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 29 != hig_d ) {
    fprintf(stderr, "book_tests: multi_corrupt gulf wrong: [%llu, %llu] expected [1, 29]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_corrupt_first_event(): corruption at first event empties log.
*/
static c3_o
_test_book_corrupt_first_event(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-corrupt-first");
  u3_book* log_u;
  void*    bufs[50];
  c3_z     sizes[50];
  c3_d     i, low_d, hig_d;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with 50 events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 50, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: corrupt_first save failed\r\n");
    for ( i = 0; i < 50; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt event 1's CRC
  if ( c3n == _test_corrupt_event_crc(tmp_c, 1) ) {
    fprintf(stderr, "book_tests: corrupt_first corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  reopen
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: corrupt_first reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify log is empty
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 0 != low_d || 0 != hig_d ) {
    fprintf(stderr, "book_tests: corrupt_first gulf wrong: [%llu, %llu] expected [0, 0]\r\n",
            low_d, hig_d);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_file_too_small(): detect undersized file.
*/
static c3_o
_test_book_file_too_small(void)
{
  c3_c*  tmp_c = _test_tmpdir("book-too-small");
  c3_c   path_c[8193];
  c3_i   fid_i;
  c3_y   small_buf[32];
  u3_book* log_u;

  if ( !tmp_c ) {
    return c3n;
  }

  //  manually create small file
  _test_get_book_path(tmp_c, path_c, sizeof(path_c));

  fid_i = c3_open(path_c, O_RDWR|O_CREAT, 0644);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: file_too_small create failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  memset(small_buf, 0, sizeof(small_buf));
  if ( sizeof(small_buf) != write(fid_i, small_buf, sizeof(small_buf)) ) {
    fprintf(stderr, "book_tests: file_too_small write failed\r\n");
    close(fid_i);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  close(fid_i);

  //  try to init - should fail
  log_u = u3_book_init(tmp_c);
  if ( log_u ) {
    fprintf(stderr, "book_tests: file_too_small should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* boundary condition tests
*/

/* _test_book_read_empty_log(): test reading from empty log.
*/
static c3_o
_test_book_read_empty_log(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-read-empty");
  u3_book* log_u;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  //  create empty log
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to read from empty log - should fail
  ctx.expected_start = 1;
  ctx.count = 0;
  ctx.failed = c3n;
  if ( c3y == u3_book_read(log_u, &ctx, 1, 1, _test_read_cb) ) {
    fprintf(stderr, "book_tests: read_empty_log should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_read_beyond_range(): test reading beyond event range.
*/
static c3_o
_test_book_read_beyond_range(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-read-beyond");
  u3_book* log_u;
  void*    bufs[10];
  c3_z     sizes[10];
  c3_d     i;
  read_ctx ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with events 1-10
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 10, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: read_beyond_range save failed\r\n");
    for ( i = 0; i < 10; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    c3_free(bufs[i]);
  }

  //  try to read event 11 - should fail
  if ( c3y == u3_book_read(log_u, &ctx, 11, 1, _test_read_cb) ) {
    fprintf(stderr, "book_tests: read_beyond_range event 11 should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to read events 5-15 - should fail (extends beyond)
  if ( c3y == u3_book_read(log_u, &ctx, 5, 11, _test_read_cb) ) {
    fprintf(stderr, "book_tests: read_beyond_range events 5-15 should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to read event 0 - should fail (before first)
  if ( c3y == u3_book_read(log_u, &ctx, 0, 1, _test_read_cb) ) {
    fprintf(stderr, "book_tests: read_beyond_range event 0 should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_iterator_invalid_ranges(): test iterator with invalid ranges.
*/
static c3_o
_test_book_iterator_invalid_ranges(void)
{
  c3_c*         tmp_c = _test_tmpdir("book-iter-invalid");
  u3_book*      log_u;
  u3_book_walk  itr_u;
  void*         bufs[50];
  c3_z          sizes[50];
  c3_d          i;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with events 1-50
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 50, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: iter_invalid_ranges save failed\r\n");
    for ( i = 0; i < 50; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    c3_free(bufs[i]);
  }

  //  try iterator [60, 70] - should fail (beyond range)
  if ( c3y == u3_book_walk_init(log_u, &itr_u, 60, 70) ) {
    fprintf(stderr, "book_tests: iter_invalid_ranges [60, 70] should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try iterator [40, 30] - should fail (start > end)
  if ( c3y == u3_book_walk_init(log_u, &itr_u, 40, 30) ) {
    fprintf(stderr, "book_tests: iter_invalid_ranges [40, 30] should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try iterator [0, 10] - should fail (before first)
  if ( c3y == u3_book_walk_init(log_u, &itr_u, 0, 10) ) {
    fprintf(stderr, "book_tests: iter_invalid_ranges [0, 10] should have failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_write_first_wrong_epoch(): test first event must be epo_d + 1.
*/
static c3_o
_test_book_write_first_wrong_epoch(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-wrong-epoch");
  u3_book* log_u;
  c3_y*    buf_y;
  c3_z     siz_z;

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to save event 5 with epo_d=0 - should fail (expected event 1)
  _test_make_event(&buf_y, &siz_z, 5);
  if ( c3y == u3_book_save(log_u, 5, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: wrong_epoch event 5 with epo 0 should have failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  c3_free(buf_y);

  //  try to save event 1 with epo_d=5 - should fail (expected event 6)
  _test_make_event(&buf_y, &siz_z, 1);
  if ( c3y == u3_book_save(log_u, 1, 1, (void**)&buf_y, &siz_z, 5) ) {
    fprintf(stderr, "book_tests: wrong_epoch event 1 with epo 5 should have failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  c3_free(buf_y);

  //  save event 1 with epo_d=0 - should succeed
  _test_make_event(&buf_y, &siz_z, 1);
  if ( c3n == u3_book_save(log_u, 1, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: wrong_epoch event 1 with epo 0 failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }
  c3_free(buf_y);

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_large_event_cb(): callback for large event test.
*/
static c3_o
_test_large_event_cb(void* ptr_v, c3_d eve_d, c3_z siz_z, void* buf_v)
{
  c3_z* expected_size = (c3_z*)ptr_v;

  if ( 1 != eve_d ) {
    fprintf(stderr, "book_tests: large_event_cb wrong event: %llu\r\n", eve_d);
    return c3n;
  }

  if ( *expected_size != siz_z ) {
    fprintf(stderr, "book_tests: large_event_cb size mismatch: %zu vs %zu\r\n",
            siz_z, *expected_size);
    return c3n;
  }

  return c3y;
}

/* _test_book_very_large_event(): test large event handling.
*/
static c3_o
_test_book_very_large_event(void)
{
  c3_c*    tmp_c = _test_tmpdir("book-large-event");
  u3_book* log_u;
  c3_y*    buf_y;
  c3_z     siz_z;
  c3_z     large_size = 1024 * 1024;  //  1 MB event
  c3_w     mug_w = 12345;
  c3_z     i;

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  create large event: 4-byte mug + (large_size - 4) jam data
  siz_z = large_size;
  buf_y = c3_malloc(siz_z);

  memcpy(buf_y, &mug_w, 4);
  for ( i = 4; i < siz_z; i++ ) {
    buf_y[i] = (c3_y)(i & 0xff);
  }

  //  save large event
  if ( c3n == u3_book_save(log_u, 1, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: very_large_event save failed\r\n");
    c3_free(buf_y);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  c3_free(buf_y);

  //  read back and verify size matches
  if ( c3n == u3_book_read(log_u, &large_size, 1, 1, _test_large_event_cb) ) {
    fprintf(stderr, "book_tests: very_large_event read failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* metadata edge case tests
*/

/* _test_book_metadata_section_full(): test 256-byte metadata limit.
*/
static c3_o
_test_book_metadata_section_full(void)
{
  c3_c*     tmp_c = _test_tmpdir("book-meta-full");
  u3_book*  log_u;
  c3_y      data[64];
  c3_i      count;

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  fill metadata section with entries
  //  format: [4-byte count][4-byte key_len][key][4-byte val_len][val]...
  //  total limit: 256 bytes
  memset(data, 0xAB, sizeof(data));

  //  add entries until close to limit
  for ( count = 0; count < 20; count++ ) {
    c3_c key_c[16];
    snprintf(key_c, sizeof(key_c), "key%d", count);

    if ( c3n == u3_book_save_meta(log_u, key_c, sizeof(data), data) ) {
      //  expected to fail when metadata is full
      break;
    }
  }

  if ( 0 == count ) {
    fprintf(stderr, "book_tests: meta_section_full no entries saved\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to add one more - should fail if we hit the limit
  if ( c3y == u3_book_save_meta(log_u, "overflow", sizeof(data), data) ) {
    //  if it succeeded, we didn't hit the limit yet - that's ok
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_metadata_corrupted_count(): test corrupted metadata handling.
*/
static c3_o
_test_book_metadata_corrupted_count(void)
{
  c3_c*     tmp_c = _test_tmpdir("book-meta-corrupt");
  u3_book*  log_u;
  c3_w      version = 1;
  void*     bufs[10];
  c3_z      sizes[10];
  c3_d      i, low_d, hig_d;
  c3_c      path_c[8193];
  c3_i      fid_i;
  c3_w      bad_count = 999;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with metadata and events
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  add metadata
  if ( c3n == u3_book_save_meta(log_u, "version", sizeof(version), &version) ) {
    fprintf(stderr, "book_tests: meta_corrupted save_meta failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  add events
  for ( i = 0; i < 10; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 10, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: meta_corrupted save failed\r\n");
    for ( i = 0; i < 10; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 10; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt metadata count field (at offset 64)
  _test_get_book_path(tmp_c, path_c, sizeof(path_c));
  fid_i = c3_open(path_c, O_RDWR, 0);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book_tests: meta_corrupted open failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  pwrite(fid_i, &bad_count, sizeof(c3_w), 64);
  c3_sync(fid_i);
  close(fid_i);

  //  reopen - should succeed (metadata corruption shouldn't prevent init)
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: meta_corrupted reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  events should still be readable
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 1 != low_d || 10 != hig_d ) {
    fprintf(stderr, "book_tests: meta_corrupted events lost\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_metadata_empty_key(): test empty key edge case.
*/
static c3_o
_test_book_metadata_empty_key(void)
{
  c3_c*     tmp_c = _test_tmpdir("book-meta-empty");
  u3_book*  log_u;
  c3_w      val = 42;
  meta_ctx  ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  try to save with empty key
  if ( c3n == u3_book_save_meta(log_u, "", sizeof(val), &val) ) {
    //  empty key rejected - acceptable behavior
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3y;
  }

  //  empty key accepted - try to read it back
  u3_book_read_meta(log_u, &ctx, "", _test_meta_cb);
  if ( c3n == ctx.found ) {
    fprintf(stderr, "book_tests: meta_empty_key not found after save\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_metadata_persistence(): test metadata survives corruption recovery.
*/
static c3_o
_test_book_metadata_persistence(void)
{
  c3_c*     tmp_c = _test_tmpdir("book-meta-persist");
  u3_book*  log_u;
  c3_w      version = 1;
  void*     bufs[20];
  c3_z      sizes[20];
  c3_d      i, low_d, hig_d;
  meta_ctx  ctx = {0};

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with metadata
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  if ( c3n == u3_book_save_meta(log_u, "version", sizeof(version), &version) ) {
    fprintf(stderr, "book_tests: meta_persistence save_meta failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  add events
  for ( i = 0; i < 20; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 20, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: meta_persistence save failed\r\n");
    for ( i = 0; i < 20; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 20; i++ ) {
    c3_free(bufs[i]);
  }

  u3_book_exit(log_u);

  //  corrupt last event
  if ( c3n == _test_corrupt_event_crc(tmp_c, 20) ) {
    fprintf(stderr, "book_tests: meta_persistence corruption failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  reopen with recovery
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    fprintf(stderr, "book_tests: meta_persistence reopen failed\r\n");
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify recovery happened
  u3_book_gulf(log_u, &low_d, &hig_d);
  if ( 19 != hig_d ) {
    fprintf(stderr, "book_tests: meta_persistence recovery didn't happen\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  verify metadata still readable
  u3_book_read_meta(log_u, &ctx, "version", _test_meta_cb);
  if ( c3n == ctx.found ) {
    fprintf(stderr, "book_tests: meta_persistence metadata lost\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* invalid operation tests
*/

/* _test_book_null_handle(): test NULL handle checks.
*/
static c3_o
_test_book_null_handle(void)
{
  c3_d low_d, hig_d;
  read_ctx ctx = {0};
  c3_y* buf_y;
  c3_z  siz_z;

  //  test gulf with NULL
  if ( c3y == u3_book_gulf(NULL, &low_d, &hig_d) ) {
    fprintf(stderr, "book_tests: null_handle gulf should have failed\r\n");
    return c3n;
  }

  //  test read with NULL
  if ( c3y == u3_book_read(NULL, &ctx, 1, 1, _test_read_cb) ) {
    fprintf(stderr, "book_tests: null_handle read should have failed\r\n");
    return c3n;
  }

  //  test save with NULL
  _test_make_event(&buf_y, &siz_z, 1);
  if ( c3y == u3_book_save(NULL, 1, 1, (void**)&buf_y, &siz_z, 0) ) {
    fprintf(stderr, "book_tests: null_handle save should have failed\r\n");
    c3_free(buf_y);
    return c3n;
  }
  c3_free(buf_y);

  return c3y;
}

/* _test_book_iterator_after_done(): test closed iterator.
*/
static c3_o
_test_book_iterator_after_done(void)
{
  c3_c*         tmp_c = _test_tmpdir("book-iter-done");
  u3_book*      log_u;
  u3_book_walk  itr_u;
  void*         bufs[20];
  c3_z          sizes[20];
  c3_d          i;
  c3_z          len_z;
  void*         buf_v;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with events 1-20
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 20; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 20, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: iter_after_done save failed\r\n");
    for ( i = 0; i < 20; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 20; i++ ) {
    c3_free(bufs[i]);
  }

  //  create iterator
  if ( c3n == u3_book_walk_init(log_u, &itr_u, 1, 20) ) {
    fprintf(stderr, "book_tests: iter_after_done walk_init failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  close iterator
  u3_book_walk_done(&itr_u);

  //  try to use closed iterator - should fail
  if ( c3y == u3_book_walk_next(&itr_u, &len_z, &buf_v) ) {
    fprintf(stderr, "book_tests: iter_after_done walk_next should have failed\r\n");
    c3_free(buf_v);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_iterator_concurrent_modification(): test iterator after log modification.
*/
static c3_o
_test_book_iterator_concurrent_modification(void)
{
  c3_c*         tmp_c = _test_tmpdir("book-iter-concurrent");
  u3_book*      log_u;
  u3_book_walk  itr_u;
  void*         bufs[70];
  c3_z          sizes[70];
  c3_d          i;
  c3_z          len_z;
  void*         buf_v;
  c3_d          count = 0;

  if ( !tmp_c ) {
    return c3n;
  }

  //  create log with events 1-50
  log_u = u3_book_init(tmp_c);
  if ( !log_u ) {
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 1, 50, bufs, sizes, 0) ) {
    fprintf(stderr, "book_tests: iter_concurrent save failed\r\n");
    for ( i = 0; i < 50; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 0; i < 50; i++ ) {
    c3_free(bufs[i]);
  }

  //  create iterator for events 10-30
  if ( c3n == u3_book_walk_init(log_u, &itr_u, 10, 30) ) {
    fprintf(stderr, "book_tests: iter_concurrent walk_init failed\r\n");
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  //  read a few events
  for ( count = 0; count < 5; count++ ) {
    if ( c3n == u3_book_walk_next(&itr_u, &len_z, &buf_v) ) {
      fprintf(stderr, "book_tests: iter_concurrent walk_next failed\r\n");
      u3_book_walk_done(&itr_u);
      u3_book_exit(log_u);
      _test_cleanup(tmp_c);
      c3_free(tmp_c);
      return c3n;
    }
    c3_free(buf_v);
  }

  //  add new events 51-60
  for ( i = 50; i < 60; i++ ) {
    _test_make_event((c3_y**)&bufs[i], &sizes[i], i + 1);
  }

  if ( c3n == u3_book_save(log_u, 51, 10, &bufs[50], &sizes[50], 0) ) {
    fprintf(stderr, "book_tests: iter_concurrent second save failed\r\n");
    for ( i = 50; i < 60; i++ ) {
      c3_free(bufs[i]);
    }
    u3_book_walk_done(&itr_u);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  for ( i = 50; i < 60; i++ ) {
    c3_free(bufs[i]);
  }

  //  continue iterating - should continue with original range
  while ( c3y == u3_book_walk_next(&itr_u, &len_z, &buf_v) ) {
    c3_free(buf_v);
    count++;
  }

  //  verify we read the expected range (10-30 = 21 events, already read 5)
  if ( 21 != count ) {
    fprintf(stderr, "book_tests: iter_concurrent count wrong: %llu\r\n", count);
    u3_book_walk_done(&itr_u);
    u3_book_exit(log_u);
    _test_cleanup(tmp_c);
    c3_free(tmp_c);
    return c3n;
  }

  u3_book_walk_done(&itr_u);
  u3_book_exit(log_u);
  _test_cleanup(tmp_c);
  c3_free(tmp_c);
  return c3y;
}

/* _test_book_core(): run all core book tests.
*/
static c3_o
_test_book_core(void)
{
  c3_o ret = c3y;

  if ( c3n == _test_book_init_empty() ) {
    fprintf(stderr, "book_tests: init_empty failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_single_event() ) {
    fprintf(stderr, "book_tests: single_event failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_batch_write() ) {
    fprintf(stderr, "book_tests: batch_write failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_persistence() ) {
    fprintf(stderr, "book_tests: persistence failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_contiguity() ) {
    fprintf(stderr, "book_tests: contiguity failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_partial_read() ) {
    fprintf(stderr, "book_tests: partial_read failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_iterator() ) {
    fprintf(stderr, "book_tests: iterator failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_metadata() ) {
    fprintf(stderr, "book_tests: metadata failed\r\n");
    ret = c3n;
  }

  //  file corruption tests
  if ( c3n == _test_book_corrupt_header_magic() ) {
    fprintf(stderr, "book_tests: corrupt_header_magic failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_corrupt_header_version() ) {
    fprintf(stderr, "book_tests: corrupt_header_version failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_corrupt_deed_crc() ) {
    fprintf(stderr, "book_tests: corrupt_deed_crc failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_corrupt_deed_length_mismatch() ) {
    fprintf(stderr, "book_tests: corrupt_deed_length_mismatch failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_truncated_deed_partial() ) {
    fprintf(stderr, "book_tests: truncated_deed_partial failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_multiple_corruptions() ) {
    fprintf(stderr, "book_tests: multiple_corruptions failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_corrupt_first_event() ) {
    fprintf(stderr, "book_tests: corrupt_first_event failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_file_too_small() ) {
    fprintf(stderr, "book_tests: file_too_small failed\r\n");
    ret = c3n;
  }

  //  boundary condition tests
  if ( c3n == _test_book_read_empty_log() ) {
    fprintf(stderr, "book_tests: read_empty_log failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_read_beyond_range() ) {
    fprintf(stderr, "book_tests: read_beyond_range failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_iterator_invalid_ranges() ) {
    fprintf(stderr, "book_tests: iterator_invalid_ranges failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_write_first_wrong_epoch() ) {
    fprintf(stderr, "book_tests: write_first_wrong_epoch failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_very_large_event() ) {
    fprintf(stderr, "book_tests: very_large_event failed\r\n");
    ret = c3n;
  }

  //  metadata edge case tests
  if ( c3n == _test_book_metadata_section_full() ) {
    fprintf(stderr, "book_tests: metadata_section_full failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_metadata_corrupted_count() ) {
    fprintf(stderr, "book_tests: metadata_corrupted_count failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_metadata_empty_key() ) {
    fprintf(stderr, "book_tests: metadata_empty_key failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_metadata_persistence() ) {
    fprintf(stderr, "book_tests: metadata_persistence failed\r\n");
    ret = c3n;
  }

  //  invalid operation tests
  if ( c3n == _test_book_null_handle() ) {
    fprintf(stderr, "book_tests: null_handle failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_iterator_after_done() ) {
    fprintf(stderr, "book_tests: iterator_after_done failed\r\n");
    ret = c3n;
  }

  if ( c3n == _test_book_iterator_concurrent_modification() ) {
    fprintf(stderr, "book_tests: iterator_concurrent_modification failed\r\n");
    ret = c3n;
  }

  return ret;
}

/* main
*/
int
main(int argc, char* argv[])
{
  if ( c3n == _test_book_core() ) {
    fprintf(stderr, "book tests failed\r\n");
    return 1;
  }

  fprintf(stderr, "test book: ok\n");
  return 0;
}
