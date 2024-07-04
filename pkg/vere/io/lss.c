/// @file

#include "vere.h"
#include <stdio.h>
#include <types.h>
#include "urcrypt.h"
#include "lss.h"

static c3_y IV[32] = {103, 230, 9, 106, 133, 174, 103, 187, 114, 243, 110, 60, 58, 245, 79, 165, 127, 82, 14, 81, 140, 104, 5, 155, 171, 217, 131, 31, 25, 205, 224, 91};

static void _leaf_hash(lss_hash out, c3_y* leaf, c3_w len, c3_d counter)
{
  c3_y cv[32];
  memcpy(cv, IV, 32);
  c3_y block[64] = {0};
  c3_y block_len = 0;
  c3_y flags = 0;
  urcrypt_blake3_chunk_output(len, leaf, cv, block, &block_len, &counter, &flags);
  urcrypt_blake3_compress(cv, block, block_len, counter, flags, block);
  memcpy(out, block, 32);
}

static void _parent_hash(lss_hash out, lss_hash left, lss_hash right)
{
  c3_y cv[32];
  memcpy(cv, IV, 32);
  c3_y block[64];
  memcpy(block, left, 32);
  memcpy(block + 32, right, 32);
  c3_y block_len = 64;
  c3_y flags = 1 << 2; // PARENT
  urcrypt_blake3_compress(cv, block, block_len, 0, flags, block);
  memcpy(out, block, 32);
}

static c3_y _bits_ctz64(c3_d val_d)
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

static c3_y _bits_len64(c3_d val_d)
{
  if (val_d == 0) {
    return 0;
  }
  c3_y ret_y = 64;
  if ((val_d & 0xFFFFFFFF00000000ULL) == 0) { ret_y -= 32; val_d <<= 32; }
  if ((val_d & 0xFFFF000000000000ULL) == 0) { ret_y -= 16; val_d <<= 16; }
  if ((val_d & 0xFF00000000000000ULL) == 0) { ret_y -= 8; val_d <<= 8; }
  if ((val_d & 0xF000000000000000ULL) == 0) { ret_y -= 4; val_d <<= 4; }
  if ((val_d & 0xC000000000000000ULL) == 0) { ret_y -= 2; val_d <<= 2; }
  if ((val_d & 0x8000000000000000ULL) == 0) { ret_y -= 1; val_d <<= 1; }
  return ret_y;
}

c3_w lss_proof_size(c3_w leaves) {
  return 1 + _bits_len64(leaves-1);
}

c3_o _lss_expect_pair(c3_w leaves, c3_w i) {
  if ( ((i != 0) && (i + (1 << (1+_bits_ctz64(i))) < leaves)) ) {
    return c3y;
  }
  return c3n;
}

static void _lss_builder_merge(lss_builder* bil_u, c3_w height, lss_hash l, lss_hash r) {
  // whenever two subtrees are merged, insert them into the pairs array;
  // if the merged tree is part of the left-side "spine" of the tree,
  // instead add the right subtree to the initial proof
  if (height+1 < _bits_len64(bil_u->counter)) {
    c3_w c = (bil_u->counter&~((1<<(height+1))-1)) - (1<<height);
    memcpy(bil_u->pairs[c][0], l, sizeof(lss_hash));
    memcpy(bil_u->pairs[c][1], r, sizeof(lss_hash));
  } else {
    if (height == 0) {
      memcpy(bil_u->proof[0], l, sizeof(lss_hash)); // proof always begins with [0, 1)
    }
    memcpy(bil_u->proof[height+1], r, sizeof(lss_hash));
  }
}

void lss_builder_ingest(lss_builder* bil_u, c3_y* leaf_y, c3_w leaf_w) {
  lss_hash h;
  _leaf_hash(h, leaf_y, leaf_w, bil_u->counter);
  c3_w height = 0;
  while ( bil_u->counter&(1<<height) ) {
    _lss_builder_merge(bil_u, height, bil_u->trees[height], h);
    _parent_hash(h, bil_u->trees[height], h);
    height++;
  }
  memcpy(bil_u->trees[height], h, sizeof(lss_hash));
  bil_u->counter++;
}

lss_hash* lss_builder_finalize(lss_builder* bil_u) {
  if ( bil_u->counter != 0 ) {
    c3_w height = _bits_ctz64(bil_u->counter);
    lss_hash h;
    memcpy(h, bil_u->trees[height], sizeof(lss_hash));
    for (height++; height < 32; height++) {
      if ( bil_u->counter&(1<<height) ) {
        _lss_builder_merge(bil_u, height, bil_u->trees[height], h);
        _parent_hash(h, bil_u->trees[height], h);
      }
    }
  }
  return bil_u->proof;
}

