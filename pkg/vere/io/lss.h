#ifndef VERE_LSS_H
#define VERE_LSS_H

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur/ur.h"
#include "arena.h"

c3_d lss_proof_size(c3_d leaves);

typedef c3_y lss_hash[32];

typedef lss_hash lss_pair[2];

typedef struct _lss_builder {
  lss_hash  trees[32];
  c3_h      leaves;
  c3_h      counter;
  lss_hash  *proof;
  lss_pair  *pairs;
} lss_builder;

void lss_builder_ingest(lss_builder* bil_u, c3_y* leaf_y, c3_h leaf_h);

// Ingests a transceived leaf (consisting of 1<<steps normal leaves) and its
// pair. Returns the leaf index below which all transceived pairs are ready.
c3_h lss_builder_transceive(lss_builder* bil_u, c3_h steps, c3_y* jumbo_y, c3_h jumbo_h, lss_pair* pair);

// Computes any remaining pairs and proof hashes, and returns the latter. Memory
// is owned by the builder.
lss_hash* lss_builder_finalize(lss_builder* bil_u);

// Returns the pair (or NULL) for the i-th leaf. Memory is owned by the builder.
lss_pair* lss_builder_pair(lss_builder* bil_u, c3_h i);

void lss_builder_init(lss_builder* bil_u, c3_h leaves);

void lss_builder_free(lss_builder* bil_u);

// Transceives the provided proof in place. Returns a pointer into the same
// array, at proof[steps].
lss_hash* lss_transceive_proof(lss_hash* proof, c3_h steps);

typedef struct _lss_verifier {
  c3_h      steps;
  c3_h      leaves;
  c3_h      counter;
  lss_pair* pairs;
} lss_verifier;

c3_o lss_verifier_ingest(lss_verifier* ver_u, c3_y* leaf_y, c3_h leaf_h, lss_pair* pair);

void lss_verifier_init(lss_verifier* ver_u, c3_h steps, c3_h leaves, lss_hash* proof, arena* are_u);

// Hashes the provided leaf and writes it to proof[0]. The leaf may be any size.
void lss_complete_inline_proof(lss_hash* proof, c3_y* leaf_y, c3_h leaf_h);

// Recovers the root of an LSS Merkle tree from the provided proof.
void lss_root(lss_hash root, lss_hash* proof, c3_h proof_h);

#endif
