/// @file


#include "vere.h"
#include <stdio.h>
#include <types.h>
#include "urcrypt.h"
#include "blake.h"


#define pf(label, ...) { fprintf(stderr, "%s (%u)\r\n", label, __LINE__); fprintf(stderr, __VA_ARGS__); }
#define pl { fprintf(stderr, "(%u)", __LINE__); }
#define pll(label) { fprintf(stderr, "%s (%u)\n", label, __LINE__); }

enum blake3_flags {
  CHUNK_START         = 1 << 0,
  CHUNK_END           = 1 << 1,
  PARENT              = 1 << 2,
  ROOT                = 1 << 3,
  KEYED_HASH          = 1 << 4,
  DERIVE_KEY_CONTEXT  = 1 << 5,
  DERIVE_KEY_MATERIAL = 1 << 6,
};




static void
_log_buf(c3_c* str_c, c3_y* buf_y, c3_w len_w)
{
  c3_w siz_w = 2*len_w + 1;
  c3_c* res_c = c3_calloc(siz_w);
  c3_w cur_w = 0;
  c3_c tmp_c[3];
  for(c3_w idx_w = 0; idx_w < len_w; idx_w++ ) {
    snprintf(res_c + (2*idx_w), siz_w - (2*idx_w), "%02x", buf_y[idx_w]);
  }
  fprintf(stderr, "%s: %s", str_c, res_c);
  fprintf(stderr, "\r\n");
  c3_free(res_c);
}

static void
_log_words(c3_c* str_c, c3_w* buf_w, c3_w len_w)
{
  fprintf(stderr, "%s\r\n", str_c);
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    fprintf(stderr, "%u ", buf_w[i_w]);
  }
  fprintf(stderr, "\r\n");
}

void
log_node(blake_node* nod_u)
{
  _log_buf("CV", (c3_w*)nod_u->cev_y, BLAKE3_OUT_LEN);
  _log_buf("block", (c3_w*)nod_u->boq_y, BLAKE3_BLOCK_LEN);
  fprintf(stderr, "counter: %llu\r\n", nod_u->con_d);
  fprintf(stderr, "length: %u\r\n", nod_u->len_y);
  fprintf(stderr, "flag: %x\r\n", nod_u->fag_y);
}

static void print_proof(void* vod_p, c3_w i_w) {
  c3_y* pof_y = (c3_y*)vod_p;
  fprintf(stderr, "proof: %u\r\n", i_w);
  _log_buf("", pof_y, BLAKE3_OUT_LEN);
}




// static c3_w IV_WORDS[8] = {0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL, 0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL};
static c3_y IV[BLAKE3_OUT_LEN] = {103, 230, 9, 106, 133, 174, 103, 187, 114, 243, 110, 60, 58, 245, 79, 165, 127, 82, 14, 81, 140, 104, 5, 155, 171, 217, 131, 31, 25, 205, 224, 91};
#define ABS_DIFF(x,y) x > y ? x - y : y - x

#define VEC_SCALING 2


void
vec_init(u3_raw_vec* vec_u, c3_w siz_w)
{
  vec_u->siz_w = siz_w;
  vec_u->vod_p = c3_calloc(siz_w * sizeof(void*));
  vec_u->len_w = 0;
}
static void
_vec_each(u3_raw_vec* vec_u, void (*func)(void*, c3_w)) {
  for (c3_w i_w = 0; i_w < vec_u->len_w; i_w++) {
      func(vec_u->vod_p[i_w], i_w);
  }
}

void
vec_free(u3_raw_vec* vec_u)
{
  c3_free(vec_u->vod_p);
}

void
vec_flop(u3_raw_vec* vec_u)
{
  for (int i = 0; i < (vec_u->len_w/2); i++) {
    c3_w j_w = (vec_u->len_w - 1 -i );
    void* vod_p = vec_u->vod_p[i];
    vec_u->vod_p[i] = vec_u->vod_p[j_w];
    vec_u->vod_p[j_w] = vod_p;
  }
}

