/// @file

#include "noun.h"
#include "crib.h"
#include "events.h"

#include <time.h>

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
}

/* _GRAB_EXACT(): a grab of this road that counts references, with the
**   given roots, finds no leak and no count to repair; else fail [cas_c].
*/
#define _GRAB_EXACT(cas_c, ...)                                         \
  do {                                                                  \
    c3_h wag_h = u3C.wag_h;                                             \
                                                                        \
    u3C.wag_h |= u3o_debug_ram;                                         \
    u3m_grab(__VA_ARGS__);                                              \
    u3C.wag_h = wag_h;                                                  \
    if ( u3a_sweep_leaked() ) {                                         \
      fprintf(stderr, "test: %s: leaked %" PRIc3_w " words\r\n",        \
              cas_c, u3a_sweep_leaked());                               \
      ret_i = 0;                                                        \
    }                                                                   \
  } while (0)

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

/* _test_unify_senior(): equal cells on an inner road, compared from the
**                       road below it, are unified there with exact
**                       counts: the kept tail is gained, and the other
**                       lost when the senior road is current again.
*/
static c3_i
_test_unify_senior(void)
{
  c3_i    ret_i = 1;
  u3_noun a, b;

  u3m_hate(0);

  a = u3nt(0, 0, 0);
  b = u3nt(0, 0, 0);

  u3m_hate(0);
  (void)u3r_sing(a, b);
  u3m_love(0);

  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify senior: not unified\r\n");
    ret_i = 0;
  }
  else if ( 2 != u3a_use(u3t(a)) ) {
    fprintf(stderr, "test: unify senior: count %" PRIc3_w ", not 2\r\n",
            u3a_use(u3t(a)));
    ret_i = 0;
  }

  _GRAB_EXACT("unify senior", a, b);

  u3z(a); u3z(b);
  u3m_love(0);

  return ret_i;
}

/* _test_unify_senior_deep(): equal cells on an inner road, compared two
**                            roads below it: the entry rises a road at
**                            the first exit, with the middle road's list
**                            marked by a grab there, and the other cell
**                            is lost when its own road is current again.
*/
static c3_i
_test_unify_senior_deep(void)
{
  c3_i    ret_i = 1;
  c3_d    cop_d = u3e_Meter.wlc_d;
  u3_noun a, b;

  u3m_hate(0);

  a = u3nt(0, 0, 0);
  b = u3nt(0, 0, 0);

  u3m_hate(0);
  u3m_hate(0);
  (void)u3r_sing(a, b);
  u3m_love(0);

  if ( (cop_d + 1) != u3e_Meter.wlc_d ) {
    fprintf(stderr, "test: unify senior deep: %" PRIu64 " copied up, not 1\r\n",
            u3e_Meter.wlc_d - cop_d);
    ret_i = 0;
  }

  //  a grab of the middle road finds its list
  //
  _GRAB_EXACT("unify senior deep: middle road", u3_none);
  u3m_love(0);

  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify senior deep: not unified\r\n");
    ret_i = 0;
  }
  else if ( 2 != u3a_use(u3t(a)) ) {
    fprintf(stderr, "test: unify senior deep: count %" PRIc3_w ", not 2\r\n",
            u3a_use(u3t(a)));
    ret_i = 0;
  }

  _GRAB_EXACT("unify senior deep", a, b);

  u3z(a); u3z(b);
  u3m_love(0);

  return ret_i;
}

/* _test_unify_senior_slot(): cells on an inner road holding equal home
**                            nouns, compared from the road below it, get
**                            one slot pointed at the other home noun,
**                            uncounted: the home counts don't change.
*/
static c3_i
_test_unify_senior_slot(void)
{
  c3_i    ret_i = 1;
  u3_noun h = u3nc(1, 2), j = u3nc(1, 2);
  c3_w    hus_w = u3a_use(h), jus_w = u3a_use(j);
  u3_noun a, b;

  u3m_hate(0);

  a = u3nc(u3k(h), 0);
  b = u3nc(u3k(j), 0);

  u3m_hate(0);
  (void)u3r_sing(a, b);
  u3m_love(0);

  if ( u3h(a) != u3h(b) ) {
    fprintf(stderr, "test: unify senior slot: not unified\r\n");
    ret_i = 0;
  }
  if ( (u3a_use(h) != hus_w) || (u3a_use(j) != jus_w) ) {
    fprintf(stderr, "test: unify senior slot: home counts changed\r\n");
    ret_i = 0;
  }

  u3z(a); u3z(b);
  u3m_love(0);

  u3z(h); u3z(j);
  return ret_i;
}

