/// @file

#include "trace.h"

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#include "allocate.h"
#include "imprison.h"
#include "jets/k.h"
#include "log.h"
#include "manage.h"
#include "options.h"
#include "retrieve.h"
#include "vortex.h"

/** Global variables.
**/
u3t_spin *stk_u;
u3t_trace u3t_Trace;

static c3_o _ct_lop_o;

/// Nock PID.
static c3_ws _nock_pid_i = 0;

/// JSON trace file.
static FILE* _file_u = NULL;

/// Trace counter. Tracks the number of entries written to the JSON trace file.
static c3_w _trace_cnt_w = 0;

/// File counter. Tracks the number of times u3t_trace_close() has been called.
static c3_w _file_cnt_w = 0;

/* u3t_push(): push on trace stack.
*/
void
u3t_push(u3_noun mon)
{
  u3R->bug.tax = u3nc(mon, u3R->bug.tax);
}

/* u3t_mean(): push `[%mean roc]` on trace stack.
*/
void
u3t_mean(u3_noun roc)
{
  u3R->bug.tax = u3nc(u3nc(c3__mean, roc), u3R->bug.tax);
}

/* u3t_drop(): drop from meaning stack.
*/
void
u3t_drop(void)
{
  u3_assert(_(u3du(u3R->bug.tax)));
  {
    u3_noun tax = u3R->bug.tax;

    u3R->bug.tax = u3k(u3t(tax));
    u3z(tax);
  }
}

/* u3t_slog(): print directly.
*/
void
u3t_slog(u3_noun hod)
{
  if ( 0 != u3C.slog_f ) {
    u3C.slog_f(hod);
  }
  else {
    u3z(hod);
  }
}

/* u3t_heck(): profile point.
*/
void
u3t_heck(u3_atom cog)
{
#if 0
  u3R->pro.cel_d++;
#else
  c3_w len_w = u3r_met(3, cog);
  c3_c* str_c = alloca(1 + len_w);

  u3r_bytes(0, len_w, (c3_y *)str_c, cog);
  str_c[len_w] = 0;

  //  Profile sampling, because it allocates on the home road,
  //  only works on when we're not at home.
  //
  if ( &(u3H->rod_u) != u3R ) {
    u3a_road* rod_u;

    rod_u = u3R;
    u3R = &(u3H->rod_u);
    {
      if ( 0 == u3R->pro.day ) {
        u3R->pro.day = u3do("doss", 0);
      }
      u3R->pro.day = u3dc("pi-heck", u3i_string(str_c), u3R->pro.day);
    }
    u3R = rod_u;
  }
#endif
}

#if 0
static void
_ct_sane(u3_noun lab)
{
  if ( u3_nul != lab ) {
    u3_assert(c3y == u3du(lab));
    u3_assert(c3y == u3ud(u3h(lab)));
    _ct_sane(u3t(lab));
  }
}
#endif

#if 1
/* _t_samp_process(): process raw sample data from live road.
*/
static u3_noun
_t_samp_process(u3_road* rod_u)
{
  u3_noun pef   = u3_nul;           // (list (pair path (map path ,@ud)))
  u3_noun muf   = u3_nul;           // (map path ,@ud)
  c3_w    len_w = 0;

  //  Accumulate a label/map stack which collapses recursive segments.
  //
  while ( rod_u ) {
    u3_noun don = rod_u->pro.don;

    while ( u3_nul != don ) {
      //  Get surface allocated label
      //
      //  u3_noun lab = u3nc(u3i_string("foobar"), 0);
      u3_noun laj = u3h(don),
              lab = u3a_take(laj);
      u3a_wash(laj);

      //  Add the label to the traced label stack, trimming recursion.
      //
      {
        u3_noun old;

        if ( u3_none == (old = u3kdb_get(u3k(muf), u3k(lab))) ) {
          muf = u3kdb_put(muf, u3k(lab), len_w);
          pef = u3nc(u3nc(lab, u3k(muf)), pef);
          len_w += 1;
        }
        else {
          if ( !_(u3a_is_cat(old)) ) {
            u3m_bail(c3__fail);
          }

          u3z(muf);
          while ( len_w > (old + 1) ) {
            u3_noun t_pef = u3k(u3t(pef));

            len_w -= 1;
            u3z(pef);
            pef = t_pef;
          }
          muf = u3k(u3t(u3h(pef)));
          u3z(lab);
        }
      }
      don = u3t(don);
    }
    rod_u = u3tn(u3_road, rod_u->par_p);
  }
  u3z(muf);

  //  Lose the maps and save a pure label stack in original order.
  //
  {
    u3_noun pal = u3_nul;

    while ( u3_nul != pef ) {
      u3_noun h_pef = u3h(pef);
      u3_noun t_pef = u3k(u3t(pef));

      pal = u3nc(u3k(u3h(h_pef)), pal);

      u3z(pef);
      pef = t_pef;
    }

    // u3l_log("sample: stack length %d", u3kb_lent(u3k(pal)));
    return pal;
  }
}
#endif

