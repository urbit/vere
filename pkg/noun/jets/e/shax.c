/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"


  //  the hashes read the atom through a view: a bob's bytes are borrowed
  //  from its mapping, a loom atom's from the loom.  only a width past
  //  the atom is copied, zero-padded, as before.
  //
  static u3_atom
  _cqe_shay(u3_atom wid,
            u3_atom dat)
  {
    c3_d len_d;
    if ( c3n == u3r_safe_chub(wid, &len_d) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y        out_y[32];
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
        u3r_view_done(&vue_u);
        return u3m_bail(c3__fail);
      }

      urcrypt_shay(dat_y, len_d, out_y);
      if ( cop_y ) {
        u3a_free(cop_y);
      }
      u3r_view_done(&vue_u);
      return u3i_bytes(32, out_y);
    }
  }

  static u3_atom
  _cqe_shax(u3_atom a)
  {
    c3_y     out_y[32];
    u3r_view vue_u;
    u3r_view_open(&vue_u, a, c3y);
    urcrypt_shay(u3r_view_bytes(&vue_u), vue_u.byt_d, out_y);
    u3r_view_done(&vue_u);
    return u3i_bytes(32, out_y);
  }

  static u3_atom
  _cqe_shal(u3_atom wid,
            u3_atom dat)
  {
    c3_d len_d;
    if ( c3n == u3r_safe_chub(wid, &len_d) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y        out_y[64];
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
        u3r_view_done(&vue_u);
        return u3m_bail(c3__fail);
      }

      urcrypt_shal(dat_y, len_d, out_y);
      if ( cop_y ) {
        u3a_free(cop_y);
      }
      u3r_view_done(&vue_u);
      return u3i_bytes(64, out_y);
    }
  }

  static u3_atom
  _cqe_shas(u3_atom sal,
            u3_atom ruz)
  {
    c3_w sal_w, ruz_w;
    c3_y *sal_y, *ruz_y, out_y[32];

    sal_y = u3r_bytes_all(&sal_w, sal);
    ruz_y = u3r_bytes_all(&ruz_w, ruz);
    urcrypt_shas(sal_y, sal_w, ruz_y, ruz_w, out_y);
    u3a_free(sal_y);
    u3a_free(ruz_y);
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

    b = u3h(u3t(cor));
    a = u3h(u3t(u3t(u3t(cor))));
    return u3qeo_raw(a, b);
  }
