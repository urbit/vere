/// @file

#ifndef U3_VERE_DB_BOOK_H
#define U3_VERE_DB_BOOK_H

#include "c3/c3.h"

  /* book: append-only event log
  */
    /* u3_book_head: on-disk file header (64 bytes)
    */
      typedef struct _u3_book_head {
        c3_w mag_w;      //  magic number: 0x424f4f4b ("BOOK")
        c3_w ver_w;      //  format version: 1
        c3_d fir_d;      //  first event number in file
        c3_d las_d;      //  last event number in file
        c3_w off_w;      //  offset to metadata section
        c3_w len_w;      //  length of metadata section (reserved, currently unused)
        c3_y pad_y[32];  //  reserved for future use, zeroed
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
        c3_i         fid_i;      //  file descriptor
        c3_c*        pax_c;      //  file path
        u3_book_head hed_u;      //  cached header
        c3_w         off_w;      //  append offset (end of last event)
        c3_o         dit_o;      //  header needs sync
      } u3_book;

    /* u3_book_walk: event iterator
    */
      typedef struct _u3_book_walk {
        c3_i fid_i;    //  file descriptor
        c3_d nex_d;    //  next event number to read
        c3_d las_d;    //  last event number, inclusive
        c3_w off_w;    //  current file offset
        c3_o liv_o;    //  iterator valid
      } u3_book_walk;

    /* u3_book_deed_head: on-disk deed header
    */
      typedef struct _u3_book_deed_head {
        c3_d len_d;    //  payload size (mug + jam)
        c3_l mug_l;    //  mug/hash
      } u3_book_deed_head;

    /* u3_book_deed_tail: on-disk deed trailer
    */
      typedef struct _u3_book_deed_tail {
        c3_w crc_w;    //  CRC32 checksum
        c3_d let_d;    //  length trailer (validates len_d)
      } u3_book_deed_tail;

    /* u3_book_deed: complete on-disk event record
    **
    **   NB: not used directly for I/O due to variable-length jam data
    **   actual format: deed_head | jam_data | deed_tail
    */
      typedef struct _u3_book_deed {
        u3_book_deed_head hed_u;
        // c3_y jam_y[];  //  variable-length jam data
        u3_book_deed_tail tal_u;
      } u3_book_deed;

    /* u3_book_reed: in-memory event record representation for I/O
    */
      typedef struct _u3_book_reed {
        c3_d  len_d;    //  total payload size
        c3_l  mug_l;    //  mug/hash
        c3_y* jam_y;    //  jam data (caller owns, len = len_d - 4)
        c3_w  crc_w;    //  CRC32 checksum
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
