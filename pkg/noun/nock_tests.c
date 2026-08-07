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

  //  GC
  //
  u3m_grab();

  fprintf(stderr, "test meme: ok\r\n");
  return 0;
}
