/// @file
///
/// Interrupts at random points: a test of inner-road virtualization.
///
/// Runs Nock workloads under u3m_soft() with random millisecond timeouts,
/// so timer signals unwind them at arbitrary instructions, and every live
/// road is promoted from wherever it stopped. Then checks that each result
/// that completes matches an uninterrupted run, that nothing on the home
/// road leaked (u3o_leak_crash), and that the state still packs and
/// computes the same results.
///
/// SOFT_TEST_N sets the number of interrupted runs (default 120), and
/// SOFT_TEST_SEED the random seed (default: the time).
///
/// The canonical ivory pill leaves no equal nouns at different addresses
/// on the home road. SOFT_TEST_DUP=1 adds some: copies of the slam
/// workloads, made with jam and cue (and made again every 20 interrupted
/// runs), and two deep nouns with 2^200 paths. SOFT_TEST_RAW=1 boots
/// from the pill without canonicalizing it first. SOFT_TEST_LOOM sets
/// the loom size in bits of bytes (default 27). SOFT_TEST_TINY=1 starts
/// the lose lists and the visited set at 16 entries, so that they grow
/// under interrupts.
///
/// Then an alarm raised while a new inner road's tables are half built
/// (u3m_Pave_f), at each of the first leaps of a workload with nested
/// roads: the run must end with %alrm or complete, and the state must
/// grab cleanly and compute the same result.
///
/// Also, an exit to home after a bail runs in the outer signal regime,
/// with the timer disarmed, as after success (u3m_Love_f).
///
/// First, a stack overflow while async signals are held (u3m_hold())
/// must leave u3o_check_corrupt set, and the grab it asks for must repair
/// what a cut-short home write left: a leaked old value, or a new value
/// gained but not written.

#include "noun.h"
#include "ivory.h"
#include "ur/ur.h"
#include "vere.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
  const c3_c* nam_c;            //  name
  u3_funk     fun_f;            //  runs on the soft's road, takes [arg]
  u3_noun     arg;              //  argument, held here
  c3_w        mug_w;            //  mug of the uninterrupted product
  c3_d        dur_d;            //  uninterrupted run, in microseconds
  c3_d        run_d;            //  runs
  c3_d        don_d;            //  ... that completed
  c3_d        int_d;            //  ... that a timeout interrupted
} _work;

static u3_noun _pil;            //  the ivory pill

/* _cue_pill(): the ivory pill.
*/
static u3_noun
_cue_pill(void)
{
  u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  u3_weak      pil   = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len,
                                         u3_Ivory_pill);
  u3s_cue_xeno_done(sil_u);

  if ( u3_none == pil ) {
    fprintf(stderr, "soft: cue ivory failed\r\n");
    exit(1);
  }
  return pil;
}

/* _setup(): boot a kernel from the ivory pill.
*/
static void
_setup(void)
{
  c3_c* lom_c = getenv("SOFT_TEST_LOOM");
  c3_w  lom_w = lom_c ? (c3_w)atoi(lom_c) : 27;

  u3m_boot_lite((size_t)1 << lom_w);
  if ( getenv("SOFT_TEST_RAW") ) {
    u3v_Ivory_canon_t = 0;
  }
  if ( getenv("SOFT_TEST_TINY") ) {
    u3a_wed_size(16, 1U << 22);
    u3m_sole_visit_size(4);
  }
  _pil = _cue_pill();
  if ( c3n == u3v_boot_lite(u3k(_pil)) ) {
    fprintf(stderr, "soft: boot failed\r\n");
    exit(1);
  }
}

/* _now(): microseconds, monotonic.
*/
static c3_d
_now(void)
{
  struct timespec tim_u;
  clock_gettime(CLOCK_MONOTONIC, &tim_u);
  return (c3_d)tim_u.tv_sec * 1000000ULL + (c3_d)tim_u.tv_nsec / 1000;
}

/* _w_life(): make a kernel from the ivory event, registering its cores
**            on the soft's road.
*/
static u3_noun
_w_life(u3_noun eve)
{
  return u3v_life(eve);
}

/* _w_slam(): slam [gat sam].
*/
static u3_noun
_w_slam(u3_noun arg)
{
  u3_noun gat = u3k(u3h(arg));
  u3_noun sam = u3k(u3t(arg));
  u3z(arg);
  return u3n_slam_on(gat, sam);
}

/* _w_wish(): compile and run hoon text, uncached off the home road.
*/
static u3_noun
_w_wish(u3_noun txt)
{
  return u3v_wish_w(txt);
}

