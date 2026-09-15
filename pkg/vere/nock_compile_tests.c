/// @file

#include "noun.h"
#include "steel.h"
#include "ur/ur.h"
#include "vere.h"

#include "ivory.h"
#include "direct.h"
#include "nock-compile.h"

#include <sys/time.h>

static c3_d    _time(void);
static u3_noun _scan_nc(u3_noun bus_fol);
static u3_weak _run(const c3_c* nam_c, u3_funk fun_f, u3_noun bus, u3_noun fol);

/* _setup(): boot the kernel from the ivory pill in u3n interpreter, analyze the
**           pill in u3nc, and boot SKA core from steel pill.
*/
static void
_setup(void)
{
  u3_cue_xeno* sil_u;
  u3_weak      pil, ska;
  u3_noun      eve;

  u3C.wag_h |= u3o_hashless;
  u3C.hap_w  = 50000;   //  memo cache sizes, as in the runtime
  u3C.per_w  = 50000;
  u3m_boot_lite(1ULL << 34);

  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len,
                                                  u3_Ivory_pill)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  if ( u3_none == (ska = u3s_cue_xeno_with(sil_u, u3_Steel_pill_len,
                                                  u3_Steel_pill)) ) {
    printf("*** fail _setup 2\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);

  if ( c3n == u3v_boot_lite(u3k(pil)) ) {
    printf("*** fail _setup 3\n");
    exit(1);
  }

  u3d_boot(ska);

  eve = u3t(pil);
  if ( u3_none == _run("u3nc scan", _scan_nc, u3t(eve), u3h(eve)) ) {
    printf("*** fail _setup 4\n");
    exit(1);
  }
  u3z(pil);
}

/* _scan_nc(): analyze and compile [bus fol] with the SKA core.
*/
static u3_noun
_scan_nc(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  u3nc_scan(bus, fol);
  return u3_nul;
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

/* _nock_n(): .*(bus fol) with u3n interpreter
*/
static u3_noun
_nock_n(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3n_nock_on(bus, fol);
}

/* _nock_nc(): .*(bus fol) with u3nc interpreter
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
  else if ( jet_t && !u3nc_Stat.arm_d ) {
    fprintf(stderr, "test %s: no jetted call sites\r\n", nam_c);
    ret_i = 0;
  }
  else {
    fprintf(stderr, "test %s: ok\r\n", nam_c);
  }

  u3z(a); u3z(b);
  u3z(bus); u3z(fol);
  return ret_i;
}

/* _gate(): a gate from hoon source, built on the kernel core, with its
**          sample set.  TRANSFERS.
*/
static u3_noun
_gate(const c3_c* src_c, u3_noun sam)
{
  u3_noun gat = u3v_wish(src_c);
  u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(sam, u3k(u3t(u3t(gat)))));
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
                      u3nc(3, 8));

  return _test("ackermann", cor, u3nq(9, 2, 0, 1), 1);
}

static c3_i
_test_iter_dec(void)
{
  u3_noun list = u3_nul;
  for (c3_w i_w = 0; i_w < 10000; i_w++) list = u3nc(1, list);

  u3_noun cor = _gate("|=  l=(list @)\n"
                      "^-  (list @)\n"
                      "?~  l  ~\n"
                      "[(dec i.l) $(l t.l)]\n",
                      list);

  return _test("iterate dec", cor, u3nq(9, 2, 0, 1), 1);
}

static c3_i
_test_iter_vint(void)
{
  u3_noun list = u3_nul;
  for (c3_w i_w = 0; i_w < 10000; i_w++) list = u3nc(1, list);

  u3_noun cor = _gate("|=  l=(list @)\n"
                      "^-  (list @)\n"
                      "?~  l  ~\n"
                      "[+(i.l) $(l t.l)]\n",
                      list);

  return _test("iterate vint", cor, u3nq(9, 2, 0, 1), 1);
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  //  the build step runs the test; with U3NC_BUILD_ONLY it only builds
  //
  if ( getenv("U3NC_BUILD_ONLY") ) {
    return 0;
  }

  _setup();

  if ( !_test("inc", 42, u3nt(4, 0, 1), 0) ) {
    exit(1);
  }

  if ( !_test_ackermann() ) {
    exit(1);
  }

  if ( !_test_iter_dec() ) {
    exit(1);
  }

  if ( !_test_iter_vint() ) {
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test nock compile: ok\r\n");
  return 0;
}
