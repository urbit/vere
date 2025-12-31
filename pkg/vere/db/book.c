/// @file

#include "db/book.h"

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <zlib.h>

#include "c3/c3.h"
#include "noun.h"

//  book: append-only event log
//
//    simple file-based persistence layer for urbit's event log.
//    optimized for sequential writes and reads, no random access.
//
//    file format:
//      [64-byte header]
//      [metadata section]
//      [events: len_d | mug_l | jam_data | crc_m | let_d]
//

/* constants
*/
  #define BOOK_MAGIC      0x424f4f4b  //  "BOOK"
  #define BOOK_VERSION    1           //  format version
  #define BOOK_META_SIZE  256         //  reserved metadata area size

/* _book_crc32(): compute CRC32 checksum.
*/
static c3_w
_book_crc32(c3_y* buf_y, c3_w len_w)
{
  return (c3_w)crc32(0L, buf_y, len_w);
}

/* _book_crc32_two(): compute CRC32 over two buffers.
*/
static c3_w
_book_crc32_two(c3_y* one_y, c3_w one_w, c3_y* two_y, c3_w two_w)
{
  c3_w crc_w = (c3_w)crc32(0L, one_y, one_w);
  return (c3_w)crc32(crc_w, two_y, two_w);
}

/* _book_write_header(): write header to file at offset 0.
*/
static c3_o
_book_write_header(u3_book* log_u)
{
  c3_zs ret_zs;

  ret_zs = pwrite(log_u->fid_i, &log_u->hed_u,
                  sizeof(u3_book_head), 0);

  if ( ret_zs != sizeof(u3_book_head) ) {
    fprintf(stderr, "book: failed to write header: %s\r\n",
            strerror(errno));
    return c3n;
  }

  if ( -1 == c3_sync(log_u->fid_i) ) {
    fprintf(stderr, "book: failed to sync header: %s\r\n",
            strerror(errno));
    return c3n;
  }

  log_u->dit_o = c3n;
  return c3y;
}

/* _book_read_header(): read and validate header.
*/
static c3_o
_book_read_header(u3_book* log_u)
{
  c3_zs ret_zs;

  ret_zs = pread(log_u->fid_i, &log_u->hed_u,
                 sizeof(u3_book_head), 0);

  if ( ret_zs != sizeof(u3_book_head) ) {
    fprintf(stderr, "book: failed to read header\r\n");
    return c3n;
  }

  if ( BOOK_MAGIC != log_u->hed_u.mag_w ) {
    fprintf(stderr, "book: invalid magic: 0x%08x\r\n",
            log_u->hed_u.mag_w);
    return c3n;
  }

  if ( BOOK_VERSION != log_u->hed_u.ver_w ) {
    fprintf(stderr, "book: unsupported version: %u\r\n",
            log_u->hed_u.ver_w);
    return c3n;
  }

  return c3y;
}

/* _book_init_header(): initialize header for new file.
*/
static void
_book_init_header(u3_book* log_u)
{
  memset(&log_u->hed_u, 0, sizeof(u3_book_head));
  log_u->hed_u.mag_w = BOOK_MAGIC;
  log_u->hed_u.ver_w = BOOK_VERSION;
  log_u->hed_u.fir_d = 0;
  log_u->hed_u.las_d = 0;
  log_u->hed_u.off_w = 0;
  log_u->hed_u.len_w = 0;
  log_u->dit_o = c3y;
}

