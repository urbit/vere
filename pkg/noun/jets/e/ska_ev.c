/// @file
///
/// +eval:ev of the SKA lib (nock-compilation.hoon): Nock with fuel over a
/// partially known subject, for the analysis to fold the calls it can run.
///
/// An exact port: the same product, the same cape of what it read (rid),
/// the same %fast registrations (fas) and the same steps, so the analysis
/// takes the same decisions with the jet as without it.  The hoon is the
/// spec; a change there goes here too.
///
/// Values (ev-val) are nouns as in the hoon: [%k n] known, [%r a] axis a
/// of the subject, [%c h t] a cell of values.  The state the hoon threads
/// (ev-st) is linear, so it lives in _ev_t; a failure ends the run with the
/// fuel left, except +jet's, which leaves the state as it was.  Recursion
/// past EV_DEEP punts to the hoon.

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#define EV_K  'k'
#define EV_R  'r'
#define EV_C  'c'

//  fuel a native jet call takes (+ev-jet-cost)
//
#define EV_JET_COST  8

//  C recursion of +go and +real, past which the hoon runs it
//
#define EV_DEEP  20000

typedef struct {
  u3_noun       suk;    //  sock of the subject
  u3_noun       rot;    //  root.reg
  u3_noun       kor;    //  core.reg
  u3_noun       bat;    //  batt.reg
  u3_noun       hot;    //  (set ring) vere has jets for
  c3_d          fel_d;  //  fuel left
  u3_noun       rid;    //  cape read
  u3_noun       cal;    //  (set @) cores whose arms the run called
  u3_noun       fas;    //  (list [clue=sock core=sock])
  u3_noun       faz;    //  (set ^) batteries registered
  c3_w          dep_w;  //  recursion depth
  c3_o          pun_o;  //  too deep: punt
#ifdef U3NC_STAT
  c3_y          why_y;  //  the last failure, an EV_WHY_*
  c3_w          wha_w;  //  its axis, if direct
  u3_noun       now;    //  the formula +go runs (not counted)
  u3_noun       wen;    //  the formula at the last failure (not counted)
#endif
  u3p(u3h_root) cha_p;  //  +chain-cape by axis
  u3p(u3h_root) cod_p;  //  +code by axis, for the current cal
} _ev_t;

c3_d u3we_ev_stat_d[4];   //  calls, memo hits, punts, steps

//  why tries fail (stat builds): of each, the tries and their steps
//
#define EV_WHY_FORM  0      //  a formula Nock doesn't run
#define EV_WHY_FUEL  1      //  out of fuel
#define EV_WHY_SLOT  2      //  an axis the subject may not have
#define EV_WHY_DATA  3      //  a read of a part of the subject not code
#define EV_WHY_HIDE  4      //  a read of a part the sock doesn't know whole
#define EV_WHY_CRAS  5      //  a crash: an increment of a cell, a bad test

#ifdef U3NC_STAT
c3_d u3we_ev_why_d[6][2];
#  define EV_WHY(ev, w, a)                                    \
     do { (ev)->why_y = (w);                                  \
          (ev)->wha_w = ( c3y == u3a_is_cat(a) ) ? (a) : ~0u;  \
          if ( (EV_WHY_FORM != (w)) && (EV_WHY_FUEL != (w)) ) {  \
            if ( u3_none != (ev)->wen ) u3z((ev)->wen);        \
            (ev)->wen = ( u3_none == (ev)->now ) ? u3_none : u3k((ev)->now); \
          }                                                    \
     } while (0)
#else
#  define EV_WHY(ev, w, a)  do {} while (0)
#endif

/* axes
*/
static inline c3_y
_ev_cap(u3_atom a)
{
  return ( c3y == u3a_is_cat(a) ) ? u3x_cap(a) : (c3_y)u3qc_cap(a);
}

static inline u3_atom
_ev_mas(u3_atom a)
{
  return ( c3y == u3a_is_cat(a) ) ? u3x_mas(a) : u3qc_mas(a);
}

/* capes
*/

/* _ev_all(): +all:ca
*/
static c3_o
_ev_all(u3_noun c)
{
  while ( c3y == u3du(c) ) {
    if ( c3n == _ev_all(u3h(c)) ) {
      return c3n;
    }
    c = u3t(c);
  }
  return c;
}

/* _ev_lot(): +lot:ca, RETAIN, the cape produced is in c.
*/
static u3_noun
_ev_lot(u3_noun c, u3_atom axe)
{
  c3_w i_w;

  if ( 0 == axe ) {
    return u3m_bail(c3__exit);
  }
  i_w = u3r_met(0, axe) - 1;
  while ( i_w ) {
    if ( c3y == u3ud(c) ) {
      return c;
    }
    c = u3r_bit(i_w - 1, axe) ? u3t(c) : u3h(c);
    i_w--;
  }
  return c;
}

/* _ev_con(): +con:ca, TRANSFER.
*/
static u3_noun
_ev_con(u3_noun h, u3_noun t)
{
  if ( (c3n == h) && (c3n == t) ) {
    return c3n;
  }
  return u3nc(h, t);
}

/* _ev_uni(): +uni:ca, RETAIN.
*/
static u3_noun
_ev_uni(u3_noun a, u3_noun b)
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
  return _ev_con(_ev_uni(u3h(a), u3h(b)), _ev_uni(u3t(a), u3t(b)));
}

