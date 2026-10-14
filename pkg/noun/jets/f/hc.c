/// @file
///
/// The hoon compiler in hc/, as +mint in +ut, +vest, and +parse-pile in
/// clay: this is its host. The compiler works on our nouns where they
/// are, and makes its own in the loom, without counting references or
/// losing anything: so each call is run on a road of its own, with
/// u3m_soft_run(), which copies what the call made out of it and drops
/// the rest. Nothing is kept from one call to the next.
///
/// Two things aren't the compiler's to do. A formula a compile runs, for
/// ^~ and the like, is run here, as +mack: hc_host_mack(). And the gate
/// +laze makes for the battery of a core whose arms are being compiled,
/// which a type or a formula can hold, is made by running the kernel's
/// +laze: hc_host_laze(). +laze isn't an arm the kernel says where to
/// find, so it's looked for in the door: _hc_find(). Both are run on
/// roads of their own again, as what runs on the compiler's can free
/// nothing.
///
/// Where the compiler fails, for hoon that doesn't compile or anything
/// else, we say nothing was done, and the kernel compiles it and fails
/// as it would have.

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"
#include "hc/hc.h"

#ifndef U3_OS_windows
#include <pthread.h>
#endif

/* _hc_mode: what's running.
*/
  enum {
    _hc_idle = 0,                       //  nothing
    _hc_look = 1,                       //  _hc_find()
    _hc_work = 2,                       //  a compile
    _hc_test = 3                        //  the kernel, to check one
  };

/* _hc: the host's state.
*/
  static struct {
    c3_t          ini_t;                //  options read: the rest is set
    c3_o          off_o;                //  not to be used
    c3_o          chk_o;                //  check each result against the kernel's
    c3_o          log_o;                //  say what each compile did
    c3_o          any_o;                //  compile for kernels not known too
    c3_w          min_w;                //  cells in the least hoon to compile here
    c3_w          sec_w;                //  seconds a compile may take here
    c3_d          end_d;                //  when the one running is to give up
    c3_o          lat_o;                //  it did
    c3_o          liv_o;                //  the heap is there

    volatile c3_y mod_y;                //  what's running
    volatile c3_o bad_o;                //  a compile was left: wipe before the next
    u3a_road*     rod_u;                //  the road it's running on

    c3_o          ker_o;                //  a kernel is known:
    c3_l          bat_l;                //    the mug of its +ut battery
    c3_l          con_l;                //    and of what's under the door
    c3_w          axe_w;                //    +laze in the door, 0 if not found

    u3_noun       van;                  //  the door of the compile running
    c3_o          pun_o;                //  it met a gate another kernel made
    uint32_t      cel_w;                //  cells it made
    c3_d          got_d;                //  and bytes of arena chunks it had

    hc_noun       sut, gol, gen;        //  what's being compiled,
    hc_noun       fan, rib;             //    and with what
    c3_o          vet_o;
    hc_noun       pax;                  //  or parsed: the path,
    u3_noun       txt;                  //    and a cord or a tape
    c3_z          len_z;
  } _hc;

//  %hemp-141, which the gate +laze makes has in its context
//
#define _hc_hemp  0x3134312d706d6568ULL

/* _hc_know: the kernels the compiler in hc/ compiles as. It follows one
** hoon.hoon, to the noun, and a kernel is that one if its +ut is: each
** of these is the mug of the +ut battery and of what's under the door,
** which is the rest of hoon.hoon, as hoon.hoon compiles itself.
**
**   U3_HC=log says what these are for the kernel that's running.
*/
  static const struct {
    c3_l bat_l;
    c3_l con_l;
  } _hc_know[] = {
    //  urbit/urbit d6ec15fbcb, [%zuse 408], hoon %135, as compiled by
    //  the kernel of the brass pill of 88c6173048
    //
    { 0x237223cf, 0x1ec989b7 }
  };

/* _hc_flee(): control is leaving without returning: see u3m_Flee_f.
*/
static void
_hc_flee(c3_o sig_o)
{
  if ( (_hc_idle != _hc.mod_y)
     && ((c3y == sig_o) || (u3R == _hc.rod_u)) )
  {
    if ( _hc_work == _hc.mod_y ) {
      _hc.bad_o = c3y;
    }
    _hc.mod_y = _hc_idle;
  }
}

