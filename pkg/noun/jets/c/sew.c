/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_noun
u3kc_sew(u3_atom a,
         u3_atom b,
         u3_atom c,
         u3_atom d,
         u3_atom e
        )
{
  if (0 == c) return e;

  c3_w b_w, c_w;
  if ( !_(u3r_safe_word(b, &b_w)) ||
       !_(u3r_safe_word(c, &c_w)) )
  {
    return u3m_bail(c3__fail);
  }

  c3_w end_w = b_w + c_w;
  if ( end_w < b_w )
  { 
    //  overflow
    //
    return u3m_bail(c3__fail);
  }

  if ( !_(u3a_is_cat(a)) || (a >= 32) )
  {
    return u3m_bail(c3__fail);
  }
  
  c3_g a_g = a;
  c3_w len_e_w = u3r_met(a_g, e);
  
  if ( _(u3a_is_mutable(u3R, e)) && (len_e_w >= end_w) )
  {
    u3a_atom* pug_u = u3a_to_ptr(e);
    c3_w* dst_w = pug_u->buf_w;
    //  XX fuse into one traversal
    //
    u3r_clear_bytes(a_g, c_w, b_w, (c3_y*)dst_w);
    u3r_chop(a_g, 0, c_w, b_w, dst_w, d);
    u3z(b), u3z(c), u3z(d);
    return e;
  }
  else
  {
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, a_g, c3_max(len_e_w, end_w));
    u3r_chop(a_g, 0, b_w, 0,   sab_u.buf_w, e);
    u3r_chop(a_g, 0, c_w, b_w, sab_u.buf_w, d);

    if (len_e_w > end_w)
    {
      u3r_chop(a_g, end_w, len_e_w - end_w, end_w, sab_u.buf_w, e);
    }
    u3z(b), u3z(c), u3z(d), u3z(e);
    return u3i_slab_mint(&sab_u);
  }
}

u3_noun
u3wc_sew(u3_noun cor)
{
  u3_noun a, b, c, d, e;
  if ( (c3n == u3r_mean(cor, u3x_sam_2,  &a,
                             u3x_sam_12, &b,
                                    106, &c,
                                    107, &d,
                              u3x_sam_7, &e, 0)) ||
       (c3n == u3ud(a)) ||
       (c3n == u3ud(b)) ||
       (c3n == u3ud(c)) ||
       (c3n == u3ud(d)) ||
       (c3n == u3ud(e)) )
  {
    return u3m_bail(c3__fail);
  }
  else
  {
    u3k(a), u3k(b), u3k(c), u3k(d), u3k(e);
    u3z(cor);
    return u3kc_sew(a, b, c, d, e);
  }
}
