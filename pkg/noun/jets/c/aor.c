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
          u3a_atom* a_u = u3a_to_ptr(a);
          u3a_atom* b_u = u3a_to_ptr(b);
          c3_w len_w_a = a_u->len_w, len_w_b = b_u->len_w;
          c3_w i_w = 0;
          c3_w *a_words = a_u->buf_w;
          c3_w *b_words = b_u->buf_w;
          while ( (i_w < len_w_a) && (i_w < len_w_b) ) {
            c3_w a_w = a_words[i_w], b_w = b_words[i_w];
            c3_y a_y = a_w & 0xFF, b_y = b_w & 0xFF;
            if (a_y != b_y) return __(a_y < b_y);

            a_y = a_w & 0xFF00;
            b_y = b_w & 0xFF00;
            if (a_y != b_y) return __(a_y < b_y);

            a_y = a_w & 0xFF0000;
            b_y = b_w & 0xFF0000;
            if (a_y != b_y) return __(a_y < b_y);

            a_y = a_w & 0xFF000000;
            b_y = b_w & 0xFF000000;
            if (a_y != b_y) return __(a_y < b_y);

            i_w++;
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
        return out;
      }
    }
  }
  
  u3_noun
  u3wc_aor(u3_noun cor)
  {
    u3_noun a, b;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_aor(a, b);
    }
  }

