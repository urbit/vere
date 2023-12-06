/*
* DEFLATE decompression
*/

#include "jets/q.h"
#include "jets/w.h"
#include "manage.h"
#include "retrieve.h"

#include "noun.h"

#include <zlib.h>
#include <time.h>

#define OUTBUF_SZ 4096

u3_noun u3qe_zlib_expand(u3_atom pos, u3_noun byts) {

  c3_d   pos_d;
  c3_d   wid_d;

  u3_atom wid;
  u3_atom dat;

  u3x_mean(byts, 2, &wid, 3, &dat, 0);

  size_t sad_i = u3r_met(3, dat);

  if (c3n == (u3r_safe_chub(pos, &pos_d))) {
    return u3_none;
  }
  if (c3n == (u3r_safe_chub(wid, &wid_d))) {
    return u3_none;
  }

  c3_y* byt_y;

  if ( c3y == u3a_is_cat(dat)) {
    byt_y = (c3_y*)&dat;
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(dat);
    byt_y = (c3_y*)vat_u->buf_w;
  }

  /* Leading zeros do not make sense 
    * for a Zlib stream 
    */
  u3_assert(wid_d == sad_i);

  z_stream zea;
  c3_w zas_w;

  zea.next_in = byt_y + pos_d;
  zea.avail_in = (wid_d - pos_d);

  zea.zalloc = Z_NULL;
  zea.zfree  = Z_NULL;
  zea.opaque = Z_NULL; 

  /* Allocate output buffer 
    */

  // Size of the output buffer
  //
  size_t sob_i = OUTBUF_SZ;
  c3_y* cuf_y = c3_malloc(sob_i);

  u3_assert(cuf_y != NULL);

  zea.next_out = cuf_y;
  zea.avail_out = sob_i;

  zas_w = inflateInit(&zea);

  if( Z_OK != zas_w) { 
    fprintf(stderr, "u3qe_zlib_expand: error while initializing Zlib, zas_w = %d\r\n", zas_w);

    c3_free(cuf_y);
    return u3_none;
  }

  while (Z_OK == (zas_w = inflate(&zea, Z_FINISH)) || zas_w == Z_BUF_ERROR) {

    if (zea.avail_in == 0){
      fprintf(stderr, "Exhausted input\r\n");
      break;
    }

    if (zea.avail_out == 0) {

      c3_y* new_y = c3_realloc(cuf_y, sob_i + OUTBUF_SZ);
      u3_assert(new_y != NULL);

      cuf_y = new_y;

      zea.avail_out = OUTBUF_SZ;

      u3_assert(sob_i == zea.total_out);

      zea.next_out = cuf_y + sob_i;
      
      sob_i += OUTBUF_SZ;
    }
  }

  if (zas_w != Z_STREAM_END) {

    fprintf(stderr, "u3qe_zlib_expand: %d input bytes\r\n", sad_i);
    fprintf(stderr, "u3qe_zlib_expand: processed %d bytes\r\n", zea.total_in);
    fprintf(stderr, "u3qe_zlib_expand: uncompressed %d bytes\r\n", zea.total_out);
    fprintf(stderr, "u3qe_zlib_expand: error while expanding, zas_w = %d, msg = %s\r\n", zas_w, zea.msg);

    // Dump ZLIB stream 
    //
    time_t now;
    time(&now);
    char filename[256];
    if ( zas_w != Z_STREAM_END ) {
      sprintf(filename, "error-stream-%d.dat", now);
    }
    else {
      sprintf(filename, "good-stream-%d.dat", now);
    }

    fprintf(stderr, "u3qe_zlib_expand: dumping corrupted stream to %s\r\n", filename);
    FILE* file= fopen(filename, "w");
    assert(file != NULL);

    // byt_y + pos_d, for zea.total_in bytes
    //
    fwrite(byt_y+pos_d, sizeof(char), zea.total_in, file);

    fclose(file);

    if ( zas_w != Z_STREAM_END ) {
      c3_free(cuf_y);
      return u3_none;
    }

  }

  size_t len_i = sob_i - zea.avail_out;
  c3_y *buf_y = c3_malloc(len_i);

  if (buf_y == NULL) {
    c3_free(cuf_y);
    u3m_bail(c3__meme);
  }

  memcpy(buf_y, cuf_y, len_i);

  u3_atom len_a = u3i_chub(len_i);

  pos_d += zea.total_in;

  zas_w = inflateEnd(&zea);

  if (zas_w != Z_OK) {
    fprintf(stderr, "u3qe_zlib_expand: zlib stream inconsistent upon finish, zas_w = %d\r\n", 
            zas_w);

    c3_free(cuf_y);
    return u3_none;
  }

  c3_free(cuf_y);
  return u3nc(u3nc(len_a, u3i_bytes(len_i, buf_y)),
              u3nc(u3i_chub(pos_d), u3k(byts)));
}

u3_noun u3we_zlib_expand(u3_noun sea) {

  u3_atom pos;
  u3_noun byts;

  u3x_mean(sea, u3x_sam_2, &pos, u3x_sam_3, &byts, 0);

  return u3qe_zlib_expand(pos, byts);
}
