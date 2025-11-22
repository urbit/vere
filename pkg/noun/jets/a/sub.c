/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#if defined(__x86_64__)
#include <immintrin.h>
#endif

#ifdef __IMMINTRIN_H
#define _subborrow_w _subborrow_u32
#else
static inline c3_b
_subborrow_w(c3_b bor_b, c3_w a_w, c3_w b_w, c3_w* restrict c_w)
{
  c3_d dif_d = (c3_d)a_w - (c3_d)b_w - (c3_d)bor_b;
  *c_w = (c3_w)dif_d;
  return (c3_b)(dif_d >> 63);
}
#endif

static void
_sub_words(c3_w* a_buf_w,
           c3_w  a_len_w,
           c3_w* b_buf_w,
           c3_w  b_len_w,
           c3_w* restrict c_buf_w)
{
  c3_b bor_b = 0;

  for (c3_w i_w = 0; i_w < b_len_w; i_w++) {
    bor_b = _subborrow_w(bor_b, a_buf_w[i_w], b_buf_w[i_w], &c_buf_w[i_w]);
  }

  c3_w i_w = b_len_w;
  for (; i_w < a_len_w && bor_b; i_w++) {
    bor_b = _subborrow_w(bor_b, a_buf_w[i_w], 0, &c_buf_w[i_w]);
  }

  u3_assert( 0 == bor_b );
  memcpy(&c_buf_w[i_w], &a_buf_w[i_w], (a_len_w - i_w) << 2);
}

u3_noun
u3qa_sub(u3_atom a,
         u3_atom b)
{
  if ( _(u3a_is_cat(a)) && _(u3a_is_cat(b)) ) {
    if ( a < b ) {
      return u3m_error("subtract-underflow");
    }
    else {
      return (a - b);
    }
  }
  else if ( 0 == b ) {
    return u3k(a);
  }
  else {
    c3_ys cmp_ys = u3r_comp(a, b);
    if ( 0 == cmp_ys ) {
      return 0;
    }
    if ( -1 == cmp_ys ) {
      return u3m_error("subtract-underflow");
    }
    u3i_slab sab_u;
    c3_w *a_buf_w, *b_buf_w, *c_buf_w;
    c3_w  a_len_w, b_len_w;
    
    a_buf_w = u3r_word_buffer(&a, &a_len_w);
    b_buf_w = u3r_word_buffer(&b, &b_len_w);
    u3i_slab_init(&sab_u, 5, a_len_w);
    c_buf_w = sab_u.buf_w;

    _sub_words(a_buf_w, a_len_w, b_buf_w, b_len_w, c_buf_w);
    return u3i_slab_mint(&sab_u);
  }
}

u3_noun
u3wa_sub(u3_noun cor)
{
  u3_noun a, b;

  if ( (c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0)) ||
       (c3n == u3ud(b)) ||
       (c3n == u3ud(a)) )
  {
    return u3m_bail(c3__fail);
  }
  else {
    return u3qa_sub(a, b);
  }
}

u3_noun
u3ka_sub(u3_noun a,
         u3_noun b)
{
  u3_noun c = u3qa_sub(a, b);

  u3z(a); u3z(b);
  return c;
}
