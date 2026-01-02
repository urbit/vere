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

/* _book_save_head(): write header to file at offset 0.
*/
static c3_o
_book_save_head(u3_book* txt_u)
{
  c3_zs ret_zs;

  ret_zs = pwrite(txt_u->fid_i, &txt_u->hed_u,
                  sizeof(u3_book_head), 0);

  if ( ret_zs != sizeof(u3_book_head) ) {
    fprintf(stderr, "book: failed to write header: %s\r\n",
            strerror(errno));
    return c3n;
  }

  if ( -1 == c3_sync(txt_u->fid_i) ) {
    fprintf(stderr, "book: failed to sync header: %s\r\n",
            strerror(errno));
    return c3n;
  }

  txt_u->dit_o = c3n;
  return c3y;
}

/* _book_read_head(): read and validate header.
*/
static c3_o
_book_read_head(u3_book* txt_u)
{
  c3_zs ret_zs;

  ret_zs = pread(txt_u->fid_i, &txt_u->hed_u,
                 sizeof(u3_book_head), 0);

  if ( ret_zs != sizeof(u3_book_head) ) {
    fprintf(stderr, "book: failed to read header\r\n");
    return c3n;
  }

  if ( BOOK_MAGIC != txt_u->hed_u.mag_w ) {
    fprintf(stderr, "book: invalid magic: 0x%08x\r\n",
            txt_u->hed_u.mag_w);
    return c3n;
  }

  if ( BOOK_VERSION != txt_u->hed_u.ver_w ) {
    fprintf(stderr, "book: unsupported version: %u\r\n",
            txt_u->hed_u.ver_w);
    return c3n;
  }

  return c3y;
}

/* _book_init_head(): initialize header for new file.
*/
static void
_book_init_head(u3_book* txt_u)
{
  memset(&txt_u->hed_u, 0, sizeof(u3_book_head));
  txt_u->hed_u.mag_w = BOOK_MAGIC;
  txt_u->hed_u.ver_w = BOOK_VERSION;
  txt_u->hed_u.fir_d = 0;
  txt_u->hed_u.las_d = 0;
  txt_u->hed_u.off_w = 0;
  txt_u->hed_u.len_w = 0;
  txt_u->dit_o = c3y;
}

/* _book_deed_size(): calculate total on-disk size of deed.
*/
static inline c3_w
_book_deed_size(c3_d len_d)
{
  return sizeof(u3_book_deed_head) + (len_d - 4) + sizeof(u3_book_deed_tail);
  // = 12 + (len_d - 4) + 12 = len_d + 20
}

/* _book_calc_crc(): compute CRC32 for reed.
*/
static c3_w
_book_calc_crc(const u3_book_reed* red_u)
{
  c3_y buf_y[12];  //  8 bytes len_d + 4 bytes mug
  memcpy(buf_y, &red_u->len_d, 8);
  memcpy(buf_y + 8, &red_u->mug_l, 4);

  return _book_crc32_two(buf_y, 12, red_u->jam_y, red_u->len_d - 4);
}

/* _book_okay_reed(): validate reed integrity.
*/
static c3_o
_book_okay_reed(const u3_book_reed* red_u)
{
  //  validate length
  if ( 0 == red_u->len_d || (1ULL << 32) < red_u->len_d ) {
    return c3n;
  }

  //  validate CRC
  c3_w crc_w = _book_calc_crc(red_u);
  if ( crc_w != red_u->crc_w ) {
    return c3n;
  }

  return c3y;
}