/* _book_read_record(): read event record at offset.
**
**   returns:
**     c3y: success, buffers allocated
**     c3n: failure (EOF or corruption)
**
**   on success, caller must free *mug_y and *jam_y
*/
static c3_o
_book_read_record(c3_i   fid_i,
                  c3_w*  off_w,
                  c3_d*  len_d,
                  c3_y** mug_y,
                  c3_y** jam_y,
                  c3_w*  crc_w,
                  c3_d*  let_d)
{
  c3_zs ret_zs;
  c3_w  off_now = *off_w;

  //  read len_d (8 bytes)
  ret_zs = pread(fid_i, len_d, sizeof(c3_d), off_now);
  if ( ret_zs != sizeof(c3_d) ) {
    return c3n;
  }
  off_now += sizeof(c3_d);

  //  validate length
  if ( 0 == *len_d || (1ULL << 32) < *len_d ) {
    fprintf(stderr, "book: invalid length: %llu\r\n", *len_d);
    return c3n;
  }

  //  read mug_l (4 bytes)
  *mug_y = c3_malloc(4);
  ret_zs = pread(fid_i, *mug_y, 4, off_now);
  if ( ret_zs != 4 ) {
    c3_free(*mug_y);
    return c3n;
  }
  off_now += 4;

  //  read jam data (len_d - 4 bytes, since len_d includes mug)
  c3_d jam_len = *len_d - 4;
  *jam_y = c3_malloc(jam_len);
  ret_zs = pread(fid_i, *jam_y, jam_len, off_now);
  if ( ret_zs != (c3_zs)jam_len ) {
    c3_free(*mug_y);
    c3_free(*jam_y);
    return c3n;
  }
  off_now += jam_len;

  //  read crc_m (4 bytes)
  ret_zs = pread(fid_i, crc_w, sizeof(c3_w), off_now);
  if ( ret_zs != sizeof(c3_w) ) {
    c3_free(*mug_y);
    c3_free(*jam_y);
    return c3n;
  }
  off_now += sizeof(c3_w);

  //  read let_d (8 bytes)
  ret_zs = pread(fid_i, let_d, sizeof(c3_d), off_now);
  if ( ret_zs != sizeof(c3_d) ) {
    c3_free(*mug_y);
    c3_free(*jam_y);
    return c3n;
  }
  off_now += sizeof(c3_d);

  //  update offset
  *off_w = off_now;

  return c3y;
}

/* _book_scan_end(): scan to find actual end of valid events.
**
**   validates each record's CRC and len_d == let_d.
**   returns offset to append next event.
**   updates header if corruption detected.
*/
static c3_w
_book_scan_end(u3_book* log_u)
{
  c3_w off_w = sizeof(u3_book_head) + BOOK_META_SIZE;  //  events start here
  c3_d count_d = 0;
  c3_d expected_d;

  if ( 0 == log_u->hed_u.fir_d && 0 == log_u->hed_u.las_d ) {
    //  empty log
    return off_w;
  }

  expected_d = log_u->hed_u.las_d - log_u->hed_u.fir_d + 1;

  while ( 1 ) {
    c3_d  len_d, let_d;
    c3_y* mug_y;
    c3_y* jam_y;
    c3_w  crc_w, calc_crc;
    c3_w  off_start = off_w;

    if ( c3n == _book_read_record(log_u->fid_i, &off_w,
                                   &len_d, &mug_y, &jam_y,
                                   &crc_w, &let_d) )
    {
      //  EOF or read error
      break;
    }

    //  validate len_d == let_d
    if ( len_d != let_d ) {
      fprintf(stderr, "book: length mismatch at offset %u\r\n", off_start);
      c3_free(mug_y);
      c3_free(jam_y);
      break;
    }

    //  validate CRC: CRC32(len_d || mug || jam)
    {
      c3_y buf_y[12];  //  8 bytes len_d + 4 bytes mug
      memcpy(buf_y, &len_d, 8);
      memcpy(buf_y + 8, mug_y, 4);

      calc_crc = _book_crc32(buf_y, 12);
      calc_crc = (c3_w)crc32(calc_crc, jam_y, len_d - 4);
    }

    c3_free(mug_y);
    c3_free(jam_y);

    if ( crc_w != calc_crc ) {
      fprintf(stderr, "book: CRC mismatch at offset %u\r\n", off_start);
      break;
    }

    count_d++;
  }

  //  check if we found fewer events than expected
  if ( count_d != expected_d ) {
    fprintf(stderr, "book: recovery: found %llu events, expected %llu\r\n",
            count_d, expected_d);

    //  update header
    if ( count_d == 0 ) {
      log_u->hed_u.fir_d = 0;
      log_u->hed_u.las_d = 0;
      off_w = sizeof(u3_book_head);
    } else {
      log_u->hed_u.las_d = log_u->hed_u.fir_d + count_d - 1;
    }

    log_u->dit_o = c3y;
    _book_write_header(log_u);

    //  truncate file
    if ( -1 == ftruncate(log_u->fid_i, off_w) ) {
      fprintf(stderr, "book: failed to truncate: %s\r\n",
              strerror(errno));
    } else {
      c3_sync(log_u->fid_i);
    }
  }

  return off_w;
}

