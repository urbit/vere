/// @file
///
/// +core-chains of the SKA lib (nock-compilation.hoon): .want, and wherever
/// it covers some of a registered core of .bus that one of its templates
/// matches (+chain-cape), the template's cape too.  An exact port: the hoon
/// is the spec; a change there goes here too.

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  the registrations it reads (regs of the SKA lib), RETAINED
//
typedef struct {
  u3_noun rot;  //  root.reg
  u3_noun kor;  //  core.reg
  u3_noun bat;  //  batt.reg
} _cc_reg;

/* capes
*/

/* _cc_all(): +all:ca
*/
static c3_o
_cc_all(u3_noun c)
{
  while ( c3y == u3du(c) ) {
    if ( c3n == _cc_all(u3h(c)) ) {
      return c3n;
    }
    c = u3t(c);
  }
  return c;
}

/* _cc_con(): +con:ca, TRANSFER.
*/
static u3_noun
_cc_con(u3_noun h, u3_noun t)
{
  if ( (c3n == h) && (c3n == t) ) {
    return c3n;
  }
  return u3nc(h, t);
}

/* _cc_uni(): +uni:ca, RETAIN.
*/
static u3_noun
_cc_uni(u3_noun a, u3_noun b)
{
  if ( c3y == u3r_sing(a, b) ) {
    return u3k(a);
  }
  if ( c3y == u3ud(a) ) {
    return ( c3y == a ) ? c3y : u3k(b);
  }
  if ( c3y == u3ud(b) ) {
    return ( c3y == b ) ? c3y : u3k(a);
  }
  return _cc_con(_cc_uni(u3h(a), u3h(b)), _cc_uni(u3t(a), u3t(b)));
}

/* socks
*/

/* _cc_pull(): +pull:so, RETAIN.
*/
static u3_noun
_cc_pull(u3_noun s, u3_atom axe)
{
  u3_noun cap, dat;
  c3_w    i_w;

  if ( 0 == axe ) {
    return u3m_bail(c3__exit);
  }
  if ( c3y == u3ud(s) ) {
    return s;
  }
  if ( 1 == axe ) {
    return u3k(s);
  }
  cap = u3h(s);
  dat = u3t(s);
  i_w = u3r_met(0, axe) - 1;
  while ( i_w ) {
    c3_b bit_b;

    if ( (c3n == cap) || (c3y == u3ud(dat)) ) {
      return u3nc(c3n, 0);
    }
    bit_b = u3r_bit(i_w - 1, axe);
    if ( c3y == u3du(cap) ) {
      cap = bit_b ? u3t(cap) : u3h(cap);
    }
    dat = bit_b ? u3t(dat) : u3h(dat);
    i_w--;
  }
  return u3nc(u3k(cap), u3k(dat));
}

/* _cc_huge_in(): +huge:so on [c1 d1] and [c2 d2], neither void.
*/
static c3_o
_cc_huge_in(u3_noun c1, u3_noun d1, u3_noun c2, u3_noun d2)
{
  while ( 1 ) {
    u3_noun lc1, rc1, lc2, rc2;

    if ( (c3y == u3r_sing(c1, c2)) && (c3y == u3r_sing(d1, d2)) ) {
      return c3y;
    }
    if ( c3y == u3ud(d1) ) {
      if ( c3n == u3ud(c1) ) {
        return u3m_bail(c3__exit);
      }
      if ( c3n == c1 ) {
        return c3y;
      }
      return __( (c3y == c2) && (c3y == u3r_sing(d1, d2)) );
    }
    if ( c3y == u3ud(d2) ) {
      if ( (c3n == u3ud(c2)) || (c3n == c1) ) {
        return u3m_bail(c3__exit);
      }
      return c3n;
    }
    if ( c3y == u3du(c1) ) { lc1 = u3h(c1); rc1 = u3t(c1); }
    else                   { lc1 = rc1 = c1; }
    if ( c3y == u3du(c2) ) { lc2 = u3h(c2); rc2 = u3t(c2); }
    else                   { lc2 = rc2 = c2; }

    if ( c3n == _cc_huge_in(lc1, u3h(d1), lc2, u3h(d2)) ) {
      return c3n;
    }
    c1 = rc1; d1 = u3t(d1);
    c2 = rc2; d2 = u3t(d2);
  }
}

/* _cc_huge(): +huge:so, RETAIN.
*/
static c3_o
_cc_huge(u3_noun one, u3_noun two)
{
  if ( c3y == u3r_sing(one, two) ) {
    return c3y;
  }
  if ( c3y == u3ud(two) ) {
    return c3y;
  }
  if ( c3y == u3ud(one) ) {
    return c3n;
  }
  return _cc_huge_in(u3h(one), u3t(one), u3h(two), u3t(two));
}

