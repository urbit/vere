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

  if ( u3t(a) > u3t(b) ) {
    u3_noun t = a;
    a = b;
    b = t;
  }
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

/* _test_dedup(): hash-consing keeps canonical cells, copies the rest.
**
**   NB: comparison unifies equal sub-cells, so the expectations use
**   [t], a cell only ever compared at the top level.
*/
static c3_i
_test_dedup(void)
{
  c3_i    ret_i = 1;
  u3_noun t, b, c, d, s, e, r;

  //  a fresh noun is canonical as it stands
  //
  t = u3nc(2, 3);
  r = u3m_dedup(u3k(t));
  if ( r != t ) {
    fprintf(stderr, "test: dedup: fresh noun copied\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  an equal noun dedups to it
  //
  b = u3nc(2, 3);
  r = u3m_dedup(u3k(b));
  if ( r != t ) {
    fprintf(stderr, "test: dedup: equal noun not shared\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  a cell with a non-canonical child is repointed in place
  //
  c = u3nc(u3nc(2, 3), 4);
  r = u3m_dedup(u3k(c));
  if ( (r != c) || (u3h(c) != t) ) {
    fprintf(stderr, "test: dedup: non-canonical child not repointed\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  a cell whose children are canonical is kept
  //
  d = u3nc(u3k(t), 5);
  r = u3m_dedup(u3k(d));
  if ( r != d ) {
    fprintf(stderr, "test: dedup: canonical children copied\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  an inner road keeps a senior noun, rather than copying it
  //
  s = u3nc(6, 7);
  u3m_hate(0);
  r = u3m_dedup(u3k(s));
  if ( r != s ) {
    fprintf(stderr, "test: dedup: senior noun copied\r\n");
    ret_i = 0;
  }
  u3z(r);
  u3m_love(0);

  //  an inner road copies a senior cell with a non-canonical child
  //
  e = u3nc(u3nc(2, 3), 8);
  u3m_hate(0);
  r = u3m_dedup(u3k(e));
  if ( (r == e) || (u3h(r) != t) || (u3h(e) == t) ) {
    fprintf(stderr, "test: dedup: senior cell not copied\r\n");
    ret_i = 0;
  }
  u3z(r);
  u3m_love(0);

  u3z(t); u3z(b); u3z(c); u3z(d); u3z(s); u3z(e);
  u3m_dedup_prune();

  return ret_i;
}

static c3_i
_test_equality(void)
{
  c3_i ret_i = 1;

  if ( !_test_dedup() ) {
    fprintf(stderr, "test: equality: dedup: failed\r\n");
    ret_i = 0;
  }

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
  u3m_grab();

  fprintf(stderr, "test equality: ok\r\n");
  return 0;
}
