/*
* DEFLATE decompression
*/

#include "jets/q.h"
#include "jets/w.h"
#include "manage.h"
#include "retrieve.h"

#include "noun.h"

#include <zlib.h>

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

  c3_y* buf_y = byt_y;

  /* Leading zeros do not make sense 
    * for a Zlib stream 
    */
  u3_assert(wid_d == sad_i);

  z_stream zea;
  c3_w zas_w;

  zea.next_in = buf_y + pos_d;
  zea.avail_in = (sad_i - pos_d);
  zea.zalloc = Z_NULL;
  zea.zfree = Z_NULL;

  /* Allocate output buffer 
    */
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

  while (Z_OK == (zas_w = inflate(&zea, Z_FINISH))) {

    if (zea.avail_in == 0) break;

    if (zea.avail_out == 0) {

      c3_y* new_y = c3_realloc(cuf_y, sob_i + OUTBUF_SZ);
      u3_assert(new_y != NULL);

      cuf_y = new_y;

      zea.avail_out = OUTBUF_SZ;
      zea.next_out = cuf_y + sob_i;
      
      sob_i += OUTBUF_SZ;
    }
  }

  if (zas_w != Z_STREAM_END) {
    fprintf(stderr, "u3qe_zlib_expand: error while expanding, zas_w = %d\r\n", zas_w);

    c3_free(cuf_y);
    return u3_none;
  }



  u3i_slab sab_u;
  size_t len_i = sob_i - zea.avail_out;
  u3i_slab_bare(&sab_u, 3, len_i);
  // XX find out why this is important - otherwise we get memory leaks
  // Are slabs supposed to be zero terminated at their length?
  //
  sab_u.buf_w[sab_u.len_w - 1] = 0;
  memmove(sab_u.buf_y, cuf_y, len_i);

  u3_atom len_a = u3i_chub(len_i);

  pos_d += zea.total_in;

  zas_w = inflateEnd(&zea);

  if (zas_w != Z_OK) {
    fprintf(stderr, "u3qe_zlib_expand: zlib stream inconsistent upon finish, zas_w = %d\r\n", 
            zas_w);
  }

  c3_free(cuf_y);
  // u3k is needed for byts, since it becomes a
  // part of a new cell
  return u3nc(u3nc(len_a, u3i_slab_moot_bytes(&sab_u)),
              u3nc(u3i_chub(pos_d), u3k(byts)));
}

u3_noun u3we_zlib_expand(u3_noun sea) {

  u3_atom pos;
  u3_noun byts;

  u3x_mean(sea, u3x_sam_2, &pos, u3x_sam_3, &byts, 0);

  return u3qe_zlib_expand(pos, byts);
}
