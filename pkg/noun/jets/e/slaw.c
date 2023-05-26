/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <ctype.h>

u3_noun
_parse_tas(u3_noun txt)
{
    c3_w len_w = u3r_met(3, txt);

    if ( !len_w ) {
        return u3_none;
    }

    // For any symbol which matches, txt will return itself as a
    // value. Therefore, this is mostly checking validity.
    //

    c3_w i_w;
    c3_c c;

    // First character must represent a lowercase letter
    //
    c = u3r_byte(0, txt);

    if (!islower(c)) {
    return u3_none;
    }

    for ( i_w = 1; i_w < len_w; i_w++ ) {

      c = (c3_y) u3r_byte(i_w, txt);

      if (!(islower(c) || isdigit(c) || c == '-')) {
          return u3_none;
      }
    }

    return u3k(txt);
}

u3_noun
u3qe_slaw(u3_atom a, u3_atom b)
{
  u3_noun res;

  switch(a) {

    case c3__da: res = u3s_sift_da(b); break;

    case 'p': res = u3s_sift_p(b);     break;

    case c3__ud: res = u3s_sift_ud(b); break;
    case c3__ui: res = u3s_sift_ui(b); break;
    case c3__uv: res = u3s_sift_uv(b); break;
    case c3__uw: res = u3s_sift_uw(b); break;
    case c3__ux: res = u3s_sift_ux(b); break;

    // %ta is used once in link.hoon. don't bother.

    case c3__tas: {
                      res = _parse_tas(b);
                      if ( res == u3_none ) {
                          return u3_nul;
                      }
                  }


    default: return u3_none;
  }

  // The u3s_sift functions
  // signal parsing failure by returning u3_none.
  // This does not mean the input was wrong - the jet
  // could simply choose not to handle certain cases.
  //
  if ( res == u3_none ) {
    return u3_none;
  }

  return u3nc(u3_nul, res);
}

u3_noun
u3we_slaw(u3_noun cor)
{
  u3_noun a;
  u3_noun b;

  if (c3n == u3r_mean(cor, u3x_sam_2, &a,
                      u3x_sam_3, &b, 0)) {
    return u3m_bail(c3__exit);
  }

  return u3qe_slaw(u3x_atom(a), u3x_atom(b));
}