/* u3t_samp(): sample.
*/
void
u3t_samp(void)
{
  if ( c3y == _ct_lop_o ) {
    // _ct_lop_o here is a mutex for modifying pro.don. we
    // do not want to sample in the middle of doing that, as
    // it can cause memory errors.
    return;
  }

  c3_w old_wag = u3C.wag_w;
  u3C.wag_w &= ~u3o_debug_cpu;
  u3C.wag_w &= ~u3o_trace;

  //  Profile sampling, because it allocates on the home road,
  //  only works on when we're not at home.
  //
  if ( &(u3H->rod_u) != u3R ) {
    c3_l      mot_l;
    u3a_road* rod_u;

    if ( _(u3T.mal_o) ) {
      mot_l = c3_s3('m','a','l');
    }
    else if ( _(u3T.coy_o) ) {
      mot_l = c3_s3('c','o','y');
    }
    else if ( _(u3T.euq_o) ) {
      mot_l = c3_s3('e','u','q');
    }
    else if ( _(u3T.far_o) ) {
      mot_l = c3_s3('f','a','r');
    }
    else if ( _(u3T.noc_o) ) {
      u3_assert(!_(u3T.glu_o));
      mot_l = c3_s3('n','o','c');
    }
    else if ( _(u3T.glu_o) ) {
      mot_l = c3_s3('g','l','u');
    }
    else {
      mot_l = c3_s3('f','u','n');
    }

    rod_u = u3R;
    u3R = &(u3H->rod_u);
    {
      u3_noun lab = _t_samp_process(rod_u);

      u3_assert(u3R == &u3H->rod_u);
      if ( 0 == u3R->pro.day ) {
        /* bunt a +doss
        */
        u3R->pro.day = u3nt(u3nq(0, 0, 0, u3nq(0, 0, 0, 0)), 0, 0);
      }
      u3R->pro.day = u3dt("pi-noon", mot_l, lab, u3R->pro.day);
    }
    u3R = rod_u;
  }
  u3C.wag_w = old_wag;
}

/* u3t_come(): push on profile stack; return yes if active push.  RETAIN.
*/
c3_o
u3t_come(u3_noun lab)
{
  if ( (u3_nul == u3R->pro.don) || !_(u3r_sing(lab, u3h(u3R->pro.don))) ) {
    u3a_gain(lab);
    _ct_lop_o = c3y;
    u3R->pro.don = u3nc(lab, u3R->pro.don);
    _ct_lop_o = c3n;
    return c3y;
  }
  else return c3n;
}

/* u3t_flee(): pop off profile stack.
*/
void
u3t_flee(void)
{
  _ct_lop_o = c3y;
  u3_noun don  = u3R->pro.don;
  u3R->pro.don = u3k(u3t(don));
  _ct_lop_o = c3n;
  u3z(don);
}

/*  u3t_trace_open(): opens a trace file and writes the preamble.
*/
void
u3t_trace_open(const c3_c* dir_c)
{
  c3_c fil_c[2048];

  if ( !dir_c ) {
    return;
  }

  snprintf(fil_c, 2048, "%s/.urb/put/trace", dir_c);

  struct stat st;
  if (  (-1 == stat(fil_c, &st))
     && (-1 == c3_mkdir(fil_c, 0700)) )
  {
    fprintf(stderr, "mkdir: %s failed: %s\r\n", fil_c, strerror(errno));
    return;
  }

  c3_c lif_c[2056];
  snprintf(lif_c, 2056, "%s/%d.json", fil_c, _file_cnt_w);

  _file_u = c3_fopen(lif_c, "w");
  _nock_pid_i = (int)getpid();

  if ( !_file_u ) {
    fprintf(stderr, "trace open: %s\r\n", strerror(errno));
    return;
  }

  fprintf(_file_u, "[ ");

  // We have two "threads", the event processing and the nock stuff.
  //   tid 1 = event processing
  //   tid 2 = nock processing
  fprintf(_file_u,
      "{\"name\": \"process_name\", \"ph\": \"M\", \"pid\": %d, \"args\": "
      "{\"name\": \"urbit\"}},\n",
      _nock_pid_i);
  fprintf(_file_u,
          "{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": %d, \"tid\": 1, "
          "\"args\": {\"name\": \"Event Processing\"}},\n",
          _nock_pid_i);
  fprintf(_file_u,
          "{\"name\": \"thread_sort_index\", \"ph\": \"M\", \"pid\": %d, "
          "\"tid\": 1, \"args\": {\"sort_index\": 1}},\n",
          _nock_pid_i);
  fprintf(_file_u,
          "{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": %d, \"tid\": 2, "
          "\"args\": {\"name\": \"Nock\"}},\n",
          _nock_pid_i);
  fprintf(_file_u,
          "{\"name\": \"thread_sort_index\", \"ph\": \"M\", \"pid\": %d, "
          "\"tid\": 2, \"args\": {\"sort_index\": 2}},\n",
          _nock_pid_i);
  _trace_cnt_w = 5;
}

