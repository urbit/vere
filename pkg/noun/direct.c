/// @file

#include "direct.h"

static inline c3_o
_assert_loob(u3_noun som)
{
    u3_assert(som <= 1);
    return som;
}

// RETAINS
//
static void
_ca_rip(u3_noun cape, u3_noun* l, u3_noun* r)
{
    if ( c3y == u3ud(cape) )
    {
        *l = *r = _assert_loob(cape);
    }
    else
    {
        u3r_cell(cape, l, r);
    }
}

// RETAINS arguments
// (here it is ok since deduplication will always happen on each iteration, so
// inductive argument of validity of uncounted refs applies)
//
static c3_o
_so_huge(u3_noun cape_one,
         u3_noun data_one,
         u3_noun cape_two,
         u3_noun data_two)
{
    if ( c3y == u3r_sing(cape_one, cape_two)
      && c3y == u3r_sing(data_one, data_two) )
    {
        return c3y;
    }

    if ( c3y == u3ud(data_one) )
    {
        if ( c3n == _assert_loob(cape_one) ) return c3y;
        return c3a(u3ud(cape_two),
               c3a(_assert_loob(cape_two),
                   u3r_sing(data_one, data_two)));
    }
    
    u3_assert(c3n != cape_one);

    if ( c3y == u3ud(data_two) )
    {
        _assert_loob(cape_two);
        return c3n;
    }

    u3_noun lope, rope, loop, roop;
    u3_noun l_data_one, r_data_one;
    u3_noun l_data_two, r_data_two;

    u3r_cell(data_one, &l_data_one, &r_data_one);
    u3r_cell(data_two, &l_data_two, &r_data_two);

    _ca_rip(cape_one, &lope, &rope);
    _ca_rip(cape_two, &loop, &roop);

    return c3a(_so_huge(lope, l_data_one, loop, l_data_two),
               _so_huge(rope, r_data_one, roop, r_data_two));
}

void
u3d_prep_ka()
{
    if ( u3R->dir.ka ) {
        return;
    }
    // [sock=hoon soak=hoon noir=hoon skan=hoon]
    //
    // u3_noun hoons = u3s_cue_bytes((c3_d)u3_Ka_core_len, u3_Ka_core);
    u3_noun hoons = u3s_cue_bytes((c3_d)u3_Ka_core_verb_len, u3_Ka_core_verb);
    u3_noun sock, soak, noir, skan;
    if ( c3n == u3r_mean(hoons,
        2,  &sock,
        6,  &soak,
        14, &noir,
        15, &skan, 0) ) {
        u3m_bail(c3__fail);
    }

    u3_noun bild = u3v_wish("!>(..zuse)");
    u3_noun slap = u3v_wish("slap");

    bild = u3n_slam_on(u3k(slap), u3nc(bild, u3k(sock)));
    bild = u3n_slam_on(u3k(slap), u3nc(bild, u3k(soak)));
    bild = u3n_slam_on(u3k(slap), u3nc(bild, u3k(noir)));
    bild = u3n_slam_on(u3k(slap), u3nc(bild, u3k(skan)));

    u3_noun ka = u3n_slam_on(slap,
        u3nc(bild, u3nt(c3__wing, c3_s2('k', 'a'), u3_nul)));

    u3z(hoons);
    u3R->dir.ka = ka;
}

//  XX: reentrance?
//
static void
_d_rout(u3_noun sub, u3_noun fol)
{
    // ( [%wing p=~[%rout]] )
    //
    u3_noun gen  = u3nt(c3__wing, c3_s4('r','o','u','t'), u3_nul),
            rout = u3dc("slap", u3R->dir.ka, gen);

    u3_noun typ = u3nt(c3__cell, c3__noun, c3__noun);
    u3_noun sam = u3nt(typ, sub, fol);  //  !>([sub=* fol=*])

    u3_noun slam = u3v_wish("slam");
    u3_noun gul = u3nt(u3nc(1, 0), u3nc(0, 0), 0);  // |~(^ ~)

    u3_noun pro = u3n_slam_et(gul, slam, u3nc(rout, sam));

    u3_assert(_(u3du(pro)));
    if ( 0 != u3h(pro) )
    {
        u3m_bail(c3__fail);
    }
    u3R->dir.ka = u3k(u3t(pro));
    u3z(pro);
}

// RETAINS
// `list` is (list pro=[sock *])
//
u3_weak
u3d_match_sock(u3_noun cape, u3_noun data, u3_noun list)
{
    u3_weak pro = u3_none;
    u3_noun cape_max, data_max;
    u3_noun i, cape_i, data_i;
    while ( u3_nul != list )
    {
        u3x_cell(list, &i, &list);
        u3x_mean(i, 4, &cape_i,
                    5, &data_i,
                    0);
        if ( c3n == _so_huge(cape_i, data_i, cape, data) ) continue;
        //  first match or better match
        //
        if ( (u3_none == pro)
              || (c3y == _so_huge(cape_max, data_max, cape_i, data_i)) )
        {
            pro = i;
            cape_max = cape_i;
            data_max = data_i;
        }
    }
    return pro;
}

static u3_noun
_d_get_boil()
{
    u3_noun gul  = u3nt(u3nc(1, 0), u3nc(0, 0), 0);  // |~(^ ~)
    u3_noun slap = u3v_wish("slap");

    // ( [%wing p=~[%lon]] )
    //
    u3_noun gen = u3nt(c3__wing, c3_s3('l','o','n'), u3_nul);
    u3_noun vax = u3n_slam_on(u3k(slap), u3nc(u3k(u3R->dir.ka), gen));
    u3_noun lon = u3k(u3t(vax));
    u3z(vax);

    // ( [%wing p=~[%cook]] )
    //
    gen = u3nt(c3__wing, c3__cook, u3_nul);
    vax = u3n_slam_on(slap, u3nc(u3k(u3R->dir.ka), gen));
    u3_noun gat = u3k(u3t(vax));
    u3z(vax);

    u3_noun pro = u3n_slam_et(gul, gat, lon);
    u3_assert(_(u3du(pro)));
    if ( 0 != u3h(pro) )
    {
        u3m_bail(c3__fail);
    }

    u3_noun boil = u3k(u3t(pro));
    u3z(pro);
    return boil;
}

//  RETAINS arguments
//  XX remove u3dc, use hard-coded axes
//
u3n_prog*
u3d_search(u3_noun sub, u3_noun fol)
{
    u3d_prep_ka();

    u3n_prog* pog_u = u3n_look_direct(sub, fol);
    if ( pog_u ) return pog_u;

    u3_noun cole, code, fols;
    u3_noun boil = _d_get_boil();
    u3_assert( c3y == u3r_mean(boil, 2, &cole, 6, &code, 7, &fols, 0) );

    u3_noun lit = u3kdb_get(u3k(fols), u3k(fol));
    if (u3_none == lit || u3_none == u3d_match_sock(c3y, sub, lit))
    {
        u3z(boil);
        _d_rout(u3k(sub), u3k(fol));
        boil = _d_get_boil();

        u3_assert( c3y == u3r_mean(boil, 2, &cole, 6, &code, 7, &fols, 0) );
    }
    //  may be u3_none
    //
    u3z(lit);
    pog_u = u3n_build_direct(sub, fol, cole, code, fols);
    u3z(boil);
    return pog_u;
}