/* _hc_init(): read the options, once.
**
**   U3_HC=off      never use the compiler
**   U3_HC=check    run the kernel's own too, and say where they differ
**   U3_HC=log      say what each compile did, and how long it took
**   U3_HC=any      compile for a kernel that isn't in _hc_know, if its
**                  +laze is as expected
**   U3_HC_MIN=n    compile here only hoons of n cells or more
**   U3_HC_SEC=n    give a compile up after n seconds here, and leave it
**                  to the kernel
**
**   or several of check, log and any, as U3_HC=check,any.
*/
static void
_hc_init(void)
{
  c3_c* var_c;

  _hc.ini_t = 1;
  _hc.off_o = c3n;
  _hc.chk_o = c3n;
  _hc.log_o = c3n;
  _hc.any_o = c3n;
  _hc.liv_o = c3n;
  _hc.ker_o = c3n;
  _hc.bad_o = c3n;
  _hc.pun_o = c3n;
  _hc.mod_y = _hc_idle;
  _hc.min_w = 16;
  _hc.sec_w = 30;

  if ( (var_c = getenv("U3_HC")) ) {
    _hc.off_o = __(0 != strstr(var_c, "off"));
    _hc.chk_o = __(0 != strstr(var_c, "check"));
    _hc.log_o = __(0 != strstr(var_c, "log"));
    _hc.any_o = __(0 != strstr(var_c, "any"));
  }
  if ( (var_c = getenv("U3_HC_MIN")) ) {
    _hc.min_w = (c3_w)strtoul(var_c, 0, 10);
  }
  if ( (var_c = getenv("U3_HC_SEC")) ) {
    _hc.sec_w = (c3_w)strtoul(var_c, 0, 10);
  }
  u3m_Flee_f = _hc_flee;
}

/* _hc_room(): the stack that's left, in bytes; 0 if there's no knowing.
*/
static c3_z
_hc_room(void)
{
  static c3_y* low_y = 0;
  c3_y         her_y;

  if ( !low_y ) {
#if defined(U3_OS_osx)
    pthread_t sef_u = pthread_self();

    low_y = (c3_y*)pthread_get_stackaddr_np(sef_u)
            - pthread_get_stacksize_np(sef_u);
#elif defined(U3_OS_linux)
    pthread_attr_t tar_u;
    void*          low_v;
    size_t         len_i;

    if ( 0 != pthread_getattr_np(pthread_self(), &tar_u) ) {
      return 0;
    }
    if ( 0 != pthread_attr_getstack(&tar_u, &low_v, &len_i) ) {
      pthread_attr_destroy(&tar_u);
      return 0;
    }
    pthread_attr_destroy(&tar_u);
    low_y = low_v;
#else
    return 0;
#endif
  }

  return ( &her_y > low_y ) ? (c3_z)(&her_y - low_y) : 0;
}

/* _hc_now(): the time, in microseconds.
*/
static c3_d
_hc_now(void)
{
  struct timeval tim_u;

  gettimeofday(&tim_u, 0);
  return ((c3_d)tim_u.tv_sec * 1000000ULL) + (c3_d)tim_u.tv_usec;
}

/* _hc_laze(): the gate +laze makes in the door [von] of [nym hud dom],
** all retained; u3_none if it makes none. Run on a road of its own.
*/
static u3_weak
_hc_laze(u3_noun von, u3_noun nym, u3_noun hud, u3_noun dom)
{
  u3_noun sam = u3nt(u3k(nym), u3k(hud), u3k(dom));
  u3_noun fol = u3nt(9, 2, u3nt(10, u3nt(6, 1, sam), u3nq(9, _hc.axe_w, 0, 1)));
  u3_noun ton = u3n_nock_an(u3k(von), fol);
  u3_noun pro;

  //  [%0 [%lazy 1 gate] ~]; and nothing of it is lost, as it may be on
  //  the compiler's road
  //
  if (  (0 != u3h(ton))
     || (c3n == u3du(pro = u3t(ton)))
     || (c3n == u3du(u3h(pro)))
     || (c3n == u3du(u3t(u3h(pro)))) )
  {
    return u3_none;
  }
  return u3t(u3t(u3h(pro)));
}

/* _hc_gate_like(): whether [som] has the shape of a gate +laze makes,
** [bat axe tal %hemp-141 bat [nym hud dom] von]; if so, its parts.
*/
static inline c3_o
_hc_gate_like(u3_noun som, u3_noun* tal, u3_noun* sam, u3_noun* von)
{
  u3_noun tel = u3t(som);
  u3_noun hem;

  if ( c3n == u3du(tel) )  return c3n;      //  [axe tal %hemp-141 ...]
  tel = u3t(tel);
  if ( c3n == u3du(tel) )  return c3n;      //  [tal %hemp-141 ...]
  *tal = u3h(tel);
  tel  = u3t(tel);
  if ( c3n == u3du(tel) )  return c3n;      //  [%hemp-141 bat ...]
  hem = u3h(tel);
  if ( c3n == u3ud(hem) )  return c3n;
  tel = u3t(tel);
  if ( c3n == u3du(tel) )  return c3n;      //  [bat [nym hud dom] von]
  tel = u3t(tel);
  if ( c3n == u3du(tel) )  return c3n;      //  [[nym hud dom] von]

  if (  (8 != u3r_met(3, hem))
     || (_hc_hemp != u3r_chub(0, hem)) )
  {
    return c3n;
  }

  *sam = u3h(tel);
  *von = u3t(tel);

  return __(  (c3y == u3du(*sam))
           && (c3y == u3du(u3t(*sam)))
           && (c3y == u3du(*von)) );
}