/* _slam_of(): a slam workload of the gate [gat_c] on [sam].
*/
static _work
_slam_of(const c3_c* nam_c, const c3_c* gat_c, u3_noun sam)
{
  _work wok_u = { nam_c, _w_slam, u3nc(u3v_wish(gat_c), sam) };
  return wok_u;
}

/* _run(): run [wok_u] under a timeout of [mil_w] ms (0 for none).
**         produces 1 if it completed, 0 if interrupted; fails on any
**         other outcome, or a product that differs.
*/
static c3_t
_run(_work* wok_u, c3_w mil_w, c3_t ref_t)
{
  u3_noun pro;
  c3_d    bef_d = _now();
  c3_t    don_t;

  pro = u3m_soft(mil_w, wok_u->fun_f, u3k(wok_u->arg));
  wok_u->run_d++;

  if ( 0 == u3h(pro) ) {
    c3_w mug_w = u3r_mug(u3t(pro));

    if ( ref_t ) {
      wok_u->mug_w = mug_w;
      wok_u->dur_d = _now() - bef_d;
    }
    else if ( mug_w != wok_u->mug_w ) {
      fprintf(stderr, "soft: %s: product %" PRIxc3_w ", expected %" PRIxc3_w "\r\n",
              wok_u->nam_c, mug_w, wok_u->mug_w);
      exit(1);
    }
    wok_u->don_d++;
    don_t = 1;
  }
  else if ( !ref_t && (c3__alrm == u3h(pro)) ) {
    wok_u->int_d++;
    don_t = 0;
  }
  else {
    fprintf(stderr, "soft: %s: failed with ", wok_u->nam_c);
    u3m_p("error", u3h(pro));
    exit(1);
  }

  u3z(pro);
  return don_t;
}

/* _grab(): collect garbage; with u3o_leak_crash, a leak fails the test.
*/
static void
_grab(_work* wok_u, c3_w len_w)
{
  u3_noun* rot = c3_malloc(sizeof(u3_noun) * (len_w + 1));

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    rot[i_w] = wok_u[i_w].arg;
  }
  rot[len_w] = _pil;
  u3m_vgrab(rot, len_w + 1);
  c3_free(rot);
}

/* _WORK: the most workloads; _len_w: how many there are.
*/
#define _WORK  10
static c3_w _len_w = 5;

/* _deep(): [n_w] levels of [d d]: 2^n_w paths, n_w cells.
*/
static u3_noun
_deep(c3_w n_w)
{
  u3_noun dep = 1;

  for ( c3_w i_w = 0; i_w < n_w; i_w++ ) {
    dep = u3nc(u3k(dep), dep);
  }
  return dep;
}

/* _w_deep_home(): compare two deep nouns, both home, sharing no cells.
*/
static u3_noun
_w_deep_home(u3_noun arg)
{
  c3_o equ_o = u3r_sing(u3h(arg), u3t(arg));
  u3z(arg);
  return equ_o;
}

/* _w_deep_junior(): compare a deep home noun with one made here.
*/
static u3_noun
_w_deep_junior(u3_noun arg)
{
  u3_noun dep   = _deep((c3_w)u3t(arg));
  c3_o    equ_o = u3r_sing(u3h(arg), dep);
  u3z(dep);
  u3z(arg);
  return equ_o;
}

/* _copy(): a copy of [som] with no cells in common with it.
*/
static u3_noun
_copy(u3_noun som)
{
  u3_atom jam = u3qe_jam(som);
  u3_noun cop = u3s_cue_atom(jam);
  u3z(jam);
  return cop;
}

