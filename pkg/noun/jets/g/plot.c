/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

// XX optimize
//
static c3_w
_met_plat_m(c3_g a_g, c3_w fum_w, c3_w met_w, u3_atom vat)
{
  c3_w len_w, wor_w;

  {
    u3i_slab sab_u;
    u3i_slab_init(&sab_u, a_g, met_w);
    u3r_chop(a_g, fum_w, met_w, 0, sab_u.buf_w, vat);

    len_w = sab_u.len_w;

    while ( len_w && !sab_u.buf_w[len_w - 1] ) {
      len_w--;
    }

    wor_w = !len_w ? 0 : sab_u.buf_w[len_w - 1];

    u3i_slab_free(&sab_u);
  }


  if ( !len_w ) {
    return 0;
  }

  {
    c3_w gal_w = len_w - 1;
    c3_w daz_w = wor_w;
    c3_y   a_y = a_g;

    //  inlined from u3r_met
    if (a_y < 5) {
      c3_y max_y = (1 << a_y) - 1;
      c3_y gow_y = 5 - a_y;

      if (gal_w > ((UINT32_MAX - (32 + max_y)) >> gow_y)) {
        return u3m_bail(c3__fail);
      }

      return (gal_w << gow_y)
        + ((c3_bits_word(daz_w) + max_y)
           >> a_y);
    }

    {
      c3_y gow_y = (a_y - 5);
      return ((gal_w + 1) + ((1 << gow_y) - 1)) >> gow_y;
    }
  }
}

static c3_w
_met_list(c3_g    a_g,
          c3_w  sep_w,
          u3_noun b_p);

static c3_w
_met_pair(c3_g* las_g,
          c3_w  sep_w,
          u3_noun a_p,
          u3_noun b_p,
          c3_g* new_g)
{
  c3_g res_g, a_g;

  while ( c3y == u3a_is_cell(a_p) ) {
    sep_w = _met_pair(las_g, sep_w, u3h(a_p), u3t(a_p), &res_g);
    las_g = &res_g;
    a_p   = u3h(b_p);
    b_p   = u3t(b_p);
  }

  if ( !_(u3a_is_cat(a_p)) || (a_p >= 32) ) {
    return u3m_bail(c3__fail);
  }

  a_g = (c3_g)a_p;

  if ( las_g && (a_g != *las_g) ) {
    sep_w = u3qc_rig_s(*las_g, sep_w, a_g);  // XX overflow
  }

  *new_g = a_g;
  return _met_list(a_g, sep_w, b_p);
}

static c3_w
_met_list(c3_g    a_g,
          c3_w  sep_w,
          u3_noun b_p)
{
  if ( u3_nul != b_p ) {
    c3_w met_w;
    u3_noun  i, t = b_p;

    do {
      u3x_cell(t, &i, &t);

      //  ?@  i.b.p
      if ( c3y == u3a_is_atom(i) ) {
        met_w  = u3r_met(a_g, i);
        sep_w += met_w;  // XX overflow
      }
      else {
        u3_noun i_i, t_i;
        u3x_cell(i, &i_i, &t_i);

        //  ?=(@ -.i.b.p)
        if ( c3y == u3a_is_atom(i_i) ) {
          if ( !_(u3a_is_cat(i_i)) ) {
            return u3m_bail(c3__fail);
          }

          met_w  = (c3_w)i_i;
          sep_w += met_w; // XX overflow
        }
        else {
          u3_noun  l, r;
          c3_o cel_o = u3r_cell(i_i, &l, &r);

          //  ?=([%c ~] -.i.b.p)
          if ( (c3y == cel_o) && ('c' == l) && (u3_nul == r) ) {
            u3_atom p_p_t_i = u3x_atom(u3h(u3h(t_i)));
            u3_atom q_p_t_i = u3x_atom(u3t(u3h(t_i)));
            u3_atom q_t_i   = u3x_atom(u3t(t_i));

            if (  !_(u3a_is_cat(p_p_t_i))
               || !_(u3a_is_cat(q_p_t_i))
               || !_(u3a_is_cat(q_t_i)) )
            {
              return u3m_bail(c3__fail);
            }

            met_w  = (c3_w)q_p_t_i;
            sep_w += met_w; // XX overflow
          }
          //  ?=([%m ~] -.i.b.p)
          else if ( (c3y == cel_o) && ('m' == l) && (u3_nul == r) ) {
            u3_atom p_p_t_i = u3x_atom(u3h(u3h(t_i)));
            u3_atom q_p_t_i = u3x_atom(u3t(u3h(t_i)));
            u3_atom q_t_i   = u3x_atom(u3t(t_i));

            if (  !_(u3a_is_cat(p_p_t_i))
               || !_(u3a_is_cat(q_p_t_i))
               || !_(u3a_is_cat(q_t_i)) )
            {
              return u3m_bail(c3__fail);
            }

            met_w  = _met_plat_m(a_g, (c3_w)p_p_t_i, (c3_w)q_p_t_i, q_t_i);
            sep_w += met_w; // XX overflow
          }
          //  ?=([%s ~] -.i.b.p) (assumed)
          else {
            c3_g new_g;
            u3x_cell(t_i, &l, &r);

            sep_w = _met_pair(&a_g, sep_w, l, r, &new_g);

            if ( new_g != a_g ) {
              sep_w = u3qc_rig_s(new_g, sep_w, a_g); // XX overflow
            }
          }
        }
      }
    }
    while ( u3_nul != t );
  }

  return sep_w;
}

static c3_w
_fax_list(u3i_slab* sab_u,
          c3_g        a_g,
          c3_w      sep_w,
          u3_noun     b_p);

