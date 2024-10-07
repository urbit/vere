#ifndef VERE_BITSET_H
#define VERE_BITSET_H

#include "c3/c3.h"

typedef struct _u3_bitset {
  c3_w  len_w;
  c3_y* buf_y;
} u3_bitset;

void bitset_init(u3_bitset* bit_u, c3_w len_w);

void bitset_free(u3_bitset* bit_u);

c3_w bitset_wyt(u3_bitset* bit_u);

void bitset_put(u3_bitset* bit_u, c3_w mem_w);

c3_o bitset_has(u3_bitset* bit_u, c3_w mem_w);

void bitset_del(u3_bitset* bit_u, c3_w mem_w);

#endif
