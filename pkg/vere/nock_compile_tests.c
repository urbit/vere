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

/* _scot_cb(): u3h_walk callback over the rings: set the flags in ptr_v
**             on every arm of a core named scot.
*/
static void
_scot_cb(u3_noun kev, void* ptr_v)
{
  const c3_o*     fla_o = ptr_v;
  u3j_harm*       ham_u;
  const u3u_harm* arm_u;

  if (  (c3y == u3r_sing_c("scot", u3h(u3h(u3h(kev)))))
     && (c3y == u3j_ring(u3h(kev), &ham_u, &arm_u)) )
  {
    ham_u->liv = fla_o[0];
    ham_u->ice = fla_o[1];
  }
}

/* _off_cb(): u3h_walk callback over the rings: turn off (liv) every arm
**            under a core whose name is in ptr_v, a comma-separated string
**            with a leading and a trailing comma.
*/
static c3_w _off_w;

static void
_off_cb(u3_noun kev, void* ptr_v)
{
  const c3_c*     off_c = ptr_v;
  u3_noun         pax   = u3h(u3h(kev));
  u3j_harm*       ham_u;
  const u3u_harm* arm_u;

  for ( ; u3_nul != pax; pax = u3t(pax) ) {
    c3_c* nam_c = u3r_string(u3h(pax));
    c3_c  key_c[64];
    c3_t  hit_t;

    snprintf(key_c, sizeof(key_c), ",%s,", nam_c);
    hit_t = !!strstr(off_c, key_c);
    c3_free(nam_c);

    if ( hit_t && (c3y == u3j_ring(u3h(kev), &ham_u, &arm_u)) ) {
      ham_u->liv = c3n;
      _off_w++;
      return;
    }
  }
}

/* _test_flags(): the flags of a jet arm hold for the compiled calls
**                through its ring: +scot off, its calls run its program;
**                +scot not perfect, its driver is tested against nock,
**                and its punt (on %p) leaves the subject to the caller.
*/
static c3_i
_test_flags(void)
{
  c3_o off_o[2] = { c3n, c3y };
  c3_o tes_o[2] = { c3y, c3n };
  c3_o def_o[2] = { c3y, c3y };
  c3_i ret_i = 1;

  u3h_walk_with(u3H->rod_u.ska.pax_p, _scot_cb, off_o);
  ret_i = _test_scot() && ret_i;

  u3h_walk_with(u3H->rod_u.ska.pax_p, _scot_cb, tes_o);
  ret_i = _test_scot() && ret_i;

  u3h_walk_with(u3H->rod_u.ska.pax_p, _scot_cb, def_o);

  fprintf(stderr, "test flags: %s\r\n", ret_i ? "ok" : "failed");
  return ret_i;
}

/* _crash(): the raw trace of [bus fol] crashing under fun_f, or u3_none
**           if it doesn't crash.  RETAINS.
*/
static u3_weak
_crash(u3_funq fun_f, u3_noun bus, u3_noun fol)
{
  u3_noun gon = u3m_soft_run(u3_nul, fun_f, u3k(bus), u3k(fol));
  u3_weak tax = u3_none;

  if ( 2 == u3h(gon) ) {
    tax = u3k(u3t(gon));
  }

  u3z(gon);
  return tax;
}

/* _has_tag(): does a frame of tax carry tag?  RETAINS.
*/
static c3_t
_has_tag(u3_noun tax, u3_noun tag)
{
  for ( ; u3_nul != tax; tax = u3t(tax) ) {
    if ( tag == u3h(u3h(tax)) ) {
      return 1;
    }
  }

  return 0;
}

