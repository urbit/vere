/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  typedef struct _u3_loss {                 //  loss problem
    u3_noun hel;                            //  a as a list
    c3_n lel_n;                             //  length of a
    c3_n lev_n;                             //  length of b
    u3_noun* hev;                           //  b as an array
    u3_noun sev;                            //  b as a set of lists
    c3_n kct_n;                             //  candidate count
    u3_noun* kad;                           //  candidate array
  } u3_loss;

  //  free loss object
  //
  static void
  _flem(u3_loss* loc_u)
  {
    u3z(loc_u->sev);
    {
      c3_n i_n;

      for ( i_n = 0; i_n < loc_u->kct_n; i_n++ ) {
        u3z(loc_u->kad[i_n]);
      }
    }
    u3a_free(loc_u->hev);
    u3a_free(loc_u->kad);
  }

  //  extract lcs  -  XX don't use the stack like this
  //
  static u3_noun
  _lext(u3_loss* loc_u,
        u3_noun  kad)
  {
    if ( u3_nul == kad ) {
      return u3_nul;
    } else {
      return u3nc(u3k(loc_u->hev[u3r_note(0, u3h(kad))]),
                  _lext(loc_u, u3t(kad)));
    }
  }

  //  extract lcs
  //
  static u3_noun
  _lexs(u3_loss* loc_u)
  {
    if ( 0 == loc_u->kct_n ) {
      return u3_nul;
    } else return u3kb_flop(_lext(loc_u, loc_u->kad[loc_u->kct_n - 1]));
  }

  //  initialize loss object
  //
  static void
  _lemp(u3_loss* loc_u,
        u3_noun  hel,
        u3_noun  hev)
  {
    loc_u->hel = hel;
    loc_u->lel_n = u3kb_lent(u3k(hel));

    //  Read hev into array.
    {
      c3_n i_n;

      loc_u->hev = u3a_malloc(u3kb_lent(u3k(hev)) * sizeof(u3_noun));

      for ( i_n = 0; u3_nul != hev; i_n++ ) {
        loc_u->hev[i_n] = u3h(hev);
        hev = u3t(hev);
      }
      loc_u->lev_n = i_n;
    }
    loc_u->kct_n = 0;
    loc_u->kad = u3a_malloc((1 + c3_min(loc_u->lev_n, loc_u->lel_n)) *
                             sizeof(u3_noun));

    //  Compute equivalence classes.
    //
    loc_u->sev = u3_nul;
    {
      c3_n i_n;

      for ( i_n = 0; i_n < loc_u->lev_n; i_n++ ) {
        u3_noun how = loc_u->hev[i_n];
        u3_noun hav;
        u3_noun teg;

        hav = u3kdb_get(u3k(loc_u->sev), u3k(how));
        teg = u3nc(u3i_note(i_n),
                   (hav == u3_none) ? u3_nul : hav);
        loc_u->sev = u3kdb_put(loc_u->sev, u3k(how), teg);
      }
    }
  }

  //  apply
  //
  static void
  _lune(u3_loss* loc_u,
        c3_n     inx_n,
        c3_n     goy_n)
  {
    u3_noun kad;

    kad = u3nc(u3i_note(goy_n),
               (inx_n == 0) ? u3_nul
                            : u3k(loc_u->kad[inx_n - 1]));
    if ( loc_u->kct_n == inx_n ) {
      u3_assert(loc_u->kct_n < (1 << 31));
      loc_u->kct_n++;
    } else {
      u3z(loc_u->kad[inx_n]);
    }
    loc_u->kad[inx_n] = kad;
  }

  //  extend fits top
  //
  static u3_noun
  _hink(u3_loss* loc_u,
        c3_n     inx_n,
        c3_n     goy_n)
  {
    return __
         ( (loc_u->kct_n == inx_n) ||
           (u3r_note(0, u3h(loc_u->kad[inx_n])) > goy_n) );
  }

  //  extend fits bottom
  //
  static u3_noun
  _lonk(u3_loss* loc_u,
        c3_n     inx_n,
        c3_n     goy_n)
  {
    return __
      ( (0 == inx_n) ||
        (u3r_note(0, u3h(loc_u->kad[inx_n - 1])) < goy_n) );
  }