void
vec_drop(u3_raw_vec* vec_u)
{
  vec_free(vec_u);
  c3_free(vec_u);
}

u3_raw_vec*
vec_make(c3_w siz_w)
{
  u3_raw_vec* vec_u = c3_calloc(sizeof(u3_raw_vec));
  vec_init(vec_u, siz_w);
  return vec_u;
}


c3_w
vec_len(u3_raw_vec* vec_u)
{
  return vec_u->len_w;
}

static void*
_vec_raw_get(u3_raw_vec* vec_u, c3_w idx_w)
{
  if ( vec_u->len_w <= idx_w ) {
    return NULL;
  }
  return vec_u->vod_p[idx_w];
}

#define _vec_get(typ,vec,idx) ((typ*)_vec_raw_get(vec, idx))

static void*
_vec_raw_got(u3_raw_vec* vec_u, c3_w idx_w)
{
  return vec_u->vod_p[idx_w];
}

#define _vec_got(typ,vec,idx) ((typ*)_vec_raw_got(vec, idx))


static void
_vec_resize(u3_raw_vec* vec_u, c3_w siz_w) {
  u3_assert( vec_u->len_w <= siz_w );
  void* vod_p = c3_calloc(siz_w * sizeof(void*));

  memcpy(vod_p, vec_u->vod_p, vec_u->len_w * sizeof(void*));
  c3_free(vec_u->vod_p);
  vec_u->vod_p = vod_p;
  vec_u->siz_w = siz_w;
}

void
vec_append(u3_raw_vec* vec_u, void* tem_p)
{
  if(vec_u->siz_w == vec_u->len_w ) {
    _vec_resize(vec_u, vec_u->siz_w * VEC_SCALING);
  }
  vec_u->vod_p[vec_u->len_w] = tem_p;
  vec_u->len_w++;
}

void
vec_copy(u3_raw_vec* dst_u, u3_raw_vec* src_u) {
  if ( dst_u->siz_w < src_u->siz_w ) {
    _vec_resize(dst_u, src_u->siz_w);
  }
  for ( c3_w i_w = 0; i_w < src_u->len_w; i_w++ ) {
    dst_u->vod_p[i_w] = src_u->vod_p[i_w];
  }
  dst_u->len_w = src_u->len_w;
}

void*
vec_pop(u3_raw_vec* vec_u, c3_w idx_w)
{
  if ( vec_u->len_w ==  0 || idx_w >= vec_u->len_w ) {
    fprintf(stderr, "Failed pop (len: %u, idx: %u)\r\n", vec_u->len_w, idx_w);
    return NULL;
  }
  void* hit_p = vec_u->vod_p[idx_w];
  vec_u->len_w--;
  for ( c3_w i_w = idx_w; i_w < vec_u->len_w; i_w++ ) {
    vec_u->vod_p[i_w] = vec_u->vod_p[i_w+1];
  }
  return hit_p;
}

void*
vec_popf(u3_raw_vec* vec_u)
{
  return vec_pop(vec_u, 0);
}

static void*
_vec_popl(u3_raw_vec* vec_u)
{
  return vec_pop(vec_u, vec_u->len_w - 1);
}


static void
_vec_swap(u3_raw_vec* vec_u, c3_w fst_w, c3_w snd_w) {
  void* tmp_p = vec_u->vod_p[fst_w];
  vec_u->vod_p[fst_w] = vec_u->vod_p[snd_w];
  vec_u->vod_p[snd_w] = tmp_p;
}



static void _vec_weld_mut(u3_raw_vec* fst_u, u3_raw_vec* snd_u)
{
  if ( fst_u->siz_w <= (fst_u->len_w + snd_u->len_w ) ) {
    _vec_resize(fst_u, (fst_u->len_w + snd_u->len_w) * VEC_SCALING );
  }
  for ( c3_w i_w = 0; i_w < snd_u->len_w; i_w++ ) {
    fst_u->vod_p[i_w + fst_u->len_w] = snd_u->vod_p[i_w];
  }
  fst_u->len_w += snd_u->len_w;
}