/* _hc_ours(): whether [von] is a +ut door of the kernel that's
** compiling, as [van] is.
*/
static c3_o
_hc_ours(u3_noun von)
{
  u3_weak bat = u3r_at(2, von);
  u3_weak con = u3r_at(15, von);

  return __(  (u3_none != bat)
           && (u3_none != con)
           && (_hc.bat_l == u3r_mug(bat))
           && (_hc.con_l == u3r_mug(con))
           && hc_same(hc_of(bat), hc_of(u3h(_hc.van)))
           && hc_same(hc_of(con), hc_of(u3x_at(15, _hc.van))) );
}

/* hc_host_laze(): the gate +laze makes of [fom],
** [sut nym hud dom vet fan rib].
*/
hc_noun
hc_host_laze(hc_noun fom)
{
  u3_noun mof = hc_to(fom);
  u3_noun sut = u3h(mof);            mof = u3t(mof);
  u3_noun nym = u3h(mof);            mof = u3t(mof);
  u3_noun hud = u3h(mof);            mof = u3t(mof);
  u3_noun dom = u3h(mof);            mof = u3t(mof);
  u3_noun vet = u3h(mof);            mof = u3t(mof);
  u3_noun fan = u3h(mof);
  u3_noun rib = u3t(mof);
  u3_noun von;
  u3_weak gat;

  von = u3i_edit(u3k(_hc.van), u3x_sam, u3k(sut));
  von = u3i_edit(von, u3qfu_van_fan, u3k(fan));
  von = u3i_edit(von, u3qfu_van_rib, u3k(rib));
  von = u3i_edit(von, u3qfu_van_vet, u3k(vet));
  gat = _hc_laze(von, nym, hud, dom);

  if ( u3_none == gat ) {
    _hc.pun_o = c3y;
    return hc_of(0);
  }
  return hc_of(gat);
}

/* hc_host_from(): what the gate [gat], found in a type, was made from,
** [sut nym hud dom vet fan rib], if the kernel that's compiling made
** it: it did if the gate is what its +laze makes of what's in it. 0 if
** not, and the compile is to fail.
*/
hc_noun
hc_host_from(hc_noun gat)
{
  u3_noun tag = hc_to(gat);
  u3_noun tal, sam, von;
  u3_weak sut, fan, rib, vet, new;
  hc_noun fom;

  if (  (c3n == u3du(tag))
     || (c3n == _hc_gate_like(tag, &tal, &sam, &von))
     || (c3n == _hc_ours(von))
     || (u3_none == (sut = u3r_at(u3x_sam, von)))
     || (u3_none == (fan = u3r_at(u3qfu_van_fan, von)))
     || (u3_none == (rib = u3r_at(u3qfu_van_rib, von)))
     || (u3_none == (vet = u3r_at(u3qfu_van_vet, von)))
     || (u3_none == (new = _hc_laze(von, u3h(sam), u3h(u3t(sam)), u3t(u3t(sam)))))
     || !hc_same(hc_of(new), gat) )
  {
    _hc.pun_o = c3y;
    return 0;
  }

  fom = hc_cons(hc_of(fan), hc_of(rib));
  fom = hc_cons(hc_of(vet), fom);
  fom = hc_cons(hc_of(u3t(u3t(sam))), fom);
  fom = hc_cons(hc_of(u3h(u3t(sam))), fom);
  fom = hc_cons(hc_of(u3h(sam)), fom);
  return hc_cons(hc_of(sut), fom);
}

/* hc_host_over(): whether the compile has run too long.
*/
int
hc_host_over(void)
{
  if ( _hc_now() > _hc.end_d ) {
    _hc.lat_o = c3y;
    return 1;
  }
  return 0;
}

/* hc_host_mack(): (mack cor [9 axe 0 1]), 0 for ~. Nothing of what
** comes back is lost, as it's on the compiler's road.
*/
hc_noun
hc_host_mack(hc_noun cor, hc_noun axe)
{
  u3_noun fol = u3nq(9, u3k(hc_to(axe)), 0, 1);
  u3_noun ton = u3n_nock_an(u3k(hc_to(cor)), fol);

  return ( 0 == u3h(ton) ) ? hc_of(u3t(ton)) : 0;
}

