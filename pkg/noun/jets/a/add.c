/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static void
_add_words(c3_w* a_buf_w,
           c3_w  a_len_w,
           c3_w* b_buf_w,
           c3_w  b_len_w,
           c3_w* restrict c_buf_w)
{
  c3_w min_w = c3_min(a_len_w, b_len_w);
  c3_w max_w = c3_max(a_len_w, b_len_w);
  c3_d sum_d = 0;

  for (c3_w i_w = 0; i_w < min_w; i_w++) {
    sum_d += (c3_d)a_buf_w[i_w] + (c3_d)b_buf_w[i_w];
    c_buf_w[i_w] = (c3_w)sum_d;
    sum_d >>= 32;
  }

  if ( a_len_w != b_len_w ) {
    c3_w* rest_w = ( a_len_w < b_len_w ) ? b_buf_w : a_buf_w;

    for (c3_w i_w = min_w; i_w < max_w; i_w++) {
      sum_d += rest_w[i_w];
      c_buf_w[i_w] = (c3_w)sum_d;
      sum_d >>= 32;
    }
  }

  if ( sum_d ) {
    c_buf_w[max_w] = (c3_w)sum_d;
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
    c3_w a_bit_w = u3r_met(0, a);
    c3_w b_bit_w = u3r_met(0, b);
    u3i_slab_init(&sab_u, 0, c3_max(a_bit_w, b_bit_w) + 1);

    c3_w *a_buf_w, *b_buf_w, *c_buf_w = sab_u.buf_w;
    c3_w  a_len_w, b_len_w;
    a_buf_w = u3r_word_buffer(&a, &a_len_w);
    b_buf_w = u3r_word_buffer(&b, &b_len_w);

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
