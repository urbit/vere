/// @file

#include "db/book.h"

#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <zlib.h>

#include "c3/c3.h"
#include "noun.h"
#include "ship.h"

//  book: append-only event log
//
//    simple file-based persistence layer for urbit's event log.
//    optimized for sequential writes and reads; no random access.
//
//    file format:
//      [24-byte header]
//      [events: len_d | buffer_data | let_d]
//
//    metadata stored in separate meta.bin file
//

  #define BOOK_MAGIC      0x424f4f4b  //  "BOOK"
  #define BOOK_VERSION    1           //  format version

  //  header slot offsets (page-aligned for atomic writes)
  #define BOOK_HEAD_A     0           //  first header slot
  #define BOOK_HEAD_B     4096        //  second header slot
  #define BOOK_DEED_BASE  8192        //  deeds start here

/* _book_head_crc(): compute CRC32 of header fields.
*/
static c3_l
_book_head_crc(const u3_book_head* hed_u)
{
  c3_z len_z = offsetof(u3_book_head, crc_w);
  return (c3_l)crc32(0, (const c3_y*)hed_u, len_z);
}

/* _book_head_okay(): validate header magic, version, and checksum.
*/
static c3_o
_book_head_okay(const u3_book_head* hed_u)
{
  if ( BOOK_MAGIC != hed_u->mag_w ) {
    return c3n;
  }

  if ( BOOK_VERSION != hed_u->ver_w ) {
    return c3n;
  }

  c3_w crc_w = _book_head_crc(hed_u);
  if ( crc_w != hed_u->crc_w ) {
    return c3n;
  }

  return c3y;
}

/* _book_meta_path(): construct path to metadata file.
**
**   NB: caller must free the result.
*/
static c3_c*
_book_meta_path(const c3_c* pax_c)
{
  c3_c* met_c = c3_malloc(strlen(pax_c) + 16);

  if ( !met_c ) {
    return 0;
  }

  snprintf(met_c, strlen(pax_c) + 16, "%s/meta.bin", pax_c);
  return met_c;
}

/* _book_init_meta_file(): open or create metadata file.
*/
static c3_i
_book_init_meta_file(const c3_c* pax_c)
{
  c3_c* met_c = _book_meta_path(pax_c);
  c3_i  met_i = c3_open(met_c, O_RDWR | O_CREAT, 0644);

  if ( 0 > met_i ) {
    c3_free(met_c);
    return -1;
  }

  struct stat buf_u;
  if ( 0 > fstat(met_i, &buf_u) ) {
    goto fail;
  }

  if ( 0 == buf_u.st_size ) {
    u3_book_meta met_u;
    memset(&met_u, 0, sizeof(u3_book_meta));

    if ( sizeof(u3_book_meta) != pwrite(met_i, &met_u, sizeof(u3_book_meta), 0) ) {
      goto fail;
    }

    if ( -1 == c3_sync(met_i) ) {
      goto fail;
    }
  }

  c3_free(met_c);
  return met_i;

fail:
  close(met_i);
  c3_free(met_c);
  return -1;
}

/* _book_read_meta_file(): read metadata from disk.
*/
static c3_o
_book_read_meta_file(c3_i met_i, u3_book_meta* met_u)
{
  if ( 0 > met_i ) {
    return c3n;
  }

  c3_zs ret_zs = pread(met_i, met_u, sizeof(u3_book_meta), 0);
  if ( ret_zs != sizeof(u3_book_meta) ) {
    return c3n;
  }

  return c3y;
}

/* _book_save_meta_file(): write metadata to disk.
*/
static c3_o
_book_save_meta_file(c3_i met_i, const u3_book_meta* met_u)
{
  if ( 0 > met_i ) {
    return c3n;
  }

  c3_zs ret_zs = pwrite(met_i, met_u, sizeof(u3_book_meta), 0);
  if ( ret_zs != sizeof(u3_book_meta) ) {
    return c3n;
  }

  if ( -1 == c3_sync(met_i) ) {
    return c3n;
  }

  return c3y;
}

/* _book_make_head(): initialize and write both header slots for new file.
**
**    fir_d and las_d start at 0, updated when first events are saved.
**    both header slots are initialized identically with seq_d = 0.
*/
static c3_o
_book_make_head(u3_book* txt_u)
{
  c3_zs ret_zs;

  memset(&txt_u->hed_u, 0, sizeof(u3_book_head));
  txt_u->hed_u.mag_w = BOOK_MAGIC;
  txt_u->hed_u.ver_w = BOOK_VERSION;
  txt_u->hed_u.fir_d = 0;
  txt_u->hed_u.las_d = 0;
  txt_u->hed_u.seq_d = 0;
  txt_u->hed_u.crc_w = _book_head_crc(&txt_u->hed_u);

  ret_zs = pwrite(txt_u->fid_i, &txt_u->hed_u,
                  sizeof(u3_book_head), BOOK_HEAD_A);

  if ( ret_zs != sizeof(u3_book_head) ) {
    u3l_log("book: failed to write header A: %s\r\n",
            strerror(errno));
    return c3n;
  }

  ret_zs = pwrite(txt_u->fid_i, &txt_u->hed_u,
                  sizeof(u3_book_head), BOOK_HEAD_B);

  if ( ret_zs != sizeof(u3_book_head) ) {
    u3l_log("book: failed to write header B: %s\r\n",
            strerror(errno));
    return c3n;
  }

  //  extend file so it passes minimum size check on reopen
  if ( -1 == ftruncate(txt_u->fid_i, BOOK_DEED_BASE) ) {
    u3l_log("book: failed to extend file: %s\r\n",
            strerror(errno));
    return c3n;
  }

  if ( -1 == c3_sync(txt_u->fid_i) ) {
    u3l_log("book: failed to sync headers: %s\r\n",
            strerror(errno));
    return c3n;
  }

  txt_u->act_w = 0;  //  start with slot A as active

  return c3y;
}

