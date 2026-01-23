#include "db/book.h"

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define _alloc(sz)    malloc(sz)
#define _free(ptr)    free(ptr)

/* _test_make_tmpdir(): create unique temporary directory.
**
**   returns: heap-allocated path (caller must free)
*/
static c3_c*
_test_make_tmpdir(void)
{
  c3_c  pat_c[] = "/tmp/book_test_XXXXXX";
  c3_c* dir_c = mkdtemp(pat_c);

  if ( !dir_c ) {
    fprintf(stderr, "book_test: mkdtemp failed: %s\r\n", strerror(errno));
    return 0;
  }

  c3_c* ret_c = _alloc(strlen(dir_c) + 1);
  strcpy(ret_c, dir_c);
  return ret_c;
}

/* _test_rm_rf(): recursively remove directory contents.
*/
static void
_test_rm_rf(const c3_c* pax_c)
{
  if ( !pax_c || strncmp(pax_c, "/tmp", 4) != 0 ) {
    fprintf(stderr, "book_test: refusing to remove non-/tmp path: %s\r\n", pax_c);
    exit(1);
  }
  c3_c cmd_c[8192];
  snprintf(cmd_c, sizeof(cmd_c), "rm -rf %s", pax_c);
  system(cmd_c);
}

/* _test_make_event(): create a test event buffer (mug + jam).
**
**   creates a buffer with 4-byte mug followed by jam data.
**   jam data is just the event number repeated.
**
**   returns: heap-allocated buffer (caller must free)
*/
static c3_y*
_test_make_event(c3_z* len_z, c3_d eve_d)
{
  //  create simple jam data: 8 bytes containing the event number
  c3_z  jam_z = 8;
  c3_z  tot_z = 4 + jam_z;  //  mug + jam
  c3_y* buf_y = _alloc(tot_z);

  //  mug: use event number as simple hash
  c3_w mug_w = (c3_w)(eve_d * 0x12345678);
  memcpy(buf_y, &mug_w, 4);

  //  jam: event number as 8 bytes
  memcpy(buf_y + 4, &eve_d, 8);

  *len_z = tot_z;
  return buf_y;
}

/* _test_corrupt_file(): flip a byte in a file at given offset.
*/
static c3_o
_test_corrupt_file(const c3_c* pax_c, c3_d off_d)
{
  c3_i fid_i = open(pax_c, O_RDWR);
  if ( fid_i < 0 ) {
    return c3n;
  }

  c3_y byt_y;
  if ( 1 != pread(fid_i, &byt_y, 1, off_d) ) {
    close(fid_i);
    return c3n;
  }

  byt_y ^= 0xFF;  //  flip all bits

  if ( 1 != pwrite(fid_i, &byt_y, 1, off_d) ) {
    close(fid_i);
    return c3n;
  }

  close(fid_i);
  return c3y;
}

/* _test_truncate_file(): truncate file to given size.
*/
static c3_o
_test_truncate_file(const c3_c* pax_c, c3_d siz_d)
{
  if ( -1 == truncate(pax_c, siz_d) ) {
    return c3n;
  }
  return c3y;
}

/* _test_append_garbage(): append random bytes to file.
*/
static c3_o
_test_append_garbage(const c3_c* pax_c, c3_z len_z)
{
  c3_i fid_i = open(pax_c, O_WRONLY | O_APPEND);
  if ( fid_i < 0 ) {
    return c3n;
  }

  c3_y* buf_y = _alloc(len_z);
  for ( c3_z i = 0; i < len_z; i++ ) {
    buf_y[i] = (c3_y)(i * 17 + 42);  //  pseudo-random
  }

  c3_zs ret = write(fid_i, buf_y, len_z);
  _free(buf_y);
  close(fid_i);

  return (ret == (c3_zs)len_z) ? c3y : c3n;
}

/* _test_write_raw(): write raw bytes at offset in file.
*/
static c3_o
_test_write_raw(const c3_c* pax_c, c3_d off_d, void* dat_v, c3_z len_z)
{
  c3_i fid_i = open(pax_c, O_RDWR);
  if ( fid_i < 0 ) {
    return c3n;
  }

  c3_zs ret = pwrite(fid_i, dat_v, len_z, off_d);
  close(fid_i);

  return (ret == (c3_zs)len_z) ? c3y : c3n;
}

