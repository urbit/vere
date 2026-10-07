/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  // @Refcount: direct product
  u3_noun
  u3qc_dor(u3_noun a,
           u3_noun b)
  {
    if ( c3y == u3r_sing(a, b) ) {
      return c3y;
    }
    else {
      if ( c3y == u3ud(a) ) {
        if ( c3y == u3ud(b) ) {
          return u3qa_lth(a, b);
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
            return u3qc_dor(u3t(a), u3t(b));
          }
          else return u3qc_dor(u3h(a), u3h(b));
        }
      }
    }
  }
  u3_noun
  u3wc_dor(u3_noun cor)
  {
    u3_noun a, b;

    a = u3h(u3h(u3t(cor)));
    b = u3t(u3h(u3t(cor)));
    return u3qc_dor(a, b);
  }