/* u3_book_init(): open/create event log.
*/
u3_book*
u3_book_init(const c3_c* pax_c)
{
  c3_c path_c[8193];
  c3_i fid_i;
  struct stat buf_u;
  u3_book* log_u;

  //  construct path to book.log
  snprintf(path_c, sizeof(path_c), "%s/book.log", pax_c);

  //  open or create file
  fid_i = c3_open(path_c, O_RDWR | O_CREAT, 0644);
  if ( 0 > fid_i ) {
    fprintf(stderr, "book: failed to open %s: %s\r\n",
            path_c, strerror(errno));
    return 0;
  }

  //  get file size
  if ( 0 > fstat(fid_i, &buf_u) ) {
    fprintf(stderr, "book: fstat failed: %s\r\n", strerror(errno));
    close(fid_i);
    return 0;
  }

  //  allocate log structure
  log_u = c3_calloc(sizeof(u3_book));
  log_u->fid_i = fid_i;
  log_u->pax_c = c3_malloc(strlen(path_c) + 1);
  strcpy(log_u->pax_c, path_c);

  if ( buf_u.st_size == 0 ) {
    //  new file: initialize header
    _book_init_header(log_u);
    _book_write_header(log_u);
    //  events start after header + reserved metadata area
    log_u->off_w = sizeof(u3_book_head) + BOOK_META_SIZE;
  }
  else if ( buf_u.st_size < (off_t)sizeof(u3_book_head) ) {
    //  corrupt file: too small
    fprintf(stderr, "book: file too small: %lld bytes\r\n",
            (long long)buf_u.st_size);
    close(fid_i);
    c3_free(log_u->pax_c);
    c3_free(log_u);
    return 0;
  }
  else {
    //  existing file: read and validate header
    if ( c3n == _book_read_header(log_u) ) {
      close(fid_i);
      c3_free(log_u->pax_c);
      c3_free(log_u);
      return 0;
    }

    //  scan to find actual end, recover from corruption
    log_u->off_w = _book_scan_end(log_u);
  }

  return log_u;
}

/* u3_book_exit(): close event log.
*/
void
u3_book_exit(u3_book* log_u)
{
  if ( !log_u ) {
    return;
  }

  //  sync header if dirty
  if ( c3y == log_u->dit_o ) {
    _book_write_header(log_u);
  }

  //  close file
  close(log_u->fid_i);

  //  free resources
  c3_free(log_u->pax_c);
  c3_free(log_u);
}

/* u3_book_gulf(): read first and last event numbers.
*/
c3_o
u3_book_gulf(u3_book* log_u, c3_d* low_d, c3_d* hig_d)
{
  if ( !log_u ) {
    return c3n;
  }

  *low_d = log_u->hed_u.fir_d;
  *hig_d = log_u->hed_u.las_d;

  return c3y;
}

/* u3_book_stat(): print book statistics.
*/
void
u3_book_stat(const c3_c* pax_c)
{
  c3_i fid_i;
  u3_book_head hed_u;
  struct stat buf_u;

  //  open the file directly
  fid_i = c3_open(pax_c, O_RDONLY, 0);
  if ( fid_i < 0 ) {
    fprintf(stderr, "book: failed to open %s: %s\r\n", pax_c, strerror(errno));
    return;
  }

  //  read and validate header
  if ( sizeof(u3_book_head) != read(fid_i, &hed_u, sizeof(u3_book_head)) ) {
    fprintf(stderr, "book: failed to read header\r\n");
    close(fid_i);
    return;
  }

  if ( BOOK_MAGIC != hed_u.mag_w ) {
    fprintf(stderr, "book: invalid magic number: 0x%x\r\n", hed_u.mag_w);
    close(fid_i);
    return;
  }

  if ( BOOK_VERSION != hed_u.ver_w ) {
    fprintf(stderr, "book: unsupported version: %u\r\n", hed_u.ver_w);
    close(fid_i);
    return;
  }

  if ( fstat(fid_i, &buf_u) < 0 ) {
    fprintf(stderr, "book: fstat failed\r\n");
    close(fid_i);
    return;
  }

  fprintf(stderr, "book info:\r\n");
  fprintf(stderr, "  file: %s\r\n", pax_c);
  fprintf(stderr, "  version: %u\r\n", hed_u.ver_w);
  fprintf(stderr, "  first event: %llu\r\n", hed_u.fir_d);
  fprintf(stderr, "  last event: %llu\r\n", hed_u.las_d);
  fprintf(stderr, "  event count: %llu\r\n",
          (0 == hed_u.las_d ) ? 0 :
          (hed_u.las_d - hed_u.fir_d + 1));
  fprintf(stderr, "  file size: %lld bytes\r\n", (long long)buf_u.st_size);
  fprintf(stderr, "  metadata offset: %u\r\n", hed_u.off_w);
  fprintf(stderr, "  metadata length: %u\r\n", hed_u.len_w);

  close(fid_i);
}

