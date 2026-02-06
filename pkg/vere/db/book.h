/// @file

#ifndef U3_VERE_DB_BOOK_H
#define U3_VERE_DB_BOOK_H

#include "c3/c3.h"

  /* book: mostly append-only event log
  **
  **   uses double-buffered headers for single-fsync commits (like LMDB).
  **   two header slots alternate; the one with higher valid seq_d is current.
  */
    /* u3_book_head: on-disk file header (32 bytes, page-aligned slots)
    **
    **   fir_d is write-once (set on first event save).
    **   las_d is updated after each batch of events is committed.
    **   seq_d is monotonically increasing; determines which slot is current.
    **   crc_w is CRC32 of preceding fields to detect partial writes.
    **
    **   two header slots at offsets 0 and 4096; deeds start at 8192.
    */
      typedef struct _u3_book_head {
        c3_w mag_w;      //  magic number: 0x424f4f4b ("BOOK")
        c3_w ver_w;      //  format version: 1
        c3_d fir_d;      //  first event number in file
        c3_d las_d;      //  last event number (commit marker)
        c3_d seq_d;      //  sequence number (for double-buffer)
        c3_w crc_w;      //  CRC32 checksum (of preceding fields)
      } u3_book_head;

    /* u3_book_meta: on-disk metadata format (fixed 256 bytes)
    **
    **   layout:
    **     [4 bytes] version
    **     [16 bytes] who_d (c3_d[2], identity)
    **     [1 byte] fak_o (fake security bit)
    **     [4 bytes] lif_w (lifecycle length)
    **     [231 bytes] reserved for future use
    **
    **   total: 256 bytes
    */
      typedef struct _u3_book_meta {
        c3_d who_d[2];   //  ship identity (16 bytes)
        c3_w ver_w;      //  metadata format version
        c3_w lif_w;      //  lifecycle length (4 bytes)
        c3_o fak_o;      //  fake security flag (1 byte)
        c3_y pad_y[231]; //  reserved (231 bytes)
      } u3_book_meta;

    /* u3_book: event log handle
    */
      typedef struct _u3_book {
        c3_i         fid_i;      //  file descriptor for book.log
        c3_i         met_i;      //  file descriptor for meta.bin
        c3_c*        pax_c;      //  file path to book.log
        u3_book_head hed_u;      //  cached header (current valid state)
        c3_d         las_d;      //  cached last event number
        c3_d         off_d;      //  cached append offset (end of last event)
        c3_w         act_w;      //  active header slot a or b (0 or 1)
      } u3_book;

    /* u3_book_walk: event iterator
    */
      typedef struct _u3_book_walk {
        c3_i fid_i;    //  file descriptor
        c3_d nex_d;    //  next event number to read
        c3_d las_d;    //  last event number, inclusive
        c3_d off_d;    //  current file offset
        c3_o liv_o;    //  iterator valid
      } u3_book_walk;

    /* u3_book_deed: on-disk event record
    **
    **   on-disk format: len_d | buffer_data | let_d
    **   where buffer_data is len_d bytes of opaque buffer data
    **   and let_d echoes len_d for validation (used for backward scanning)
    **
    **   NB: not used directly for I/O due to variable-length buffer data
    */
      typedef struct _u3_book_deed {
        c3_d len_d;    //  buffer size (bytes)
        // c3_y buf_y[];  //  variable-length buffer data
        c3_d let_d;    //  length trailer (echoes len_d, used for backward scanning)
      } u3_book_deed;

    /* u3_book_reed: in-memory event record representation for I/O
    **
    **   represents a complete event buffer including any prefixes.
    **   the book API treats buffers as opaque byte arrays.
    */
      typedef struct _u3_book_reed {
        c3_d  len_d;    //  total buffer size (bytes)
        c3_y* buf_y;    //  complete buffer (caller owns)
      } u3_book_reed;

    /* u3_book_init(): open/create event log at [pax_c].
    */
      u3_book*
      u3_book_init(const c3_c* pax_c);

    /* u3_book_exit(): close event log.
    */
      void
      u3_book_exit(u3_book* txt_u);
    
    /* u3_book_stat(): print book stats.
    */
      void
      u3_book_stat(const c3_c* pax_c);

    /* u3_book_gulf(): read first and last event numbers.
    */
      c3_o
      u3_book_gulf(u3_book* txt_u, c3_d* low_d, c3_d* hig_d);

    /* u3_book_read(): read [len_d] events starting at [eve_d].
    */
      c3_o
      u3_book_read(u3_book* txt_u,
                   void*    ptr_v,
                   c3_d     eve_d,
                   c3_d     len_d,
                   c3_o  (*read_f)(void*, c3_d, c3_z, void*));

    /* u3_book_save(): save [len_d] events starting at [eve_d].
    */
      c3_o
      u3_book_save(u3_book* txt_u,
                   c3_d     eve_d,
                   c3_d     len_d,
                   void**   byt_p,
                   c3_z*    siz_i,
                   c3_d     epo_d);

    /* u3_book_read_meta(): read fixed metadata section.
    */
      void
      u3_book_read_meta(u3_book*    txt_u,
                        void*       ptr_v,
                        const c3_c* key_c,
                        void     (*read_f)(void*, c3_zs, void*));

    /* u3_book_save_meta(): write fixed metadata section.
    */
      c3_o
      u3_book_save_meta(u3_book*    txt_u,
                        const c3_c* key_c,
                        c3_z        val_z,
                        void*       val_p);

    /* u3_book_walk_init(): initialize event iterator.
    */
      c3_o
      u3_book_walk_init(u3_book*      txt_u,
                        u3_book_walk* itr_u,
                        c3_d          nex_d,
                        c3_d          las_d);

    /* u3_book_walk_next(): read next event from iterator.
    */
      c3_o
      u3_book_walk_next(u3_book_walk* itr_u, c3_z* len_z, void** buf_v);

    /* u3_book_walk_done(): close iterator.
    */
      void
      u3_book_walk_done(u3_book_walk* itr_u);

#endif /* ifndef U3_VERE_DB_BOOK_H */
