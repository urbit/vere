/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  static u3_atom
  _aor_tramp(u3_noun a,
             u3_noun b)
  {
    if ( c3y == u3r_sing(a, b) ) {
      return c3y;
    }
    else {
      if ( c3y == u3ud(a) ) {
        if ( c3y == u3ud(b) ) {
          c3_w len_a, len_b, i = 0;
          c3_y *a_bytes = u3r_bytes_all(&len_a, a);
          c3_y *b_bytes = u3r_bytes_all(&len_b, b);
          while ( (i < len_a) && (i < len_b) ) {
            c3_y a_slice=a_bytes[i], b_slice=b_bytes[i];
            u3m_p("a_slice", a_slice);
            u3m_p("b_slice", b_slice);
            if (a_slice != b_slice) {
              return __(a_slice < b_slice);
            }
            i++;
          }
          return u3m_bail(c3__fail);
        }
        else {
          return c3y;
        }
      }
      else {
        if ( c3y == u3ud(b) ) {
          return c3n;
        }
        else {
          if ( c3y == u3r_sing(u3h(a), u3h(b)) ) {
            return 3;
          }
          else {
            return 2;
          }
        }
      }
    }
  }

  u3_noun
  u3qc_aor(u3_noun a,
           u3_noun b)
  {
    u3_noun out = 4;
    while ( 1 ) {
      out = _aor_tramp(a, b);
      if (out == 2) {
        a = u3h(a);
        b = u3h(b);
      }
      else if (out == 3) {
        a = u3t(a);
        b = u3t(b);
      }
      else {
        u3m_p("out", out);
        return out;
      }
    }
  }
  
  u3_noun
  u3wc_aor(u3_noun cor)
  {
    // return u3_none;
    u3_noun a, b;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_aor(a, b);
    }
  }