//type Hash [32]byte

//func (h Hash) String() string { return hex.EncodeToString(h[:]) }

//type Pair [2][8]uint32



static void
_log_blake_subtree(blake_subtree* sub_u)
{
  fprintf(stderr, "sub: (%llu, %llu)\r\n", sub_u->sin_d, sub_u->dex_d);
}

void
log_bao(blake_bao* bao_u)
{
  fprintf(stderr, "num: %llu\r\n", bao_u->num_w);
  fprintf(stderr, "con: %llu\r\n", bao_u->con_w);
  fprintf(stderr, "sub: (%llu, %llu)\r\n", bao_u->sub_u.sin_d, bao_u->sub_u.dex_d);
  fprintf(stderr, "queue\r\n");
  _vec_each(&bao_u->que_u, print_proof);
  fprintf(stderr, "stack\r\n");
  _vec_each(&bao_u->sta_u, print_proof);
}

static c3_y
_count_lead_zeros(c3_d val_d)
{
  if (val_d == 0 ) {
    return 64;
  }
  c3_y ret_y = 0;
  if ((val_d & 0xFFFFFFFF00000000ULL) == 0) { ret_y += 32; val_d <<= 32; }
  if ((val_d & 0xFFFF000000000000ULL) == 0) { ret_y += 16; val_d <<= 16; }
  if ((val_d & 0xFF00000000000000ULL) == 0) { ret_y += 8; val_d <<= 8; }
  if ((val_d & 0xF000000000000000ULL) == 0) { ret_y += 4; val_d <<= 4; }
  if ((val_d & 0xC000000000000000ULL) == 0) { ret_y += 2; val_d <<= 2; }
  if ((val_d & 0x8000000000000000ULL) == 0) { ret_y += 1; val_d <<= 1; }
  return ret_y;
}

static c3_y _count_trail_zero(c3_d val_d)
{
  if (val_d == 0 ) {
    return 64;
  }
  c3_y ret_y = 0;
  if ((val_d & 0x00000000FFFFFFFFULL) == 0) { ret_y += 32; val_d >>= 32; }
  if ((val_d & 0x000000000000FFFFULL) == 0) { ret_y += 16; val_d >>= 16; }
  if ((val_d & 0x00000000000000FFULL) == 0) { ret_y += 8; val_d >>= 8; }
  if ((val_d & 0x000000000000000FULL) == 0) { ret_y += 4; val_d >>= 4; }
  if ((val_d & 0x0000000000000003ULL) == 0) { ret_y += 2; val_d >>= 2; }
  if ((val_d & 0x0000000000000001ULL) == 0) { ret_y += 1; val_d >>= 1; }
  return ret_y;
}

static c3_d _bitlen(c3_d bit_d)
{
  return 64 - _count_lead_zeros(bit_d);
}

static c3_d
_height(blake_subtree* sub_u)
{
  c3_d ret_d = _bitlen(ABS_DIFF(sub_u->dex_d, sub_u->sin_d)) - 1;
  return ret_d;
}

static c3_d
_largest_pow2(c3_d val_d)
{
	c3_d ret_d = 1 << (_bitlen(val_d-1) - 1);
  return ret_d;
}

static void
y_to_w(c3_y* byt_y, c3_w wod_w[16])
{
  for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
    c3_w cur_w = 0;
    for ( c3_w j_w = 0; j_w < 4; j_w++ ) {
      cur_w |= byt_y[(i_w * 4) + j_w] << (8*j_w);
    }
    wod_w[i_w] = cur_w;
  }
}

static void
w_to_y(c3_w wod_w[16], c3_y byt_y[64])
{
  for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
    for ( c3_w j_w = 0; j_w < 4; j_w++ ) {
      byt_y[(i_w * 4) + j_w] = (wod_w[i_w] >> (8*j_w)) & 0xff;
    }
  }
}