/* u3_book_save(): save [len_d] events starting at [eve_d].
**
**   byt_p: array of buffers (mug + jam format)
**   siz_i: array of buffer sizes
*/
c3_o
u3_book_save(u3_book* log_u,
             c3_d     eve_d,
             c3_d     len_d,
             void**   byt_p,
             c3_z*    siz_i,
             c3_d     epo_d)
{
  c3_w i;
  c3_w off_now;

  if ( !log_u ) {
    return c3n;
  }

  //  validate contiguity
  if ( 0 == log_u->hed_u.las_d ) {
    //  empty log: first event must be the first event in the epoch
    if ( epo_d + 1 != eve_d ) {
      fprintf(stderr, "book: first event must be 1, got %llu\r\n", eve_d);
      return c3n;
    }
    log_u->hed_u.fir_d = eve_d;
  }
  else {
    //  non-empty: must be contiguous
    if ( eve_d != log_u->hed_u.las_d + 1 ) {
      fprintf(stderr, "book: event gap: expected %llu, got %llu\r\n",
              log_u->hed_u.las_d + 1, eve_d);
      return c3n;
    }
  }

  //  write each event record
  off_now = log_u->off_w;

  for ( i = 0; i < len_d; i++ ) {
    c3_y* buf_y = (c3_y*)byt_p[i];
    c3_d  siz_d = (c3_d)siz_i[i];
    c3_d  len_write;
    c3_l  mug_l;
    c3_y* jam_y;
    c3_d  jam_len;
    c3_w  crc_w;
    c3_zs ret_zs;
    c3_y  len_buf[8];
    c3_y  crc_buf[4];
    c3_y  let_buf[8];

    //  extract mug from buffer (first 4 bytes)
    if ( siz_d < 4 ) {
      fprintf(stderr, "book: event %llu buffer too small: %llu\r\n",
              eve_d + i, siz_d);
      return c3n;
    }

    memcpy(&mug_l, buf_y, 4);
    jam_y = buf_y + 4;
    jam_len = siz_d - 4;

    //  len_d is total payload: 4 bytes mug + jam data
    len_write = siz_d;

    //  compute CRC32 over: len_d (8 bytes) + mug_l (4 bytes) + jam data
    {
      c3_y tmp_buf[12];
      memcpy(tmp_buf, &len_write, 8);
      memcpy(tmp_buf + 8, &mug_l, 4);
      crc_w = _book_crc32_two(tmp_buf, 12, jam_y, jam_len);
    }

    //  prepare buffers for writing
    memcpy(len_buf, &len_write, 8);
    memcpy(crc_buf, &crc_w, 4);
    memcpy(let_buf, &len_write, 8);

    //  write record: len_d | mug_l | jam | crc_m | let_d
    ret_zs = pwrite(log_u->fid_i, len_buf, 8, off_now);
    if ( ret_zs != 8 ) {
      fprintf(stderr, "book: failed to write len_d for event %llu: %s\r\n",
              eve_d + i, strerror(errno));
      return c3n;
    }
    off_now += 8;

    ret_zs = pwrite(log_u->fid_i, &mug_l, 4, off_now);
    if ( ret_zs != 4 ) {
      fprintf(stderr, "book: failed to write mug for event %llu: %s\r\n",
              eve_d + i, strerror(errno));
      return c3n;
    }
    off_now += 4;

    ret_zs = pwrite(log_u->fid_i, jam_y, jam_len, off_now);
    if ( ret_zs != (c3_zs)jam_len ) {
      fprintf(stderr, "book: failed to write jam for event %llu: %s\r\n",
              eve_d + i, strerror(errno));
      return c3n;
    }
    off_now += jam_len;

    ret_zs = pwrite(log_u->fid_i, crc_buf, 4, off_now);
    if ( ret_zs != 4 ) {
      fprintf(stderr, "book: failed to write crc for event %llu: %s\r\n",
              eve_d + i, strerror(errno));
      return c3n;
    }
    off_now += 4;

    ret_zs = pwrite(log_u->fid_i, let_buf, 8, off_now);
    if ( ret_zs != 8 ) {
      fprintf(stderr, "book: failed to write let_d for event %llu: %s\r\n",
              eve_d + i, strerror(errno));
      return c3n;
    }
    off_now += 8;
  }

  //  sync data to disk
  if ( -1 == c3_sync(log_u->fid_i) ) {
    fprintf(stderr, "book: failed to sync events: %s\r\n",
            strerror(errno));
    return c3n;
  }

  //  update header
  log_u->hed_u.las_d = eve_d + len_d - 1;
  log_u->off_w = off_now;
  log_u->dit_o = c3y;

  //  write and sync header
  if ( c3n == _book_write_header(log_u) ) {
    return c3n;
  }

  return c3y;
}

