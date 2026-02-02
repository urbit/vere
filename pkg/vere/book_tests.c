#include "db/book.h"

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define _alloc(sz)    malloc(sz)
#define _free(ptr)    free(ptr)

/* _test_make_tmpdir(): create unique temporary directory with epoch subdir.
**
**   creates /tmp/book_test_XXXXXX/0i0 and returns the epoch path.
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

  //  create epoch subdirectory 0i0
  c3_c epo_c[256];
  snprintf(epo_c, sizeof(epo_c), "%s/0i0", dir_c);
  if ( -1 == mkdir(epo_c, 0755) ) {
    fprintf(stderr, "book_test: mkdir failed: %s\r\n", strerror(errno));
    return 0;
  }

  c3_c* ret_c = _alloc(strlen(epo_c) + 1);
  strcpy(ret_c, epo_c);
  return ret_c;
}

/* _test_rm_rf(): recursively remove directory contents.
**
**   expects epoch path like /tmp/book_test_XXXXXX/0i0
**   removes the parent directory (the whole test dir)
*/
static void
_test_rm_rf(const c3_c* pax_c)
{
  if ( !pax_c || strncmp(pax_c, "/tmp", 4) != 0 ) {
    fprintf(stderr, "book_test: refusing to remove non-/tmp path: %s\r\n", pax_c);
    exit(1);
  }

  //  strip epoch suffix to get parent tmpdir
  c3_c* par_c = strdup(pax_c);
  c3_c* las_c = strrchr(par_c, '/');
  if ( las_c ) *las_c = '\0';

  c3_c cmd_c[8192];
  snprintf(cmd_c, sizeof(cmd_c), "rm -rf %s", par_c);
  system(cmd_c);
  free(par_c);
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
  //  deed size on disk = 12 (head) + 8 (jam) + 8 (tail) = 28 bytes
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

  //  file layout: [header] [deed1] [deed2]
  //  deed size = sizeof(deed_head) + (len_d - 4) + sizeof(deed_tail)
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
// Benchmarks
//==============================================================================

/* _bench_make_event(): create a dummy event of specified size.
**
**   creates a buffer with 4-byte mug followed by dummy data.
**   the data is filled with a pattern based on the event number.
**
**   returns: heap-allocated buffer (caller must free)
*/
static c3_y*
_bench_make_event(c3_z siz_z, c3_d eve_d)
{
  c3_y* buf_y = _alloc(siz_z);

  //  mug: simple hash from event number
  c3_w mug_w = (c3_w)(eve_d * 0x12345678);
  memcpy(buf_y, &mug_w, 4);

  //  fill remaining bytes with pattern
  for ( c3_z i = 4; i < siz_z; i++ ) {
    buf_y[i] = (c3_y)((eve_d + i) & 0xFF);
  }

  return buf_y;
}

/* _bench_get_time_ns(): get current time in nanoseconds.
*/
static c3_d
_bench_get_time_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (c3_d)ts.tv_sec * 1000000000ULL + (c3_d)ts.tv_nsec;
}

/* _bench_write_speed(): benchmark write performance.
**
**   writes [num_d] events of [siz_z] bytes each, one at a time.
**   reports total time, events/sec, MB/s, and per-event latency.
*/
static c3_i
_bench_write_speed(c3_d num_d, c3_z siz_z)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  write_speed: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  pre-allocate event buffer (reuse for all writes)
  c3_y* evt_y = _bench_make_event(siz_z, 1);

  //  start timing
  c3_d beg_d = _bench_get_time_ns();

  //  write events one at a time
  for ( c3_d i = 0; i < num_d; i++ ) {
    //  update event data pattern for variety
    c3_w mug_w = (c3_w)((i + 1) * 0x12345678);
    memcpy(evt_y, &mug_w, 4);

    void* byt_p[1] = { evt_y };
    c3_z  siz_i[1] = { siz_z };

    c3_o sav_o = u3_book_save(txt_u, i + 1, 1, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  write_speed: save failed at event %" PRIu64 "\r\n", i + 1);
      ret_i = 0;
      _free(evt_y);
      goto cleanup;
    }
  }

  //  end timing
  c3_d end_d = _bench_get_time_ns();
  c3_d lap_d = end_d - beg_d;  //  elapsed nanoseconds

  //  calculate metrics
  double elapsed_sec = (double)lap_d / 1e9;
  double events_per_sec = (double)num_d / elapsed_sec;
  double total_bytes = (double)num_d * (double)siz_z;
  double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / elapsed_sec;
  double us_per_event = ((double)lap_d / 1000.0) / (double)num_d;

  //  report results
  fprintf(stderr, "\r\n");
  fprintf(stderr, "  write_speed benchmark (single-event writes):\r\n");
  fprintf(stderr, "    events written:  %" PRIu64 "\r\n", num_d);
  fprintf(stderr, "    event size:      %" PRIu64 " bytes\r\n", (c3_d)siz_z);
  fprintf(stderr, "    total data:      %.2f MB\r\n", total_bytes / (1024.0 * 1024.0));
  fprintf(stderr, "    total time:      %.3f seconds\r\n", elapsed_sec);
  fprintf(stderr, "    write speed:     %.0f events/sec\r\n", events_per_sec);
  fprintf(stderr, "    throughput:      %.2f MB/sec\r\n", mb_per_sec);
  fprintf(stderr, "    latency:         %.1f us/event\r\n", us_per_event);
  fprintf(stderr, "\r\n");

  _free(evt_y);
  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  write_speed_benchmark: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _bench_write_speed_batched(): benchmark batched write performance.
