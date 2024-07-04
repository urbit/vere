#ifndef VERE_LSS_H
#define VERE_LSS_H

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"

c3_w lss_proof_size(c3_w leaves);

typedef c3_y lss_hash[32];

typedef lss_hash lss_pair[2];

typedef struct _lss_builder {
  lss_hash  trees[32];
  c3_w      counter;
  lss_hash  *proof;
  lss_pair  *pairs;
} lss_builder;

void lss_builder_ingest(lss_builder* bil_u, c3_y* leaf_y, c3_w leaf_w);

lss_hash* lss_builder_finalize(lss_builder* bil_u);

lss_pair* lss_builder_pair(lss_builder* bil_u, c3_w i);

void lss_builder_init(lss_builder* bil_u, c3_w leaves);

void lss_builder_free(lss_builder* bil_u);

typedef struct _lss_verifier {
  c3_w      leaves;
  c3_w      counter;
  lss_pair* pairs;
} lss_verifier;

c3_o lss_verifier_ingest(lss_verifier* ver_u, c3_y* leaf_y, c3_w leaf_w, lss_pair* pair);

c3_o lss_verifier_init(lss_verifier* ver_u, c3_w leaves, lss_hash* proof);

void lss_verifier_free(lss_verifier* ver_u);

void lss_complete_inline_proof(lss_hash* proof, c3_y* leaf_y, c3_w leaf_w);

void lss_root(lss_hash root, lss_hash* proof, c3_w proof_w);

#endif