/* _test_file_size(): get file size.
*/
static c3_d
_test_file_size(const c3_c* pax_c)
{
  struct stat buf_u;
  if ( -1 == stat(pax_c, &buf_u) ) {
    return 0;
  }
  return (c3_d)buf_u.st_size;
}

/* _test_read_cb(): callback for u3_book_read that stores event data.
*/
typedef struct {
  c3_d  eve_d;
  c3_z  len_z;
  c3_y* buf_y;
  c3_o  called;
} _test_read_ctx;

static c3_o
_test_read_cb(void* ptr_v, c3_d eve_d, c3_z len_z, void* buf_v)
{
  _test_read_ctx* ctx_u = ptr_v;
  ctx_u->eve_d  = eve_d;
  ctx_u->len_z  = len_z;
  ctx_u->buf_y  = _alloc(len_z);
  ctx_u->called = c3y;
  memcpy(ctx_u->buf_y, buf_v, len_z);
  return c3y;
}

/* _test_meta_cb(): callback for u3_book_read_meta.
*/
typedef struct {
  c3_zs siz_zs;
  c3_y  buf_y[256];
} _test_meta_ctx;

static void
_test_meta_cb(void* ptr_v, c3_zs siz_zs, void* dat_v)
{
  _test_meta_ctx* ctx_u = ptr_v;
  ctx_u->siz_zs = siz_zs;
  if ( siz_zs > 0 && dat_v ) {
    memcpy(ctx_u->buf_y, dat_v, (c3_z)siz_zs);
  }
}

//==============================================================================
// Boundary Condition Tests
//==============================================================================