/* _hc_has_hemp(): whether %hemp-141 is in a formula, looking at no more
** than [*lef_w] nouns.
*/
static c3_o
_hc_has_hemp(u3_noun fol, c3_w* lef_w)
{
  while ( *lef_w ) {
    (*lef_w)--;

    if ( c3y == u3ud(fol) ) {
      return __(  (8 == u3r_met(3, fol))
               && (_hc_hemp == u3r_chub(0, fol)) );
    }
    if ( c3y == _hc_has_hemp(u3h(fol), lef_w) ) {
      return c3y;
    }
    fol = u3t(fol);
  }
  return c3n;
}

/* _hc_try(): whether the arm at [axe_w] of the door [van] is +laze: it
** is if, of a core of one arm, it makes the gate +laze would.
*/
static c3_o
_hc_try(u3_noun van, c3_w axe_w)
{
  //  +hemp has %hemp-141 in it too, and isn't to be given what +laze
  //  takes: of [%dry %noun .] it makes a formula, and +laze nothing
  //
  {
    u3_noun sam = u3nt(c3__dry, c3__noun, u3nc(0, 1));
    u3_noun fol = u3nt(9, 2, u3nt(10, u3nt(6, 1, sam), u3nq(9, axe_w, 0, 1)));
    u3_noun ton = u3n_nock_an(u3k(van), fol);
    c3_o    hem_o = __(0 == u3h(ton));

    u3z(ton);
    if ( c3y == hem_o ) {
      return c3n;
    }
  }

  //  of [~ %dry {[%$ {[%a .]}]}], +laze makes
  //
  //    [[%lazy 1 bat 0 {[1 .]} %hemp-141 bat [~ %dry dom] van] ~]
  //
  {
    u3_noun gen = u3nc(0, 1);
    u3_noun dom = u3nt(u3nc(0, u3nt(u3nc('a', u3k(gen)), 0, 0)), 0, 0);
    u3_noun sam = u3nt(0, c3__dry, dom);
    u3_noun tal = u3nt(u3nc(1, gen), 0, 0);
    u3_noun fol = u3nt(9, 2, u3nt(10, u3nt(6, 1, u3k(sam)), u3nq(9, axe_w, 0, 1)));
    u3_noun ton = u3n_nock_an(u3k(van), fol);
    u3_noun pro, lay;
    c3_o    ret_o = c3n;

    if (  (0 == u3h(ton))
       && (c3y == u3du(pro = u3t(ton)))
       && (0 == u3t(pro))
       && (c3y == u3du(u3h(pro)))
       && (c3y == u3r_sing_c("lazy", u3h(u3h(pro))))
       && (c3y == u3du(u3t(u3h(pro))))
       && (1 == u3h(u3t(u3h(pro))))
       && (c3y == u3du(lay = u3t(u3t(u3h(pro))))) )
    {
      u3_noun lat, las, lav;

      ret_o = __(  (c3y == _hc_gate_like(lay, &lat, &las, &lav))
                && (0 == u3h(u3t(lay)))
                && (c3y == u3r_sing(tal, lat))
                && (c3y == u3r_sing(sam, las))
                && (c3y == u3r_sing(van, lav)) );
    }
    u3z(ton);
    u3z(tal);
    u3z(sam);
    return ret_o;
  }
}

/* _hc_find_in(): look for +laze among the arms of a battery, [bat] at
** [axe_w] in the door [van]; its axis there, or 0.
*/
static c3_w
_hc_find_in(u3_noun van, u3_noun bat, c3_w axe_w)
{
  c3_w pro_w;

  if ( (c3n == u3du(bat)) || (axe_w >= (1 << 28)) ) {
    return 0;
  }
  //  a formula's head is an atom, and a battery's another battery, or a
  //  formula
  //
  if ( c3y == u3ud(u3h(bat)) ) {
    c3_w lef_w = 1 << 20;

    return (  (c3y == _hc_has_hemp(bat, &lef_w))
           && (c3y == _hc_try(van, axe_w)) )
           ? axe_w : 0;
  }
  if ( (pro_w = _hc_find_in(van, u3h(bat), 2 * axe_w)) ) {
    return pro_w;
  }
  return _hc_find_in(van, u3t(bat), (2 * axe_w) + 1);
}

/* _hc_find(): look for +laze in the door [van]; its axis, or 0.
*/
static c3_w
_hc_find(u3_noun van)
{
  c3_w pro_w;

  _hc.rod_u = u3R;
  _hc.mod_y = _hc_look;
  pro_w     = _hc_find_in(van, u3h(van), 2);
  _hc.mod_y = _hc_idle;

  return pro_w;
}

/* _hc_fresh(): forget what the compiler kept of a call that was left.
*/
static void
_hc_fresh(void)
{
  hc_wipe();
  _hc.bad_o = c3n;
}