static void
_compress_node(c3_y out_y[BLAKE3_BLOCK_LEN], blake_node* nod_u)
{
  urcrypt_blake3_compress(nod_u->cev_y,
                          nod_u->boq_y,
                          nod_u->len_y,
                          nod_u->con_d,
                          nod_u->fag_y,
                          out_y);
}


void
make_chain_value(c3_y out_y[32], blake_node* nod_u)
{
  c3_y tmp_y[64];
  _compress_node(tmp_y, nod_u);
  memcpy(out_y, tmp_y, BLAKE3_OUT_LEN);
}

static blake_node*
_compress_chunk(c3_y* dat_y, c3_w dat_w, c3_y cev_y[BLAKE3_OUT_LEN], c3_d con_d, c3_w fag_y)
{
  blake_node* nod_u = c3_calloc(sizeof(blake_node));
  nod_u->con_d = con_d;
  nod_u->fag_y = fag_y;
  memcpy(nod_u->cev_y, cev_y, BLAKE3_OUT_LEN);
  urcrypt_blake3_chunk_output(dat_w, dat_y, nod_u->cev_y, nod_u->boq_y, &nod_u->len_y, &nod_u->con_d, &nod_u->fag_y);
  return nod_u;
}

blake_node* blake_leaf_hash(c3_y* dat_y, c3_w dat_w, c3_d con_d)
{
  return _compress_chunk(dat_y, dat_w, IV, con_d, 0);
}

static blake_node*
_parent_node(c3_y sin_y[BLAKE3_OUT_LEN], c3_y dex_y[BLAKE3_OUT_LEN], c3_y key_y[BLAKE3_OUT_LEN], c3_w fag_y)
{
  blake_node* nod_u = c3_calloc(sizeof(blake_node));
  nod_u->con_d = 0;
  memcpy(nod_u->cev_y, key_y, BLAKE3_OUT_LEN);
  nod_u->len_y = BLAKE3_BLOCK_LEN;
  nod_u->fag_y = fag_y | PARENT;
  memcpy(nod_u->boq_y, sin_y, BLAKE3_OUT_LEN);
  memcpy(nod_u->boq_y + BLAKE3_OUT_LEN, dex_y, BLAKE3_OUT_LEN);
  return nod_u;
}

static blake_node*
_parent_hash(blake_node* sin_u, blake_node* dex_u)
{
  c3_y sin_y[BLAKE3_OUT_LEN];
  c3_y dex_y[BLAKE3_OUT_LEN];
  make_chain_value(sin_y, sin_u);
  make_chain_value(dex_y, dex_u);
  return _parent_node(sin_y, dex_y, IV, 0);
}



static void
_root_hash(c3_y out_y[64], blake_node* nod_u)
{
  nod_u->fag_y |= ROOT;
  _compress_node(out_y, nod_u);
}

void print_pair(void* vod_p, c3_w i_w) {
  blake_pair* par_u = (blake_pair*)vod_p;
  fprintf(stderr, "pair: %u", i_w);
  _log_buf("sin: ", par_u->sin_y, BLAKE3_OUT_LEN);
  _log_buf("dex: ", par_u->dex_y, BLAKE3_OUT_LEN);
}

c3_w emp_w = 1024;
static c3_y emp_y[1024] = {0};

