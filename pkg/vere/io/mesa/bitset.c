#include "bitset.h"
#include <stdio.h>

#include "vere.h"

void bitset_init(u3_bitset* bit_u, c3_d len_d, arena* are_u)
{
  u3_assert( UINT32_MAX >= len_d );
  bit_u->len_h = len_d;
  bit_u->buf_y = new(are_u, c3_y, (len_d >> 3) + 1);
  memset(bit_u->buf_y, 0, (len_d >> 3) + 1);
}

static c3_y
_popcnt(c3_y num_y)
{
  return __builtin_popcount(num_y);
}

static void
_log_bitset(u3_bitset* bit_u)
{
  c3_h cur_h = 0;
  while( cur_h < bit_u->len_h ) {
    if ( c3y == bitset_has(bit_u, cur_h) ) {
      u3l_log("%u", cur_h);
    }
    cur_h++;
  }
}

c3_h
bitset_wyt(u3_bitset* bit_u)
{
  c3_h ret_h = 0;
  c3_h len_h = (bit_u->len_h >> 3);
  for(int i = 0; i < len_h; i++ ) {
    ret_h += _popcnt(bit_u->buf_y[i]);
  }
  return ret_h;
}

void bitset_put(u3_bitset* bit_u, c3_h mem_h)
{
  if (( mem_h > bit_u->len_h )) {
    u3l_log("overrun %u, %u", mem_h, bit_u->len_h);
    return;
  }
  c3_h idx_h = mem_h >> 3;
  c3_h byt_h = bit_u->buf_y[idx_h];
  c3_y rem_y = mem_h & 0x7;
  c3_y mas_y = (1 << rem_y);
  bit_u->buf_y[idx_h] = byt_h | mas_y;
}

c3_o
bitset_has(u3_bitset* bit_u, c3_h mem_h) {
  if (( mem_h > bit_u->len_h )) {
    u3l_log("overrun %u, %u", mem_h, bit_u->len_h);
    return c3n;
  }

  u3_assert( mem_h < bit_u->len_h );
  c3_h idx_h = mem_h >> 3;
  c3_y rem_y = mem_h & 0x7;
  return __( (bit_u->buf_y[idx_h] >> rem_y) & 0x1);
}

void
bitset_del(u3_bitset* bit_u, c3_h mem_h)
{
  u3_assert( mem_h < bit_u->len_h );
  c3_h idx_h = mem_h >> 3;
  c3_h byt_h = bit_u->buf_y[idx_h];
  c3_y rem_y = mem_h & 0x7;
  c3_y mas_y = ~(1 << rem_y);
  bit_u->buf_y[idx_h] &= mas_y;
}




#ifdef BITSET_TEST
c3_h main()
{
  u3_bitset bit_u;
  bitset_init(&bit_u, 500);

  bitset_put(&bit_u, 5);
  bitset_put(&bit_u, 50);
  bitset_put(&bit_u, 100);

  c3_h wyt_h = bitset_wyt(&bit_u);
  if ( 3 != wyt_h ) {
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

  wyt_h = bitset_wyt(&bit_u);

  if ( 2 != wyt_h ) {
    u3l_log("wyt failed have %u expect %u", wyt_h, 2);
    exit(1);
  }
  return 0;
}

#endif
