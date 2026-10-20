/// @file
///   Streaming jam-then-SHA-256: computes shax(jam(a)) without
///   materializing the intermediate jam output.  Feeds jam bits
///   directly into an incremental SHA-256 context.

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <stdint.h>
#include <sys/types.h>

typedef struct libscrypt_SHA256Context {
  uint32_t state[8];
  uint32_t count[2];
  unsigned char buf[64];
} _js_SHA256_CTX;

extern void libscrypt_SHA256_Init(_js_SHA256_CTX *);
extern void libscrypt_SHA256_Update(_js_SHA256_CTX *, const void *, size_t);
extern void libscrypt_SHA256_Final(unsigned char [], _js_SHA256_CTX *);

//  ---- streaming bit-writer → SHA-256 ---------------------------------

#define _JS_BUFSZ 8192

typedef struct {
  _js_SHA256_CTX sha_u;
  c3_y  buf_y[_JS_BUFSZ];
  c3_w  buf_w;
  c3_y  par_y;     //  partial byte accumulator
  c3_g  off_g;     //  bits used in par_y (0–7)
  c3_d  bit_d;     //  total bits written
  u3p(u3h_root) har_p;  //  dedup HAMT: noun → first-occurrence bit pos
} _jam_shax;

static void
_js_init(_jam_shax* ctx)
{
  libscrypt_SHA256_Init(&ctx->sha_u);
  ctx->buf_w = 0;
  ctx->par_y = 0;
  ctx->off_g = 0;
  ctx->bit_d = 0;
  ctx->har_p = u3h_new();
}

static inline void
_js_push_byte(_jam_shax* ctx, c3_y byt_y)
{
  ctx->buf_y[ctx->buf_w++] = byt_y;
  if ( _JS_BUFSZ == ctx->buf_w ) {
    libscrypt_SHA256_Update(&ctx->sha_u, ctx->buf_y, _JS_BUFSZ);
    ctx->buf_w = 0;
  }
}

//  write [wid_g] bits of [val_d] (LSB-first) into the stream.
//  wid_g must be <= 64.
//
static void
_js_bits(_jam_shax* ctx, c3_d val_d, c3_g wid_g)
{
  for ( c3_g i_g = 0; i_g < wid_g; i_g++ ) {
    if ( val_d & ((c3_d)1 << i_g) ) {
      ctx->par_y |= ((c3_y)1 << ctx->off_g);
    }
    ctx->off_g++;
    ctx->bit_d++;
    if ( 8 == ctx->off_g ) {
      _js_push_byte(ctx, ctx->par_y);
      ctx->par_y = 0;
      ctx->off_g = 0;
    }
  }
}

//  write [len_d] source bytes, bit-shifted by ctx->off_g, into the stream.
//
static void
_js_bytes(_jam_shax* ctx, const c3_y* src_y, c3_d len_d)
{
  c3_g off_g = ctx->off_g;

  if ( 0 == off_g ) {
    //  fast path: byte-aligned
    //
    for ( c3_d i_d = 0; i_d < len_d; i_d++ ) {
      _js_push_byte(ctx, src_y[i_d]);
    }
    ctx->bit_d += len_d * 8;
  }
  else {
    //  slow path: bit-shifted
    //
    c3_g rsh_g = 8 - off_g;
    for ( c3_d i_d = 0; i_d < len_d; i_d++ ) {
      c3_y src = src_y[i_d];
      _js_push_byte(ctx, ctx->par_y | (src << off_g));
      ctx->par_y = src >> rsh_g;
      ctx->bit_d += 8;
    }
  }
}

static void
_js_done(_jam_shax* ctx, c3_y out_y[32])
{
  //  flush partial byte (zero-padded high bits)
  //
  if ( ctx->off_g > 0 ) {
    _js_push_byte(ctx, ctx->par_y);
  }
  //  flush remaining buffer to SHA-256
  //
  if ( ctx->buf_w > 0 ) {
    libscrypt_SHA256_Update(&ctx->sha_u, ctx->buf_y, ctx->buf_w);
  }
  libscrypt_SHA256_Final(out_y, &ctx->sha_u);
  u3h_free(ctx->har_p);
}

//  ---- mat encoding ---------------------------------------------------

