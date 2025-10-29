/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "urcrypt.h"
  
  // `@ux`(rev 3 32 l:ed:crypto)
  static c3_y _cqee_l_prime[] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 
  };

  u3_atom
  u3qee_recs(u3_atom a)
  {
    c3_w met_w = u3r_met(3, a);

    if ( 64 < met_w ) {
      u3_atom l_prime = u3i_bytes(32, _cqee_l_prime);
      u3_atom pro = u3qa_mod(a, l_prime);
      u3z(l_prime);
      return pro;
    }

    c3_y a_y[64];

    u3r_bytes(0, 64, a_y, a);
    urcrypt_ed_scalar_reduce(a_y);
    return u3i_bytes(32, a_y);
  }

  u3_noun
  u3wee_recs(u3_noun cor)
  {
    u3_noun a;

    if ( (u3_none == (a = u3r_at(u3x_sam, cor))) ||
         (c3n == u3ud(a)) )
    {
      return u3m_bail(c3__exit);
    } else {
      return u3qee_recs(a);
    }
  }