/*  u3t_trace_close(): closes a trace file. optional.
*/
void
u3t_trace_close(void)
{
  if ( !_file_u )
    return;

  // We don't terminate the JSON because of the file format.
  fclose(_file_u);
  _trace_cnt_w = 0;
  _file_cnt_w++;
}

/*  u3t_trace_time(): microsecond clock
*/
c3_d u3t_trace_time(void)
{
  struct timeval tim_tv;
  gettimeofday(&tim_tv, 0);
  return 1000000ULL * tim_tv.tv_sec + tim_tv.tv_usec;
}

/* u3t_nock_trace_push(): push a trace onto the trace stack; returns yes if pushed.
 *
 * The trace stack is a stack of [path time-entered].
 */
c3_o
u3t_nock_trace_push(u3_noun lab)
{
  if ( !_file_u )
    return c3n;

  if ( (u3_nul == u3R->pro.trace) ||
       !_(u3r_sing(lab, u3h(u3h(u3R->pro.trace)))) ) {
    u3a_gain(lab);
    c3_d time = u3t_trace_time();
    u3R->pro.trace = u3nc(u3nc(lab, u3i_chubs(1, &time)), u3R->pro.trace);
    return c3y;
  }
  else {
    return c3n;
  }
}

/* u3t_nock_trace_pop(): pops a trace from the trace stack.
 *
 * When we remove the trace from the stack, we check to see if the sample is
 * large enough to process, as we'll otherwise keep track of individual +add
 * calls. If it is, we write it out to the tracefile.
 */
void
u3t_nock_trace_pop(void)
{
  if ( !_file_u )
    return;

  u3_noun trace  = u3R->pro.trace;
  u3R->pro.trace = u3k(u3t(trace));

  u3_noun item = u3h(trace);
  u3_noun lab = u3h(item);
  c3_d start_time = u3r_chub(0, u3t(item));

  // 33microseconds (a 30th of a millisecond).
  c3_d duration = u3t_trace_time() - start_time;
  if (duration > 33) {
    c3_c* name = u3m_pretty_path(lab);

    fprintf(_file_u,
            "{\"cat\": \"nock\", \"name\": \"%s\", \"ph\":\"%c\", \"pid\": %d, "
            "\"tid\": 2, \"ts\": %" PRIu64 ", \"dur\": %" PRIu64 "}, \n",
            name,
            'X',
            _nock_pid_i,
            start_time,
            duration);

    c3_free(name);
    _trace_cnt_w++;
  }

  u3z(trace);
}

/* u3t_event_trace(): dumps a simple event from outside nock.
*/
void
u3t_event_trace(const c3_c* name, c3_c type)
{
  if ( !_file_u )
    return;

  fprintf(_file_u,
          "{\"cat\": \"event\", \"name\": \"%s\", \"ph\":\"%c\", \"pid\": %d, "
          "\"tid\": 1, \"ts\": %" PRIu64 ", \"id\": \"0x100\"}, \n",
          name,
          type,
          _nock_pid_i,
          u3t_trace_time());
  _trace_cnt_w++;
}

/* u3t_print_steps: print step counter.
*/
void
u3t_print_steps(FILE* fil_u, c3_c* cap_c, c3_d sep_d)
{
  u3_assert( 0 != fil_u );

  c3_w gib_w = (sep_d / 1000000000ULL);
  c3_w mib_w = (sep_d % 1000000000ULL) / 1000000ULL;
  c3_w kib_w = (sep_d % 1000000ULL) / 1000ULL;
  c3_w bib_w = (sep_d % 1000ULL);

  //  XX prints to stderr since it's called on shutdown, daemon may be gone
  //
  if ( sep_d ) {
    if ( gib_w ) {
      fprintf(fil_u, "%s: G/%d.%03d.%03d.%03d\r\n",
          cap_c, gib_w, mib_w, kib_w, bib_w);
    }
    else if ( mib_w ) {
      fprintf(fil_u, "%s: M/%d.%03d.%03d\r\n", cap_c, mib_w, kib_w, bib_w);
    }
    else if ( kib_w ) {
      fprintf(fil_u, "%s: K/%d.%03d\r\n", cap_c, kib_w, bib_w);
    }
    else if ( bib_w ) {
      fprintf(fil_u, "%s: %d\r\n", cap_c, bib_w);
    }
  }
}