/* _book_take_head(): select valid header from two candidates.
*/
static c3_o
_book_take_head(const u3_book_head* hed_u, c3_o val_o,
                const u3_book_head* deh_u, c3_o lav_o,
                u3_book_head* out_u, c3_w* act_w)
{
  if ( c3y == val_o && c3y == lav_o ) {
    if ( hed_u->seq_d >= deh_u->seq_d ) {
      *out_u = *hed_u;
      if ( act_w ) *act_w = 0;  //  A
    } else {
      *out_u = *deh_u;
      if ( act_w ) *act_w = 1;  //  B
    }
    return c3y;
  }
  if ( c3y == val_o ) {
    *out_u = *hed_u;
    if ( act_w ) *act_w = 0;    //  A
    return c3y;
  }
  if ( c3y == lav_o ) {
    *out_u = *deh_u;
    if ( act_w ) *act_w = 1;    //  B
    return c3y;
  }
  return c3n;
}

/* _book_read_head(): read both header slots and select valid one.
**
**   reads both header slots, validates checksums, and selects the one
**   with the higher sequence number. this implements the LMDB-style
**   double-buffered commit protocol.
**
**   on success, txt_u->hed_u contains the valid header and txt_u->act_w
**   is set to the active slot index (0 or 1).
*/
static c3_o
_book_read_head(u3_book* txt_u)
{
  u3_book_head hed_a, hed_b;
  c3_o         val_a, val_b;
  c3_zs        ret_zs;

  ret_zs = pread(txt_u->fid_i, &hed_a, sizeof(u3_book_head), BOOK_HEAD_A);
  if ( ret_zs != sizeof(u3_book_head) ) {
    fprintf(stderr, "book: failed to read header A\r\n");
    val_a = c3n;
  }
  else {
    val_a = _book_head_okay(&hed_a);
  }

  ret_zs = pread(txt_u->fid_i, &hed_b, sizeof(u3_book_head), BOOK_HEAD_B);
  if ( ret_zs != sizeof(u3_book_head) ) {
    fprintf(stderr, "book: failed to read header B\r\n");
    val_b = c3n;
  }
  else {
    val_b = _book_head_okay(&hed_b);
  }

  if ( c3n == _book_take_head(&hed_a, val_a, &hed_b, val_b,
                                 &txt_u->hed_u, &txt_u->act_w) ) {
    fprintf(stderr, "book: no valid header found\r\n");
    return c3n;
  }

  return c3y;
}

/* _book_deed_size(): calculate total on-disk size of deed.
*/
static inline c3_w
_book_deed_size(c3_d len_d)
{
  return sizeof(c3_d) + len_d + sizeof(c3_d);
}

/* _book_reed_to_buff(): allocate buffer and copy deed data.
**
**    NB: caller must free the returned buffer.
*/
static c3_y*
_book_reed_to_buff(u3_book_reed* red_u, c3_z* len_z)
{
  *len_z = red_u->len_d;
  c3_y* buf_y = c3_malloc(*len_z);

  if ( !buf_y ) {
    return 0;
  }

  memcpy(buf_y, red_u->buf_y, red_u->len_d);
  c3_free(red_u->buf_y);

  return buf_y;
}

/* _book_read_deed(): read deed from file into [red_u].
**
**   returns:
**     c3y: success, buf_y allocated with complete buffer
**     c3n: failure (EOF or corruption)
**
**   on success, caller must free red_u->buf_y
*/
static c3_o
_book_read_deed(c3_i fid_i, c3_d* off_d, u3_book_reed* red_u)
{
  c3_zs ret_zs;
  c3_d  now_d = *off_d;

  c3_d len_d;
  ret_zs = pread(fid_i, &len_d, sizeof(c3_d), now_d);
  if ( ret_zs != sizeof(c3_d) ) {
    return c3n;
  }
  now_d += sizeof(c3_d);

  red_u->buf_y = c3_malloc(len_d);
  if ( !red_u->buf_y ) {
    return c3n;
  }
  ret_zs = pread(fid_i, red_u->buf_y, len_d, now_d);
  if ( ret_zs != (c3_zs)len_d ) {
    c3_free(red_u->buf_y);
    return c3n;
  }
  now_d += len_d;

  c3_d let_d;
  ret_zs = pread(fid_i, &let_d, sizeof(c3_d), now_d);
  if ( ret_zs != sizeof(c3_d) ) {
    c3_free(red_u->buf_y);
    return c3n;
  }
  now_d += sizeof(c3_d);

  if ( len_d != let_d ) {
    c3_free(red_u->buf_y);
    return c3n;
  }

  red_u->len_d = len_d;
  *off_d = now_d;

  return c3y;
}

