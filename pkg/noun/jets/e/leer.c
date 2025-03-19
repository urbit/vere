/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

static u3_atom
_leer_cut(c3_n pos_n, c3_n len_n, u3_atom src)
{
  if ( 0 == len_n ) {
    return 0;
  }
  else {
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 3, len_n);
    // XX: 64 what?
    sab_u.buf_n[sab_u.len_n - 1] = 0;

    u3r_bytes(pos_n, len_n, sab_u.buf_y, src);

    return u3i_slab_mint_bytes(&sab_u);
  }
}

// Leaving the lore jet in place for backwards compatibility.
// TODO: remove u3[qw]e_lore (also from jet tree)

u3_noun
u3qe_lore(u3_atom lub)
{
  c3_n    len_n = u3r_met(3, lub);
  c3_n    pos_n = 0;
  u3_noun tez = u3_nul;

  while ( 1 ) {
    c3_n meg_n = 0;
    c3_y end_y;

    c3_y byt_y;
    while ( 1 ) {
      if ( pos_n >= len_n ) {
        byt_y = 0;
        end_y = c3y;
        break;
      }
      byt_y = u3r_byte(pos_n + meg_n, lub);

      if ( (10 == byt_y) || (0 == byt_y) ) {
        end_y = __(byt_y == 0);
        break;
      } else meg_n++;
    }

    if ((byt_y == 0) && ((pos_n + meg_n + 1) < len_n)) {
      return u3m_bail(c3__exit);
    }

    if ( !_(end_y) && pos_n >= len_n ) {
      return u3kb_flop(tez);
    }
    else {
      tez = u3nc(_leer_cut(pos_n, meg_n, lub), tez);
      if ( _(end_y) ) {
        return u3kb_flop(tez);
      }
      pos_n += (meg_n + 1);
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
    c3_n pos_n, i_n = 0, len_n = u3r_met(3, txt);
    u3_noun* hed;
    u3_noun* tel;

    while ( i_n < len_n ) {
      //  scan till end or newline
      //
      for ( pos_n = i_n; i_n < len_n; ++i_n ) {
        if ( 10 == u3r_byte(i_n, txt) ) {
          break;
        }
      }

      //  append to list
      //
      *lit = u3i_defcons(&hed, &tel);
      *hed = _leer_cut(pos_n, i_n - pos_n, txt);
      lit  = tel;

      i_n++;
    }
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
