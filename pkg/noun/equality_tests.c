/// @file

#include "noun.h"
#include "crib.h"
#include "events.h"
#include "soletab.h"

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

  //  counting references, a grab of this road finds no leak and no count
  //  to repair
  //
  {
    c3_h wag_h = u3C.wag_h;

    u3C.wag_h |= u3o_debug_ram;
    u3m_grab(a, b);
    u3C.wag_h = wag_h;
    if ( u3a_sweep_leaked() ) {
      fprintf(stderr, "test: unify senior: leaked %" PRIc3_w " words\r\n",
              u3a_sweep_leaked());
      ret_i = 0;
    }
  }

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

  //  counting references, a grab of the middle road finds its list
  //
  {
    c3_h wag_h = u3C.wag_h;

    u3C.wag_h |= u3o_debug_ram;
    u3m_grab(u3_none);
    u3C.wag_h = wag_h;
    if ( u3a_sweep_leaked() ) {
      fprintf(stderr, "test: unify senior deep: middle road leaked %" PRIc3_w " words\r\n",
              u3a_sweep_leaked());
      ret_i = 0;
    }
  }
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

  {
    c3_h wag_h = u3C.wag_h;

    u3C.wag_h |= u3o_debug_ram;
    u3m_grab(a, b);
    u3C.wag_h = wag_h;
    if ( u3a_sweep_leaked() ) {
      fprintf(stderr, "test: unify senior deep: leaked %" PRIc3_w " words\r\n",
              u3a_sweep_leaked());
      ret_i = 0;
    }
  }

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
  u3a_wed_size(16, 16);
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

  u3a_wed_size(1U << 14, 1U << 22);
  u3z(hol);
  return ret_i;
}

