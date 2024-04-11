/// @file

#include <allocate.h>
#include <stdio.h>
#include "zlib.h"

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "uv.h"

static uv_buf_t
_dawn_oct_to_buf(u3_noun oct)
{
  if ( c3n == u3a_is_cat(u3h(oct)) ) {
    exit(1);
  }

  c3_w len_w  = u3h(oct);
  c3_y* buf_y = u3a_malloc(1 + len_w);
  buf_y[len_w] = 0;

  u3r_bytes(0, len_w, buf_y, u3t(oct));

  return uv_buf_init((void*)buf_y, len_w);
}

static u3_noun
_dawn_buf_to_oct(uv_buf_t buf_u)
{
  u3_noun len = u3i_words(1, (c3_w*)&buf_u.len);

  if ( c3n == u3a_is_cat(len) ) {
    exit(1);
  }

  return u3nc(len, u3i_bytes(buf_u.len, (const c3_y*)buf_u.base));
}

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
u3qe_unzip_gzip(u3_noun zipped_octs)
{
  uv_buf_t zipped_buf = _dawn_oct_to_buf(zipped_octs);

  int ret;
  z_stream strm;

  strm.zalloc = gzip_malloc;
  strm.zfree = gzip_free;
  strm.opaque = Z_NULL;
  strm.avail_in = zipped_buf.len;
  strm.next_in = zipped_buf.base;

  ret = inflateInit2(&strm, 16);
  if (ret < 0 || ret == 2) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    /* return u3m_bail(c3__exit); */
  }

  uint chunk = zipped_buf.len / 10;
  strm.avail_out = zipped_buf.len + 16384;
  strm.next_out = u3a_malloc(zipped_buf.len + 16384);

  void* this_address = strm.next_out;
  ret = inflate(&strm, Z_FINISH);
  if (ret < 0 || ret == 2) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    /* return u3m_bail(c3__exit); */
  }

  while (ret == -5) {
    strm.avail_out = chunk;

    /* u3l_log("%lu", (strm.total_out + chunk)); */
    this_address = u3a_realloc(this_address, (strm.total_out + chunk));
    strm.next_out = this_address + strm.total_out;

    ret = inflate(&strm, Z_FINISH);
  }
  if (ret < 0 || ret == 2) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    /* return u3m_bail(c3__exit); */
  }

  u3a_free(zipped_buf.base);
  uv_buf_t unzipped_buf;
  unzipped_buf.base = this_address;
  unzipped_buf.len = strm.total_out;
  u3_noun unzipped_octs = _dawn_buf_to_oct(unzipped_buf);
  u3a_free(unzipped_buf.base);
  ret = inflateEnd(&strm);
  if (ret < 0 || ret == 2) {
    u3l_log("%i", ret);
    u3l_log("%s", strm.msg);
    /* return u3m_bail(c3__exit); */
  }

  return unzipped_octs;
}

u3_noun
u3we_unzip_gzip(u3_noun cor)
{
  /* u3l_log("hejsan C"); */
  u3_noun a = u3r_at(u3x_sam, cor);
  /* u3m_p('hejsan', a); */
  /* u3m_p("yo ", u3r_at(u3x_sam, cor)); */

  /* if ( (u3_none == (a = u3r_at(u3x_sam, cor))) || */
  /*      (c3n == u3ud(a)) ) */
  if ( _(u3du(a)) ) {
    return u3qe_unzip_gzip(a);
  }
  else {
    return u3m_bail(c3__exit);
  }
}

