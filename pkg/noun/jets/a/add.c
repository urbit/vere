/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#if defined(__x86_64__)
#include <immintrin.h>
#endif

#ifdef __IMMINTRIN_H
  #ifdef VERE64
    #define _addcarry_w _addcarry_u64
    #define _addcarry_w_ptr(p) ((unsigned long long*)(p))
  #else
    #define _addcarry_w _addcarry_u32
    #define _addcarry_w_ptr(p) ((unsigned int*)(p))
  #endif
#else
  #ifdef VERE64
    static inline c3_b
    _addcarry_w(c3_b car_b, c3_w a_w, c3_w b_w, c3_w* restrict c_w)
    {
      c3_q sum = (c3_q)car_b + (c3_q)a_w + (c3_q)b_w;
      *c_w = (c3_w)sum;
      return (c3_b)(sum >> 64);
    }
  #else
    static inline c3_b
    _addcarry_w(c3_b car_b, c3_w a_w, c3_w b_w, c3_w* restrict c_w)
    {
      c3_d sum_d = (c3_d)car_b + (c3_d)a_w + (c3_d)b_w;
      *c_w = (c3_w)sum_d;
      return (c3_b)(sum_d >> 32);
    }
  #endif
  #define _addcarry_w_ptr(p)  (p)
#endif

static void
_add_words(const c3_w* a_buf_w,
           c3_w  a_len_w,
           const c3_w* b_buf_w,
           c3_w  b_len_w,
           c3_w* restrict c_buf_w)
{
  c3_w min_w = c3_min(a_len_w, b_len_w);
  c3_w max_w = c3_max(a_len_w, b_len_w);
  c3_b car_b = 0;

  for (c3_w i_w = 0; i_w < min_w; i_w++) {
    car_b = _addcarry_w(car_b,
                        a_buf_w[i_w],
                        b_buf_w[i_w],
                        _addcarry_w_ptr(&c_buf_w[i_w]));
  }

  const c3_w* rest_w = ( a_len_w < b_len_w ) ? b_buf_w : a_buf_w;

  c3_w i_w = min_w;
  for (; i_w < max_w && car_b; i_w++) {
    car_b = _addcarry_w(car_b, rest_w[i_w], 0, _addcarry_w_ptr(&c_buf_w[i_w]));
  }

  if ( car_b ) {
    c_buf_w[max_w] = 1;
  }
  else {
    memcpy(&c_buf_w[i_w], &rest_w[i_w], (max_w - i_w) * sizeof(c3_w));
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
    u3i_slab    sab_u;
    u3r_view    a_vue, b_vue;
    const c3_w *a_buf_w, *b_buf_w;
    c3_w        a_len_w, b_len_w;

    //  a view borrows the words wherever the atom lives; the views go
    //  with the road if the slab bails
    //
    u3r_view_open(&a_vue, a, c3y);
    u3r_view_open(&b_vue, b, c3y);
    a_buf_w = u3r_view_words(&a_vue, &a_len_w);
    b_buf_w = u3r_view_words(&b_vue, &b_len_w);

    //  sized in bits to avoid growing atom buffers on each addition,
    //  as u3a_wtrim noops as of 3e8473d
    //
    u3i_slab_init(&sab_u, 0, c3_max(a_vue.bit_d, b_vue.bit_d) + 1);

    _add_words(a_buf_w, a_len_w, b_buf_w, b_len_w, sab_u.buf_w);

    u3r_view_done(&a_vue);
    u3r_view_done(&b_vue);
    return u3i_slab_mint(&sab_u);
  }
}

u3_noun
u3wa_add(u3_noun cor)
{
  u3_noun a, b;

  a = u3h(u3h(u3t(cor)));
  b = u3t(u3h(u3t(cor)));

  if ( (c3n == u3ud(a)) ||
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
