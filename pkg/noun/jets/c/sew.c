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
  c3_w len_e_w = u3r_met(a_g, e);
  u3i_slab sab_u;
  c3_w* src_w;
  c3_w len_src_w;

  //  view is only used for the bob path; done() is a no-op on a
  //  zero-init struct, so we can unconditionally call it at the end.
  //
  u3r_view vue_u = {0};

  if ( _(u3a_is_cat(e)) ) {
    len_src_w = e ? 1 : 0;
    src_w = &e;
  }
  else if ( c3y == u3a_is_bob(e) ) {
    //  zero-copy view on the blob file.  the legacy src_u->buf_w
    //  cast would have returned seq_w for bobs; mmap is page-aligned
    //  (therefore word-aligned) and POSIX guarantees bytes past EOF
    //  within the mapped last page read as zero, so the word-at-a-time
    //  read by u3r_chop_words is safe.
    //
    u3r_view_init(&vue_u, e);
    len_src_w = (vue_u.len_w + u3a_word_bytes - 1) >> u3a_word_bytes_shift;
    src_w = (c3_w*)vue_u.byt_y;
  }
  else {
    u3a_atom* src_u = u3a_to_ptr(e);
    len_src_w = src_u->len_w;
    src_w = src_u->buf_w;
  }

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
  if ( (c3n == u3r_mean(cor, {u3x_sam_2,  &a},
                             {u3x_sam_12, &b},
                                    {106, &c},
                                    {107, &d},
                              {u3x_sam_7, &e})) ||
       (c3n == u3ud(a)) ||
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
