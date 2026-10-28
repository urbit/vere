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
static u3_weak _run(const c3_c* nam_c, u3_funk fun_f, u3_noun bus, u3_noun fol);

/* _setup(): boot the kernel from the ivory pill in the u3n interpreter,
**           which registers its cores, and boot the SKA core from the
**           steel pill, which gets the registrations.
*/
static void
_setup(void)
{
  u3_cue_xeno* sil_u;
  u3_weak      pil, ska;

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
  u3z(pil);

  //  check reference counts after every run: u3m_soft_top() marks and
  //  sweeps the inner road, recounting references, and a leak or a short
  //  count aborts.  Off for benchmarking: the sweep is timed with the run.
  //
  if ( !getenv("U3NC_BENCH") ) {
    u3C.wag_h |= u3o_debug_ram | u3o_leak_crash;
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

static c3_i
_test_iter_add(void)
{
  u3_noun list = u3_nul;
  for (c3_w i_w = 0; i_w < 10000; i_w++) list = u3nc(i_w, list);

  u3_noun cor = _gate("|=  l=(list @)\n"
                      "^-  (list @)\n"
                      "?~  l  ~\n"
                      "[(add i.l 1) $(l t.l)]\n",
                      list);

  return _test("iterate add", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_scot_tail(): a jet that punts (+scot on %p), in tail position: JSP,
**                    then the unary program of the callee.
*/
static c3_i
_test_scot_tail(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @t\n"
                      "(scot %p n)\n",
                      123456789);

  return _test("scot tail", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_scot(): a jet that hits (+scot on %ud) and one that punts (%p),
**               not in tail position: CAP.
*/
static c3_i
_test_scot(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  [@t @t]\n"
                      "[(scot %ud n) (scot %p n)]\n",
                      123456789);

  return _test("scot", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_memo_toss(): transient memoization (~+), keyed by the parts of
**                    the subject the callee uses: CAM.
*/
static c3_i
_test_memo_toss(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @\n"
                      "~+\n"
                      "?:  =(0 n)  0\n"
                      "+($(n (dec n)))\n",
                      1000);

  return _test("memo toss", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_memo_keep(): persistent memoization (%memo./path), keyed by the
**                    whole subject: CSM.  The cache is shared with u3n, so
**                    u3nc runs first on a fresh sample to miss it, and
**                    then u3n is checked against what u3nc saved.
*/
static c3_i
_test_memo_keep(void)
{
  const c3_c* src_c = "|=  n=@\n"
                      "^-  @\n"
                      "~>  %memo./ska\n"
                      "?:  =(0 n)  0\n"
                      "+($(n (dec n)))\n";
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_noun cor = _gate(src_c, 1000);
  u3_weak pro = _run("u3nc fresh ", _nock_nc, cor, fol);

  if ( u3_none == pro ) {
    u3z(cor); u3z(fol);
    return 0;
  }

  u3z(pro);
  u3z(fol);
  return _test("memo keep", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_dies(): a value live into one arm of a branch only: dropped at
**               the start of the other arm.  m is an indirect atom, so
**               the drop (and every consume of x) moves a real reference.
*/
static c3_i
_test_dies(void)
{
  u3_noun cor = _gate("|=  [n=@ m=@]\n"
                      "^-  @\n"
                      "=/  x  (add n m)\n"
                      "?:  =(0 (mod n 2))  x\n"
                      "$(n (dec n))\n",
                      u3nc(7, u3i_string("an atom long enough to be indirect")));

  return _test("dies", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_twice(): a register read twice by one op, consumed by one read:
**                [t t] in CON, (add b b) in ADD, and c twice among the
**                arguments of a tail call.
*/
static c3_i
_test_twice(void)
{
  u3_noun cor = _gate("|=  [a=@ b=@ c=@]\n"
                      "^-  *\n"
                      "=/  t=*  0\n"
                      "|-  ^-  *\n"
                      "?:  =(0 a)  [t (add b b) c]\n"
                      "$(a (dec a), b c, c c, t [t t])\n",
                      u3nt(10, 3, 5));

  return _test("twice", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_cycle(): the arguments of a self call in each other's parameter
**                slots, a rotation of three: the moves that put them in
**                place need a slot to step aside into, and the program
**                has none to spare, so it gets one more.  Indirect atoms,
**                so that every move carries a real reference.
*/
static c3_i
_test_cycle(void)
{
  u3_noun cor = _gate("|=  [a=@ b=@ c=@ n=@]\n"
                      "^-  [@ @ @]\n"
                      "?:  =(0 n)  [a b c]\n"
                      "$(a b, b c, c a, n (dec n))\n",
                      u3nq(u3i_string("the first of three indirect atoms"),
                           u3i_string("the second of three indirect atoms"),
                           u3i_string("the third of three indirect atoms"),
                           7));

  return _test("cycle", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_swap(): two arguments of a self call in each other's slots, and
**               a temporary above the parameters that is dead by the
**               call: the one that steps aside takes its slot.  The other
**               arm of the branch passes every argument in place.
*/
static c3_i
_test_swap(void)
{
  u3_noun cor = _gate("|=  [a=@ b=@ n=@]\n"
                      "^-  [@ @]\n"
                      "?:  =(0 n)  [a b]\n"
                      "=/  m  (dec n)\n"
                      "?:  =(0 (mod m 2))  $(a a, b b, n m)\n"
                      "$(a b, b a, n m)\n",
                      u3nt(u3i_string("the first of two indirect atoms"),
                           u3i_string("the second of two indirect atoms"),
                           7));

  return _test("swap", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_partial(): a register that dies on one edge into a block that
**                   another edge reaches with it consumed already: y is
**                   indirect and read only by the second test of the ?&,
**                   so where the first test fails it dies on the edge to
**                   the no arm, and where the second test runs it is
**                   consumed before the same arm.  Both happen in the loop.
*/
static c3_i
_test_partial(void)
{
  u3_noun cor = _gate("|=  [b=@ n=@]\n"
                      "^-  @\n"
                      "=/  acc  0\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "=/  y  (add b n)\n"
                      "=.  acc\n"
                      "  ?:  ?&(=(0 (mod n 2)) =(0 (mod y 3)))\n"
                      "    (add acc 1)\n"
                      "  (add acc 2)\n"
                      "$(n (dec n))\n",
                      u3nc(u3i_string("an atom long enough to be indirect, yes"), 9));

  return _test("partial", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_equal(): branches on the equality of two registers, indirect
**                atoms in a loop.  The first, a test of a ?| whose other
**                test the yes block shares, has one source read past it
**                only by that other test, on the no side: the EQQ
**                consumes it on the fall-through.  The last, in tail
**                position, has each source read past it in one arm only:
**                the EQQ consumes one on each edge.
*/
static c3_i
_test_equal(void)
{
  u3_noun cor = _gate("|=  [a=@ n=@]\n"
                      "^-  @\n"
                      "=/  acc  0\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "=/  v  (add n a)\n"
                      "=/  w  (add a 2)\n"
                      "=.  acc\n"
                      "  ?:  ?|(=(v w) =(0 (mod v 2)))\n"
                      "    (add acc 1)\n"
                      "  (add acc w)\n"
                      "=/  y  (add a n)\n"
                      "=/  z  (add a 3)\n"
                      "?:  =(y z)\n"
                      "  $(n (dec n), acc (add acc y))\n"
                      "$(n (dec n), acc (add acc z))\n",
                      u3nc(u3i_string("an atom long enough to be indirect, yes"), 5));

  return _test("equal", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_fork(): a ?: in value position whose arms are autoconses of
**               indirect atoms, taken apart past it.  The IR materializes
**               the loobean and tests it again in every block that reads
**               the product, each arm fulfilling the reads its own way;
**               a register defined in one arm and read past the join is
**               left undefined (kil) on the other path, and its drop
**               there must find nothing to release.
*/
static c3_i
_test_fork(void)
{
  u3_noun cor = _gate("|=  [a=@ n=@]\n"
                      "^-  @\n"
                      "=/  acc  0\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "=/  y  (add a n)\n"
                      "=/  p  ?:(=(y (add a 3)) [(add y 1) 1] [2 (add y 2)])\n"
                      "?:  =(0 (mod n 2))\n"
                      "  $(n (dec n), acc (add acc -.p))\n"
                      "$(n (dec n), acc (add acc +.p))\n",
                      u3nc(u3i_string("an atom long enough to be indirect, yes"), 5));

  return _test("fork", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_unused(): the product of a call that is never used (but may
**                 crash, so the call stays): dropped right after it.
*/
static c3_i
_test_unused(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @\n"
                      "=/  x  (dec n)\n"
                      "=/  y  (scot %ud n)\n"
                      "+(n)\n",
                      5);

  return _test("unused", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_bignum(): an indirect atom accumulated in a loop, consumed at
**                 each step by the jet call that replaces it.
*/
static c3_i
_test_bignum(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @\n"
                      "=/  acc=@  1\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "$(n (dec n), acc (mul acc 3))\n",
                      200);

  return _test("bignum", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_rebuild(): a core edited and rebuilt each iteration, with its
**                  parts deconstructed and consed back.
*/
static c3_i
_test_rebuild(void)
{
  u3_noun cor = _gate("|=  [n=@ acc=[p=@ q=@ r=(list @)]]\n"
                      "^-  [p=@ q=@ r=(list @)]\n"
                      "?:  =(0 n)  acc\n"
                      "$(n (dec n), acc acc(q +(q.acc), r [p.acc r.acc]))\n",
                      u3nc(50, u3nt(1, 2, u3_nul)));

  return _test("rebuild", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_owned(): a list built and transformed within one computation, so
**                 that its cells are uniquely owned when deconstructed:
**                 a tail-recursive map that conses as it takes apart, and
**                 a non-tail one that conses after the recursive call.
*/
static c3_i
_test_owned(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  [(list @) (list @)]\n"
                      "=/  l=(list @)  ~\n"
                      "=.  l  |-  ^-  (list @)\n"
                      "       ?:  =(0 n)  l\n"
                      "       $(n (dec n), l [n l])\n"
                      "=/  m=(list @)  ~\n"
                      "=.  m  |-  ^-  (list @)\n"
                      "       ?:  =(0 n)  m\n"
                      "       $(n (dec n), m [n m])\n"
                      ":-  =/  acc=(list @)  ~\n"
                      "    |-  ^-  (list @)\n"
                      "    ?~  l  acc\n"
                      "    $(l t.l, acc [+(i.l) acc])\n"
                      "|-  ^-  (list @)\n"
                      "?~  m  ~\n"
                      "[+(i.m) $(m t.m)]\n",
                      10000);

  return _test("owned", cor, u3nq(9, 2, 0, 1), 1);
}

/* _fast_n(): the gate cor kicked through u3n's jet dashboard: ~ if
**            u3j_kick() finds no jet, else its product and that of the
**            %fun hook on the gate.  For u3m_soft().
*/
static u3_noun
_fast_n(u3_noun cor_fol)
{
  u3_noun cor, fol, kic;

  u3x_cell(cor_fol, &cor, &fol);
  u3k(cor); u3z(cor_fol);

  if ( u3_none == (kic = u3j_kick(u3k(cor), 2)) ) {
    u3z(cor);
    return u3_nul;
  }

  return u3nc(kic, u3j_hook(cor, "fun"));
}

/* _test_fast(): a %fast hint run only by u3nc: a gate labeled as the
**               kernel's dec, built over its layer, with a hook on its
**               arm.  The analysis registers it, and u3n jets and hooks
**               it through the registration %ir carries back.
*/
static c3_i
_test_fast(void)
{
  u3_noun cor = _gate("|=  *\n"
                      "=>  ..add\n"
                      "~%  %dec  +>\n"
                      "  ==\n"
                      "    %fun  $\n"
                      "  ==\n"
                      "|=  a=@\n"
                      "(sub a 1)\n",
                      0);
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_weak gat = _run("u3nc fast  ", _nock_nc, cor, fol);
  u3_noun dec, pro;
  c3_i    ret_i = 1;

  u3z(cor); u3z(fol);

  if ( u3_none == gat ) {
    fprintf(stderr, "test fast: crashed\r\n");
    return 0;
  }

  //  with a sample, as u3n meets it
  //
  dec = u3nt(u3k(u3h(gat)), 42, u3k(u3t(u3t(gat))));
  u3z(gat);
  pro = _run("u3n  fast  ", _fast_n, dec, u3_nul);

  if ( u3_none == pro ) {
    fprintf(stderr, "test fast: crashed\r\n");
    ret_i = 0;
  }
  else if ( u3_nul == pro ) {
    fprintf(stderr, "test fast: not jetted by u3n\r\n");
    ret_i = 0;
  }
  else {
    u3_noun exp = u3nc(41, 41);

    if ( c3n == u3r_sing(pro, exp) ) {
      fprintf(stderr, "test fast: mismatch\r\n");
      u3m_p("u3n ", pro);
      ret_i = 0;
    }
    else {
      fprintf(stderr, "test fast: ok\r\n");
    }
    u3z(exp);
    u3z(pro);
  }

  u3z(dec);
  return ret_i;
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

  if ( !_test_iter_add() ) {
    exit(1);
  }

  if ( !_test_scot_tail() ) {
    exit(1);
  }

  if ( !_test_scot() ) {
    exit(1);
  }

  if ( !_test_memo_toss() ) {
    exit(1);
  }

  if ( !_test_memo_keep() ) {
    exit(1);
  }

  if ( !_test_dies() ) {
    exit(1);
  }

  if ( !_test_twice() ) {
    exit(1);
  }

  if ( !_test_cycle() ) {
    exit(1);
  }

  if ( !_test_swap() ) {
    exit(1);
  }

  if ( !_test_partial() ) {
    exit(1);
  }

  if ( !_test_equal() ) {
    exit(1);
  }

  if ( !_test_fork() ) {
    exit(1);
  }

  if ( !_test_unused() ) {
    exit(1);
  }

  if ( !_test_bignum() ) {
    exit(1);
  }

  if ( !_test_rebuild() ) {
    exit(1);
  }

  if ( !_test_owned() ) {
    exit(1);
  }

  if ( !_test_fast() ) {
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test nock compile: ok\r\n");
  return 0;
}
