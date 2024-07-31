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
          c3_w len_a_w, len_b_w, i_w = 0;
          c3_w *a_words, *b_words;
          c3_w a_w, b_w;
          if ( c3y == u3a_is_cat(a) ) {
            len_a_w = 1;
            a_words = &a;
          }
          else {
            u3a_atom* a_u = u3a_to_ptr(a);
            len_a_w = (a_u->len_w);
            a_words = a_u->buf_w;
          }
          if ( c3y == u3a_is_cat(b) ) {
            len_b_w = 1;
            b_words = &b;
          }
          else {
            u3a_atom* b_u = u3a_to_ptr(b);
            len_b_w = (b_u->len_w);
            b_words = b_u->buf_w;
          }
          while ( (i_w < len_a_w) && (i_w < len_b_w) ) {
            c3_y a_y, b_y;
            a_w = a_words[i_w];
            b_w = b_words[i_w];
            for (c3_w j = 0; j < 4; j++) {
              a_y = a_w % (1 << 8);
              b_y = b_w % (1 << 8);
              if ( a_y != b_y ) return __(a_y < b_y);
              a_w = a_w >> 8;
              b_w = b_w >> 8;
            }
            i_w++;
          }
          if ( len_a_w == len_b_w) {
            return u3m_bail(c3__fail); // impossible: nonequal atoms with equal payloads 
          }
          return __(len_a_w < len_b_w);
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
    u3_noun out;
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

