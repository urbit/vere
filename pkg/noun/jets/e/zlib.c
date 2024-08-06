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

u3_noun u3qe_zlib_expand(u3_atom pos, u3_atom oft, u3_noun octs) {

  c3_d   pos_d;
  c3_d   oft_d;
  c3_d   wid_d;

  u3_atom wid;
  u3_atom dat;

  u3x_cell(octs, &wid, &dat);

  size_t sad_i = u3r_met(3, dat);

  if (c3n == (u3r_safe_chub(pos, &pos_d))) {
    return u3_none;
  }
  if (c3n == (u3r_safe_chub(oft, &oft_d))) {
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

  z_stream zea;
  c3_w zas_w;

  zea.next_in = byt_y + oft_d + pos_d;
  zea.avail_in = (wid_d - oft_d - pos_d);

  zea.zalloc = Z_NULL;
  zea.zfree  = Z_NULL;
  zea.opaque = Z_NULL; 


  /* Allocate output buffer 
    */

  // Size of the output buffer
  //
  size_t sob_i = OUTBUF_SZ;
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 3, sob_i);
  c3_y* cuf_y = sab_u.buf_y;

  u3_assert(cuf_y != NULL);

  zea.next_out = cuf_y;
  zea.avail_out = sob_i;

  zas_w = inflateInit2(&zea, 47);

  if( Z_OK != zas_w) { 
    fprintf(stderr, "u3qe_zlib_expand: error while initializing Zlib, zas_w = %d\r\n", zas_w);

    u3i_slab_free(&sab_u);
    return u3_none;
  }

  while (Z_OK == (zas_w = inflate(&zea, Z_FINISH)) || zas_w == Z_BUF_ERROR) {

    if (zea.avail_in == 0){
      fprintf(stderr, "Exhausted input\r\n");
      break;
    }

    if (zea.avail_out == 0) {
      u3i_slab_grow(&sab_u, 3, sob_i + OUTBUF_SZ);
      cuf_y = sab_u.buf_y + sob_i;

      zea.avail_out = OUTBUF_SZ;
      zea.next_out = cuf_y;
      u3_assert(sob_i == zea.total_out);
      sob_i += OUTBUF_SZ;
    }
  }


  size_t len_i = sob_i - zea.avail_out;
  pos_d += zea.total_in;

  zas_w = inflateEnd(&zea);

  if (zas_w != Z_OK) {
    fprintf(stderr, "u3qe_zlib_expand: zlib stream inconsistent upon finish, zas_w = %d\r\n", 
            zas_w);

    u3i_slab_free(&sab_u);
    return u3_none;
  }

  u3_atom len_a = u3i_chub(len_i);
  u3_atom buf_a = u3i_slab_mint(&sab_u);

  return u3nc(u3nc(len_a, buf_a),
              u3nt(u3i_chub(pos_d), u3k(oft), u3k(octs)));
}

u3_noun u3we_zlib_expand(u3_noun cor) {

  u3_atom pos;
  u3_atom oft;
  u3_noun octs;

  u3x_mean(cor, u3x_sam_2, &pos, 
                u3x_sam_6, &oft, 
                u3x_sam_7, &octs, 0);

  return u3qe_zlib_expand(pos, oft, octs);
}