static void recurse_blake_subtree(blake_subtree* sub_u, blake_node* nod_u, u3_vec(pair)* par_u)
{
  c3_d hit_d = _height(sub_u);
  if ( hit_d == 0 ) {
    blake_node* new_u = blake_leaf_hash(emp_y, emp_w, sub_u->sin_d);
    memcpy(nod_u, new_u, sizeof(blake_node));
    return;
  }

  c3_d mid_d = sub_u->sin_d + _largest_pow2(ABS_DIFF(sub_u->dex_d, sub_u->sin_d));

  blake_subtree* sin_u = c3_calloc(sizeof(blake_subtree));
  *sin_u = (blake_subtree){sub_u->sin_d, mid_d};
  blake_node* lod_u = c3_calloc(sizeof(blake_node));
  u3_vec(pair)* lar_u = vec_make(8);
  recurse_blake_subtree(sin_u, lod_u, lar_u);

  blake_subtree* dex_u = c3_calloc(sizeof(blake_subtree));
  *dex_u = (blake_subtree){mid_d, sub_u->dex_d};
  blake_node* rod_u = c3_calloc(sizeof(blake_node));
  u3_vec(pair)* rar_u = vec_make(8);
  recurse_blake_subtree(dex_u, rod_u, rar_u);

  blake_pair* new_u = c3_calloc(sizeof(blake_pair));
  make_chain_value(new_u->sin_y, lod_u);
  make_chain_value(new_u->dex_y, rod_u);
  memcpy(nod_u, _parent_hash(lod_u, rod_u), sizeof(blake_node));

  vec_append(par_u, new_u);
  _vec_weld_mut(par_u, lar_u);
  _vec_weld_mut(par_u, rar_u);
}

static void
_bao_build(c3_d num_d, c3_y has_y[64], u3_vec(c3_w[8])* pof_u, u3_vec(pair)* par_u)
{
  blake_subtree* sub_u = c3_calloc(sizeof(blake_subtree));
  *sub_u = (blake_subtree){0, num_d};
  blake_node* nod_u = c3_calloc(sizeof(blake_node));
  recurse_blake_subtree(sub_u, nod_u, par_u);
  _root_hash(has_y, nod_u);

  if ( vec_len(par_u) != 0 ) {
    for ( c3_w i_w = _bitlen(num_d - 1); i_w > 1; i_w-- ) {
      blake_pair* pir_u = vec_popf(par_u);
      vec_append(pof_u, pir_u->dex_y);
    }
    blake_pair* pir_u = vec_popf(par_u);
    vec_append(pof_u, pir_u->dex_y);
    vec_append(pof_u, pir_u->sin_y);
    for ( c3_w i_w = 0; i_w < (vec_len(pof_u) >> 1); i_w++ ) {
      c3_w jay_w = vec_len(pof_u) - i_w - 1;
      _vec_swap(pof_u, i_w, jay_w);
    }
  }
}

static void
_push_leaf(blake_bao* bao_u, c3_y lef_y[BLAKE3_OUT_LEN])
{
  vec_append(&bao_u->que_u, lef_y);
}

static void
_pop_leaf(c3_y out_y[BLAKE3_OUT_LEN], blake_bao* bao_u)
{
  c3_y* res_y = vec_popf(&bao_u->que_u);
  memcpy(out_y, res_y, BLAKE3_OUT_LEN);
  c3_free(res_y);
}

static void
_push_parent(blake_bao* bao_u, c3_y par_y[BLAKE3_OUT_LEN])
{
  vec_append(&bao_u->sta_u, par_y);
}

static void
_pop_parent(c3_y out_y[BLAKE3_OUT_LEN], blake_bao* bao_u)
{
  c3_y* res_y = _vec_popl(&bao_u->sta_u);
  memcpy(out_y, res_y, BLAKE3_OUT_LEN);
  c3_free(res_y);
}

static void
_verifier_next(blake_bao* bao_u)
{
  if ( _height(&bao_u->sub_u) == 0 ) {
    bao_u->sub_u.sin_d += 1;
    bao_u->sub_u.dex_d += (1 << _count_trail_zero(bao_u->sub_u.dex_d));
    if ( bao_u->sub_u.dex_d > bao_u->num_w ) {
      bao_u->sub_u.dex_d = bao_u->num_w;
    }
  } else {
    bao_u->sub_u = (blake_subtree){bao_u->sub_u.sin_d, bao_u->sub_u.sin_d + _largest_pow2(ABS_DIFF(bao_u->sub_u.dex_d, bao_u->sub_u.sin_d))};
  }
}

