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
