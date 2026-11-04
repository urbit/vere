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

/* _slog(): render a slog, as the king would.
*/
static void
_slog(u3_noun hod)
{
  u3_pier_tank(0, u3h(hod), u3k(u3t(hod)));
  u3z(hod);
}

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

  //  U3NC_LOOM: loom size in bits, for machines that can't map 16GB
  //
  {
    c3_c* lom_c = getenv("U3NC_LOOM");
    u3m_boot_lite(1ULL << (lom_c ? atoi(lom_c) : 34));
  }

  //  slogs rendered to stderr, which the build shows, without color codes;
  //  after the boot, which clears the handler
  //
  u3C.slog_f = _slog;
  u3_Host.ops_u.dem = c3y;
  u3_Host.ops_u.tem = c3y;

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

/* _test_ska(): the %ska hint runs its formula compiled, from u3n: the
**              raw trace of a crash inside it carries a deferred %mean
**              frame, which only compiled nock pushes.  (The trap sits
**              in a called gate: the analysis defers a %mean trap of a
**              callee, not one of the entry function itself.)  Not
**              inside the +mink jet, whose raw trace goes to Hoon's
**              +mook: there the frame is a plain %mean.  +mock renders
**              the same crash in C, like u3t_mook on +mink's trace;
**              +mule and +mole run their traps through hooks.
*/
static c3_i
_test_ska(void)
{
  const c3_c* src_c = "|=  a=@\n"
                      "=/  foo\n"
                      "  |=  b=@\n"
                      "  ~>  %mean.|.(leaf+(scow %ud b))\n"
                      "  (dec b)\n"
                      "~>  %ska\n"
                      "(foo a)\n";
  const c3_c* gat_c = "|=  a=@\n"
                      "=/  g\n"
                      "  |=  b=@\n"
                      "  ~>  %ska\n"
                      "  ~>  %mean.|.(leaf+(scow %ud b))\n"
                      "  (dec b)\n";
  const c3_c* kin_c = "(mink [g(+< a) 9 2 0 1] ~)\n";
  const c3_c* kok_c = "(mock [g(+< a) 9 2 0 1] ~)\n";
  const c3_c* vir_c = "|=  a=@\n"
                      "[(mule |.((dec a))) (mole |.((dec a)))]\n";
  u3_noun fol = u3nq(9, 2, 0, 1);
  c3_c    mik_c[256], mok_c[256];
  u3_noun cor, kin, kok;
  u3_weak tax;
  c3_i    ret_i;

  snprintf(mik_c, sizeof(mik_c), "%s%s", gat_c, kin_c);
  snprintf(mok_c, sizeof(mok_c), "%s%s", gat_c, kok_c);

  ret_i = _test("ska hint", _gate(src_c, 5), u3k(fol), 1);

  cor   = _gate(src_c, 0);
  tax   = _crash(u3n_nock_on, cor, fol);
  ret_i = (u3_none != tax) && _has_tag(tax, c3__meaf) && ret_i;
  fprintf(stderr, "test ska hint, deferred frame: %s\r\n",
          ( (u3_none != tax) && _has_tag(tax, c3__meaf) ) ? "ok" : "failed");
  if ( u3_none != tax ) u3z(tax);
  u3z(cor);

  ret_i = _test("ska mink", _gate(mik_c, 5), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mock", _gate(mok_c, 5), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mink, crash", _gate(mik_c, 0), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mock, crash", _gate(mok_c, 0), u3k(fol), 1) && ret_i;

  //  +mink's raw trace has a plain %mean frame and no deferred one, and
  //  renders to what +mock produced
  //
  cor = _gate(mik_c, 0);
  kin = _run("u3n  mink  ", _nock_n, cor, fol);
  u3z(cor);
  cor = _gate(mok_c, 0);
  kok = _run("u3n  mock  ", _nock_n, cor, fol);
  u3z(cor);

  if ( (u3_none == kin) || (u3_none == kok) ) {
    ret_i = 0;
  }
  else {
    u3_noun tan = u3t_mook(u3k(u3t(kin)));
    c3_i    ok_i = (2 == u3h(kin))
                && _has_tag(u3t(kin), c3__mean)
                && !_has_tag(u3t(kin), c3__meaf)
                && (c3y == u3r_sing(tan, u3t(kok)));

    fprintf(stderr, "test ska mink, flag: %s\r\n", ok_i ? "ok" : "failed");
    ret_i = ok_i && ret_i;
    u3z(tan);
  }
  if ( u3_none != kin ) u3z(kin);
  if ( u3_none != kok ) u3z(kok);

  ret_i = _test("ska mule mole", _gate(vir_c, 5), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mule mole, crash", _gate(vir_c, 0), u3k(fol), 1)
       && ret_i;

  u3z(fol);
  return ret_i;
}

/* _test_jets(): one call of every array-driven and punting arm, from a
**               gate on the kernel whose sample carries the arguments,
**               so the compiler can't fold the call: both interpreters
**               agree, and, built with -Dska-stat, the compiled call
**               hit a driver.
*/
static c3_i
_test_jets(void)
{
  static const struct {
    const c3_c* nam_c;
    const c3_c* src_c;
    const c3_c* sam_c;
  } cas_u[] = {
    { "add", "|=([a=@ b=@] (add a b))", "[3 4]" },
    { "find", "|=([a=(list @) b=(list @)] (find a b))", "[~[3 4] ~[1 2 3 4 5]]" },
    { "flop", "|=(a=(list @) (flop a))", "~[1 2 3]" },
    { "lent", "|=(a=(list @) (lent a))", "~[1 2 3]" },
    { "need", "|=(a=(unit @) (need a))", "[~ 42]" },
    { "scag", "|=([a=@ b=(list @)] (scag a b))", "[2 ~[1 2 3 4]]" },
    { "slag", "|=([a=@ b=(list @)] (slag a b))", "[2 ~[1 2 3 4]]" },
    { "zing", "|=(a=(list (list @)) (zing a))", "~[~[1 2] ~[3] ~[4 5]]" },
    { "welp", "|=([a=(list @) b=(list @)] (welp a b))", "[~[1 2] ~[3 4]]" },
    { "cap", "|=(a=@ (cap a))", "6" },
    { "peg", "|=([a=@ b=@] (peg a b))", "[6 5]" },
    { "mas", "|=(a=@ (mas a))", "6" },
    { "can", "|=([a=bloq b=(list [p=step q=@])] (can a b))", "[3 ~[[1 1] [2 515]]]" },
    { "dor", "|=([a=* b=*] (dor a b))", "[[1 2] [1 3]]" },
    { "gor", "|=([a=* b=*] (gor a b))", "[[1 2] 3]" },
    { "mor", "|=([a=* b=*] (mor a b))", "[[1 2] 3]" },
    { "pow", "|=([a=@ b=@] (pow a b))", "[3 5]" },
    { "rep", "|=([a=bite b=(list @)] (rep a b))", "[[3 2] ~[1 2 3]]" },
    { "rev", "|=([a=bloq b=@ c=@] (rev a b c))", "[3 4 999]" },
    { "swp", "|=([a=bloq b=@] (swp a b))", "[3 999]" },
    { "sqt", "|=(a=@ (sqt a))", "99" },
    { "xeb", "|=(a=@ (xeb a))", "99" },
    { "ind", "|=(a=@tas (ind:po a))", "%zod" },
    { "ins", "|=(a=@tas (ins:po a))", "%doz" },
    { "raw", "|=([a=@ b=@] (~(raw og a) b))", "[7 32]" },
    { "apt:by", "|=(m=(map @ @) ~(apt by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "del:by", "|=([m=(map @ @) k=@] (~(del by m) k))", "[(my ~[[1 2] [3 4] [5 6]]) 3]" },
    { "dif:by", "|=([m=(map @ @) n=(map @ @)] (~(dif by m) n))", "[(my ~[[1 2] [3 4] [5 6]]) (my ~[[3 4] [7 8]])]" },
    { "gas:by", "|=([m=(map @ @) l=(list [@ @])] (~(gas by m) l))", "[(my ~[[1 2] [3 4]]) ~[[5 6] [7 8]]]" },
    { "get:by", "|=([m=(map @ @) k=@] (~(get by m) k))", "[(my ~[[1 2] [3 4] [5 6]]) 3]" },
    { "has:by", "|=([m=(map @ @) k=@] (~(has by m) k))", "[(my ~[[1 2] [3 4] [5 6]]) 3]" },
    { "int:by", "|=([m=(map @ @) n=(map @ @)] (~(int by m) n))", "[(my ~[[1 2] [3 4] [5 6]]) (my ~[[3 4] [7 8]])]" },
    { "key:by", "|=(m=(map @ @) ~(key by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "put:by", "|=([m=(map @ @) k=@ v=@] (~(put by m) k v))", "[(my ~[[1 2] [3 4] [5 6]]) 7 8]" },
    { "tap:by", "|=(m=(map @ @) ~(tap by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "uni:by", "|=([m=(map @ @) n=(map @ @)] (~(uni by m) n))", "[(my ~[[1 2] [3 4] [5 6]]) (my ~[[3 4] [7 8]])]" },
    { "wyt:by", "|=(m=(map @ @) ~(wyt by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "apt:in", "|=(s=(set @) ~(apt in s))", "(sy ~[1 3 5 7])" },
    { "bif:in", "|=([s=(set @) k=@] (~(bif in s) k))", "[(sy ~[1 3 5 7]) 4]" },
    { "del:in", "|=([s=(set @) k=@] (~(del in s) k))", "[(sy ~[1 3 5 7]) 3]" },
    { "dif:in", "|=([s=(set @) t=(set @)] (~(dif in s) t))", "[(sy ~[1 3 5 7]) (sy ~[3 4 5])]" },
    { "gas:in", "|=([s=(set @) l=(list @)] (~(gas in s) l))", "[(sy ~[1 3]) ~[5 7 9]]" },
    { "has:in", "|=([s=(set @) k=@] (~(has in s) k))", "[(sy ~[1 3 5 7]) 5]" },
    { "int:in", "|=([s=(set @) t=(set @)] (~(int in s) t))", "[(sy ~[1 3 5 7]) (sy ~[3 4 5])]" },
    { "put:in", "|=([s=(set @) k=@] (~(put in s) k))", "[(sy ~[1 3 5 7]) 4]" },
    { "tap:in", "|=(s=(set @) ~(tap in s))", "(sy ~[1 3 5 7])" },
    { "uni:in", "|=([s=(set @) t=(set @)] (~(uni in s) t))", "[(sy ~[1 3 5 7]) (sy ~[3 4 5])]" },
    { "wyt:in", "|=(s=(set @) ~(wyt in s))", "(sy ~[1 3 5 7])" },
    { "cue", "|=(a=@ (cue a))", "(jam [1 2 3])" },
    { "mat", "|=(a=@ (mat a))", "1.000" },
    { "rub", "|=([a=@ b=@] (rub a b))", "[0 q:(mat 1.000)]" },
    { "leer", "|=(a=@t (to-wain:format a))", "(rap 3 'foo' 10 'bar' 10 'baz' ~)" },
    { "loss", "|=([a=(list @) b=(list @)] (loss:differ a b))", "[~[1 2 3 4 5] ~[2 4 6]]" },
    { "lune", "|=(a=@t (lune:unix:userlib a))", "(rap 3 'foo' 10 'bar' 10 ~)" },
    { "drg:cofl", "|=(a=[e=@s a=@u] (drg:+:fl a))", "[--3 5]" },
    { "lug:cofl", "|=([t=?(%fl %ce %sm %lg %ne %na %nt) a=[e=@s a=@u] s=?] (lug:+:fl t a s))", "[%ne [--0 (add (bex 120) 1)] &]" },
    { "add:rd", "|=([a=@rd b=@rd] (add:rd a b))", "[.~1.5 .~2.25]" },
    { "sub:rd", "|=([a=@rd b=@rd] (sub:rd a b))", "[.~1.5 .~2.25]" },
    { "mul:rd", "|=([a=@rd b=@rd] (mul:rd a b))", "[.~1.5 .~2.25]" },
    { "div:rd %u", "|=([a=@rd b=@rd] (~(div rd %u) a b))", "[.~1 .~3]" },
    { "sqt:rd", "|=(a=@rd (sqt:rd a))", ".~2" },
    { "fma:rd", "|=([a=@rd b=@rd c=@rd] (fma:rd a b c))", "[.~1.5 .~2 .~0.25]" },
    { "lth:rd", "|=([a=@rd b=@rd] (lth:rd a b))", "[.~1.5 .~2.25]" },
    { "lte:rd", "|=([a=@rd b=@rd] (lte:rd a b))", "[.~1.5 .~2.25]" },
    { "equ:rd", "|=([a=@rd b=@rd] (equ:rd a b))", "[.~1.5 .~2.25]" },
    { "gte:rd", "|=([a=@rd b=@rd] (gte:rd a b))", "[.~1.5 .~2.25]" },
    { "gth:rd", "|=([a=@rd b=@rd] (gth:rd a b))", "[.~1.5 .~2.25]" },
    { "add:rs", "|=([a=@rs b=@rs] (add:rs a b))", "[.1.5 .2.25]" },
    { "sub:rs", "|=([a=@rs b=@rs] (sub:rs a b))", "[.1.5 .2.25]" },
    { "mul:rs", "|=([a=@rs b=@rs] (mul:rs a b))", "[.1.5 .2.25]" },
    { "div:rs %u", "|=([a=@rs b=@rs] (~(div rs %u) a b))", "[.1 .3]" },
    { "sqt:rs", "|=(a=@rs (sqt:rs a))", ".2" },
    { "fma:rs", "|=([a=@rs b=@rs c=@rs] (fma:rs a b c))", "[.1.5 .2 .0.25]" },
    { "lth:rs", "|=([a=@rs b=@rs] (lth:rs a b))", "[.1.5 .2.25]" },
    { "lte:rs", "|=([a=@rs b=@rs] (lte:rs a b))", "[.1.5 .2.25]" },
    { "equ:rs", "|=([a=@rs b=@rs] (equ:rs a b))", "[.1.5 .2.25]" },
    { "gte:rs", "|=([a=@rs b=@rs] (gte:rs a b))", "[.1.5 .2.25]" },
    { "gth:rs", "|=([a=@rs b=@rs] (gth:rs a b))", "[.1.5 .2.25]" },
    { "add:rq", "|=([a=@rq b=@rq] (add:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "sub:rq", "|=([a=@rq b=@rq] (sub:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "mul:rq", "|=([a=@rq b=@rq] (mul:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "div:rq %u", "|=([a=@rq b=@rq] (~(div rq %u) a b))", "[.~~~1 .~~~3]" },
    { "sqt:rq", "|=(a=@rq (sqt:rq a))", ".~~~2" },
    { "fma:rq", "|=([a=@rq b=@rq c=@rq] (fma:rq a b c))", "[.~~~1.5 .~~~2 .~~~0.25]" },
    { "lth:rq", "|=([a=@rq b=@rq] (lth:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "lte:rq", "|=([a=@rq b=@rq] (lte:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "equ:rq", "|=([a=@rq b=@rq] (equ:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "gte:rq", "|=([a=@rq b=@rq] (gte:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "gth:rq", "|=([a=@rq b=@rq] (gth:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "add:rh", "|=([a=@rh b=@rh] (add:rh a b))", "[.~~1.5 .~~2.25]" },
    { "sub:rh", "|=([a=@rh b=@rh] (sub:rh a b))", "[.~~1.5 .~~2.25]" },
    { "mul:rh", "|=([a=@rh b=@rh] (mul:rh a b))", "[.~~1.5 .~~2.25]" },
    { "div:rh %u", "|=([a=@rh b=@rh] (~(div rh %u) a b))", "[.~~1 .~~3]" },
    { "sqt:rh", "|=(a=@rh (sqt:rh a))", ".~~2" },
    { "fma:rh", "|=([a=@rh b=@rh c=@rh] (fma:rh a b c))", "[.~~1.5 .~~2 .~~0.25]" },
    { "lth:rh", "|=([a=@rh b=@rh] (lth:rh a b))", "[.~~1.5 .~~2.25]" },
    { "lte:rh", "|=([a=@rh b=@rh] (lte:rh a b))", "[.~~1.5 .~~2.25]" },
    { "equ:rh", "|=([a=@rh b=@rh] (equ:rh a b))", "[.~~1.5 .~~2.25]" },
    { "gte:rh", "|=([a=@rh b=@rh] (gte:rh a b))", "[.~~1.5 .~~2.25]" },
    { "gth:rh", "|=([a=@rh b=@rh] (gth:rh a b))", "[.~~1.5 .~~2.25]" },
    { "just fun", "|=([daf=char tub=nail] ((just daf) tub))", "['a' [1 1] \"abc\"]" },
    { "mask fun", "|=([bud=(list char) tub=nail] ((mask bud) tub))", "[\"xyz\" [1 1] \"yes\"]" },
    { "shim fun", "|=([les=@ mos=@ tub=nail] ((shim les mos) tub))", "['a' 'z' [1 1] \"hello\"]" },
    { "easy fun", "|=([huf=* tub=nail] ((easy huf) tub))", "[%foo [1 1] \"bar\"]" },
    { "slaw", "|=(a=@ta (slaw %ud a))", "'123'" },
    { "scow", "|=(a=@ (scow %ud a))", "123" },
    { "cell", "|=([a=type b=type] (cell a b))", "[-:!>(1) -:!>(~)]" },
    { "comb", "|=([a=nock b=nock] (comb a b))", "[[0 2] [2 [0 3] [0 4]]]" },
    { "cons", "|=([a=nock b=nock] (cons a b))", "[[1 2] [1 3]]" },
    { "core", "|=([a=type b=coil] (core a b))", "=/(t -:!>(|=(a=@ a)) ?>(?=([%core *] t) +.t))" },
    { "face", "|=([a=term b=type] (face a b))", "[%foo -:!>(1)]" },
    { "fitz", "|=([a=@ta b=@ta] (fitz a b))", "[%ud %u]" },
    { "fork", "|=(a=(list type) (fork a))", "~[-:!>(1) -:!>(~) -:!>(1)]" },
    { "look", "|=([a=term b=(map term hoon)] (look:..ride a b))", "[%bar (~(gas by *(map term hoon)) ~[[%foo [%bust %null]] [%bar [%bust %noun]]])]" },
    { "loot", "|=([a=term b=(map term tome)] (loot a b))", "[%qux (~(gas by *(map term tome)) ~[[%foo (~(gas by *tome) [%bar [%bust %null]]~)] [%baz (~(gas by *tome) [%qux [%bust %noun]]~)]])]" },
    { "en:base16:mimes:html", "|=(a=octs (en:base16:mimes:html a))", "[4 0xdead.beef]" },
    { "de:base16:mimes:html", "|=(a=cord (de:base16:mimes:html a))", "'deadbeef'" },
    { "en:ecba:aes", "|=([k=@H b=@H] (~(en ecba:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "de:ecba:aes", "|=([k=@H b=@H] (~(de ecba:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "en:ecbb:aes", "|=([k=@I b=@H] (~(en ecbb:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "de:ecbb:aes", "|=([k=@I b=@H] (~(de ecbb:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "en:ecbc:aes", "|=([k=@I b=@H] (~(en ecbc:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "de:ecbc:aes", "|=([k=@I b=@H] (~(de ecbc:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "en:cbca:aes", "|=([k=@H p=@H t=@] (~(en cbca:aes:crypto k p) t))", "[0x1234.5678 0x9abc 'hello world']" },
    { "de:cbca:aes", "|=([k=@H p=@H t=@] (~(de cbca:aes:crypto k p) t))", "[0x1234.5678 0x9abc 0xdead.beef]" },
    { "en:cbcb:aes", "|=([k=@I p=@H t=@] (~(en cbcb:aes:crypto k p) t))", "[0x1234.5678 0x9abc 'hello world']" },
    { "de:cbcb:aes", "|=([k=@I p=@H t=@] (~(de cbcb:aes:crypto k p) t))", "[0x1234.5678 0x9abc 0xdead.beef]" },
    { "en:cbcc:aes", "|=([k=@I p=@H t=@] (~(en cbcc:aes:crypto k p) t))", "[0x1234.5678 0x9abc 'hello world']" },
    { "de:cbcc:aes", "|=([k=@I p=@H t=@] (~(de cbcc:aes:crypto k p) t))", "[0x1234.5678 0x9abc 0xdead.beef]" },
    { "puck:ed", "|=(s=@I (puck:ed:crypto s))", "(shax 'seed')" },
    { "sign:ed", "|=([m=@ s=@] (sign:ed:crypto m s))", "['hello' (shax 'seed')]" },
    { "sign-octs:ed", "|=([m=octs s=@] (sign-octs:ed:crypto m s))", "[[5 'hello'] (shax 'seed')]" },
    { "veri:ed", "|=([m=@ s=@] (veri:ed:crypto (sign:ed:crypto m s) m (puck:ed:crypto s)))", "['hello' (shax 'seed')]" },
    { "veri-octs:ed", "|=([m=octs s=@] (veri-octs:ed:crypto (sign-octs:ed:crypto m s) m (puck:ed:crypto s)))", "[[5 'hello'] (shax 'seed')]" },
    { "shar:ed", "|=([a=@ b=@] (shar:ed:crypto (puck:ed:crypto a) b))", "[(shax 'foo') (shax 'bar')]" },
    { "point-add:ed", "|=([a=@ b=@] (point-add:ed:crypto (puck:ed:crypto a) (puck:ed:crypto b)))", "[(shax 'foo') (shax 'bar')]" },
    { "scalarmult:ed", "|=([a=@ b=@] (scalarmult:ed:crypto a (puck:ed:crypto b)))", "[(shax 'foo') (shax 'bar')]" },
    { "scalarmult-base:ed", "|=(a=@ (scalarmult-base:ed:crypto a))", "(shax 'foo')" },
    { "add-scalarmult-scalarmult-base:ed", "|=([a=@ p=@ b=@] (add-scalarmult-scalarmult-base:ed:crypto a (puck:ed:crypto p) b))", "[(shax 'foo') (shax 'bar') (shax 'baz')]" },
    { "add-double-scalarmult:ed", "|=([a=@ p=@ b=@ q=@] (add-double-scalarmult:ed:crypto a (puck:ed:crypto p) b (puck:ed:crypto q)))", "[(shax 'foo') (shax 'bar') (shax 'baz') (shax 'qux')]" },
    { "hmac:hmac", "|=([k=@ m=@] (hmac:hmac:crypto [sha-256l:sha 64 32] [(met 3 k) k] [(met 3 m) m]))", "['key' 'hello world']" },
    { "pbk:scr", "|=([p=@ s=@ c=@ d=@] (pbk:scr:crypto p s c d))", "['password' 'salt' 2 32]" },
    { "pbl:scr", "|=([p=@ pl=@ s=@ sl=@ c=@ d=@] (pbl:scr:crypto p pl s sl c d))", "['password' 8 'salt' 4 2 32]" },
    { "verify:schnorr:secp256k1", "|=([pk=@I m=@I sig=@J] (verify:schnorr:secp256k1:secp:crypto pk m sig))", "[0xf930.8a01.9258.c310.4934.4f85.f89d.5229.b531.c845.836f.99b0.8601.f113.bce0.36f9 0 0xe907.831f.8084.8d10.69a5.371b.4024.1036.4bdf.1c5f.8307.b008.4c55.f1ce.2dca.8215.25f6.6a4a.85ea.8b71.e482.a74f.382d.2ce5.ebee.e8fd.b217.2f47.7df4.900d.3105.36c0]" },
    { "keccak-224:keccak", "|=(a=octs (keccak-224:keccak:crypto a))", "[5 'hello']" },
    { "keccak-256:keccak", "|=(a=octs (keccak-256:keccak:crypto a))", "[5 'hello']" },
    { "keccak-384:keccak", "|=(a=octs (keccak-384:keccak:crypto a))", "[5 'hello']" },
    { "keccak-512:keccak", "|=(a=octs (keccak-512:keccak:crypto a))", "[5 'hello']" },
    { "ripemd-160:ripemd", "|=(a=byts (ripemd-160:ripemd:crypto a))", "[5 'hello']" },
    { "sha-1l:sha", "|=(a=byts (sha-1l:sha a))", "[5 'hello']" },
    { "shax", "|=(a=@ (shax a))", "'hello world'" },
    { "shay", "|=([l=@ d=@] (shay l d))", "[11 'hello world']" },
    { "shas", "|=([s=@ r=@] (shas s r))", "['salt' 'hello world']" },
    { "shal", "|=([l=@ d=@] (shal l d))", "[11 'hello world']" },
    { "luck:ed", "|=(s=@I (luck:ed:crypto s))", "(shax 'seed')" },
    { "point-neg:ed", "|=(a=@ (point-neg:ed:crypto (puck:ed:crypto a)))", "(shax 'foo')" },
    { "recs:ed", "|=(a=@ (recs:ed:crypto a))", "(shax 'foo')" },
    { "scad:ed", "|=([s=@ c=@] =/(k (luck:ed:crypto s) (scad:ed:crypto pub.k sek.k c)))", "[(shax 'seed') (shax 'foo')]" },
    { "scas:ed", "|=([s=@ c=@] (scas:ed:crypto sek:(luck:ed:crypto s) c))", "[(shax 'seed') (shax 'foo')]" },
    { "scap:ed", "|=([s=@ c=@] (scap:ed:crypto (puck:ed:crypto s) c))", "[(shax 'seed') (shax 'foo')]" },
    { "sign-raw:ed", "|=([m=@ s=@] (sign-raw:ed:crypto m (luck:ed:crypto s)))", "['hello' (shax 'seed')]" },
    { "sign-octs-raw:ed", "|=([m=octs s=@] (sign-octs-raw:ed:crypto m (luck:ed:crypto s)))", "[[5 'hello'] (shax 'seed')]" },
    { "slar:ed", "|=([a=@ b=@] (slar:ed:crypto (puck:ed:crypto a) sek:(luck:ed:crypto b)))", "[(shax 'foo') (shax 'bar')]" },
    { "smac:ed", "|=([a=@ b=@ c=@] (smac:ed:crypto a b c))", "[(shax 'foo') (shax 'bar') (shax 'baz')]" },
    { "hash:blake3", "|=([out=@ud msg=octs] (blake3:blake:crypto out msg))", "[32 5 'hello']" },
    { "chunk-output:blake3", "|=([c=@ m=octs] (chunk-output:blake3:blake:crypto c m))", "[0 5 'hello']" },
    { "crc32", "|=(a=octs (crc32:crc:checksum a))", "[5 'hello']" },
    { "crypt:chacha", "|=(a=[rounds=@ud key=@uxI nonce=@uxG counter=@udG msg=octs] (crypt:chacha:crypto a))", "[20 0x1 0x2 0 [5 'hello']]" },
    { "xchacha:chacha", "|=(a=[rounds=@ud key=@uxI nonce=@ux] (xchacha:chacha:crypto a))", "[20 0x1 0x2]" },
    { "de:json", "|=(a=cord (de:json:html a))", "'{\"a\": [1, true, null, \"x\"]}'" },
    { "en:json", "|=(a=json (en:json:html a))", "[%a ~[[%n '1'] [%b &] ~ [%s 'x'] [%o (malt ['k' %s 'v']~)]]]" },
    { "mate", "|=([a=(unit @) b=(unit @)] (mate a b))", "[[~ 7] [~ 7]]" },
    { "clz", "|=([a=bite b=@] (clz a b))", "[[3 2] 5]" },
    { "ctz", "|=(a=@ (ctz a))", "48" },
    { "ham", "|=(a=@ (ham a))", "0xf.f00f" },
    { "hew", "|=([a=bite c=@ b=[@ @ @]] ((hew a c) b))", "[[3 0] 0x1234.5678 [1 2 1]]" },
    { "rig", "|=([a=bite b=bloq] (rig a b))", "[[3 2] 0]" },
    { "bif:by", "|=([a=(map @ @) b=@] (~(bif by a) b))", "[(my ~[[1 10] [2 20] [3 30]]) 2]" },
    { "fax:plot", "|=(p=plot (fax:plot p))", "[3 ~[0x11 0x22 0x33]]" },
    { "met:plot", "|=(p=plot (met:plot p))", "[3 ~[0x11 0x22 0x33]]" },
    { "nest:ut", "|=([a=type b=type] (~(nest ut a) & b))", "[-:!>(1) -:!>(2)]" },
    { "mint:ut", "|=([a=type b=hoon] (~(mint ut a) %noun b))", "[-:!>(.) (ream '(add 2 2)')]" },
  };
  u3_noun fol   = u3nq(9, 2, 0, 1);
  c3_i    ret_i = 1;
  c3_w    i_w;

  for ( i_w = 0; i_w < (sizeof(cas_u) / sizeof(cas_u[0])); i_w++ ) {
    u3_noun gat = u3m_soft(0, u3v_wish_w, u3i_string(cas_u[i_w].src_c));
    u3_noun sam = u3m_soft(0, u3v_wish_w, u3i_string(cas_u[i_w].sam_c));
    u3_noun cor;
    c3_c    nam_c[64];
#ifdef U3NC_STAT
    c3_d    jet_d = u3nc_Stat.jet_d;
#endif

    snprintf(nam_c, sizeof(nam_c), "jet %s", cas_u[i_w].nam_c);

    //  a source that doesn't compile fails its case, not the run
    //
    if ( (0 != u3h(gat)) || (0 != u3h(sam)) ) {
      if ( 0 != u3h(gat) ) {
        u3_pier_punt_goof(nam_c, gat);
        u3z(sam);
      }
      else {
        u3_pier_punt_goof(nam_c, sam);
        u3z(gat);
      }
      fprintf(stderr, "test %s: bad source\r\n", nam_c);
      ret_i = 0;
      continue;
    }

    cor = u3nc(u3k(u3h(u3t(gat))),
               u3nc(u3k(u3t(sam)), u3k(u3t(u3t(u3t(gat))))));
    u3z(gat); u3z(sam);

    ret_i = _test(nam_c, cor, u3k(fol), 1) && ret_i;

    //  a small arm with a known battery is inlined rather than called,
    //  so no driver runs for it: a note, not a failure
    //
#ifdef U3NC_STAT
    if ( jet_d == u3nc_Stat.jet_d ) {
      fprintf(stderr, "test %s: no jet hit\r\n", nam_c);
    }
#endif
  }

  u3z(fol);
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

    c3_i ret_i = _test("probe", cor, u3nq(9, 2, 0, 1), 1);

    //  U3NC_REGS: the argument counts of the programs compiled
    //
    if ( getenv("U3NC_REGS") ) {
      u3nc_regs();
    }

    return ret_i ? 0 : 1;
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

  if ( !_test_ska() ) {
    exit(1);
  }

  if ( !_test_jets() ) {
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test nock compile: ok\r\n");
  return 0;
}
