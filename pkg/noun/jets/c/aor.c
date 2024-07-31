/// @file

#include "jets/q.h"
#include "jets/w.h"
#include <manage.h>
#include "noun.h"

// returns 0 if yes
// 1 if no 
// 2 if indeterminate
static c3_y _aor_word(c3_w yay_w, c3_w bob_w)
{
  c3_w i_w = 0;
  c3_y res_y = 2;
  for ( i_w = 0; i_w < 4; i_w++ ) {
    c3_y yay_y = (yay_w >> (i_w*8)) & 0xFF;
    c3_y bob_y = (bob_w >> (i_w*8)) & 0xFF;
    if ( yay_y == bob_y ) {
      continue;
    }
    res_y = ( yay_y < bob_y ) ? c3y : c3n;
    break;
  }
  return res_y;
}



 u3_noun
u3qc_aor(u3_noun a,
         u3_noun b)
{
  if ( c3y == u3r_sing(a, b) ) {
    return c3y;
  }
  else if ( c3n == u3ud(a) ) {
    if ( c3y == u3ud(b) ) {
      return c3n;
    } else if ( c3y == u3r_sing(u3h(a), u3h(b)) ) {
      __attribute__((musttail)) return u3qc_aor(u3t(a), u3t(b));
    }
    else {
      __attribute__((musttail)) return u3qc_aor(u3h(a), u3h(b));
    }
  } else if ( c3n == u3ud(b) ) {
    return c3y;
  } else {

    c3_w wen_w = 0;
    c3_w sia_w = u3r_met(3, a);
    c3_w sib_w = u3r_met(3, b);
    c3_w siz_w = sia_w > sib_w ? sia_w : sib_w;
    
    if ( (c3y == u3a_is_cat(a)) || (c3y == u3a_is_cat(b)) ) {
      c3_w i_w = 0;
      c3_w yay_w = u3r_word(0, a);
      c3_w bob_w = u3r_word(0, b);
      c3_y res_y = _aor_word(yay_w, bob_w);
      if ( res_y == 2 ) {
        return sia_w < sib_w ? c3y : c3n;
      }
      return res_y;
    } else { // if ( c3n == u3a_is_cat(a) &&  c3n == u3a_is_cat(b) ) {
      c3_w i_w = 0;
      u3a_atom* yay_u = ((u3a_atom *)u3a_to_ptr(a));
      u3a_atom* bob_u = ((u3a_atom *)u3a_to_ptr(b));
      c3_w siz_w = 
        bob_u->len_w > yay_u->len_w ? yay_u->len_w : bob_u->len_w;
      c3_y res_y = 2;
      for (i_w = 0; i_w < siz_w; i_w++ ) {
        res_y = _aor_word(yay_u->buf_w[i_w], bob_u->buf_w[i_w]);
        if ( res_y != 2 ) {
          //fprintf(stderr, "found diff on (%u, %u)\r\n", yay_u->buf_w[i_w], bob_u->buf_w[i_w]);
          break;
        }
      }
      if ( res_y == 2 ) {
        return sia_w < sib_w ? c3y : c3n;
      }
      return (c3_o)res_y;
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