/* registrations
*/

/* _cc_ju_tap(): ~(tap in (~(get ju jug) key)), RETAIN.
*/
static u3_noun
_cc_ju_tap(u3_noun jug, u3_noun key)
{
  u3_noun got = u3qdb_get(jug, key);
  u3_noun pro;

  if ( u3_nul == got ) {
    return u3_nul;
  }
  pro = u3qdi_tap(u3t(got));
  u3z(got);
  return pro;
}

/* _cc_chain(): +chain-cape, RETAIN; the cape, or none.
*/
static u3_weak
_cc_chain(_cc_reg* reg_u, u3_noun bus)
{
  u3_noun bat, bad, paz, i;

  if ( (c3y == u3ud(bus)) || (c3y == u3ud(u3t(bus))) ) {
    return u3_none;
  }
  bat = _cc_pull(bus, 2);
  if ( c3y == u3ud(bat) ) {
    return u3_none;
  }
  if ( c3n == _cc_all(u3h(bat)) ) {
    u3z(bat);
    return u3_none;
  }
  bad = u3t(bat);
  if ( c3y == u3ud(bad) ) {
    u3z(bat);
    return u3_none;
  }
  if (  (c3y == u3qdb_has(reg_u->rot, u3t(bus)))
     && (c3y == _cc_all(u3h(bus))) )
  {
    u3z(bat);
    return c3y;
  }
  paz = _cc_ju_tap(reg_u->bat, bad);
  u3z(bat);

  for ( i = paz; u3_nul != i; i = u3t(i) ) {
    u3_noun tez = _cc_ju_tap(reg_u->kor, u3h(i));
    u3_noun j;

    for ( j = tez; u3_nul != j; j = u3t(j) ) {
      u3_noun tem = u3h(j);

      if ( c3y == u3ud(tem) ) {
        continue;
      }
      if ( c3y == _cc_huge(tem, bus) ) {
        u3_noun pro = u3k(u3h(tem));
        u3z(tez);
        u3z(paz);
        return pro;
      }
    }
    u3z(tez);
  }
  u3z(paz);
  return u3_none;
}

/* _cc_chains(): +core-chains' loop on [cap dat], neither void nor |
**               nor an atom; RETAIN.
*/
static u3_noun
_cc_chains(_cc_reg* reg_u, u3_noun want, u3_noun cap, u3_noun dat)
{
  u3_noun hc, tc, l, r, out, pro;
  u3_weak tem;

  if ( c3y == u3ud(want) ) {
    return want;
  }
  if ( (c3n == cap) || (c3y == u3ud(dat)) ) {
    return u3k(want);
  }
  {
    u3_noun bus = u3nc(u3k(cap), u3k(dat));
    tem = _cc_chain(reg_u, bus);
    u3z(bus);
  }
  if ( c3y == tem ) {
    return c3y;
  }
  //  +hed:so and +tel:so of [cap dat]: a cape & is & on both sides
  //
  if ( c3y == u3ud(cap) ) { hc = tc = cap; }
  else                    { hc = u3h(cap); tc = u3t(cap); }

  l   = _cc_chains(reg_u, u3h(want), hc, u3h(dat));
  r   = _cc_chains(reg_u, u3t(want), tc, u3t(dat));
  out = _cc_con(l, r);
  if ( u3_none == tem ) {
    return out;
  }
  pro = _cc_uni(out, tem);
  u3z(out);
  u3z(tem);
  return pro;
}

/* u3we_core_chains(): +core-chains.
*/
u3_noun
u3we_core_chains(u3_noun cor)
{
  u3_noun want, bus, reg, arm, new, pro;
  _cc_reg reg_u;
#ifdef U3NC_STAT
  c3_d    beg_d = __builtin_ia32_rdtsc();
#endif

  u3x_mean(cor, {u3x_sam_2, &want}, {u3x_sam_6, &bus}, {u3x_sam_7, &reg});

  if ( (c3y == u3ud(want)) || (c3y == u3ud(bus)) ) {
    return u3k(want);
  }
  u3x_quil(reg, &reg_u.rot, &reg_u.kor, &reg_u.bat, &arm, &new);
  pro = _cc_chains(&reg_u, want, u3h(bus), u3t(bus));

#ifdef U3NC_STAT
  u3we_prof_d[5][0] += __builtin_ia32_rdtsc() - beg_d;
  u3we_prof_d[5][1]++;
  u3we_prof_d[5][2]++;
#endif
  return pro;
}