lss_pair* lss_builder_pair(lss_builder* bil_u, c3_w i) {
  if ( c3y == _lss_expect_pair(bil_u->counter, i) ) {
    return &bil_u->pairs[i];
  }
  return NULL;
}

void lss_builder_init(lss_builder* bil_u, c3_w leaves) {
  bil_u->counter = 0;
  bil_u->proof = c3_calloc(lss_proof_size(leaves) * sizeof(lss_hash));
  bil_u->pairs = c3_calloc(leaves * sizeof(lss_pair));
}

void lss_builder_free(lss_builder* bil_u) {
  c3_free(bil_u->proof);
  c3_free(bil_u->pairs);
  c3_free(bil_u);
}

static c3_o _lss_verifier_find_pair(lss_verifier* los_u, c3_w i, c3_w height, lss_hash h)
{
  c3_w odd = (1<<_bits_len64(los_u->leaves-1)) - los_u->leaves;
  c3_w mask = (1<<_bits_len64(i ^ (los_u->leaves-1))) - 1;
  height += _bits_ctz64(~((odd&~mask) >> height));
  c3_b parity = (i >> height) & 1;
  return _(memcmp(los_u->pairs[height][parity], h, 32) == 0);
}

c3_o lss_verifier_ingest(lss_verifier* los_u, c3_y* leaf_y, c3_w leaf_w, lss_pair* pair) {
  // verify leaf
  lss_hash h;
  _leaf_hash(h, leaf_y, leaf_w, los_u->counter);
  if ( c3n == _lss_verifier_find_pair(los_u, los_u->counter, 0, h) ) {
    return c3n;
  }
  // check whether pair is expected
  if ( (pair != NULL) != (c3y == _lss_expect_pair(los_u->leaves, los_u->counter)) ) {
    return c3n;
  } else if (pair == NULL) {
    los_u->counter++;
    return c3y;
  }
  // verify and insert pair
  c3_w height = _bits_ctz64(los_u->counter);
  c3_w pairStart = los_u->counter + (1 << height); // first leaf "covered" by this pair
  lss_hash parent_hash;
  _parent_hash(parent_hash, (*pair)[0], (*pair)[1]);
  if ( c3n == _lss_verifier_find_pair(los_u, pairStart, height+1, parent_hash) ) {
    return c3n;
  }
  memcpy(los_u->pairs[height][0], (*pair)[0], sizeof(lss_hash));
  memcpy(los_u->pairs[height][1], (*pair)[1], sizeof(lss_hash));
  los_u->counter++;
  return c3y;
}

c3_o lss_verifier_init(lss_verifier* los_u, c3_w leaves, lss_hash* proof) {
  c3_w proof_w = lss_proof_size(leaves);
  c3_w pairs_w = _bits_len64(leaves);
  los_u->leaves = leaves;
  los_u->counter = 0;
  los_u->pairs = c3_calloc(pairs_w * sizeof(lss_pair));
  memcpy(los_u->pairs[0][0], proof[0], sizeof(lss_hash));
  for (c3_w i = 1; i < proof_w; i++) {
    memcpy(los_u->pairs[i-1][1], proof[i], sizeof(lss_hash));
  }
  return c3y;
}

void lss_verifier_free(lss_verifier* los_u) {
  c3_free(los_u->pairs);
  c3_free(los_u);
}

void lss_complete_inline_proof(lss_hash* proof, c3_y* leaf_y, c3_w leaf_w) {
  _leaf_hash(proof[0], leaf_y, leaf_w, 0);
}

void lss_root(lss_hash root, lss_hash* proof, c3_w proof_w) {
  memcpy(root, proof[0], sizeof(lss_hash));
  for (c3_w i = 1; i < proof_w; i++) {
    _parent_hash(root, root, proof[i]);
  }
}

#ifdef LSS_TEST

static void _test__bits_ctz64()
{
  #define asrt(x,y) if ( x != y ) { fprintf(stderr, "failed at %s:%u: got %u, expected %u", __FILE__, __LINE__, x,y); exit(1); }
  asrt(_bits_ctz64(0ULL), 64);
  asrt(_bits_ctz64(0x0000000000000001ULL), 0);
  asrt(_bits_ctz64(0x0000000000000002ULL), 1);
  asrt(_bits_ctz64(0x0000000000000100ULL), 8);
  asrt(_bits_ctz64(0x0000000080000000ULL), 31);
  asrt(_bits_ctz64(0x0000000100000000ULL), 32);
  asrt(_bits_ctz64(0x4000000000000000ULL), 62);
  asrt(_bits_ctz64(0x8000000000000000ULL), 63);
  #undef asrt
}