/* _book_save_deed(): save complete deed to file using scatter-gather I/O.
**
**   uses pwritev() to write head + buffer + tail in a single syscall.
*/
static c3_o
_book_save_deed(c3_i fid_i, c3_d* off_d, const u3_book_reed* red_u)
{
  c3_d len_d = red_u->len_d;
  c3_d let_d = len_d;

  struct iovec iov_u[3];
  iov_u[0].iov_base = &len_d;
  iov_u[0].iov_len  = sizeof(c3_d);
  iov_u[1].iov_base = red_u->buf_y;
  iov_u[1].iov_len  = len_d;
  iov_u[2].iov_base = &let_d;
  iov_u[2].iov_len  = sizeof(c3_d);

  c3_z tot_z = sizeof(c3_d) + len_d + sizeof(c3_d);
  c3_zs ret_zs = pwritev(fid_i, iov_u, 3, *off_d);

  if ( ret_zs != (c3_zs)tot_z ) {
    return c3n;
  }

  *off_d += tot_z;
  return c3y;
}

/* _book_skip_deed(): advance file offset past next deed without reading it.
*/
static c3_o
_book_skip_deed(c3_i fid_i, c3_d* off_d)
{
  c3_zs ret_zs;
  c3_d  len_d;

  ret_zs = pread(fid_i, &len_d, sizeof(c3_d), *off_d);
  if ( ret_zs != sizeof(c3_d) ) {
    return c3n;
  }

  *off_d += _book_deed_size(len_d);

  return c3y;
}

/* _book_scan_back(): fast reverse scan to validate last deed.
**
**   this is the fast path for normal startup. uses header's las_d
**   as the authoritative last event number, and validates backward
**   from file end using the trailing let_d field.
**
**   on success:
**     - sets *off_d to append offset (byte after last valid deed)
**     - sets txt_u->las_d from header's las_d
**
**   returns:
**     c3y: last deed valid OR file is empty (no deeds)
**     c3n: corruption detected (caller should fall back to _book_scan_fore)
**
**   NB: does NOT truncate file or perform recovery; just reports state.
*/
static c3_o
_book_scan_back(u3_book* txt_u, c3_d* off_d)
{
  struct stat buf_u;
  c3_d        end_d;
  c3_d        pos_d;

  if ( -1 == fstat(txt_u->fid_i, &buf_u) ) {
    *off_d = BOOK_DEED_BASE;
    return c3n;
  }

  end_d = (c3_d)buf_u.st_size;

  //  empty or header-only file is valid (no deeds yet)
  if ( end_d <= BOOK_DEED_BASE ) {
    *off_d = BOOK_DEED_BASE;
    txt_u->las_d = txt_u->hed_u.las_d;
    return c3y;
  }

  //  if header says no events, but file has data beyond header,
  //  that's uncommitted data - fall back to forward scan
  if ( 0 == txt_u->hed_u.las_d ) {
    *off_d = BOOK_DEED_BASE;
    return c3n;
  }

  pos_d = end_d;
  c3_d min_size = sizeof(u3_book_deed) + sizeof(c3_d);

  while ( pos_d > BOOK_DEED_BASE ) {
    c3_zs ret_zs;
    c3_d  let_d;
    c3_d  siz_d;
    c3_d  ded_d;

    if ( pos_d < BOOK_DEED_BASE + min_size ) {
      break;
    }

    ret_zs = pread(txt_u->fid_i, &let_d, sizeof(c3_d),
                   pos_d - sizeof(c3_d));
    if ( ret_zs != sizeof(c3_d) ) {
      break;
    }

    //  calculate deed size and start position
    siz_d = _book_deed_size(let_d);
    if ( siz_d > pos_d - BOOK_DEED_BASE ) {
      //  deed would extend before header
      break;
    }

    ded_d = pos_d - siz_d;

    {
      u3_book_reed red_u;
      c3_d         tmp_d = ded_d;

      if ( c3n == _book_read_deed(txt_u->fid_i, &tmp_d, &red_u) ) {
        break;
      }

      if ( 0 == red_u.len_d ) {
        c3_free(red_u.buf_y);
        break;
      }

      //  deed is valid — use header's las_d as authoritative
      c3_free(red_u.buf_y);
      *off_d = pos_d;
      txt_u->las_d = txt_u->hed_u.las_d;
      return c3y;
    }
  }

  //  no valid deeds found
  *off_d = BOOK_DEED_BASE;
  return c3n;
}

