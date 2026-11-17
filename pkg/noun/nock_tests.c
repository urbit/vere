/// @file

#include "noun.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
}

static u3_noun
_nock_fol(u3_noun fol)
{
  return u3n_nock_on(u3_nul, fol);
}

static c3_i
_test_nock_meme(void)
{
  //  (jam !=(=(~ =|(i=@ |-(?:(=(i ^~((bex 32))) ~ [i $(i +(i))]))))))
  //
  const c3_y buf_y[] = {
    0xe1, 0x16, 0x1b,  0x4, 0x1b, 0xe1, 0x20, 0x58, 0x1c, 0x76, 0x4d, 0x96, 0xd8,
    0x31, 0x60,  0x0,  0x0,  0x0,  0x0, 0xd8,  0x8, 0x37, 0xce,  0xd, 0x92, 0x21,
    0x83, 0x68, 0x61, 0x87, 0x39, 0xce, 0x4d,  0xe, 0x92, 0x21, 0x87, 0x19,  0x8
  };
  u3_noun fol = u3s_cue_bytes(sizeof(buf_y), buf_y);
  u3_noun gon;
  c3_h    i_w;
  c3_i  ret_i = 1;

  for ( i_w = 0; i_w < 3; i_w++ ) {
    gon = u3m_soft(0, _nock_fol, u3k(fol));

    if ( c3n == u3r_p(gon, c3__meme, 0) ) {
      u3m_p("nock meme unexpected mote", u3h(gon));
      ret_i = 0;
      u3z(gon);
      break;
    }

    u3z(gon);
  }

  u3z(fol);

  return ret_i;
}