/* _hc_known(): whether a kernel is one the compiler compiles as, by
** the mugs of its +ut battery, or 0 for any, and of the rest.
*/
static c3_o
_hc_known(c3_l bat_l, c3_l con_l)
{
  c3_w i_w;

  for ( i_w = 0; i_w < (sizeof(_hc_know) / sizeof(_hc_know[0])); i_w++ ) {
    if (  (con_l == _hc_know[i_w].con_l)
       && (!bat_l || (bat_l == _hc_know[i_w].bat_l)) )
    {
      return c3y;
    }
  }
  return _hc.any_o;
}

/* _hc_up(): whether the compiler can be used: it's there, with its
** heap, and isn't running.
*/
static c3_o
_hc_up(void)
{
  if ( !_hc.ini_t ) {
    _hc_init();
  }
  if ( (c3y == _hc.off_o) || (_hc_idle != _hc.mod_y) ) {
    return c3n;
  }

  if ( c3n == _hc.liv_o ) {
    if ( !hc_boot() ) {
      u3l_log("hc: no memory for the compiler: the kernel's own compiles");
      _hc.off_o = c3y;
      return c3n;
    }
    _hc.liv_o = c3y;
  }
  if ( c3y == _hc.bad_o ) {
    //  what's in the loom of the compile that was left went with its road
    //
    _hc_fresh();
  }
  return c3y;
}

/* _hc_ready(): whether the compiler can be used, with the door [van].
*/
static c3_o
_hc_ready(u3_noun van)
{
  u3_weak bat, con;

  if ( c3n == _hc_up() ) {
    return c3n;
  }

  if (  (u3_none == (bat = u3r_at(2, van)))
     || (u3_none == (con = u3r_at(15, van)))
     || (u3_none == u3r_at(u3qfu_van_vet, van)) )
  {
    return c3n;
  }

  //  where +laze is in a kernel's +ut is found once
  //
  if (  (c3n == _hc.ker_o)
     || (_hc.bat_l != u3r_mug(bat))
     || (_hc.con_l != u3r_mug(con)) )
  {
    c3_l bat_l = u3r_mug(bat);
    c3_l con_l = u3r_mug(con);
    c3_o kno_o = _hc_known(bat_l, con_l);
    c3_w axe_w = 0;

    _hc.ker_o = c3n;

    if ( c3y == kno_o ) {
      axe_w = _hc_find(van);
    }
    if ( c3y == _hc.log_o ) {
      u3l_log("hc: +ut { 0x%x, 0x%x }: %s", (c3_h)bat_l, (c3_h)con_l,
              ( c3n == kno_o ) ? "not a kernel known here"
              : axe_w ? "compiling" : "no +laze as expected");
    }

    _hc.bat_l = u3r_mug(bat);
    _hc.con_l = u3r_mug(con);
    _hc.axe_w = axe_w;
    _hc.ker_o = c3y;
  }

  return __(0 != _hc.axe_w);
}

/* _hc_big(): whether a noun has [*lef_w] cells or more.
*/
static c3_o
_hc_big(u3_noun som, c3_w* lef_w)
{
  while ( c3y == u3du(som) ) {
    if ( !*lef_w ) {
      return c3y;
    }
    (*lef_w)--;

    if ( c3y == _hc_big(u3h(som), lef_w) ) {
      return c3y;
    }
    som = u3t(som);
  }
  return __(0 == *lef_w);
}

/* _hc_spot(): the path in the first spot found in a hoon, looking at
** no more than [*lef_w] cells; u3_none if there's none. Retained.
*/
static u3_weak
_hc_spot(u3_noun gen, c3_w* lef_w)
{
  while ( (c3y == u3du(gen)) && *lef_w ) {
    u3_noun hed = u3h(gen);
    u3_noun tel = u3t(gen);

    (*lef_w)--;

    //  [%dbug [path pint] hoon]
    //
    if (  (c3__dbug == hed)
       && (c3y == u3du(tel))
       && (c3y == u3du(u3h(tel)))
       && (c3y == u3du(u3h(u3h(tel))))
       && (c3y == u3ud(u3h(u3h(u3h(tel))))) )
    {
      return u3h(u3h(tel));
    }
    if ( c3y == u3du(hed) ) {
      u3_weak pro = _hc_spot(hed, lef_w);

      if ( u3_none != pro ) {
        return pro;
      }
    }
    gen = tel;
  }
  return u3_none;
}

/* _hc_work_mint(), _hc_work_vest(), _hc_work_pile(): the compiler, on
** the road u3m_soft_run() made for it: what it makes, or 0.
*/
static u3_noun
_hc_work_mint(u3_noun aga, u3_noun agb)
{
  c3_z    rom_z = _hc_room();
  hc_noun res;

  if ( (rom_z < (4 << 20)) || setjmp(hc_fail) ) {
    return 0;
  }
  hc_arm(1);
  res = hc_mint(_hc.sut, _hc.gol, _hc.gen, _(_hc.vet_o), _hc.fan, _hc.rib,
                rom_z - (2 << 20));
  hc_arm(0);
  return ( res && (c3n == _hc.pun_o) ) ? hc_to(res) : 0;
}

