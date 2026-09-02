/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "murmur3.h"

//  +feis:ob constant parameters to +fe:ob
//
static const c3_h a_h =     0xffff;
static const c3_h b_h =    0x10000;
static const c3_h k_h = 0xffff0000;

//  +raku:ob
//
static const c3_h rak_h[4] = { 0xb76d5eed, 0xee281300, 0x85bcae01, 0x4b387af7 };

/* _fe_ob(): +fe:ob, with constant parameters factored out.
**           correct over the domain [0x0, 0xfffe.ffff]
*/
static c3_h
_fe_ob(c3_h m_h)
{
  c3_h l_h = m_h % a_h;
  c3_h r_h = m_h / a_h;
  c3_h f_h, t_h;
  c3_y j_y, k_y[2];

  for ( j_y = 0; j_y < 4; j_y++ ) {
    k_y[0] = r_h & 0xff;
    k_y[1] = (r_h >> 8) & 0xff;

    MurmurHash3_x86_32(k_y, 2, rak_h[j_y], &f_h);

    //  NB: this addition can overflow a c3_h (before mod)
    //
    t_h = ((c3_d)f_h + l_h) % (!(j_y & 1) ? a_h : b_h);
    l_h = r_h;
    r_h = t_h;
  }

  //  legendary @max19
  //
  return ( a_h == r_h )
         ? (r_h * a_h) + l_h
         : (l_h * a_h) + r_h;
}

/* _feis_ob(): +feis:ob, also offsetting by 0x1.000 (as in +fein:ob).
**             correct over the domain [0x1.0000, 0xffff.ffff]
*/
static c3_h
_feis_ob(c3_h m_h)
{
  c3_h c_h = _fe_ob(m_h - b_h);
  return b_h + (( c_h < k_h ) ? c_h : _fe_ob(c_h));
}

u3_atom
u3qe_fein_ob(u3_atom pyn)
{
  c3_w met_w = u3r_met(4, pyn);
  if ( UINT32_MAX < met_w ) {
    u3m_bail(c3__fail);
  }
  c3_h sor_h = met_w;

  if ( (sor_h < 2) || (sor_h > 4) ) {
    return u3k(pyn);
  }

  if ( 2 == sor_h ) {
    return u3i_half(_feis_ob(u3r_half(0, pyn)));
  }
  else {
    c3_h pyn_h[2];
    u3r_halfs(0, 2, pyn_h, pyn);

    if ( pyn_h[0] < b_h ) {
      return u3k(pyn);
    }
    else {
      pyn_h[0] = _feis_ob(pyn_h[0]);
      return u3i_halfs(2, pyn_h);
    }
  }
}

u3_noun
u3we_fein_ob(u3_noun cor)
{
  return u3qe_fein_ob(u3x_atom(u3x_at(u3x_sam, cor)));
}