/* _ev_pat(): (pat:ca & a)
*/
static u3_noun
_ev_pat(u3_atom a)
{
  u3_noun c = c3y;
  c3_w    i_w, len_w;

  if ( 0 == a ) {
    return u3m_bail(c3__exit);
  }
  len_w = u3r_met(0, a) - 1;
  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c = u3r_bit(i_w, a) ? u3nc(c3n, c) : u3nc(c, c3n);
  }
  return c;
}

/* socks
*/

/* _ev_pull(): +pull:so, RETAIN.
*/
static u3_noun
_ev_pull(u3_noun s, u3_atom axe)
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

/* _ev_knit(): +knit:so, RETAIN.
*/
static u3_noun
_ev_knit(u3_noun one, u3_noun two)
{
  u3_noun l, r;

  if ( (c3y == u3ud(one)) || (c3y == u3ud(two)) ) {
    return c3__void;
  }
  l = u3h(one);
  r = u3h(two);
  if ( (c3n == l) && (c3n == r) ) {
    return u3nc(c3n, 0);
  }
  return u3nc(u3nc(u3k(l), u3k(r)), u3nc(u3k(u3t(one)), u3k(u3t(two))));
}

/* _ev_huge_in(): +huge:so on [c1 d1] and [c2 d2], neither void.
*/
static c3_o
_ev_huge_in(u3_noun c1, u3_noun d1, u3_noun c2, u3_noun d2)
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

    if ( c3n == _ev_huge_in(lc1, u3h(d1), lc2, u3h(d2)) ) {
      return c3n;
    }
    c1 = rc1; d1 = u3t(d1);
    c2 = rc2; d2 = u3t(d2);
  }
}

/* _ev_huge(): +huge:so, RETAIN.
*/
static c3_o
_ev_huge(u3_noun one, u3_noun two)
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
  return _ev_huge_in(u3h(one), u3t(one), u3h(two), u3t(two));
}

/* _ev_bat_of(): +ev-bat, RETAIN; the battery, or none.
*/
static u3_weak
_ev_bat_of(u3_noun cor)
{
  u3_noun b = _ev_pull(cor, 2);
  u3_noun pro;

  if ( c3y == u3ud(b) ) {
    return u3_none;
  }
  if ( (c3n == _ev_all(u3h(b))) || (c3y == u3ud(u3t(b))) ) {
    u3z(b);
    return u3_none;
  }
  pro = u3k(u3t(b));
  u3z(b);
  return pro;
}

/* registrations
*/

/* _ev_ju_tap(): ~(tap in (~(get ju jug) key)), RETAIN.
*/
static u3_noun
_ev_ju_tap(u3_noun jug, u3_noun key)
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