/* _book_scan_fore(): recovery forward scan to find last valid deed.
**
**   used as fallback when _book_scan_back fails (corruption recovery).
**   validates each record's CRC and len_d == let_d sequentially.
**   if corruption is found, truncates file and updates header.
**
**   on completion:
**     - sets *off_d to append offset
**     - sets txt_u->las_d to last valid event number
**     - truncates file if corrupted trailing data was found
**     - updates header if recovery changed the count
**
**   returns:
**     c3y: always (recovery is best-effort)
*/
static c3_o
_book_scan_fore(u3_book* txt_u, c3_d* off_d)
{
  c3_d cur_d = BOOK_DEED_BASE;  //  start of events
  c3_d cot_d = 0;  //  count of valid deeds found
  c3_d las_d = 0;  //  last valid event number found
  c3_d exp_d;      //  expected event count from header

  if ( 0 == txt_u->hed_u.fir_d && 0 == txt_u->hed_u.las_d ) {
    //  empty log is valid (no deeds yet)
    txt_u->las_d = 0;
    *off_d = cur_d;
    return c3y;
  }

  //  expected count based on header's las_d
  //  NB: fir_d is the epoch base; events are fir_d+1 through las_d
  exp_d = ( txt_u->hed_u.las_d > txt_u->hed_u.fir_d )
        ? txt_u->hed_u.las_d - txt_u->hed_u.fir_d
        : 0;

  while ( 1 ) {
    u3_book_reed red_u;
    c3_d  beg_d = cur_d;

    if ( c3n == _book_read_deed(txt_u->fid_i, &cur_d, &red_u) ) {
      break;
    }

    if ( 0 == red_u.len_d ) {
      u3l_log("book: validation failed at offset %" PRIu64 "\r\n", beg_d);
      c3_free(red_u.buf_y);
      break;
    }

    //  deed is valid - calculate its event number
    //  NB: first deed is event fir_d + 1
    las_d = txt_u->hed_u.fir_d + 1 + cot_d;
    c3_free(red_u.buf_y);
    cot_d++;
  }

  //  check if we found fewer events than header claims
  if ( cot_d != exp_d ) {
    u3l_log("book: recovery: found %" PRIu64 " events, expected %" PRIu64 "\r\n",
            cot_d, exp_d);

    //  update las_d based on what we found
    if ( 0 == cot_d ) {
      txt_u->las_d = 0;
      las_d = 0;
      cur_d = BOOK_DEED_BASE;
    } else {
      txt_u->las_d = las_d;
    }

    //  truncate file to remove invalid data
    if ( -1 == ftruncate(txt_u->fid_i, cur_d) ) {
      u3l_log("book: failed to truncate: %s\r\n",
              strerror(errno));
    } else {
      if ( -1 == c3_sync(txt_u->fid_i) ) {
        u3l_log("book: failed to sync after truncate: %s\r\n",
                strerror(errno));
      }
    }

    //  update header to match recovered state (write to inactive slot)
    txt_u->hed_u.las_d = las_d;
    txt_u->hed_u.seq_d++;
    txt_u->hed_u.crc_w = _book_head_crc(&txt_u->hed_u);

    c3_d slot_d = (txt_u->act_w == 0) ? BOOK_HEAD_B : BOOK_HEAD_A;
    if ( sizeof(u3_book_head) != pwrite(txt_u->fid_i, &txt_u->hed_u,
                                        sizeof(u3_book_head), slot_d) )
    {
      u3l_log("book: failed to update header: %s\r\n", strerror(errno));
    } else {
      if ( -1 == c3_sync(txt_u->fid_i) ) {
        u3l_log("book: failed to sync header: %s\r\n", strerror(errno));
      }
      txt_u->act_w = (txt_u->act_w == 0) ? 1 : 0;
    }
  } else {
    txt_u->las_d = las_d;
  }

  *off_d = cur_d;
  return c3y;
}

/* _book_pull_epoc(): parse epoch number from directory path.
**
**   expects path ending in "0iN" where N is the epoch number.
**
**   returns: c3y on success with *epo_d set, c3n on failure
*/
static c3_o
_book_pull_epoc(const c3_c* pax_c, c3_d* epo_d)
{
  const c3_c* las_c = strrchr(pax_c, '/');
  las_c = las_c ? las_c + 1 : pax_c;

  //  expect "0iN" format
  if ( strncmp(las_c, "0i", 2) != 0 || !las_c[2] ) {
    fprintf(stderr, "book: init must be called with epoch directory\r\n");
    return c3n;
  }

  errno = 0;
  *epo_d = strtoull(las_c + 2, NULL, 10);
  if ( errno == EINVAL ) {
    fprintf(stderr, "book: invalid epoch number in path\r\n");
    return c3n;
  }

  return c3y;
}

