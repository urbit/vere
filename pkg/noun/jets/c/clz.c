/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

u3_atom
u3qc_clz(u3_atom boq, u3_atom sep, u3_atom a)
{
  if ( !_(u3a_is_cat(boq)) || (boq >= 32) ) {
    return u3m_bail(c3__fail);
  }

  if ( !_(u3a_is_cat(sep)) ) {
    return u3m_bail(c3__fail);
  }

  c3_g boq_g = boq;
  c3_w sep_w = sep;
  c3_w tot_w = sep_w << boq_g;

  if ( (tot_w >> boq_g) != sep_w ) {
    return u3m_bail(c3__fail);
  }

  c3_w met_w = u3r_met(0, a);

  if ( met_w <= tot_w ) {
    tot_w -= met_w;
    return u3i_word(tot_w);
  }
  else {
    c3_w wid_w = tot_w >>  5;
    c3_w bit_w = tot_w  & 31;
    c3_w wor_w;

    if ( bit_w ) {
      wor_w  = u3r_word(wid_w, a);
      wor_w &= (1 << bit_w) - 1;

      if ( wor_w ) {
        return bit_w - (32 - c3_lz_w(wor_w));
      }
    }

    while ( wid_w-- ) {
      wor_w = u3r_word(wid_w, a);

      if ( wor_w ) {
        bit_w += c3_lz_w(wor_w);
        break;
      }

      bit_w += 32;
    }

    return u3i_word(bit_w);
  }
}

u3_noun
u3wc_clz(u3_noun cor)
{
  u3_atom boq, sep, vat;
  {
    u3_noun sam = u3h(u3t(cor));
    u3_noun bit = u3h(sam);

    u3x_bite(bit, &boq, &sep);
    vat = u3x_atom(u3t(sam));
  }

  return u3qc_clz(boq, sep, vat);
}
