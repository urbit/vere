/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

c3_d
u3qc_rig_s(c3_g foq_g,
           c3_w sep_w,
           c3_g toq_g)
{
  c3_d sep_d = sep_w;

  if ( foq_g >= toq_g ) {
    return sep_d << (foq_g - toq_g);
  }
  else {
    c3_g dif_g = toq_g - foq_g;

    sep_d += (1 << dif_g) - 1;
    return sep_d >> dif_g;
  }
}

u3_noun
u3qc_rig(u3_atom foq,
         u3_atom sep,
         u3_atom toq)
{
  if ( c3y == u3r_sing(foq, toq) ) {
    return u3k(sep);
  }

  if (  (c3y == u3a_is_cat(foq)) && (foq < 32)
     && (c3y == u3a_is_cat(toq)) && (toq < 32)
     && (c3y == u3a_is_cat(sep)) )
  {
    c3_d sep_d = u3qc_rig_s((c3_g)foq, (c3_w)sep, (c3_g)toq);
    return u3i_chub(sep_d);
  }

  if ( c3y == u3qa_gth(foq, toq) ) {
    u3_atom d = u3qa_sub(foq, toq);
    u3_atom e = u3qc_lsh(0, d, sep);
    u3z(d);
    return e;
  }
  else {
    u3_atom d = u3qa_sub(toq, foq);
    u3_atom e = u3qc_rsh(0, d, sep);
    u3_atom f = u3qc_end(0, d, sep);

    if ( f ) {
      e = u3i_vint(e);
      u3z(f);
    }

    u3z(d);
    return e;
  }
}

u3_noun
u3wc_rig(u3_noun cor)
{
  u3_atom boq, sep, vat;
  {
    u3_noun sam = u3h(u3t(cor));
    u3_noun bit = u3h(sam);

    if ( c3y == u3a_is_atom(bit) ) {
      return 0;
    }

    boq = u3x_atom(u3h(bit));
    sep = u3x_atom(u3t(bit));
    vat = u3x_atom(u3t(sam));
  }

  return u3qc_rig(boq, sep, vat);
}