/* _test_lose_full_eager(): with the home lose list full, the exit take's
**                          rewrite of an older home noun leaves a slot as
**                          it is, rather than leaking its old value.
*/
static c3_i
_test_lose_full_eager(void)
{
  c3_i    ret_i = 1;
  c3_w    num_w = 20, max_w = 16, i_w, swp_w = 0;
  c3_d    ctr_d = u3e_Meter.ctr_d;
  u3_noun mem = u3_nul, old = u3_nul, cop = u3_nul;

  //  [old] lists [i 2] for each i, uncounted elsewhere and not members;
  //  [mem] lists members equal to them. the walk meets [old]'s first
  //  element first
  //
  for ( i_w = num_w; i_w--; ) {
    mem = u3nc(u3m_sole_try(u3nc(i_w + 1, 2)), mem);
    old = u3nc(u3nc(i_w + 1, 2), old);
    cop = u3nc(u3nc(i_w + 1, 2), cop);
  }

  //  an inner road's cache holds [old], which the exit to home takes
  //
  u3a_wed_size(max_w, max_w);
  u3m_hate(0);
  u3h_put(u3R->cax.per_p, 50, u3k(old));
  u3m_love(0);

  {
    u3_noun lis = old, mis = mem;

    for ( i_w = 0; i_w < num_w; i_w++ ) {
      c3_t swp_t = ( u3h(lis) == u3h(mis) );

      if ( swp_t != (i_w < max_w) ) {
        fprintf(stderr, "test: lose full eager: element %" PRIc3_w " %s\r\n",
                i_w, swp_t ? "swapped" : "left");
        ret_i = 0;
      }
      swp_w += swp_t;
      lis = u3t(lis);
      mis = u3t(mis);
    }
  }

  if ( (max_w != swp_w) || ((num_w - max_w) != (u3e_Meter.ctr_d - ctr_d)) ) {
    fprintf(stderr, "test: lose full eager: %" PRIc3_w " swapped, %" PRIu64 " left\r\n",
            swp_w, u3e_Meter.ctr_d - ctr_d);
    ret_i = 0;
  }
  if ( u3a_sole_has(old) ) {
    fprintf(stderr, "test: lose full eager: a list with left slots is a member\r\n");
    ret_i = 0;
  }
  if ( u3C.wag_h & u3o_check_corrupt ) {
    fprintf(stderr, "test: lose full eager: asked for a grab\r\n");
    ret_i = 0;
  }

  //  counting references, the drain freed what was swapped out, and
  //  nothing else
  //
  {
    c3_h wag_h = u3C.wag_h;

    u3C.wag_h |= u3o_debug_ram;
    u3m_grab(mem, old, cop);
    u3C.wag_h = wag_h;
    if ( u3a_sweep_leaked() ) {
      fprintf(stderr, "test: lose full eager: leaked %" PRIc3_w " words\r\n",
              u3a_sweep_leaked());
      ret_i = 0;
    }
  }

  if ( c3n == u3r_sing(old, cop) ) {
    fprintf(stderr, "test: lose full eager: changed the value\r\n");
    ret_i = 0;
  }

  u3a_wed_size(1U << 14, 1U << 22);
  u3h_free(u3R->cax.per_p);
  u3R->cax.per_p = u3h_new_cache(u3C.per_w);
  u3z(mem); u3z(old); u3z(cop);
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

/* _test_sole(): hash-consing keeps canonical cells, copies the rest.
**
**   NB: comparison unifies equal sub-cells, so the expectations use
**   [t], a cell only ever compared at the top level.
*/
static c3_i
_test_sole(void)
{
  c3_i    ret_i = 1;
  u3_noun t, b, c, d, s, e, r;

  //  a fresh noun is canonical as it stands
  //
  t = u3nc(2, 3);
  r = u3m_sole_try(u3k(t));
  if ( r != t ) {
    fprintf(stderr, "test: sole: fresh noun copied\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  an equal noun canonicalizes to it
  //
  b = u3nc(2, 3);
  r = u3m_sole_try(u3k(b));
  if ( r != t ) {
    fprintf(stderr, "test: sole: equal noun not shared\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  a cell with a non-canonical child is repointed in place
  //
  c = u3nc(u3nc(2, 3), 4);
  r = u3m_sole_try(u3k(c));
  if ( (r != c) || (u3h(c) != t) ) {
    fprintf(stderr, "test: sole: non-canonical child not repointed\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  a cell whose children are canonical is kept
  //
  d = u3nc(u3k(t), 5);
  r = u3m_sole_try(u3k(d));
  if ( r != d ) {
    fprintf(stderr, "test: sole: canonical children copied\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  an inner road keeps a senior noun, rather than copying it
  //
  s = u3nc(6, 7);
  u3m_hate(0);
  r = u3m_sole_try(u3k(s));
  if ( r != s ) {
    fprintf(stderr, "test: sole: senior noun copied\r\n");
    ret_i = 0;
  }
  u3z(r);
  u3m_love(0);

  //  an inner road leaves a senior cell with a non-canonical child as
  //  it is, rather than copying it
  //
  e = u3nc(u3nc(2, 3), 8);
  u3m_hate(0);
  r = u3m_sole_try(u3k(e));
  if ( (r != e) || (u3h(e) == t) ) {
    fprintf(stderr, "test: sole: senior cell copied or rewritten\r\n");
    ret_i = 0;
  }
  u3z(r);
  u3m_love(0);
  u3z(e);

  //  but canonicalizes a junior cell's children, as on the home road
  //
  u3m_hate(0);
  e = u3nc(u3nc(2, 3), 8);
  r = u3m_sole_try(u3k(e));
  if ( (r != e) || (u3h(e) != t) ) {
    fprintf(stderr, "test: sole: junior child not repointed\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(e);
  u3m_love(0);
  e = 0;

  u3z(t); u3z(b); u3z(c); u3z(d); u3z(s); u3z(e);

  return ret_i;
}

/* _test_sole_set(): the off-loom home-road hashcons set.
*/
static c3_i
_test_sole_set(void)
{
  c3_i    ret_i = 1;
  u3_noun x, y, r, p, q, a, b, m, n;

  //  a freed member leaves the set
  //
  x = u3m_sole_try(u3nc(10, 11));
  y = u3nc(10, 11);
  u3z(x);
  r = u3m_sole_try(u3k(y));
  if ( r != y ) {
    fprintf(stderr, "test: sole set: freed member still found\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(y);

  //  so do members freed in a cascade
  //
  x = u3m_sole_try(u3nc(u3nc(12, 13), 14));
  y = u3nc(u3nc(12, 13), 14);
  u3z(x);
  r = u3m_sole_try(u3k(y));
  if ( (r != y) || !u3a_sole_has(u3h(y)) ) {
    fprintf(stderr, "test: sole set: cascade-freed member still found\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(y);

  //  pinned junior entries that are taken join the home set
  //
  u3m_hate(0);
  p = u3m_sole_pin(u3nc(u3nc(20, 21), 22));
  r = u3m_sole_pin(u3nc(30, 31));     //  not taken
  u3z(r);
  p = u3m_love(p);
  if ( !u3a_sole_has(p) || !u3a_sole_has(u3h(p)) ) {
    fprintf(stderr, "test: sole set: taken entry not a member\r\n");
    ret_i = 0;
  }
  q = u3nc(u3nc(20, 21), 22);
  r = u3m_sole_try(u3k(q));
  if ( r != p ) {
    fprintf(stderr, "test: sole set: taken entry not shared\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(q);
  q = u3nc(30, 31);
  r = u3m_sole_try(u3k(q));
  if ( r != q ) {
    fprintf(stderr, "test: sole set: untaken entry kept\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(q); u3z(p);

  //  home nouns listed in a junior set are left alone at exit: the set
  //  holds no references to them, so canonicalizing [x] in place would
  //  free [c], which it still lists (#1133). they're put directly,
  //  standing for a set that lists a home noun whose canonical equal
  //  [m] is a member by the time the road exits
  //
  m = u3m_sole_try(u3nc(23, 24));
  {
    u3_noun c = u3nc(23, 24);
    u3_noun x = u3nc(c, 25);

    u3m_hate(0);
    u3h_put(u3R->dup_p, x, x);
    u3h_put(u3R->dup_p, c, c);
    u3m_love(0);

    if ( (u3h(x) != c) || u3a_sole_has(x) ) {
      fprintf(stderr, "test: sole set: listed home noun rewritten\r\n");
      ret_i = 0;
    }
    u3z(x);
  }
  u3z(m);

  //  rebuilding keeps members reachable from persistent tables only
  //
  p = u3m_sole_try(u3nc(40, 41));
  m = u3m_sole_try(u3nc(42, 43));
  u3h_put(u3R->cax.per_p, 44, u3k(p));
  u3m_sole_ream();
  if ( !u3a_sole_has(p) || u3a_sole_has(m) ) {
    fprintf(stderr, "test: sole set: ream kept the wrong members\r\n");
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
        m = u3m_sole_try(u3nc(45, 46));
      }
      else {
        m = u3m_sole_try(u3nc(45, 46));
        n = u3nc(45, 46);
      }

      a = u3nc(u3k(m), 1);
      b = u3nc(u3k(n), 1);
      (void)u3r_sing(a, b);
      if ( (u3h(a) != m) || (u3h(b) != m) ) {
        fprintf(stderr, "test: sole set: unified away from the member (%" PRIc3_w ")\r\n", i_w);
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

/* _test_sole_dag(): canonicalizing a shared noun on an inner road takes time
**                    in its distinct cells, not its leaves (2^26 of them:
**                    a walk of them takes seconds). junior cells are
**                    canonicalized, each once; senior cells that aren't
**                    members are left as they are.
*/
static c3_i
_test_sole_dag(void)
{
  c3_i ret_i = 1;

  //  a junior noun, with an equal member for the whole of it, or a part
  //
  for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
    struct timespec bef_u;
    u3_noun can, fre, r;
    c3_d    mic_d;

    can = u3m_sole_try(_dag(i_w ? 13 : 26));

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    fre = _dag(26);
    r = u3m_sole_try(u3k(fre));
    if ( !i_w && (r != can) ) {
      fprintf(stderr, "test: sole dag: member not found\r\n");
      ret_i = 0;
    }
    if ( c3n == u3r_sing(r, fre) ) {
      fprintf(stderr, "test: sole dag: changed the value\r\n");
      ret_i = 0;
    }
    u3z(r); u3z(fre);
    u3m_love(0);

    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: sole dag: %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
    u3z(can);
  }

  //  a junior noun with no equal member: the visited set keeps it
  //  linear, at full size and when it must evict
  //
  for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
    struct timespec bef_u;
    u3_noun fre, r;
    c3_d    mic_d;

    if ( i_w ) u3m_sole_visit_size(4);

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    fre = _dag_on(26, 9, 10);
    r = u3m_sole_try(u3k(fre));
    if ( r != fre ) {
      fprintf(stderr, "test: sole dag: fresh junior noun replaced\r\n");
      ret_i = 0;
    }
    u3z(r); u3z(fre);
    u3m_love(0);

    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: sole dag: unvisited %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
  }
  u3m_sole_visit_size(14);

  //  a home noun that isn't a member, or an intermediate road's, is left
  //  as it is, though an equal member exists: the exit to home
  //  canonicalizes what's promoted. likewise while debugging memory,
  //  where a sweep of the intermediate road finds no leaks
  //
  for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
    c3_h    wag_h = u3C.wag_h;
    struct timespec bef_u;
    u3_noun can, fre, r;
    c3_d    mic_d;

    can = u3m_sole_try(_dag(26));
    if ( i_w ) u3m_hate(0);
    fre = _dag(26);

    if ( 2 == i_w ) u3C.wag_h |= u3o_debug_ram;
    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    r = u3m_sole_try(u3k(fre));
    if ( (r != fre) || (u3h(fre) == u3h(can)) ) {
      fprintf(stderr, "test: sole dag: senior noun %u rewritten\r\n", (unsigned)i_w);
      ret_i = 0;
    }
    u3z(r);
    u3m_love(0);

    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: sole dag: senior %u %" PRIu64 " us\r\n", (unsigned)i_w, mic_d);
      ret_i = 0;
    }

    if ( 2 == i_w ) {
      u3C.wag_h |= u3o_leak_crash;
      u3m_grab(fre);
      u3C.wag_h = wag_h;
    }
    u3z(fre);
    if ( i_w ) u3m_love(0);
    u3z(can);
  }

  return ret_i;
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

/* _test_sing_dag_all(): every setting, at the current table size.
*/
static c3_i
_test_sing_dag_all(void)
{
  c3_i    ret_i = 1;
  u3_noun a, b;

  //  home road: unification merges what it compares
  //
  a = _dag(200); b = _dag(200);
  ret_i &= _sing_dag("home", a, b);
  u3z(a); u3z(b);

  //  inner road, two home nouns: the pair table
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
  //  copy: unification gives up at each junior-home boundary
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

  //  the same while debugging memory, which unifies on this road only:
  //  the pair table, for the one comparison
  //
  {
    c3_h wag_h = u3C.wag_h;

    u3C.wag_h |= u3o_debug_ram;
    u3m_hate(0);
    a = _dag(200); b = _dag(200);
    u3m_hate(0);
    ret_i &= _sing_dag("nested, debug", a, b);
    u3m_love(0);
    u3m_love(0);
    u3C.wag_h = wag_h;
  }

  //  as while promoting a junior road ([kid_p] set): this road unifies
  //  too, losing old values at the end of the exit (here, by hand)
  //
  a = _dag(200); b = _dag(200);
  u3R->kid_p = u3of(u3a_road, u3R);
  ret_i &= _sing_dag("promotion", a, b);
  u3R->kid_p = 0;
  u3a_wed_drain();
  if ( u3h(a) != u3h(b) ) {
    fprintf(stderr, "test: sing dag: promotion: not unified\r\n");
    ret_i = 0;
  }
  u3z(a); u3z(b);

  return ret_i;
}

/* _test_sing_dag(): long comparisons are bounded everywhere, and with a
**   table small enough to evict.
*/
static c3_i
_test_sing_dag(void)
{
  c3_i ret_i = 1;

  if ( !_test_sing_dag_all() ) {
    ret_i = 0;
  }

  return ret_i;
}

/* _test_sole_lazy(): u3m_sole_home() canonicalizes on the home road, and
**                    leaves inner roads' nouns to the exit home.
*/
static c3_i
_test_sole_lazy(void)
{
  c3_i    ret_i = 1;
  u3_noun can = u3m_sole_home(u3nt(7, 8, 9));
  u3_noun som;

  if ( !u3a_sole_has(can) ) {
    fprintf(stderr, "test: sole lazy: home not canonical\r\n");
    ret_i = 0;
  }

  u3m_hate(0);
  som = u3nt(7, 8, 9);
  {
    u3_noun pro = u3m_sole_home(u3k(som));

    if ( pro != som ) {
      fprintf(stderr, "test: sole lazy: inner road canonicalized\r\n");
      ret_i = 0;
    }
    u3z(pro);
  }
  {
    u3_noun pro = u3m_sole_try(u3k(som));

    if ( pro != can ) {
      fprintf(stderr, "test: sole lazy: try missed the member\r\n");
      ret_i = 0;
    }
    u3z(pro);
  }
  u3z(som);
  u3m_love(0);

  u3z(can);
  return ret_i;
}

/* _gap(): x_{k+1} = [x_k [w_k x_k]], w_k a fresh list of [len_w] atoms:
**   each shared x_k is met again only after w_k's cells, so a table that
**   evicts loses it.
*/
static u3_noun
_gap(c3_w dep_w, c3_w len_w)
{
  u3_noun x = u3nc(1, 2);
  for ( c3_w i_w = 0; i_w < dep_w; i_w++ ) {
    u3_noun w = 0;
    for ( c3_w j_w = 0; j_w < len_w; j_w++ ) {
      w = u3nc(100 + (i_w * len_w) + j_w, w);
    }
    x = u3nc(u3k(x), u3nc(w, x));
  }
  return x;
}

/* _test_gap(): the pair table and the visited set grow instead of
**              evicting, from 16 entries, so the worst case for an
**              evicting table stays linear; then they shrink back.
*/
static c3_i
_test_gap(void)
{
  c3_i ret_i = 1;
  struct timespec bef_u;
  c3_d mic_d;

  u3m_sole_visit_size(4);

  //  a comparison of two home copies on an inner road, linear with no
  //  table: unification writes home slots
  //
  {
    u3_noun a = _gap(40, 64), b = _gap(40, 64);

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    if ( c3y != u3r_sing(a, b) ) {
      fprintf(stderr, "test: gap: sing: not equal\r\n");
      ret_i = 0;
    }
    u3m_love(0);
    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: gap: sing %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
    u3z(a); u3z(b);
  }

  //  an eager walk of a junior noun
  //
  {
    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    u3z(u3m_sole_try(_gap(40, 64)));
    u3m_love(0);
    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: gap: try %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
  }

  //  the visited set grew, and shrinks back over a few quiet excursions
  //
  {
    c3_d vcp_d = u3m_sole_visit_cap();

    if ( vcp_d <= 16 ) {
      fprintf(stderr, "test: gap: visited set didn't grow (%" PRIu64 ")\r\n",
              vcp_d);
      ret_i = 0;
    }
    for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
      u3m_hate(0);
      u3m_love(0);
    }
    if ( 16 != u3m_sole_visit_cap() ) {
      fprintf(stderr, "test: gap: didn't shrink (%" PRIu64 ")\r\n",
              u3m_sole_visit_cap());
      ret_i = 0;
    }
  }

  u3m_sole_visit_size(14);
  return ret_i;
}

/* _test_crib(): a crib set grows, evicts and reports at its ceiling,
**              clears, and shrinks back; a crib array grows to its
**              ceiling, and drops past it.
*/
static c3_i
_test_crib(void)
{
  c3_i    ret_i = 1;
  u3c_set set_u = {0};
  u3c_arr arr_u = {0};
  c3_w    i_w, mis_w;

  u3c_set_init(&set_u, "test set", 4, 10);

  for ( i_w = 0; i_w < 300; i_w++ ) {
    u3c_set_put(&set_u, i_w + 2);
  }
  for ( i_w = mis_w = 0; i_w < 300; i_w++ ) {
    if ( !u3c_set_has(&set_u, i_w + 2) ) mis_w++;
  }
  if ( mis_w || set_u.ovf_d || (u3c_set_cap(&set_u) < 512) ) {
    fprintf(stderr, "test: crib: growth: %u missing, %" PRIu64 " evicted, %" PRIu64 " cap\r\n",
            (unsigned)mis_w, set_u.ovf_d, u3c_set_cap(&set_u));
    ret_i = 0;
  }

  //  past the ceiling, entries are evicted, and counted
  //
  for ( i_w = 300; i_w < 3000; i_w++ ) {
    u3c_set_put(&set_u, i_w + 2);
  }
  if ( (1024 != u3c_set_cap(&set_u)) || !set_u.ovf_d ) {
    fprintf(stderr, "test: crib: ceiling: %" PRIu64 " cap, %" PRIu64 " evicted\r\n",
            u3c_set_cap(&set_u), set_u.ovf_d);
    ret_i = 0;
  }

  //  a clear drops everything
  //
  u3c_set_clear(&set_u);
  for ( i_w = mis_w = 0; i_w < 3000; i_w++ ) {
    if ( u3c_set_has(&set_u, i_w + 2) ) mis_w++;
  }
  if ( mis_w ) {
    fprintf(stderr, "test: crib: clear: %u left\r\n", (unsigned)mis_w);
    ret_i = 0;
  }

  //  the report logs the evictions once; shrinking gives back memory as
  //  the peak decays
  //
  u3c_set_report(&set_u);
  for ( i_w = 0; i_w < 16; i_w++ ) {
    u3c_set_shrink(&set_u);
  }
  if ( set_u.ovf_d || (16 != u3c_set_cap(&set_u)) ) {
    fprintf(stderr, "test: crib: shrink: %" PRIu64 " cap, %" PRIu64 " evicted\r\n",
            u3c_set_cap(&set_u), set_u.ovf_d);
    ret_i = 0;
  }

  //  and it grows again from there, clean
  //
  for ( i_w = 0; i_w < 300; i_w++ ) {
    u3c_set_put(&set_u, i_w + 5000);
  }
  for ( i_w = mis_w = 0; i_w < 300; i_w++ ) {
    if ( !u3c_set_has(&set_u, i_w + 5000) ) mis_w++;
    if ( u3c_set_has(&set_u, i_w + 2) ) mis_w++;
  }
  if ( mis_w ) {
    fprintf(stderr, "test: crib: regrowth: %u wrong\r\n", (unsigned)mis_w);
    ret_i = 0;
  }

  //  arrays
  //
  u3c_arr_init(&arr_u, "test array", sizeof(c3_d), 4, 100);
  for ( i_w = 0; i_w < 100; i_w++ ) {
    c3_d* d_d = u3c_arr_at(&arr_u, i_w);
    if ( !d_d ) { ret_i = 0; break; }
    *d_d = i_w;
  }
  if ( u3c_arr_at(&arr_u, 100) || (1 != arr_u.drp_d) ) {
    fprintf(stderr, "test: crib: array ceiling\r\n");
    ret_i = 0;
  }
  for ( i_w = 0; i_w < 100; i_w++ ) {
    if ( *(c3_d*)u3c_arr_at(&arr_u, i_w) != i_w ) { ret_i = 0; break; }
  }
  u3c_arr_report(&arr_u);
  if ( arr_u.drp_d ) {
    fprintf(stderr, "test: crib: array report\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _ts_same(): the compact table's member is [*ptr_v].
*/
static c3_t
_ts_same(u3_noun key, void* ptr_v)
{
  return key == *(u3_noun*)ptr_v;
}

/* _ts_check(): every bucket's distance is from its member's home, in
**              Robin Hood order, and the count is right.
*/
static c3_i
_ts_check(_cs_tab* tab_u)
{
  c3_d len_d = 0;

  for ( c3_d i_d = 0; i_d < tab_u->cap_d; i_d++ ) {
    c3_h key_h = *_cs_key(tab_u, i_d);

    if ( key_h ) {
      u3_noun som   = _cs_noun(tab_u, key_h);
      c3_d    hom_d = _cs_home(tab_u, u3r_mug(som));
      c3_d    dis_d = ( i_d >= hom_d ) ? i_d - hom_d : i_d + tab_u->cap_d - hom_d;

      if ( dis_d != *_cs_dis(tab_u, i_d) ) {
        fprintf(stderr, "test: soletab: bucket %" PRIu64 " distance %u, not %" PRIu64 "\r\n",
                i_d, (unsigned)*_cs_dis(tab_u, i_d), dis_d);
        return 0;
      }
      len_d++;
    }
  }
  if ( len_d != tab_u->len_d ) {
    fprintf(stderr, "test: soletab: %" PRIu64 " members, counted %" PRIu64 "\r\n",
            tab_u->len_d, len_d);
    return 0;
  }
  return 1;
}

/* _test_soletab(): the compact table against a reference: random adds,
**                  erases and lookups, growing at the limit and between
**                  "events", then shrinking.
*/
static c3_i
_test_soletab(void)
{
  c3_i    ret_i = 1;
  c3_w    num_w = 6000, i_w, k_w;
  u3_noun cel[6000];
  c3_t    has_t[6000] = {0};
  c3_d    len_d = 0;
  _cs_tab tab_u;

  //  keys round-trip, for cells and atoms
  //
  {
    u3_noun c = u3nc(1, 2);
    c3_d    big_d[2] = { 0x123456789abcdefULL, 0xfedcba987654321ULL };
    u3_noun a = u3i_chubs(2, big_d);

    _cs_tab vat_u = { .cel_t = 0 }, cet_u = { .cel_t = 1 };
    if (  (c != _cs_noun(&cet_u, _cs_slot(c)))
       || (a != _cs_noun(&vat_u, _cs_slot(a))) )
    {
      fprintf(stderr, "test: soletab: keys don't round-trip\r\n");
      ret_i = 0;
    }
    u3z(c); u3z(a);
  }

  for ( i_w = 0; i_w < num_w; i_w++ ) {
    cel[i_w] = u3nc(i_w + 1, 7);
    (void)u3r_mug(cel[i_w]);
  }

  _cs_init(&tab_u, 1);
  srand(1);

  for ( k_w = 0; ret_i && (k_w < 300000); k_w++ ) {
    i_w = (c3_w)rand() % num_w;

    switch ( rand() % 3 ) {
      case 0: {
        if ( !has_t[i_w] ) {
          if ( k_w & 1 ) {
            _cs_add(&tab_u, cel[i_w]);
          }
          else {
            c3_d pos_d;
            if (  (cel[i_w] != _cs_put(&tab_u, cel[i_w], u3r_mug(cel[i_w]), _ts_same, &pos_d))
               || (~0ULL == pos_d) )
            {
              fprintf(stderr, "test: soletab: put of %u wrong\r\n", (unsigned)i_w);
              ret_i = 0;
            }
          }
          has_t[i_w] = 1;
          len_d++;
        }
        else {
          c3_d pos_d;
          if (  (cel[i_w] != _cs_put(&tab_u, cel[i_w], u3r_mug(cel[i_w]), _ts_same, &pos_d))
             || (~0ULL != pos_d) )
          {
            fprintf(stderr, "test: soletab: put of present %u wrong\r\n", (unsigned)i_w);
            ret_i = 0;
          }
        }
      } break;

      case 1: {
        c3_d pos_d = _cs_erase(&tab_u, cel[i_w]);

        if ( has_t[i_w] != (~0ULL != pos_d) ) {
          fprintf(stderr, "test: soletab: erase of %u wrong\r\n", (unsigned)i_w);
          ret_i = 0;
        }
        if ( has_t[i_w] ) len_d--;
        has_t[i_w] = 0;
      } break;

      default: {
        u3_weak got = _cs_get(&tab_u, u3r_mug(cel[i_w]), _ts_same, &cel[i_w]);

        if ( has_t[i_w] != (u3_none != got) ) {
          fprintf(stderr, "test: soletab: lookup of %u wrong\r\n", (unsigned)i_w);
          ret_i = 0;
        }
      } break;
    }

    if ( len_d != tab_u.len_d ) {
      fprintf(stderr, "test: soletab: length %" PRIu64 ", not %" PRIu64 "\r\n",
              tab_u.len_d, len_d);
      ret_i = 0;
    }

    //  an "event" boundary now and then
    //
    if ( 0 == (k_w % 5000) ) {
      _cs_tend(&tab_u);
      ret_i &= _ts_check(&tab_u);
    }
  }

  //  erase most, then shrink between events
  //
  {
    c3_d lin_d;

    for ( i_w = 0; i_w < num_w; i_w++ ) {
      if ( has_t[i_w] && (i_w % 16) ) {
        _cs_erase(&tab_u, cel[i_w]);
        has_t[i_w] = 0;
      }
    }
    lin_d = tab_u.lin_d;
    _cs_tend(&tab_u);
    if ( tab_u.lin_d >= lin_d ) {
      fprintf(stderr, "test: soletab: didn't shrink (%" PRIu64 " lines)\r\n", lin_d);
      ret_i = 0;
    }
    ret_i &= _ts_check(&tab_u);
    for ( i_w = 0; i_w < num_w; i_w++ ) {
      u3_weak got = _cs_get(&tab_u, u3r_mug(cel[i_w]), _ts_same, &cel[i_w]);
      if ( has_t[i_w] != (u3_none != got) ) {
        fprintf(stderr, "test: soletab: after shrinking, %u wrong\r\n", (unsigned)i_w);
        ret_i = 0;
        break;
      }
    }
  }

  if ( !_cs_met.gro_d[0] || !_cs_met.gro_d[1] ) {
    fprintf(stderr, "test: soletab: resizes %" PRIu64 " between, %" PRIu64 " at the limit\r\n",
            _cs_met.gro_d[0], _cs_met.gro_d[1]);
    ret_i = 0;
  }

  c3_free(tab_u.lin_u);
  for ( i_w = 0; i_w < num_w; i_w++ ) u3z(cel[i_w]);
  return ret_i;
}

static c3_i
_test_equality(void)
{
  c3_i ret_i = 1;

  if ( !_test_sole_set() ) {
    fprintf(stderr, "test: equality: sole set: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sole_lazy() ) {
    fprintf(stderr, "test: equality: sole lazy: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_soletab() ) {
    fprintf(stderr, "test: equality: soletab: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_crib() ) {
    fprintf(stderr, "test: equality: crib: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_gap() ) {
    fprintf(stderr, "test: equality: gap: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sing_dag() ) {
    fprintf(stderr, "test: equality: sing dag: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sole_dag() ) {
    fprintf(stderr, "test: equality: sole dag: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sole() ) {
    fprintf(stderr, "test: equality: sole: failed\r\n");
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

  if ( !_test_lose_full() ) {
    fprintf(stderr, "test: equality: lose full: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_lose_full_eager() ) {
    fprintf(stderr, "test: equality: lose full eager: failed\r\n");
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