static void
_push_leaves(blake_bao* bao_u, c3_y lef_y[BLAKE3_OUT_LEN], c3_y rig_y[BLAKE3_OUT_LEN])
{
  _push_leaf(bao_u, lef_y);
  _push_leaf(bao_u, rig_y);
  _verifier_next(bao_u);
  _verifier_next(bao_u);
}

static c3_o
_veri_check_leaf(blake_bao* bao_u, c3_y* dat_y, c3_w dat_w)
{
  if ( vec_len(&bao_u->que_u) > 0 ) {
    c3_y out_y[BLAKE3_OUT_LEN];
    _pop_leaf(out_y, bao_u);
    c3_y cav_y[BLAKE3_OUT_LEN];
    blake_node* nod_u = blake_leaf_hash(dat_y, dat_w, bao_u->con_w);
    make_chain_value(cav_y, nod_u);
    bao_u->con_w++;
    c3_free(nod_u);
    return 0 == memcmp(cav_y, out_y, BLAKE3_OUT_LEN) ? c3y : c3n;
  } else if ( vec_len(&bao_u->sta_u) > 0 ) {
    c3_y out_y[BLAKE3_OUT_LEN];
    _pop_parent(out_y, bao_u);
    c3_y cav_y[BLAKE3_OUT_LEN];
    blake_node* nod_u = blake_leaf_hash(dat_y, dat_w, bao_u->con_w);
    make_chain_value(cav_y, nod_u);
    c3_free(nod_u);
    bao_u->con_w++;
    return 0 == memcmp(cav_y, out_y, BLAKE3_OUT_LEN) ? c3y : c3n;
  }
  return c3n;
}

static c3_o
_veri_check_pair(blake_bao* bao_u, blake_pair* par_u)
{
  if ( vec_len(&bao_u->sta_u) == 0 ) {
    fprintf(stderr, "bailing empty stack\r\n");
    return c3n;
  }
  c3_y par_y[BLAKE3_OUT_LEN];
  // par_y is wrong
  _pop_parent(par_y, bao_u);
  blake_node* nod_u = _parent_node(par_u->sin_y, par_u->dex_y, IV, 0);
  c3_y cav_y[BLAKE3_OUT_LEN];
  make_chain_value(cav_y, nod_u);
  c3_free(nod_u);
  return _(memcmp(par_y, cav_y, BLAKE3_OUT_LEN) == 0);
}

static c3_o
_at_leaf(blake_subtree* sub_u)
{
  return (sub_u->sin_d+1 == sub_u->dex_d) ? c3y : c3n;
}


blake_bao*
blake_bao_make(c3_w num_w, u3_vec(c3_y[BLAKE3_OUT_LEN])* pof_u)
{
  blake_bao* bao_u = c3_calloc(sizeof(blake_bao));
  bao_u->num_w = num_w;
  bao_u->con_w = 0;
  bao_u->sub_u = (blake_subtree){0,1};
  if ( vec_len(pof_u) == 0 ) {
    if ( num_w < 2 ) {
      c3_free(bao_u);
      return NULL;
    }
    vec_init(&bao_u->sta_u, 1);
    vec_init(&bao_u->que_u, 1);
    return bao_u;
  }
  c3_y* fst_y = vec_popf(pof_u);
  c3_y* snd_y = vec_popf(pof_u);
  vec_init(&bao_u->sta_u, 8);
  vec_init(&bao_u->que_u, 8);
  vec_copy(&bao_u->sta_u, pof_u);
  vec_flop(&bao_u->sta_u);
  _push_leaves(bao_u, fst_y, snd_y);
  return bao_u;
}

static void
_free(void* vod_p, c3_w i_w)
{
  c3_free(vod_p);
}

void
blake_bao_free(blake_bao* bao_u)
{
  _vec_each(&bao_u->sta_u, _free);
  _vec_each(&bao_u->que_u, _free);
  c3_free(bao_u);
}

