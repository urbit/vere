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

//  write mat-encoded value [val_w] (the bit-length of an atom).
//
static void
_js_mat_w(_jam_shax* ctx, c3_w val_w)
{
  if ( 0 == val_w ) {
    _js_bits(ctx, 1, 1);
    return;
  }
  c3_g b_g = (c3_g)c3_bits_word(val_w);
  //  b_g zero bits + one 1 bit
  //
  _js_bits(ctx, (c3_d)1 << b_g, b_g + 1);
  //  low (b_g - 1) bits of val_w
  //
  if ( b_g > 1 ) {
    _js_bits(ctx, (c3_d)val_w, b_g - 1);
  }
}

//  ---- atom encoding --------------------------------------------------

//  compute bit-length of an atom from mmap'd bytes (for bob atoms).
//
static c3_w
_js_bob_met(const c3_y* byt_y, c3_d len_d)
{
  c3_d pos_d = len_d;
  while ( pos_d > 0 && 0 == byt_y[pos_d - 1] ) {
    pos_d--;
  }
  if ( 0 == pos_d ) return 0;
  c3_y top_y = byt_y[pos_d - 1];
  c3_y clz_y = (c3_y)(__builtin_clz((unsigned int)top_y) - 24);
  return (c3_w)((pos_d - 1) * 8 + (8 - clz_y));
}

//  encode a single atom: tag 0 + mat(bit_len) + data bits.
//
static void
_js_encode_atom(_jam_shax* ctx, u3_atom a)
{
  //  atom tag
  //
  _js_bits(ctx, 0, 1);

  if ( 0 == a ) {
    _js_mat_w(ctx, 0);
    return;
  }

  u3r_view vue_u = {0};
  c3_w     bit_w;

  if ( _(u3a_is_cat(a)) ) {
    bit_w = (c3_g)c3_bits_word(a);
  }
  else if ( c3y == u3a_is_bob(a) ) {
    u3r_view_init(&vue_u, a);
    bit_w = _js_bob_met(vue_u.byt_y, vue_u.len_w);
    if ( 0 == bit_w ) {
      u3r_view_done(&vue_u);
      _js_mat_w(ctx, 0);
      return;
    }
  }
  else {
    bit_w = u3r_met(0, a);
    u3r_view_init(&vue_u, a);
  }

  //  mat header: encodes the bit-length
  //
  _js_mat_w(ctx, bit_w);

  //  atom data bits
  //
  if ( _(u3a_is_cat(a)) ) {
    _js_bits(ctx, (c3_d)a, (c3_g)bit_w);
  }
  else {
    c3_w full_w = bit_w >> 3;
    c3_g rem_g  = bit_w & 7;
    _js_bytes(ctx, vue_u.byt_y, full_w);
    if ( rem_g > 0 ) {
      _js_bits(ctx, vue_u.byt_y[full_w], rem_g);
    }
    u3r_view_done(&vue_u);
  }
}

//  ---- noun encoding (with dedup) ------------------------------------

static void _js_encode(_jam_shax* ctx, u3_noun a);

static void
_js_encode(_jam_shax* ctx, u3_noun a)
{
  u3_weak got = u3h_git(ctx->har_p, a);

  if ( _(u3a_is_atom(a)) ) {
    if ( u3_none == got ) {
      //  first occurrence: record position, encode atom
      //
      u3h_put(ctx->har_p, a, u3i_chub(ctx->bit_d));
      _js_encode_atom(ctx, a);
    }
    else {
      //  seen before: compare atom encoding cost vs backref cost
      //
      c3_w a_w = _(u3a_is_cat(a)) ? c3_bits_word(a) : u3r_met(0, a);
      c3_w a_cost = 1 + (0 == a ? 1 : 2 * c3_bits_word(a_w) + a_w);

      c3_d  pos_d = 0;
      u3r_safe_chub(got, &pos_d);
      c3_w  p_w = (0 == pos_d) ? 0 : c3_bits_word((c3_w)pos_d);
      c3_w  b_cost = 2 + (0 == pos_d ? 1 : 2 * p_w);

      if ( a_cost <= b_cost ) {
        _js_encode_atom(ctx, a);
      }
      else {
        _js_bits(ctx, 3, 2);  //  backref tag 11
        _js_mat_w(ctx, (c3_w)pos_d);
      }
    }
  }
  else {
    if ( u3_none != got ) {
      //  cell seen before: always use backref
      //
      c3_d pos_d = 0;
      u3r_safe_chub(got, &pos_d);
      _js_bits(ctx, 3, 2);
      _js_mat_w(ctx, (c3_w)pos_d);
    }
    else {
      //  first occurrence: record position, encode cell + recurse
      //
      u3h_put(ctx->har_p, a, u3i_chub(ctx->bit_d));
      _js_bits(ctx, 1, 2);  //  cell tag 01
      _js_encode(ctx, u3h(a));
      _js_encode(ctx, u3t(a));
    }
  }
}

//  ---- public API ----------------------------------------------------

u3_atom
u3qe_jam_shax(u3_noun a)
{
  _jam_shax ctx;
  _js_init(&ctx);
  _js_encode(&ctx, a);

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