/* _test_unify_senior_full(): debugging memory, with no room on this road
**                            for a lose-list chunk, equal cells on the road
**                            above are left as they are, with exact counts,
**                            not written uncounted from another road.
*/
static c3_i
_test_unify_senior_full(void)
{
  c3_i    ret_i = 1;
  c3_h    wag_h = u3C.wag_h;
  u3_noun a, b;

  u3m_hate(0);

  a = u3nt(0, 0, 0);
  b = u3nt(0, 0, 0);

  //  fill this road until a new chunk wouldn't leave 8 pages free; the
  //  heap goes with the road
  //
  u3m_hate(0);
  while ( u3a_open(u3R) > ((c3_w)6 << u3a_page) ) {
    (void)u3a_walloc((c3_w)1 << u3a_page);
  }
  u3C.wag_h |= u3o_debug_ram;
  (void)u3r_sing(a, b);
  u3C.wag_h = wag_h;
  u3m_love(0);

  if ( u3t(a) == u3t(b) ) {
    fprintf(stderr, "test: unify senior full: unified without room\r\n");
    ret_i = 0;
  }
  else if ( (1 != u3a_use(u3t(a))) || (1 != u3a_use(u3t(b))) ) {
    fprintf(stderr, "test: unify senior full: counts %" PRIc3_w " %" PRIc3_w ", not 1\r\n",
            u3a_use(u3t(a)), u3a_use(u3t(b)));
    ret_i = 0;
  }
  _GRAB_EXACT("unify senior full", a, b);

  u3z(a); u3z(b);
  u3m_love(0);

  return ret_i;
}

