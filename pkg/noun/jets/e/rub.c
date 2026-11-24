/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  /* _rub_bit(): bit [x] of the atom [vue_u] views, 0 past its end.
  */
  static c3_y
  _rub_bit(const u3r_view* vue_u, u3_atom x)
  {
    c3_d x_d;

    if ( (c3n == u3r_safe_chub(x, &x_d)) || (x_d >= vue_u->bit_d) ) {
      return 0;
    }
    return (vue_u->byt_y[x_d >> 3] >> (x_d & 7)) & 1;
  }
  u3_noun
  u3qe_rub(u3_atom a,
           u3_atom b)
  {
    u3_atom c, d, e;
    u3_atom w, x, y, z;
    u3_atom p, q;

    u3_atom m;
    {
      c3_w  bit_w = u3r_met(0, b);
      u3_noun bit = u3i_word(bit_w);
      m = u3qa_add(a, bit);
      u3z(bit);
    }

    //  Compute c and d.  one view of .b for the scan: a bob maps once,
    //  not per bit
    {
      u3r_view vue_u;

      u3r_view_open(&vue_u, b, 0, c3y);
      x = u3k(a);

      while ( 0 == _rub_bit(&vue_u, x) ) {
        u3_atom y = u3qa_inc(x);

        //  Sanity check: crash if decoding more bits than available
        if ( c3y == u3qa_gth(x, m)) {
          //  u3l_log("[%%rub-hard %d %d %d]", a, x, m);
          u3r_view_done(&vue_u);
          return u3m_bail(c3__exit);
        }

        u3z(x);
        x = y;
      }
      u3r_view_done(&vue_u);

      if ( c3y == u3r_sing(x, a) ) {
        u3z(x);
        u3z(m);
        return u3nc(1, 0);
      }
      c = u3qa_sub(x, a);
      d = u3qa_inc(x);

      u3z(x);
      u3z(m);
    }

    //  Compute e, p, q.
    {
      x = u3qa_dec(c);
      y = u3qc_bex(x);
      z = u3qc_cut(0, d, x, b);

      e = u3qa_add(y, z);
      u3z(y); u3z(z);

      w = u3qa_add(c, c);
      y = u3qa_add(w, e);
      z = u3qa_add(d, x);

      p = u3qa_add(w, e);
      q = u3qc_cut(0, z, e, b);

      u3z(c); u3z(d); u3z(e);
      u3z(w); u3z(x); u3z(y); u3z(z);

      return u3nc(p, q);
    }
  }
  u3_noun
  u3we_rub(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));

    if ( (c3n == u3ud(a)) ||
         (c3n == u3ud(b)) )
    {
      return u3m_bail(c3__fail);
    } else {
      return u3qe_rub(a, b);
    }
  }
