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

/* _test_hcons(): the off-loom home-road hashcons set.
*/
static c3_i
_test_hcons(void)
{
  c3_i    ret_i = 1;
  u3_noun x, y, r, p, q, a, b, m, n;

  //  a freed member leaves the set
  //
  x = u3m_dedup(u3nc(10, 11));
  y = u3nc(10, 11);
  u3z(x);
  r = u3m_dedup(u3k(y));
  if ( r != y ) {
    fprintf(stderr, "test: hcons: freed member still found\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(y);

  //  so do members freed in a cascade
  //
  x = u3m_dedup(u3nc(u3nc(12, 13), 14));
  y = u3nc(u3nc(12, 13), 14);
  u3z(x);
  r = u3m_dedup(u3k(y));
  if ( (r != y) || !u3a_hcon_has(u3h(y)) ) {
    fprintf(stderr, "test: hcons: cascade-freed member still found\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(y);

  //  junior entries that are taken join the home set
  //
  u3m_hate(0);
  p = u3m_dedup(u3nc(u3nc(20, 21), 22));
  r = u3m_dedup(u3nc(30, 31));     //  not taken
  u3z(r);
  p = u3m_love(p);
  if ( !u3a_hcon_has(p) || !u3a_hcon_has(u3h(p)) ) {
    fprintf(stderr, "test: hcons: taken entry not a member\r\n");
    ret_i = 0;
  }
  q = u3nc(u3nc(20, 21), 22);
  r = u3m_dedup(u3k(q));
  if ( r != p ) {
    fprintf(stderr, "test: hcons: taken entry not shared\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(q);
  q = u3nc(30, 31);
  r = u3m_dedup(u3k(q));
  if ( r != q ) {
    fprintf(stderr, "test: hcons: untaken entry kept\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(q); u3z(p);

  //  rebuilding keeps members reachable from persistent tables only
  //
  p = u3m_dedup(u3nc(40, 41));
  m = u3m_dedup(u3nc(42, 43));
  u3h_put(u3R->cax.per_p, 44, u3k(p));
  u3m_dedup_ream();
  if ( !u3a_hcon_has(p) || u3a_hcon_has(m) ) {
    fprintf(stderr, "test: hcons: ream kept the wrong members\r\n");
    ret_i = 0;
  }
  u3h_free(u3R->cax.per_p);
  u3R->cax.per_p = u3h_new_cache(u3C.per_w);
  u3z(p); u3z(m);

  //  unification on the home road keeps the member,
  //  whichever is allocated first
  //
  {
    c3_w i_w;

    for ( i_w = 0; i_w < 2; i_w++ ) {
      if ( i_w ) {
        n = u3nc(45, 46);
        m = u3m_dedup(u3nc(45, 46));
      }
      else {
        m = u3m_dedup(u3nc(45, 46));
        n = u3nc(45, 46);
      }

      a = u3nc(u3k(m), 1);
      b = u3nc(u3k(n), 1);
      (void)u3r_sing(a, b);
      if ( (u3h(a) != m) || (u3h(b) != m) ) {
        fprintf(stderr, "test: hcons: unified away from the member (%" PRIc3_w ")\r\n", i_w);
        ret_i = 0;
      }
      u3z(a); u3z(b); u3z(m); u3z(n);
    }
  }

  return ret_i;
}

static c3_i
_test_equality(void)
{
  c3_i ret_i = 1;

  if ( !_test_hcons() ) {
    fprintf(stderr, "test: equality: hcons: failed\r\n");
    ret_i = 0;
  }

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
