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
        c3_w len_w;      //  length of metadata section
        c3_y pad_y[32];  //  reserved for future use, zeroed
      } u3_book_head;

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

    /* u3_book_init(): open/create event log at [pax_c].
    */
      u3_book*
      u3_book_init(const c3_c* pax_c);

    /* u3_book_exit(): close event log.
    */
      void
      u3_book_exit(u3_book* log_u);
    
    /* u3_book_stat(): print book stats.
    */
      void
      u3_book_stat(const c3_c* pax_c);

    /* u3_book_gulf(): read first and last event numbers.
    */
      c3_o
      u3_book_gulf(u3_book* log_u, c3_d* low_d, c3_d* hig_d);

    /* u3_book_read(): read [len_d] events starting at [eve_d].
    */
      c3_o
      u3_book_read(u3_book* log_u,
                   void*    ptr_v,
                   c3_d     eve_d,
                   c3_d     len_d,
                   c3_o  (*read_f)(void*, c3_d, c3_z, void*));

    /* u3_book_save(): save [len_d] events starting at [eve_d].
    */
      c3_o
      u3_book_save(u3_book* log_u,
                   c3_d     eve_d,
                   c3_d     len_d,
                   void**   byt_p,
                   c3_z*    siz_i,
                   c3_d     epo_d);

    /* u3_book_read_meta(): read metadata by string key from log.
    */
      void
      u3_book_read_meta(u3_book*    log_u,
                        void*       ptr_v,
                        const c3_c* key_c,
                        void     (*read_f)(void*, c3_zs, void*));

    /* u3_book_save_meta(): save metadata by string key into log.
    */
      c3_o
      u3_book_save_meta(u3_book*    log_u,
                        const c3_c* key_c,
                        c3_z        val_z,
                        void*       val_p);

    /* u3_book_walk_init(): initialize event iterator.
    */
      c3_o
      u3_book_walk_init(u3_book*      log_u,
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
