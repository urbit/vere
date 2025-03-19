/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"

  static u3_noun
  _cqe_sha1(u3_atom wid, u3_atom dat)
  {
    c3_n len_w;
    if ( !u3r_note_fit(&len_w, wid) ) {
      return u3m_bail(c3__fail);
    }
    else {
      c3_y out_y[20];
      c3_y *dat_y = u3r_bytes_alloc(0, len_w, dat);

      urcrypt_sha1(dat_y, len_w, out_y);
      u3a_free(dat_y);
      return u3i_bytes(20, out_y);
    }
  }

  u3_noun
  u3we_sha1(u3_noun cor)
  {
    u3_noun wid, dat;

    if ( (c3n == u3r_baad(cor, u3x_sam_2, &wid, u3x_sam_3, &dat, 0)) ||
         (c3n == u3ud(wid)) ||
         (c3n == u3ud(dat)) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return _cqe_sha1(wid, dat);
    }
  }