static c3_i
_test_meme(void)
{
  c3_i ret_i = 1;

  if ( !_test_nock_meme() ) {
    fprintf(stderr, "test nock meme: failed\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _soft_cax_bail(): bail from inside a virtualization-with-cache frame.
*/
static u3_noun
_soft_cax_bail(u3_noun aga, u3_noun agb)
{
  u3z(aga);
  u3z(agb);
  return u3m_bail(c3__exit);
}

/* _test_soft_cax(): u3m_soft_cax catches a bail rather than asserting.
**
**   Regression test for a bitness bug: u3m_soft_cax was the only one of
**   the four _setjmp(u3R->esc.buf) trap sites without the VERE64 split.
**   Under VERE64, u3m_bail stores the ball in u3R->esc.why_w and longjmps
**   with a literal 1, since a 64-bit noun does not fit in longjmp's int
**   return.  Reading the setjmp value as the bail therefore produced 1,
**   and the catch arm's u3_assert(_(u3du(why))) failed on an atom.
**
**   Reachable in production through jets/e/mice.c, so any bail under
**   mock-with-cache aborted a 64-bit runtime.
*/
static c3_i
_test_soft_cax(void)
{
  //  a true exit produces [[2 tax] ~]
  //
  u3_noun pro = u3m_soft_cax(_soft_cax_bail, 0, 0);
  c3_i  ret_i = 1;

  if (  (c3n == u3du(pro))
     || (c3n == u3du(u3h(pro)))
     || (2 != u3h(u3h(pro))) )
  {
    u3m_p("test soft_cax: unexpected product", pro);
    ret_i = 0;
  }

  u3z(pro);
  return ret_i;
}

/* main(): run all test cases.
*/
/* _defer_pair(): u3m_soft() test fn, produces [arg arg].
*/
static u3_noun
_defer_pair(u3_noun arg)
{
  return u3nc(u3k(arg), u3k(arg));
}

/* _defer_fail(): u3m_soft() test fn, fails.
*/
static u3_noun
_defer_fail(u3_noun arg)
{
  return u3m_bail(c3__exit);
}

/* _defer_hom: the home noun that _defer_open_fail() gains.
*/
static u3_noun _defer_hom;

/* _defer_open_fail(): u3m_soft_with() hook: opens the window, gains
**                     [_defer_hom] as a take does, then fails as a take
**                     out of memory on the home road does.
*/
static void
_defer_open_fail(void)
{
  u3a_defer_open();
  u3k(_defer_hom);
  u3m_signal(c3__meme);
}

/* _test_defer(): deferred gains across a successful exit into home.
**
**   each armed step must leave [hom]'s count where an unarmed control
**   leaves it, without writing it while the window is open.
*/
static c3_i
_test_defer(void)
{
  c3_i    ret_i = 1;
  u3_noun hom   = u3nc(u3i_string("deferred"), u3i_string("gains"));
  u3_noun arg, gon;
  c3_w    bas_w, tak_w, rel_w;

  //  control: take product [hom hom], release arg; then release product
  //
  arg   = u3k(hom);
  bas_w = u3a_use(hom);
  gon   = u3m_soft(0, _defer_pair, arg);
  tak_w = u3a_use(hom);
  u3z(gon);
  rel_w = u3a_use(hom);

  if ( tak_w <= bas_w ) {
    fprintf(stderr, "defer: control gained nothing\r\n");
    ret_i = 0;
  }

  //  armed, product kept: nothing written until close, then the same
  //
  arg = u3k(hom);
  gon = u3m_soft_with(0, _defer_pair, arg, u3a_defer_open);

  if ( bas_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: written while open: have %"PRIc3_w", need %"PRIc3_w"\r\n",
                    u3a_use(hom), bas_w);
    ret_i = 0;
  }

  u3a_defer_close();

  if ( tak_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: close: have %"PRIc3_w", need %"PRIc3_w"\r\n",
                    u3a_use(hom), tak_w);
    ret_i = 0;
  }

  u3z(gon);

  //  armed, product released before close: the gains cancel
  //
  arg = u3k(hom);
  gon = u3m_soft_with(0, _defer_pair, arg, u3a_defer_open);
  u3z(gon);
  u3a_defer_close();

  if ( rel_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: cancel: have %"PRIc3_w", need %"PRIc3_w"\r\n",
                    u3a_use(hom), rel_w);
    ret_i = 0;
  }

  //  past the cap, gains on new nouns are written as usual, and the
  //  result is the same: [hom] fills the table, so the product's other
  //  noun (a fresh indirect atom, not in it) spills. NB: it must be
  //  longer than a direct atom on either word size, or it has no count
  //
  {
    c3_w    max_w = u3a_Defer_max_w;
    u3_noun gun;

    arg = u3k(hom);
    u3a_Defer_max_w = 1;
    gon = u3m_soft_with(0, _defer_pair, arg, u3a_defer_open);

    if ( bas_w != u3a_use(hom) ) {
      fprintf(stderr, "defer: cap: hom written while open\r\n");
      ret_i = 0;
    }

    gun = u3m_soft(0, _defer_pair, u3i_string("spilled past the cap"));

    if ( 2 != u3a_use(u3h(u3t(gun))) ) {
      fprintf(stderr, "defer: cap: spill not written: %"PRIc3_w"\r\n",
                      u3a_use(u3h(u3t(gun))));
      ret_i = 0;
    }

    u3a_defer_close();
    u3a_Defer_max_w = max_w;

    if ( tak_w != u3a_use(hom) ) {
      fprintf(stderr, "defer: cap: have %"PRIc3_w", need %"PRIc3_w"\r\n",
                      u3a_use(hom), tak_w);
      ret_i = 0;
    }

    u3z(gun);
    u3z(gon);
  }

  //  a cancelled lose doesn't free: with our own reference released
  //  while the product's gains are pending, [hom] lives on in it
  //
  arg = u3k(hom);
  gon = u3m_soft_with(0, _defer_pair, arg, u3a_defer_open);
  u3z(hom);
  u3a_defer_close();

  if ( c3n == u3r_sing_c("deferred", u3h(u3h(u3t(gon)))) ) {
    fprintf(stderr, "defer: product damaged\r\n");
    ret_i = 0;
  }

  hom = u3k(u3h(u3t(gon)));
  u3z(gon);

  //  a failed event never opens the window: the hook runs only on
  //  success, and the trace is rendered with the window shut
  //
  bas_w = u3a_use(hom);
  gon = u3m_soft_with(0, _defer_fail, u3k(hom), u3a_defer_open);
  u3z(gon);

  if ( bas_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: failure: have %"PRIc3_w", need %"PRIc3_w"\r\n",
                    u3a_use(hom), bas_w);
    ret_i = 0;
  }

  arg = u3k(hom);

  if ( (bas_w + 1) != u3a_use(hom) ) {
    fprintf(stderr, "defer: window open after failure\r\n");
    ret_i = 0;
  }

  u3z(arg);

  //  a failure past the hook, in the takes, finds the window open, with
  //  a gain pending: the signal's reset writes the gain and shuts the
  //  window, and the next event opens it again
  //
  _defer_hom = hom;
  gon = u3m_soft_with(0, _defer_pair, u3_nul, _defer_open_fail);

  if ( 0 == u3h(gon) ) {
    fprintf(stderr, "defer: no failure past the hook\r\n");
    ret_i = 0;
  }

  u3z(gon);

  if ( (bas_w + 1) != u3a_use(hom) ) {
    fprintf(stderr, "defer: pending gain not written after a failure past "
                    "the hook: have %"PRIc3_w", need %"PRIc3_w"\r\n",
                    u3a_use(hom), bas_w + 1);
    ret_i = 0;
  }

  arg = u3k(hom);

  if ( (bas_w + 2) != u3a_use(hom) ) {
    fprintf(stderr, "defer: window open after a failure past the hook\r\n");
    ret_i = 0;
  }

  u3z(arg);
  u3z(hom);   //  the hook's gain

  gon = u3m_soft_with(0, _defer_pair, u3_nul, u3a_defer_open);
  u3z(gon);
  u3a_defer_close();
  u3a_defer_close();
  u3z(hom);
  return ret_i;
}

