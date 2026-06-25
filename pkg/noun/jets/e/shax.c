/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"


  static u3_atom
  _cqe_shay(u3_atom wid,
            u3_atom dat)
  {
    c3_w len_w;
    if ( !u3r_word_fit(&len_w, wid) ) {
      return u3m_bail(c3__fail);
    }

    c3_y out_y[32];
    u3r_view vue_u;
    u3r_view_padded(&vue_u, dat, len_w);
    urcrypt_shay((c3_y*)vue_u.byt_y, len_w, out_y);
    u3r_view_done(&vue_u);
    return u3i_bytes(32, out_y);
  }

  static u3_atom
  _cqe_shax(u3_atom a)
  {
    c3_y out_y[32];
    u3r_view vue_u;
    u3r_view_init(&vue_u, a);
    urcrypt_shay((c3_y*)vue_u.byt_y, vue_u.len_w, out_y);
    u3r_view_done(&vue_u);
    return u3i_bytes(32, out_y);
  }

  static u3_atom
  _cqe_shal(u3_atom wid,
            u3_atom dat)
  {
    c3_w len_w;
    if ( !u3r_word_fit(&len_w, wid) ) {
      return u3m_bail(c3__fail);
    }

    c3_y out_y[64];
    u3r_view vue_u;
    u3r_view_padded(&vue_u, dat, len_w);
    urcrypt_shal((c3_y*)vue_u.byt_y, len_w, out_y);
    u3r_view_done(&vue_u);
    return u3i_bytes(64, out_y);
  }

  static u3_atom
  _cqe_shas(u3_atom sal,
            u3_atom ruz)
  {
    c3_y out_y[32];

    u3r_view sa_u, ru_u;
    u3r_view_init(&sa_u, sal);
    u3r_view_init(&ru_u, ruz);

    urcrypt_shas((c3_y*)sa_u.byt_y, sa_u.len_w,
                 (c3_y*)ru_u.byt_y, ru_u.len_w, out_y);

    u3r_view_done(&sa_u);
    u3r_view_done(&ru_u);
    return u3i_bytes(32, out_y);
  }

  u3_noun
  u3we_shax(u3_noun cor)
  {
    u3_noun a;

    if ( (u3_none == (a = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shax(a);
    }
  }

  u3_noun
  u3we_shay(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (b = u3r_at(u3x_sam_3, cor))) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shay(a, b);
    }
  }

  u3_noun
  u3we_shal(u3_noun cor)
  {
    u3_noun a, b;

    if ( (u3_none == (a = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (b = u3r_at(u3x_sam_3, cor))) ||
         (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shal(a, b);
    }
  }

  u3_noun
  u3we_shas(u3_noun cor)
  {
    u3_noun sal, ruz;

    if ( (u3_none == (sal = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (ruz = u3r_at(u3x_sam_3, cor))) ||
         (c3n == u3ud(sal)) ||
         (c3n == u3ud(ruz)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return _cqe_shas(sal, ruz);
    }
  }

  static u3_noun
  _og_list(u3_noun a,
           u3_noun b,
           u3_noun c)
  {
    u3_noun l = u3_nul;

    if ( !_(u3a_is_cat(b)) ) {
      return u3m_bail(c3__fail);
    }
    while ( 0 != b ) {
      u3_noun x = u3qc_mix(a, c);
      u3_noun y = u3qc_mix(b, x);
      u3_noun d = _cqe_shas(c3_s4('o','g','-','b'), y);
      u3_noun m;

      u3z(x); u3z(y);

      if ( b < 256 ) {
        u3_noun e = u3qc_end(0, b, d);

        u3z(d);
        m = u3nc(b, e);
        b = 0;
      } else {
        m = u3nc(256, d);
        c = d;

        b -= 256;
      }
      l = u3nc(m, l);
    }
    return u3kb_flop(l);
  }

  u3_noun
  u3qeo_raw(u3_atom a,
            u3_atom b)
  {
    u3_noun x = u3qc_mix(b, a);
    u3_noun c = _cqe_shas(c3_s4('o','g','-','a'), x);
    u3_noun l = _og_list(a, b, c);
    u3_noun r = u3qc_can(0, l);

    u3z(l);
    u3z(c);
    u3z(x);

    return r;
  }

  u3_noun
  u3weo_raw(u3_noun cor)
  {
    u3_noun a, b;

    if ( c3n == u3r_mean(cor, {u3x_sam, &b}, {u3x_con_sam, &a}) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qeo_raw(a, b);
    }
  }
