/// @file

#include <allocate.h>
#include <stdio.h>
#include "zlib.h"

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static void*
gzip_malloc(voidpf opaque, uInt items, uInt size)
{
  size_t len = items * size;
  void* result = u3a_malloc(len);
  return result;
}

static void
gzip_free(voidpf opaque, voidpf address)
{
  u3a_free(address);
}

u3_noun
u3qe_gunzip(u3_noun zipped_octs)
{
  u3_atom head = u3h(zipped_octs);
  u3_atom tail = u3t(zipped_octs);
  c3_w tel_w = u3r_met(3, tail);
  c3_w hed_w;
  if ( c3n == u3r_safe_word(head, &hed_w) ) {
    return u3m_bail(c3__fail);
  }
  c3_c* input;

  if (c3y == u3a_is_cat(tail)) {
    input = &tail;
  }
  else {
    u3a_atom* vat_u = u3a_to_ptr(tail);
    input = (c3_y*)vat_u->buf_w;
  }

  if ( tel_w > hed_w ) {
    return u3m_error("subtract-underflow");
  }

  int ret;
  z_stream strm;

  int leading_zeros = hed_w - tel_w;

  if (leading_zeros > 0) {
    strm.avail_in = hed_w - leading_zeros;
  }
  else {
    strm.avail_in = hed_w;
  }

  strm.zalloc = gzip_malloc;
  strm.zfree = gzip_free;
  strm.opaque = Z_NULL;
  strm.next_in = input;

  ret = inflateInit2(&strm, 16);
  if (ret != 0) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    return u3m_bail(c3__exit);
  }

  uint chunk = hed_w / 10;
  strm.avail_out = 16384;
  strm.next_out = u3a_malloc(16384);

  void* this_address = strm.next_out;
  ret = inflate(&strm, Z_FINISH);

  if (strm.total_in + leading_zeros == hed_w) {
    int zero = 0;
    strm.next_in = &zero;
    for (int i=0; i<leading_zeros; i++) {
      strm.avail_in = 1;
      ret = inflate(&strm, Z_FINISH);
    }
  }

  if (((ret > -5 && ret < 0) || ret == 2) || (strm.avail_in == 0 && ret == Z_BUF_ERROR)) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    return u3m_bail(c3__exit);
  }

  while (strm.avail_in > 0) {
    strm.avail_out = chunk;

    this_address = u3a_realloc(this_address, (strm.total_out + chunk));
    strm.next_out = this_address + strm.total_out;

    ret = inflate(&strm, Z_FINISH);

    if ((ret > -5 && ret < 0) || ret == 2) {
      u3l_log("%i", ret);
      u3l_log("%s", strm.msg);
      return u3m_bail(c3__exit);
    }
  }

  if (strm.total_in + leading_zeros == hed_w) {
    int zero = 0;
    strm.next_in = &zero;
    for (int i=0; i<leading_zeros; i++) {
      strm.avail_in = 1;
      ret = inflate(&strm, Z_FINISH);
    }
  }

  if (ret < 0 || ret == 2) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    return u3m_bail(c3__exit);
  }

  u3_noun unzipped_octs = u3nc(strm.total_out, u3i_bytes(strm.total_out, this_address));
  ret = inflateEnd(&strm);

  if (ret < 0 || ret == 2) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    return u3m_bail(c3__exit);
  }

  return unzipped_octs;
}

u3_noun
u3we_gunzip(u3_noun cor)
{
  u3_noun a = u3r_at(u3x_sam, cor);

  if ( _(u3du(a)) ) {
    return u3qe_gunzip(a);
  }
  else {
    return u3m_bail(c3__exit);
  }
}
