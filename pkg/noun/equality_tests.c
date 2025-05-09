/// @file

#include "noun.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
}

static c3_i
_test_unify(void)
{
  c3_i ret_i = 1;

  u3_noun  a = u3nt(0, 0, 0);
  u3_noun  b = u3nt(0, 0, 0);
  c3_w kep_w;

  fprintf(stderr, "before: 0x%x 0x%x\r\n", u3t(a), u3t(b));
  u3_assert( u3t(a) < u3t(b) );
  kep_w = u3t(a);

  (void)u3r_sing(a, b);

  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify: failed\r\n");
    ret_i = 0;
  }
  else if ( kep_w != u3t(b) ) {
    fprintf(stderr, "test: unify: deeper failed\r\n");
    ret_i = 0;
  }

  fprintf(stderr, "after: 0x%x 0x%x\r\n", u3t(a), u3t(b));

  u3z(a); u3z(b);

  return ret_i;
}

static c3_i
_test_equality(void)
{
  c3_i ret_i = 1;

  if ( !_test_unify() ) {
    fprintf(stderr, "test equality unify: failed\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_equality() ) {
    fprintf(stderr, "test equality: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test equality: ok\r\n");
  return 0;
}