/* _ev_chain_in(): +chain-cape, RETAIN; the cape, or none.
*/
static u3_weak
_ev_chain_in(_ev_t* ev, u3_noun bus)
{
  u3_noun bat, bad, paz, i;

  if ( (c3y == u3ud(bus)) || (c3y == u3ud(u3t(bus))) ) {
    return u3_none;
  }
  bat = _ev_pull(bus, 2);
  if ( c3y == u3ud(bat) ) {
    return u3_none;
  }
  if ( c3n == _ev_all(u3h(bat)) ) {
    u3z(bat);
    return u3_none;
  }
  bad = u3t(bat);
  if ( c3y == u3ud(bad) ) {
    u3z(bat);
    return u3_none;
  }
  if (  (c3y == u3qdb_has(ev->rot, u3t(bus)))
     && (c3y == _ev_all(u3h(bus))) )
  {
    u3z(bat);
    return c3y;
  }
  paz = _ev_ju_tap(ev->bat, bad);
  u3z(bat);

  for ( i = paz; u3_nul != i; i = u3t(i) ) {
    u3_noun tez = _ev_ju_tap(ev->kor, u3h(i));
    u3_noun j;

    for ( j = tez; u3_nul != j; j = u3t(j) ) {
      u3_noun tem = u3h(j);

      if ( c3y == u3ud(tem) ) {
        continue;
      }
      if ( c3y == _ev_huge(tem, bus) ) {
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

/* _ev_chain(): +chain-cape of the subject at axis n, RETAIN, cached.
*/
static u3_weak
_ev_chain(_ev_t* ev, u3_atom n)
{
  u3_weak hit = u3h_get(ev->cha_p, n);
  u3_weak pro;

  if ( u3_none != hit ) {
    if ( u3_nul == hit ) {
      return u3_none;
    }
    pro = u3k(u3t(hit));
    u3z(hit);
    return pro;
  }
  {
    u3_noun bus = _ev_pull(ev->suk, n);
    pro = _ev_chain_in(ev, bus);
    u3z(bus);
  }
  u3h_put(ev->cha_p, n, ( u3_none == pro ) ? u3_nul : u3nc(u3_nul, u3k(pro)));
  return pro;
}

/* _ev_code_in(): +code, RETAIN.
*/
static c3_o
_ev_code_in(_ev_t* ev, u3_atom a)
{
  u3_noun n = 1, r = u3k(a);
  c3_o    pro;

  while ( 1 ) {
    u3_weak t = _ev_chain(ev, n);

    if ( u3_none != t ) {
      c3_o all_o = _ev_all(_ev_lot(t, r));
      u3z(t);
      if ( c3y == all_o ) {
        pro = c3y;
        break;
      }
    }
    if ( c3y == u3qdi_has(ev->cal, n) ) {
      if ( 1 == r ) {
        r = u3qc_peg(n, 3);
        u3z(n);
        n = 1;
        continue;
      }
      if ( 2 == _ev_cap(r) ) {
        pro = c3y;
        break;
      }
      {
        u3_noun nn = u3qc_peg(n, 3);
        u3_noun nr = _ev_mas(r);
        u3z(n); u3z(r);
        n = nn; r = nr;
      }
      continue;
    }
    if ( 1 == r ) {
      pro = c3n;
      break;
    }
    {
      u3_noun nn = u3qc_peg(n, _ev_cap(r));
      u3_noun nr = _ev_mas(r);
      u3z(n); u3z(r);
      n = nn; r = nr;
    }
  }
  u3z(n);
  u3z(r);
  return pro;
}

/* _ev_code(): +code for the current cal, cached.
*/
static c3_o
_ev_code(_ev_t* ev, u3_atom a)
{
  u3_weak hit = u3h_get(ev->cod_p, a);
  c3_o    pro;

  if ( u3_none != hit ) {
    return hit;
  }
  pro = _ev_code_in(ev, a);
  u3h_put(ev->cod_p, a, pro);
  return pro;
}

/* _ev_cal(): the run called an arm of the core at axis a.
*/
static void
_ev_cal(_ev_t* ev, u3_atom a)
{
  if ( c3n == u3qdi_has(ev->cal, a) ) {
    u3_noun cal = u3qdi_put(ev->cal, a);
    u3z(ev->cal);
    ev->cal = cal;
    u3h_free(ev->cod_p);
    ev->cod_p = u3h_new();
  }
}

/* values
*/

/* _ev_cons(): +cons, TRANSFER.
*/
static u3_noun
_ev_cons(u3_noun h, u3_noun t)
{
  if ( (EV_K == u3h(h)) && (EV_K == u3h(t)) ) {
    u3_noun pro = u3nc(EV_K, u3nc(u3k(u3t(h)), u3k(u3t(t))));
    u3z(h);
    u3z(t);
    return pro;
  }
  return u3nt(EV_C, h, t);
}

/* _ev_has(): +has, the sock knows the subject has axis c.
*/
static c3_o
_ev_has(_ev_t* ev, u3_atom c)
{
  u3_noun s = ev->suk;
  u3_noun cap, dat;
  c3_w    i_w;

  if ( 1 == c ) {
    return c3y;
  }
  if ( c3y == u3ud(s) ) {
    return c3n;
  }
  cap = u3h(s);
  dat = u3t(s);
  i_w = u3r_met(0, c) - 1;
  while ( i_w ) {
    c3_b bit_b;

    if ( c3y == u3ud(cap) ) {
      if ( c3n == cap ) {
        return c3n;
      }
      while ( i_w ) {
        if ( c3y == u3ud(dat) ) {
          return c3n;
        }
        dat = u3r_bit(i_w - 1, c) ? u3t(dat) : u3h(dat);
        i_w--;
      }
      return c3y;
    }
    if ( c3y == u3ud(dat) ) {
      return c3n;
    }
    bit_b = u3r_bit(i_w - 1, c);
    cap = bit_b ? u3t(cap) : u3h(cap);
    dat = bit_b ? u3t(dat) : u3h(dat);
    i_w--;
  }
  return c3y;
}

/* _ev_slot(): +slot, RETAIN.
*/
static u3_weak
_ev_slot(_ev_t* ev, u3_noun v, u3_atom b)
{
  u3_noun axe;
  u3_weak pro;

  if ( 0 == b ) {
    return u3_none;
  }
  axe = u3k(b);
  while ( 1 ) {
    if ( 1 == axe ) {
      pro = u3k(v);
      break;
    }
    if ( EV_K == u3h(v) ) {
      u3_weak n = u3r_at(axe, u3t(v));
      if ( u3_none == n ) {
        EV_WHY(ev, EV_WHY_SLOT, b);
        pro = u3_none;
      }
      else {
        pro = u3nc(EV_K, u3k(n));
      }
      break;
    }
    if ( EV_C == u3h(v) ) {
      u3_noun nax;
      v   = ( 2 == _ev_cap(axe) ) ? u3h(u3t(v)) : u3t(u3t(v));
      nax = _ev_mas(axe);
      u3z(axe);
      axe = nax;
      continue;
    }
    {
      u3_noun c = u3qc_peg(u3t(v), axe);
      if ( c3n == _ev_has(ev, c) ) {
        EV_WHY(ev, EV_WHY_SLOT, c);
        u3z(c);
        pro = u3_none;
      }
      else {
        pro = u3nc(EV_R, c);
      }
      break;
    }
  }
  u3z(axe);
  return pro;
}

/* _ev_real(): +real, RETAIN.
*/
static u3_weak
_ev_real(_ev_t* ev, u3_noun v, c3_o cod_o)
{
  switch ( u3h(v) ) {
    default: return u3m_bail(c3__fail);

    case EV_K: return u3k(u3t(v));

    case EV_C: {
      u3_weak h, t = u3_none;

      if ( ++ev->dep_w > EV_DEEP ) {
        ev->pun_o = c3y;
        ev->dep_w--;
        return u3_none;
      }
      h = _ev_real(ev, u3h(u3t(v)), cod_o);
      if ( u3_none != h ) {
        t = _ev_real(ev, u3t(u3t(v)), cod_o);
      }
      ev->dep_w--;
      if ( u3_none == t ) {
        if ( u3_none != h ) {
          u3z(h);
        }
        return u3_none;
      }
      return u3nc(h, t);
    }

    case EV_R: {
      u3_noun a = u3t(v);
      u3_noun s, pro, pat, rid;

      if ( (c3n == cod_o) && (c3n == _ev_code(ev, a)) ) {
        EV_WHY(ev, EV_WHY_DATA, a);
        return u3_none;
      }
      s = _ev_pull(ev->suk, a);
      if ( c3y == u3ud(s) ) {
        EV_WHY(ev, EV_WHY_HIDE, a);
        return u3_none;
      }
      if ( c3n == _ev_all(u3h(s)) ) {
        EV_WHY(ev, EV_WHY_HIDE, a);
        u3z(s);
        return u3_none;
      }
      pro = u3k(u3t(s));
      u3z(s);
      pat = _ev_pat(a);
      rid = _ev_uni(ev->rid, pat);
      u3z(pat);
      u3z(ev->rid);
      ev->rid = rid;
      return pro;
    }
  }
}

/* _ev_cell(): +cell, RETAIN; c3y, c3n or none.
*/
static u3_weak
_ev_cell(_ev_t* ev, u3_noun v)
{
  switch ( u3h(v) ) {
    default:   return u3m_bail(c3__fail);
    case EV_K: return u3du(u3t(v));
    case EV_C: return c3y;
    case EV_R: {
      u3_weak z = _ev_real(ev, v, c3n);
      c3_o    pro;

      if ( u3_none == z ) {
        return u3_none;
      }
      pro = u3du(z);
      u3z(z);
      return pro;
    }
  }
}

/* _ev_edit_in(): +edit's loop, RETAIN.
*/
static u3_weak
_ev_edit_in(_ev_t* ev, u3_noun v, u3_atom b, u3_noun new)
{
  u3_weak l, r, e;
  u3_noun m;

  if ( 1 == b ) {
    return u3k(new);
  }
  if ( u3_none == (l = _ev_slot(ev, v, 2)) ) {
    return u3_none;
  }
  if ( u3_none == (r = _ev_slot(ev, v, 3)) ) {
    u3z(l);
    return u3_none;
  }
  m = _ev_mas(b);
  if ( 2 == _ev_cap(b) ) {
    e = _ev_edit_in(ev, l, m, new);
    u3z(m);
    u3z(l);
    if ( u3_none == e ) {
      u3z(r);
      return u3_none;
    }
    return _ev_cons(e, r);
  }
  else {
    e = _ev_edit_in(ev, r, m, new);
    u3z(m);
    u3z(r);
    if ( u3_none == e ) {
      u3z(l);
      return u3_none;
    }
    return _ev_cons(l, e);
  }
}

/* _ev_sock_of(): +sock-of, RETAIN.
*/
static u3_noun
_ev_sock_of(_ev_t* ev, u3_noun v)
{
  switch ( u3h(v) ) {
    default:   return u3m_bail(c3__fail);
    case EV_K: return u3nc(c3y, u3k(u3t(v)));
    case EV_R: return _ev_pull(ev->suk, u3t(v));
    case EV_C: {
      u3_noun h   = _ev_sock_of(ev, u3h(u3t(v)));
      u3_noun t   = _ev_sock_of(ev, u3t(u3t(v)));
      u3_noun pro = _ev_knit(h, t);
      u3z(h);
      u3z(t);
      return pro;
    }
  }
}

/* jets
*/
static const c3_m _ev_pure_m[] = {
  c3_s3('d','e','c'), c3_s3('a','d','d'), c3_s3('s','u','b'),
  c3_s3('m','u','l'), c3_s3('d','i','v'), c3_s3('m','o','d'),
  c3_s3('d','v','r'), c3_s3('g','t','e'), c3_s3('g','t','h'),
  c3_s3('l','t','e'), c3_s3('l','t','h'), c3_s3('m','a','x'),
  c3_s3('m','i','n'), c3_s3('b','e','x'), c3_s3('c','a','n'),
  c3_s3('c','a','t'), c3_s3('c','u','t'), c3_s3('e','n','d'),
  c3_s3('l','s','h'), c3_s3('m','e','t'), c3_s3('r','a','p'),
  c3_s3('r','e','p'), c3_s3('r','e','v'), c3_s3('r','i','p'),
  c3_s3('r','s','h'), c3_s3('s','w','p'), c3_s3('s','q','t'),
  c3_s3('m','i','x'), c3_s3('c','o','n'), c3_s3('d','i','s'),
  c3_s3('p','o','w'), c3_s3('x','e','b'), c3_s3('f','i','l'),
  c3_s3('m','u','g'), c3_s3('m','u','k'), c3_s3('g','o','r'),
  c3_s3('m','o','r'), c3_s3('d','o','r'), c3_s3('a','o','r'),
  c3_s4('l','e','n','t'), c3_s4('f','l','o','p'), c3_s4('w','e','l','d'),
  c3_s4('s','l','a','g'), c3_s4('s','c','a','g'), c3_s4('t','r','i','p'),
  c3_s4('c','r','i','p'), c3_s3('j','a','m'), c3_s3('m','a','t'),
  0
};

static const c3_m _ev_door_m[] = {
  c3_s2('i','n'), c3_s2('b','y'), 0
};

static const c3_m _ev_door_arm_m[] = {
  c3_s3('p','u','t'), c3_s3('g','e','t'), c3_s3('h','a','s'),
  c3_s3('d','e','l'), c3_s3('g','a','s'), c3_s3('t','a','p'),
  c3_s3('w','y','t'), c3_s3('b','i','f'), c3_s3('u','n','i'),
  c3_s3('i','n','t'), c3_s3('d','i','f'), c3_s3('a','p','t'),
  0
};

static c3_o
_ev_in(u3_noun a, const c3_m* tab_m)
{
  if ( c3n == u3a_is_cat(a) ) {
    return c3n;
  }
  for ( ; *tab_m; tab_m++ ) {
    if ( a == *tab_m ) {
      return c3y;
    }
  }
  return c3n;
}

/* _ev_pure(): +pure, RETAIN.
*/
static c3_o
_ev_pure(_ev_t* ev, u3_noun p, u3_noun arm)
{
  u3_noun key = u3nc(u3k(p), u3k(arm));
  c3_o    has = u3qdi_has(ev->hot, key);

  u3z(key);
  if ( (c3n == has) || (c3n == u3du(p)) ) {
    return c3n;
  }
  if ( c3y == _ev_in(u3h(p), _ev_pure_m) ) {
    return c3y;
  }
  return __(  (c3y == u3du(u3t(p)))
           && (c3y == _ev_in(u3h(u3t(p)), _ev_door_m))
           && (c3y == _ev_in(u3h(p), _ev_door_arm_m)) );
}

/* _ev_safe(): +ev-safe.
*/
static c3_o
_ev_safe(u3_noun nam, u3_noun sam)
{
  switch ( nam ) {
    default: return c3y;

    case c3__dec: return __( (c3y == u3ud(sam)) && (0 != sam) );

    case c3__sub: {
      if (  (c3n == u3du(sam))
         || (c3n == u3ud(u3h(sam)))
         || (c3n == u3ud(u3t(sam))) )
      {
        return c3n;
      }
      return u3qa_gte(u3h(sam), u3t(sam));
    }

    case c3__div:
    case c3__mod:
    case c3_s3('d','v','r'): {
      return __(  (c3y == u3du(sam))
               && (c3y == u3ud(u3h(sam)))
               && (c3y == u3ud(u3t(sam)))
               && (0 != u3t(sam)) );
    }
  }
}

/* _ev_jet(): +jet, RETAIN; the product, or none with the state as it was.
*/
static u3_weak
_ev_jet(_ev_t* ev, u3_noun core, u3_noun arm)
{
  u3_noun rid = u3k(ev->rid);
#ifdef U3NC_STAT
  c3_y    why_y = ev->why_y;
  c3_w    wha_w = ev->wha_w;
#endif
  u3_noun paz = u3_nul;
  u3_weak bat = u3_none, kor = u3_none, b;
  u3_noun pro;

  if ( u3_none == (b = _ev_slot(ev, core, 2)) ) {
    goto fail;
  }
  bat = _ev_real(ev, b, c3y);
  u3z(b);
  if ( (u3_none == bat) || (c3y == u3ud(bat)) ) {
    goto fail;
  }
  {
    u3_noun all = _ev_ju_tap(ev->bat, bat);
    u3_noun i;

    for ( i = all; u3_nul != i; i = u3t(i) ) {
      if ( c3y == _ev_pure(ev, u3h(i), arm) ) {
        paz = u3nc(u3k(u3h(i)), paz);
      }
    }
    u3z(all);
    paz = u3kb_flop(paz);
  }
  if ( u3_nul == paz ) {
    goto fail;
  }
  if ( u3_none == (kor = _ev_real(ev, core, c3n)) ) {
    goto fail;
  }
  {
    u3_noun two = u3nc(c3y, u3k(kor));
    c3_o    fit_o = c3n;
    u3_noun i;

    for ( i = paz; (c3n == fit_o) && (u3_nul != i); i = u3t(i) ) {
      u3_noun tez = _ev_ju_tap(ev->kor, u3h(i));
      u3_noun j;

      for ( j = tez; u3_nul != j; j = u3t(j) ) {
        if ( c3y == _ev_huge(u3h(j), two) ) {
          fit_o = c3y;
          break;
        }
      }
      u3z(tez);
    }
    u3z(two);
    if ( c3n == fit_o ) {
      goto fail;
    }
  }
  {
    u3_noun p   = u3h(paz);
    u3_noun nam = ( u3_nul == p ) ? 0 : u3h(p);

    if (  (c3n == u3du(kor))
       || (c3n == u3du(u3t(kor)))
       || (c3n == _ev_safe(nam, u3h(u3t(kor)))) )
    {
      goto fail;
    }
  }
  pro = u3n_nock_on(u3k(kor), u3nt(9, u3k(arm), u3nc(0, 1)));
  ev->fel_d = ( ev->fel_d < EV_JET_COST ) ? 0 : (ev->fel_d - EV_JET_COST);
  u3z(kor);
  u3z(bat);
  u3z(paz);
  u3z(rid);
  return u3nc(EV_K, pro);

fail:
  if ( u3_none != kor ) u3z(kor);
  if ( u3_none != bat ) u3z(bat);
  u3z(paz);
  u3z(ev->rid);
  ev->rid = rid;
#ifdef U3NC_STAT
  ev->why_y = why_y;
  ev->wha_w = wha_w;
#endif
  return u3_none;
}

/* _ev_go(): +go, .*(s f), TRANSFER s and f; the value, or none.
*/
static u3_weak
_ev_go(_ev_t* ev, u3_noun s, u3_noun f)
{
  u3_weak pro = u3_none;
#ifdef U3NC_STAT
  u3_noun now = ev->now;   //  the caller's formula, alive while it is
#endif

  if ( ++ev->dep_w > EV_DEEP ) {
    ev->pun_o = c3y;
    goto done;
  }

  while ( 1 ) {
    u3_noun hf, tf;

    if ( 0 == ev->fel_d ) {
      EV_WHY(ev, EV_WHY_FUEL, 0);
      goto done;
    }
#ifdef U3NC_STAT
    ev->now = f;
#endif
    EV_WHY(ev, EV_WHY_FORM, 0);
    ev->fel_d--;
    if ( c3y == u3ud(f) ) {
      goto done;
    }
    hf = u3h(f);
    tf = u3t(f);

    if ( c3y == u3du(hf) ) {
      u3_weak h, t;

      if ( u3_none == (h = _ev_go(ev, u3k(s), u3k(hf))) ) {
        goto done;
      }
      if ( u3_none == (t = _ev_go(ev, u3k(s), u3k(tf))) ) {
        u3z(h);
        goto done;
      }
      pro = _ev_cons(h, t);
      goto done;
    }

    switch ( hf ) {
      default: goto done;

      case 0: {
        if ( c3y == u3ud(tf) ) {
          pro = _ev_slot(ev, s, tf);
        }
        goto done;
      }

      case 1: {
        pro = u3nc(EV_K, u3k(tf));
        goto done;
      }

      case 2: {
        u3_noun b, c;
        u3_weak x, y, z;

        if ( c3n == u3r_cell(tf, &b, &c) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(b))) ) {
          goto done;
        }
        if ( u3_none == (y = _ev_go(ev, u3k(s), u3k(c))) ) {
          u3z(x);
          goto done;
        }
        if ( u3_none == (z = _ev_real(ev, y, c3y)) ) {
          u3z(x);
          u3z(y);
          goto done;
        }
        //  +bat: axis a.y is in the battery of the core at a.x
        //
        if ( (EV_R == u3h(x)) && (EV_R == u3h(y)) ) {
          u3_noun r = u3qc_peg(u3t(x), 2);
          c3_w    d_w = u3r_met(0, r);
          c3_w    e_w = u3r_met(0, u3t(y));

          if ( e_w >= d_w ) {
            u3_noun q = u3qc_rsh(0, e_w - d_w, u3t(y));
            if ( c3y == u3r_sing(r, q) ) {
              _ev_cal(ev, u3t(x));
            }
            u3z(q);
          }
          u3z(r);
        }
        u3z(y);
        u3z(s); s = x;
        u3z(f); f = z;
        continue;
      }

      case 3: {
        u3_weak x, c;

        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(tf))) ) {
          goto done;
        }
        c = _ev_cell(ev, x);
        u3z(x);
        if ( u3_none != c ) {
          pro = u3nc(EV_K, ( c3y == c ) ? 0 : 1);
        }
        goto done;
      }

      case 4: {
        u3_weak x, z;

        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(tf))) ) {
          goto done;
        }
        z = _ev_real(ev, x, c3n);
        u3z(x);
        if ( u3_none == z ) {
          goto done;
        }
        if ( c3y == u3du(z) ) {
          EV_WHY(ev, EV_WHY_CRAS, 0);
          u3z(z);
          goto done;
        }
        pro = u3nc(EV_K, u3i_vint(z));
        goto done;
      }

      case 5: {
        u3_noun b, c;
        u3_weak x, y, p, q;

        if ( c3n == u3r_cell(tf, &b, &c) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(b))) ) {
          goto done;
        }
        if ( u3_none == (y = _ev_go(ev, u3k(s), u3k(c))) ) {
          u3z(x);
          goto done;
        }
        if ( c3y == u3r_sing(x, y) ) {
          u3z(x);
          u3z(y);
          pro = u3nc(EV_K, 0);
          goto done;
        }
        p = _ev_real(ev, x, c3n);
        u3z(x);
        if ( u3_none == p ) {
          u3z(y);
          goto done;
        }
        q = _ev_real(ev, y, c3n);
        u3z(y);
        if ( u3_none == q ) {
          u3z(p);
          goto done;
        }
        pro = u3nc(EV_K, ( c3y == u3r_sing(p, q) ) ? 0 : 1);
        u3z(p);
        u3z(q);
        goto done;
      }

      case 6: {
        u3_noun b, c, d, nf;
        u3_weak x, z;

        if ( c3n == u3r_trel(tf, &b, &c, &d) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(b))) ) {
          goto done;
        }
        z = _ev_real(ev, x, c3n);
        u3z(x);
        if ( u3_none == z ) {
          goto done;
        }
        if      ( 0 == z ) { nf = u3k(c); }
        else if ( 1 == z ) { nf = u3k(d); }
        else {
          EV_WHY(ev, EV_WHY_CRAS, 0);
          u3z(z);
          goto done;
        }
        u3z(f); f = nf;
        continue;
      }

      case 7: {
        u3_noun b, c, nf;
        u3_weak x;

        if ( c3n == u3r_cell(tf, &b, &c) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(b))) ) {
          goto done;
        }
        nf = u3k(c);
        u3z(s); s = x;
        u3z(f); f = nf;
        continue;
      }

      case 8: {
        u3_noun b, c, nf;
        u3_weak x;

        if ( c3n == u3r_cell(tf, &b, &c) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(b))) ) {
          goto done;
        }
        nf = u3k(c);
        s = _ev_cons(x, s);
        u3z(f); f = nf;
        continue;
      }

      case 9: {
        u3_noun b, c;
        u3_weak x, j, y, z;

        if ( (c3n == u3r_cell(tf, &b, &c)) || (c3n == u3ud(b)) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(c))) ) {
          goto done;
        }
        if ( u3_none != (j = _ev_jet(ev, x, b)) ) {
          u3z(x);
          pro = j;
          goto done;
        }
        if ( u3_none == (y = _ev_slot(ev, x, b)) ) {
          u3z(x);
          goto done;
        }
        z = _ev_real(ev, y, c3y);
        u3z(y);
        if ( u3_none == z ) {
          u3z(x);
          goto done;
        }
        if ( EV_R == u3h(x) ) {
          _ev_cal(ev, u3t(x));
        }
        u3z(s); s = x;
        u3z(f); f = z;
        continue;
      }

      case 10: {
        u3_noun bc, d, b, c;
        u3_weak x, y;

        if (  (c3n == u3r_cell(tf, &bc, &d))
           || (c3n == u3r_cell(bc, &b, &c))
           || (c3n == u3ud(b)) )
        {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(c))) ) {
          goto done;
        }
        if ( u3_none == (y = _ev_go(ev, u3k(s), u3k(d))) ) {
          u3z(x);
          goto done;
        }
        if ( 0 != b ) {
          pro = _ev_edit_in(ev, y, b, x);
        }
        u3z(x);
        u3z(y);
        goto done;
      }

      case 11: {
        u3_noun p, c, b, d, nf;
        u3_weak x, y;

        if ( c3n == u3r_cell(tf, &p, &c) ) {
          goto done;
        }
        if ( c3y == u3ud(p) ) {
          nf = u3k(c);
          u3z(f); f = nf;
          continue;
        }
        u3x_cell(p, &b, &d);
        if ( c3n == u3ud(b) ) {
          goto done;
        }
        if ( u3_none == (x = _ev_go(ev, u3k(s), u3k(d))) ) {
          goto done;
        }
        if ( u3_none == (y = _ev_go(ev, u3k(s), u3k(c))) ) {
          u3z(x);
          goto done;
        }
        if ( c3__fast == b ) {
          u3_noun kor = _ev_sock_of(ev, y);
          u3_weak bat = _ev_bat_of(kor);

          if ( (u3_none != bat) && (c3y == u3qdi_has(ev->faz, bat)) ) {
            u3z(bat);
            u3z(kor);
          }
          else {
            if ( u3_none != bat ) {
              u3_noun faz = u3qdi_put(ev->faz, bat);
              u3z(ev->faz);
              ev->faz = faz;
              u3z(bat);
            }
            ev->fas = u3nc(u3nc(_ev_sock_of(ev, x), kor), ev->fas);
          }
        }
        u3z(x);
        pro = y;
        goto done;
      }
    }
  }

