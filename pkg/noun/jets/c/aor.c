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
          //  a bob's bytes live in a file: load it onto the loom
          //
          u3_atom lom_a = u3_none, lom_b = u3_none;
          if ( c3y == u3a_is_bob(a) ) {
            if ( u3_none == (lom_a = u3r_blob_load(a)) ) {
              return u3m_bail(c3__fail);
            }
            a = lom_a;
          }
          if ( c3y == u3a_is_bob(b) ) {
            if ( u3_none == (lom_b = u3r_blob_load(b)) ) {
              return u3m_bail(c3__fail);
            }
            b = lom_b;
          }

          c3_w len_a_w = u3r_met(3, a);
          c3_w len_b_w = u3r_met(3, b);;
          c3_y *buf_a_y, *buf_b_y;
          c3_y cut_a_y = 0, cut_b_y = 0;
          c3_o ret_o;
          if ( c3y == u3a_is_cat(a) ) {
            buf_a_y = (c3_y*)&a;
          }
          else {
            u3a_atom* a_u = u3a_to_ptr(a);
            buf_a_y = (c3_y*)(a_u->buf_w);
          }
          if ( c3y == u3a_is_cat(b) ) {
            buf_b_y = (c3_y*)&b;
          }
          else {
            u3a_atom* b_u = u3a_to_ptr(b);
            buf_b_y = (c3_y*)(b_u->buf_w);
          }
          c3_w len_min_w = c3_min(len_a_w, len_b_w);
          c3_w i_w;
          for (i_w = 0; i_w < len_min_w; i_w++) {
            cut_a_y = buf_a_y[i_w];
            cut_b_y = buf_b_y[i_w];
            if ( cut_a_y != cut_b_y ) break;
          }
          ret_o = ( i_w < len_min_w ) ? __(cut_a_y < cut_b_y)
                                      : __(len_a_w < len_b_w);
          if ( u3_none != lom_a ) u3z(lom_a);
          if ( u3_none != lom_b ) u3z(lom_b);
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