/* _test_lose_full(): past the home lose list's ceiling, a home slot is
**                    still written and its old value leaked, for the grab
**                    u3o_check_corrupt asks for; debugging memory, the
**                    write is refused instead.
*/
static c3_i
_test_lose_full(void)
{
  c3_i    ret_i = 1;
  c3_w    num_w = 20, i_w, uni_w = 0;
  u3_noun hol = u3_nul;

  //  [x 0] and [y 0], with x and y equal home cells, held in [hol]
  //
  for ( i_w = 0; i_w < num_w; i_w++ ) {
    hol = u3nc(u3nc(u3nc(u3nc(i_w, 2), 0), u3nc(u3nc(i_w, 2), 0)), hol);
  }

  u3C.wag_h &= ~u3o_check_corrupt;
  u3a_lose_size(16, 16);
  u3m_hate(0);
  {
    u3_noun lis = hol;
    while ( u3_nul != lis ) {
      (void)u3r_sing(u3h(u3h(lis)), u3t(u3h(lis)));
      lis = u3t(lis);
    }
  }
  u3m_love(0);

  {
    u3_noun lis = hol;
    while ( u3_nul != lis ) {
      if ( u3h(u3h(u3h(lis))) == u3h(u3t(u3h(lis))) ) uni_w++;
      lis = u3t(lis);
    }
  }

  if ( u3a_dbg_ram() ) {
    if ( (16 != uni_w) || (u3C.wag_h & u3o_check_corrupt) ) {
      fprintf(stderr, "test: lose full: debugging, %" PRIc3_w " unified\r\n", uni_w);
      ret_i = 0;
    }
  }
  else if ( (num_w != uni_w) || !(u3C.wag_h & u3o_check_corrupt) ) {
    fprintf(stderr, "test: lose full: %" PRIc3_w " unified, check %d\r\n",
            uni_w, !!(u3C.wag_h & u3o_check_corrupt));
    ret_i = 0;
  }
  else {
    c3_h wag_h = u3C.wag_h;

    //  the repair grab doesn't crash on the leak it's for
    //
    u3C.wag_h |= u3o_leak_crash;
    u3m_grab_check(hol);
    if ( !u3a_sweep_leaked() ) {
      fprintf(stderr, "test: lose full: the grab found no leak\r\n");
      ret_i = 0;
    }
    u3m_grab(hol);
    if ( u3a_sweep_leaked() ) {
      fprintf(stderr, "test: lose full: still leaking after the grab\r\n");
      ret_i = 0;
    }
    u3C.wag_h = wag_h & ~u3o_check_corrupt;
  }

  u3a_lose_size(1U << 14, 1U << 23);
  u3z(hol);
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

  //  one home slot is pointed at the other's value, which gains it on
  //  the home road now, and the old value is lost at the exit
  //
  if ( u3t(a) != u3t(b) ) {
    fprintf(stderr, "test: unify inner-home: not unified\r\n");
    ret_i = 0;
  }
  else if ( 2 != u3a_use(u3t(a)) ) {
    fprintf(stderr, "test: unify inner-home: kept value not gained (%" PRIc3_w ")\r\n",
            u3a_use(u3t(a)));
    ret_i = 0;
  }

  u3m_love(0);

  if ( (u3t(a) != u3t(b)) || (2 != u3a_use(u3t(a))) ) {
    fprintf(stderr, "test: unify inner-home: wrong after the exit\r\n");
    ret_i = 0;
  }

  u3z(a); u3z(b);

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

/* _dag_on(): [x_n x_n] for x_0 = [p q], n deep: 2^n leaves, n+1 cells.
*/
static u3_noun
_dag_on(c3_w dep_w, u3_noun p, u3_noun q)
{
  u3_noun x = u3nc(p, q);
  for ( c3_w i_w = 0; i_w < dep_w; i_w++ ) {
    x = u3nc(u3k(x), x);
  }
  return x;
}

/* _us_since(): microseconds since [bef_u].
*/
static c3_d
_us_since(struct timespec* bef_u)
{
  struct timespec aft_u;
  clock_gettime(CLOCK_MONOTONIC, &aft_u);
  return (c3_d)(aft_u.tv_sec - bef_u->tv_sec) * 1000000
       + (aft_u.tv_nsec - bef_u->tv_nsec) / 1000;
}

/* _sing_dag(): compare two equal nouns of [dep_w] levels, built apart,
**   in the current setting. with 2^dep_w paths, a comparison that
**   re-compared every path would never finish.
*/
static c3_i
_sing_dag(const c3_c* cas_c, u3_noun a, u3_noun b)
{
  if ( c3y != u3r_sing(a, b) ) {
    fprintf(stderr, "test: sing dag: %s: not equal\r\n", cas_c);
    return 0;
  }
  return 1;
}

/* _test_sing_dag(): comparisons of equal DAGs with 2^200 paths finish, in
**                   every place unification can merge or write a slot: a
**                   comparison that re-compared every path never would.
*/
static c3_i
_test_sing_dag(void)
{
  c3_i    ret_i = 1;
  u3_noun a, b;

  //  home road: unification merges what it compares
  //
  a = _dag(200); b = _dag(200);
  ret_i &= _sing_dag("home", a, b);
  u3z(a); u3z(b);

  //  inner road, two home nouns: a home slot is written
  //
  a = _dag(200); b = _dag(200);
  u3m_hate(0);
  ret_i &= _sing_dag("inner, home", a, b);
  u3m_love(0);
  u3z(a); u3z(b);

  //  inner road, one junior: unification
  //
  a = _dag(200);
  u3m_hate(0);
  b = _dag(200);
  ret_i &= _sing_dag("inner, junior", a, b);
  u3m_love(0);
  u3z(a);

  //  inner road, a junior DAG over a home one, against an equal home
  //  copy: junior slots and home slots are written
  //
  a = _dag(200); b = _dag(220);
  u3m_hate(0);
  {
    u3_noun j = u3k(a);
    for ( c3_w i_w = 0; i_w < 20; i_w++ ) {
      j = u3nc(u3k(j), j);
    }
    ret_i &= _sing_dag("inner, junior over home", j, b);
    u3z(j);
  }
  u3m_love(0);
  u3z(a); u3z(b);

  //  a nested road, both on the road above: unification there
  //
  u3m_hate(0);
  a = _dag(200); b = _dag(200);
  u3m_hate(0);
  ret_i &= _sing_dag("nested", a, b);
  u3m_love(0);
  u3m_love(0);

  //  as while promoting a junior road ([kid_p] set): this road unifies
  //  too, losing old values at the end of the exit (here, by hand)
  //
  a = _dag(200); b = _dag(200);
  u3R->kid_p = u3of(u3a_road, u3R);
  ret_i &= _sing_dag("promotion", a, b);
  u3R->kid_p = 0;
  u3a_lose_drain(0, 0, 0, 0);
  if ( u3h(a) != u3h(b) ) {
    fprintf(stderr, "test: sing dag: promotion: not unified\r\n");
    ret_i = 0;
  }
  u3z(a); u3z(b);

  return ret_i;
}

/* _test_crib(): a crib grows to its ceiling, and gives memory back.
*/
static c3_i
_test_crib(void)
{
  c3_i ret_i = 1;

  //  the region
  //
  {
    u3a_crib cib_u = {0};
    size_t   max_i = 1 << 20;
    c3_w     bad_w = 0;

    u3a_crib_init(&cib_u, 4096, max_i);
    memset(cib_u.buf_y, 0xff, 4096);
    if ( !u3a_crib_grow(&cib_u, max_i) || u3a_crib_grow(&cib_u, max_i + 1)
       || (max_i != cib_u.len_i) )
    {
      fprintf(stderr, "test: crib: grow\r\n");
      ret_i = 0;
    }
    for ( size_t i_i = 4096; i_i < max_i; i_i++ ) {
      if ( cib_u.buf_y[i_i] ) bad_w++;
    }
    memset(cib_u.buf_y, 0xff, max_i);
    u3a_crib_shrink(&cib_u, 4096 + 7);
    if ( (4096 + 7) != cib_u.len_i ) bad_w++;
    u3_assert( u3a_crib_grow(&cib_u, max_i) );
    for ( size_t i_i = 0; i_i < max_i; i_i++ ) {
      if ( (i_i < 4096 + 7) != !!cib_u.buf_y[i_i] ) bad_w++;
    }
    if ( bad_w ) {
      fprintf(stderr, "test: crib: %u bytes wrong\r\n", (unsigned)bad_w);
      ret_i = 0;
    }
    //  NB: the test's reservation (1 MiB of address space) is kept
  }

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

static c3_i
_test_equality(void)
{
  c3_i ret_i = 1;

  if ( !_test_dedup() ) {
    fprintf(stderr, "test: equality: dedup: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_hcons() ) {
    fprintf(stderr, "test: equality: hcons: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_crib() ) {
    fprintf(stderr, "test: equality: crib: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sing_dag() ) {
    fprintf(stderr, "test: equality: sing dag: failed\r\n");
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

  if ( !_test_unify_senior() ) {
    fprintf(stderr, "test: equality: unify senior: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_unify_senior_deep() ) {
    fprintf(stderr, "test: equality: unify senior deep: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_unify_senior_slot() ) {
    fprintf(stderr, "test: equality: unify senior slot: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_unify_senior_full() ) {
    fprintf(stderr, "test: equality: unify senior full: failed\r\n");
    ret_i = 0;
  }
  if ( !_test_lose_full() ) {
    fprintf(stderr, "test: equality: lose full: failed\r\n");
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

  //  GC, failing on a leak
  //
  u3C.wag_h |= u3o_leak_crash;
  u3m_grab();

  fprintf(stderr, "test equality: ok\r\n");
  return 0;
}
