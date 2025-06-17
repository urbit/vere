/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qc_xeb(u3_atom a)
  {
    c3_n met_n = u3r_met(0, a);

    if ( !_(u3a_is_cat(met_n)) ) {
      return u3i_note(met_n);
    }
    else return met_n;
  }
  u3_noun
  u3wc_xeb(u3_noun cor)
  {
    u3_noun a;

    if ( (u3_none == (a = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qc_xeb(a);
    }
  }

