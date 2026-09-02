/// @file

#include "vere.h"
#include <stdio.h>
#include <types.h>
#include "urcrypt.h"
#include "lss.h"

static c3_y IV[32] = {103, 230, 9, 106, 133, 174, 103, 187, 114, 243, 110, 60, 58, 245, 79, 165, 127, 82, 14, 81, 140, 104, 5, 155, 171, 217, 131, 31, 25, 205, 224, 91};

static void _leaf_hash(lss_hash out, c3_y* leaf_y, c3_h leaf_h, c3_d counter_d)
{
  c3_y cv[32];
  memcpy(cv, IV, 32);
  c3_y block[64] = {0};
  c3_y block_len = 0;
  c3_y flags = 0;
  urcrypt_blake3_chunk_output(leaf_h, leaf_y, cv, block, &block_len, &counter_d, &flags);
  urcrypt_blake3_compress(cv, block, block_len, counter_d, flags, block);
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

static void _subtree_root(lss_hash out, c3_y* leaf_y, c3_h leaf_h, c3_d counter_d)
{
  if ( leaf_h <= 1024 ) {
    _leaf_hash(out, leaf_y, leaf_h, counter_d);
    return;
  }
  c3_h leaves_h = (leaf_h + 1023) / 1024;
  c3_h mid_h = 1 << (c3_bits_half(leaves_h-1) - 1);
  lss_hash l, r;
  _subtree_root(l, leaf_y, (mid_h * 1024), counter_d);
  _subtree_root(r, leaf_y + (mid_h * 1024), leaf_h - (mid_h * 1024), counter_d + mid_h);
  _parent_hash(out, l, r);
}

c3_d lss_proof_size(c3_d leaves) {
  return 1 + c3_bits_chub(leaves-1);
}

c3_o _lss_expect_pair(c3_h leaves, c3_h i) {
  return __((i != 0) && ((i + (1 << (1+c3_tz_w(i)))) < leaves));
}

static void _lss_builder_merge(lss_builder* bil_u, c3_h height, lss_hash l, lss_hash r) {
  // whenever two subtrees are merged, insert them into the pairs array;
  // if the merged tree is part of the left-side "spine" of the tree,
  // instead add the right subtree to the initial proof
  if ( bil_u->counter >> (height+1) ) {
    c3_h i = (bil_u->counter&~((1<<(height+1))-1)) - (1<<height);
    memcpy(bil_u->pairs[i][0], l, sizeof(lss_hash));
    memcpy(bil_u->pairs[i][1], r, sizeof(lss_hash));
  } else {
    if (height == 0) {
      memcpy(bil_u->proof[0], l, sizeof(lss_hash)); // proof always begins with [0, 1)
    }
    memcpy(bil_u->proof[height+1], r, sizeof(lss_hash));
  }
}

void lss_builder_ingest(lss_builder* bil_u, c3_y* leaf_y, c3_h leaf_h) {
  lss_hash h;
  _leaf_hash(h, leaf_y, leaf_h, bil_u->counter);
  c3_h height = 0;
  while ( bil_u->counter&(1<<height) ) {
    _lss_builder_merge(bil_u, height, bil_u->trees[height], h);
    _parent_hash(h, bil_u->trees[height], h);
    height++;
  }
  memcpy(bil_u->trees[height], h, sizeof(lss_hash));
  bil_u->counter++;
}

c3_h lss_builder_transceive(lss_builder* bil_u, c3_h steps, c3_y* jumbo_y, c3_h jumbo_h, lss_pair* pair) {
  if ( pair != NULL ) {
    c3_h i = bil_u->counter;
    memcpy(bil_u->pairs[i][0], (*pair)[0], sizeof(lss_hash));
    memcpy(bil_u->pairs[i][1], (*pair)[1], sizeof(lss_hash));
  }
  for (c3_h i = 0; (i < (1<<steps)) && (jumbo_h > 0); i++) {
    c3_h leaf_h = c3_min(jumbo_h, 1024);
    lss_builder_ingest(bil_u, jumbo_y, leaf_h);
    jumbo_y += leaf_h;
    jumbo_h -= leaf_h;
  }
  return c3_min(bil_u->counter - (1<<steps)/2, bil_u->leaves);
}

lss_hash* lss_builder_finalize(lss_builder* bil_u) {
  if ( bil_u->counter != 0 ) {
    c3_h height = c3_tz_w(bil_u->counter);
    lss_hash h;
    memcpy(h, bil_u->trees[height], sizeof(lss_hash));
    for (height++; height < sizeof(bil_u->trees)/sizeof(lss_hash); height++) {
      if ( bil_u->counter&(1<<height) ) {
        _lss_builder_merge(bil_u, height, bil_u->trees[height], h);
        _parent_hash(h, bil_u->trees[height], h);
      }
    }
  }
  return bil_u->proof;
}

lss_pair* lss_builder_pair(lss_builder* bil_u, c3_h i) {
  if ( c3y == _lss_expect_pair(bil_u->leaves, i) ) {
    return &bil_u->pairs[i];
  }
  return NULL;
}

void lss_builder_init(lss_builder* bil_u, c3_h leaves) {
  bil_u->leaves = leaves;
  bil_u->counter = 0;
  bil_u->proof = c3_calloc(lss_proof_size(leaves) * sizeof(lss_hash));
  bil_u->pairs = c3_calloc(leaves * sizeof(lss_pair));
}

void lss_builder_free(lss_builder* bil_u) {
  c3_free(bil_u->proof);
  c3_free(bil_u->pairs);
  c3_free(bil_u);
}

lss_hash* lss_transceive_proof(lss_hash* proof, c3_h steps) {
  for (c3_h i = 0; i < steps; i++) {
    _parent_hash(proof[i+1], proof[i], proof[i+1]);
  }
  return proof + steps;
}

static c3_o _lss_verifier_check_hash(lss_verifier* los_u, c3_h i, c3_h height, lss_hash h)
{
  // Binary numeral trees are composed of a set of perfect binary trees of
  // unique heights. Unless the set consists of a single tree, there will be
  // pairs that span two trees. We call these pairs "odd" because the heights of
  // their children are uneven. Specifically, the left child of an odd pair has
  // a greater height than the right child.
  //
  // When such a pair is inserted by lss_verifier_ingest, both children are
  // inserted at the same height, causing right child to be in the "wrong"
  // place. The abstruse bithacking below corrects for this. First, it
  // calculates the positions of the odd pairs within a tree of this size. Then
  // it determines how many odd pairs are directly above us, and increments the
  // height accordingly. A mask is used to ensure that we only perform this
  // adjustment when necessary.
  c3_h odd = (1<<c3_bits_half(los_u->leaves-1)) - los_u->leaves;
  c3_h mask = (1<<c3_bits_half(i ^ (los_u->leaves-1))) - 1;
  height += c3_tz_w(~((odd&~mask) >> height));
  c3_b parity = (i >> height) & 1;
  return __(memcmp(los_u->pairs[height][parity], h, sizeof(lss_hash)) == 0);
}

c3_o lss_verifier_ingest(lss_verifier* los_u, c3_y* leaf_y, c3_h leaf_h, lss_pair* pair) {
  // verify leaf
  /* los_u->counter++; */
  /* return c3y; */

  lss_hash h;
  _subtree_root(h, leaf_y, leaf_h, los_u->counter << los_u->steps);
  if ( c3n == _lss_verifier_check_hash(los_u, los_u->counter, 0, h) ) {
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
  c3_h height = c3_tz_w(los_u->counter);
  c3_h start = los_u->counter + (1 << height); // first leaf "covered" by this pair
  lss_hash parent_hash;
  _parent_hash(parent_hash, (*pair)[0], (*pair)[1]);
  if ( c3n == _lss_verifier_check_hash(los_u, start, height+1, parent_hash) ) {
    return c3n;
  }
  memcpy(los_u->pairs[height][0], (*pair)[0], sizeof(lss_hash));
  memcpy(los_u->pairs[height][1], (*pair)[1], sizeof(lss_hash));
  los_u->counter++;
  return c3y;
}

void lss_verifier_init(lss_verifier* los_u, c3_h steps, c3_h leaves, lss_hash* proof, arena* are_u) {
  c3_h proof_h = lss_proof_size(leaves);
  c3_h pairs_h = c3_bits_half(leaves);
  los_u->steps = steps;
  los_u->leaves = leaves;
  los_u->counter = 0;
  los_u->pairs = new(are_u, lss_pair, pairs_h);
  memcpy(los_u->pairs[0][0], proof[0], sizeof(lss_hash));
  for (c3_h i = 1; i < proof_h; i++) {
    memcpy(los_u->pairs[i-1][1], proof[i], sizeof(lss_hash));
  }
}

void lss_complete_inline_proof(lss_hash* proof, c3_y* leaf_y, c3_h leaf_h) {
  _subtree_root(proof[0], leaf_y, leaf_h, 0);
}

void lss_root(lss_hash root, lss_hash* proof, c3_h proof_h) {
  memcpy(root, proof[0], sizeof(lss_hash));
  for (c3_h i = 1; i < proof_h; i++) {
    _parent_hash(root, root, proof[i]);
  }
}

#ifdef LSS_TEST

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

  c3_h dat_h = 1024 * 7 + 1;
  c3_y* dat_y = c3_calloc(dat_h);

  c3_y* leaves_y[8];
  c3_h leaves_h[8];
  lss_hash leaves_s[8];
  lss_pair *pairs[8];

  // construct leaves
  for ( c3_h i = 0; i < 8; i++ ) {
    leaves_y[i] = dat_y + 1024 * i;
    leaves_h[i] = i < 7 ? 1024 : 1;
    _leaf_hash(leaves_s[i], leaves_y[i], leaves_h[i], i);
    pairs[i] = NULL;
  }
  // construct pairs
  pairs[1] = _make_pair(leaves_s[2], leaves_s[3]);
  pairs[3] = _make_pair(leaves_s[4], leaves_s[5]);
  pairs[5] = _make_pair(leaves_s[6], leaves_s[7]);
  lss_hash pair3_hash;
  _parent_hash(pair3_hash, (*pairs[3])[0], (*pairs[3])[1]);
  lss_hash pair5_hash;
  _parent_hash(pair5_hash, (*pairs[5])[0], (*pairs[5])[1]);
  pairs[2] = _make_pair(pair3_hash, pair5_hash);

  // construct proof
  lss_hash *proof = c3_calloc(4 * sizeof(lss_hash));
  memcpy(proof[0], leaves_s[0], 32);
  memcpy(proof[1], leaves_s[1], 32);
  _parent_hash(proof[2], leaves_s[2], leaves_s[3]);
  _parent_hash(proof[3], (*pairs[2])[0], (*pairs[2])[1]);

  // verify
  lss_verifier lss_u;
  memset(&lss_u, 0, sizeof(lss_verifier));
  lss_verifier_init(&lss_u, 0, 8, proof);
  for ( c3_h i = 0; i < 8; i++ ) {
    asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[i], leaves_h[i], pairs[i]))
  }

  #undef asrt_ok
}