/* u3t_damp(): print and clear profile data.
*/
void
u3t_damp(FILE* fil_u)
{
  u3_assert( 0 != fil_u );

  if ( 0 != u3R->pro.day ) {
    u3_noun wol = u3do("pi-tell", u3R->pro.day);

    //  XX prints to stderr since it's called on shutdown, daemon may be gone
    //
    {
      u3_noun low = wol;

      while ( u3_nul != low ) {
        c3_c* str_c = (c3_c*)u3r_tape(u3h(low));
        fputs(str_c, fil_u);
        fputs("\r\n", fil_u);

        c3_free(str_c);
        low = u3t(low);
      }

      u3z(wol);
    }

    /* bunt a +doss
    */
    u3R->pro.day = u3nt(u3nq(0, 0, 0, u3nq(0, 0, 0, 0)), 0, 0);
  }

  u3t_print_steps(fil_u, "nocks", u3R->pro.nox_d);
  u3t_print_steps(fil_u, "cells", u3R->pro.cel_d);

  u3R->pro.nox_d = 0;
  u3R->pro.cel_d = 0;
}

/* _ct_sigaction(): profile sigaction callback.
*/
void _ct_sigaction(c3_i x_i)
{
  u3t_samp();
}

/* u3t_init(): initialize tracing layer.
*/
void
u3t_init(void)
{
  u3T.noc_o = c3n;
  u3T.glu_o = c3n;
  u3T.mal_o = c3n;
  u3T.far_o = c3n;
  u3T.coy_o = c3n;
  u3T.euq_o = c3n;
}

c3_w
u3t_trace_cnt(void)
{
  return _trace_cnt_w;
}

c3_w
u3t_file_cnt(void)
{
  return _file_cnt_w;
}

/* u3t_boot(): turn sampling on.
*/
void
u3t_boot(void)
{
#ifndef U3_OS_windows
  if ( u3C.wag_w & u3o_debug_cpu ) {
    _ct_lop_o = c3n;
#if defined(U3_OS_PROF)
    //  skip profiling if we don't yet have an arvo kernel
    //
    if ( 0 == u3A->roc ) {
      return;
    }

    // Register _ct_sigaction to be called on `SIGPROF`.
    {
      struct sigaction sig_s = {{0}};
      sig_s.sa_handler = _ct_sigaction;
      sigemptyset(&(sig_s.sa_mask));
      sigaction(SIGPROF, &sig_s, 0);
    }

    // Unblock `SIGPROF` for this thread (we will block it again when `u3t_boff` is called).
    {
      sigset_t set;
      sigemptyset(&set);
      sigaddset(&set, SIGPROF);
      if ( 0 != pthread_sigmask(SIG_UNBLOCK, &set, NULL) ) {
        u3l_log("trace: thread mask SIGPROF: %s", strerror(errno));
      }
    }

    // Ask for SIGPROF to be sent every 10ms.
    {
      struct itimerval itm_v = {{0}};
      itm_v.it_interval.tv_usec = 10000;
      itm_v.it_value = itm_v.it_interval;
      setitimer(ITIMER_PROF, &itm_v, 0);
    }
#endif
  }
#endif
}

/* u3t_boff(): turn profile sampling off.
*/
void
u3t_boff(void)
{
#ifndef U3_OS_windows
  if ( u3C.wag_w & u3o_debug_cpu ) {
#if defined(U3_OS_PROF)
    // Mask SIGPROF signals in this thread (and this is the only
    // thread that unblocked them).
    {
      sigset_t set;
      sigemptyset(&set);
      sigaddset(&set, SIGPROF);
      if ( 0 != pthread_sigmask(SIG_BLOCK, &set, NULL) ) {
        u3l_log("trace: thread mask SIGPROF: %s", strerror(errno));
      }
    }

    // Disable the SIGPROF timer.
    {
      struct itimerval itm_v = {{0}};
      setitimer(ITIMER_PROF, &itm_v, 0);
    }

    // Ignore SIGPROF signals.
    {
      struct sigaction sig_s = {{0}};
      sigemptyset(&(sig_s.sa_mask));
      sig_s.sa_handler = SIG_IGN;
      sigaction(SIGPROF, &sig_s, 0);
    }
#endif
  }
#endif
}


/* u3t_slog_cap(): slog a tank with a caption with
** a given priority c3_l (assumed 0-3).
*/
void
u3t_slog_cap(c3_l pri_l, u3_noun cap, u3_noun tan)
{
  u3t_slog(
    u3nc(
      pri_l,
      u3nt(
        c3__rose,
        u3nt(u3nt(':', ' ', u3_nul), u3_nul, u3_nul),
        u3nt(cap, tan, u3_nul)
      )
    )
  );
}


/* u3t_slog_trace(): given a c3_l priority pri and a raw stack tax
** flop the order into start-to-end, render, and slog each item
** until done.
*/
void
u3t_slog_trace(c3_l pri_l, u3_noun tax)
{
  // render the stack
  // Note: ton is a reference to a data struct
  // we have just allocated
  // lit is used as a moving cursor pointer through
  // that allocated struct
  // once we finish lit will be null, but ton will still
  // point to the whole valid allocated data structure
  // and thus we can free it safely at the end of the func
  // to clean up after ourselves.
  // Note: flop reverses the stack trace list 'tax'
  u3_noun ton = u3dc("mook", 2, u3kb_flop(tax));
  u3_noun lit = u3t(ton);

  // print the stack one stack item at a time
  while ( u3_nul != lit ) {
    u3t_slog(u3nc(pri_l, u3k(u3h(lit)) ));
    lit = u3t(lit);
  }

  u3z(ton);
}


