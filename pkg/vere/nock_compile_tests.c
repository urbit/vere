/// @file

#include "ivory.h"
#include "noun.h"
#include "steel.h"
#include "ur/ur.h"
#include "vere.h"

#include "direct.h"
#include "nock-compile.h"

#include <sys/time.h>

static c3_d    _time(void);
static u3_noun _nock_n(u3_noun bus_fol);
static u3_noun _nock_nc(u3_noun bus_fol);
static u3_weak _run(const c3_c* nam_c, u3_funk fun_f, u3_noun bus, u3_noun fol);

/* _setup(): boot with the ivory pill and the SKA core.
*/
static u3_noun _ride;   //  the +ride gate of the ship's hoon.hoon
static u3_noun _hoon;   //  its context: the hoon core
static u3_noun _typ;    //  the type of the hoon core

static void
_setup(const c3_c* pax_c, const c3_c* src_c)
{
  u3_cue_xeno* sil_u;
  u3_weak      pil, ska;

  u3C.wag_h |= u3o_hashless;
  u3m_boot_lite(1ULL << 31);

  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len,
                                                  u3_Ivory_pill)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  if ( c3n == u3v_boot_lite(pil) ) {
    printf("*** fail _setup 2\n");
    exit(1);
  }
  if ( u3_none == (ska = u3s_cue_xeno_with(sil_u, u3_Steel_pill_len,
                                                  u3_Steel_pill)) ) {
    printf("*** fail _setup 3\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);

  u3d_boot(ska);

  //  evaluate hoon.hoon with compiled nock, so that the SKA core sees
  //  the jet registrations of the standard library; its product is the
  //  +ride gate, whose context is the hoon core
  //
  {
    u3_noun fol = u3ke_cue(u3m_file((c3_c*)pax_c));
    u3_noun gat, rid;

    if ( u3_none == (gat = _run("u3nc hoon", _nock_nc, 0, fol)) ) {
      printf("*** fail _setup 4\n");
      exit(1);
    }
    u3z(fol);

    _ride = gat;
    _hoon = u3k(u3t(u3t(gat)));

    rid = u3v_wish("..ride");
    fprintf(stderr, "hoon core %s the ivory pill's\r\n",
            ( c3y == u3r_sing(rid, _hoon) ) ? "equals" : "differs from");
    u3z(rid);
  }

  //  the type of the hoon core, by compiling hoon.hoon's source with its
  //  own compiler: the type of the file's product, then of its context
  //
  {
    u3_noun txt = u3m_file((c3_c*)src_c);
    c3_d    beg_d = _time();
    u3_noun res = u3n_slam_on(u3k(_ride), u3nc(c3__noun, txt));

    fprintf(stderr, "mint hoon.hoon: %" PRIu64 " us\r\n", _time() - beg_d);

    _typ = u3k(u3h(res));
    u3z(res);

    res  = u3n_slam_on(u3k(_ride), u3nc(_typ, u3i_string("+>")));
    _typ = u3k(u3h(res));
    u3z(res);
  }
}

/* _time(): wall clock, in microseconds.
*/
static c3_d
_time(void)
{
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  return ((c3_d)tim_u.tv_sec * 1000000) + tim_u.tv_usec;
}

/* _nock_n(): .*(bus fol) with the bytecode interpreter.  TRANSFERS.
*/
static u3_noun
_nock_n(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3n_nock_on(bus, fol);
}

/* _nock_nc(): .*(bus fol) with compiled nock.  TRANSFERS.
*/
static u3_noun
_nock_nc(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3nc_nock_on(bus, fol);
}

/* _run(): run fun_f on [bus fol] softly, timing it.  RETAINS.
**         Produces the product, or u3_none after printing the trace.
*/
static u3_weak
_run(const c3_c* nam_c, u3_funk fun_f, u3_noun bus, u3_noun fol)
{
  c3_d    beg_d = _time();
  u3_noun gon   = u3m_soft(0, fun_f, u3nc(u3k(bus), u3k(fol)));
  c3_d    end_d = _time();
  u3_noun pro;

  if ( 0 != u3h(gon) ) {
    u3_pier_punt_goof(nam_c, gon);
    return u3_none;
  }

  fprintf(stderr, "%s: %" PRIu64 " us\r\n", nam_c, end_d - beg_d);

  pro = u3k(u3t(gon));
  u3z(gon);
  return pro;
}

/* _test(): compare u3n and u3nc on [bus fol].  TRANSFERS.
*/
static c3_i
_test(const c3_c* nam_c, u3_noun bus, u3_noun fol, c3_t jet_t)
{
  u3_weak a, b;
  c3_i    ret_i = 1;

  //  twice: the first u3nc run analyzes and compiles
  //
  u3z(_run("u3n  first ", _nock_n, bus, fol));
  u3z(_run("u3nc first ", _nock_nc, bus, fol));
  a = _run("u3n  second", _nock_n, bus, fol);
  b = _run("u3nc second", _nock_nc, bus, fol);

  if ( (u3_none == a) || (u3_none == b) ) {
    fprintf(stderr, "test %s: crashed\r\n", nam_c);
    ret_i = 0;
  }
  else if ( c3n == u3r_sing(a, b) ) {
    fprintf(stderr, "test %s: mismatch\r\n", nam_c);
    u3m_p("u3n ", a);
    u3m_p("u3nc", b);
    ret_i = 0;
  }
  else if ( u3nc_Stat.rin_d ) {
    fprintf(stderr, "test %s: %" PRIu64 " call sites with an unresolved "
                    "jet ring\r\n", nam_c, u3nc_Stat.rin_d);
    ret_i = 0;
  }
  else if ( jet_t && !u3nc_Stat.jet_d ) {
    fprintf(stderr, "test %s: no jet hits\r\n", nam_c);
    ret_i = 0;
  }
  else {
    fprintf(stderr, "test %s: ok\r\n", nam_c);
  }

  u3z(a); u3z(b);
  u3z(bus); u3z(fol);
  return ret_i;
}

/* _gate(): a gate from hoon source, built on the hoon core, with its
**          sample set.  TRANSFERS.
*/
static u3_noun
_gate(const c3_c* src_c, u3_noun sam)
{
  u3_noun res = u3n_slam_on(u3k(_ride), u3nc(u3k(_typ), u3i_string(src_c)));
  u3_noun gat = u3n_nock_on(u3k(_hoon), u3k(u3t(res)));
  u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(sam, u3k(u3t(u3t(gat)))));

  u3z(res);
  u3z(gat);
  return cor;
}

/* _test_ackermann(): ackermann 3 8, with dec from the standard library.
*/
static c3_i
_test_ackermann(void)
{
  u3_noun cor = _gate("|=  [m=@ n=@]\n"
                      "^-  @\n"
                      "?:  =(0 m)  +(n)\n"
                      "?:  =(0 n)  $(m (dec m), n 1)\n"
                      "$(m (dec m), n $(n (dec n)))\n",
                      u3nc(3, 10));

  return _test("ackermann", cor, u3nt(9, 2, u3nc(0, 1)), 1);
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup(( argc > 1 ) ? argv[1] : "pkg/vere/steel/hoon-formula.noun",
         ( argc > 2 ) ? argv[2] : "pkg/vere/steel/hoon.hoon");

  if ( !_test("inc", 42, u3nt(4, 0, 1), 0) ) {
    exit(1);
  }

  if ( !_test_ackermann() ) {
    exit(1);
  }

  //  GC
  //
  u3z(_ride);
  u3z(_hoon);
  u3z(_typ);
  u3m_grab(u3_none);

  fprintf(stderr, "test nock compile: ok\r\n");
  return 0;
}
