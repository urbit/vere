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
  r = u3m_dedup_eager(u3k(t));
  if ( r != t ) {
    fprintf(stderr, "test: dedup: fresh noun copied\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  an equal noun dedups to it
  //
  b = u3nc(2, 3);
  r = u3m_dedup_eager(u3k(b));
  if ( r != t ) {
    fprintf(stderr, "test: dedup: equal noun not shared\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  a cell with a non-canonical child is repointed in place
  //
  c = u3nc(u3nc(2, 3), 4);
  r = u3m_dedup_eager(u3k(c));
  if ( (r != c) || (u3h(c) != t) ) {
    fprintf(stderr, "test: dedup: non-canonical child not repointed\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  a cell whose children are canonical is kept
  //
  d = u3nc(u3k(t), 5);
  r = u3m_dedup_eager(u3k(d));
  if ( r != d ) {
    fprintf(stderr, "test: dedup: canonical children copied\r\n");
    ret_i = 0;
  }
  u3z(r);

  //  an inner road keeps a senior noun, rather than copying it
  //
  s = u3nc(6, 7);
  u3m_hate(0);
  r = u3m_dedup_eager(u3k(s));
  if ( r != s ) {
    fprintf(stderr, "test: dedup: senior noun copied\r\n");
    ret_i = 0;
  }
  u3z(r);
  u3m_love(0);

  //  an inner road leaves a senior cell with a non-canonical child as
  //  it is, rather than copying it
  //
  e = u3nc(u3nc(2, 3), 8);
  u3m_hate(0);
  r = u3m_dedup_eager(u3k(e));
  if ( (r != e) || (u3h(e) == t) ) {
    fprintf(stderr, "test: dedup: senior cell copied or rewritten\r\n");
    ret_i = 0;
  }
  u3z(r);
  u3m_love(0);

  //  but canonicalizes a junior cell's children, as on the home road
  //
  u3m_hate(0);
  e = u3nc(u3nc(2, 3), 8);
  r = u3m_dedup_eager(u3k(e));
  if ( (r != e) || (u3h(e) != t) ) {
    fprintf(stderr, "test: dedup: junior child not repointed\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(e);
  u3m_love(0);
  e = 0;

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
  x = u3m_dedup_eager(u3nc(10, 11));
  y = u3nc(10, 11);
  u3z(x);
  r = u3m_dedup_eager(u3k(y));
  if ( r != y ) {
    fprintf(stderr, "test: hcons: freed member still found\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(y);

  //  so do members freed in a cascade
  //
  x = u3m_dedup_eager(u3nc(u3nc(12, 13), 14));
  y = u3nc(u3nc(12, 13), 14);
  u3z(x);
  r = u3m_dedup_eager(u3k(y));
  if ( (r != y) || !u3a_hcon_has(u3h(y)) ) {
    fprintf(stderr, "test: hcons: cascade-freed member still found\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(y);

  //  junior entries that are taken join the home set
  //
  u3m_hate(0);
  p = u3m_dedup_eager(u3nc(u3nc(20, 21), 22));
  r = u3m_dedup_eager(u3nc(30, 31));     //  not taken
  u3z(r);
  p = u3m_love(p);
  if ( !u3a_hcon_has(p) || !u3a_hcon_has(u3h(p)) ) {
    fprintf(stderr, "test: hcons: taken entry not a member\r\n");
    ret_i = 0;
  }
  q = u3nc(u3nc(20, 21), 22);
  r = u3m_dedup_eager(u3k(q));
  if ( r != p ) {
    fprintf(stderr, "test: hcons: taken entry not shared\r\n");
    ret_i = 0;
  }
  u3z(r); u3z(q);
  q = u3nc(30, 31);
  r = u3m_dedup_eager(u3k(q));
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
  m = u3m_dedup_eager(u3nc(23, 24));
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
  p = u3m_dedup_eager(u3nc(40, 41));
  m = u3m_dedup_eager(u3nc(42, 43));
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
        m = u3m_dedup_eager(u3nc(45, 46));
      }
      else {
        m = u3m_dedup_eager(u3nc(45, 46));
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

/* _test_dedup_dag(): deduping a shared noun on an inner road takes time
**                    in its distinct cells, not its leaves (2^26 of them:
**                    a walk of them takes seconds). junior cells are
**                    canonicalized, each once; senior cells that aren't
**                    members are left as they are.
*/
static c3_i
_test_dedup_dag(void)
{
  c3_i ret_i = 1;

  //  a junior noun, with an equal member for the whole of it, or a part
  //
  for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
    struct timespec bef_u;
    u3_noun can, fre, r;
    c3_d    mic_d;

    can = u3m_dedup_eager(_dag(i_w ? 13 : 26));

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    fre = _dag(26);
    r = u3m_dedup_eager(u3k(fre));
    if ( !i_w && (r != can) ) {
      fprintf(stderr, "test: dedup dag: member not found\r\n");
      ret_i = 0;
    }
    if ( c3n == u3r_sing(r, fre) ) {
      fprintf(stderr, "test: dedup dag: changed the value\r\n");
      ret_i = 0;
    }
    u3z(r); u3z(fre);
    u3m_love(0);

    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: dedup dag: %" PRIu64 " us\r\n", mic_d);
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

    if ( i_w ) u3m_dedup_visit_size(4);

    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    fre = _dag_on(26, 9, 10);
    r = u3m_dedup_eager(u3k(fre));
    if ( r != fre ) {
      fprintf(stderr, "test: dedup dag: fresh junior noun replaced\r\n");
      ret_i = 0;
    }
    u3z(r); u3z(fre);
    u3m_love(0);

    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: dedup dag: unvisited %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
  }
  u3m_dedup_visit_size(14);

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

    can = u3m_dedup_eager(_dag(26));
    if ( i_w ) u3m_hate(0);
    fre = _dag(26);

    if ( 2 == i_w ) u3C.wag_h |= u3o_debug_ram | u3o_leak_crash;
    clock_gettime(CLOCK_MONOTONIC, &bef_u);
    u3m_hate(0);
    r = u3m_dedup_eager(u3k(fre));
    if ( (r != fre) || (u3h(fre) == u3h(can)) ) {
      fprintf(stderr, "test: dedup dag: senior noun %u rewritten\r\n", (unsigned)i_w);
      ret_i = 0;
    }
    u3z(r);
    u3m_love(0);

    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: dedup dag: senior %u %" PRIu64 " us\r\n", (unsigned)i_w, mic_d);
      ret_i = 0;
    }

    if ( 2 == i_w ) {
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

  //  as while promoting a junior road ([kid_p] set), when nothing is
  //  unified: the pair table, for the one comparison
  //
  a = _dag(200); b = _dag(200);
  u3R->kid_p = u3of(u3a_road, u3R);
  ret_i &= _sing_dag("promotion", a, b);
  u3R->kid_p = 0;
  if ( u3h(a) == u3h(b) ) {
    fprintf(stderr, "test: sing dag: promotion: unified\r\n");
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

  u3r_pair_size(4);
  if ( !_test_sing_dag_all() ) {
    fprintf(stderr, "test: sing dag: small table: failed\r\n");
    ret_i = 0;
  }

  u3r_pair_size(14);
  return ret_i;
}

/* _test_dedup_lazy(): u3m_dedup() canonicalizes on the home road, and
**                    leaves inner roads' nouns to the exit home.
*/
static c3_i
_test_dedup_lazy(void)
{
  c3_i    ret_i = 1;
  u3_noun can = u3m_dedup(u3nt(7, 8, 9));
  u3_noun som;

  if ( !u3a_hcon_has(can) ) {
    fprintf(stderr, "test: dedup lazy: home not canonical\r\n");
    ret_i = 0;
  }

  u3m_hate(0);
  som = u3nt(7, 8, 9);
  {
    u3_noun pro = u3m_dedup(u3k(som));

    if ( pro != som ) {
      fprintf(stderr, "test: dedup lazy: inner road deduplicated\r\n");
      ret_i = 0;
    }
    u3z(pro);
  }
  {
    u3_noun pro = u3m_dedup_eager(u3k(som));

    if ( pro != can ) {
      fprintf(stderr, "test: dedup lazy: eager missed the member\r\n");
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

  u3r_pair_size(4);
  u3m_dedup_visit_size(4);

  //  a comparison of two home copies on an inner road
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
    u3z(u3m_dedup_eager(_gap(40, 64)));
    u3m_love(0);
    if ( (mic_d = _us_since(&bef_u)) > 1000000 ) {
      fprintf(stderr, "test: gap: dedup %" PRIu64 " us\r\n", mic_d);
      ret_i = 0;
    }
  }

  //  both grew, and shrink back over a few quiet excursions
  //
  {
    c3_d pcp_d = u3r_pair_cap(), vcp_d = u3m_dedup_visit_cap();

    if ( (pcp_d <= 16) || (vcp_d <= 16) ) {
      fprintf(stderr, "test: gap: didn't grow (%" PRIu64 ", %" PRIu64 ")\r\n",
              pcp_d, vcp_d);
      ret_i = 0;
    }
    for ( c3_w i_w = 0; i_w < 16; i_w++ ) {
      u3m_hate(0);
      u3m_love(0);
    }
    if ( (16 != u3r_pair_cap()) || (16 != u3m_dedup_visit_cap()) ) {
      fprintf(stderr, "test: gap: didn't shrink (%" PRIu64 ", %" PRIu64 ")\r\n",
              u3r_pair_cap(), u3m_dedup_visit_cap());
      ret_i = 0;
    }
  }

  u3r_pair_size(14);
  u3m_dedup_visit_size(14);
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

  if ( !_test_dedup_lazy() ) {
    fprintf(stderr, "test: equality: dedup lazy: failed\r\n");
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