/* u3t_slog_nara(): slog only the deepest road's trace with
** c3_l priority pri
*/
void
u3t_slog_nara(c3_l pri_l)
{
  u3_noun tax = u3k(u3R->bug.tax);
  u3t_slog_trace(pri_l, tax);
}


/* u3t_slog_hela(): join all roads' traces together into one tax
** and pass it to slog_trace along with the given c3_l priority pri_l
*/
void
u3t_slog_hela(c3_l pri_l)
{
  // rod_u protects us from mutating the global state
  u3_road* rod_u = u3R;

  // inits to the the current road's trace
  u3_noun tax = u3k(rod_u->bug.tax);

  // while there is a parent road ref ...
  while ( &(u3H->rod_u) != rod_u ) {
    // ... point at the next road and append its stack to tax
    rod_u = u3tn(u3_road, rod_u->par_p);
    tax = u3kb_weld(tax, u3k(rod_u->bug.tax));
  }

  u3t_slog_trace(pri_l, tax);
}

/* _ct_roundf(): truncate a float to precision equivalent to %.2f */
static float
_ct_roundf(float per_f)
{
  // scale the percentage so that all siginificant digits
  // would be retained when truncted to an int, then add 0.5
  // to account for rounding without using round or roundf
  float big_f = (per_f*10000)+0.5;
  // truncate to int
  c3_w big_w = (c3_w) big_f;
  // convert to float and scale down such that
  // our last two digits are right of the decimal
  float tuc_f = (float) big_w/100.0;
  return tuc_f;
}

/* _ct_meme_percent(): convert two ints into a percentage */
static float
_ct_meme_percent(c3_w lit_w, c3_w big_w)
{
  // get the percentage of our inputs as a float
  float raw_f = (float) lit_w/big_w;
  return _ct_roundf(raw_f);
}

/* _ct_all_heap_size(): return the size in bytes of ALL space on the Loom
**                      over all roads, currently in use as heap.
*/
static c3_w
_ct_all_heap_size(u3_road* r) {
  if (r == &(u3H->rod_u)) {
    return u3a_heap(r)*4;
  } else {
    // recurse
    return (u3a_heap(r)*4) + _ct_all_heap_size(u3tn(u3_road, r->par_p));
  }
}

/* These two structs, bar_item and bar_info, store the mutable data
** to normalize measured Loom usage values into ints that will fit
** into a fixed width ascii bar chart.
*/
struct
bar_item {
  // index
  c3_w dex_w;
  // lower bound
  c3_w low_w;
  // original value
  float ori_f;
  // difference
  float dif_f;
};

struct
bar_info {
  struct bar_item s[6];
};

/* _ct_boost_small(): we want zero to be zero,
**                    anything between zero and one to be one,
**                    and all else to be whatever it is.
*/
static float
_ct_boost_small(float num_f)
{
  return
    0.0 >= num_f ? 0.0:
    1.0 > num_f ? 1.0:
    num_f;
}

/* _ct_global_difference(): each low_w represents the normalized integer value
 *                          of its loom item, and ideally the sum of all loom low_w
 *                          values should be 100. This function reports how far from
 *                          the ideal bar_u is.
*/
static c3_ws
_ct_global_difference(struct bar_info bar_u)
{
  c3_w low_w = 0;
  for (c3_w i=0; i < 6; i++) {
    low_w += bar_u.s[i].low_w;
  }
  return 100 - low_w;
}

/* _ct_compute_roundoff_error(): for each loom item in bar_u
**                               compute the current difference between the int
**                               size and the original float size.
*/
static struct bar_info
_ct_compute_roundoff_error(struct bar_info bar_u)
{
  for (c3_w i=0; i < 6; i++) {
    bar_u.s[i].dif_f = bar_u.s[i].ori_f - bar_u.s[i].low_w;
  }
  return bar_u;
}

/* _ct_sort_by_roundoff_error(): sort loom items from most mis-sized to least */
static struct bar_info
_ct_sort_by_roundoff_error(struct bar_info bar_u)
{
  struct bar_item tem_u;
  for (c3_w i=1; i < 6; i++) {
    for (c3_w j=0; j < 6-i; j++) {
      if (bar_u.s[j+1].dif_f > bar_u.s[j].dif_f) {
        tem_u = bar_u.s[j];
        bar_u.s[j] = bar_u.s[j+1];
        bar_u.s[j+1] = tem_u;
      }
    }
  }
  return bar_u;
}

