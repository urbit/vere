/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqee_veri_octs(u3_noun sig,
             u3_noun len,
             u3_noun dat,
             u3_noun pub)
  {
    c3_y sig_y[64], pub_y[32];
    c3_d len_d;
    if ( (0 != u3r_bytes_fit(64, sig_y, sig)) ||
         (0 != u3r_bytes_fit(32, pub_y, pub)) ||
         (c3n == u3r_safe_chub(len, &len_d)) ) {
      return c3n;
    }
    else {
      c3_y*       cop_y = 0;
      const c3_y* dat_y;
      u3r_view    vue_u;

      u3r_view_open(&vue_u, dat, c3y);
      if ( len_d <= vue_u.byt_d ) {
        dat_y = u3r_view_bytes(&vue_u);
      }
      else if ( len_d <= c3_w_max ) {
        dat_y = cop_y = u3r_bytes_alloc(0, (c3_w)len_d, dat);
      }
      else {
        //  a width past the atom that no word holds: c3n, as develop's
        //  word check gives
        //
        u3r_view_done(&vue_u);
        return c3n;
      }

      c3_t val_t = urcrypt_ed_veri(dat_y, len_d, pub_y, sig_y);

      if ( cop_y ) {
        u3a_free(cop_y);
      }
      u3r_view_done(&vue_u);

      return val_t ? c3y : c3n;
    }
  }

  u3_noun
  u3wee_veri_octs(u3_noun cor)
  {
    u3_noun sig, msg, pub;
    u3_noun len, dat;
    if ( c3n == u3r_mean(cor,
                         {u3x_sam_2, &sig}, {u3x_sam_6, &msg},
                         {u3x_sam_7, &pub}) ||
         c3n == u3r_cell(msg, &len, &dat) ||
         (c3n == u3ud(sig)) ||
         (c3n == u3ud(pub)) ||
         (c3n == u3ud(len)) ||
         (c3n == u3ud(dat)) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_veri_octs(sig, len, dat, pub);
    }
  }

  static u3_atom
  _cqee_veri(u3_noun s,
             u3_noun m,
             u3_noun pk)
  {
    c3_y  sig_y[64], pub_y[32];

    if ( (0 != u3r_bytes_fit(64, sig_y, s)) ||
         (0 != u3r_bytes_fit(32, pub_y, pk)) ) {
      return c3n;
    }
    else {
      u3r_view vue_u;
      u3r_view_open(&vue_u, m, c3y);
      c3_t     val_t = urcrypt_ed_veri(u3r_view_bytes(&vue_u), vue_u.byt_d,
                                       pub_y, sig_y);
      u3r_view_done(&vue_u);

      return val_t ? c3y : c3n;
    }
  }

  u3_noun
  u3wee_veri(u3_noun cor)
  {
    u3_noun a, b, c;
    if ( (c3n == u3r_mean(cor,
                         {u3x_sam_2, &a}, {u3x_sam_6, &b},
                         {u3x_sam_7, &c})) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) ||
         (c3n == u3ud(c)) ) {
      return u3m_bail(c3__fail);
    } else {
      return _cqee_veri(a, b, c);
    }
  }