static c3_w
_fax_pair(u3i_slab* sab_u,
          c3_g*     las_g,
          c3_w      sep_w,
          u3_noun     a_p,
          u3_noun     b_p,
          c3_g*     new_g)
{
  c3_g res_g, a_g;

  while ( c3y == u3a_is_cell(a_p) ) {
    sep_w = _fax_pair(sab_u, las_g, sep_w, u3h(a_p), u3t(a_p), &res_g);
    las_g = &res_g;
    a_p   = u3h(b_p);
    b_p   = u3t(b_p);
  }

  if ( !_(u3a_is_cat(a_p)) || (a_p >= 32) ) {
    return u3m_bail(c3__fail);
  }

  a_g = (c3_g)a_p;

  if ( las_g && (a_g != *las_g) ) {
    sep_w = u3qc_rig_s(*las_g, sep_w, a_g);  // XX overflow
  }

  *new_g = a_g;
  return _fax_list(sab_u, a_g, sep_w, b_p);
}

static c3_w
_fax_list(u3i_slab* sab_u,
          c3_g        a_g,
          c3_w      sep_w,
          u3_noun     b_p)
{
  if ( u3_nul != b_p ) {
    c3_w met_w;
    u3_noun  i, t = b_p;

    do {
      u3x_cell(t, &i, &t);

      //  ?@  i.b.p
      if ( c3y == u3a_is_atom(i) ) {
        met_w  = u3r_met(a_g, i);

        u3r_chop(a_g, 0, met_w, sep_w, sab_u->buf_w, i);

        sep_w += met_w;  // XX overflow
      }
      else {
        u3_noun i_i, t_i;
        u3x_cell(i, &i_i, &t_i);

        //  ?=(@ -.i.b.p)
        if ( c3y == u3a_is_atom(i_i) ) {
          if ( !_(u3a_is_cat(i_i)) ) {
            return u3m_bail(c3__fail);
          }

          met_w  = (c3_w)i_i;

          u3r_chop(a_g, 0, met_w, sep_w, sab_u->buf_w, u3x_atom(t_i));

          sep_w += met_w; // XX overflow
        }
        else {
          u3_noun  l, r;
          c3_o cel_o = u3r_cell(i_i, &l, &r);

          //  ?=([%c ~] -.i.b.p)
          if ( (c3y == cel_o) && ('c' == l) && (u3_nul == r) ) {
            u3_atom p_p_t_i = u3x_atom(u3h(u3h(t_i)));
            u3_atom q_p_t_i = u3x_atom(u3t(u3h(t_i)));
            u3_atom q_t_i   = u3x_atom(u3t(t_i));

            if (  !_(u3a_is_cat(p_p_t_i))
               || !_(u3a_is_cat(q_p_t_i))
               || !_(u3a_is_cat(q_t_i)) )
            {
              return u3m_bail(c3__fail);
            }

            met_w  = (c3_w)q_p_t_i;

            u3r_chop(a_g, (c3_w)p_p_t_i, met_w, sep_w, sab_u->buf_w, q_t_i);

            sep_w += met_w; // XX overflow
          }
          //  ?=([%m ~] -.i.b.p)
          else if ( (c3y == cel_o) && ('m' == l) && (u3_nul == r) ) {
            u3_atom p_p_t_i = u3x_atom(u3h(u3h(t_i)));
            u3_atom q_p_t_i = u3x_atom(u3t(u3h(t_i)));
            u3_atom q_t_i   = u3x_atom(u3t(t_i));

            if (  !_(u3a_is_cat(p_p_t_i))
               || !_(u3a_is_cat(q_p_t_i))
               || !_(u3a_is_cat(q_t_i)) )
            {
              return u3m_bail(c3__fail);
            }

            met_w = _met_plat_m(a_g, (c3_w)p_p_t_i, (c3_w)q_p_t_i, q_t_i);

            u3r_chop(a_g, (c3_w)p_p_t_i, met_w, sep_w, sab_u->buf_w, q_t_i);

            sep_w += met_w; // XX overflow
          }
          //  ?=([%s ~] -.i.b.p) (assumed)
          else {
            c3_g new_g;
            u3x_cell(t_i, &l, &r);

            sep_w = _fax_pair(sab_u, &a_g, sep_w, l, r, &new_g);

            if ( new_g != a_g ) {
              sep_w = u3qc_rig_s(new_g, sep_w, a_g); // XX overflow
            }
          }
        }
      }
    }
    while ( u3_nul != t );
  }

  return sep_w;
}

u3_noun
u3qg_plot_met(u3_noun a_p, u3_noun b_p)
{
  c3_g out_g;
  c3_w sep_w = _met_pair(NULL, 0, a_p, b_p, &out_g);

  return u3nc(out_g, u3i_word(sep_w));
}

u3_noun
u3wg_plot_met(u3_noun cor)
{
  u3_noun a_p, b_p;
  {
    u3_noun sam = u3h(u3t(cor));
    a_p = u3h(sam);
    b_p = u3t(sam);
  }
  return u3qg_plot_met(a_p, b_p);
}

u3_noun
u3qg_plot_fax(u3_noun a_p, u3_noun b_p)
{
  c3_g     out_g;
  c3_w     sep_w = _met_pair(NULL, 0, a_p, b_p, &out_g);
  u3i_slab sab_u;

  u3i_slab_init(&sab_u, out_g, sep_w);

  _fax_pair(&sab_u, NULL, 0, a_p, b_p, &out_g);

  return u3nt(u3i_slab_mint(&sab_u), out_g, u3i_word(sep_w));
}

u3_noun
u3wg_plot_fax(u3_noun cor)
{
  u3_noun a_p, b_p;
  {
    u3_noun sam = u3h(u3t(cor));
    a_p = u3h(sam);
    b_p = u3t(sam);
  }
  return u3qg_plot_fax(a_p, b_p);
}
