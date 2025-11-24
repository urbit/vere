/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#if defined(__x86_64__)
#include <immintrin.h>
#endif

#ifdef __IMMINTRIN_H
#define _addcarry_w _addcarry_u32
#else
static inline c3_b
_addcarry_w(c3_b car_b, c3_w a_w, c3_w b_w, c3_w* restrict c_w)
{
  c3_d sum_d = (c3_d)car_b + (c3_d)a_w + (c3_d)b_w;
  *c_w = (c3_w)sum_d;
  return (c3_b)(sum_d >> 32);
}
#endif

static void
_add_words(c3_w* a_buf_w,
           c3_w  a_len_w,
           c3_w* b_buf_w,
           c3_w  b_len_w,
           c3_w* restrict c_buf_w)
{
  c3_w min_w = c3_min(a_len_w, b_len_w);
  c3_w max_w = c3_max(a_len_w, b_len_w);
  c3_b car_b = 0;

  for (c3_w i_w = 0; i_w < min_w; i_w++) {
    car_b = _addcarry_w(car_b, a_buf_w[i_w], b_buf_w[i_w], &c_buf_w[i_w]);
  }

  c3_w* rest_w = ( a_len_w < b_len_w ) ? b_buf_w : a_buf_w;

  c3_w i_w = min_w;
  for (; i_w < max_w && car_b; i_w++) {
    car_b = _addcarry_w(car_b, rest_w[i_w], 0, &c_buf_w[i_w]);
  }

  if ( car_b ) {
    c_buf_w[max_w] = 1;
  }
  else {
    memcpy(&c_buf_w[i_w], &rest_w[i_w], (max_w - i_w) << 2);
  }
}

u3_noun
u3qa_add(u3_atom a,
         u3_atom b)
{
  if ( _(u3a_is_cat(a)) && _(u3a_is_cat(b)) ) {
    c3_w c = a + b;

    return u3i_word(c);
  }
  else if ( 0 == a ) {
    return u3k(b);
  }
  else if ( 0 == b ) {
    return u3k(a);
  }
  else {
    u3i_slab sab_u;
    c3_w *a_buf_w, *b_buf_w, *c_buf_w;
    c3_w  a_len_w, b_len_w;

    a_buf_w = u3r_word_buffer(&a, &a_len_w);
    b_buf_w = u3r_word_buffer(&b, &b_len_w);
    //  u3i_slab_init(&sab_u, 5, c3_max(a_len_w, b_len_w) + 1);
    //  we have to do more measuring to avoid growing atom buffers on each
    //  addition as u3a_wtrim noops as of 3e8473d
    //
    c3_w a_met0_w = u3r_met(0, a),
         b_met0_w = u3r_met(0, b);
    u3i_slab_init(&sab_u, 0, c3_max(a_met0_w, b_met0_w) + 1);
    
    c_buf_w = sab_u.buf_w;

    _add_words(a_buf_w, a_len_w, b_buf_w, b_len_w, c_buf_w);
    return u3i_slab_mint(&sab_u);
  }
}

u3_noun
u3wa_add(u3_noun cor)
{
  u3_noun a, b;

  if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ||
       (c3n == u3ud(a)) ||
       (c3n == u3ud(b)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_add(a, b);
  }
}

u3_noun
u3ka_add(u3_noun a,
         u3_noun b)
{
  u3_noun c = u3qa_add(a, b);

  u3z(a); u3z(b);
  return c;
}