/* _ct_sort_by_index(): sort loom items into loom order */
static struct bar_info
_ct_sort_by_index(struct bar_info bar_u)
{
  struct bar_item tem_u;
  for (c3_w i=1; i < 6; i++) {
    for (c3_w j=0; j < 6-i; j++) {
      if (bar_u.s[j+1].dex_w < bar_u.s[j].dex_w) {
        tem_u = bar_u.s[j];
        bar_u.s[j] = bar_u.s[j+1];
        bar_u.s[j+1] = tem_u;
      }
    }
  }
  return bar_u;
}

/* _ct_reduce_error(): reduce error by one int step
 *                     making oversized things a bit smaller
 *                     and undersized things a bit bigger
*/
static struct bar_info
_ct_reduce_error(struct bar_info bar_u, c3_ws dif_s)
{
  for (c3_w i=0; i < 6; i++) {
    if (bar_u.s[i].low_w == 0) continue;
    if (bar_u.s[i].low_w == 1) continue;
    if (dif_s > 0) {
      bar_u.s[i].low_w++;
      dif_s--;
    }
    if (dif_s < 0) {
      bar_u.s[i].low_w--;
      dif_s++;
    }
  }
  return bar_u;
}

/* _ct_report_bargraph(): render all six raw loom elements into a fixed-size ascii bargraph */
static void
_ct_report_bargraph(
    c3_c bar_c[105], float hip_f, float hep_f, float fre_f, float pen_f, float tak_f, float tik_f
)
{
  float in[6];
  in[0] = _ct_boost_small(hip_f);
  in[1] = _ct_boost_small(hep_f);
  in[2] = _ct_boost_small(fre_f);
  in[3] = _ct_boost_small(pen_f);
  in[4] = _ct_boost_small(tak_f);
  in[5] = _ct_boost_small(tik_f);

  // init the list of structs
  struct bar_info bar_u;
  for (c3_w i=0; i < 6; i++) {
    bar_u.s[i].dex_w = i;
    bar_u.s[i].ori_f = in[i];
    bar_u.s[i].low_w = (c3_w) bar_u.s[i].ori_f;
  }

  // repeatedly adjust for roundoff error
  // until it is elemenated or we go 100 cycles
  c3_ws dif_s = 0;
  for (c3_w x=0; x<100; x++) {
    bar_u = _ct_compute_roundoff_error(bar_u);
    dif_s = _ct_global_difference(bar_u);
    if (dif_s == 0) break;
    bar_u = _ct_sort_by_roundoff_error(bar_u);
    bar_u = _ct_reduce_error(bar_u, dif_s);
  }
  bar_u = _ct_sort_by_index(bar_u);

  for (c3_w x=1; x<104; x++) {
    bar_c[x] = ' ';
  }
  bar_c[0] = '[';

  // create our bar chart
  const c3_c sym_c[6] = "=-%#+~";
  c3_w x = 0, y = 0;
  for (c3_w i=0; i < 6; i++) {
    x++;
    for (c3_w j=0; j < bar_u.s[i].low_w; j++) {
      bar_c[x+j] = sym_c[i];
      y = x+j;
    }
    if (y > 0) x = y;
  }
  bar_c[101] = ']';
  bar_c[102] = 0;
}

/* _ct_size_prefix(): return the correct metric scalar prifix for a given int */
static c3_c
_ct_size_prefix(c3_d num_d)
{
  return
    (num_d / 1000000000) ? 'G':
    (num_d % 1000000000) / 1000000 ? 'M':
    (num_d % 1000000) / 1000 ? 'K':
    (num_d % 1000) ? ' ':
    'X';
}

/* _ct_report_string(): convert a int into a string, adding a metric scale prefix letter*/
static void
_ct_report_string(c3_c rep_c[32], c3_d num_d)
{
  memset(rep_c, ' ', 31);

  // add the G/M/K prefix
  rep_c[24] = _ct_size_prefix(num_d);
  // consume wor_w into a string one base-10 digit at a time
  // including dot formatting
  c3_w i = 0, j = 0;
  while (num_d > 0) {
    if (j == 3) {
      rep_c[22-i] = '.';
      i++;
      j = 0;
    } else {
      rep_c[22-i] = (num_d%10)+'0';
      num_d /= 10;
      i++;
      j++;
    }
  }
}

/*  _ct_etch_road_depth(): return a the current road depth as a fixed size string */
static void
 _ct_etch_road_depth(c3_c rep_c[32], u3_road* r, c3_w num_w) {
  if (r == &(u3H->rod_u)) {
    _ct_report_string(rep_c, num_w);
    // this will be incorrectly indented, so we fix that here
    c3_w i = 14;
    while (i > 0) {
      rep_c[i] = rep_c[i+16];
      rep_c[i+16] = ' ';
      i--;
    }
  } else {
    _ct_etch_road_depth(rep_c, u3tn(u3_road, r->par_p), ++num_w);
  }
}

