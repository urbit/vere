#ifndef VERE_BLAKE_H
#define VERE_BLAKE_H

#include "vere.h"
#include "ivory.h"

#include "noun.h"
#include "ur.h"

#define BLAKE3_BLOCK_LEN  64
#define BLAKE3_OUT_LEN  32

enum bao_ingest_result {
  BAO_BAD_ORDER = 1,
  BAO_FAILED = 2,
  BAO_SUCCESS = 3
};



typedef struct _blake_pair {
  c3_y sin_y[BLAKE3_OUT_LEN];
  c3_y dex_y[BLAKE3_OUT_LEN];
} blake_pair;

typedef struct _u3_raw_vec {
  c3_w siz_w;
  c3_w len_w;
  void** vod_p;
} u3_raw_vec;

#define u3_vec(type) u3_raw_vec


typedef struct _blake_node {
  c3_y  cev_y[BLAKE3_OUT_LEN];
  c3_y  boq_y[BLAKE3_BLOCK_LEN];
  c3_d  con_d;
  c3_y  len_y;
  c3_y  fag_y;
} blake_node;

typedef struct _blake_subtree {
  c3_d sin_d;
  c3_d dex_d;
} blake_subtree;

typedef struct _blake_verifier {
	c3_d            num_d;
  c3_w            con_w;
	blake_subtree   sub_u;
  u3_vec(c3_y[BLAKE3_OUT_LEN]) que_u;
  u3_vec(c3_w[BLAKE3_OUT_LEN]) sta_u;
} blake_verifier;

typedef struct _blake_bao {
  c3_w                con_w;
  u3_vec(blake_pair)  par_u;
  blake_verifier      ver_u;
} blake_bao;


blake_node* blake_leaf_hash(c3_y* dat_y, c3_w dat_w, c3_d con_d);

blake_bao*  blake_bao_make(c3_w, c3_y[BLAKE3_BLOCK_LEN], u3_vec(c3_y[8])*, u3_vec(blake_pair)*);

c3_y bao_ingest(blake_bao*, c3_w, c3_y*, c3_w);

#endif