#if 0
  //  search for first index >= inx_n and <= max_n that fits
  //  the hink and lonk criteria.
  //
  static u3_noun
  _binka(u3_loss* loc_u,
         c3_n*    inx_n,
         c3_n     max_n,
         c3_n     goy_n)
  {
    while ( *inx_n <= max_n ) {
      if ( c3n == _lonk(loc_u, *inx_n, goy_n) ) {
        return c3n;
      }
      if ( c3y == _hink(loc_u, *inx_n, goy_n) ) {
        return c3y;
      }
      else ++*inx_n;
    }
    return c3n;
  }
#endif

  //  search for lowest index >= inx_n and <= max_n for which
  //  both hink(inx_n) and lonk(inx_n) are true.  lonk is false
  //  if inx_n is too high, hink is false if it is too low.
  //
  static u3_noun
  _bink(u3_loss* loc_u,
        c3_n*    inx_n,
        c3_n     max_n,
        c3_n     goy_n)
  {
    u3_assert(max_n >= *inx_n);

    if ( max_n == *inx_n ) {
      if ( c3n == _lonk(loc_u, *inx_n, goy_n) ) {
        return c3n;
      }
      if ( c3y == _hink(loc_u, *inx_n, goy_n) ) {
        return c3y;
      }
      else {
        ++*inx_n;
        return c3n;
      }
    }
    else {
      c3_n mid_n = *inx_n + ((max_n - *inx_n) / 2);

      if ( (c3n == _lonk(loc_u, mid_n, goy_n)) ||
           (c3y == _hink(loc_u, mid_n, goy_n)) )
      {
        return _bink(loc_u, inx_n, mid_n, goy_n);
      } else {
        *inx_n = mid_n + 1;
        return _bink(loc_u, inx_n, max_n, goy_n);
      }
    }
  }


  static void
  _merg(u3_loss* loc_u,
        c3_n     inx_n,
        u3_noun  gay)
  {
    if ( (u3_nul == gay) || (inx_n > loc_u->kct_n) ) {
      return;
    }
    else {
      u3_noun i_gay = u3h(gay);
      c3_n    goy_n = u3r_note(0, i_gay);
      u3_noun bik;

      bik = _bink(loc_u, &inx_n, loc_u->kct_n, goy_n);

      if ( c3y == bik ) {
        _merg(loc_u, inx_n + 1, u3t(gay));
        _lune(loc_u, inx_n, goy_n);
      }
      else {
        _merg(loc_u, inx_n, u3t(gay));
      }
    }
  }

  //  compute lcs
  //
  static void
  _loss(u3_loss* loc_u)
  {
    while ( u3_nul != loc_u->hel ) {
      u3_noun i_hel = u3h(loc_u->hel);
      u3_noun guy   = u3kdb_get(u3k(loc_u->sev), u3k(i_hel));

      if ( u3_none != guy ) {
        u3_noun gay = u3kb_flop(guy);

        _merg(loc_u, 0, gay);
        u3z(gay);
      }

      loc_u->hel = u3t(loc_u->hel);
    }
  }

  u3_noun
  u3qe_loss(u3_noun hel,
            u3_noun hev)
  {
    u3_loss loc_u;
    u3_noun lcs;

    _lemp(&loc_u, hel, hev);
    _loss(&loc_u);
    lcs = _lexs(&loc_u);

    _flem(&loc_u);
    return lcs;
  }

  static u3_noun
  _listp(u3_noun lix)
  {
    while ( 1 ) {
      if ( u3_nul == lix ) return c3y;
      if ( c3n == u3du(lix) ) return c3n;
      lix = u3t(lix);
    }
  }

  u3_noun
  u3we_loss(u3_noun cor)
  {
    u3_noun hel, hev;

    if ( (u3_none == (hel = u3r_at(u3x_sam_2, cor))) ||
         (u3_none == (hev = u3r_at(u3x_sam_3, cor))) ||
         (c3n == _listp(hel)) ||
         (c3n == _listp(hev)) )
    {
      return u3m_bail(c3__fail);
    } else {
      return u3qe_loss(hel, hev);
    }
  }