done:
  u3z(s);
  u3z(f);
  ev->dep_w--;
#ifdef U3NC_STAT
  ev->now = now;
#endif
  return pro;
}

/* _ev_eval(): +eval, without the memo; none to punt.
*/
static u3_weak
_ev_eval(u3_noun fol, c3_d fel_d, c3_o cor_o,
         u3_noun suk, u3_noun reg, u3_noun hot)
{
  _ev_t   ev;
  u3_noun arm, new;
  u3_weak r, z = u3_none, pro;

  u3x_quil(reg, &ev.rot, &ev.kor, &ev.bat, &arm, &new);
  ev.suk   = suk;
  ev.hot   = hot;
  ev.fel_d = fel_d;
  ev.rid   = c3n;
  ev.cal   = ( c3y == cor_o ) ? u3qdi_put(u3_nul, 1) : u3_nul;
  ev.fas   = u3_nul;
  ev.faz   = u3_nul;
  ev.dep_w = 0;
  ev.pun_o = c3n;
#ifdef U3NC_STAT
  ev.why_y = EV_WHY_FORM;
  ev.wha_w = 0;
  ev.now   = u3_none;
  ev.wen   = u3_none;
#endif
  ev.cha_p = u3h_new();
  ev.cod_p = u3h_new();

  r = _ev_go(&ev, u3nc(EV_R, 1), u3k(fol));
  if ( u3_none != r ) {
    z = _ev_real(&ev, r, c3n);
    u3z(r);
  }

  if ( c3y == ev.pun_o ) {
    if ( u3_none != z ) u3z(z);
    pro = u3_none;
  }
  else if ( u3_none == z ) {
    pro = u3nc(c3n, u3i_chub(fel_d - ev.fel_d));
  }
  else {
    pro = u3nc(c3y, u3nq(z, u3k(ev.rid), u3k(ev.fas),
                         u3i_chub(fel_d - ev.fel_d)));
  }
  u3we_ev_stat_d[3] += fel_d - ev.fel_d;
#ifdef U3NC_STAT
  if ( (u3_none != pro) && (c3n == u3h(pro)) ) {
    u3we_ev_why_d[ev.why_y][0]++;
    u3we_ev_why_d[ev.why_y][1] += fel_d - ev.fel_d;

    //  U3NC_EV_WHY=N: the failures of N steps or more
    //
    if (  getenv("U3NC_EV_WHY")
       && ((fel_d - ev.fel_d) >= strtoull(getenv("U3NC_EV_WHY"), 0, 10)) )
    {
      static const c3_c* why_c[] = {"form", "fuel", "slot", "data", "hide", "crash"};
      fprintf(stderr, "ev fail: %s at %x, %" PRIu64 " steps, formula %x\r\n",
                      why_c[ev.why_y], ev.wha_w, fel_d - ev.fel_d,
                      u3r_mug(fol));

      //  a try of a hinted formula, as the %memo.%core of a rule's trap
      //
      if (  (11 == u3h(fol)) && (c3y == u3du(u3t(fol)))
         && (c3y == u3du(u3h(u3t(fol)))) )
      {
        u3_noun hin = u3h(u3t(fol));
        u3m_p("  try of hinted formula, hint", u3h(hin));
        if ( c3y == u3du(u3t(hin)) ) u3m_p("  clue", u3t(hin));
      }

      //  where the read lands: the nearest registered core above it, the
      //  axis from that core, and what the sock knows there
      //
      if ( (EV_WHY_DATA == ev.why_y) && (~0u != ev.wha_w) && ev.wha_w ) {
        c3_w n_w = ev.wha_w;

        while ( n_w >= 1 ) {
          u3_noun bus = _ev_pull(suk, n_w);
          u3_weak bat = _ev_bat_of(bus);
          u3_noun paz = ( u3_none == bat ) ? u3_nul : _ev_ju_tap(ev.bat, bat);

          if ( u3_nul != paz ) {
            c3_w r_w = 1, d_w = 0, a_w = ev.wha_w;
            while ( a_w > n_w ) { r_w |= (a_w & 1) << d_w; d_w++; a_w >>= 1; }
            r_w |= (1u << d_w);
            u3m_p("  under core", u3h(paz));
            fprintf(stderr, "  at its axis %u (subject axis %x of core at %x)\r\n",
                            r_w, ev.wha_w, n_w);
            u3z(paz);
            u3z(bat);
            u3z(bus);
            break;
          }
          if ( u3_none != bat ) u3z(bat);
          u3z(bus);
          if ( 1 == n_w ) {
            fprintf(stderr, "  under no registered core\r\n");
            break;
          }
          n_w >>= 1;
        }
        {
          u3_noun k = _ev_pull(suk, ev.wha_w);
          if ( c3y == u3du(k) ) {
            fprintf(stderr, "  known there: cape %s, data mug %x\r\n",
                            ( c3y == u3h(k) ) ? "&" : ( c3n == u3h(k) ) ? "|" : "partial",
                            u3r_mug(u3t(k)));
            if ( c3y == u3h(k) ) {
              u3_noun jam = u3qe_jam(u3t(k));
              c3_w    len_w = u3r_met(3, jam);
              u3z(jam);
              fprintf(stderr, "  value: %u bytes jammed\r\n", len_w);
              if ( len_w < 400 ) u3m_p("  value", u3t(k));
            }
          }
          u3z(k);
        }
        if ( u3_none != ev.wen ) {
          fprintf(stderr, "  in formula %x\r\n", u3r_mug(ev.wen));
          if ( getenv("U3NC_EV_WHY_FOL") ) u3m_p("  formula", ev.wen);
        }
      }
    }
  }
#endif

#ifdef U3NC_STAT
  if ( u3_none != ev.wen ) u3z(ev.wen);
#endif
  u3z(ev.rid);
  u3z(ev.cal);
  u3z(ev.fas);
  u3z(ev.faz);
  u3h_free(ev.cha_p);
  u3h_free(ev.cod_p);
  return pro;
}