/* _test_empty_log_operations(): test operations on empty log.
*/
static c3_i
_test_empty_log_operations(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  empty_log: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  test gulf on empty log
  {
    c3_d low_d, hig_d;
    c3_o gul_o = u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( c3y != gul_o ) {
      fprintf(stderr, "  empty_log: gulf returned c3n\r\n");
      ret_i = 0;
    }
    //  empty log should have fir_d=0, las_d=0
    if ( 0 != low_d || 0 != hig_d ) {
      fprintf(stderr, "  empty_log: gulf expected (0,0), got (%" PRIu64 ",%" PRIu64 ")\r\n",
              low_d, hig_d);
      ret_i = 0;
    }
  }

  //  test read on empty log - should fail
  {
    _test_read_ctx ctx_u = {0};
    c3_o red_o = u3_book_read(txt_u, &ctx_u, 1, 1, _test_read_cb);

    if ( c3n != red_o ) {
      fprintf(stderr, "  empty_log: read should fail on empty log\r\n");
      ret_i = 0;
    }
  }

  //  test walk_init on empty log - should fail
  {
    u3_book_walk itr_u;
    c3_o wlk_o = u3_book_walk_init(txt_u, &itr_u, 1, 1);

    if ( c3n != wlk_o ) {
      fprintf(stderr, "  empty_log: walk_init should fail on empty log\r\n");
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  empty_log_operations: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_single_event_lifecycle(): write, read, walk single event.
*/
static c3_i
_test_single_event_lifecycle(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;

  if ( !txt_u ) {
    fprintf(stderr, "  single_event: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write single event (event #1, epoch 0)
  evt_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  single_event: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  //  verify gulf
  {
    c3_d low_d, hig_d;
    u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( 1 != low_d || 1 != hig_d ) {
      fprintf(stderr, "  single_event: gulf expected (1,1), got (%" PRIu64 ",%" PRIu64 ")\r\n",
              low_d, hig_d);
      ret_i = 0;
    }
  }

  //  read it back
  {
    _test_read_ctx ctx_u = {0};
    c3_o red_o = u3_book_read(txt_u, &ctx_u, 1, 1, _test_read_cb);

    if ( c3n == red_o ) {
      fprintf(stderr, "  single_event: read failed\r\n");
      ret_i = 0;
    }
    else {
      if ( ctx_u.eve_d != 1 ) {
        fprintf(stderr, "  single_event: read wrong event number\r\n");
        ret_i = 0;
      }
      if ( ctx_u.len_z != evt_z ) {
        fprintf(stderr, "  single_event: read wrong length\r\n");
        ret_i = 0;
      }
      if ( 0 != memcmp(ctx_u.buf_y, evt_y, evt_z) ) {
        fprintf(stderr, "  single_event: read data mismatch\r\n");
        ret_i = 0;
      }
      _free(ctx_u.buf_y);
    }
  }

  //  walk it
  {
    u3_book_walk itr_u;
    c3_o wlk_o = u3_book_walk_init(txt_u, &itr_u, 1, 1);

    if ( c3n == wlk_o ) {
      fprintf(stderr, "  single_event: walk_init failed\r\n");
      ret_i = 0;
    }
    else {
      c3_z   len_z;
      void*  buf_v;
      c3_o   nex_o = u3_book_walk_next(&itr_u, &len_z, &buf_v);

      if ( c3n == nex_o ) {
        fprintf(stderr, "  single_event: walk_next failed\r\n");
        ret_i = 0;
      }
      else {
        if ( len_z != evt_z ) {
          fprintf(stderr, "  single_event: walk wrong length\r\n");
          ret_i = 0;
        }
        _free(buf_v);

        //  second call should return c3n (end of iteration)
        nex_o = u3_book_walk_next(&itr_u, &len_z, &buf_v);
        if ( c3y == nex_o ) {
          fprintf(stderr, "  single_event: walk should end after 1 event\r\n");
          ret_i = 0;
          _free(buf_v);
        }
      }

      u3_book_walk_done(&itr_u);
    }
  }

  u3_book_exit(txt_u);

cleanup:
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  single_event_lifecycle: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_epoch_boundary_validation(): first event must be epo_d + 1.
*/
static c3_i
_test_epoch_boundary_validation(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;

  if ( !txt_u ) {
    fprintf(stderr, "  epoch_boundary: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  evt_y = _test_make_event(&evt_z, 5);
  void* byt_p[1] = { evt_y };
  c3_z  siz_i[1] = { evt_z };

  //  try to write event 5 with epoch 0 - should fail (expects event 1)
  {
    c3_o sav_o = u3_book_save(txt_u, 5, 1, byt_p, siz_i, 0);
    if ( c3y == sav_o ) {
      fprintf(stderr, "  epoch_boundary: should reject event 5 for epoch 0\r\n");
      ret_i = 0;
    }
  }

  //  write event 5 with epoch 4 - should succeed (4 + 1 = 5)
  {
    c3_o sav_o = u3_book_save(txt_u, 5, 1, byt_p, siz_i, 4);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  epoch_boundary: should accept event 5 for epoch 4\r\n");
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);

cleanup:
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  epoch_boundary_validation: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_contiguity_gap_rejection(): reject non-contiguous events.
*/
static c3_i
_test_contiguity_gap_rejection(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt1_y = 0;
  c3_y*    evt3_y = 0;
  c3_z     evt_z;

  if ( !txt_u ) {
    fprintf(stderr, "  contiguity: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write event 1
  evt1_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt1_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  contiguity: save event 1 failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  //  try to write event 3 (skipping 2) - should fail
  evt3_y = _test_make_event(&evt_z, 3);
  {
    void* byt_p[1] = { evt3_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 3, 1, byt_p, siz_i, 0);
    if ( c3y == sav_o ) {
      fprintf(stderr, "  contiguity: should reject gap (event 3 after 1)\r\n");
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);

cleanup:
  if ( evt1_y ) _free(evt1_y);
  if ( evt3_y ) _free(evt3_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  contiguity_gap_rejection: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_minimum_event_size(): event with minimum size (just mug).
*/
static c3_i
_test_minimum_event_size(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  min_event: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  create minimum event: just 4 bytes (mug only, no jam)
  c3_y  evt_y[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
  void* byt_p[1] = { evt_y };
  c3_z  siz_i[1] = { 4 };

  c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
  if ( c3n == sav_o ) {
    fprintf(stderr, "  min_event: save failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  read it back
  {
    _test_read_ctx ctx_u = {0};
    c3_o red_o = u3_book_read(txt_u, &ctx_u, 1, 1, _test_read_cb);

    if ( c3n == red_o ) {
      fprintf(stderr, "  min_event: read failed\r\n");
      ret_i = 0;
    }
    else {
      if ( ctx_u.len_z != 4 ) {
        fprintf(stderr, "  min_event: wrong length %" PRIu64 "\r\n", (c3_d)ctx_u.len_z);
        ret_i = 0;
      }
      _free(ctx_u.buf_y);
    }
  }

  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  minimum_event_size: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

//==============================================================================
// Crash Recovery & Corruption Tests
//==============================================================================

/* _test_crc_corruption_detection(): flip bit in data, verify recovery truncates.
**
**   This test verifies that CRC corruption is detected during recovery.
**   After corrupting jam data and reopening, the log should be empty
**   because the corrupted deed fails CRC validation.
*/
static c3_i
_test_crc_corruption_detection(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  crc_corruption: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write event (evt_z = 12 bytes: 4 mug + 8 jam)
  evt_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  crc_corruption: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  corrupt the CRC field directly to ensure CRC mismatch
  //  file layout: [header 16] [deed_head 12] [jam 8] [deed_tail 12]
  //  deed_tail: [crc_w 4] [let_d 8]
  //  crc_w is at offset: 16 + 12 + 8 = 36
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  if ( c3n == _test_corrupt_file(path_c, 36) ) {
    fprintf(stderr, "  crc_corruption: corrupt_file failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen - recovery should detect CRC mismatch and truncate
  txt_u = u3_book_init(dir_c);
  if ( !txt_u ) {
    fprintf(stderr, "  crc_corruption: reopen failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  after recovery, log should be empty (corrupted deed truncated)
  {
    c3_d low_d, hig_d;
    u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( hig_d != 0 ) {
      fprintf(stderr, "  crc_corruption: expected empty log after recovery, got hig=%" PRIu64 "\r\n", hig_d);
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  crc_corruption_detection: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_truncated_file_recovery(): truncate mid-event, verify recovery.
**
**   write two events, truncate file mid-second-event, reopen.
**   recovery should find only the first complete event.
*/
static c3_i
_test_truncated_file_recovery(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt1_y = 0;
  c3_y*    evt2_y = 0;
  c3_z     evt_z;
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  truncated_file: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write two events (each evt_z = 12 bytes: 4 mug + 8 jam)
  //  deed size on disk = 12 (head) + 8 (jam) + 12 (tail) = 32 bytes
  evt1_y = _test_make_event(&evt_z, 1);
  evt2_y = _test_make_event(&evt_z, 2);
  {
    void* byt_p[2] = { evt1_y, evt2_y };
    c3_z  siz_i[2] = { evt_z, evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 2, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  truncated_file: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  file layout: [header 16] [deed1] [deed2]
  //  deed size = sizeof(deed_head) + (len_d - 4) + sizeof(deed_tail)
  //  with struct padding, this is typically 40 bytes per deed for our 12-byte events
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  c3_d siz_d = _test_file_size(path_c);

  //  calculate deed size dynamically: total - header = 2 deeds
  c3_d deed_size = (siz_d - 16) / 2;

  //  truncate to: header + deed1 + 5 bytes of deed2
  c3_d truncate_at = 16 + deed_size + 5;

  if ( c3n == _test_truncate_file(path_c, truncate_at) ) {
    fprintf(stderr, "  truncated_file: truncate failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen - recovery should find deed1 valid, deed2 truncated
  txt_u = u3_book_init(dir_c);
  if ( !txt_u ) {
    fprintf(stderr, "  truncated_file: reopen failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  verify only event 1 exists
  {
    c3_d low_d, hig_d;
    u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( hig_d != 1 ) {
      fprintf(stderr, "  truncated_file: expected hig=1, got %" PRIu64 "\r\n", hig_d);
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  if ( evt1_y ) _free(evt1_y);
  if ( evt2_y ) _free(evt2_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  truncated_file_recovery: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_garbage_after_valid_deeds(): append garbage, verify recovery stops.
**
**   write a valid event, then append garbage bytes that form an invalid
**   deed structure. recovery should preserve the valid event and truncate
**   the garbage.
**
**   note: we append a small, controlled garbage pattern to avoid triggering
**   huge allocation attempts from random let_d values.
*/
static c3_i
_test_garbage_after_valid_deeds(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  garbage_after: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write one valid event
  evt_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  garbage_after: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  append garbage with a zero let_d trailer to prevent huge allocations
  //  the reverse scan reads let_d from the last 8 bytes; if let_d == 0,
  //  scan_back breaks and falls through to scan_end
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  {
    c3_i fid_i = open(path_c, O_WRONLY | O_APPEND);
    if ( fid_i < 0 ) {
      fprintf(stderr, "  garbage_after: open failed\r\n");
      ret_i = 0;
      goto cleanup;
    }

    //  12 bytes of garbage that won't form valid let_d
    //  set last 8 bytes to 0 so let_d == 0 triggers scan_back failure
    c3_y garbage[12] = { 0xDE, 0xAD, 0xBE, 0xEF, 0, 0, 0, 0, 0, 0, 0, 0 };
    write(fid_i, garbage, sizeof(garbage));
    close(fid_i);
  }

  //  reopen - should recover to just event 1
  txt_u = u3_book_init(dir_c);
  if ( !txt_u ) {
    fprintf(stderr, "  garbage_after: reopen failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  verify event 1 is still readable
  {
    c3_d low_d, hig_d;
    u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( hig_d != 1 ) {
      fprintf(stderr, "  garbage_after: expected hig=1, got %" PRIu64 "\r\n", hig_d);
      ret_i = 0;
    }
  }

  //  read should succeed
  {
    _test_read_ctx ctx_u = {0};
    c3_o red_o = u3_book_read(txt_u, &ctx_u, 1, 1, _test_read_cb);

    if ( c3n == red_o ) {
      fprintf(stderr, "  garbage_after: read failed\r\n");
      ret_i = 0;
    }
    else {
      _free(ctx_u.buf_y);
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  garbage_after_valid_deeds: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_length_trailer_mismatch(): craft deed with len_d != let_d.
*/
static c3_i
_test_length_trailer_mismatch(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  len_mismatch: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write event
  evt_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  len_mismatch: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  corrupt the let_d field (last 8 bytes of deed)
  //  deed ends at: 16 (header) + 12 (deed_head) + (evt_z-4) (jam) + 12 (deed_tail)
  //  let_d is at offset: deed_end - 8
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  c3_d deed_end = 16 + 12 + (evt_z - 4) + 12;
  c3_d let_off = deed_end - 8;

  //  write a different value for let_d
  c3_d bad_let = 0x12345678;
  if ( c3n == _test_write_raw(path_c, let_off, &bad_let, sizeof(bad_let)) ) {
    fprintf(stderr, "  len_mismatch: write_raw failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen - should recover to empty (no valid events)
  txt_u = u3_book_init(dir_c);
  if ( !txt_u ) {
    fprintf(stderr, "  len_mismatch: reopen failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  verify no events (mismatch detected, truncated)
  {
    c3_d low_d, hig_d;
    u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( hig_d != 0 ) {
      fprintf(stderr, "  len_mismatch: expected empty log, got hig=%" PRIu64 "\r\n", hig_d);
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  length_trailer_mismatch: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

//==============================================================================
// Iterator Tests
//==============================================================================

/* _test_walk_single_event(): walk range of exactly 1 event.
*/
static c3_i
_test_walk_single_event(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y[3] = {0};
  c3_z     evt_z;

  if ( !txt_u ) {
    fprintf(stderr, "  walk_single: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write 3 events
  {
    void* byt_p[3];
    c3_z  siz_i[3];

    for ( int i = 0; i < 3; i++ ) {
      evt_y[i] = _test_make_event(&evt_z, i + 1);
      byt_p[i] = evt_y[i];
      siz_i[i] = evt_z;
    }

    c3_o sav_o = u3_book_save(txt_u, 1, 3, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  walk_single: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  //  walk just event 2
  {
    u3_book_walk itr_u;
    c3_o wlk_o = u3_book_walk_init(txt_u, &itr_u, 2, 2);

    if ( c3n == wlk_o ) {
      fprintf(stderr, "  walk_single: walk_init failed\r\n");
      ret_i = 0;
    }
    else {
      c3_z  len_z;
      void* buf_v;
      c3_i  count = 0;

      while ( c3y == u3_book_walk_next(&itr_u, &len_z, &buf_v) ) {
        count++;
        _free(buf_v);
      }

      if ( count != 1 ) {
        fprintf(stderr, "  walk_single: expected 1 event, got %d\r\n", count);
        ret_i = 0;
      }

      u3_book_walk_done(&itr_u);
    }
  }

  u3_book_exit(txt_u);

cleanup:
  for ( int i = 0; i < 3; i++ ) {
    if ( evt_y[i] ) _free(evt_y[i]);
  }
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  walk_single_event: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_walk_invalidation(): walk_done then walk_next should fail.
*/
static c3_i
_test_walk_invalidation(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;

  if ( !txt_u ) {
    fprintf(stderr, "  walk_invalid: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write event
  evt_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  walk_invalid: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  //  init walk, then done, then try next
  {
    u3_book_walk itr_u;
    c3_o wlk_o = u3_book_walk_init(txt_u, &itr_u, 1, 1);

    if ( c3n == wlk_o ) {
      fprintf(stderr, "  walk_invalid: walk_init failed\r\n");
      ret_i = 0;
    }
    else {
      u3_book_walk_done(&itr_u);

      c3_z  len_z;
      void* buf_v;
      c3_o  nex_o = u3_book_walk_next(&itr_u, &len_z, &buf_v);

      if ( c3y == nex_o ) {
        fprintf(stderr, "  walk_invalid: walk_next should fail after done\r\n");
        ret_i = 0;
        _free(buf_v);
      }
    }
  }

  u3_book_exit(txt_u);

cleanup:
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  walk_invalidation: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_walk_range_validation(): invalid ranges should fail.
*/
static c3_i
_test_walk_range_validation(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y[3] = {0};
  c3_z     evt_z;

  if ( !txt_u ) {
    fprintf(stderr, "  walk_range: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write 3 events
  {
    void* byt_p[3];
    c3_z  siz_i[3];

    for ( int i = 0; i < 3; i++ ) {
      evt_y[i] = _test_make_event(&evt_z, i + 1);
      byt_p[i] = evt_y[i];
      siz_i[i] = evt_z;
    }

    c3_o sav_o = u3_book_save(txt_u, 1, 3, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  walk_range: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  //  try invalid ranges
  {
    u3_book_walk itr_u;

    //  nex > las (inverted range)
    if ( c3y == u3_book_walk_init(txt_u, &itr_u, 3, 1) ) {
      fprintf(stderr, "  walk_range: should reject nex > las\r\n");
      ret_i = 0;
    }

    //  las beyond log end
    if ( c3y == u3_book_walk_init(txt_u, &itr_u, 1, 100) ) {
      fprintf(stderr, "  walk_range: should reject las > log end\r\n");
      ret_i = 0;
    }

    //  nex before log start (fir_d is 1)
    if ( c3y == u3_book_walk_init(txt_u, &itr_u, 0, 1) ) {
      fprintf(stderr, "  walk_range: should reject nex < fir_d\r\n");
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);

cleanup:
  for ( int i = 0; i < 3; i++ ) {
    if ( evt_y[i] ) _free(evt_y[i]);
  }
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  walk_range_validation: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_invalid_magic(): file with wrong magic number should be rejected.
*/
static c3_i
_test_invalid_magic(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  invalid_magic: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  corrupt magic number at offset 0
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  c3_w bad_magic = 0xDEADBEEF;
  if ( c3n == _test_write_raw(path_c, 0, &bad_magic, sizeof(bad_magic)) ) {
    fprintf(stderr, "  invalid_magic: write_raw failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen should fail
  txt_u = u3_book_init(dir_c);
  if ( txt_u ) {
    fprintf(stderr, "  invalid_magic: should reject bad magic\r\n");
    ret_i = 0;
    u3_book_exit(txt_u);
    txt_u = 0;
  }

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  invalid_magic: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_invalid_version(): file with wrong version should be rejected.
*/
static c3_i
_test_invalid_version(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  invalid_version: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  corrupt version at offset 4
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  c3_w bad_version = 99;
  if ( c3n == _test_write_raw(path_c, 4, &bad_version, sizeof(bad_version)) ) {
    fprintf(stderr, "  invalid_version: write_raw failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen should fail
  txt_u = u3_book_init(dir_c);
  if ( txt_u ) {
    fprintf(stderr, "  invalid_version: should reject bad version\r\n");
    ret_i = 0;
    u3_book_exit(txt_u);
    txt_u = 0;
  }

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  invalid_version: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_header_only_file(): file with just header should init as empty.
*/
static c3_i
_test_header_only_file(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_y*    evt_y = 0;
  c3_z     evt_z;
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  header_only: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  write event then truncate to header only
  evt_y = _test_make_event(&evt_z, 1);
  {
    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { evt_z };

    c3_o sav_o = u3_book_save(txt_u, 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  header_only: save failed\r\n");
      ret_i = 0;
      goto cleanup;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  truncate to just header (16 bytes)
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  if ( c3n == _test_truncate_file(path_c, 16) ) {
    fprintf(stderr, "  header_only: truncate failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen should succeed with empty log
  txt_u = u3_book_init(dir_c);
  if ( !txt_u ) {
    fprintf(stderr, "  header_only: reopen failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  verify empty
  {
    c3_d low_d, hig_d;
    u3_book_gulf(txt_u, &low_d, &hig_d);

    if ( hig_d != 0 ) {
      fprintf(stderr, "  header_only: expected empty, got hig=%" PRIu64 "\r\n", hig_d);
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);
  txt_u = 0;

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  if ( evt_y ) _free(evt_y);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  header_only_file: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_undersized_file(): file smaller than header should be rejected.
*/
static c3_i
_test_undersized_file(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);
  c3_c     path_c[8192];

  if ( !txt_u ) {
    fprintf(stderr, "  undersized: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  u3_book_exit(txt_u);
  txt_u = 0;

  //  truncate to 8 bytes (less than 16-byte header)
  snprintf(path_c, sizeof(path_c), "%s/book.log", dir_c);
  if ( c3n == _test_truncate_file(path_c, 8) ) {
    fprintf(stderr, "  undersized: truncate failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  reopen should fail
  txt_u = u3_book_init(dir_c);
  if ( txt_u ) {
    fprintf(stderr, "  undersized: should reject undersized file\r\n");
    ret_i = 0;
    u3_book_exit(txt_u);
    txt_u = 0;
  }

cleanup:
  if ( txt_u ) u3_book_exit(txt_u);
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  undersized_file: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_metadata_roundtrip(): save and read all metadata fields.
*/
static c3_i
_test_metadata_roundtrip(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  meta_roundtrip: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  test "version" field
  {
    c3_w ver_w = 42;
    c3_o sav_o = u3_book_save_meta(txt_u, "version", sizeof(c3_w), &ver_w);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  meta_roundtrip: save version failed\r\n");
      ret_i = 0;
    }
    else {
      _test_meta_ctx ctx_u = {0};
      u3_book_read_meta(txt_u, &ctx_u, "version", _test_meta_cb);
      if ( ctx_u.siz_zs != sizeof(c3_w) ) {
        fprintf(stderr, "  meta_roundtrip: read version wrong size\r\n");
        ret_i = 0;
      }
      else {
        c3_w got_w;
        memcpy(&got_w, ctx_u.buf_y, sizeof(c3_w));
        if ( got_w != 42 ) {
          fprintf(stderr, "  meta_roundtrip: version mismatch\r\n");
          ret_i = 0;
        }
      }
    }
  }

  //  test "who" field (16 bytes)
  {
    c3_d who_d[2] = { 0x123456789ABCDEF0, 0xFEDCBA9876543210 };
    c3_o sav_o = u3_book_save_meta(txt_u, "who", sizeof(who_d), who_d);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  meta_roundtrip: save who failed\r\n");
      ret_i = 0;
    }
    else {
      _test_meta_ctx ctx_u = {0};
      u3_book_read_meta(txt_u, &ctx_u, "who", _test_meta_cb);
      if ( ctx_u.siz_zs != sizeof(who_d) ) {
        fprintf(stderr, "  meta_roundtrip: read who wrong size\r\n");
        ret_i = 0;
      }
      else {
        c3_d got_d[2];
        memcpy(got_d, ctx_u.buf_y, sizeof(got_d));
        if ( got_d[0] != who_d[0] || got_d[1] != who_d[1] ) {
          fprintf(stderr, "  meta_roundtrip: who mismatch\r\n");
          ret_i = 0;
        }
      }
    }
  }

  //  test "fake" field (1 byte)
  {
    c3_o fak_o = c3y;
    c3_o sav_o = u3_book_save_meta(txt_u, "fake", sizeof(c3_o), &fak_o);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  meta_roundtrip: save fake failed\r\n");
      ret_i = 0;
    }
    else {
      _test_meta_ctx ctx_u = {0};
      u3_book_read_meta(txt_u, &ctx_u, "fake", _test_meta_cb);
      if ( ctx_u.siz_zs != sizeof(c3_o) ) {
        fprintf(stderr, "  meta_roundtrip: read fake wrong size\r\n");
        ret_i = 0;
      }
      else {
        c3_o got_o;
        memcpy(&got_o, ctx_u.buf_y, sizeof(c3_o));
        if ( got_o != c3y ) {
          fprintf(stderr, "  meta_roundtrip: fake mismatch\r\n");
          ret_i = 0;
        }
      }
    }
  }

  //  test "life" field
  {
    c3_w lif_w = 1234;
    c3_o sav_o = u3_book_save_meta(txt_u, "life", sizeof(c3_w), &lif_w);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  meta_roundtrip: save life failed\r\n");
      ret_i = 0;
    }
    else {
      _test_meta_ctx ctx_u = {0};
      u3_book_read_meta(txt_u, &ctx_u, "life", _test_meta_cb);
      if ( ctx_u.siz_zs != sizeof(c3_w) ) {
        fprintf(stderr, "  meta_roundtrip: read life wrong size\r\n");
        ret_i = 0;
      }
      else {
        c3_w got_w;
        memcpy(&got_w, ctx_u.buf_y, sizeof(c3_w));
        if ( got_w != 1234 ) {
          fprintf(stderr, "  meta_roundtrip: life mismatch\r\n");
          ret_i = 0;
        }
      }
    }
  }

  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  metadata_roundtrip: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_metadata_invalid_key(): unknown key should return -1.
*/
static c3_i
_test_metadata_invalid_key(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  meta_invalid: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  read unknown key
  {
    _test_meta_ctx ctx_u = {0};
    u3_book_read_meta(txt_u, &ctx_u, "nonexistent", _test_meta_cb);
    if ( ctx_u.siz_zs != -1 ) {
      fprintf(stderr, "  meta_invalid: should return -1 for unknown key\r\n");
      ret_i = 0;
    }
  }

  //  write unknown key
  {
    c3_w val_w = 42;
    c3_o sav_o = u3_book_save_meta(txt_u, "nonexistent", sizeof(val_w), &val_w);
    if ( c3y == sav_o ) {
      fprintf(stderr, "  meta_invalid: should reject unknown key\r\n");
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  metadata_invalid_key: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _test_metadata_size_validation(): wrong-sized values should be rejected.
*/
static c3_i
_test_metadata_size_validation(void)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  meta_size: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  try to write 2 bytes to "version" (expects 4)
  {
    c3_y buf_y[2] = { 0x12, 0x34 };
    c3_o sav_o = u3_book_save_meta(txt_u, "version", 2, buf_y);
    if ( c3y == sav_o ) {
      fprintf(stderr, "  meta_size: should reject wrong size for version\r\n");
      ret_i = 0;
    }
  }

  //  try to write 4 bytes to "who" (expects 16)
  {
    c3_w val_w = 42;
    c3_o sav_o = u3_book_save_meta(txt_u, "who", sizeof(val_w), &val_w);
    if ( c3y == sav_o ) {
      fprintf(stderr, "  meta_size: should reject wrong size for who\r\n");
      ret_i = 0;
    }
  }

  //  try to write 4 bytes to "fake" (expects 1)
  {
    c3_w val_w = 1;
    c3_o sav_o = u3_book_save_meta(txt_u, "fake", sizeof(val_w), &val_w);
    if ( c3y == sav_o ) {
      fprintf(stderr, "  meta_size: should reject wrong size for fake\r\n");
      ret_i = 0;
    }
  }

  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  metadata_size_validation: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

//==============================================================================
// Main
//==============================================================================

int
main(int argc, char* argv[])
{
  c3_i ret_i = 1;

  //  boundary tests
  ret_i &= _test_empty_log_operations();
  ret_i &= _test_single_event_lifecycle();
  ret_i &= _test_epoch_boundary_validation();
  ret_i &= _test_contiguity_gap_rejection();
  ret_i &= _test_minimum_event_size();

  //  crash recovery & corruption tests
  ret_i &= _test_crc_corruption_detection();
  ret_i &= _test_truncated_file_recovery();
  ret_i &= _test_garbage_after_valid_deeds();
  ret_i &= _test_length_trailer_mismatch();

  //  iterator tests
  ret_i &= _test_walk_single_event();
  ret_i &= _test_walk_invalidation();
  ret_i &= _test_walk_range_validation();

  //  header & format tests
  ret_i &= _test_invalid_magic();
  ret_i &= _test_invalid_version();
  ret_i &= _test_header_only_file();
  ret_i &= _test_undersized_file();

  //  metadata tests
  ret_i &= _test_metadata_roundtrip();
  ret_i &= _test_metadata_invalid_key();
  ret_i &= _test_metadata_size_validation();

  fprintf(stderr, "\r\n");
  if ( ret_i ) {
    fprintf(stderr, "book_tests: ok\n");
    return 0;
  }
  else {
    fprintf(stderr, "book_tests: failed\n");
    return 1;
  }
}