static void
_push_parents(blake_bao* bao_u, blake_pair* par_u)
{
  c3_y* fst_y = c3_calloc(BLAKE3_OUT_LEN);
  c3_y* snd_y = c3_calloc(BLAKE3_OUT_LEN);
  memcpy(fst_y, par_u->dex_y, BLAKE3_OUT_LEN);
  memcpy(snd_y, par_u->sin_y, BLAKE3_OUT_LEN);
  _push_parent(bao_u, fst_y);
  _push_parent(bao_u, snd_y);
}

c3_y
blake_bao_verify(blake_bao* bao_u, c3_y* dat_y, c3_w dat_w, blake_pair* par_u)
{
  if ( c3n == _veri_check_leaf(bao_u, dat_y, dat_w) ) {
    return BAO_FAILED;
  }
  if ( par_u == NULL ) {
    return BAO_GOOD;
  }
  if ( c3n == _veri_check_pair(bao_u, par_u) ) {
    return BAO_FAILED;
  }
  _verifier_next(bao_u);
  if ( _at_leaf(&bao_u->sub_u) == c3y ) {
    c3_y* fst_y = c3_calloc(BLAKE3_OUT_LEN);
    memcpy(fst_y, par_u->sin_y, BLAKE3_OUT_LEN);
    c3_y* snd_y = c3_calloc(BLAKE3_OUT_LEN);
    memcpy(snd_y, par_u->dex_y, BLAKE3_OUT_LEN);
    _push_leaves(bao_u, fst_y, snd_y);
  } else {
    _push_parents(bao_u, par_u);
  }
  if ( bao_u->con_w == bao_u->num_w ) {
    return BAO_DONE;
  }
  return BAO_GOOD;
}

#ifdef BLAKE_TEST


static void _test_lead_zeros()
{
  #define asrt(x,y) if ( x != y ) { fprintf(stderr, "failed (line: %u) got %llu expected %llu", __LINE__, x,y); exit(1); }
  // Test with 0, should be 64 leading zeros
  asrt(_count_lead_zeros(0ULL), 64);

  // Test with maximum unsigned 64-bit integer, should be 0 leading zeros
  asrt(_count_lead_zeros(0xFFFFFFFFFFFFFFFFULL), 0);

  // Test with 1 (which is 2^0), should be 63 leading zeros
  asrt(_count_lead_zeros(1ULL), 63);

  // Test with 2^63, should be 1 leading zero
  asrt(_count_lead_zeros(0x4000000000000000ULL), 1);

  // Test with a number in the middle, for example 2^31, should be 32 leading zeros
  asrt(_count_lead_zeros(0x0000000080000000ULL), 32);

  // Test with a small number, should still recognize leading zeros correctly
  asrt(_count_lead_zeros(0x00000000000000FFULL), 56);

  // Additional tests for edge cases
  asrt(_count_lead_zeros(0x2000000000000000ULL), 2);

  // Test with a value just below the halfway mark, should have 33 leading zeros
  asrt(_count_lead_zeros(0x000000007FFFFFFFULL), 33);

  asrt(_bitlen(7), 3);

  asrt(_count_trail_zero(3), 0);
  #undef asrt
}