/* u3we_ev_eval(): +eval:ev, memoized as the hoon is (~+).
*/
u3_noun
u3we_ev_eval(u3_noun cor)
{
  u3_noun fol, fel, coo, suk, reg, hot;
  u3_weak pro;

  u3x_mean(cor, {u3x_sam_2, &fol}, {u3x_sam_6, &fel}, {u3x_sam_7, &coo},
                {60, &suk}, {122, &reg}, {123, &hot});

  if (  (c3n == u3a_is_cat(fel))
     || ((c3y != coo) && (c3n != coo)) )
  {
    return u3_none;
  }
  u3we_ev_stat_d[0]++;
  if ( u3_none != (pro = u3z_find_m(u3z_memo_toss, c3__eval, cor)) ) {
    u3we_ev_stat_d[1]++;
    return pro;
  }
  pro = _ev_eval(fol, fel, coo, suk, reg, hot);
  if ( u3_none == pro ) {
    u3we_ev_stat_d[2]++;
    return u3_none;
  }
  return u3z_save_m(u3z_memo_toss, c3__eval, cor, pro);
}

/* +core-chains of the SKA lib: .want, and wherever it covers some of a
** registered core of .bus that one of its templates matches (+chain-cape),
** the template's cape too.  An exact port, with the helpers of +ev.
*/

