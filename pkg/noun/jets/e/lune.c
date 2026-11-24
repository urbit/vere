/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qe_lune(u3_atom lub)
  {
    if (lub == 0) {
      return u3_nul;
    }

    //  one view for the scan and every line: a bob maps once, not per
    //  byte or line.  every index is within the atom
    //
    {
      c3_w        end_w  = u3r_met(3, lub) - 1;
      c3_w        pos_w  = end_w;
      u3_noun     lin = u3_nul;
      u3_noun     pro;
      u3r_view    vue_u;
      const c3_y* lub_y;

      u3r_view_open(&vue_u, lub, 0, c3y);
      lub_y = vue_u.byt_y;

      if (lub_y[pos_w] != 10) {
        u3r_view_done(&vue_u);
        return u3m_error("noeol");
      }

      if (pos_w == 0) {
        u3r_view_done(&vue_u);
        return u3nc(u3_nul, lin);
      }

      while (--pos_w) {
        if (lub_y[pos_w] == 10) {
          lin = u3nc(u3i_bytes((end_w - pos_w - 1), lub_y + pos_w + 1), lin);
          end_w = pos_w;
        }
      }

      if (lub_y[pos_w] == 10) {
        pro = u3nc(u3_nul,
                   u3nc(u3i_bytes((end_w - pos_w - 1), lub_y + pos_w + 1), lin));
      }
      else {
        pro = u3nc(u3i_bytes((end_w - pos_w), lub_y + pos_w), lin);
      }

      u3r_view_done(&vue_u);
      return pro;
    }
  }

  u3_noun
  u3we_lune(u3_noun cor)
  {
    u3_noun lub;

    if ( (u3_none == (lub = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(lub)) )
    {
      return u3m_bail(c3__fail);
    } else {
      return u3qe_lune(lub);
    }
  }