static void _test_lss_build_verify(c3_h dat_w)
{
  #define asrt_ok(y) if ( c3y != y ) { fprintf(stderr, "failed at %s:%u\n", __FILE__, __LINE__); exit(1); }

  c3_y* dat_y = c3_calloc(dat_w);
  for ( c3_h i = 0; i < dat_w; i++ ) {
    dat_y[i] = i;
  }
  c3_h leaves_h = (dat_w + 1023) / 1024;

  // build
  lss_builder bil_u;
  lss_builder_init(&bil_u, leaves_h);
  for ( c3_h i = 0; i < leaves_h; i++ ) {
    c3_y* leaf_y = dat_y + (i*1024);
    c3_h leaf_h = (i < leaves_h - 1) ? 1024 : dat_w % 1024;
    lss_builder_ingest(&bil_u, leaf_y, leaf_h);
  }
  lss_hash* proof = lss_builder_finalize(&bil_u);

  // verify
  lss_verifier lss_u;
  lss_verifier_init(&lss_u, 0, leaves_h, proof);
  for ( c3_h i = 0; i < leaves_h; i++ ) {
    c3_y* leaf_y = dat_y + (i*1024);
    c3_h leaf_h = (i < leaves_h - 1) ? 1024 : dat_w % 1024;
    lss_pair* pair = lss_builder_pair(&bil_u, i);
    asrt_ok(lss_verifier_ingest(&lss_u, leaf_y, leaf_h, pair));
  }

  #undef asrt_ok
}

