/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_weak
u3qc_sew(u3_atom a,
         u3_atom b,
         u3_atom c,
         u3_atom d,
         u3_atom e
        )
{
  c3_w b_w, c_w;
  if (0 == c) return u3k(e);
  if ( !_(u3r_safe_word(b, &b_w)) ||
       !_(u3r_safe_word(c, &c_w)) ) {
    return u3_none;
  }
  if ( !_(u3a_is_cat(a)) || (a >= u3a_word_bits) ) {
    return u3m_bail(c3__fail);
  }

  c3_g a_g = a;

  //  a view borrows the words wherever the atom lives
  //
  c3_w        len_e_w = u3r_met(a_g, e);
  u3i_slab    sab_u;
  u3r_view    vue_u;
  const c3_w* src_w;
  c3_w        len_src_w;

  u3r_view_open(&vue_u, e, c3y);
  src_w = u3r_view_words(&vue_u, &len_src_w);
  u3i_slab_init(&sab_u, a_g, c3_max(len_e_w, b_w + c_w));
  u3r_chop_words(a_g, 0, b_w, 0, sab_u.buf_w, len_src_w, src_w);
  u3r_chop(a_g, 0, c_w, b_w, sab_u.buf_w, d);
  if (len_e_w > b_w + c_w) {
    u3r_chop_words(a_g,
             b_w + c_w,
             len_e_w - (b_w + c_w),
             b_w + c_w,
             sab_u.buf_w,
             len_src_w,
             src_w);
  }
  u3r_view_done(&vue_u);
  return u3i_slab_mint(&sab_u);
}

u3_weak
u3wc_sew(u3_noun cor)
{
  u3_noun a, b, c, d, e;
  a = u3h(u3h(u3t(cor)));
  b = u3h(u3h(u3t(u3h(u3t(cor)))));
  c = u3h(u3t(u3h(u3t(u3h(u3t(cor))))));
  d = u3t(u3t(u3h(u3t(u3h(u3t(cor))))));
  e = u3t(u3t(u3h(u3t(cor))));

  if ( (c3n == u3ud(a)) ||
       (c3n == u3ud(b)) ||
       (c3n == u3ud(c)) ||
       (c3n == u3ud(d)) ||
       (c3n == u3ud(e)) )
  {
    return u3m_bail(c3__fail);
  } else {
    return u3qc_sew(a, b, c, d, e);
  }
}
