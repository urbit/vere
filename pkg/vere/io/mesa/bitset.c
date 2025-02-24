#include "bitset.h"
#include <stdio.h>

#include "vere.h"

void bitset_init(u3_bitset* bit_u, c3_w len_w, arena* are_u)
{
  bit_u->len_w = len_w;
  bit_u->buf_y = new(are_u, c3_y, (len_w >> 3) + 1);
  memset(bit_u->buf_y, 0, (len_w >> 3) + 1);
}

static c3_y
_popcnt(c3_y num_y)
{
  return __builtin_popcount(num_y);
}

static void
_log_bitset(u3_bitset* bit_u)
{
  c3_w cur_w = 0;
  while( cur_w < bit_u->len_w ) {
    if ( c3y == bitset_has(bit_u, cur_w) ) {
      u3l_log("%u", cur_w);
    }
    cur_w++;
  }
}

c3_w
bitset_wyt(u3_bitset* bit_u)
{
  c3_w ret_w = 0;
  c3_w len_w = (bit_u->len_w >> 3);
  for(int i = 0; i < len_w; i++ ) {
    ret_w += _popcnt(bit_u->buf_y[i]);
  }
  return ret_w;
}

void bitset_put(u3_bitset* bit_u, c3_w mem_w)
{
  if (( mem_w > bit_u->len_w )) {
    u3l_log("overrun %u, %u", mem_w, bit_u->len_w);
    return;
  }
  c3_w idx_w = mem_w >> 3;
  c3_w byt_y = bit_u->buf_y[idx_w];
  c3_y rem_y = mem_w & 0x7;
  c3_y mas_y = (1 << rem_y);
  bit_u->buf_y[idx_w] = byt_y | mas_y;
}

c3_o
bitset_has(u3_bitset* bit_u, c3_w mem_w) {
  if (( mem_w > bit_u->len_w )) {
    u3l_log("overrun %u, %u", mem_w, bit_u->len_w);
    return c3n;
  }

  u3_assert( mem_w < bit_u->len_w );
  c3_w idx_w = mem_w >> 3;
  c3_y rem_y = mem_w & 0x7;
  return __( (bit_u->buf_y[idx_w] >> rem_y) & 0x1);
}

void
bitset_del(u3_bitset* bit_u, c3_w mem_w)
{
  u3_assert( mem_w < bit_u->len_w );
  c3_w idx_w = mem_w >> 3;
  c3_w byt_y = bit_u->buf_y[idx_w];
  c3_y rem_y = mem_w & 0x7;
  c3_y mas_y = ~(1 << rem_y);
  bit_u->buf_y[idx_w] &= mas_y;
}




#ifdef BITSET_TEST
c3_w main()
{
  u3_bitset bit_u;
  bitset_init(&bit_u, 500);

  bitset_put(&bit_u, 5);
  bitset_put(&bit_u, 50);
  bitset_put(&bit_u, 100);

  c3_w wyt_w = bitset_wyt(&bit_u);
  if ( 3 != wyt_w ) {
    u3l_log("wyt failed have %u expect %u", wyt_w, 3);
    exit(1);
  }

  if ( c3y == bitset_has(&bit_u, 3) ) {
    u3l_log("false positive for has_bitset");
    exit(1);
  }

  if ( c3n == bitset_has(&bit_u, 50) ) {
    u3l_log("false negative for has_bitset");
    exit(1);
  }

  bitset_del(&bit_u, 50);

  if ( c3y == bitset_has(&bit_u, 50) ) {
    u3l_log("false positive for has_bitset");
    exit(1);
  }

  wyt_w = bitset_wyt(&bit_u);

  if ( 2 != wyt_w ) {
    u3l_log("wyt failed have %u expect %u", wyt_w, 2);
    exit(1);
  }
  return 0;
}

#endif
