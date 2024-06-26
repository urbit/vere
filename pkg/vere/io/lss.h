#ifndef VERE_LSS_H
#define VERE_LSS_H

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"

typedef c3_y lss_hash[32];

typedef lss_hash lss_pair[2];

typedef struct _lss_verifier {
  c3_w      leaves;
  c3_w      counter;
  lss_pair* pairs;
} lss_verifier;

c3_o lss_verifier_ingest(lss_verifier* ver_u, c3_y* leaf_y, c3_w leaf_w, lss_pair* pair);

c3_o lss_verifier_init(lss_verifier* ver_u, c3_w leaves, lss_hash* proof, c3_w proof_w);

void lss_verifier_free(lss_verifier* ver_u);

void lss_complete_inline_proof(lss_hash* proof, c3_y* leaf_y, c3_w leaf_w);

void lss_root(lss_hash root, lss_hash* proof, c3_w proof_w);

#endif