/* _book_read_deed(): read deed from file into [red_u].
**
**   returns:
**     c3y: success, jam_y allocated
**     c3n: failure (EOF or corruption)
**
**   on success, caller must free red_u->jam_y
*/
static c3_o
_book_read_deed(c3_i fid_i, c3_w* off_w, u3_book_reed* red_u)
{
  c3_zs ret_zs;
  c3_w  now_w = *off_w;
  c3_d  let_d;

  //  read deed_head
  u3_book_deed_head hed_u;
  ret_zs = pread(fid_i, &hed_u, sizeof(u3_book_deed_head), now_w);
  if ( ret_zs != sizeof(u3_book_deed_head) ) {
    return c3n;
  }
  now_w += sizeof(u3_book_deed_head);

  //  validate length
  if ( 0 == hed_u.len_d || (1ULL << 32) < hed_u.len_d ) {
    fprintf(stderr, "book: invalid length: %" PRIu64 "\r\n", hed_u.len_d);
    return c3n;
  }

  //  populate reed from head
  red_u->len_d = hed_u.len_d;
  red_u->mug_l = hed_u.mug_l;

  //  read jam data (len_d - mug bytes)
  c3_d jaz_d = red_u->len_d - 4;
  red_u->jam_y = c3_malloc(jaz_d);
  ret_zs = pread(fid_i, red_u->jam_y, jaz_d, now_w);
  if ( ret_zs != (c3_zs)jaz_d ) {
    c3_free(red_u->jam_y);
    return c3n;
  }
  now_w += jaz_d;

  //  read deed_tail
  u3_book_deed_tail tal_u;
  ret_zs = pread(fid_i, &tal_u, sizeof(u3_book_deed_tail), now_w);
  if ( ret_zs != sizeof(u3_book_deed_tail) ) {
    c3_free(red_u->jam_y);
    return c3n;
  }
  now_w += sizeof(u3_book_deed_tail);

  //  populate reed from tail
  red_u->crc_w = tal_u.crc_w;
  let_d = tal_u.let_d;

  //  validate len_d == let_d
  if ( red_u->len_d != let_d ) {
    c3_free(red_u->jam_y);
    return c3n;
  }

  //  update offset
  *off_w = now_w;

  return c3y;
}

/* _book_save_deed(): save complete deed to file.
**
**   returns:
**     c3y: success
**     c3n: failure
*/
static c3_o
_book_save_deed(c3_i fid_i, c3_w* off_w, const u3_book_reed* red_u)
{
  c3_zs ret_zs;
  c3_w  now_w = *off_w;
  c3_d  jaz_d = red_u->len_d - 4;  //  len_d - mug bytes

  //  write deed_head
  u3_book_deed_head hed_u;
  hed_u.len_d = red_u->len_d;
  hed_u.mug_l = red_u->mug_l;

  ret_zs = pwrite(fid_i, &hed_u, sizeof(u3_book_deed_head), now_w);
  if ( ret_zs != sizeof(u3_book_deed_head) ) {
    return c3n;
  }
  now_w += sizeof(u3_book_deed_head);

  //  write jam data
  ret_zs = pwrite(fid_i, red_u->jam_y, jaz_d, now_w);
  if ( ret_zs != (c3_zs)jaz_d ) {
    return c3n;
  }
  now_w += jaz_d;

  //  write deed_tail
  u3_book_deed_tail tal_u;
  tal_u.crc_w = red_u->crc_w;
  tal_u.let_d = red_u->len_d;  //  length trailer (same as len_d)

  ret_zs = pwrite(fid_i, &tal_u, sizeof(u3_book_deed_tail), now_w);
  if ( ret_zs != sizeof(u3_book_deed_tail) ) {
    return c3n;
  }
  now_w += sizeof(u3_book_deed_tail);

  //  update offset
  *off_w = now_w;

  return c3y;
}

/* _book_skip_deed(): skip over deed without reading jam data.
**
**   returns:
**     c3y: success
**     c3n: failure (EOF)
*/
static c3_o
_book_skip_deed(c3_i fid_i, c3_w* off_w)
{
  c3_zs ret_zs;
  c3_d  len_d;

  //  read only the len_d field
  ret_zs = pread(fid_i, &len_d, sizeof(c3_d), *off_w);
  if ( ret_zs != sizeof(c3_d) ) {
    return c3n;
  }

  //  skip entire deed: deed_head + jam + deed_tail
  *off_w += _book_deed_size(len_d);

  return c3y;
}

