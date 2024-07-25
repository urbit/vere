/// @file

#include <allocate.h>
#include <stdio.h>
#include "zlib.h"

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static void*
zlib_malloc(voidpf opaque, uInt items, uInt size)
{
  size_t len = items * size;
  void* result = u3a_malloc(len);
  return result;
}

static void
zlib_free(voidpf opaque, voidpf address)
{
  u3a_free(address);
}

u3_noun
_decompress(u3_atom pos, u3_noun octs, int window_bits)
{
  u3_atom p_octs = u3h(octs);
  u3_atom q_octs = u3t(octs);

  c3_w p_octs_w, pos_w;

  if ( c3n == u3r_safe_word(p_octs, &p_octs_w) ) {
    return u3_none;
  }
  if (c3n == u3r_safe_word(pos, &pos_w)) {
    return u3_none;
  }

  c3_w len_w = u3r_met(3, q_octs);

  int leading_zeros = 0;

  if (p_octs_w > len_w) {
    leading_zeros = p_octs_w - len_w;
  }
  else {
    len_w = p_octs_w;
  }

  // Bytestream exhausted
  //
  if (pos_w >= len_w) {
    return u3_none;
  }

  c3_y* input;

  if (c3y == u3a_is_cat(q_octs)) {
    input = (c3_y*)&q_octs + pos_w;
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(q_octs);
    input = (c3_y*)vat_u->buf_w + pos_w;
  }

  int ret;
  z_stream strm;

  if (pos_w < len_w) {
    strm.avail_in = (len_w - pos_w);
  }
  else {
    strm.avail_in = 0;
  }

  strm.zalloc = zlib_malloc;
  strm.zfree = zlib_free;
  strm.opaque = Z_NULL;
  strm.next_in = input;

  ret = inflateInit2(&strm, window_bits);

  if (ret != Z_OK) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    return u3m_bail(c3__exit);
  }

  c3_w chunk_w = len_w / 10;
  u3i_slab sab_u;

#define INIT_SZ 16384
  strm.avail_out = INIT_SZ;
  u3i_slab_init(&sab_u, 3, INIT_SZ);
  strm.next_out = sab_u.buf_y;

  void* this_address = strm.next_out;

#define ZEROS_SZ 256
  c3_y zeros[ZEROS_SZ];

  if (leading_zeros) {
    memset(zeros, 0, ZEROS_SZ);
  }

  while ((ret = inflate(&strm, Z_FINISH)) == Z_BUF_ERROR) {

    // Output exhausted: reallocate
    //
    if (strm.avail_out == 0) {
      strm.avail_out = chunk_w;

      u3i_slab_grow(&sab_u, 3, strm.total_out + chunk_w);
      strm.next_out = sab_u.buf_y + strm.total_out;
    }

    // Input exhausted: input leading zeros?
    //
    if (strm.avail_in == 0) {

      if (leading_zeros) {
        // Position in the stream exceeded atom bytes,
        // but is still below stream length
        //
        if (strm.total_in + pos_w >= len_w
            && strm.total_in + pos_w < p_octs_w) {

          c3_w rem_w = p_octs_w - (strm.total_in + pos_w);
          strm.next_in = zeros;

          if (rem_w > ZEROS_SZ) {
            strm.avail_in = ZEROS_SZ;
          }
          else {
            strm.avail_in = rem_w;
          }
        }
        else {
          u3l_log("%i", ret);
          u3l_log("%s", strm.msg);
          inflateEnd(&strm);
          u3i_slab_free(&sab_u);
          return u3m_bail(c3__exit);
        }
      }
      else {
        u3l_log("%i", ret);
        u3l_log("%s", strm.msg);
        inflateEnd(&strm);
        u3i_slab_free(&sab_u);
        return u3m_bail(c3__exit);
      }
    }
  }
  if (ret != Z_STREAM_END) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    inflateEnd(&strm);
    u3i_slab_free(&sab_u);
    return u3m_bail(c3__exit);
  }
  ret = inflateEnd(&strm);

  if (ret != Z_OK) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    u3i_slab_free(&sab_u);
    return u3m_bail(c3__exit);
  }

  u3_noun decompressed_octs = u3nc(strm.total_out, u3i_slab_mint(&sab_u));
  u3_noun new_pos = pos_w + strm.total_in;
  u3_noun new_stream = u3nc(u3i_word(new_pos), u3k(octs));

  return u3nc(decompressed_octs, new_stream);
}

u3_noun
u3qe_decompress_gzip(u3_atom pos, u3_noun octs)
{
  return _decompress(pos, octs, 31);
}
u3_noun
u3qe_decompress_zlib(u3_atom pos, u3_noun octs)
{
  return _decompress(pos, octs, 15);
}

u3_noun
u3we_decompress_gzip(u3_noun cor)
{
  u3_atom pos;
  u3_noun octs;

  u3_noun a = u3r_at(u3x_sam, cor);
  u3x_cell(a, &pos, &octs);

  if(_(u3a_is_atom(pos)) && _(u3a_is_cell(octs))) {
    return u3qe_decompress_gzip(pos, octs);
  }

  else {
    return u3m_bail(c3__exit);
  }
}

u3_noun
u3we_decompress_zlib(u3_noun cor)
{
  u3_atom pos;
  u3_noun octs;

  u3_noun a = u3r_at(u3x_sam, cor);
  u3x_cell(a, &pos, &octs);

  if(_(u3a_is_atom(pos)) && _(u3a_is_cell(octs))) {
    return u3qe_decompress_zlib(pos, octs);
  }

  else {
    return u3m_bail(c3__exit);
  }
}