**
**   writes [num_d] events of [siz_z] bytes in batches of [bat_d].
**   reports total time, events/sec, MB/s, and per-event latency.
*/
static c3_i
_bench_write_speed_batched(c3_d num_d, c3_z siz_z, c3_d bat_d)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  u3_book* txt_u = u3_book_init(dir_c);

  if ( !txt_u ) {
    fprintf(stderr, "  write_speed_batched: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  allocate batch arrays
  c3_y** evt_y = _alloc(bat_d * sizeof(c3_y*));
  void** byt_p = _alloc(bat_d * sizeof(void*));
  c3_z*  siz_i = _alloc(bat_d * sizeof(c3_z));

  //  pre-allocate event buffers for batch
  for ( c3_d i = 0; i < bat_d; i++ ) {
    evt_y[i] = _bench_make_event(siz_z, i + 1);
    byt_p[i] = evt_y[i];
    siz_i[i] = siz_z;
  }

  //  start timing
  c3_d start_d = _bench_get_time_ns();

  //  write events in batches
  c3_d wit_d = 0;  //  counter
  while ( wit_d < num_d ) {
    c3_d remaining = num_d - wit_d;
    c3_d batch_size = (remaining < bat_d) ? remaining : bat_d;

    //  update event data patterns
    for ( c3_d i = 0; i < batch_size; i++ ) {
      c3_w mug_w = (c3_w)((wit_d + i + 1) * 0x12345678);
      memcpy(evt_y[i], &mug_w, 4);
    }

    c3_o sav_o = u3_book_save(txt_u, wit_d + 1, batch_size, byt_p, siz_i, 0);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  write_speed_batched: save failed at event %" PRIu64 "\r\n",
              wit_d + 1);
      ret_i = 0;
      goto cleanup_buffers;
    }

    wit_d += batch_size;
  }

  //  end timing
  c3_d end_d = _bench_get_time_ns();
  c3_d lap_d = end_d - start_d;  //  nanoseconds

  //  calculate metrics
  double elapsed_sec = (double)lap_d / 1e9;
  double events_per_sec = (double)num_d / elapsed_sec;
  double total_bytes = (double)num_d * (double)siz_z;
  double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / elapsed_sec;
  double us_per_event = ((double)lap_d / 1000.0) / (double)num_d;

  //  report results
  fprintf(stderr, "\r\n");
  fprintf(stderr, "  write_speed benchmark (batched writes, batch=%" PRIu64 "):\r\n", bat_d);
  fprintf(stderr, "    events written:  %" PRIu64 "\r\n", num_d);
  fprintf(stderr, "    event size:      %" PRIu64 " bytes\r\n", (c3_d)siz_z);
  fprintf(stderr, "    total data:      %.2f MB\r\n", total_bytes / (1024.0 * 1024.0));
  fprintf(stderr, "    total time:      %.3f seconds\r\n", elapsed_sec);
  fprintf(stderr, "    write speed:     %.0f events/sec\r\n", events_per_sec);
  fprintf(stderr, "    throughput:      %.2f MB/sec\r\n", mb_per_sec);
  fprintf(stderr, "    latency:         %.1f us/event\r\n", us_per_event);
  fprintf(stderr, "\r\n");

cleanup_buffers:
  for ( c3_d i = 0; i < bat_d; i++ ) {
    _free(evt_y[i]);
  }
  _free(evt_y);
  _free(byt_p);
  _free(siz_i);

  u3_book_exit(txt_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  write_speed_batched_benchmark: %s\r\n", ret_i ? "ok" : "FAILED");
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

  //  crash recovery tests
  ret_i &= _test_truncated_file_recovery();

  //  iterator tests
  ret_i &= _test_walk_single_event();
  ret_i &= _test_walk_invalidation();
  ret_i &= _test_walk_range_validation();

  //  header & format tests
  ret_i &= _test_invalid_magic();
  ret_i &= _test_invalid_version();
  ret_i &= _test_undersized_file();

  //  metadata tests
  ret_i &= _test_metadata_roundtrip();
  ret_i &= _test_metadata_invalid_key();
  ret_i &= _test_metadata_size_validation();

  //  benchmarks
  ret_i &= _bench_write_speed(10000, 128);
  ret_i &= _bench_write_speed_batched(1000000, 1280, 1000);

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