/* _test_edit_road(): u3i_edit() copies on the home road, and edits a
**                    uniquely held cell in place on a junior road.
*/
static c3_i
_test_edit_road(void)
{
  c3_i ret_i = 1;

  //  home road: copied, so a hashcons member can't change under the set
  //
  {
    u3_noun q   = u3nt(1, 2, 3);
    u3_noun pro = u3i_edit(q, 2, 99);

    if ( (pro == q) || (99 != u3h(pro)) || (2 != u3h(u3t(pro))) ) {
      fprintf(stderr, "edit: home road edited in place\r\n");
      ret_i = 0;
    }
    u3z(pro);
  }

  //  junior road: edited in place
  //
  {
    u3_noun q, pro;
    c3_t    our_t;

    u3m_hate(0);

    q     = u3nt(1, 2, 3);
    pro   = u3i_edit(q, 2, 99);
    our_t = (pro == q) && (99 == u3h(pro)) && (2 == u3h(u3t(pro)));
    u3z(pro);

    u3m_love(0);

    if ( !our_t ) {
      fprintf(stderr, "edit: junior road not edited in place\r\n");
      ret_i = 0;
    }
  }

  return ret_i;
}

/* _reap_fill(): u3m_soft() test fn: leaves half of the road's free
**               memory filled, as roads leave what they have used: with
**               the bytes of a hashtable slot that points nowhere.
*/
static u3_noun
_reap_fill(u3_noun arg)
{
  c3_w  len_w = u3a_open(u3R) / 2;
  c3_w* buf_w = u3a_walloc(len_w);

  memset(buf_w, 0x6b, (size_t)len_w * sizeof(c3_w));
  u3a_wfree(buf_w);
  return arg;
}

/* _reap_fol(): the formula that produces [aga] by running [1 aga] on 0,
**              with the pair of them, [0 1 aga], as a constant.
*/
static u3_noun
_reap_fol(u3_noun aga)
{
  return u3nt(7, u3nc(1, u3nt(0, 1, aga)), u3nq(2, u3nc(0, 2), 0, 3));
}

/* _reap_part(): u3m_soft_run() test fn: makes and runs _reap_fol(aga).
**
**   The key of a program is [0 formula], deduplicated: the key of
**   [1 aga] is the constant, a part of the key of the formula around it.
*/
static u3_noun
_reap_part(u3_noun aga, u3_noun agb)
{
  return u3n_nock_on(u3_nul, _reap_fol(aga));
}

/* _reap_deep(): u3m_soft_run() test fn: _reap_part(agb), [aga] roads down.
*/
static u3_noun
_reap_deep(u3_noun aga, u3_noun agb)
{
  u3_noun gon = ( 0 == aga )
              ? u3m_soft_run(u3_nul, _reap_part, agb, u3_nul)
              : u3m_soft_run(u3_nul, _reap_deep, aga - 1, agb);
  u3_noun pro = u3k(u3t(gon));

  u3z(gon);
  return pro;
}

/* _reap_top(): u3m_soft() test fn: _reap_part(arg), three roads down.
*/
static u3_noun
_reap_top(u3_noun arg)
{
  return _reap_deep(2, arg);
}

