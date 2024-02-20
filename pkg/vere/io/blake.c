/// @file


#include "vere.h"
#include <stdio.h>
#include <types.h>
#include "urcrypt.h"

#define BLAKE_TEST
#define BLAKE3_BLOCK_LEN  64

enum blake3_flags {
  CHUNK_START         = 1 << 0,
  CHUNK_END           = 1 << 1,
  PARENT              = 1 << 2,
  ROOT                = 1 << 3,
  KEYED_HASH          = 1 << 4,
  DERIVE_KEY_CONTEXT  = 1 << 5,
  DERIVE_KEY_MATERIAL = 1 << 6,
};

typedef struct _blake_node {
	c3_w  cev_w[8];
	c3_w  boq_w[16];
	c3_d  con_d;
	c3_w  len_w;
	c3_w  fag_w;
} blake_node;




static c3_w IV[8] = {0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL, 0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL};
#define ABS_DIFF(x,y) x > y ? x - y : y - x

#define VEC_SCALING 2

typedef struct _u3_raw_vec {
  c3_w siz_w;
  c3_w len_w;
  void** vod_p;
} u3_raw_vec;

#define u3_vec(type) u3_raw_vec

static void
_vec_init(u3_raw_vec* vec_u, c3_w siz_w)
{
  vec_u->siz_w = siz_w;
  vec_u->vod_p = c3_calloc(siz_w * sizeof(void*));
  vec_u->len_w = 0;
}

static void
_vec_free(u3_raw_vec* vec_u)
{
  c3_free(vec_u->vod_p);
}

static u3_raw_vec*
_vec_make(c3_w siz_w)
{
  u3_raw_vec* vec_u = c3_calloc(sizeof(u3_raw_vec));
  _vec_init(vec_u, siz_w);
  return vec_u;
}



static c3_w
_vec_len(u3_raw_vec* vec_u)
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

  fprintf(stderr, "new size: %u", siz_w);
  memcpy(vod_p, vec_u->vod_p, vec_u->len_w * sizeof(void*));
  c3_free(vec_u->vod_p);
  vec_u->vod_p = vod_p;
  vec_u->siz_w = siz_w;
}

static void
_vec_append(u3_raw_vec* vec_u, void* tem_p)
{
  if(vec_u->siz_w == vec_u->len_w ) {
    _vec_resize(vec_u, vec_u->siz_w * VEC_SCALING);
  }
  vec_u->vod_p[vec_u->len_w] = tem_p;
  vec_u->len_w++;
}

static void*
_vec_popf(u3_raw_vec* vec_u) {
  if ( vec_u->len_w == 0 ) {
    fprintf(stderr, "Failed pop\r\n");
    return NULL;
  }
  void* hit_p = vec_u->vod_p[0];
  void* vod_p = c3_calloc(vec_u->siz_w);
  memcpy(vod_p, vec_u->vod_p + sizeof(void*), vec_u->len_w - 1);
  c3_free(vec_u->vod_p);
  vec_u->len_w--;
  vec_u->vod_p = vod_p;
  return hit_p;
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
  memcpy(fst_u->vod_p + (fst_u->len_w * sizeof(void*)), snd_u->vod_p, snd_u->len_w );
  fst_u->len_w += snd_u->len_w;
}

//type Hash [32]byte

//func (h Hash) String() string { return hex.EncodeToString(h[:]) }

//type Pair [2][8]uint32
typedef struct _pair {
  c3_w sin_w[8];
  c3_w dex_w[8];
} pair;

//type subtree [2]int
typedef struct _subtree {
  c3_d sin_d;
  c3_d dex_d;
} subtree;

typedef struct _verifier {
	c3_d      num_d;
	subtree   sub_u;
  u3_vec(c3_w[8]) que_u;
  u3_vec(c3_w[8]) sta_u;
} verifier;

static c3_d _count_lead_zeros(c3_d bit_d) 
{
  return (bit_d == 0) ? 64 : __builtin_clz(bit_d);
}

static c3_d _bitlen(c3_d bit_d)
{
  return 64 - _count_lead_zeros(bit_d);
}

static c3_d
_height(subtree* sub_u)
{
  return _bitlen(ABS_DIFF(sub_u->dex_d, sub_u->sin_d)) - 1;
}