/* _build(): (re)make the workloads' arguments, keeping their names,
**           products and counts.
*/
static void
_build(_work* wok_u)
{
  _work new_u[_WORK] = {
    { "life", _w_life, u3k(u3t(_pil)) },
    _slam_of("jets",
             "|=(n=@ (lent (turn (gulf 1 n) |=(a=@ (mul a a)))))",
             20000),
    _slam_of("mule",
             "|=(n=@ (mule |.((roll (turn (gulf 1 n) |=(a=@ (mug a))) add))))",
             20000),
    _slam_of("mint",
             "|=(t=@t (~(mint ut -:!>(..ream)) %noun (ream t)))",
             u3i_string("|=  [a=@ b=@]  ^-  @  ?:  =(0 a)  b  $(a (dec a), b +(b))")),
    { "wish", _w_wish,
      u3i_string("(lent (turn (gulf 1 3.000) |=(a=@ (cat 3 a a))))") },
  };

  for ( c3_w i_w = 0; i_w < 5; i_w++ ) {
    wok_u[i_w].nam_c = new_u[i_w].nam_c;
    wok_u[i_w].fun_f = new_u[i_w].fun_f;
    wok_u[i_w].arg   = new_u[i_w].arg;
  }

  //  copies of the slam workloads, equal to them but not shared
  //
  if ( 10 == _len_w ) {
    static const c3_c* nam_c[3] = { "jets/copy", "mule/copy", "mint/copy" };

    for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
      wok_u[5 + i_w].nam_c = nam_c[i_w];
      wok_u[5 + i_w].fun_f = _w_slam;
      wok_u[5 + i_w].arg   = _copy(wok_u[1 + i_w].arg);
    }

    //  2^200 paths: linear only if repeated pairs are recognized
    //
    wok_u[8].nam_c = "deep/home";
    wok_u[8].fun_f = _w_deep_home;
    wok_u[8].arg   = u3nc(_deep(200), _deep(200));
    wok_u[9].nam_c = "deep/junior";
    wok_u[9].fun_f = _w_deep_junior;
    wok_u[9].arg   = u3nc(_deep(200), 200);
  }
}

/* _recopy(): make the copies again, so that later runs find home nouns
**             that no exit has unified yet.
*/
static void
_recopy(_work* wok_u)
{
  if ( 10 == _len_w ) {
    for ( c3_w i_w = 0; i_w < 3; i_w++ ) {
      u3z(wok_u[5 + i_w].arg);
      wok_u[5 + i_w].arg = _copy(wok_u[1 + i_w].arg);
    }
  }
}

/* _drop(): release the nouns held here.
*/
static void
_drop(_work* wok_u)
{
  for ( c3_w i_w = 0; i_w < _len_w; i_w++ ) {
    u3z(wok_u[i_w].arg);
    wok_u[i_w].arg = 0;
  }
  u3z(_pil);
  _pil = 0;
}

/* _over_deep(): recurse until the C stack overflows.
*/
static volatile c3_w _over_stop_w;

static c3_w
_over_deep(c3_w dep_w)
{
  volatile c3_y buf_y[512];

  buf_y[0] = (c3_y)dep_w;
  if ( _over_stop_w == dep_w ) return buf_y[0];
  return _over_deep(dep_w + 1) + buf_y[0];
}

/* _w_over(): on the soft's road, with async signals held, do what a
**            home write cut short leaves: point the head of home cell
**            [arg] at its tail, gaining the tail, but don't release the
**            old head. then overflow.
*/
static u3_noun
_w_over(u3_noun arg)
{
  u3a_cell* cel_u = u3a_to_ptr(arg);
  u3_noun   tel   = cel_u->tel;

  u3m_hold();
  ((u3a_noun*)u3a_to_ptr(tel))->use_w += 1;
  cel_u->hed = tel;
  _over_stop_w = 0;
  u3z(arg);
  return _over_deep(1);
}

/* _w_over_gain(): as _w_over(), but cut short after the gain.
*/
static u3_noun
_w_over_gain(u3_noun arg)
{
  u3_noun tel = u3t(arg);

  u3m_hold();
  ((u3a_noun*)u3a_to_ptr(tel))->use_w += 1;
  _over_stop_w = 0;
  u3z(arg);
  return _over_deep(1);
}

/* _over_run(): run [fun_f] on [cel], expecting a held overflow.
*/
static void
_over_run(u3_funk fun_f, u3_noun cel)
{
  u3_noun pro;

  u3C.wag_h &= ~u3o_check_corrupt;
  pro = u3m_soft(0, fun_f, u3k(cel));

  if ( c3__over != u3h(pro) ) {
    fprintf(stderr, "soft: over: expected %%over\r\n");
    u3m_p("got", u3h(pro));
    exit(1);
  }
  u3z(pro);

  if ( !(u3C.wag_h & u3o_check_corrupt) ) {
    fprintf(stderr, "soft: over: u3o_check_corrupt not set\r\n");
    exit(1);
  }
}

