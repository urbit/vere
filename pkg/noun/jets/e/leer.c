/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

/* _leer_byte(): byte [i_w] of the atom [vue_u] views, 0 past its end.
**
**   both jets read one view of their text: a bob maps once, not per byte
**   or line.
*/
static inline c3_y
_leer_byte(const u3r_view* vue_u, c3_w i_w)
{
  return ( i_w < vue_u->byt_d ) ? vue_u->byt_y[i_w] : 0;
}

static u3_atom
_leer_cut(c3_w pos_w, c3_w len_w, const u3r_view* vue_u)
{
  if ( 0 == len_w ) {
    return 0;
  }
  else {
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 3, len_w);
    // XX: 64 what?
    sab_u.buf_w[sab_u.len_w - 1] = 0;

    u3r_view_copy(vue_u, pos_w, sab_u.buf_y, len_w);

    return u3i_slab_mint_bytes(&sab_u);
  }
}

// Leaving the lore jet in place for backwards compatibility.
// TODO: remove u3[qw]e_lore (also from jet tree)

u3_noun
u3qe_lore(u3_atom lub)
{
  c3_w     len_w = u3r_met(3, lub);
  c3_w     pos_w = 0;
  u3_noun  tez = u3_nul;
  u3r_view vue_u;

  u3r_view_open(&vue_u, lub, 0, c3y);

  while ( 1 ) {
    c3_w meg_w = 0;
    c3_y end_y;

    c3_y byt_y;
    while ( 1 ) {
      if ( pos_w >= len_w ) {
        byt_y = 0;
        end_y = c3y;
        break;
      }
      byt_y = _leer_byte(&vue_u, pos_w + meg_w);

      if ( (10 == byt_y) || (0 == byt_y) ) {
        end_y = __(byt_y == 0);
        break;
      } else meg_w++;
    }

    if ((byt_y == 0) && ((pos_w + meg_w + 1) < len_w)) {
      u3r_view_done(&vue_u);
      return u3m_bail(c3__exit);
    }

    if ( !_(end_y) && pos_w >= len_w ) {
      u3r_view_done(&vue_u);
      return u3kb_flop(tez);
    }
    else {
      tez = u3nc(_leer_cut(pos_w, meg_w, &vue_u), tez);
      if ( _(end_y) ) {
        u3r_view_done(&vue_u);
        return u3kb_flop(tez);
      }
      pos_w += (meg_w + 1);
    }
  }
}

u3_noun
u3we_lore(u3_noun cor)
{
  u3_noun lub;

  if ( (u3_none == (lub = u3r_at(u3x_sam, cor))) ||
       (c3n == u3ud(lub)) )
  {
    return u3m_bail(c3__fail);
  } else {
    return u3qe_lore(lub);
  }
}

u3_noun
u3qe_leer(u3_atom txt)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  {
    c3_w     pos_w, i_w = 0, len_w = u3r_met(3, txt);
    u3_noun* hed;
    u3_noun* tel;
    u3r_view vue_u;

    u3r_view_open(&vue_u, txt, 0, c3y);

    while ( i_w < len_w ) {
      //  scan till end or newline
      //
      for ( pos_w = i_w; i_w < len_w; ++i_w ) {
        if ( 10 == _leer_byte(&vue_u, i_w) ) {
          break;
        }
      }

      //  append to list
      //
      *lit = u3i_defcons(&hed, &tel);
      *hed = _leer_cut(pos_w, i_w - pos_w, &vue_u);
      lit  = tel;

      i_w++;
    }

    u3r_view_done(&vue_u);
  }

  *lit = u3_nul;

  return pro;
}

u3_noun
u3we_leer(u3_noun cor)
{
  u3_noun txt = u3x_at(u3x_sam, cor);

  if ( c3n == u3ud(txt) ) {
    return u3m_bail(c3__fail);
  }

  return u3qe_leer(txt);
}
