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

#include "noun.h"
#include "ivory.h"
#include "ur/ur.h"
#include "vere.h"

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
  u3m_boot_lite(1 << 27);
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

/* _WORK: the number of workloads.
*/
#define _WORK  5

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

  for ( c3_w i_w = 0; i_w < _WORK; i_w++ ) {
    wok_u[i_w].nam_c = new_u[i_w].nam_c;
    wok_u[i_w].fun_f = new_u[i_w].fun_f;
    wok_u[i_w].arg   = new_u[i_w].arg;
  }
}

/* _drop(): release the nouns held here.
*/
static void
_drop(_work* wok_u)
{
  for ( c3_w i_w = 0; i_w < _WORK; i_w++ ) {
    u3z(wok_u[i_w].arg);
    wok_u[i_w].arg = 0;
  }
  u3z(_pil);
  _pil = 0;
}

int
main(int argc, char* argv[])
{
  c3_w  num_w = 120;
  c3_w  sed_w = (c3_w)time(0);
  c3_c* env_c;

  if ( (env_c = getenv("SOFT_TEST_N")) )    num_w = (c3_w)atoi(env_c);
  if ( (env_c = getenv("SOFT_TEST_SEED")) ) sed_w = (c3_w)atoi(env_c);

  _setup();
  srand(sed_w);
  fprintf(stderr, "soft: seed %" PRIc3_w ", %" PRIc3_w " interrupted runs\r\n",
          sed_w, num_w);

  _work wok_u[_WORK];
  c3_w  len_w = _WORK;

  memset(wok_u, 0, sizeof(wok_u));
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

  //  interrupted runs, each with a timeout up to a bit past its duration
  //
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    _work* w_u   = &wok_u[rand() % len_w];
    c3_w   top_w = (c3_w)(w_u->dur_d / 1000) + 2;
    c3_w   mil_w = 1 + (rand() % (top_w + (top_w / 4)));

    _run(w_u, mil_w, 0);
    if ( 0 == ((i_w + 1) % 20) ) {
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
