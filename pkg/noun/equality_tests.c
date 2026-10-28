/// @file

#include "noun.h"

#include <time.h>

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

  //  home nouns listed in a junior set are left alone at exit: the set
  //  holds no references to them, so canonicalizing [x] in place would
  //  free [c], which it still lists (#1133). they're put directly,
  //  standing for a set that lists a home noun whose canonical equal
  //  [m] is a member by the time the road exits
  //
  m = u3m_dedup(u3nc(23, 24));
  {
    u3_noun c = u3nc(23, 24);
    u3_noun x = u3nc(c, 25);

    u3m_hate(0);
    u3h_put(u3R->dup_p, x, x);
    u3h_put(u3R->dup_p, c, c);
    u3m_love(0);

    if ( (u3h(x) != c) || u3a_hcon_has(x) ) {
      fprintf(stderr, "test: hcons: listed home noun rewritten\r\n");
      ret_i = 0;
    }
    u3z(x);
  }
  u3z(m);

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

/* _dag(): [x_n x_n] for x_0 = [1 2], n deep: 2^n leaves, n+1 cells.
*/
static u3_noun
_dag(c3_w dep_w)
{
  u3_noun x = u3nc(1, 2);
  for ( c3_w i_w = 0; i_w < dep_w; i_w++ ) {
    x = u3nc(u3k(x), x);
  }
  return x;
}

/* _test_dedup_dag(): deduping a shared noun on an inner road takes time
**                    in its distinct cells, not its leaves, even when the
**                    home road already holds an equal noun (whose senior
**                    copy can't be rewritten).
*/
static c3_i
_test_dedup_dag(void)
{
  c3_i ret_i = 1;

  //  an equal member exists for the whole noun, or for a part of it
  //
  for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
    struct timespec bef_u, aft_u;
    u3_noun can, fre, r;
    c3_d    mic_d;

    can = u3m_dedup(_dag(i_w ? 13 : 26));
    fre = _dag(26);

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    r = u3m_dedup(u3k(fre));
    if ( !i_w && (r != can) ) {
      fprintf(stderr, "test: dedup dag: member not found\r\n");
      ret_i = 0;
    }
    if ( c3n == u3r_sing(r, fre) ) {
      fprintf(stderr, "test: dedup dag: changed the value\r\n");
      ret_i = 0;
    }
    u3z(r);
    u3m_love(0);
    clock_gettime(CLOCK_MONOTONIC, &aft_u);

    //  2^26 leaves: a walk of them takes seconds
    //
    mic_d = (c3_d)(aft_u.tv_sec - bef_u.tv_sec) * 1000000
          + (aft_u.tv_nsec - bef_u.tv_nsec) / 1000;
    if ( mic_d > 1000000 ) {
      fprintf(stderr, "test: dedup dag: %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }

    u3z(can); u3z(fre);
  }

  //  a shared noun on an intermediate road is unified in place
  //
  {
    struct timespec bef_u, aft_u;
    u3_noun can, fre, r;
    c3_d    mic_d;

    can = u3m_dedup(_dag(26));
    u3m_hate(0);
    fre = _dag(26);

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    r = u3m_dedup(u3k(fre));
    if ( r != can ) {
      fprintf(stderr, "test: dedup dag: middle member not found\r\n");
      ret_i = 0;
    }
    u3z(r);
    u3m_love(0);
    clock_gettime(CLOCK_MONOTONIC, &aft_u);

    if ( u3h(fre) != u3h(can) ) {
      fprintf(stderr, "test: dedup dag: middle not unified\r\n");
      ret_i = 0;
    }
    mic_d = (c3_d)(aft_u.tv_sec - bef_u.tv_sec) * 1000000
          + (aft_u.tv_nsec - bef_u.tv_nsec) / 1000;
    if ( mic_d > 1000000 ) {
      fprintf(stderr, "test: dedup dag: middle %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
    u3z(fre);
    u3m_love(0);
    u3z(can);
  }

  //  but not while debugging memory (as u3a_wed()), where the old
  //  children would be reported as leaks when the road is swept
  //
  {
    c3_h    wag_h = u3C.wag_h;
    struct timespec bef_u, aft_u;
    u3_noun can, fre, r;
    c3_d    mic_d;

    can = u3m_dedup(_dag(26));
    u3m_hate(0);
    fre = _dag(26);

    u3C.wag_h |= u3o_debug_ram | u3o_leak_crash;
    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    r = u3m_dedup(u3k(fre));
    if ( r != can ) {
      fprintf(stderr, "test: dedup dag: debug member not found\r\n");
      ret_i = 0;
    }
    u3z(r);
    u3m_love(0);
    clock_gettime(CLOCK_MONOTONIC, &aft_u);

    if ( u3h(fre) == u3h(can) ) {
      fprintf(stderr, "test: dedup dag: debug unified in place\r\n");
      ret_i = 0;
    }
    mic_d = (c3_d)(aft_u.tv_sec - bef_u.tv_sec) * 1000000
          + (aft_u.tv_nsec - bef_u.tv_nsec) / 1000;
    if ( mic_d > 1000000 ) {
      fprintf(stderr, "test: dedup dag: debug %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }

    //  the road's sweep finds no leaks (and would abort if it did)
    //
    u3m_grab(fre);
    u3C.wag_h = wag_h;

    u3z(fre);
    u3m_love(0);
    u3z(can);
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

  if ( !_test_dedup_dag() ) {
    fprintf(stderr, "test: equality: dedup dag: failed\r\n");
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