static u3_noun
_hc_work_vest(u3_noun aga, u3_noun agb)
{
  hc_noun res;
  c3_y*   txt_y;
  u3_noun tep;
  c3_z    i_z = 0;

  if ( setjmp(hc_fail) ) {
    return 0;
  }
  hc_arm(1);
  txt_y = hc_text(_hc.len_z);

  for ( tep = _hc.txt; c3y == u3du(tep); tep = u3t(tep) ) {
    txt_y[i_z++] = (c3_y)u3h(tep);
  }
  res = hc_vest(txt_y, _hc.len_z);
  hc_arm(0);
  return res ? hc_to(res) : 0;
}

static u3_noun
_hc_work_pile(u3_noun aga, u3_noun agb)
{
  hc_noun res;
  c3_y*   txt_y;

  if ( setjmp(hc_fail) ) {
    return 0;
  }
  hc_arm(1);
  txt_y = hc_text(_hc.len_z);
  u3r_bytes(0, _hc.len_z, txt_y, _hc.txt);
  res = hc_pile(txt_y, _hc.len_z, _hc.pax);
  hc_arm(0);
  return res ? hc_to(res) : 0;
}

/* _hc_path(): a path as text, in [buf_c], cut short if it's long.
*/
static void
_hc_path(u3_noun pax, c3_c* buf_c, c3_z cap_z)
{
  c3_z len_z = 0;

  for ( ; (c3y == u3du(pax)) && (len_z + 2 < cap_z); pax = u3t(pax) ) {
    u3_noun kot   = u3h(pax);
    c3_z    met_z = ( c3y == u3ud(kot) ) ? u3r_met(3, kot) : 0;

    buf_c[len_z++] = '/';
    if ( met_z > (cap_z - len_z - 1) ) {
      met_z = cap_z - len_z - 1;
    }
    if ( met_z ) {
      u3r_bytes(0, (c3_w)met_z, (c3_y*)buf_c + len_z, kot);
      len_z += met_z;
    }
  }
  buf_c[len_z] = 0;
}

/* _hc_toil(): run the compiler on a road of its own. What it made,
** copied out of that road, or u3_none; and nothing is left of the call.
*/
static u3_weak
_hc_toil(u3_funq fun_f)
{
  u3_weak pro = u3_none;
  u3_noun ton;

  _hc.pun_o = c3n;
  _hc.lat_o = c3n;
  _hc.end_d = _hc_now() + ((c3_d)_hc.sec_w * 1000000ULL);
  _hc.rod_u = u3R;
  _hc.mod_y = _hc_work;

  ton = u3m_soft_run(u3_nul, fun_f, 0, 0);

  _hc.mod_y = _hc_idle;
  _hc.cel_w = hc_cells();
  _hc.got_d = hc_bytes();
  hc_arm(0);
  hc_wipe();

  if ( (0 == u3h(ton)) && (c3y == u3du(u3t(ton))) ) {
    pro = u3k(u3t(ton));
  }
  u3z(ton);
  return pro;
}

/* _hc_differ(): say where two nouns that should be the same aren't.
*/
static void
_hc_differ(u3_noun one, u3_noun two)
{
  c3_w dep_w = 0;

  fprintf(stderr, "hc: differs at +");

  while (  (c3y == u3du(one))
        && (c3y == u3du(two))
        && (dep_w++ < 4096) )
  {
    if ( c3n == u3r_sing(u3h(one), u3h(two)) ) {
      fprintf(stderr, "<");
      one = u3h(one);
      two = u3h(two);
    }
    else {
      fprintf(stderr, ">");
      one = u3t(one);
      two = u3t(two);
    }
  }
  fprintf(stderr, "\r\n");

  if ( (c3y == u3ud(one)) || (c3y == u3ud(two)) ) {
    u3m_p("hc: kernel", one);
    u3m_p("hc: ours  ", two);
  }
}

/* _hc_check(): run the gate [cor] as the kernel has it, and say so if
** what it makes isn't [pro], which is lost. What the kernel made.
*/
static u3_noun
_hc_check(const c3_c* wat_c, u3_noun cor, u3_noun pro)
{
  u3_noun ref;
  c3_d    now_d = _hc_now();

  _hc.rod_u = u3R;
  _hc.mod_y = _hc_test;
  ref       = u3n_nock_on(u3k(cor), u3k(u3x_at(u3x_bat, cor)));
  _hc.mod_y = _hc_idle;

  if ( c3y == _hc.log_o ) {
    u3l_log("hc: %s: the kernel's took %" PRIu64 " ms",
            wat_c, (_hc_now() - now_d) / 1000);
  }

  if ( c3n == u3r_sing(ref, pro) ) {
    u3l_log("hc: %s differs from the kernel's: sample %x",
            wat_c, (c3_h)u3r_mug(u3x_at(u3x_sam, cor)));
    _hc_differ(ref, pro);
  }
  u3z(pro);
  return ref;
}

