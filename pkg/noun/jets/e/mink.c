/// @file

#include "allocate.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  static u3_noun
  _mink_in(u3_noun bus, u3_noun fol)
  {
    u3R->how.fag_w |= u3a_flag_mink;
    return u3n_nock_on(bus, fol);
  }

  u3_noun
  u3we_mink(u3_noun cor)
  {
    u3_noun bus, fol, gul;

    bus = u3h(u3h(u3h(u3t(cor))));
    fol = u3t(u3h(u3h(u3t(cor))));
    gul = u3t(u3h(u3t(cor)));
    u3_noun som;
    
    som = u3m_soft_run(u3k(gul), _mink_in, u3k(bus), u3k(fol));

    return som;
  }