/* _ev_chains(): +core-chains' loop on [cap dat], neither void nor |
**               nor an atom; RETAIN.
*/
static u3_noun
_ev_chains(_ev_t* ev, u3_noun want, u3_noun cap, u3_noun dat)
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
    tem = _ev_chain_in(ev, bus);
    u3z(bus);
  }
  if ( c3y == tem ) {
    return c3y;
  }
  //  +hed:so and +tel:so of [cap dat]: a cape & is & on both sides
  //
  if ( c3y == u3ud(cap) ) { hc = tc = cap; }
  else                    { hc = u3h(cap); tc = u3t(cap); }

  l   = _ev_chains(ev, u3h(want), hc, u3h(dat));
  r   = _ev_chains(ev, u3t(want), tc, u3t(dat));
  out = _ev_con(l, r);
  if ( u3_none == tem ) {
    return out;
  }
  pro = _ev_uni(out, tem);
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
  _ev_t   ev;
#ifdef U3NC_STAT
  c3_d    beg_d = __builtin_ia32_rdtsc();
#endif

  u3x_mean(cor, {u3x_sam_2, &want}, {u3x_sam_6, &bus}, {u3x_sam_7, &reg});

  if ( (c3y == u3ud(want)) || (c3y == u3ud(bus)) ) {
    return u3k(want);
  }
  memset(&ev, 0, sizeof(ev));
  u3x_quil(reg, &ev.rot, &ev.kor, &ev.bat, &arm, &new);
  pro = _ev_chains(&ev, want, u3h(bus), u3t(bus));

#ifdef U3NC_STAT
  u3we_prof_d[5][0] += __builtin_ia32_rdtsc() - beg_d;
  u3we_prof_d[5][1]++;
  u3we_prof_d[5][2]++;
#endif
  return pro;
}