/* _test_over(): a stack overflow with signals held asks for a grab, and
**               the grab repairs the leak, or the count left too high.
*/
static void
_test_over(void)
{
  //  [X Y], X and Y equal home cells, and only the pair holding X
  //
  u3_noun cel = u3nc(u3nc(1, 2), u3nc(1, 2));

  //  cut short after the write: the grab mars would make finds the
  //  orphaned X, and frees it
  //
  _over_run(_w_over, cel);
  u3m_grab_check(cel, _pil);
  if ( u3C.wag_h & u3o_check_corrupt ) {
    fprintf(stderr, "soft: over: u3o_check_corrupt not cleared\r\n");
    exit(1);
  }
  if ( !u3a_sweep_leaked() ) {
    fprintf(stderr, "soft: over: the grab found no leak\r\n");
    exit(1);
  }
  fprintf(stderr, "soft: over: grab repaired a leak of %" PRIc3_w " words\r\n",
          u3a_sweep_leaked());

  u3m_grab(cel, _pil);
  if ( u3a_sweep_leaked() ) {
    fprintf(stderr, "soft: over: still leaking after the grab\r\n");
    exit(1);
  }

  //  cut short after the gain: Y is held once, by [cel], but counted
  //  twice. counting references, the grab repairs it
  //
  {
    u3_noun tel = u3t(cel);
    c3_w    use_w = u3a_use(tel);

    _over_run(_w_over_gain, cel);
    if ( u3a_use(tel) != use_w + 1 ) {
      fprintf(stderr, "soft: over: gain not left (%" PRIc3_w ")\r\n", u3a_use(tel));
      exit(1);
    }
    u3m_grab_check(cel, _pil);
    if ( u3a_use(tel) != use_w ) {
      fprintf(stderr, "soft: over: count not repaired (%" PRIc3_w ", not %" PRIc3_w ")\r\n",
              u3a_use(tel), use_w);
      exit(1);
    }
    fprintf(stderr, "soft: over: grab repaired a count left at %" PRIc3_w "\r\n",
            use_w + 1);
  }
  u3z(cel);
}

/* the exits to home: whether each ran with the virtual timer armed, or
**   with a SIGTERM handler other than the outer one ([_love_out_u]).
*/
static c3_w             _love_cnt_w;
static c3_w             _love_arm_w;
static c3_w             _love_sig_w;
static struct sigaction _love_out_u;

static void
_love_hook(void)
{
  struct itimerval itm_u;
  struct sigaction act_u;

  _love_cnt_w++;
  if ( !getitimer(ITIMER_VIRTUAL, &itm_u) && timerisset(&itm_u.it_value) ) {
    _love_arm_w++;
  }
  if (  !sigaction(SIGTERM, 0, &act_u)
     && (act_u.sa_handler != _love_out_u.sa_handler) )
  {
    _love_sig_w++;
  }
}

static u3_noun
_love_bail(u3_noun arg)
{
  u3z(arg);
  return u3m_bail(c3__exit);
}

static u3_noun
_love_pass(u3_noun arg)
{
  return arg;
}

/* _test_love(): exits to home, after success and after a bail, with a
**   long timeout: neither may run with the timer armed, or with the
**   inner signal handlers.
*/
static void
_test_love(void)
{
  u3_noun pro;

  sigaction(SIGTERM, 0, &_love_out_u);
  u3m_Love_f = _love_hook;
  _love_cnt_w = _love_arm_w = _love_sig_w = 0;

  pro = u3m_soft(60000, _love_pass, u3nc(1, 2));
  if ( 0 != u3h(pro) ) {
    fprintf(stderr, "soft: love: expected success\r\n");
    exit(1);
  }
  u3z(pro);

  pro = u3m_soft(60000, _love_bail, u3nc(1, 2));
  if ( c3__exit != u3h(pro) ) {
    fprintf(stderr, "soft: love: expected %%exit\r\n");
    u3m_p("got", u3h(pro));
    exit(1);
  }
  u3z(pro);

  //  three exits: the bail's trace is rendered in a third u3m_soft()
  //
  u3m_Love_f = 0;
  if ( (3 != _love_cnt_w) || _love_arm_w || _love_sig_w ) {
    fprintf(stderr, "soft: love: %" PRIc3_w " exits, %" PRIc3_w " with the timer"
                    " armed, %" PRIc3_w " with inner handlers\r\n",
            _love_cnt_w, _love_arm_w, _love_sig_w);
    exit(1);
  }
  fprintf(stderr, "soft: love: %" PRIc3_w " exits home, all in the outer signal regime\r\n",
          _love_cnt_w);
}

