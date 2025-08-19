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

    {
      c3_n end_n  = u3r_met(3, lub) - 1;
      c3_n pos_n  = end_n;
      u3_noun lin = u3_nul;

      if (u3r_byte(pos_n, lub) != 10) {
        return u3m_error("noeol");
      }

      if (pos_n == 0) {
        return u3nc(u3_nul, lin);
      }

      while (--pos_n) {
        if (u3r_byte(pos_n, lub) == 10) {
          lin = u3nc(u3qc_cut(3, (pos_n + 1), (end_n - pos_n - 1), lub), lin);
          end_n = pos_n;
        }
      }

      if (u3r_byte(pos_n, lub) == 10) {
        return u3nc(u3_nul,
                    u3nc(u3qc_cut(3, (pos_n + 1), (end_n - pos_n - 1), lub), lin));
      }

      return u3nc(u3qc_cut(3, pos_n, (end_n - pos_n), lub), lin);
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
