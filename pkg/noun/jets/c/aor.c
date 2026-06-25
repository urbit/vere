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
          c3_o a_bob = ( c3y == u3a_is_cat(a) ) ? c3n : u3a_is_bob(a);
          c3_o b_bob = ( c3y == u3a_is_cat(b) ) ? c3n : u3a_is_bob(b);
          c3_w len_a_w = u3r_met(3, a);
          c3_w len_b_w = u3r_met(3, b);
          c3_w len_min_w = c3_min(len_a_w, len_b_w);

          //  bob atoms: buf_w holds a loom offset, not bytes, so read
          //  through u3r_view (mmap for bobs, heap copy for normal atoms).
          //
          if ( (c3y == a_bob) || (c3y == b_bob) ) {
            u3r_view va_u, vb_u;
            u3r_view_init(&va_u, a);
            u3r_view_init(&vb_u, b);
            u3_noun ret = u3_none;
            for ( c3_w i_w = 0; i_w < len_min_w; i_w++ ) {
              c3_y cut_a_y = va_u.byt_y[i_w];
              c3_y cut_b_y = vb_u.byt_y[i_w];
              if ( cut_a_y != cut_b_y ) {
                ret = __(cut_a_y < cut_b_y);
                break;
              }
            }
            u3r_view_done(&va_u);
            u3r_view_done(&vb_u);
            return ( u3_none == ret ) ? __(len_a_w < len_b_w) : ret;
          }

          {
            c3_y *buf_a_y, *buf_b_y;
            c3_y cut_a_y, cut_b_y;
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
            for (c3_w i_w = 0; i_w < len_min_w; i_w++) {
              cut_a_y = buf_a_y[i_w];
              cut_b_y = buf_b_y[i_w];
              if ( cut_a_y != cut_b_y ) return __(cut_a_y < cut_b_y);
            }
            return __(len_a_w < len_b_w);
          }
        }
      }
    }
  } 
  
  u3_noun
  u3wc_aor(u3_noun cor)
  {
    u3_noun a, b;

    if ( c3n == u3r_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b}) ) {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_aor(a, b);
    }
  }

