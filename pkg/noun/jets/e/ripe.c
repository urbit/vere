/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_atom
  _cqe_ripe(u3_atom wid, u3_atom dat)
  {
    c3_w len_w;
    if ( !u3r_word_fit(&len_w, wid) ) {
      return u3m_bail(c3__fail);
    }

    c3_y out_y[20];
    u3r_view vue_u;
    u3r_view_padded(&vue_u, dat, len_w);

    u3_atom ret = ( 0 == urcrypt_ripemd160((c3_y*)vue_u.byt_y, len_w, out_y) )
                ? u3i_bytes(20, out_y)
                : u3_none;

    u3r_view_done(&vue_u);
    return ret;
  }

  u3_noun
  u3we_ripe(u3_noun cor)
  {
    u3_noun wid, dat;

    if ( (c3n == u3r_mean(cor, {u3x_sam_2, &wid},
                               {u3x_sam_3, &dat}) ||
                 u3ud(wid) || u3ud(dat))
       )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3l_punt("ripe", _cqe_ripe(wid, dat));
    }
  }