/* _test_reap(): a junior program whose key has left before it is reaped.
**
**   u3n_reap() takes the keys and the literals of the programs that the
**   parent road lacks, in the order of the walk. A key met later can be
**   a part of what was taken: its mug slot then holds the address of its
**   copy, and looked up as it is, it reads the parent's table past its
**   end. The memory there is free memory, filled here beforehand.
*/
static c3_i
_test_reap(void)
{
  c3_i    ret_i = 1;
  u3_atom aga;
  u3_noun gon;

  //  the least atom for which the walk (in the order of the mugs) meets
  //  the key of the formula before the key of its part
  //
  for ( aga = 0; ; aga++ ) {
    u3_noun fol = _reap_fol(aga);
    u3_noun key = u3nc(u3_nul, fol);
    c3_o    fit = __( u3r_mug(key) < u3r_mug(u3t(u3h(u3t(fol)))) );

    u3z(key);
    if ( c3y == fit ) {
      break;
    }
  }

  u3z(u3m_soft(0, _reap_fill, u3_nul));
  gon = u3m_soft(0, _reap_top, aga);

  if ( (0 != u3h(gon)) || (aga != u3t(gon)) ) {
    u3m_p("reap: unexpected product", gon);
    ret_i = 0;
  }

  u3z(gon);
  return ret_i;
}

/* _part_run(): u3m_soft_run() test fn: runs formula [aga].
*/
static u3_noun
_part_run(u3_noun aga, u3_noun agb)
{
  return u3n_nock_on(u3_nul, u3k(aga));
}

/* _part_top(): u3m_soft() test fn: makes a formula on this road, runs it
**              a road down, and produces it with its product.
**
**   The road below finds home's program for an equal formula. Its key
**   keeps this road's formula, since senior unification is off for the
**   run (as it is under memory debugging).
*/
static u3_noun
_part_top(u3_noun arg)
{
  u3_noun fol = u3nt(1, 77, 88);
  u3_noun gon;

  u3C.wag_h |= u3o_debug_ram;
  gon = u3m_soft_run(u3_nul, _part_run, u3k(fol), u3_nul);
  u3C.wag_h &= ~u3o_debug_ram;

  return u3nc(fol, gon);
}

/* _test_reap_part(): a junior program whose key hasn't left, but whose
**                    formula has, with the product.
**
**   Home has a program for an equal formula, and the junior one must be
**   merged into it: looked up by its key, whose formula holds the address
**   of its copy where its mug was, it must still be found.
*/
static c3_i
_test_reap_part(void)
{
  c3_i    ret_i = 1;
  u3_noun fol   = u3nt(1, 77, 88);
  u3_noun key   = u3nc(u3_nul, u3k(fol));
  u3_noun gon;
  u3_weak one, two;

  gon = u3m_soft(0, _nock_fol, u3k(fol));
  u3z(gon);
  one = u3h_git(u3R->byc.har_p, key);

  gon = u3m_soft(0, _part_top, u3_nul);

  if ( (0 != u3h(gon)) || (c3n == u3r_sing(fol, u3h(u3t(gon)))) ) {
    u3m_p("reap part: unexpected product", gon);
    ret_i = 0;
  }
  u3z(gon);
  two = u3h_git(u3R->byc.har_p, key);

  if ( (u3_none == one) || (one != two) ) {
    fprintf(stderr, "reap part: home's program was replaced, not merged\r\n");
    ret_i = 0;
  }

  u3z(key);
  u3z(fol);
  return ret_i;
}

int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_meme() ) {
    fprintf(stderr, "test meme: failed\r\n");
    exit(1);
  }

  if ( !_test_soft_cax() ) {
    fprintf(stderr, "test soft_cax: failed\r\n");
    exit(1);
  }
  fprintf(stderr, "test soft_cax: ok\r\n");

  if ( !_test_defer() ) {
    fprintf(stderr, "test defer: failed\r\n");
    exit(1);
  }
  fprintf(stderr, "test defer: ok\r\n");

  if ( !_test_reap() ) {
    fprintf(stderr, "test reap: failed\r\n");
    exit(1);
  }
  fprintf(stderr, "test reap: ok\r\n");

  if ( !_test_reap_part() ) {
    fprintf(stderr, "test reap part: failed\r\n");
    exit(1);
  }
  fprintf(stderr, "test reap part: ok\r\n");

  if ( !_test_edit_road() ) {
    fprintf(stderr, "test edit road: failed\r\n");
    exit(1);
  }
  fprintf(stderr, "test edit road: ok\r\n");

  //  GC
  //
  u3m_grab();

  fprintf(stderr, "test meme: ok\r\n");
  return 0;
}