static c3_d
_largest_pow2(c3_d val_d) 
{
	return 1 << (_bitlen(val_d-1) - 1);
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
_make_chain_value(c3_w res_w[8], blake_node* nod_u)
{
  //c3_w* blake3
}

static blake_node
_compress_chunk(c3_y* dat_y, c3_w dat_w, c3_w cev_w[8], c3_d con_d, c3_w fag_w)
{
  // A node represents a chunk or parent in the BLAKE3 Merkle tree.
  blake_node nod_u;
  memcpy(&nod_u.cev_w, cev_w, 8 * sizeof(c3_w));
  nod_u.con_d = con_d;
  nod_u.len_w = BLAKE3_BLOCK_LEN;
  nod_u.fag_w |= CHUNK_START;

  c3_y* boq_y = alloca(BLAKE3_BLOCK_LEN);
  c3_w  lef_w = dat_w;
  memset(boq_y, 0, BLAKE3_BLOCK_LEN);
  while ( lef_w > BLAKE3_BLOCK_LEN ) {
    memcpy(boq_y, dat_y, BLAKE3_BLOCK_LEN);
    boq_y += BLAKE3_BLOCK_LEN;
    lef_w -= BLAKE3_BLOCK_LEN;
    y_to_w(boq_y, nod_u.boq_w);
    c3_w cev_w[8];
    memset(cev_w, 0, 8 * sizeof(c3_w));
    _make_chain_value(cev_w, &nod_u);
    memcpy(nod_u.cev_w, cev_w, 8 * sizeof(c3_w));
    nod_u.fag_w &= ~CHUNK_START;
  }
  memset(boq_y, 0, BLAKE3_BLOCK_LEN);
  nod_u.len_w = lef_w;
  memcpy(boq_y, dat_y, lef_w);
  y_to_w(boq_y, nod_u.boq_w);
  nod_u.fag_w |= CHUNK_END;

  return nod_u;
}

static
blake_node _leaf_hash(c3_d con_d)
{

//_compress_chunk(c3_y* dat_y, c3_w dat_w, c3_w* cev_w, c3_d con_d, c3_w fag_w)
  c3_y* dat_y = alloca(1024);
  memset(dat_y, 0, 1024);
  return _compress_chunk(dat_y, 1024, IV, con_d, 0);
}

static blake_node
_parent_node(c3_w sin_w[8], c3_w dex_w[8], c3_w key_w[8], c3_w fag_w)
{
  blake_node nod_u;
  nod_u.con_d = 0;
  memcpy(&nod_u.cev_w, key_w, 8 * sizeof(c3_w));
  nod_u.len_w = BLAKE3_BLOCK_LEN;
  nod_u.fag_w |= PARENT;
  memcpy(nod_u.boq_w, sin_w, 8 * sizeof(c3_w));
  memcpy(&nod_u.boq_w[8], dex_w, 8 * sizeof(c3_w));
  return nod_u;
}

static blake_node
_parent_hash(blake_node* sin_u, blake_node* dex_u)
{
  c3_w sin_w[8], dex_w[8];
  memset(sin_w, 0, 8 * sizeof(c3_w));
  memset(dex_w, 0, 8 * sizeof(c3_w));
  _make_chain_value(sin_w, sin_u);
  _make_chain_value(dex_w, dex_u);
  return _parent_node(sin_w, dex_w, IV, 0);
}

static void
_compress_node(c3_y out_y[64], blake_node* nod_u)
{
  //urcrypt_blake3_compress(nod_u->cev_w, nod_u->boq_w, nod_u->len_w, nod_u->con_w, nod_u->fag_w, out_y);



}


static void 
_root_hash(c3_y out_y[16], blake_node* nod_u)
{
  nod_u->fag_w |= ROOT;

}

static void recurse_subtree(subtree* sub_u, blake_node* nod_u, u3_vec(pair)* par_u)
{
  if ( _height(sub_u) == 0 ) {
    *nod_u = _leaf_hash(sub_u->sin_d);
    return;
  }
  c3_d mid_d = sub_u->sin_d + _largest_pow2(ABS_DIFF(sub_u->dex_d, sub_u->sin_d));

  subtree sin_u = (subtree){sub_u->sin_d, mid_d};
  blake_node* lod_u = c3_calloc(sizeof(blake_node));
  u3_vec(pair)* lar_u = _vec_make(8);
  recurse_subtree(&sin_u, lod_u, lar_u);

  subtree dex_u = (subtree){mid_d, sub_u->dex_d};
  blake_node* rod_u = c3_calloc(sizeof(blake_node));
  u3_vec(pair)* rar_u = _vec_make(8);
  recurse_subtree(&dex_u, rod_u, rar_u);

  *nod_u = _parent_hash(lod_u, rod_u);
  c3_w* lcv_w = c3_calloc(8* sizeof(c3_w));
  _make_chain_value(lcv_w, lod_u);
  c3_w* rcv_w = c3_calloc(8* sizeof(c3_w));
  _make_chain_value(rcv_w, rod_u);

  _vec_append(par_u, lcv_w);
  _vec_append(par_u, rcv_w);
  _vec_weld_mut(par_u, lar_u);
  _vec_weld_mut(par_u, rar_u);
}

static void
_bao_build(c3_d num_d, c3_y has_y[32], u3_vec(c3_w[8])* pof_u, u3_vec(pair)* par_u) {
  subtree sub_u;
  memset(&sub_u, 0, sizeof(subtree));
  blake_node* nod_u = c3_calloc(sizeof(blake_node));
  recurse_subtree(&sub_u, nod_u, par_u);

  if ( _vec_len(par_u) != 0 ) {
    for ( c3_w i_w = _bitlen(num_d - 1); i_w > 1; i_w-- ) {
      _vec_append(pof_u, _vec_get(pair, par_u, 0)->dex_w);
      _vec_popf(par_u);
    }
    pair* fst_u = _vec_get(pair, par_u, 0);
    _vec_append(pof_u, fst_u->dex_w);
    _vec_append(pof_u, fst_u->sin_w);
    _vec_popf(par_u);
    for ( c3_w i_w = 0; i_w < (_vec_len(pof_u) >> 1); i_w++ ) {
      c3_w jay_w = _vec_len(pof_u) - i_w - 1;
      _vec_swap(pof_u, i_w, jay_w);
    }
  }
}

static void
_push_leaf(verifier* ver_u, c3_w lef_w[8]) 
{
  _vec_append(&ver_u->que_u, lef_w);
}

static void
_pop_leaf(c3_w out_w[8], verifier* ver_u)
{
  c3_w* res_w = _vec_popf(&ver_u->que_u);
  memcpy(out_w, res_w, 8);
}

static void
_push_parent(verifier* ver_u, c3_w par_w[8])
{
  _vec_append(&ver_u->sta_u, par_w);
}

static void
_pop_parent(c3_w out_w[8], verifier* ver_u)
{
  c3_w* res_w = _vec_popf(&ver_u->sta_u);
  memcpy(out_w, res_w, 8);
}

static void
_verifier_next(verifier* ver_u) 
{
  if ( _height(&ver_u->sub_u) == 0 ) {
    ver_u->sub_u.sin_d += 1;
    ver_u->sub_u.dex_d += 1;
    if ( ver_u->sub_u.dex_d > ver_u->num_d ) {
      ver_u->sub_u.dex_d = ver_u->num_d;
    }
    subtree sub_u = (subtree){ver_u->sub_u.sin_d, ver_u->sub_u.sin_d + _largest_pow2(ABS_DIFF(ver_u->sub_u.dex_d, ver_u->sub_u.sin_d))};
    ver_u->sub_u = sub_u;
  }
}

static c3_o
_verifier_init(verifier* ver_u, c3_y rot_y[32], u3_vec(c3_w[8])* pof_u)
{
  if ( _vec_len(pof_u) == 0 ) {
    return ver_u->num_d <= 1 ? c3y : c3n;
  }
  blake_node nod_u = _parent_node(_vec_get(c3_w, pof_u, 0), _vec_get(c3_w, pof_u, 1), IV, 0);
  for (c3_w i_w = 2; i_w < (_vec_len(pof_u) - 2); i_w++ ) {
    c3_w cev_w[8];
    memset(cev_w, 0, 8);
    _make_chain_value(cev_w, &nod_u);
    c3_w* pof_w = _vec_get(c3_w, pof_u, i_w);
    nod_u = _parent_node(cev_w, pof_w, IV, 0);
  }
  c3_y ron_y[32];
  _root_hash(ron_y, &nod_u);

  if ( memcmp(ron_y, rot_y, 32) != 0 ) {
    return c3n;
  }

  ver_u->sub_u = (subtree){2,4};
  _vec_init(&ver_u->que_u, 8);
  _vec_append(&ver_u->que_u, _vec_get(c3_w, pof_u, 0));
  _vec_append(&ver_u->que_u, _vec_get(c3_w, pof_u, 1));

  _vec_init(&ver_u->sta_u, 8);
  _vec_weld_mut(&ver_u->sta_u, pof_u);

  for ( c3_w i_w = 0; i_w < (_vec_len(&ver_u->sta_u)/2); i_w++ ) {
    c3_w jay_w = _vec_len(&ver_u->sta_u) - i_w - 1;
    _vec_swap(&ver_u->sta_u, i_w, jay_w);
  }
  return c3y;
}

static c3_o
_veri_check_leaf(verifier* ver_u, c3_d lef_d)
{
  if ( _vec_len(&ver_u->sta_u) > 0 ) {
    c3_w out_w[8];
    _pop_leaf(out_w, ver_u);
    c3_w cav_w[8];
    blake_node nod_u = _leaf_hash(lef_d);
    _make_chain_value(cav_w, &nod_u);
    return 0 == memcmp(cav_w, out_w, 8 * sizeof(c3_w)) ? c3y : c3n;
  } else if ( _vec_len(&ver_u->sta_u) > 0 ) {
    c3_w out_w[8];
    _pop_parent(out_w, ver_u);
    c3_w cav_w[8];
    blake_node nod_u = _leaf_hash(lef_d);
    _make_chain_value(cav_w, &nod_u);
    return 0 == memcmp(cav_w, out_w, 8 * sizeof(c3_w)) ? c3y : c3n;
  }
  return c3n;
}

static c3_o
_veri_check_pair(verifier* ver_u, pair* par_u)
{
  c3_w out_w[8];
  _pop_parent(out_w, ver_u);
  blake_node nod_u = _parent_node(par_u->sin_w, par_u->dex_w, IV, 0);
  c3_w cav_w[8];
  _make_chain_value(cav_w, &nod_u);
  c3_w par_w[8];
  _pop_parent(par_w, ver_u);
  c3_o ret_o =  _( _vec_len(&ver_u->sta_u) > 0 && memcmp(par_w, cav_w, 8 * sizeof(c3_w)));
  return ret_o;
}

static void _veri_init(verifier* ver_u, c3_d num_d)
{
  ver_u->num_d = num_d;
  ver_u->sub_u = (subtree){0, num_d};
}
 
#ifdef BLAKE_TEST

static void _test_bao() 
{
  fprintf(stderr, "Starting test\r\n");
  for ( c3_w num_w = 1; num_w <= 100; num_w++ ) {
    c3_y has_y[32];
    u3_vec(c3_w[8])* pof_u = _vec_make(10);
    u3_vec(pair)* par_u = _vec_make(10);
    _bao_build(num_w, has_y, pof_u, par_u);
    verifier ver_u;
    memset(&ver_u, 0, sizeof(verifier));
    _veri_init(&ver_u, num_w);
    if ( c3n == _verifier_init(&ver_u, has_y, pof_u) ) {
      fprintf(stderr, "Failed on %u\r\n", num_w);
      exit(1);
    }

    if ( num_w == 1 ) {
      c3_y out_y[16];
      blake_node nod_u = _leaf_hash(0);
      _root_hash(out_y, &nod_u);
      if ( memcmp(out_y, has_y, 32) != 0 ) {
        fprintf(stderr, "1 special case\r\n");
        exit(1);
      }
      for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
        if ( c3n == _veri_check_leaf(&ver_u, i_w) ) {
          fprintf(stderr, "Check leaf %u failed for %u\r\n", i_w, num_w);
          exit(1);
        } 
        if ( i_w < _vec_len(par_u) ) {
          pair* pir_u = _vec_get(pair, par_u, i_w);
          if ( _veri_check_pair(&ver_u, pir_u) == c3n ) {
            fprintf(stderr, "check pair failed (%u, %u)", i_w, num_w);
            exit(1);
          }
          _verifier_next(&ver_u);
          if ( _height(&ver_u.sub_u) == 0 ) {
            _push_leaf(&ver_u, pir_u->sin_w);
            _push_leaf(&ver_u, pir_u->dex_w);
            _verifier_next(&ver_u);
            _verifier_next(&ver_u);
          } else {
            _push_parent(&ver_u, pir_u->sin_w);
            _push_parent(&ver_u, pir_u->dex_w);
          }
        }
      }
    } 
  }
}