/* u3_book_init(): open/create event log in epoch directory.
*/
u3_book*
u3_book_init(const c3_c* pax_c)
{
  c3_c        log_c[8193];
  c3_i met_i, fid_i = -1;
  struct stat buf_u;
  u3_book*    txt_u = 0;

  snprintf(log_c, sizeof(log_c), "%s/book.log", pax_c);

  fid_i = c3_open(log_c, O_RDWR | O_CREAT, 0644);
  if ( 0 > fid_i ) {
    u3l_log("book: failed to open %s: %s\r\n", log_c, strerror(errno));
    return 0;
  }

  met_i = _book_init_meta_file(pax_c);
  if ( 0 > met_i ) {
    u3l_log("book: failed to open meta.bin\r\n");
    goto fail1;
  }

  if ( 0 > fstat(fid_i, &buf_u) ) {
    u3l_log("book: fstat failed: %s\r\n", strerror(errno));
    goto fail2;
  }

  txt_u = c3_calloc(sizeof(u3_book));
  txt_u->fid_i = fid_i;
  txt_u->met_i = met_i;
  txt_u->pax_c = c3_malloc(strlen(log_c) + 1);
  if ( !txt_u->pax_c ) {
    goto fail3;
  }
  strcpy(txt_u->pax_c, log_c);

  if ( buf_u.st_size == 0 ) {
    //  new file: initialize and write header
    if ( c3n == _book_make_head(txt_u) ) {
      goto fail4;
    }

    //  extract epoch number from path
    c3_d epo_d;
    if ( c3n == _book_pull_epoc(pax_c, &epo_d) ) {
      goto fail3;
    }

    if ( epo_d ) {
      //  update header with epoch info and rewrite both slots
      txt_u->hed_u.fir_d = epo_d;
      txt_u->hed_u.las_d = epo_d;
      txt_u->hed_u.crc_w = _book_head_crc(&txt_u->hed_u);

      //  write header slot A
      if ( sizeof(u3_book_head) != pwrite(fid_i, &txt_u->hed_u,
                                          sizeof(u3_book_head), BOOK_HEAD_A) )
      {
        u3l_log("book: failed to write header A: %s\r\n", strerror(errno));
        goto fail4;
      }

      //  write header slot B
      if ( sizeof(u3_book_head) != pwrite(fid_i, &txt_u->hed_u,
                                          sizeof(u3_book_head), BOOK_HEAD_B) )
      {
        u3l_log("book: failed to write header B: %s\r\n", strerror(errno));
        goto fail4;
      }

      if ( -1 == c3_sync(fid_i) ) {
        u3l_log("book: failed to sync header: %s\r\n", strerror(errno));
        goto fail4;
      }
    }

    txt_u->las_d = epo_d;
    txt_u->off_d = BOOK_DEED_BASE;
  }
  else if ( buf_u.st_size < (off_t)BOOK_DEED_BASE ) {
    //  corrupt file: too small for headers
    u3l_log("book: file too small: %lld bytes\r\n", (long long)buf_u.st_size);
    goto fail4;
  }
  else {
    //  existing file: read and validate header
    if ( c3n == _book_read_head(txt_u) ) {
      goto fail4;
    }

    //  try fast reverse scan first
    if ( c3n == _book_scan_back(txt_u, &txt_u->off_d) ) {
      //  fall back to forward scan for recovery
      _book_scan_fore(txt_u, &txt_u->off_d);
    }

    //  fir_d pre-initialized but no events found: set las_d to match
    if ( txt_u->hed_u.fir_d && !txt_u->las_d ) {
      txt_u->las_d = txt_u->hed_u.fir_d;
    }
  }

  return txt_u;

fail4:
  c3_free(txt_u->pax_c);
fail3:
  c3_free(txt_u);
fail2:
  close(met_i);
fail1:
  close(fid_i);
  return 0;
}

/* u3_book_exit(): close event log and release resources.
*/
void
u3_book_exit(u3_book* txt_u)
{
  if ( !txt_u ) {
    return;
  }

  close(txt_u->fid_i);

  if ( 0 <= txt_u->met_i ) {
    close(txt_u->met_i);
  }

  c3_free(txt_u->pax_c);
  c3_free(txt_u);
}

/* u3_book_gulf(): read first and last event numbers from log.
*/
c3_o
u3_book_gulf(u3_book* txt_u, c3_d* low_d, c3_d* hig_d)
{
  if ( !txt_u ) {
    return c3n;
  }

  *low_d = txt_u->hed_u.fir_d;
  *hig_d = txt_u->las_d;

  return c3y;
}

void
u3_book_stat(const c3_c* log_c)
{
  c3_i fid_i;
  u3_book_head hed_a, hed_b, hed_u;
  c3_o         val_a, val_b;
  struct stat buf_u;

  fid_i = c3_open(log_c, O_RDONLY, 0);
  if ( fid_i < 0 ) {
    fprintf(stderr, "book: failed to open %s: %s\r\n", log_c, strerror(errno));
    return;
  }

  c3_zs ret_zs;
  ret_zs = pread(fid_i, &hed_a, sizeof(u3_book_head), BOOK_HEAD_A);
  val_a = (ret_zs == sizeof(u3_book_head)) ? _book_head_okay(&hed_a) : c3n;

  ret_zs = pread(fid_i, &hed_b, sizeof(u3_book_head), BOOK_HEAD_B);
  val_b = (ret_zs == sizeof(u3_book_head)) ? _book_head_okay(&hed_b) : c3n;

  if ( c3n == _book_take_head(&hed_a, val_a, &hed_b, val_b, &hed_u, 0) ) {
    fprintf(stderr, "book: no valid header found\r\n");
    close(fid_i);
    return;
  }

  if ( fstat(fid_i, &buf_u) < 0 ) {
    fprintf(stderr, "book: fstat failed\r\n");
    close(fid_i);
    return;
  }

  fprintf(stderr, "book info:\r\n");
  fprintf(stderr, "  file: %s\r\n", log_c);
  fprintf(stderr, "  format: %u\r\n", hed_u.ver_w);
  fprintf(stderr, "  first event: %" PRIu64 "\r\n", hed_u.fir_d);
  fprintf(stderr, "  last event: %" PRIu64 "\r\n", hed_u.las_d);
  fprintf(stderr, "  sequence: %" PRIu64 "\r\n", hed_u.seq_d);
  fprintf(stderr, "  file size: %lld bytes\r\n", (long long)buf_u.st_size);

  u3_book_meta met_u;
  c3_c* epo_c = 0;
  {
    const c3_c* sep_c = strrchr(log_c, '/');
    if ( sep_c && 0 == strcmp(sep_c, "/book.log") ) {
      c3_z len_z = sep_c - log_c;
      epo_c = c3_malloc(len_z + 1);
      if ( epo_c ) {
        memcpy(epo_c, log_c, len_z);
        epo_c[len_z] = '\0';
      }
    }
  }
  c3_c* met_c = epo_c ? _book_meta_path(epo_c) : 0;
  c3_free(epo_c);
  c3_i met_i = c3_open(met_c, O_RDONLY, 0);

  if ( met_i >= 0 ) {
    c3_zs ret_zs = pread(met_i, &met_u, sizeof(u3_book_meta), 0);
    if ( ret_zs == sizeof(u3_book_meta) ) {
      fprintf(stderr, "\r\ndisk metadata:\r\n");
      fprintf(stderr, "  who: %s\r\n", u3_ship_to_string(met_u.who_d));
      fprintf(stderr, "  version: %u\r\n", met_u.ver_w);
      fprintf(stderr, "  fake: %s\r\n", _(met_u.fak_o) ? "yes" : "no");
      fprintf(stderr, "  life: %u\r\n", met_u.lif_w);
    }
    close(met_i);
  }
  c3_free(met_c);

  close(fid_i);
}