/* _ct_etch_memory(): create a single line report of a given captioned item
 *                    with a percentage of space used and the bytes used
 *                    scaled by a metric scaling postfix (ie MB, GB, etc)
*/
static void
_ct_etch_memory(c3_c rep_c[32], float per_f, c3_w num_w)
{
  // create the basic report string
  _ct_report_string(rep_c, num_w);
  // add the Bytes postfix to the size report
  rep_c[25] = 'B';

  // add the space-percentage into the report
  rep_c[2] = '0', rep_c[3] = '.', rep_c[4] = '0', rep_c[5] = '0';
  c3_w per_i = (c3_w) (per_f*100);
  c3_w i = 0;
  while (per_i > 0 && i < 6) {
    if (i != 2) {
      rep_c[5-i] = (per_i%10)+'0';
      per_i /= 10;
    }
    i++;
  }
  // add the percent sign
  rep_c[6] = '%';
}

/* _ct_etch_steps(): create a single line report of a given captioned item
**                   scaled by a metric scaling postfix, but unitless.
*/
static void
_ct_etch_steps(c3_c rep_c[32], c3_d sep_d)
{
  _ct_report_string(rep_c, sep_d);
}

/* u3t_etch_meme(): report memory stats at call time */
u3_noun
u3t_etch_meme(c3_l mod_l)
{
  u3a_road* lum_r;
  lum_r = &(u3H->rod_u);
  // this will need to switch to c3_d when we go to a 64 loom
  c3_w top_w = u3a_full(lum_r)*4,
       ful_w = u3a_full(u3R)*4,
       fre_w = u3a_idle(u3R)*4,
       tak_w = u3a_temp(u3R)*4,
       hap_w = u3a_heap(u3R)*4,
       pen_w = u3a_open(u3R)*4;

  c3_w imu_w = top_w-ful_w;
  c3_w hep_w = hap_w-fre_w;


  float hep_f = _ct_meme_percent(hep_w, top_w),
        fre_f = _ct_meme_percent(fre_w, top_w),
        pen_f = _ct_meme_percent(pen_w, top_w),
        tak_f = _ct_meme_percent(tak_w, top_w);
  float ful_f = hep_f + fre_f + pen_f + tak_f;

  c3_w hip_w = _ct_all_heap_size(u3R) - hap_w;
  c3_w tik_w = imu_w - hip_w;
  float hip_f = _ct_meme_percent(hip_w, top_w),
        tik_f = _ct_meme_percent(tik_w, top_w);

#ifdef U3_CPU_DEBUG
  /* iff we are using CPU_DEBUG env var
  ** we can report more facts:
  **  max_w: max allocated on the current road (not global, not including child roads)
  **  cel_d: max cells allocated in current road (inc closed kids, but not parents)
  **  nox_d: nock steps performed in current road
  */
  c3_w max_w = (u3R->all.max_w*4)+imu_w;
  float max_f = _ct_meme_percent(max_w, top_w);
  c3_d cel_d = u3R->pro.cel_d;
  c3_d nox_d = u3R->pro.nox_d;
  // iff we have a max_f we will render it into the bar graph
  // in other words iff we have max_f it will always replace something
  c3_w inc_w = (max_f > hip_f+1.0) ? (c3_w) max_f+0.5 : (c3_w) hip_f+1.5;
#endif

  // warn if any sanity checks have failed
  if (100.01 < (hip_f + hep_f + fre_f + pen_f + tak_f + tik_f))
    u3t_slog_cap(2, u3i_string("warning"), u3i_string("loom sums over 100.01%"));
  if ( 99.99 > (hip_f + hep_f + fre_f + pen_f + tak_f + tik_f))
    u3t_slog_cap(2, u3i_string("warning"), u3i_string("loom sums under 99.99%"));

  c3_c bar_c[105];
  bar_c[0] = 0;
  _ct_report_bargraph(bar_c, hip_f, hep_f, fre_f, pen_f, tak_f, tik_f);

  c3_w dol = (c3_w) _ct_roundf(hip_f/100);
  bar_c[dol] = '$';
#ifdef U3_CPU_DEBUG
  if (max_f > 0.0) {
    bar_c[inc_w] = '|';
  }
#endif

  c3_c dir_n[8];
  dir_n[0] = 0;
  if ( u3a_is_north(u3R) == c3y ) {
    strcat(dir_n, "  North");
  } else {
    strcat(dir_n, "  South");
  }

  if (mod_l == 0) {
    return u3i_string(bar_c);
  }
  else {
    c3_c rep_c[32];
    rep_c[31] = 0;
    c3_c str_c[1024];
    str_c[0] = 0;
    // each report line is at most 54 chars long
    strcat(str_c, "Legend | Report:");

    strcat(str_c, "\n                loom: "); _ct_etch_memory(rep_c, 100.0, top_w); strcat(str_c, rep_c);
    strcat(str_c, "\n                road: "); _ct_etch_memory(rep_c, ful_f, ful_w); strcat(str_c, rep_c);
    strcat(str_c, "\n");
    strcat(str_c, "\n  =  immutable  heap: "); _ct_etch_memory(rep_c, hip_f, hip_w); strcat(str_c, rep_c);
    strcat(str_c, "\n  -      solid  heap: "); _ct_etch_memory(rep_c, hep_f, hep_w); strcat(str_c, rep_c);
    strcat(str_c, "\n  %      freed  heap: "); _ct_etch_memory(rep_c, fre_f, fre_w); strcat(str_c, rep_c);
    strcat(str_c, "\n  #       open space: "); _ct_etch_memory(rep_c, pen_f, pen_w); strcat(str_c, rep_c);
    strcat(str_c, "\n  +            stack: "); _ct_etch_memory(rep_c, tak_f, tak_w); strcat(str_c, rep_c);
    strcat(str_c, "\n  ~  immutable stack: "); _ct_etch_memory(rep_c, tik_f, tik_w); strcat(str_c, rep_c);
    strcat(str_c, "\n");
    strcat(str_c, "\n  $ allocation frame: "); _ct_etch_memory(rep_c, hip_f, hip_w); strcat(str_c, rep_c);
#ifdef U3_CPU_DEBUG
    strcat(str_c, "\n  |  road max memory: "); _ct_etch_memory(rep_c, max_f, max_w); strcat(str_c, rep_c);
    strcat(str_c, "\n");
    strcat(str_c, "\n     road cells made: "); _ct_etch_steps(rep_c, cel_d); strcat(str_c, rep_c);
    strcat(str_c, "\n     road nocks made: "); _ct_etch_steps(rep_c, nox_d); strcat(str_c, rep_c);
#endif
    strcat(str_c, "\n      road direction: "); strcat(str_c, dir_n);
    strcat(str_c, "\n          road depth: "); _ct_etch_road_depth(rep_c, u3R, 1); strcat(str_c, rep_c);
    strcat(str_c, "\n\nLoom: "); strcat(str_c, bar_c);
    return u3i_string(str_c);
  }
}

