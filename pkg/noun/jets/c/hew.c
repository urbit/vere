/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static c3_w
_hew_in(c3_g     a_g,
        c3_w   pos_w,
        u3_atom  vat,
        u3_noun  sam,
        u3_noun* out)
{
  u3_noun h, t, *l, *r;

  while ( c3y == u3r_cell(sam, &h, &t) ) {
    *out  = u3i_defcons(&l, &r);
    pos_w = _hew_in(a_g, pos_w, vat, h, l);
    sam   = t;
    out   = r;
  }

  if ( !_(u3a_is_cat(sam)) ) {
    return u3m_bail(c3__fail);
  }

  if ( !sam ) {
    *out = 0;
    return pos_w;
  }
  else {
    c3_w     wid_w = (c3_w)sam;
    c3_w     new_w = pos_w + wid_w;
    u3i_slab sab_u;

    if ( new_w < pos_w ) {
      return u3m_bail(c3__fail);
    }

    u3i_slab_init(&sab_u, a_g, wid_w);
    u3r_chop(a_g, pos_w, wid_w, 0, sab_u.buf_w, vat);

    *out = u3i_slab_mint(&sab_u);
    return new_w;
  }
}

u3_noun
u3qc_hew(u3_atom boq,
         u3_atom sep,
         u3_atom vat,
         u3_noun sam)
{
  if ( !_(u3a_is_cat(boq)) || (boq >= 32) ) {
    return u3m_bail(c3__fail);
  }

  if ( !_(u3a_is_cat(sep)) ) {
    return u3m_bail(c3__fail);
  }

  u3_noun pro;
  c3_w  pos_w = _hew_in((c3_g)boq, (c3_w)sep, vat, sam, &pro);
  return u3nt(pro, boq, u3i_word(pos_w));
}

u3_noun
u3wc_hew(u3_noun cor)
{
  u3_atom boq, sep, vat;
  u3_noun sam;
  {
    u3_noun pay = u3t(cor);
    u3_noun con = u3t(pay);
    u3_noun gat = u3t(con);       // outer gate
    u3_noun cam = u3h(u3t(gat));  // outer sample
    u3_noun   d = u3h(con);

    boq = u3x_atom(u3h(d));
    sep = u3x_atom(u3t(d));
    vat = u3x_atom(u3t(cam));
    sam = u3h(pay);
  }

  return u3qc_hew(boq, sep, vat, sam);
}