/* u3_book_save(): save [len_d] events starting at [eve_d].
**
**   byt_p: array of buffers
**   siz_i: array of buffer sizes
**
**   uses double-buffered headers for single-fsync commits:
**   1. write deed data
**   2. write updated header to INACTIVE slot
**   3. single fsync makes both durable atomically
*/
c3_o
u3_book_save(u3_book* txt_u,
             c3_d     eve_d,
             c3_d     len_d,
             void**   byt_p,
             c3_z*    siz_i,
             c3_d     epo_d)
{
  c3_d now_d;

  if ( !txt_u ) {
    return c3n;
  }

  //  validate contiguity
  if ( 0 == txt_u->hed_u.fir_d && 0 == txt_u->las_d ) {
    //  empty log: first event must be the first event in the epoch
    if ( epo_d + 1 != eve_d ) {
      fprintf(stderr, "book: first event must be start of epoch, "
                      "expected %" PRIu64 ", got %" PRIu64
                      "\r\n", epo_d + 1, eve_d);
      return c3n;
    }
    //  fir_d is the epoch base (last event before this epoch)
    txt_u->hed_u.fir_d = epo_d;
  }
  else {
    //  non-empty: must be contiguous
    if ( eve_d != txt_u->las_d + 1 ) {
      fprintf(stderr, "book: event gap: expected %" PRIu64 ", got %" PRIu64 "\r\n",
              txt_u->las_d + 1, eve_d);
      return c3n;
    }
  }

  //  batch write all deeds using scatter-gather I/O
  //
  //  for each deed we need 3 iovec entries: len_d + buffer + let_d
  //  pwritev has IOV_MAX limit (typically 1024), so we chunk if needed
  //
  now_d = txt_u->off_d;

  #define BOOK_IOV_MAX 1020
  c3_w max_deeds_w = BOOK_IOV_MAX / 3;

  c3_d* len_u = c3_malloc(len_d * sizeof(c3_d));
  c3_d* let_u = c3_malloc(len_d * sizeof(c3_d));

  c3_w iov_max_w = (len_d < max_deeds_w) ? len_d * 3 : BOOK_IOV_MAX;
  struct iovec* iov_u = c3_malloc(iov_max_w * sizeof(struct iovec));

  if ( !len_u || !let_u || !iov_u ) {
    c3_free(len_u);
    c3_free(let_u);
    c3_free(iov_u);
    fprintf(stderr, "book: failed to allocate batch write buffers\r\n");
    return c3n;
  }

  for ( c3_w i_w = 0; i_w < len_d; i_w++ ) {
    c3_y* buf_y = (c3_y*)byt_p[i_w];
    c3_d  siz_d = (c3_d)siz_i[i_w];

    if ( siz_d < 4 ) {
      fprintf(stderr, "book: event %" PRIu64 " buffer too small: %" PRIu64 "\r\n",
              eve_d + i_w, siz_d);
      c3_free(len_u);
      c3_free(let_u);
      c3_free(iov_u);
      return c3n;
    }

    len_u[i_w] = siz_d;
    let_u[i_w] = siz_d;
  }

  #define DEEDS_PER_CHUNK (BOOK_IOV_MAX / 3)
  c3_w dun_w = 0;

  while ( dun_w < len_d ) {
    c3_w cun_w = len_d - dun_w;
    if ( cun_w > DEEDS_PER_CHUNK ) {
      cun_w = DEEDS_PER_CHUNK;
    }

    c3_z cun_z = 0;
    for ( c3_w i_w = 0; i_w < cun_w; i_w++ ) {
      c3_w src_w = dun_w + i_w;
      c3_w idx_w = i_w * 3;
      c3_y* buf_y = (c3_y*)byt_p[src_w];

      iov_u[idx_w + 0].iov_base = &len_u[src_w];
      iov_u[idx_w + 0].iov_len  = sizeof(c3_d);
      iov_u[idx_w + 1].iov_base = buf_y;
      iov_u[idx_w + 1].iov_len  = siz_i[src_w];
      iov_u[idx_w + 2].iov_base = &let_u[src_w];
      iov_u[idx_w + 2].iov_len  = sizeof(c3_d);

      cun_z += sizeof(c3_d) + siz_i[src_w] + sizeof(c3_d);
    }

    c3_zs ret_zs = pwritev(txt_u->fid_i, iov_u, cun_w * 3, now_d);

    if ( ret_zs != (c3_zs)cun_z ) {
      fprintf(stderr, "book: batch write failed: wrote %zd of %zu bytes: %s\r\n",
              ret_zs, cun_z, strerror(errno));
      c3_free(len_u);
      c3_free(let_u);
      c3_free(iov_u);
      return c3n;
    }

    now_d += cun_z;
    dun_w += cun_w;
  }

  c3_free(len_u);
  c3_free(let_u);
  c3_free(iov_u);

  c3_d new_las_d = eve_d + len_d - 1;
  txt_u->hed_u.las_d = new_las_d;
  txt_u->hed_u.seq_d++;
  txt_u->hed_u.crc_w = _book_head_crc(&txt_u->hed_u);

  //  write to inactive slot (double-buffer protocol)
  c3_d slot_d = (txt_u->act_w == 0) ? BOOK_HEAD_B : BOOK_HEAD_A;
  if ( sizeof(u3_book_head) != pwrite(txt_u->fid_i, &txt_u->hed_u,
                                      sizeof(u3_book_head), slot_d) )
  {
    fprintf(stderr, "book: failed to write header: %s\r\n", strerror(errno));
    return c3n;
  }

  //  single fsync: makes both deed data and new header durable atomically
  if ( -1 == c3_sync(txt_u->fid_i) ) {
    fprintf(stderr, "book: failed to sync: %s\r\n", strerror(errno));
    return c3n;
  }

  txt_u->act_w = (txt_u->act_w == 0) ? 1 : 0;
  txt_u->las_d = new_las_d;
  txt_u->off_d = now_d;

  return c3y;
}