/* the leap points: inner roads whose tables are being built, counted
**   while armed; the alarm is raised at the [_pave_at_w]th.
*/
static c3_t _pave_arm_t;
static c3_w _pave_cnt_w;
static c3_w _pave_at_w;

static void
_pave_hook(void)
{
  if ( !_pave_arm_t ) return;

  if ( ++_pave_cnt_w == _pave_at_w ) {
    _pave_arm_t = 0;
    raise(SIGVTALRM);
  }
}

/* _test_pave(): an alarm while each of the first few inner roads of
**   [w_u] builds its tables.
*/
static void
_test_pave(_work* wok_u, c3_w len_w, _work* w_u)
{
  c3_w tot_w, int_w = 0;

  u3m_Pave_f = _pave_hook;

  _pave_cnt_w = 0;
  _pave_at_w  = 0;
  _pave_arm_t = 1;
  _run(w_u, 0, 0);
  _pave_arm_t = 0;
  tot_w = c3_min(_pave_cnt_w, 8);

  for ( c3_w at_w = 1; at_w <= tot_w; at_w++ ) {
    _pave_cnt_w = 0;
    _pave_at_w  = at_w;
    _pave_arm_t = 1;
    if ( !_run(w_u, 0, 0) ) int_w++;
    _pave_arm_t = 0;
    _grab(wok_u, len_w);
    _run(w_u, 0, 0);
  }

  u3m_Pave_f = 0;
  fprintf(stderr, "soft: pave: %s: alarm in %" PRIc3_w " leaps, %" PRIc3_w
                  " interrupted\r\n", w_u->nam_c, tot_w, int_w);
}

int
main(int argc, char* argv[])
{
  c3_w  num_w = 120;
  c3_w  sed_w = (c3_w)time(0);
  c3_c* env_c;

  if ( (env_c = getenv("SOFT_TEST_N")) )    num_w = (c3_w)atoi(env_c);
  if ( (env_c = getenv("SOFT_TEST_SEED")) ) sed_w = (c3_w)atoi(env_c);
  if ( getenv("SOFT_TEST_DUP") )            _len_w = 10;

  _setup();
  srand(sed_w);
  fprintf(stderr, "soft: seed %" PRIc3_w ", %" PRIc3_w " interrupted runs\r\n",
          sed_w, num_w);

  _work wok_u[_WORK];
  c3_w  len_w = _len_w;

  memset(wok_u, 0, sizeof(wok_u));
  _test_over();
  _test_love();
  _build(wok_u);

  //  leaks fail the test from here on
  //
  u3C.wag_h |= u3o_leak_crash;
  _grab(wok_u, len_w);

  //  uninterrupted reference runs
  //
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    _run(&wok_u[i_w], 0, 1);
    fprintf(stderr, "soft: %s: %" PRIxc3_w " in %" PRIu64 " us\r\n",
            wok_u[i_w].nam_c, wok_u[i_w].mug_w, wok_u[i_w].dur_d);
  }

  //  an alarm while an inner road's tables are half built, in the
  //  workload with nested roads
  //
  _test_pave(wok_u, len_w, &wok_u[2]);

  //  interrupted runs, each with a timeout up to a bit past its duration
  //
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    _work* w_u   = &wok_u[rand() % len_w];
    c3_w   top_w = (c3_w)(w_u->dur_d / 1000) + 2;
    c3_w   mil_w = 1 + (rand() % (top_w + (top_w / 4)));

    _run(w_u, mil_w, 0);
    if ( 0 == ((i_w + 1) % 20) ) {
      _recopy(wok_u);
      _grab(wok_u, len_w);
    }
  }

  //  the state still packs, and computes the same results. a pack moves
  //  boxes, and rewrites only the runtime's roots, so the nouns held here
  //  go first and are made again after
  //
  _grab(wok_u, len_w);
  _drop(wok_u);
  if ( !getenv("SOFT_TEST_NOPACK") ) {
    u3m_pack();
  }
  _pil = _cue_pill();
  _build(wok_u);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    _run(&wok_u[i_w], 0, 0);
  }
  _grab(wok_u, len_w);

  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    fprintf(stderr, "soft: %s: %" PRIu64 " runs, %" PRIu64 " completed, "
                    "%" PRIu64 " interrupted\r\n",
            wok_u[i_w].nam_c, wok_u[i_w].run_d, wok_u[i_w].don_d,
            wok_u[i_w].int_d);
  }
  fprintf(stderr, "test soft: ok\r\n");
  return 0;
}