/* u3_book_read(): read [len_d] events starting at [eve_d].
**
**   invokes callback for each event with:
**     ptr_v: context pointer
**     eve_d: event number
**     len_i: buffer size (mug + jam)
**     buf_v: buffer pointer (mug + jam format)
*/
c3_o
u3_book_read(u3_book* log_u,
             void*    ptr_v,
             c3_d     eve_d,
             c3_d     len_d,
             c3_o  (*read_f)(void*, c3_d, c3_z, void*))
{
  c3_w  off_w;
  c3_d  cur_d;
  c3_d  i;

  if ( !log_u ) {
    return c3n;
  }

  //  validate range
  if ( 0 == log_u->hed_u.las_d ) {
    //  empty log
    fprintf(stderr, "book: read from empty log\r\n");
    return c3n;
  }

  if ( eve_d < log_u->hed_u.fir_d || eve_d > log_u->hed_u.las_d ) {
    fprintf(stderr, "book: event %llu out of range [%llu, %llu]\r\n",
            eve_d, log_u->hed_u.fir_d, log_u->hed_u.las_d);
    return c3n;
  }

  if ( eve_d + len_d - 1 > log_u->hed_u.las_d ) {
    fprintf(stderr, "book: read range exceeds last event\r\n");
    return c3n;
  }

  //  scan to starting event (events start after header + metadata area)
  off_w = sizeof(u3_book_head) + BOOK_META_SIZE;
  cur_d = log_u->hed_u.fir_d;

  while ( cur_d < eve_d ) {
    c3_d  skip_len;
    c3_zs ret_zs;

    ret_zs = pread(log_u->fid_i, &skip_len, sizeof(c3_d), off_w);
    if ( ret_zs != sizeof(c3_d) ) {
      fprintf(stderr, "book: failed to scan to event %llu\r\n", eve_d);
      return c3n;
    }

    //  skip entire record: len_d(8) + mug(4) + jam(len-4) + crc(4) + let_d(8)
    off_w += 8 + 4 + (skip_len - 4) + 4 + 8;
    cur_d++;
  }

  //  read requested events
  for ( i = 0; i < len_d; i++, cur_d++ ) {
    c3_d  len_rec;
    c3_y* mug_y;
    c3_y* jam_y;
    c3_w  crc_w, calc_crc;
    c3_d  let_d;
    c3_y* buf_y;
    c3_z  len_z;

    //  read record
    if ( c3n == _book_read_record(log_u->fid_i, &off_w,
                                   &len_rec, &mug_y, &jam_y,
                                   &crc_w, &let_d) )
    {
      fprintf(stderr, "book: failed to read event %llu\r\n", cur_d);
      return c3n;
    }

    //  validate len_d == let_d
    if ( len_rec != let_d ) {
      fprintf(stderr, "book: length mismatch at event %llu\r\n", cur_d);
      c3_free(mug_y);
      c3_free(jam_y);
      return c3n;
    }

    //  validate CRC
    {
      c3_y tmp_buf[12];
      memcpy(tmp_buf, &len_rec, 8);
      memcpy(tmp_buf + 8, mug_y, 4);
      calc_crc = _book_crc32(tmp_buf, 12);
      calc_crc = (c3_w)crc32(calc_crc, jam_y, len_rec - 4);
    }

    if ( crc_w != calc_crc ) {
      fprintf(stderr, "book: CRC mismatch at event %llu\r\n", cur_d);
      c3_free(mug_y);
      c3_free(jam_y);
      return c3n;
    }

    //  reconstruct buffer in mug + jam format for callback
    len_z = len_rec;
    buf_y = c3_malloc(len_z);
    memcpy(buf_y, mug_y, 4);
    memcpy(buf_y + 4, jam_y, len_rec - 4);

    c3_free(mug_y);
    c3_free(jam_y);

    //  invoke callback
    if ( c3n == read_f(ptr_v, cur_d, len_z, buf_y) ) {
      c3_free(buf_y);
      return c3n;
    }

    c3_free(buf_y);
  }

  return c3y;
}

