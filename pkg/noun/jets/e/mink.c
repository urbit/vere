/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

  u3_noun
  u3we_mink(u3_noun cor)
  {
    u3_noun bus, fol, gul;

    bus = u3h(u3h(u3h(u3t(cor))));
    fol = u3t(u3h(u3h(u3t(cor))));
    gul = u3t(u3h(u3t(cor)));
    u3_noun som;

    som = u3n_nock_et(u3k(gul), u3k(bus), u3k(fol));

    return som;
  }
