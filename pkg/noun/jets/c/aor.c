/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  u3_noun
  u3qc_aor(u3_noun a,
           u3_noun b)
  {
    while ( 1 ) {
      if ( c3y == u3r_sing(a, b) ) return c3y;
      if ( c3n == u3ud(a) ) {
        if ( c3y == u3ud(b) ) return c3n;
        if ( c3y == u3r_sing(u3h(a), u3h(b)) ) {
          a = u3t(a);
          b = u3t(b);
        }
        else {
          a = u3h(a);
          b = u3h(b);
        }
      }
      else {
        if ( c3n == u3ud(b) ) return c3y;
        {
          //  views borrow the bytes wherever the atoms live
          //
          u3r_view a_vue, b_vue;
          u3r_view_open(&a_vue, a, c3y);
          u3r_view_open(&b_vue, b, c3y);
          if ( (a_vue.byt_d > c3_w_max) || (b_vue.byt_d > c3_w_max) ) {
            return u3m_bail(c3__fail);
          }

          c3_w len_a_w = (c3_w)a_vue.byt_d;
          c3_w len_b_w = (c3_w)b_vue.byt_d;
          const c3_y *buf_a_y = u3r_view_bytes(&a_vue);
          const c3_y *buf_b_y = u3r_view_bytes(&b_vue);
          c3_y cut_a_y = 0, cut_b_y = 0;
          c3_o ret_o;
          c3_w len_min_w = c3_min(len_a_w, len_b_w);
          c3_w i_w;
          for (i_w = 0; i_w < len_min_w; i_w++) {
            cut_a_y = buf_a_y[i_w];
            cut_b_y = buf_b_y[i_w];
            if ( cut_a_y != cut_b_y ) break;
          }
          ret_o = ( i_w < len_min_w ) ? __(cut_a_y < cut_b_y)
                                      : __(len_a_w < len_b_w);
          u3r_view_done(&a_vue);
          u3r_view_done(&b_vue);
          return ret_o;
        }
      }
    }
  } 
  
  u3_noun
  u3wc_aor(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));
    return u3qc_aor(a, b);
  }