/* u3_book_walk_init(): initialize event iterator.
**
**   sets up iterator to read events from [nex_d] to [las_d] inclusive.
*/
c3_o
u3_book_walk_init(u3_book*      log_u,
                  u3_book_walk* itr_u,
                  c3_d          nex_d,
                  c3_d          las_d)
{
  c3_w off_w;
  c3_d cur_d;

  if ( !log_u || !itr_u ) {
    return c3n;
  }

  //  validate range
  if ( 0 == log_u->hed_u.las_d ) {
    fprintf(stderr, "book: walk_init on empty log\r\n");
    return c3n;
  }

  if ( nex_d < log_u->hed_u.fir_d || nex_d > log_u->hed_u.las_d ) {
    fprintf(stderr, "book: walk_init start %llu out of range [%llu, %llu]\r\n",
            nex_d, log_u->hed_u.fir_d, log_u->hed_u.las_d);
    return c3n;
  }

  if ( las_d < nex_d || las_d > log_u->hed_u.las_d ) {
    fprintf(stderr, "book: walk_init end %llu out of range [%llu, %llu]\r\n",
            las_d, nex_d, log_u->hed_u.las_d);
    return c3n;
  }

  //  scan to starting event (events start after header + metadata area)
  off_w = sizeof(u3_book_head) + BOOK_META_SIZE;
  cur_d = log_u->hed_u.fir_d;

  while ( cur_d < nex_d ) {
    c3_d  skip_len;
    c3_zs ret_zs;

    ret_zs = pread(log_u->fid_i, &skip_len, sizeof(c3_d), off_w);
    if ( ret_zs != sizeof(c3_d) ) {
      fprintf(stderr, "book: walk_init failed to scan to event %llu\r\n", nex_d);
      return c3n;
    }

    //  skip entire record
    off_w += 8 + 4 + (skip_len - 4) + 4 + 8;
    cur_d++;
  }

  //  initialize iterator
  itr_u->fid_i = log_u->fid_i;
  itr_u->nex_d = nex_d;
  itr_u->las_d = las_d;
  itr_u->off_w = off_w;
  itr_u->liv_o = c3y;

  return c3y;
}

