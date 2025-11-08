/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static void
_sub_words(c3_w* a_buf_w,
           c3_w  a_len_w,
           c3_w* b_buf_w,
           c3_w  b_len_w,
           c3_w* restrict c_buf_w)
{
  c3_d dif_d, bor_d = 0;

  for (c3_w i_w = 0; i_w < b_len_w; i_w++) {
    dif_d = (c3_d)a_buf_w[i_w] - (c3_d)b_buf_w[i_w] - bor_d;
    c_buf_w[i_w] = (c3_w)dif_d;
    bor_d = dif_d >> 63;
  }

  for (c3_w i_w = b_len_w; i_w < a_len_w; i_w++) {
    dif_d = (c3_d)a_buf_w[i_w] - bor_d;
    c_buf_w[i_w] = (c3_w)dif_d;
    bor_d = dif_d >> 63;
  }
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
    if ( _(u3qa_lth(a, b)) ) {
      return u3m_error("subtract-underflow");
    }
    if ( a == b ) {
      return 0;
    }
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, 0, u3r_met(0, a));
    
    c3_w *a_buf_w, *b_buf_w, *c_buf_w = sab_u.buf_w;
    c3_w  a_len_w, b_len_w;
    a_buf_w = u3r_word_buffer(&a, &a_len_w);
    b_buf_w = u3r_word_buffer(&b, &b_len_w);

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