static void _test_lss_build_verify_jumbo(c3_h steps, c3_h dat_h)
{
  #define asrt_ok(y) if ( c3y != y ) { fprintf(stderr, "failed at %s:%u\n", __FILE__, __LINE__); exit(1); }

  c3_y* dat_y = c3_calloc(dat_h);
  for ( c3_h i = 0; i < dat_h; i++ ) {
    dat_y[i] = i;
  }
  c3_h leaves_h = (dat_h + 1023) / 1024;

  // build
  lss_builder bil_u;
  lss_builder_init(&bil_u, leaves_h);
  for ( c3_h i = 0; i < leaves_h; i++ ) {
    c3_y* leaf_y = dat_y + (i*1024);
    c3_h leaf_h = c3_min(dat_h - (i*1024), 1024);
    lss_builder_ingest(&bil_u, leaf_y, leaf_h);
  }
  lss_hash* proof = lss_builder_finalize(&bil_u);

  // transceive up
  c3_h jumbo_leaf_h = 1024 << steps;
  c3_h jumbo_leaves_h = (dat_h + jumbo_leaf_h - 1) / jumbo_leaf_h;

  // verify (if possible)
  if ( jumbo_leaves_h > 1 ) {
    lss_verifier lss_u;
    lss_verifier_init(&lss_u, steps, jumbo_leaves_h, lss_transceive_proof(proof, steps));
    for ( c3_h i = 0; i < jumbo_leaves_h; i++ ) {
      c3_y* leaf_y = dat_y + (i*(1024<<steps));
      c3_h leaf_h = c3_min(dat_h - (i*(1024<<steps)), (1024<<steps));
      lss_pair* pair = lss_builder_pair(&bil_u, i<<steps);
      asrt_ok(lss_verifier_ingest(&lss_u, leaf_y, leaf_h, pair));
    }
  }

  // transceive down
  lss_builder dil_u;
  lss_builder_init(&dil_u, leaves_h);
  for ( c3_h i = 0; i < jumbo_leaves_h; i++ ) {
    c3_y* leaf_y = dat_y + (i*jumbo_leaf_h);
    c3_h leaf_h = c3_min(dat_h - (i*jumbo_leaf_h), jumbo_leaf_h);
    lss_pair* pair = lss_builder_pair(&bil_u, i<<steps);
    c3_h ready_h = lss_builder_transceive(&dil_u, steps, leaf_y, leaf_h, pair);
    for ( c3_h j = 0; j < ready_h; j++ ) {
      asrt_ok(__((lss_builder_pair(&dil_u, j) != NULL) || (c3n == _lss_expect_pair(leaves_h, j))));
    }
  }
  proof = lss_builder_finalize(&dil_u);

  // verify
  lss_verifier dss_u;
  lss_verifier_init(&dss_u, 0, leaves_h, proof);
  for ( c3_h i = 0; i < leaves_h; i++ ) {
    c3_y* leaf_y = dat_y + (i*1024);
    c3_h leaf_h = c3_min(dat_h - (i*1024), 1024);
    lss_pair* pair = lss_builder_pair(&dil_u, i);
    asrt_ok(lss_verifier_ingest(&dss_u, leaf_y, leaf_h, pair));
  }

  #undef asrt_ok
}

int main() {
  _test_lss_manual_verify_8();

  _test_lss_build_verify(2 * 1024);
  _test_lss_build_verify(2 * 1024 + 2);
  _test_lss_build_verify(3 * 1024);
  _test_lss_build_verify(5 * 1024 + 5);
  _test_lss_build_verify(17 * 1024 + 512);
  _test_lss_build_verify(128 * 1024);

  _test_lss_build_verify_jumbo(1, 4 * 1024 + 97);
  _test_lss_build_verify_jumbo(3, 21 * 1024);
  _test_lss_build_verify_jumbo(6, 256 * 1024);
  _test_lss_build_verify_jumbo(10, 1 * 1024 + 1);
  return 0;
}

#endif