/* _mean_both(): crash [bus fol] under u3n and u3nc and render both traces,
**               and the u3n one through +mook too; whether the u3nc trace
**               has a frame tagged tag, if tag is set.  For u3m_soft().
**               Produces [[ren rec mok has] tac], tac the raw u3nc trace,
**               or ~ if a run didn't crash.
*/
static u3_noun
_mean_both(u3_noun arg)
{
  u3_noun tag, bus, fol, ren, rec, mok, pro;
  u3_weak tan, tac;

  u3x_trel(arg, &tag, &bus, &fol);

  tan = _crash(u3n_nock_on, bus, fol);
  tac = _crash(u3nc_nock_on, bus, fol);

  if ( (u3_none == tan) || (u3_none == tac) ) {
    if ( u3_none != tan ) u3z(tan);
    if ( u3_none != tac ) u3z(tac);
    u3z(arg);
    return u3_nul;
  }

  ren = u3t_mook(u3k(tan));
  rec = u3t_mook(u3k(tac));
  mok = u3dc("mook", 2, u3k(tan));
  pro = u3nc(u3nq(ren, rec, u3k(u3t(mok)), __(!tag || _has_tag(tac, tag))),
             tac);

  u3z(mok);
  u3z(tan);
  u3z(arg);
  return pro;
}

/* _test_crash(): [bus fol] crashes: the rendered traces of u3n and u3nc
**                agree, and the renderer agrees with +mook on the u3n
**                trace; tag: the frame the u3nc trace must have, or 0.
**                TRANSFERS.
*/
static c3_i
_test_crash(const c3_c* nam_c, u3_noun bus, u3_noun fol, u3_noun tag)
{
  u3_noun gon = u3m_soft(0, _mean_both, u3nt(tag, bus, fol));
  u3_noun ren, rec, mok, has;
  c3_i    ret_i = 1;

  if ( 0 != u3h(gon) ) {
    u3_pier_punt_goof(nam_c, gon);
    fprintf(stderr, "test %s: failed\r\n", nam_c);
    return 0;
  }

  if ( u3_nul == u3t(gon) ) {
    fprintf(stderr, "test %s: no crash\r\n", nam_c);
    u3z(gon);
    return 0;
  }

  u3x_qual(u3h(u3t(gon)), &ren, &rec, &mok, &has);

  if ( c3n == u3r_sing(ren, rec) ) {
    fprintf(stderr, "test %s: u3n and u3nc traces differ\r\n", nam_c);
    u3_pier_punt(0, u3k(ren));
    u3_pier_punt(0, u3k(rec));
    ret_i = 0;
  }
  else if ( c3n == u3r_sing(ren, mok) ) {
    fprintf(stderr, "test %s: renderer and +mook differ\r\n", nam_c);
    u3_pier_punt(0, u3k(ren));
    u3_pier_punt(0, u3k(mok));
    ret_i = 0;
  }
  else if ( c3n == has ) {
    u3_noun tac = u3t(u3t(gon));

    fprintf(stderr, "test %s: u3nc pushed no %%%s frame\r\n", nam_c,
            ( c3__meaf == tag ) ? "meaf" : "meac");
    for ( ; u3_nul != tac; tac = u3t(tac) ) {
      u3m_p("frame tag", u3h(u3h(tac)));
    }
    ret_i = 0;
  }
  else {
    fprintf(stderr, "test %s: ok\r\n", nam_c);
    u3_pier_punt(2, u3k(ren));
  }

  u3z(gon);
  return ret_i;
}