static void _test_bao()
{
  c3_w dat_w = 1024;
  c3_y* dat_y = c3_calloc(dat_w);
  for ( c3_w num_w = 1; num_w <= 100; num_w++ ) {
    c3_y has_y[BLAKE3_BLOCK_LEN];
    u3_vec(c3_y[8])* pof_u = vec_make(10);
    u3_vec(blake_pair)* par_u = vec_make(10);
    _bao_build(num_w, has_y, pof_u, par_u);

    blake_bao bao_u;
    memset(&bao_u, 0, sizeof(blake_bao));
    _veri_init(&bao_u, num_w);
    if ( c3n == _verifier_init(&bao_u, has_y, pof_u) ) {
      fprintf(stderr, "Failed on %u\r\n", num_w);
      exit(1);
    }

    if ( num_w == 1 ) {
      c3_y out_y[BLAKE3_BLOCK_LEN];
      blake_node* nod_u = blake_leaf_hash(dat_y, dat_w, 0);
      _root_hash(out_y, nod_u);
      if ( memcmp(out_y, has_y, BLAKE3_BLOCK_LEN) != 0 ) {
        _log_buf("expected", has_y, BLAKE3_BLOCK_LEN);
        _log_buf("got", out_y, BLAKE3_BLOCK_LEN);

        fprintf(stderr, "1 special case\r\n");
      }
      continue;
    }
    for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
      if ( c3n == _veri_check_leaf(&bao_u, i_w, dat_y, dat_w) ) {
        fprintf(stderr, "Check leaf %u failed for %u\r\n", i_w, num_w);
        exit(1);
      }

      // pl
      // _log_verifier(&bao_u);
      if ( i_w < vec_len(par_u) ) {
        blake_pair* pir_u = _vec_get(blake_pair, par_u, i_w);
        if ( _veri_check_pair(&bao_u, pir_u) == c3n ) {
          fprintf(stderr, "check pair failed (%u, %u)", i_w, num_w);
          exit(1);
        }
        _verifier_next(&bao_u);
        if ( _height(&bao_u.sub_u) == 0 ) {
          _push_leaf(&bao_u, pir_u->sin_y);
          _push_leaf(&bao_u, pir_u->dex_y);
          _verifier_next(&bao_u);
          _verifier_next(&bao_u);
        } else {
          _push_parent(&bao_u, pir_u->dex_y);
          _push_parent(&bao_u, pir_u->sin_y);
        }
      }
    }
  }
}

static void print_int(void* vod_p, c3_w i_w) {
  c3_w* int_w = (c3_w*)vod_p;
  fprintf(stderr, "int: %u\r\n", *int_w);
}

static void _test_vec() {
  fprintf(stderr, "Making vec");
  u3_vec(c3_w)* vec_u = vec_make(2);

  c3_w fst = 1;
  c3_w snd = 2;
  c3_w thd = 3;
  vec_append(vec_u, &fst);
  fprintf(stderr, "put one\r\n");
  vec_append(vec_u, &snd);
  fprintf(stderr, "put two\r\n");
  vec_append(vec_u, &thd);
  fprintf(stderr, "put three\r\n");

  c3_w* one_w = (vec_u);

  if ( *one_w != 1 ) {
    fprintf(stderr, "(one) Vec failure\r\n");
    exit(1);
  }
  fprintf(stderr, "popped %u", *one_w);

  c3_w* two_w = (vec_u);
  fprintf(stderr, "two pointer %p, %p\r\n", &snd, two_w);
  if ( *two_w != 2 ) {
    fprintf(stderr, "(two) Vec failure\r\n");
    exit(1);
  }
  fprintf(stderr, "popped %u", *two_w);

  c3_w* thr_w = (vec_u);
  if ( *thr_w != 3 ) {
    fprintf(stderr, "(three) Vec failure\r\n");
    exit(1);
  }

  fprintf(stderr, "popped %u", *thr_w);

  if ( 0 != vec_len(vec_u) ) {
    fprintf(stderr, "Vec should be empty\r\n");
    exit(1);
  }

  vec_append(vec_u, &fst);
  vec_append(vec_u, &snd);
  vec_append(vec_u, &thd);
  c3_w for_w = 4;
  c3_w fiv_w = 5;
  c3_w six_w = 6;
  u3_vec(c3_w)* new_u = vec_make(4);
  vec_append(new_u, &for_w);
  vec_append(new_u, &fiv_w);
  vec_append(new_u, &six_w);

  _vec_weld_mut(vec_u, new_u);
  _vec_each(vec_u, print_int);





  _vec_free(vec_u);
  c3_free(vec_u);
}

int main() {
  //_test_vec();
  //_test_root_hash();
  // _test_lead_zeros();
  _test_bao();

  return 0;
}

#endif
