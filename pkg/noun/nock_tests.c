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
  u3a_defer_arm();
  gon = u3m_soft(0, _defer_pair, arg);

  if ( bas_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: written while open: have %u, need %u\r\n",
                    u3a_use(hom), bas_w);
    ret_i = 0;
  }

  u3a_defer_close();

  if ( tak_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: close: have %u, need %u\r\n",
                    u3a_use(hom), tak_w);
    ret_i = 0;
  }

  u3z(gon);

  //  armed, product released before close: the gains cancel
  //
  arg = u3k(hom);
  u3a_defer_arm();
  gon = u3m_soft(0, _defer_pair, arg);
  u3z(gon);
  u3a_defer_close();

  if ( rel_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: cancel: have %u, need %u\r\n",
                    u3a_use(hom), rel_w);
    ret_i = 0;
  }

  //  past the cap, gains on new nouns are written as usual, and the
  //  result is the same: [hom] fills the table, so the product's other
  //  noun (a fresh atom, not in it) spills
  //
  {
    c3_w    max_w = u3a_Defer_max_w;
    u3_noun gun;

    arg = u3k(hom);
    u3a_Defer_max_w = 1;
    u3a_defer_arm();
    gon = u3m_soft(0, _defer_pair, arg);

    if ( bas_w != u3a_use(hom) ) {
      fprintf(stderr, "defer: cap: hom written while open\r\n");
      ret_i = 0;
    }

    gun = u3m_soft(0, _defer_pair, u3i_string("spill"));

    if ( 2 != u3a_use(u3h(u3t(gun))) ) {
      fprintf(stderr, "defer: cap: spill not written: %u\r\n",
                      u3a_use(u3h(u3t(gun))));
      ret_i = 0;
    }

    u3a_defer_close();
    u3a_Defer_max_w = max_w;

    if ( tak_w != u3a_use(hom) ) {
      fprintf(stderr, "defer: cap: have %u, need %u\r\n",
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
  u3a_defer_arm();
  gon = u3m_soft(0, _defer_pair, arg);
  u3z(hom);
  u3a_defer_close();

  if ( c3n == u3r_sing_c("deferred", u3h(u3h(u3t(gon)))) ) {
    fprintf(stderr, "defer: product damaged\r\n");
    ret_i = 0;
  }

  hom = u3k(u3h(u3t(gon)));
  u3z(gon);

  //  a failed event never opens the window: u3m_soft() renders its
  //  trace on the home road
  //
  bas_w = u3a_use(hom);
  u3a_defer_arm();
  gon = u3m_soft(0, _defer_fail, u3k(hom));
  u3z(gon);

  if ( bas_w != u3a_use(hom) ) {
    fprintf(stderr, "defer: failure: have %u, need %u\r\n",
                    u3a_use(hom), bas_w);
    ret_i = 0;
  }

  arg = u3k(hom);

  if ( (bas_w + 1) != u3a_use(hom) ) {
    fprintf(stderr, "defer: window open after failure\r\n");
    ret_i = 0;
  }

  u3z(arg);
  u3a_defer_close();
  u3z(hom);
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

  //  GC
  //
  u3m_grab();

  fprintf(stderr, "test meme: ok\r\n");
  return 0;
}