/* u3qfu_hc_mint(): [type nock] of [gen] against [sut] with [gol] wanted,
** in the +ut door [van] of the +mint core [cor], by the compiler in hc/.
** u3_none if it's not done here. Arguments retained.
*/
u3_weak
u3qfu_hc_mint(u3_noun cor,
              u3_noun van,
              u3_noun sut,
              u3_noun gol,
              u3_noun gen)
{
  u3_weak pro;
  u3_weak fan, rib, vet;
  c3_d    now_d = 0;

  if ( c3n == _hc_ready(van) ) {
    return u3_none;
  }
  {
    c3_w lef_w = _hc.min_w;

    if ( c3n == _hc_big(gen, &lef_w) ) {
      return u3_none;
    }
  }
  if (  (u3_none == (fan = u3r_at(u3qfu_van_fan, van)))
     || (u3_none == (rib = u3r_at(u3qfu_van_rib, van)))
     || (u3_none == (vet = u3r_at(u3qfu_van_vet, van)))
     || (vet > 1) )
  {
    return u3_none;
  }

  if ( c3y == _hc.log_o ) {
    now_d = _hc_now();
  }

  _hc.van   = van;
  _hc.sut   = hc_of(sut);
  _hc.gol   = hc_of(gol);
  _hc.gen   = hc_of(gen);
  _hc.fan   = hc_of(fan);
  _hc.rib   = hc_of(rib);
  _hc.vet_o = __(0 == vet);

  {
    uint32_t cel_w;

    pro   = _hc_toil(_hc_work_mint);
    cel_w = _hc.cel_w;

    if ( c3y == _hc.log_o ) {
      c3_w    lef_w = 1 << 20;
      c3_w    lok_w = 4096;
      u3_weak pax   = _hc_spot(gen, &lok_w);

      _hc_big(gen, &lef_w);

      if ( u3_none != pax ) {
        c3_c pax_c[256];

        _hc_path(pax, pax_c, sizeof(pax_c));
        u3l_log("hc: at %s", pax_c);
      }
      u3l_log("hc: gen %x of %u cells: %s, %" PRIu64 " us, %u cells made, %" PRIu64 " KB of chunks",
              (c3_h)u3r_mug(gen), (c3_h)((1 << 20) - lef_w),
              ( c3y == _hc.pun_o ) ? "punted" : ( u3_none == pro ) ? "failed" : "done",
              _hc_now() - now_d, cel_w, _hc.got_d >> 10);
    }
  }

  //  given up for taking too long, it's the kernel's alone to compile,
  //  all the way down: what took too long here is in there somewhere
  //
  if ( (u3_none == pro) && (c3y == _hc.lat_o) ) {
    _hc.lat_o = c3n;
    _hc.rod_u = u3R;
    _hc.mod_y = _hc_test;
    pro       = u3n_nock_on(u3k(cor), u3k(u3x_at(u3x_bat, cor)));
    _hc.mod_y = _hc_idle;
    return pro;
  }

  if ( (c3y == _hc.chk_o) && (u3_none != pro) ) {
    pro = _hc_check("+mint", cor, pro);
  }
  return pro;
}

/* u3qf_hc_vest(): the edge +vest makes of the nail [tub], in the +vest
** core [cor], by the parser in hc/. u3_none if it's not done here, as
** for text that doesn't parse. Arguments retained.
*/
u3_weak
u3qf_hc_vest(u3_noun cor, u3_noun tub)
{
  u3_weak pro = u3_none;
  u3_weak pen = u3r_at(u3x_con, cor);
  u3_noun tep;
  c3_w    lin_w = 1, col_w = 1;
  c3_z    len_z = 0;
  c3_d    now_d = 0;

  //  [[1 1] tape], the tape of bytes
  //
  if (  (c3n == u3du(tub))
     || (c3n == u3du(u3h(tub)))
     || (1 != u3h(u3h(tub)))
     || (1 != u3t(u3h(tub))) )
  {
    return u3_none;
  }
  for ( tep = u3t(tub); c3y == u3du(tep); tep = u3t(tep) ) {
    if ( u3h(tep) > 0xff ) {
      return u3_none;
    }
    if ( 10 == u3h(tep) ) {
      lin_w++;
      col_w = 1;
    }
    else {
      col_w++;
    }
    len_z++;
  }

  if (  (0 != tep)
     || (len_z >= (1 << 28))
     || (u3_none == pen)
     || (c3n == _hc_up())
     || (c3n == _hc_known(0, u3r_mug(pen))) )
  {
    return u3_none;
  }

  if ( c3y == _hc.log_o ) {
    now_d = _hc_now();
  }

  {
    u3_weak gen;

    _hc.txt   = u3t(tub);
    _hc.len_z = len_z;

    if ( u3_none != (gen = _hc_toil(_hc_work_vest)) ) {
      //  [p=hair q=[~ u=[p=hoon q=[p=hair q=~]]]], both hairs the end
      //
      pro = u3nt(u3nc(lin_w, col_w),
                 u3_nul,
                 u3nt(gen, u3nc(lin_w, col_w), u3_nul));
    }
  }

  if ( c3y == _hc.log_o ) {
    u3l_log("hc: +vest of %zu bytes: %s, %" PRIu64 " ms",
            len_z, ( u3_none == pro ) ? "failed" : "done",
            (_hc_now() - now_d) / 1000);
  }

  if ( (c3y == _hc.chk_o) && (u3_none != pro) ) {
    pro = _hc_check("+vest", cor, pro);
  }
  return pro;
}

