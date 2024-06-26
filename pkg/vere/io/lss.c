/// @file

#include "vere.h"
#include <stdio.h>
#include <types.h>
#include "urcrypt.h"
#include "lss.h"

static c3_y IV[32] = {103, 230, 9, 106, 133, 174, 103, 187, 114, 243, 110, 60, 58, 245, 79, 165, 127, 82, 14, 81, 140, 104, 5, 155, 171, 217, 131, 31, 25, 205, 224, 91};

static void _leaf_hash(c3_y out[32], c3_y* leaf, c3_w len, c3_d counter)
{
  c3_y cv[32];
  memcpy(cv, IV, 32);
  c3_y block[64] = {0};
  c3_y block_len = 0;
  c3_y flags = 0;
  urcrypt_blake3_chunk_output(len, leaf, cv, block, &block_len, &counter, &flags);
  urcrypt_blake3_compress(cv, block, block_len, counter, flags, out);
}

static void _parent_hash(c3_y out[32], c3_y left[32], c3_y right[32])
{
  c3_y cv[32];
  memcpy(cv, IV, 32);
  c3_y block[64];
  memcpy(block, left, 32);
  memcpy(block + 32, right, 32);
  c3_y block_len = 64;
  c3_y flags = 1 << 2; // PARENT
  urcrypt_blake3_compress(cv, block, block_len, 0, flags, out);
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

static c3_o lss_find_pair(lss_verifier* los_u, c3_w height, c3_b sel, c3_y h[32])
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
  c3_y leaf_hash[32];
  _leaf_hash(leaf_hash, leaf_y, leaf_w, los_u->counter);
  if (!(lss_find_pair(los_u, 0, los_u->counter&1, leaf_hash))) { // XX segfaults
    return c3n;
  }
  // check whether pair is expected
  c3_w height = bits_ctz64(los_u->counter) + 1;
  if ((pair != NULL) != ((los_u->counter == 0) || (los_u->counter + (1 << height) < los_u->leaves))) {
    return c3n;
  } else if (pair == NULL) {
    return c3y;
  }
  // verify and insert pair
  c3_b sel = ~(los_u->counter >> height) & 1;
  c3_y parent_hash[32];
  _parent_hash(parent_hash, (*pair)[0], (*pair)[1]);
  if (!lss_find_pair(los_u, height, sel, parent_hash)) {
    return c3n;
  }
  los_u->pairs[height-1] = pair;
  return c3y;
}

c3_o lss_verifier_init(lss_verifier* los_u, c3_w leaves, lss_hash* proof, c3_w proof_w) {
  if (proof_w < 2 || proof_w > bits_len64(leaves)) {
    return c3n;
  }
  los_u->leaves = leaves;
  los_u->counter = 0;
  los_u->pairs = c3_calloc(bits_len64(leaves) * sizeof(lss_pair*));
  memcpy(los_u->pairs[0][0], proof[0], 32);
  for (c3_w i = 1; i < proof_w; i++) {
    memcpy(los_u->pairs[i-1][1], proof[i], 32);
  }
  return c3y;
}

void lss_verifier_free(lss_verifier* los_u) {
  c3_free(los_u->pairs);
  c3_free(los_u);
}

void lss_complete_inline_proof(lss_hash* proof, c3_y* leaf_y, c3_w leaf_w) {
  c3_y leaf_hash[32];
  _leaf_hash(leaf_hash, leaf_y, leaf_w, 0);
  memcpy(proof, leaf_hash, 32);
}