/* u3t_sstack_init: initalize a root node on the spin stack 
*/
void
u3t_sstack_init()
{
#ifndef U3_OS_windows
  c3_c shm_name[256];
  snprintf(shm_name, sizeof(shm_name), SLOW_STACK_NAME, getppid());
  c3_w shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
  if ( -1 == shm_fd) {
    perror("shm_open failed");
    return;
  }

  if ( -1 == ftruncate(shm_fd, TRACE_PSIZE)) {
    perror("truncate failed");
    return;
  }

  stk_u = mmap(NULL, TRACE_PSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  
  if ( MAP_FAILED == stk_u ) {
    perror("mmap failed");
    return;
  }

  stk_u->off_w = 0;
  stk_u->fow_w = 0;
  u3t_sstack_push(c3__root);
#endif
}

#ifndef U3_OS_windows
/* u3t_sstack_open: initalize a root node on the spin stack 
 */
u3t_spin*
u3t_sstack_open()
{
  //Setup spin stack
  c3_c shm_name[256];
  snprintf(shm_name, sizeof(shm_name), SLOW_STACK_NAME, getpid());
  c3_w shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0);
  if ( -1 == shm_fd) {
    perror("shm_open failed");
    return NULL; 
  }

  u3t_spin* stk_u = mmap(NULL, TRACE_PSIZE, 
                         PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  
  if ( MAP_FAILED == stk_u ) {
    perror("mmap failed");
    return NULL; 
  }

  return stk_u;
}
#endif
/* u3t_sstack_exit: shutdown the shared memory for thespin stack 
*/
void
u3t_sstack_exit()
{
  munmap(stk_u, u3a_page);
}

/* u3t_sstack_push: push a noun on the spin stack.
*/
void
u3t_sstack_push(u3_noun nam)
{
  if ( !stk_u ) {
    u3z(nam);
    return;
  }

  if ( c3n == u3ud(nam) ) {
    u3z(nam);
    nam = c3__cell;
  }

  c3_w  met_w = u3r_met(3, nam);
  
  // Exit if full
  if ( 0 < stk_u->fow_w || 
       sizeof(stk_u->dat_y) < stk_u->off_w + met_w + sizeof(c3_w) ) {
    stk_u->fow_w++;
    return;
  }

  u3r_bytes(0, met_w, (c3_y*)(stk_u->dat_y+stk_u->off_w), nam);
  stk_u->off_w += met_w;

  memcpy(&stk_u->dat_y[stk_u->off_w], &met_w, sizeof(c3_w));
  stk_u->off_w += sizeof(c3_w);
  u3z(nam);
}

/* u3t_sstack_pop: pop a noun from the spin stack.
*/
void
u3t_sstack_pop()
{
  if (  !stk_u ) return;
  if ( 0 < stk_u->fow_w ) {
    stk_u->fow_w--;
  } else {
    c3_w len_w = (c3_w) stk_u->dat_y[stk_u->off_w - sizeof(c3_w)];
    stk_u->off_w -= (len_w+sizeof(c3_w));
  }
}