//  write mat's length prefix for an atom of [len_w] bits: zero bits as
//  many as len_w has, a one, then all but the top bit of len_w.
//  [len_w] zero is the single bit 1, which is all of (mat 0).
//
static void
_js_len(_jam_shax* ctx, c3_w len_w)
{
  if ( 0 == len_w ) {
    _js_bits(ctx, 1, 1);
    return;
  }
  c3_g b_g = (c3_g)c3_bits_word(len_w);

  _js_bits(ctx, (c3_d)1 << b_g, b_g + 1);

  if ( b_g > 1 ) {
    _js_bits(ctx, (c3_d)len_w, b_g - 1);
  }
}

//  write (mat pos), a backref's position: prefix, then the bits.
//
static void
_js_mat_d(_jam_shax* ctx, c3_d pos_d)
{
  c3_g bit_g = (c3_g)c3_bits_chub(pos_d);

  _js_len(ctx, bit_g);
  _js_bits(ctx, pos_d, bit_g);
}

//  ---- atom encoding --------------------------------------------------

//  write (mat a): the length prefix, then the atom's bits, streamed
//  from a view so a bob is never loaded.
//
static void
_js_mat_atom(_jam_shax* ctx, u3_atom a)
{
  if ( _(u3a_is_cat(a)) ) {
    c3_g bit_g = (c3_g)c3_bits_word(a);
    _js_len(ctx, bit_g);
    _js_bits(ctx, (c3_d)a, bit_g);
    return;
  }

  u3r_view vue_u;
  u3r_view_open(&vue_u, a, c3y);

  if ( vue_u.bit_d > c3_w_max ) {
    u3m_bail(c3__fail);
  }

  {
    const c3_y* vue_y = u3r_view_bytes(&vue_u);
    c3_w        bit_w = (c3_w)vue_u.bit_d;
    c3_w        ful_w = bit_w >> 3;
    c3_g        rem_g = bit_w & 7;

    _js_len(ctx, bit_w);
    _js_bytes(ctx, vue_y, ful_w);
    if ( rem_g ) {
      _js_bits(ctx, vue_y[ful_w], rem_g);
    }
  }
  u3r_view_done(&vue_u);
}

//  ---- noun encoding: u3s_jam_fib's rules, bit for bit ---------------

//  an atom: itself if new, or if it is no wider than the position of
//  its first occurrence; otherwise a backref (tag 11).
//
static void
_js_atom_cb(u3_atom a, void* ptr_v)
{
  _jam_shax* ctx = ptr_v;
  u3_weak    got = u3h_git(ctx->har_p, a);

  if ( u3_none == got ) {
    u3h_put(ctx->har_p, a, u3i_chub(ctx->bit_d));
    _js_bits(ctx, 0, 1);
    _js_mat_atom(ctx, a);
    return;
  }

  {
    c3_d pos_d = 0;
    u3r_safe_chub(got, &pos_d);

    if ( u3r_met_d(0, a) <= c3_bits_chub(pos_d) ) {
      _js_bits(ctx, 0, 1);
      _js_mat_atom(ctx, a);
    }
    else {
      _js_bits(ctx, 3, 2);
      _js_mat_d(ctx, pos_d);
    }
  }
}

//  a cell: a backref if seen (and its children are skipped), else tag
//  01 and the walk descends.
//
static c3_o
_js_cell_cb(u3_noun a, void* ptr_v)
{
  _jam_shax* ctx = ptr_v;
  u3_weak    got = u3h_git(ctx->har_p, a);

  if ( u3_none == got ) {
    u3h_put(ctx->har_p, a, u3i_chub(ctx->bit_d));
    _js_bits(ctx, 1, 2);
    return c3y;
  }

  {
    c3_d pos_d = 0;
    u3r_safe_chub(got, &pos_d);
    _js_bits(ctx, 3, 2);
    _js_mat_d(ctx, pos_d);
  }
  return c3n;
}

//  ---- public API ----------------------------------------------------

u3_atom
u3qe_jam_shax(u3_noun a)
{
  _jam_shax ctx;
  _js_init(&ctx);
  u3a_walk_fore(a, &ctx, _js_atom_cb, _js_cell_cb);

  c3_y hash_y[32];
  _js_done(&ctx, hash_y);
  return u3i_bytes(32, hash_y);
}

u3_noun
u3we_jam_shax(u3_noun cor)
{
  u3_noun a = u3x_at(u3x_sam, cor);
  return u3qe_jam_shax(a);
}