/* _hc_know_pile: the mugs of the %clay-utilities batteries whose
** +pile-rule the parser in hc/ parses as. U3_HC=log says what it is for
** the clay that's running.
*/
  static const c3_l _hc_know_pile[] = {
    //  urbit/urbit d6ec15fbcb
    //
    0x5e0b4525
  };

/* u3qf_hc_pile(): the pile +parse-pile makes of the text [txt] of the
** file at [pax], in the +parse-pile core [cor], by the parser in hc/.
** u3_none if it's not done here, as for text that doesn't parse.
** Arguments retained.
*/
u3_weak
u3qf_hc_pile(u3_noun cor, u3_noun pax, u3_noun txt)
{
  u3_weak pro = u3_none;
  u3_weak uti = u3r_at(u3x_con, cor);
  u3_weak pen;
  u3_noun tep;
  c3_z    len_z;
  c3_d    now_d = 0;
  c3_o    kno_o;

  //  %clay-utilities is over clay's types, over the vane's gate, over
  //  zuse, lull, and arvo's cores down to hoon's last: twelve tails
  //
  if (  (c3n == u3ud(txt))
     || (u3_none == uti)
     || (c3n == u3du(uti))
     || (u3_none == (pen = u3r_at(8191, uti)))
     || ((len_z = u3r_met(3, txt)) >= (1 << 28)) )
  {
    return u3_none;
  }
  for ( tep = pax; c3y == u3du(tep); tep = u3t(tep) ) {
    if ( c3n == u3ud(u3h(tep)) ) {
      return u3_none;
    }
  }
  if ( (0 != tep) || (c3n == _hc_up()) ) {
    return u3_none;
  }

  {
    c3_l bat_l = u3r_mug(u3h(uti));
    c3_w i_w;

    kno_o = _hc.any_o;
    for ( i_w = 0; i_w < (sizeof(_hc_know_pile) / sizeof(c3_l)); i_w++ ) {
      if ( bat_l == _hc_know_pile[i_w] ) {
        kno_o = c3y;
      }
    }
    if ( (c3n == kno_o) || (c3n == _hc_known(0, u3r_mug(pen))) ) {
      if ( c3y == _hc.log_o ) {
        u3l_log("hc: +parse-pile in { 0x%x }, hoon { 0x%x }: not known here",
                (c3_h)bat_l, (c3_h)u3r_mug(pen));
      }
      return u3_none;
    }
  }

  if ( c3y == _hc.log_o ) {
    now_d = _hc_now();
  }

  _hc.txt   = txt;
  _hc.len_z = len_z;
  _hc.pax   = hc_of(pax);
  pro       = _hc_toil(_hc_work_pile);

  if ( c3y == _hc.log_o ) {
    c3_c pax_c[256];

    _hc_path(pax, pax_c, sizeof(pax_c));
    u3l_log("hc: +parse-pile of %s, %zu bytes: %s, %" PRIu64 " us",
            pax_c, len_z, ( u3_none == pro ) ? "failed" : "done",
            _hc_now() - now_d);
  }

  if ( (c3y == _hc.chk_o) && (u3_none != pro) ) {
    pro = _hc_check("+parse-pile", cor, pro);
  }
  return pro;
}

/* u3wf_parse_pile(): +parse-pile in clay.
*/
u3_noun
u3wf_parse_pile(u3_noun cor)
{
  u3_noun pax, txt;

  if ( c3n == u3r_mean(cor, {u3x_sam_2, &pax}, {u3x_sam_3, &txt}) ) {
    return u3_none;
  }
  return u3qf_hc_pile(cor, pax, txt);
}

/* u3wf_vest(): +vest.
*/
u3_noun
u3wf_vest(u3_noun cor)
{
  return u3qf_hc_vest(cor, u3x_at(u3x_sam, cor));
}
