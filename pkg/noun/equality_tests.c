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
_test_unify_home(void)
{
  c3_i ret_i = 1;

  u3_noun  a = u3nt(0, 0, 0);
  u3_noun  b = u3nt(0, 0, 0);
  c3_w kep_w;

  u3_assert( u3t(a) < u3t(b) );
  kep_w = u3t(a);

  (void)u3r_sing(a, b);

  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify home: failed\r\n");
    ret_i = 0;
  }
  else if ( kep_w != u3t(b) ) {
    fprintf(stderr, "test: unify home: deeper failed\r\n");
    ret_i = 0;
  }

  u3z(a); u3z(b);

  return ret_i;
}

static c3_i
_test_unify_inner(void)
{
  c3_i   ret_i = 1;
  c3_w   kep_w;
  u3_noun a, b;

  a = u3nt(0, 0, 0);
  kep_w = u3t(a);

  u3m_hate(0);

  b = u3nt(0, 0, 0);

  (void)u3r_sing(a, b);

  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify inner 1: failed\r\n");
    ret_i = 0;
  }
  else if ( kep_w != u3t(b) ) {
    fprintf(stderr, "test: unify inner 1: deeper failed\r\n");
    ret_i = 0;
  }

  b = u3m_love(0);

  u3z(a); u3z(b);

  //  --------

  b = u3nt(0, 0, 0);
  kep_w = u3t(b);

  u3m_hate(0);

  a = u3nt(0, 0, 0);

  u3m_hate(0);

  (void)u3r_sing(a, b);

  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify inner 2: failed\r\n");
    ret_i = 0;
  }
  else if ( kep_w != u3t(a) ) {
    fprintf(stderr, "test: unify inner 2: deeper failed\r\n");
    ret_i = 0;
  }

  a = u3m_love(u3m_love(0));

  u3z(a); u3z(b);

  return ret_i;
}

static c3_i
_test_unify_inner_home(void)
{
  c3_i ret_i = 1;

  u3_noun  a = u3nt(0, 0, 0);
  u3_noun  b = u3nt(0, 0, 0);

  u3m_hate(0);

  (void)u3r_sing(a, b);

  if ( u3t(a) == u3t(b) ) {
    fprintf(stderr, "test: unify inner-home: succeeded?\r\n");
    ret_i = 0;
  }

  u3m_love(0);

  u3z(a); u3z(b);

  return ret_i;
}

static c3_i
_test_equality(void)
{
  c3_i ret_i = 1;

  if ( !_test_unify_home() ) {
    fprintf(stderr, "test: equality: unify home: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_unify_inner() ) {
    fprintf(stderr, "test: equality: unify inner: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_unify_inner_home() ) {
    fprintf(stderr, "test: equality: unify inner-home: failed\r\n");
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