/* u3_book_walk_next(): read next event from iterator.
**
**   allocates buffer for event (caller must free).
**   returns c3n when no more events or error.
*/
c3_o
u3_book_walk_next(u3_book_walk* itr_u, c3_z* len_z, void** buf_v)
{
  c3_d  len_rec;
  c3_y* mug_y;
  c3_y* jam_y;
  c3_w  crc_w, calc_crc;
  c3_d  let_d;
  c3_y* buf_y;

  if ( !itr_u || c3n == itr_u->liv_o ) {
    return c3n;
  }

  //  check if we've reached the end
  if ( itr_u->nex_d > itr_u->las_d ) {
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  read record
  if ( c3n == _book_read_record(itr_u->fid_i, &itr_u->off_w,
                                 &len_rec, &mug_y, &jam_y,
                                 &crc_w, &let_d) )
  {
    fprintf(stderr, "book: walk_next failed to read event %llu\r\n",
            itr_u->nex_d);
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  validate len_d == let_d
  if ( len_rec != let_d ) {
    fprintf(stderr, "book: walk_next length mismatch at event %llu\r\n",
            itr_u->nex_d);
    c3_free(mug_y);
    c3_free(jam_y);
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  validate CRC
  {
    c3_y tmp_buf[12];
    memcpy(tmp_buf, &len_rec, 8);
    memcpy(tmp_buf + 8, mug_y, 4);
    calc_crc = _book_crc32(tmp_buf, 12);
    calc_crc = (c3_w)crc32(calc_crc, jam_y, len_rec - 4);
  }

  if ( crc_w != calc_crc ) {
    fprintf(stderr, "book: walk_next CRC mismatch at event %llu\r\n",
            itr_u->nex_d);
    c3_free(mug_y);
    c3_free(jam_y);
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  reconstruct buffer in mug + jam format
  *len_z = len_rec;
  buf_y = c3_malloc(*len_z);
  memcpy(buf_y, mug_y, 4);
  memcpy(buf_y + 4, jam_y, len_rec - 4);

  c3_free(mug_y);
  c3_free(jam_y);

  *buf_v = buf_y;

  //  advance to next event
  itr_u->nex_d++;

  return c3y;
}

/* u3_book_walk_done(): close iterator.
*/
void
u3_book_walk_done(u3_book_walk* itr_u)
{
  if ( !itr_u ) {
    return;
  }

  //  mark iterator as invalid
  itr_u->liv_o = c3n;
  itr_u->fid_i = -1;
}

/* u3_book_read_meta(): read metadata by string key from log.
**
**   invokes callback with (ptr_v, len, data) or (ptr_v, -1, 0) if not found.
*/
void
u3_book_read_meta(u3_book*    log_u,
                  void*       ptr_v,
                  const c3_c* key_c,
                  void     (*read_f)(void*, c3_zs, void*))
{
  c3_w   key_len;
  c3_y*  meta_buf;
  c3_w   meta_len;
  c3_zs  ret_zs;
  c3_w   offset;
  c3_w   count;
  c3_w   i;

  if ( !log_u ) {
    read_f(ptr_v, -1, 0);
    return;
  }

  //  check if metadata section exists
  if ( 0 == log_u->hed_u.len_w ) {
    read_f(ptr_v, -1, 0);
    return;
  }

  //  read entire metadata section
  meta_len = log_u->hed_u.len_w;
  meta_buf = c3_malloc(meta_len);

  ret_zs = pread(log_u->fid_i, meta_buf, meta_len, log_u->hed_u.off_w);
  if ( ret_zs != (c3_zs)meta_len ) {
    fprintf(stderr, "book: read_meta: failed to read metadata section\r\n");
    c3_free(meta_buf);
    read_f(ptr_v, -1, 0);
    return;
  }

  //  parse metadata section
  //  format: [4 bytes: count] + entries
  //  entry:  [4 bytes: key_len][key][4 bytes: val_len][val]

  if ( meta_len < 4 ) {
    fprintf(stderr, "book: read_meta: metadata section too small\r\n");
    c3_free(meta_buf);
    read_f(ptr_v, -1, 0);
    return;
  }

  memcpy(&count, meta_buf, 4);
  offset = 4;

  key_len = strlen(key_c);

  //  linear search for key
  for ( i = 0; i < count; i++ ) {
    c3_w   entry_key_len;
    c3_y*  entry_key;
    c3_w   entry_val_len;
    c3_y*  entry_val;

    //  read key length
    if ( offset + 4 > meta_len ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (key len)\r\n");
      c3_free(meta_buf);
      read_f(ptr_v, -1, 0);
      return;
    }
    memcpy(&entry_key_len, meta_buf + offset, 4);
    offset += 4;

    //  read key
    if ( offset + entry_key_len > meta_len ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (key)\r\n");
      c3_free(meta_buf);
      read_f(ptr_v, -1, 0);
      return;
    }
    entry_key = meta_buf + offset;
    offset += entry_key_len;

    //  read value length
    if ( offset + 4 > meta_len ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (val len)\r\n");
      c3_free(meta_buf);
      read_f(ptr_v, -1, 0);
      return;
    }
    memcpy(&entry_val_len, meta_buf + offset, 4);
    offset += 4;

    //  read value
    if ( offset + entry_val_len > meta_len ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (val)\r\n");
      c3_free(meta_buf);
      read_f(ptr_v, -1, 0);
      return;
    }
    entry_val = meta_buf + offset;
    offset += entry_val_len;

    //  check if this is the key we're looking for
    if ( entry_key_len == key_len &&
         0 == memcmp(entry_key, key_c, key_len) )
    {
      //  found it - invoke callback
      read_f(ptr_v, entry_val_len, entry_val);
      c3_free(meta_buf);
      return;
    }
  }

  //  not found
  c3_free(meta_buf);
  read_f(ptr_v, -1, 0);
}

/* u3_book_save_meta(): save metadata by string key into log.
**
**   updates or inserts key-value pair in metadata section.
*/
c3_o
u3_book_save_meta(u3_book*    log_u,
                  const c3_c* key_c,
                  c3_z        val_z,
                  void*       val_p)
{
  c3_w   key_len;
  c3_y*  old_meta = 0;
  c3_w   old_len = 0;
  c3_w   old_count = 0;
  c3_y*  new_meta;
  c3_w   new_len;
  c3_w   new_count;
  c3_w   offset;
  c3_w   i;
  c3_o   found = c3n;
  c3_zs  ret_zs;

  if ( !log_u ) {
    return c3n;
  }

  key_len = strlen(key_c);

  //  read existing metadata if present
  if ( 0 != log_u->hed_u.len_w ) {
    old_len = log_u->hed_u.len_w;
    old_meta = c3_malloc(old_len);

    ret_zs = pread(log_u->fid_i, old_meta, old_len, log_u->hed_u.off_w);
    if ( ret_zs != (c3_zs)old_len ) {
      fprintf(stderr, "book: save_meta: failed to read old metadata\r\n");
      c3_free(old_meta);
      return c3n;
    }

    if ( old_len < 4 ) {
      fprintf(stderr, "book: save_meta: corrupt old metadata\r\n");
      c3_free(old_meta);
      return c3n;
    }

    memcpy(&old_count, old_meta, 4);
  }

  //  calculate new metadata size
  //  worst case: all old entries + new entry
  new_len = 4;  //  count field

  //  add existing entries (except if we're updating)
  if ( old_meta ) {
    offset = 4;
    for ( i = 0; i < old_count; i++ ) {
      c3_w entry_key_len, entry_val_len;

      if ( offset + 4 > old_len ) break;
      memcpy(&entry_key_len, old_meta + offset, 4);
      offset += 4;

      if ( offset + entry_key_len > old_len ) break;

      //  check if this is the key we're updating
      if ( entry_key_len == key_len &&
           0 == memcmp(old_meta + offset, key_c, key_len) )
      {
        found = c3y;
        //  skip old value, we'll add new one
        offset += entry_key_len;
        if ( offset + 4 > old_len ) break;
        memcpy(&entry_val_len, old_meta + offset, 4);
        offset += 4 + entry_val_len;
        continue;
      }

      //  add this entry to new size
      offset += entry_key_len;
      if ( offset + 4 > old_len ) break;
      memcpy(&entry_val_len, old_meta + offset, 4);
      offset += 4;

      new_len += 4 + entry_key_len + 4 + entry_val_len;
      offset += entry_val_len;
    }
  }

  //  add new/updated entry
  new_len += 4 + key_len + 4 + val_z;

  //  allocate new metadata buffer
  new_meta = c3_malloc(new_len);

  //  write count
  new_count = (c3y == found) ? old_count : old_count + 1;
  memcpy(new_meta, &new_count, 4);
  offset = 4;

  //  copy existing entries (except updated one)
  if ( old_meta ) {
    c3_w old_offset = 4;
    for ( i = 0; i < old_count; i++ ) {
      c3_w entry_key_len, entry_val_len;

      if ( old_offset + 4 > old_len ) break;
      memcpy(&entry_key_len, old_meta + old_offset, 4);

      if ( old_offset + 4 + entry_key_len > old_len ) break;

      //  skip if this is the key we're updating
      if ( entry_key_len == key_len &&
           0 == memcmp(old_meta + old_offset + 4, key_c, key_len) )
      {
        old_offset += 4 + entry_key_len;
        if ( old_offset + 4 > old_len ) break;
        memcpy(&entry_val_len, old_meta + old_offset, 4);
        old_offset += 4 + entry_val_len;
        continue;
      }

      //  copy this entry
      memcpy(new_meta + offset, old_meta + old_offset, 4);
      offset += 4;
      old_offset += 4;

      memcpy(new_meta + offset, old_meta + old_offset, entry_key_len);
      offset += entry_key_len;
      old_offset += entry_key_len;

      if ( old_offset + 4 > old_len ) break;
      memcpy(&entry_val_len, old_meta + old_offset, 4);
      memcpy(new_meta + offset, &entry_val_len, 4);
      offset += 4;
      old_offset += 4;

      memcpy(new_meta + offset, old_meta + old_offset, entry_val_len);
      offset += entry_val_len;
      old_offset += entry_val_len;
    }
  }

  //  add new/updated entry
  memcpy(new_meta + offset, &key_len, 4);
  offset += 4;
  memcpy(new_meta + offset, key_c, key_len);
  offset += key_len;
  {
    c3_w val_len_w = (c3_w)val_z;  //  convert c3_z to c3_w for 4-byte field
    memcpy(new_meta + offset, &val_len_w, 4);
  }
  offset += 4;
  memcpy(new_meta + offset, val_p, val_z);
  offset += val_z;

  //  write new metadata section in reserved area after header
  c3_w new_off = sizeof(u3_book_head);

  //  ensure metadata fits in reserved space
  if ( new_len > BOOK_META_SIZE ) {
    fprintf(stderr, "book: save_meta: metadata too large (%u > %u)\r\n",
            new_len, BOOK_META_SIZE);
    c3_free(new_meta);
    if ( old_meta ) c3_free(old_meta);
    return c3n;
  }

  ret_zs = pwrite(log_u->fid_i, new_meta, new_len, new_off);
  if ( ret_zs != (c3_zs)new_len ) {
    fprintf(stderr, "book: save_meta: failed to write metadata: %s\r\n",
            strerror(errno));
    c3_free(new_meta);
    if ( old_meta ) c3_free(old_meta);
    return c3n;
  }

  c3_free(new_meta);
  if ( old_meta ) c3_free(old_meta);

  //  sync metadata
  if ( -1 == c3_sync(log_u->fid_i) ) {
    fprintf(stderr, "book: save_meta: failed to sync metadata: %s\r\n",
            strerror(errno));
    return c3n;
  }

  //  update header
  log_u->hed_u.off_w = new_off;
  log_u->hed_u.len_w = new_len;
  log_u->dit_o = c3y;

  //  write and sync header
  if ( c3n == _book_write_header(log_u) ) {
    return c3n;
  }

  //  off_w is not affected by metadata writes - events append at off_w

  return c3y;
}