/* _test_mean(): %mean traps: a trap with a known battery is pushed as
**               its arm and the parts of the trap the arm takes (%meaf),
**               the known product of a pure arm as a constant (%meac),
**               an unknown trap as itself.  Each gate on a sample that
**               runs through, and on 0, where +dec crashes; the arm of
**               one crashes too (####); one is deep enough to skip
**               frames; one is %spot-hinted.  The ~| case, a deferred
**               +sell, runs only with U3NC_SLOW set: the analysis of the
**               printer takes 25 s.
*/
static c3_i
_test_mean(void)
{
  static const struct {
    const c3_c* nam_c;
    const c3_c* src_c;
    c3_w        sam_w;
    u3_noun     tag;
  } cas_u[] = {
    { "deferred",
      "|=  a=@\n~>  %mean.|.(leaf+(scow %ud a))\n(dec a)\n",
      5, c3__meaf },
    { "constant cons",
      "|=  a=@\n~_  leaf+\"foo\"\n(dec a)\n",
      5, c3__meac },
    { "constant atom",
      "|=  a=@\n~_  %foo\n(dec a)\n",
      5, c3__meac },
    { "unknown",
      "|=  a=@\n~>  %mean.?:(=(a 5) |.(leaf+\"a\") |.(leaf+\"b\"))\n(dec a)\n",
      5, 0 },
    { "arm crash",
      "|=  a=@\n~>  %mean.|.(leaf+(scow %ud (dec a)))\n(dec a)\n",
      5, c3__meaf },
    { "spotted",
      "!:\n|=  a=@\n~>  %mean.|.(leaf+(scow %ud a))\n(dec a)\n",
      5, c3__meaf },
    { "deep",
      "|=  a=@\n|-  ^-  @\n~>  %mean.|.(leaf+(scow %ud a))\n"
      "?:  =(a 0)  !!\n$(a (dec a))\n",
      1100, c3__meaf },
    { "vase",
      "|=  a=@\n~|  [%foo a]\n(dec a)\n",
      5, c3__meaf },
  };
  c3_i ret_i = 1;
  c3_w i_w;

  for ( i_w = 0; i_w < (sizeof(cas_u) / sizeof(cas_u[0])); i_w++ ) {
    u3_noun fol;
    c3_c    nam_c[64];

    if ( !strcmp(cas_u[i_w].nam_c, "vase") && !getenv("U3NC_SLOW") ) {
      continue;
    }

    fol = u3nq(9, 2, 0, 1);

    //  the deep one runs through on no sample: it always crashes
    //
    if ( 1100 != cas_u[i_w].sam_w ) {
      snprintf(nam_c, sizeof(nam_c), "mean %s", cas_u[i_w].nam_c);
      ret_i = _test(nam_c, _gate(cas_u[i_w].src_c, cas_u[i_w].sam_w),
                    u3k(fol), 1)
           && ret_i;
    }

    snprintf(nam_c, sizeof(nam_c), "mean %s, crash", cas_u[i_w].nam_c);
    ret_i = _test_crash(nam_c, _gate(cas_u[i_w].src_c,
                                     ( 1100 == cas_u[i_w].sam_w ) ? 1100 : 0),
                        fol, cas_u[i_w].tag)
         && ret_i;
  }

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

  //  U3NC_PROBE: the source of a gate on the kernel, run on the sample
  //  U3NC_PROBE_SAM (default 5) by both interpreters, and nothing else
  //
  //  U3NC_PROBE_OFF: cores whose jets to turn off first, comma-separated
  //
  if ( getenv("U3NC_PROBE") ) {
    c3_c*   sam_c = getenv("U3NC_PROBE_SAM");
    c3_c*   off_c = getenv("U3NC_PROBE_OFF");
    u3_noun cor   = _gate(getenv("U3NC_PROBE"), sam_c ? atoi(sam_c) : 5);

    if ( off_c ) {
      c3_c* key_c = c3_malloc(strlen(off_c) + 3);
      snprintf(key_c, strlen(off_c) + 3, ",%s,", off_c);
      u3h_walk_with(u3H->rod_u.ska.pax_p, _off_cb, key_c);
      c3_free(key_c);
      fprintf(stderr, "probe: %u arms off\r\n", _off_w);
    }

    return _test("probe", cor, u3nq(9, 2, 0, 1), 1) ? 0 : 1;
  }

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

  if ( !_test_flags() ) {
    exit(1);
  }

  if ( !_test_mean() ) {
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test nock compile: ok\r\n");
  return 0;
}