static lss_pair* _make_pair(lss_hash left, lss_hash right)
{
  lss_pair* pair = c3_malloc(sizeof(lss_pair));
  memcpy((*pair)[0], left, 32);
  memcpy((*pair)[1], right, 32);
  return pair;
}

static void _test_lss_manual_verify_8()
{
  #define asrt_ok(y) if ( c3y != y ) { fprintf(stderr, "failed at %s:%u\n", __FILE__, __LINE__); exit(1); }

  c3_w dat_w = 1024 * 7 + 1;
  c3_y* dat_y = c3_calloc(dat_w);

  c3_y* leaves_y[8];
  c3_w leaves_w[8];
  lss_hash leaves_h[8];
  lss_pair *pairs[8];

  // construct leaves
  for ( c3_w i = 0; i < 8; i++ ) {
    leaves_y[i] = dat_y + 1024 * i;
    leaves_w[i] = i < 7 ? 1024 : 1;
    _leaf_hash(leaves_h[i], leaves_y[i], leaves_w[i], i);
    pairs[i] = NULL;
  }
  // construct pairs
  pairs[1] = _make_pair(leaves_h[2], leaves_h[3]);
  pairs[3] = _make_pair(leaves_h[4], leaves_h[5]);
  pairs[5] = _make_pair(leaves_h[6], leaves_h[7]);
  lss_hash pair3_hash;
  _parent_hash(pair3_hash, (*pairs[3])[0], (*pairs[3])[1]);
  lss_hash pair5_hash;
  _parent_hash(pair5_hash, (*pairs[5])[0], (*pairs[5])[1]);
  pairs[2] = _make_pair(pair3_hash, pair5_hash);

  // construct proof
  lss_hash *proof = c3_calloc(4 * sizeof(lss_hash));
  memcpy(proof[0], leaves_h[0], 32);
  memcpy(proof[1], leaves_h[1], 32);
  _parent_hash(proof[2], leaves_h[2], leaves_h[3]);
  _parent_hash(proof[3], (*pairs[2])[0], (*pairs[2])[1]);

  // verify
  lss_verifier lss_u;
  memset(&lss_u, 0, sizeof(lss_verifier));
  asrt_ok(lss_verifier_init(&lss_u, 8, proof))
  for ( c3_w i = 0; i < 8; i++ ) {
    asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[i], leaves_w[i], pairs[i]))
  }

  #undef asrt_ok
}

static void _test_lss_build_verify(c3_w dat_w)
{
  #define asrt_ok(y) if ( c3y != y ) { fprintf(stderr, "failed at %s:%u\n", __FILE__, __LINE__); exit(1); }

  c3_y* dat_y = c3_calloc(dat_w);
  for ( c3_w i = 0; i < dat_w; i++ ) {
    dat_y[i] = i;
  }
  c3_w leaves_w = (dat_w + 1023) / 1024;

  // build
  lss_builder bil_u;
  lss_builder_init(&bil_u, leaves_w);
  for ( c3_w i = 0; i < leaves_w; i++ ) {
    c3_y* leaf_y = dat_y + (i*1024);
    c3_w leaf_w = (i < leaves_w - 1) ? 1024 : dat_w % 1024;
    lss_builder_ingest(&bil_u, leaf_y, leaf_w);
  }
  lss_hash* proof = lss_builder_finalize(&bil_u);

  // verify
  lss_verifier lss_u;
  asrt_ok(lss_verifier_init(&lss_u, leaves_w, proof))
  for ( c3_w i = 0; i < leaves_w; i++ ) {
    c3_y* leaf_y = dat_y + (i*1024);
    c3_w leaf_w = (i < leaves_w - 1) ? 1024 : dat_w % 1024;
    lss_pair* pair = lss_builder_pair(&bil_u, i);
    asrt_ok(lss_verifier_ingest(&lss_u, leaf_y, leaf_w, pair));
  }

  #undef asrt_ok
}

int main() {
  _test__bits_ctz64();
  _test_lss_manual_verify_8();
  _test_lss_build_verify(2 * 1024);
  _test_lss_build_verify(2 * 1024 + 2);
  _test_lss_build_verify(3 * 1024);
  _test_lss_build_verify(5 * 1024 + 5);
  _test_lss_build_verify(17 * 1024 + 512);
  _test_lss_build_verify(128 * 1024);
  return 0;
}

#endif
