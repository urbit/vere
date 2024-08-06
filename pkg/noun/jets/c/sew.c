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
  if (0 == c) return e;
  if ( !_(u3a_is_cat(b)) ||
       !_(u3a_is_cat(c)) ) {
    return u3_none;
  }
  if ( !_(u3a_is_cat(a)) || (a >= 32) ) {
    return u3m_bail(c3__fail);
  }

  c3_g a_g = a;
  c3_w b_w = b, c_w = c;
  c3_w len_e_w = u3r_met(a_g, e);
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, a_g, c3_max(len_e_w, b_w + c_w));
  u3r_chop(a_g, 0, b_w, 0, sab_u.buf_w, e);
  u3r_chop(a_g, 0, c_w, b_w, sab_u.buf_w, d);
  if (len_e_w > b_w + c_w) {
    u3r_chop(a_g,
             b_w + c_w,
             len_e_w - (b_w + c_w),
             b_w + c_w,
             sab_u.buf_w,
             e);
  }
  return u3i_slab_mint(&sab_u);
}

u3_weak
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
  } else {
    return u3qc_sew(u3x_atom(a),
                    u3x_atom(b),
                    u3x_atom(c),
                    u3x_atom(d),
                    u3x_atom(e));
  }
}