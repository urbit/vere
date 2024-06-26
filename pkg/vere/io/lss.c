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

static c3_y bits_ctz64(c3_d val_d)
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

static c3_y bits_len64(c3_d val_d)
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

static c3_o lss_find_pair(lss_verifier* los_u, c3_w height, c3_b sel, lss_hash h)
{
  if (memcmp(los_u->pairs[height][sel], h, 32) == 0) {
    return c3y;
  }
  // look for "irregular" pairs, which will always be on the right
  c3_w max_height = bits_len64(los_u->leaves);
  for (height++; height < max_height; height++) {
    if (memcmp(los_u->pairs[height][1], h, 32) == 0) {
      return c3y;
    }
  }
  return c3n;
}

c3_o lss_verifier_ingest(lss_verifier* los_u, c3_y* leaf_y, c3_w leaf_w, lss_pair* pair) {
  // verify leaf
  lss_hash leaf_hash;
  _leaf_hash(leaf_hash, leaf_y, leaf_w, los_u->counter);
  if ( c3n == lss_find_pair(los_u, 0, los_u->counter&1, leaf_hash) ) {
    return c3n;
  }
  // check whether pair is expected
  c3_w height = bits_ctz64(los_u->counter) + 1;
  if ((pair != NULL) != ((los_u->counter != 0) && (los_u->counter + (1 << height) < los_u->leaves))) {
    return c3n;
  } else if (pair == NULL) {
    los_u->counter++;
    return c3y;
  }
  // verify and insert pair
  c3_b sel = ~(los_u->counter >> height) & 1;
  lss_hash parent_hash;
  _parent_hash(parent_hash, (*pair)[0], (*pair)[1]);
  if ( c3n == lss_find_pair(los_u, height, sel, parent_hash) ) {
    return c3n;
  }
  memcpy(los_u->pairs[height-1][0], (*pair)[0], sizeof(lss_hash));
  memcpy(los_u->pairs[height-1][1], (*pair)[1], sizeof(lss_hash));
  los_u->counter++;
  return c3y;
}

c3_o lss_verifier_init(lss_verifier* los_u, c3_w leaves, lss_hash* proof, c3_w proof_w) {
  los_u->leaves = leaves;
  los_u->counter = 0;
  c3_w pairs_w = bits_len64(leaves);
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

static void _test_bits_ctz64()
{
  #define asrt(x,y) if ( x != y ) { fprintf(stderr, "failed (line: %u) got %u expected %u", __LINE__, x,y); exit(1); }
  asrt(bits_ctz64(0ULL), 64);
  asrt(bits_ctz64(0x0000000000000001ULL), 0);
  asrt(bits_ctz64(0x0000000000000002ULL), 1);
  asrt(bits_ctz64(0x0000000000000100ULL), 8);
  asrt(bits_ctz64(0x0000000080000000ULL), 31);
  asrt(bits_ctz64(0x0000000100000000ULL), 32);
  asrt(bits_ctz64(0x4000000000000000ULL), 62);
  asrt(bits_ctz64(0x8000000000000000ULL), 63);
  #undef asrt
}

static lss_pair* _make_pair(lss_hash left, lss_hash right)
{
  lss_pair* pair = c3_malloc(sizeof(lss_pair));
  memcpy((*pair)[0], left, 32);
  memcpy((*pair)[1], right, 32);
  return pair;
}

static void _test_lss_8()
{
  #define asrt_ok(y) if ( c3y != y ) { fprintf(stderr, "failed (line: %u)", __LINE__); exit(1); }

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
  asrt_ok(lss_verifier_init(&lss_u, 8, proof, 4))
  // for ( c3_w i = 0; i < 8; i++ ) {
  //   asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[i], leaves_w[i], pairs[i]))
  // }
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[0], leaves_w[0], pairs[0]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[1], leaves_w[1], pairs[1]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[2], leaves_w[2], pairs[2]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[3], leaves_w[3], pairs[3]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[4], leaves_w[4], pairs[4]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[5], leaves_w[5], pairs[5]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[6], leaves_w[6], pairs[6]))
  asrt_ok(lss_verifier_ingest(&lss_u, leaves_y[7], leaves_w[7], pairs[7]))

  #undef asrt_ok
}

int main() {
  _test_bits_ctz64();
  _test_lss_8();
  return 0;
}

#endif