static void _test_vec() {
  fprintf(stderr, "Making vec");
  u3_vec(c3_w)* vec_u = _vec_make(2);

  c3_w fst = 1;
  c3_w snd = 2;
  c3_w thd = 3;
  _vec_append(vec_u, &fst);
  fprintf(stderr, "put one\r\n");
  _vec_append(vec_u, &snd);
  fprintf(stderr, "put two\r\n");
  _vec_append(vec_u, &thd);
  fprintf(stderr, "put three\r\n");

  c3_w* one_w = _vec_popf(vec_u);

  if ( *one_w != 1 ) {
    fprintf(stderr, "(one) Vec failure\r\n");
    exit(1);
  }
  fprintf(stderr, "popped %u", *one_w);

  c3_w* two_w = _vec_popf(vec_u);
  fprintf(stderr, "two pointer %p, %p\r\n", &snd, two_w);
  if ( *two_w != 2 ) {
    fprintf(stderr, "(two) Vec failure\r\n");
    exit(1);
  }
  fprintf(stderr, "popped %u", *two_w);

  c3_w* thr_w = _vec_popf(vec_u);
  if ( *thr_w != 3 ) {
    fprintf(stderr, "(three) Vec failure\r\n");
    exit(1);
  }

  fprintf(stderr, "popped %u", *thr_w);

  if ( 0 != _vec_len(vec_u) ) {
    fprintf(stderr, "Vec should be empty\r\n");
    exit(1);
  }
  _vec_free(vec_u);
  c3_free(vec_u);
}

int main() {
  //_test_bao();
  _test_vec();
  
  return 0;
}

#endif