/* u3_book_read(): read events from log, invoking callback for each event.
*/
c3_o
u3_book_read(u3_book* txt_u,
             void*    ptr_v,
             c3_d     eve_d,
             c3_d     len_d,
             c3_o  (*read_f)(void*, c3_d, c3_z, void*))
{
  c3_d  off_d;
  c3_d  cur_d;

  if ( !txt_u ) {
    return c3n;
  }

  if ( 0 == txt_u->las_d ) {
    fprintf(stderr, "book: read from empty log\r\n");
    return c3n;
  }

  //  NB: fir_d is the epoch base; first stored event is fir_d + 1
  if ( eve_d <= txt_u->hed_u.fir_d || eve_d > txt_u->las_d ) {
    fprintf(stderr, "book: event %" PRIu64 " out of range (%" PRIu64 ", %" PRIu64 "]\r\n",
            eve_d, txt_u->hed_u.fir_d, txt_u->las_d);
    return c3n;
  }

  if ( eve_d + len_d - 1 > txt_u->las_d ) {
    fprintf(stderr, "book: read range exceeds last event\r\n");
    return c3n;
  }

  //  NB: fir_d is the epoch base; first deed is event fir_d + 1
  off_d = BOOK_DEED_BASE;
  cur_d = txt_u->hed_u.fir_d + 1;

  while ( cur_d < eve_d ) {
    if ( c3n == _book_skip_deed(txt_u->fid_i, &off_d) ) {
      fprintf(stderr, "book: failed to scan to event %" PRIu64 "\r\n", eve_d);
      return c3n;
    }
    cur_d++;
  }

  for ( c3_d i_d = 0; i_d < len_d; i_d++, cur_d++ ) {
    u3_book_reed red_u;
    c3_y* buf_y;
    c3_z  len_z;

    if ( c3n == _book_read_deed(txt_u->fid_i, &off_d, &red_u) ) {
      fprintf(stderr, "book: failed to read event %" PRIu64 "\r\n", cur_d);
      return c3n;
    }

    if ( 0 == red_u.len_d ) {
      fprintf(stderr, "book: validation failed at event %" PRIu64 "\r\n", cur_d);
      c3_free(red_u.buf_y);
      return c3n;
    }

    buf_y = _book_reed_to_buff(&red_u, &len_z);
    if ( !buf_y ) {
      c3_free(red_u.buf_y);
      return c3n;
    }

    if ( c3n == read_f(ptr_v, cur_d, len_z, buf_y) ) {
      c3_free(buf_y);
      return c3n;
    }

    c3_free(buf_y);
  }

  return c3y;
}