/* _book_scan_end(): scan to find actual end of valid events.
**
**   validates each record's CRC and len_d == let_d.
**   returns offset to append next event.
**   updates header if corruption detected.
*/
static c3_w
_book_scan_end(u3_book* txt_u)
{
  c3_w off_w = sizeof(u3_book_head) + BOOK_META_SIZE;  //  start
  c3_d cot_d = 0;  //  count
  c3_d exp_d;      //  expected event number

  if ( 0 == txt_u->hed_u.fir_d && 0 == txt_u->hed_u.las_d ) {
    //  empty log
    return off_w;
  }

  exp_d = txt_u->hed_u.las_d - txt_u->hed_u.fir_d + 1;

  while ( 1 ) {
    u3_book_reed red_u;
    c3_w  off_start = off_w;

    //  read deed into reed
    if ( c3n == _book_read_deed(txt_u->fid_i, &off_w, &red_u) ) {
      //  EOF or read error
      break;
    }

    //  validate reed (CRC and length checks)
    if ( c3n == _book_okay_reed(&red_u) ) {
      fprintf(stderr, "book: validation failed at offset %u\r\n", off_start);
      c3_free(red_u.jam_y);
      break;
    }

    c3_free(red_u.jam_y);
    cot_d++;
  }

  //  check if we found fewer events than expected
  if ( cot_d != exp_d ) {
    fprintf(stderr, "book: recovery: found %" PRIu64 " events, expected %" PRIu64 "\r\n",
            cot_d, exp_d);

    //  update header
    if ( cot_d == 0 ) {
      txt_u->hed_u.fir_d = 0;
      txt_u->hed_u.las_d = 0;
      off_w = sizeof(u3_book_head);
    } else {
      txt_u->hed_u.las_d = txt_u->hed_u.fir_d + cot_d - 1;
    }

    txt_u->dit_o = c3y;
    _book_save_head(txt_u);

    //  truncate file
    if ( -1 == ftruncate(txt_u->fid_i, off_w) ) {
      fprintf(stderr, "book: failed to truncate: %s\r\n",
              strerror(errno));
    } else {
      c3_sync(txt_u->fid_i);
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
  u3_book* txt_u;

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
  txt_u = c3_calloc(sizeof(u3_book));
  txt_u->fid_i = fid_i;
  txt_u->pax_c = c3_malloc(strlen(path_c) + 1);
  strcpy(txt_u->pax_c, path_c);

  if ( buf_u.st_size == 0 ) {
    //  new file: initialize header
    _book_init_head(txt_u);
    _book_save_head(txt_u);
    //  events start after header + reserved metadata area
    txt_u->off_w = sizeof(u3_book_head) + BOOK_META_SIZE;
  }
  else if ( buf_u.st_size < (off_t)sizeof(u3_book_head) ) {
    //  corrupt file: too small
    fprintf(stderr, "book: file too small: %lld bytes\r\n",
            (long long)buf_u.st_size);
    close(fid_i);
    c3_free(txt_u->pax_c);
    c3_free(txt_u);
    return 0;
  }
  else {
    //  existing file: read and validate header
    if ( c3n == _book_read_head(txt_u) ) {
      close(fid_i);
      c3_free(txt_u->pax_c);
      c3_free(txt_u);
      return 0;
    }

    //  scan to find actual end, recover from corruption
    txt_u->off_w = _book_scan_end(txt_u);
  }

  return txt_u;
}

/* u3_book_exit(): close event log.
*/
void
u3_book_exit(u3_book* txt_u)
{
  if ( !txt_u ) {
    return;
  }

  //  sync header if dirty
  if ( c3y == txt_u->dit_o ) {
    _book_save_head(txt_u);
  }

  //  close file
  close(txt_u->fid_i);

  //  free resources
  c3_free(txt_u->pax_c);
  c3_free(txt_u);
}

/* u3_book_gulf(): read first and last event numbers.
*/
c3_o
u3_book_gulf(u3_book* txt_u, c3_d* low_d, c3_d* hig_d)
{
  if ( !txt_u ) {
    return c3n;
  }

  *low_d = txt_u->hed_u.fir_d;
  *hig_d = txt_u->hed_u.las_d;

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
  fprintf(stderr, "  first event: %" PRIu64 "\r\n", hed_u.fir_d);
  fprintf(stderr, "  last event: %" PRIu64 "\r\n", hed_u.las_d);
  fprintf(stderr, "  event count: %" PRIu64 "\r\n",
          (0 == hed_u.las_d ) ? 0 :
          (hed_u.las_d - hed_u.fir_d + 1));
  fprintf(stderr, "  file size: %lld bytes\r\n", (long long)buf_u.st_size);
  fprintf(stderr, "  metadata offset: %u\r\n", hed_u.off_w);
  fprintf(stderr, "  metadata length: %u\r\n", hed_u.len_w);

  close(fid_i);
}

/* u3_book_save(): save [len_d] events starting at [eve_d].
**
**   byt_p: array of buffers (mug + jam)
**   siz_i: array of buffer sizes
*/
c3_o
u3_book_save(u3_book* txt_u,
             c3_d     eve_d,
             c3_d     len_d,
             void**   byt_p,
             c3_z*    siz_i,
             c3_d     epo_d)
{
  c3_w now_w;

  if ( !txt_u ) {
    return c3n;
  }

  //  validate contiguity
  if ( 0 == txt_u->hed_u.las_d ) {
    //  empty log: first event must be the first event in the epoch
    if ( epo_d + 1 != eve_d ) {
      fprintf(stderr, "book: first event must be 1, got %" PRIu64 "\r\n", eve_d);
      return c3n;
    }
    txt_u->hed_u.fir_d = eve_d;
  }
  else {
    //  non-empty: must be contiguous
    if ( eve_d != txt_u->hed_u.las_d + 1 ) {
      fprintf(stderr, "book: event gap: expected %" PRIu64 ", got %" PRIu64 "\r\n",
              txt_u->hed_u.las_d + 1, eve_d);
      return c3n;
    }
  }

  //  write each event deed
  now_w = txt_u->off_w;

  for ( c3_w i_w = 0; i_w < len_d; i_w++ ) {
    c3_y* buf_y = (c3_y*)byt_p[i_w];
    c3_d  siz_d = (c3_d)siz_i[i_w];
    u3_book_reed red_u;

    //  extract mug from buffer (first 4 bytes)
    if ( siz_d < 4 ) {
      fprintf(stderr, "book: event %" PRIu64 " buffer too small: %" PRIu64 "\r\n",
              eve_d + i_w, siz_d);
      return c3n;
    }

    //  build reed from input buffer
    memcpy(&red_u.mug_l, buf_y, 4);
    red_u.jam_y = buf_y + 4;
    red_u.len_d = siz_d;  //  total payload: mug + jam
    red_u.crc_w = _book_calc_crc(&red_u);

    //  save deed to file
    if ( c3n == _book_save_deed(txt_u->fid_i, &now_w, &red_u) ) {
      fprintf(stderr, "book: failed to save deed for event %" PRIu64 ": %s\r\n",
              eve_d + i_w, strerror(errno));
      return c3n;
    }
  }

  //  sync data to disk
  if ( -1 == c3_sync(txt_u->fid_i) ) {
    fprintf(stderr, "book: failed to sync events: %s\r\n",
            strerror(errno));
    return c3n;
  }

  //  update header
  txt_u->hed_u.las_d = eve_d + len_d - 1;
  txt_u->off_w = now_w;
  txt_u->dit_o = c3y;

  //  write and sync header
  if ( c3n == _book_save_head(txt_u) ) {
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
u3_book_read(u3_book* txt_u,
             void*    ptr_v,
             c3_d     eve_d,
             c3_d     len_d,
             c3_o  (*read_f)(void*, c3_d, c3_z, void*))
{
  c3_w  off_w;
  c3_d  cur_d;

  if ( !txt_u ) {
    return c3n;
  }

  //  validate range
  if ( 0 == txt_u->hed_u.las_d ) {
    //  empty log
    fprintf(stderr, "book: read from empty log\r\n");
    return c3n;
  }

  if ( eve_d < txt_u->hed_u.fir_d || eve_d > txt_u->hed_u.las_d ) {
    fprintf(stderr, "book: event %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\r\n",
            eve_d, txt_u->hed_u.fir_d, txt_u->hed_u.las_d);
    return c3n;
  }

  if ( eve_d + len_d - 1 > txt_u->hed_u.las_d ) {
    fprintf(stderr, "book: read range exceeds last event\r\n");
    return c3n;
  }

  //  scan to starting event (events start after header + metadata area)
  off_w = sizeof(u3_book_head) + BOOK_META_SIZE;
  cur_d = txt_u->hed_u.fir_d;

  while ( cur_d < eve_d ) {
    if ( c3n == _book_skip_deed(txt_u->fid_i, &off_w) ) {
      fprintf(stderr, "book: failed to scan to event %" PRIu64 "\r\n", eve_d);
      return c3n;
    }
    cur_d++;
  }

  //  read requested events
  for ( c3_d i_d = 0; i_d < len_d; i_d++, cur_d++ ) {
    u3_book_reed red_u;
    c3_y* buf_y;
    c3_z  len_z;

    //  read deed into reed
    if ( c3n == _book_read_deed(txt_u->fid_i, &off_w, &red_u) ) {
      fprintf(stderr, "book: failed to read event %" PRIu64 "\r\n", cur_d);
      return c3n;
    }

    //  validate reed
    if ( c3n == _book_okay_reed(&red_u) ) {
      fprintf(stderr, "book: validation failed at event %" PRIu64 "\r\n", cur_d);
      c3_free(red_u.jam_y);
      return c3n;
    }

    //  reconstruct buffer in mug + jam format for callback
    len_z = red_u.len_d;
    buf_y = c3_malloc(len_z);
    memcpy(buf_y, &red_u.mug_l, 4);
    memcpy(buf_y + 4, red_u.jam_y, red_u.len_d - 4);

    c3_free(red_u.jam_y);

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
u3_book_walk_init(u3_book*      txt_u,
                  u3_book_walk* itr_u,
                  c3_d          nex_d,
                  c3_d          las_d)
{
  c3_w off_w;
  c3_d cur_d;

  if ( !txt_u || !itr_u ) {
    return c3n;
  }

  //  validate range
  if ( 0 == txt_u->hed_u.las_d ) {
    fprintf(stderr, "book: walk_init on empty log\r\n");
    return c3n;
  }

  if ( nex_d < txt_u->hed_u.fir_d || nex_d > txt_u->hed_u.las_d ) {
    fprintf(stderr, "book: walk_init start %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\r\n",
            nex_d, txt_u->hed_u.fir_d, txt_u->hed_u.las_d);
    return c3n;
  }

  if ( las_d < nex_d || las_d > txt_u->hed_u.las_d ) {
    fprintf(stderr, "book: walk_init end %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\r\n",
            las_d, nex_d, txt_u->hed_u.las_d);
    return c3n;
  }

  //  scan to starting event (events start after header + metadata area)
  off_w = sizeof(u3_book_head) + BOOK_META_SIZE;
  cur_d = txt_u->hed_u.fir_d;

  while ( cur_d < nex_d ) {
    if ( c3n == _book_skip_deed(txt_u->fid_i, &off_w) ) {
      fprintf(stderr, "book: walk_init failed to scan to event %" PRIu64 "\r\n", nex_d);
      return c3n;
    }
    cur_d++;
  }

  //  initialize iterator
  itr_u->fid_i = txt_u->fid_i;
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
  u3_book_reed red_u;
  c3_y* buf_y;

  if ( !itr_u || c3n == itr_u->liv_o ) {
    return c3n;
  }

  //  check if we've reached the end
  if ( itr_u->nex_d > itr_u->las_d ) {
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  read deed into reed
  if ( c3n == _book_read_deed(itr_u->fid_i, &itr_u->off_w, &red_u) ) {
    fprintf(stderr, "book: walk_next failed to read event %" PRIu64 "\r\n",
            itr_u->nex_d);
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  validate reed
  if ( c3n == _book_okay_reed(&red_u) ) {
    fprintf(stderr, "book: walk_next validation failed at event %" PRIu64 "\r\n",
            itr_u->nex_d);
    c3_free(red_u.jam_y);
    itr_u->liv_o = c3n;
    return c3n;
  }

  //  reconstruct buffer in mug + jam format
  *len_z = red_u.len_d;
  buf_y = c3_malloc(*len_z);
  memcpy(buf_y, &red_u.mug_l, 4);
  memcpy(buf_y + 4, red_u.jam_y, red_u.len_d - 4);

  c3_free(red_u.jam_y);

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
u3_book_read_meta(u3_book*    txt_u,
                  void*       ptr_v,
                  const c3_c* key_c,
                  void     (*read_f)(void*, c3_zs, void*))
{
  c3_w   ken_w;  //  key length
  c3_y*  buf_y;  //  metadata buffer
  c3_w   len_w;  //  metadata length
  c3_zs  ret_zs;
  c3_w   off_w;
  c3_w   cot_w;  //  count

  if ( !txt_u ) {
    read_f(ptr_v, -1, 0);
    return;
  }

  //  check if metadata section exists
  if ( 0 == txt_u->hed_u.len_w ) {
    read_f(ptr_v, -1, 0);
    return;
  }

  //  read entire metadata section
  len_w = txt_u->hed_u.len_w;
  buf_y = c3_malloc(len_w);

  ret_zs = pread(txt_u->fid_i, buf_y, len_w, txt_u->hed_u.off_w);
  if ( ret_zs != (c3_zs)len_w ) {
    fprintf(stderr, "book: read_meta: failed to read metadata section\r\n");
    c3_free(buf_y);
    read_f(ptr_v, -1, 0);
    return;
  }

  //  parse metadata section
  //  format: [4 bytes: count] + entries
  //  entry:  [4 bytes: key_len][key][4 bytes: val_len][val]

  if ( len_w < 4 ) {
    fprintf(stderr, "book: read_meta: metadata section too small\r\n");
    c3_free(buf_y);
    read_f(ptr_v, -1, 0);
    return;
  }

  memcpy(&cot_w, buf_y, 4);
  off_w = 4;

  ken_w = strlen(key_c);

  //  linear search for key
  for ( c3_w i_w = 0; i_w < cot_w; i_w++ ) {
    c3_w   entry_key_len;
    c3_y*  entry_key;
    c3_w   entry_val_len;
    c3_y*  entry_val;

    //  read key length
    if ( off_w + 4 > len_w ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (key len)\r\n");
      c3_free(buf_y);
      read_f(ptr_v, -1, 0);
      return;
    }
    memcpy(&entry_key_len, buf_y + off_w, 4);
    off_w += 4;

    //  read key
    if ( off_w + entry_key_len > len_w ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (key)\r\n");
      c3_free(buf_y);
      read_f(ptr_v, -1, 0);
      return;
    }
    entry_key = buf_y + off_w;
    off_w += entry_key_len;

    //  read value length
    if ( off_w + 4 > len_w ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (val len)\r\n");
      c3_free(buf_y);
      read_f(ptr_v, -1, 0);
      return;
    }
    memcpy(&entry_val_len, buf_y + off_w, 4);
    off_w += 4;

    //  read value
    if ( off_w + entry_val_len > len_w ) {
      fprintf(stderr, "book: read_meta: corrupt metadata (val)\r\n");
      c3_free(buf_y);
      read_f(ptr_v, -1, 0);
      return;
    }
    entry_val = buf_y + off_w;
    off_w += entry_val_len;

    //  check if this is the key we're looking for
    if ( entry_key_len == ken_w &&
         0 == memcmp(entry_key, key_c, ken_w) )
    {
      //  found it - invoke callback
      read_f(ptr_v, entry_val_len, entry_val);
      c3_free(buf_y);
      return;
    }
  }

  //  not found
  c3_free(buf_y);
  read_f(ptr_v, -1, 0);
}

/* u3_book_save_meta(): save metadata by string key into log.
**
**   updates or inserts key-value pair in metadata section.
*/
c3_o
u3_book_save_meta(u3_book*    txt_u,
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
  c3_o   found = c3n;
  c3_zs  ret_zs;

  if ( !txt_u ) {
    return c3n;
  }

  key_len = strlen(key_c);

  //  read existing metadata if present
  if ( 0 != txt_u->hed_u.len_w ) {
    old_len = txt_u->hed_u.len_w;
    old_meta = c3_malloc(old_len);

    ret_zs = pread(txt_u->fid_i, old_meta, old_len, txt_u->hed_u.off_w);
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
    for ( c3_w i_w = 0; i_w < old_count; i_w++ ) {
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
    for ( c3_w i_w = 0; i_w < old_count; i_w++ ) {
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

  ret_zs = pwrite(txt_u->fid_i, new_meta, new_len, new_off);
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
  if ( -1 == c3_sync(txt_u->fid_i) ) {
    fprintf(stderr, "book: save_meta: failed to sync metadata: %s\r\n",
            strerror(errno));
    return c3n;
  }

  //  update header
  txt_u->hed_u.off_w = new_off;
  txt_u->hed_u.len_w = new_len;
  txt_u->dit_o = c3y;

  //  write and sync header
  if ( c3n == _book_save_head(txt_u) ) {
    return c3n;
  }

  //  off_w is not affected by metadata writes - events append at off_w

  return c3y;
}