void
u3_book_read_meta(u3_book*    txt_u,
                  void*       ptr_v,
                  const c3_c* key_c,
                  void     (*read_f)(void*, c3_zs, void*))
{
  u3_book_meta met_u;

  if ( !txt_u ) {
    read_f(ptr_v, -1, 0);
    return;
  }

  if ( c3n == _book_read_meta_file(txt_u->met_i, &met_u) ) {
    u3l_log("book: read_meta: failed to read metadata\r\n");
    read_f(ptr_v, -1, 0);
    return;
  }

  if ( 0 == strcmp(key_c, "version") ) {
    read_f(ptr_v, sizeof(c3_w), &met_u.ver_w);
  }
  else if ( 0 == strcmp(key_c, "who") ) {
    read_f(ptr_v, sizeof(c3_d[2]), met_u.who_d);
  }
  else if ( 0 == strcmp(key_c, "fake") ) {
    read_f(ptr_v, sizeof(c3_o), &met_u.fak_o);
  }
  else if ( 0 == strcmp(key_c, "life") ) {
    read_f(ptr_v, sizeof(c3_w), &met_u.lif_w);
  }
  else {
    read_f(ptr_v, -1, 0);
  }
}

/* u3_book_save_meta(): write fixed metadata section.
*/
c3_o
u3_book_save_meta(u3_book*    txt_u,
                  const c3_c* key_c,
                  c3_z        val_z,
                  void*       val_p)
{
  u3_book_meta met_u;

  if ( !txt_u ) {
    return c3n;
  }

  if ( c3n == _book_read_meta_file(txt_u->met_i, &met_u) ) {
    u3l_log("book: save_meta: failed to read current metadata\r\n");
    return c3n;
  }

  if ( 0 == strcmp(key_c, "version") ) {
    if ( val_z != sizeof(c3_w) ) return c3n;
    memcpy(&met_u.ver_w, val_p, val_z);
  }
  else if ( 0 == strcmp(key_c, "who") ) {
    if ( val_z != sizeof(c3_d[2]) ) return c3n;
    memcpy(met_u.who_d, val_p, val_z);
  }
  else if ( 0 == strcmp(key_c, "fake") ) {
    if ( val_z != sizeof(c3_o) ) return c3n;
    memcpy(&met_u.fak_o, val_p, val_z);
  }
  else if ( 0 == strcmp(key_c, "life") ) {
    if ( val_z != sizeof(c3_w) ) return c3n;
    memcpy(&met_u.lif_w, val_p, val_z);
  }
  else {
    return c3n;
  }

  if ( c3n == _book_save_meta_file(txt_u->met_i, &met_u) ) {
    u3l_log("book: save_meta: failed to write metadata\r\n");
    return c3n;
  }

  return c3y;
}

/* u3_book_walk_init(): initialize event iterator.
*/
c3_o
u3_book_walk_init(u3_book*      txt_u,
                  u3_book_walk* itr_u,
                  c3_d          nex_d,
                  c3_d          las_d)
{
  c3_d off_d;
  c3_d cur_d;

  if ( !txt_u || !itr_u ) {
    return c3n;
  }

  if ( 0 == txt_u->las_d ) {
    fprintf(stderr, "book: walk_init on empty log\r\n");
    return c3n;
  }

  //  NB: fir_d is the epoch base; first stored event is fir_d + 1
  if ( nex_d <= txt_u->hed_u.fir_d || nex_d > txt_u->las_d ) {
    fprintf(stderr, "book: walk_init start %" PRIu64 " out of range (%" PRIu64 ", %" PRIu64 "]\r\n",
            nex_d, txt_u->hed_u.fir_d, txt_u->las_d);
    return c3n;
  }

  if ( las_d < nex_d || las_d > txt_u->las_d ) {
    fprintf(stderr, "book: walk_init end %" PRIu64 " out of range [%" PRIu64 ", %" PRIu64 "]\r\n",
            las_d, nex_d, txt_u->las_d);
    return c3n;
  }

  //  NB: fir_d is the epoch base; first deed is event fir_d + 1
  off_d = BOOK_DEED_BASE;
  cur_d = txt_u->hed_u.fir_d + 1;

  while ( cur_d < nex_d ) {
    if ( c3n == _book_skip_deed(txt_u->fid_i, &off_d) ) {
      fprintf(stderr, "book: walk_init failed to scan to event %" PRIu64 "\r\n", nex_d);
      return c3n;
    }
    cur_d++;
  }

  itr_u->fid_i = txt_u->fid_i;
  itr_u->nex_d = nex_d;
  itr_u->las_d = las_d;
  itr_u->off_d = off_d;
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

  if ( itr_u->nex_d > itr_u->las_d ) {
    itr_u->liv_o = c3n;
    return c3n;
  }

  if ( c3n == _book_read_deed(itr_u->fid_i, &itr_u->off_d, &red_u) ) {
    fprintf(stderr, "book: walk_next failed to read event %" PRIu64 "\r\n",
            itr_u->nex_d);
    itr_u->liv_o = c3n;
    return c3n;
  }

  if ( 0 == red_u.len_d ) {
    fprintf(stderr, "book: walk_next validation failed at event %" PRIu64 "\r\n",
            itr_u->nex_d);
    c3_free(red_u.buf_y);
    itr_u->liv_o = c3n;
    return c3n;
  }

  buf_y = _book_reed_to_buff(&red_u, len_z);
  if ( !buf_y ) {
    c3_free(red_u.buf_y);
    itr_u->liv_o = c3n;
    return c3n;
  }

  *buf_v = buf_y;
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

  itr_u->liv_o = c3n;
  itr_u->fid_i = -1;
}
