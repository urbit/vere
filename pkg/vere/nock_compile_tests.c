/// @file

#include "noun.h"
#include "steel.h"
#include "ur/ur.h"
#include "vere.h"

#include "ivory.h"
#include "direct.h"
#include "mars.h"
#include "nock-compile.h"
#include "jets/w.h"

#include <sys/time.h>
#if defined(U3NC_STAT) && defined(__x86_64__)
#  include <x86intrin.h>
#  define _tsc()  ((c3_d)__rdtsc())
#else
#  define _tsc()  ((c3_d)0)
#endif

static c3_d    _time(void);
static u3_weak _run(const c3_c* nam_c, u3_funk fun_f, u3_noun bus, u3_noun fol);
static u3_noun _nock_nc(u3_noun bus_fol);

/* _slurp(): the contents of a file, NUL-terminated, or 0 with a message.
*/
static c3_c*
_slurp(const c3_c* pax_c, size_t* len_i)
{
  FILE* fil_u = fopen(pax_c, "rb");
  c3_c* buf_c;
  long  len_l;

  if ( !fil_u ) {
    fprintf(stderr, "can't open %s\r\n", pax_c);
    return 0;
  }
  fseek(fil_u, 0, SEEK_END);
  len_l = ftell(fil_u);
  fseek(fil_u, 0, SEEK_SET);
  buf_c = c3_calloc(len_l + 1);
  if ( (size_t)len_l != fread(buf_c, 1, len_l, fil_u) ) {
    fprintf(stderr, "short read of %s\r\n", pax_c);
    return 0;
  }
  fclose(fil_u);
  if ( len_i ) *len_i = len_l;
  return buf_c;
}

/* _slog(): render a slog, as the king would.
*/
static c3_d _slog_d[2];   //  slogs printed, and their cycles

static void
_slog(u3_noun hod)
{
  c3_d beg_d = _tsc();

  u3_pier_tank(0, u3h(hod), u3k(u3t(hod)));
  u3z(hod);
  _slog_d[0]++;
  _slog_d[1] += _tsc() - beg_d;
}

/* _setup(): boot the kernel from the ivory pill in the u3n interpreter,
**           which registers its cores, and boot the SKA core from the
**           steel pill, which gets the registrations.
*/
static void
_setup(void)
{
  u3_cue_xeno* sil_u;
  u3_weak      pil;

  u3C.wag_h |= u3o_hashless;
  u3C.hap_w  = 50000;   //  memo cache sizes, as in the runtime
  u3C.per_w  = 50000;

  //  U3NC_HAP: the transient memo cache's cap instead, for benchmarks
  //
  if ( getenv("U3NC_HAP") ) {
    u3C.hap_w = atoi(getenv("U3NC_HAP"));
  }

  //  U3NC_LOOM: loom size in bits, for machines that can't map 16GB
  //
  {
    c3_c* lom_c = getenv("U3NC_LOOM");
    u3m_boot_lite(1ULL << (lom_c ? atoi(lom_c) : 34));
  }

  //  slogs rendered to stderr, which the build shows, without color codes;
  //  after the boot, which clears the handler
  //
  u3C.slog_f = _slog;
  u3_Host.ops_u.dem = c3y;
  u3_Host.ops_u.tem = c3y;

  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, u3_Ivory_pill_len,
                                                  u3_Ivory_pill)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);

  if ( c3n == u3v_boot_lite(u3k(pil)) ) {
    printf("*** fail _setup 3\n");
    exit(1);
  }

  //  U3NC_STEEL: the steel pill from that file instead of this binary's
  //
  if ( getenv("U3NC_STEEL") ) {
    size_t len_i;
    c3_c*  buf_c = _slurp(getenv("U3NC_STEEL"), &len_i);

    if ( !buf_c ) exit(1);
    u3d_boot(u3i_bytes(len_i, (c3_y*)buf_c));
    c3_free(buf_c);
  }
  else {
    u3_mars_steel();
  }
  u3z(pil);

  //  check reference counts after every run: u3m_soft_top() marks and
  //  sweeps the inner road, recounting references, and a leak or a short
  //  count aborts.  With U3NC_ALL only, the run of every test; off for
  //  benchmarking: the sweep is timed with the run.
  //
  if ( getenv("U3NC_ALL") && !getenv("U3NC_BENCH") ) {
    u3C.wag_h |= u3o_debug_ram | u3o_leak_crash;
  }
}

/* _prof_moth(): the samples of a +moan.
*/
static c3_d
_prof_moth(u3_noun mon)
{
  c3_d tot_d = 0;
  c3_w i_w;

  for ( i_w = 0; i_w < 6; i_w++ ) {
    tot_d += u3r_chub(0, u3h(mon));
    mon = u3t(mon);
  }
  return tot_d + u3r_chub(0, mon);
}

/* _prof_term(): a is a printable term of 2 to 40 bytes.
*/
static c3_t
_prof_term(u3_atom a)
{
  c3_w met_w = u3r_met(3, a);
  c3_y buf_y[40];
  c3_w i_w;
  c3_t let_t = 0;

  if ( (met_w < 2) || (met_w > 40) ) return 0;
  u3r_bytes(0, met_w, buf_y, a);
  for ( i_w = 0; i_w < met_w; i_w++ ) {
    c3_y c_y = buf_y[i_w];
    if ( (c_y >= 'a') && (c_y <= 'z') ) { let_t = 1; continue; }
    if ( ((c_y >= '0') && (c_y <= '9')) || ('-' == c_y) || ('.' == c_y) ) continue;
    return 0;
  }
  return let_t;
}

/* _prof_names(): the terms in a label, depth first within a budget: a
**                label is a path of names, or for cores the SKA
**                registered, a location (the names nested root last) or
**                a battery (the names of the hints in it).
*/
static void
_prof_names(u3_noun lab, c3_c* buf_c, c3_w* b_w, c3_w* bud_w, c3_w* num_w)
{
  while ( (*bud_w > 0) && (*num_w < 10) && (*b_w < 200) ) {
    (*bud_w)--;
    if ( c3y == u3ud(lab) ) {
      if ( _prof_term(lab) ) {
        c3_w met_w = u3r_met(3, lab);
        buf_c[(*b_w)++] = '/';
        u3r_bytes(0, met_w, (c3_y*)buf_c + *b_w, lab);
        *b_w += met_w;
        (*num_w)++;
      }
      return;
    }
    _prof_names(u3h(lab), buf_c, b_w, bud_w, num_w);
    lab = u3t(lab);
  }
}

typedef struct {
  u3_noun pax;
  c3_d    inc_d;
  c3_d    sef_d;
} _prof_row;

static c3_i
_prof_cmp(const void* a, const void* b)
{
  const _prof_row* x = a;
  const _prof_row* y = b;
  return ( x->inc_d < y->inc_d ) ? 1 : ( x->inc_d > y->inc_d ) ? -1 : 0;
}

/* _prof_dump(): print the profile (+doss) by label: the samples a label
**               was on the stack for, and those it was the innermost.
**               In C: +pi-tell crashes on this profile.
*/
static void
_prof_dump(c3_w top_w)
{
  u3_noun day = u3R->pro.day;
  u3_noun mon, hit, cut, lis, i;
  c3_d    tot_d;
  c3_w    len_w = 0, n_w = 0;
  _prof_row* row_u;

  if ( !day ) {
    fprintf(stderr, "prof: no samples\r\n");
    return;
  }
  u3x_trel(day, &mon, &hit, &cut);
  tot_d = _prof_moth(mon);
  lis   = u3qdb_tap(cut);
  for ( i = lis; u3_nul != i; i = u3t(i) ) len_w++;
  row_u = c3_malloc(sizeof(*row_u) * (len_w + 1));

  for ( i = lis; u3_nul != i; i = u3t(i) ) {
    u3_noun pax = u3h(u3h(i)), hum = u3t(u3h(i));
    u3_noun out = u3qdb_tap(u3h(u3t(hum))), j;
    c3_d    inc_d = _prof_moth(u3h(hum)), cal_d = 0;

    for ( j = out; u3_nul != j; j = u3t(j) ) {
      cal_d += u3r_chub(0, u3t(u3h(j)));
    }
    u3z(out);
    row_u[n_w].pax   = pax;
    row_u[n_w].inc_d = inc_d;
    row_u[n_w].sef_d = ( inc_d > cal_d ) ? inc_d - cal_d : 0;
    n_w++;
  }
  qsort(row_u, n_w, sizeof(*row_u), _prof_cmp);

  fprintf(stderr, "prof: %" PRIu64 " samples (10 ms), %u labels; "
                  "on stack %%, innermost %%, label\r\n", tot_d, n_w);
  for ( c3_w k_w = 0; (k_w < n_w) && (k_w < top_w); k_w++ ) {
    c3_c   buf_c[256];
    c3_w   b_w = 0;

    {
      c3_w bud_w = 4000, num_w = 0;
      _prof_names(row_u[k_w].pax, buf_c, &b_w, &bud_w, &num_w);
    }
    buf_c[b_w] = 0;
    if ( (k_w < 4) && getenv("U3NC_PROF_RAW") ) {
      u3m_p("prof label", row_u[k_w].pax);
    }
    fprintf(stderr, "prof %5.1f %5.1f  %s\r\n",
                    tot_d ? 100.0 * row_u[k_w].inc_d / tot_d : 0.0,
                    tot_d ? 100.0 * row_u[k_w].sef_d / tot_d : 0.0, buf_c);
  }
  c3_free(row_u);
  u3z(lis);
}

/* _time(): wall clock, in microseconds.
*/
static c3_d
_time(void)
{
  struct timeval tim_u;
  gettimeofday(&tim_u, 0);
  return ((c3_d)tim_u.tv_sec * 1000000) + tim_u.tv_usec;
}

/* _nock_n(): .*(bus fol) with u3n interpreter
*/
static u3_noun
_nock_n(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3n_nock_on(bus, fol);
}

/* _nock_nc(): .*(bus fol) with u3nc interpreter
*/
static u3_noun
_nock_nc(u3_noun bus_fol)
{
  u3_noun bus, fol;
  u3x_cell(bus_fol, &bus, &fol);
  u3k(bus); u3k(fol); u3z(bus_fol);
  return u3nc_nock_on(bus, fol);
}

#ifdef U3NC_STAT
/* _run_jet_d: the jet hits of the last run of the compiled interpreter,
**             which, in a second run, are the program's own: a first
**             one compiles, and the SKA core hits jets of its own.
*/
static c3_d _run_jet_d;
#endif

/* _run(): run fun_f on [bus fol] softly, timing it.  RETAINS.
**         Produces the product, or u3_none after printing the trace.
*/
static u3_weak
_run(const c3_c* nam_c, u3_funk fun_f, u3_noun bus, u3_noun fol)
{
#ifdef U3NC_STAT
  u3nc_stat sav_u = u3nc_Stat;
#endif
#ifdef U3NC_STAT
  c3_d    cel_d = u3a_Cel_d;
#endif
  c3_d    beg_d = _time();
  c3_d    tsb_d = _tsc();
  u3_noun gon   = u3m_soft(0, fun_f, u3nc(u3k(bus), u3k(fol)));
  c3_d    tse_d = _tsc();
  c3_d    end_d = _time();
  u3_noun pro;

  (void)tsb_d; (void)tse_d;

  if ( 0 != u3h(gon) ) {
    u3_pier_punt_goof(nam_c, gon);
    return u3_none;
  }

  fprintf(stderr, "%s: %" PRIu64 " us\r\n", nam_c, end_d - beg_d);
#ifdef U3NC_STAT
  //  the cells the run allocated, in either interpreter
  //
  fprintf(stderr, "  alloc %" PRIu64 " cells, %.1f Mc\r\n",
          u3a_Cel_d - cel_d, (tse_d - tsb_d) / 1e6);
#endif

  //  the runtime counters of this run of the compiled interpreter
  //
#ifdef U3NC_STAT
  if ( _nock_nc == fun_f ) {
#define D(f)  (u3nc_Stat.f - sav_u.f)
    _run_jet_d = D(jet_d);
    fprintf(stderr, "  stat con %" PRIu64 " uni %" PRIu64 " tok %" PRIu64
                    " rus %" PRIu64
                    " spa %" PRIu64 " spg %" PRIu64 " spw %" PRIu64
                    " dek %" PRIu64 " deb %" PRIu64
                    " dro %" PRIu64 " drt %" PRIu64
                    " kal %" PRIu64 " gan %" PRIu64 " bor %" PRIu64
                    " bok %" PRIu64 " bot %" PRIu64
                    " dbi %" PRIu64 " ali %" PRIu64 " alx %" PRIu64
                    " dir %" PRIu64 " sub %" PRIu64 " jet %" PRIu64 "\r\n",
            D(con_d), D(uni_d), D(tok_d), D(rus_d), D(spa_d), D(spg_d), D(spw_d),
            D(dek_d), D(deb_d), D(dro_d), D(drt_d),
            D(kal_d), D(gan_d), D(bor_d), D(bok_d), D(bot_d),
            D(dbi_d), D(ali_d), D(alx_d),
            D(dir_d), D(sub_d), D(jet_d));
    //  where the time of the run went, in TSC cycles (Mc: millions):
    //  indirect calls finding their programs, memoized calls making
    //  keys, looking them up and saving products
    //
    fprintf(stderr, "  time run %.1f Mc ent %" PRIu64 " nok %" PRIu64
                    " in %.1f Mc (%.0f c each), %" PRIu64 " entries scanned;"
                    " memo %" PRIu64 " (%" PRIu64 " hits) in %.1f Mc,"
                    " %" PRIu64 " saves in %.1f Mc; jets %" PRIu64
                    " in %.1f Mc\r\n",
            (tse_d - tsb_d) / 1e6, D(ent_d), D(nok_d), D(nok_c) / 1e6,
            D(nok_d) ? (double)D(nok_c) / D(nok_d) : 0.0, D(mat_d),
            D(mem_d), D(meh_d), D(mem_c) / 1e6, D(mes_d), D(mes_c) / 1e6,
            D(jet_d), D(jet_c) / 1e6);
    fprintf(stderr, "  match %" PRIu64 " sock nodes walked, %" PRIu64
                    " known subtrees compared (not the same noun) in %.1f Mc\r\n",
            D(hug_d), D(hus_d), D(hus_c) / 1e6);
#undef D
  }
#endif

  pro = u3k(u3t(gon));
  u3z(gon);
  return pro;
}

/* _lose(): release a product, or nothing.
*/
static void
_lose(u3_weak pro)
{
  if ( u3_none != pro ) {
    u3z(pro);
  }
}

/* _test(): compare u3n and u3nc on [bus fol].  TRANSFERS.
**
**   U3NC_ONLY=n|nc: run only that interpreter (u3nc for any other
**   value), and don't compare: to profile one of them alone.
**   U3NC_REPS=k: k warm runs instead of one,
**   the last compared: to drown the compilation in a profile.
*/
static c3_i
_test(const c3_c* nam_c, u3_noun bus, u3_noun fol, c3_t jet_t)
{
  c3_c*   onl_c = getenv("U3NC_ONLY");
  c3_c*   rep_c = getenv("U3NC_REPS");
  c3_t    n_t   = !onl_c || !strcmp(onl_c, "n");
  c3_t    nc_t  = !onl_c || strcmp(onl_c, "n");
  c3_w    rep_w = ( rep_c && (atoi(rep_c) > 0) ) ? atoi(rep_c) : 1;
  u3_weak a = u3_none, b = u3_none;
  c3_i    ret_i = 1;

  if ( n_t ) {
    u3z(_run("u3n  first ", _nock_n, bus, fol));
  }
  if ( nc_t ) {
    //  U3NC_PROF: vere's sampling profiler over the compiling run, the
    //  samples by the labels of the registered cores they ran in
    //
    c3_t pof_t = !!getenv("U3NC_PROF");

    if ( pof_t ) {
      u3C.wag_h |= u3o_debug_cpu;
      u3t_boot();
    }
    u3z(_run("u3nc first ", _nock_nc, bus, fol));
    if ( pof_t ) {
      u3t_boff();
      u3C.wag_h &= ~u3o_debug_cpu;
      _prof_dump(atoi(getenv("U3NC_PROF")) > 1 ? atoi(getenv("U3NC_PROF")) : 60);
    }
    fprintf(stderr, "  slogs: %" PRIu64 " printed in %.3f s\r\n",
                    _slog_d[0], _slog_d[1] / 1.99e9);
#ifdef U3NC_STAT
    if ( getenv("U3NC_PROF_ARMS") ) {
      for ( c3_w p_w = 0; p_w < U3WE_PROF_N; p_w++ ) {
        fprintf(stderr, "  arm %-26s %9.3f s in %7" PRIu64 " outer calls, %8" PRIu64 " calls%s\r\n",
                        u3we_prof_nam_c[p_w], u3we_prof_d[p_w][0] / 1.99e9,
                        u3we_prof_d[p_w][1], u3we_prof_d[p_w][2],
                        u3we_prof_dep_w[p_w] ? " (unbalanced)" : "");
      }
    }
#endif
  }

  while ( rep_w-- ) {
    _lose(a); _lose(b);
    a = n_t  ? _run("u3n  second", _nock_n, bus, fol) : u3_none;
    b = nc_t ? _run("u3nc second", _nock_nc, bus, fol) : u3_none;
  }

  if ( (n_t && (u3_none == a)) || (nc_t && (u3_none == b)) ) {
    fprintf(stderr, "test %s: crashed\r\n", nam_c);
    ret_i = 0;
  }
  else if ( !n_t || !nc_t ) {
    fprintf(stderr, "test %s: ran (%s only, not compared)\r\n",
                    nam_c, n_t ? "u3n" : "u3nc");
  }
  else if ( c3n == u3r_sing(a, b) ) {
    fprintf(stderr, "test %s: mismatch\r\n", nam_c);
    u3m_p("u3n ", a);
    u3m_p("u3nc", b);
    ret_i = 0;
  }
  else {
    fprintf(stderr, "test %s: ok\r\n", nam_c);
  }

  _lose(a); _lose(b);
  u3z(bus); u3z(fol);
  return ret_i;
}

/* _gate(): a gate from hoon source, built on the kernel core, with its
**          sample set.  TRANSFERS.
*/
static u3_noun
_gate(const c3_c* src_c, u3_noun sam)
{
  u3_noun gat = u3v_wish(src_c);
  u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(sam, u3k(u3t(u3t(gat)))));
  u3z(gat);
  return cor;
}

/* _test_ackermann(): ackermann 3 8, with dec from the standard library.
*/
static c3_i
_test_ackermann(void)
{
  u3_noun cor = _gate("|=  [m=@ n=@]\n"
                      "^-  @\n"
                      "?:  =(0 m)  +(n)\n"
                      "?:  =(0 n)  $(m (dec m), n 1)\n"
                      "$(m (dec m), n $(n (dec n)))\n",
                      u3nc(3, 8));

  return _test("ackermann", cor, u3nq(9, 2, 0, 1), 1);
}

static c3_i
_test_iter_dec(void)
{
  u3_noun list = u3_nul;
  for (c3_w i_w = 0; i_w < 10000; i_w++) list = u3nc(1, list);

  u3_noun cor = _gate("|=  l=(list @)\n"
                      "^-  (list @)\n"
                      "?~  l  ~\n"
                      "[(dec i.l) $(l t.l)]\n",
                      list);

  return _test("iterate dec", cor, u3nq(9, 2, 0, 1), 1);
}

static c3_i
_test_iter_vint(void)
{
  u3_noun list = u3_nul;
  for (c3_w i_w = 0; i_w < 10000; i_w++) list = u3nc(1, list);

  u3_noun cor = _gate("|=  l=(list @)\n"
                      "^-  (list @)\n"
                      "?~  l  ~\n"
                      "[+(i.l) $(l t.l)]\n",
                      list);

  return _test("iterate vint", cor, u3nq(9, 2, 0, 1), 1);
}

static c3_i
_test_iter_add(void)
{
  u3_noun list = u3_nul;
  for (c3_w i_w = 0; i_w < 10000; i_w++) list = u3nc(i_w, list);

  u3_noun cor = _gate("|=  l=(list @)\n"
                      "^-  (list @)\n"
                      "?~  l  ~\n"
                      "[(add i.l 1) $(l t.l)]\n",
                      list);

  return _test("iterate add", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_scot_tail(): a jet that punts (+scot on %p), in tail position: JSP,
**                    then the unary program of the callee.
*/
static c3_i
_test_scot_tail(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @t\n"
                      "(scot %p n)\n",
                      123456789);

  return _test("scot tail", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_scot(): a jet that hits (+scot on %ud) and one that punts (%p),
**               not in tail position: CAP.
*/
static c3_i
_test_scot(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  [@t @t]\n"
                      "[(scot %ud n) (scot %p n)]\n",
                      123456789);

  return _test("scot", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_scot_loop(): a loop of punting +scot calls (%p, planets), long
**                    enough to profile: U3NC_SCOT_N iterations, default
**                    100000 under U3NC_BENCH and 1000 otherwise.  The
**                    product sums the lengths of the cords.
*/
static c3_i
_test_scot_loop(void)
{
  c3_c*   len_c = getenv("U3NC_SCOT_N");
  c3_w    len_w = len_c ? atoi(len_c) : ( getenv("U3NC_BENCH") ? 100000 : 1000 );
  u3_noun cor   = _gate("|=  n=@\n"
                        "^-  @\n"
                        "=/  i  0\n"
                        "=/  acc  0\n"
                        "|-\n"
                        "?:  =(i n)  acc\n"
                        "$(i +(i), acc (add acc (met 3 (scot %p (add i 123.456.789)))))\n",
                        u3i_word(len_w));

  return _test("scot loop", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_memo_toss(): transient memoization (~+), keyed by the parts of
**                    the subject the callee uses: CAM.
*/
static c3_i
_test_memo_toss(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @\n"
                      "~+\n"
                      "?:  =(0 n)  0\n"
                      "+($(n (dec n)))\n",
                      1000);

  return _test("memo toss", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_memo_keep(): persistent memoization (%memo./path), keyed by the
**                    whole subject: CSM.  The cache is shared with u3n, so
**                    u3nc runs first on a fresh sample to miss it, and
**                    then u3n is checked against what u3nc saved.
*/
static c3_i
_test_memo_keep(void)
{
  const c3_c* src_c = "|=  n=@\n"
                      "^-  @\n"
                      "~>  %memo./ska\n"
                      "?:  =(0 n)  0\n"
                      "+($(n (dec n)))\n";
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_noun cor = _gate(src_c, 1000);
  u3_weak pro = _run("u3nc fresh ", _nock_nc, cor, fol);

  if ( u3_none == pro ) {
    u3z(cor); u3z(fol);
    return 0;
  }

  u3z(pro);
  u3z(fol);
  return _test("memo keep", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_memo_jet(): persistent memoization of a formula that is also a
**                   jetted arm: CSM still gets the whole subject, not the
**                   first of the registers of the jet's need.
*/
static c3_i
_test_memo_jet(void)
{
  u3_noun gat = u3v_wish("dec");
  u3_noun cor = u3nt(u3k(u3h(gat)), 1001, u3k(u3t(u3t(gat))));
  u3_noun hin = u3nt(11,
                     u3nt(c3__memo, 1, u3nc(c3__ska, u3_nul)),
                     u3k(u3h(gat)));
  u3_noun fol = u3nq(2, u3nc(0, 1), 1, hin);
  u3_weak pro = _run("u3nc fresh ", _nock_nc, cor, fol);

  u3z(gat);

  if ( u3_none == pro ) {
    fprintf(stderr, "test memo jet: crashed\r\n");
    u3z(cor); u3z(fol);
    return 0;
  }

  u3z(pro);
  return _test("memo jet", cor, fol, 1);
}

/* _test_memo_mono(): persistent memoization right in a function compiled as
**                    one of its whole subject: CSM there too.
*/
static c3_i
_test_memo_mono(void)
{
  u3_noun fol = u3nt(11,
                     u3nt(c3__memo, 1, u3nc(c3__ska, u3_nul)),
                     u3nt(4, 0, 2));
  u3_noun bus = u3nc(1001, 7);
  u3_weak pro = _run("u3nc fresh ", _nock_nc, bus, fol);

  if ( u3_none == pro ) {
    fprintf(stderr, "test memo mono: crashed\r\n");
    u3z(bus); u3z(fol);
    return 0;
  }

  u3z(pro);
  return _test("memo mono", bus, fol, 1);
}

/* _test_dies(): a value live into one arm of a branch only: dropped at
**               the start of the other arm.  m is an indirect atom, so
**               the drop (and every consume of x) moves a real reference.
*/
static c3_i
_test_dies(void)
{
  u3_noun cor = _gate("|=  [n=@ m=@]\n"
                      "^-  @\n"
                      "=/  x  (add n m)\n"
                      "?:  =(0 (mod n 2))  x\n"
                      "$(n (dec n))\n",
                      u3nc(7, u3i_string("an atom long enough to be indirect")));

  return _test("dies", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_twice(): a register read twice by one op, consumed by one read:
**                [t t] in CON, (add b b) in ADD, and c twice among the
**                arguments of a tail call.
*/
static c3_i
_test_twice(void)
{
  u3_noun cor = _gate("|=  [a=@ b=@ c=@]\n"
                      "^-  *\n"
                      "=/  t=*  0\n"
                      "|-  ^-  *\n"
                      "?:  =(0 a)  [t (add b b) c]\n"
                      "$(a (dec a), b c, c c, t [t t])\n",
                      u3nt(10, 3, 5));

  return _test("twice", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_cycle(): the arguments of a self call in each other's parameter
**                slots, a rotation of three: the moves that put them in
**                place need a slot to step aside into, and the program
**                has none to spare, so it gets one more.  Indirect atoms,
**                so that every move carries a real reference.
*/
static c3_i
_test_cycle(void)
{
  u3_noun cor = _gate("|=  [a=@ b=@ c=@ n=@]\n"
                      "^-  [@ @ @]\n"
                      "?:  =(0 n)  [a b c]\n"
                      "$(a b, b c, c a, n (dec n))\n",
                      u3nq(u3i_string("the first of three indirect atoms"),
                           u3i_string("the second of three indirect atoms"),
                           u3i_string("the third of three indirect atoms"),
                           7));

  return _test("cycle", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_swap(): two arguments of a self call in each other's slots, and
**               a temporary above the parameters that is dead by the
**               call: the one that steps aside takes its slot.  The other
**               arm of the branch passes every argument in place.
*/
static c3_i
_test_swap(void)
{
  u3_noun cor = _gate("|=  [a=@ b=@ n=@]\n"
                      "^-  [@ @]\n"
                      "?:  =(0 n)  [a b]\n"
                      "=/  m  (dec n)\n"
                      "?:  =(0 (mod m 2))  $(a a, b b, n m)\n"
                      "$(a b, b a, n m)\n",
                      u3nt(u3i_string("the first of two indirect atoms"),
                           u3i_string("the second of two indirect atoms"),
                           7));

  return _test("swap", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_partial(): a register that dies on one edge into a block that
**                   another edge reaches with it consumed already: y is
**                   indirect and read only by the second test of the ?&,
**                   so where the first test fails it dies on the edge to
**                   the no arm, and where the second test runs it is
**                   consumed before the same arm.  Both happen in the loop.
*/
static c3_i
_test_partial(void)
{
  u3_noun cor = _gate("|=  [b=@ n=@]\n"
                      "^-  @\n"
                      "=/  acc  0\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "=/  y  (add b n)\n"
                      "=.  acc\n"
                      "  ?:  ?&(=(0 (mod n 2)) =(0 (mod y 3)))\n"
                      "    (add acc 1)\n"
                      "  (add acc 2)\n"
                      "$(n (dec n))\n",
                      u3nc(u3i_string("an atom long enough to be indirect, yes"), 9));

  return _test("partial", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_equal(): branches on the equality of two registers, indirect
**                atoms in a loop.  The first, a test of a ?| whose other
**                test the yes block shares, has one source read past it
**                only by that other test, on the no side: the EQQ
**                consumes it on the fall-through.  The last, in tail
**                position, has each source read past it in one arm only:
**                the EQQ consumes one on each edge.
*/
static c3_i
_test_equal(void)
{
  u3_noun cor = _gate("|=  [a=@ n=@]\n"
                      "^-  @\n"
                      "=/  acc  0\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "=/  v  (add n a)\n"
                      "=/  w  (add a 2)\n"
                      "=.  acc\n"
                      "  ?:  ?|(=(v w) =(0 (mod v 2)))\n"
                      "    (add acc 1)\n"
                      "  (add acc w)\n"
                      "=/  y  (add a n)\n"
                      "=/  z  (add a 3)\n"
                      "?:  =(y z)\n"
                      "  $(n (dec n), acc (add acc y))\n"
                      "$(n (dec n), acc (add acc z))\n",
                      u3nc(u3i_string("an atom long enough to be indirect, yes"), 5));

  return _test("equal", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_fork(): a ?: in value position whose arms are autoconses of
**               indirect atoms, taken apart past it.  The IR materializes
**               the loobean and tests it again in every block that reads
**               the product, each arm fulfilling the reads its own way;
**               a register defined in one arm and read past the join is
**               left undefined (kil) on the other path, and its drop
**               there must find nothing to release.
*/
static c3_i
_test_fork(void)
{
  u3_noun cor = _gate("|=  [a=@ n=@]\n"
                      "^-  @\n"
                      "=/  acc  0\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "=/  y  (add a n)\n"
                      "=/  p  ?:(=(y (add a 3)) [(add y 1) 1] [2 (add y 2)])\n"
                      "?:  =(0 (mod n 2))\n"
                      "  $(n (dec n), acc (add acc -.p))\n"
                      "$(n (dec n), acc (add acc +.p))\n",
                      u3nc(u3i_string("an atom long enough to be indirect, yes"), 5));

  return _test("fork", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_unused(): the product of a call that is never used (but may
**                 crash, so the call stays): dropped right after it.
*/
static c3_i
_test_unused(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @\n"
                      "=/  x  (dec n)\n"
                      "=/  y  (scot %ud n)\n"
                      "+(n)\n",
                      5);

  return _test("unused", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_bignum(): an indirect atom accumulated in a loop, consumed at
**                 each step by the jet call that replaces it.
*/
static c3_i
_test_bignum(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  @\n"
                      "=/  acc=@  1\n"
                      "|-  ^-  @\n"
                      "?:  =(0 n)  acc\n"
                      "$(n (dec n), acc (mul acc 3))\n",
                      200);

  return _test("bignum", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_rebuild(): a core edited and rebuilt each iteration, with its
**                  parts deconstructed and consed back.
*/
static c3_i
_test_rebuild(void)
{
  u3_noun cor = _gate("|=  [n=@ acc=[p=@ q=@ r=(list @)]]\n"
                      "^-  [p=@ q=@ r=(list @)]\n"
                      "?:  =(0 n)  acc\n"
                      "$(n (dec n), acc acc(q +(q.acc), r [p.acc r.acc]))\n",
                      u3nc(50, u3nt(1, 2, u3_nul)));

  return _test("rebuild", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_owned(): a list built and transformed within one computation, so
**                 that its cells are uniquely owned when deconstructed:
**                 a tail-recursive map that conses as it takes apart, and
**                 a non-tail one that conses after the recursive call.
*/
static c3_i
_test_owned(void)
{
  u3_noun cor = _gate("|=  n=@\n"
                      "^-  [(list @) (list @)]\n"
                      "=/  l=(list @)  ~\n"
                      "=.  l  |-  ^-  (list @)\n"
                      "       ?:  =(0 n)  l\n"
                      "       $(n (dec n), l [n l])\n"
                      "=/  m=(list @)  ~\n"
                      "=.  m  |-  ^-  (list @)\n"
                      "       ?:  =(0 n)  m\n"
                      "       $(n (dec n), m [n m])\n"
                      ":-  =/  acc=(list @)  ~\n"
                      "    |-  ^-  (list @)\n"
                      "    ?~  l  acc\n"
                      "    $(l t.l, acc [+(i.l) acc])\n"
                      "|-  ^-  (list @)\n"
                      "?~  m  ~\n"
                      "[+(i.m) $(m t.m)]\n",
                      10000);

  return _test("owned", cor, u3nq(9, 2, 0, 1), 1);
}

/* _test_sew_one(): run src on sam through both interpreters, and, with the
**                  runtime counters, check that u3nc edited an atom in
**                  place mut_d times (over its two runs).
*/
static c3_i
_test_sew_one(const c3_c* nam_c, const c3_c* src_c, u3_noun sam, c3_d mut_d)
{
  c3_i ret_i;
#ifdef U3NC_STAT
  c3_d old_d = u3nc_Stat.mut_d;
#endif

  ret_i = _test(nam_c, _gate(src_c, sam), u3nq(9, 2, 0, 1), 1);

#ifdef U3NC_STAT
  if ( (u3nc_Stat.mut_d - old_d) != mut_d ) {
    fprintf(stderr, "test %s: %" PRIu64 " edits in place, expected %" PRIu64
                    "\r\n", nam_c, u3nc_Stat.mut_d - old_d, mut_d);
    ret_i = 0;
  }
#else
  (void)mut_d;
#endif

  return ret_i;
}

/* _test_sew(): +sew through its transferring driver (CAX/JMX).  A buffer
**              filled byte by byte, consumed at each call, so the driver
**              edits it in place; the same with the buffer borrowed (live
**              after the call), and with a second reference to it (the
**              previous buffer kept), where it must copy; and the edge
**              cases of the edit in place: a write reaching past the
**              buffer's words, writes that clear its top word down to a
**              shorter atom, a direct one and zero, bit and word bloqs,
**              spans across word boundaries, a value wider than its span,
**              an empty span, and a tail call.  The oracles are +rep and
**              +cut, and the copying driver that u3n runs.
*/
static c3_i
_test_sew(void)
{
  c3_i ret_i = 1;

  ret_i = _test_sew_one("sew fill",
                        "|=  n=@\n"
                        "=/  buf=@  (bex (mul 8 n))\n"
                        "=/  i=@  0\n"
                        "=/  out=@\n"
                        "  |-  ^-  @\n"
                        "  ?:  =(i n)  buf\n"
                        "  $(i +(i), buf (sew 3 [i 1 (end 3 i)] buf))\n"
                        "?>  =(out (add (bex (mul 8 n)) (rep 3 (gulf 0 (dec n)))))\n"
                        "out\n",
                        4096, 2 * 4096) && ret_i;

  ret_i = _test_sew_one("sew borrowed",
                        "|=  n=@\n"
                        "=/  buf=@  (bex (mul 8 n))\n"
                        "=/  i=@  0\n"
                        "=/  acc=@  0\n"
                        "|-  ^-  @\n"
                        "?:  =(i n)  ?>(=(0 acc) buf)\n"
                        "=/  x=@  (sew 3 [i 1 (end 3 i)] buf)\n"
                        "$(i +(i), acc (mix acc (cut 3 [i 1] x)))\n",
                        4096, 0) && ret_i;

  ret_i = _test_sew_one("sew shared",
                        "|=  n=@\n"
                        "=/  buf=@  (bex (mul 8 n))\n"
                        "=/  old=@  0\n"
                        "=/  i=@  0\n"
                        "|-  ^-  [@ @]\n"
                        "?:  =(i n)\n"
                        "  ?>  =(buf (add (bex (mul 8 n)) (rep 3 (gulf 0 (dec n)))))\n"
                        "  ?>  =(old (add (bex (mul 8 n)) (rep 3 (gulf 0 (sub n 2)))))\n"
                        "  [old buf]\n"
                        "$(i +(i), old buf, buf (sew 3 [i 1 (end 3 i)] buf))\n",
                        4096, 0) && ret_i;

  ret_i = _test_sew_one("sew edges",
                        "|=  n=@\n"
                        "=/  w=@  (mul 64 n)\n"
                        ":*  (sew 3 [8 1 0xff] (bex w))\n"
                        "    (sew 3 [12 1 0xff] (bex w))\n"
                        "    (sew 3 [8 1 0] (bex w))\n"
                        "    (sew 3 [8 1 0] (add (bex w) 5))\n"
                        "    (sew 3 [8 1 0] (add (bex w) (bex 40)))\n"
                        "    (sew 0 [31 2 0b11] (bex w))\n"
                        "    (sew 5 [1 1 0xdead.beef] (bex w))\n"
                        "    (sew 3 [1 1 0x1234] (bex w))\n"
                        "    (sew 3 [0 8 (bex (add w 6))] (bex w))\n"
                        "    (sew 3 [0 0 0xff] (bex w))\n"
                        "    (sew 4 [2 3 (dec (bex 48))] (bex w))\n"
                        "    (sew 3 [7 2 0xffff] (bex w))\n"
                        "    (sew 3 [0 9 (dec (bex 72))] (bex w))\n"
                        "    =/  e=@  (bex w)\n"
                        "    [(sew 3 [8 1 0xff] e) e]\n"
                        "==\n",
                        1, 2 * 11) && ret_i;

  ret_i = _test_sew_one("sew tail",
                        "|=  n=@\n"
                        "(sew 3 [8 1 0xff] (bex (mul 64 n)))\n",
                        1, 2 * 1) && ret_i;

  return ret_i;
}

/* _fast_n(): the gate cor kicked through u3n's jet dashboard: ~ if
**            u3j_kick() finds no jet, else its product and that of the
**            %fun hook on the gate.  For u3m_soft().
*/
static u3_noun
_fast_n(u3_noun cor_fol)
{
  u3_noun cor, fol, kic;

  u3x_cell(cor_fol, &cor, &fol);
  u3k(cor); u3z(cor_fol);

  if ( u3_none == (kic = u3j_kick(u3k(cor), 2)) ) {
    u3z(cor);
    return u3_nul;
  }

  return u3nc(kic, u3j_hook(cor, "fun"));
}

/* _test_fast(): a %fast hint run only by u3nc: a gate labeled as the
**               kernel's dec, built over its layer, with a hook on its
**               arm.  The analysis registers it, and u3n jets and hooks
**               it through the registration %ir carries back.
*/
static c3_i
_test_fast(void)
{
  u3_noun cor = _gate("|=  *\n"
                      "=>  ..add\n"
                      "~%  %dec  +>\n"
                      "  ==\n"
                      "    %fun  $\n"
                      "  ==\n"
                      "|=  a=@\n"
                      "(sub a 1)\n",
                      0);
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_weak gat = _run("u3nc fast  ", _nock_nc, cor, fol);
  u3_noun dec, pro;
  c3_i    ret_i = 1;

  u3z(cor); u3z(fol);

  if ( u3_none == gat ) {
    fprintf(stderr, "test fast: crashed\r\n");
    return 0;
  }

  //  with a sample, as u3n meets it
  //
  dec = u3nt(u3k(u3h(gat)), 42, u3k(u3t(u3t(gat))));
  u3z(gat);
  pro = _run("u3n  fast  ", _fast_n, dec, u3_nul);

  if ( u3_none == pro ) {
    fprintf(stderr, "test fast: crashed\r\n");
    ret_i = 0;
  }
  else if ( u3_nul == pro ) {
    fprintf(stderr, "test fast: not jetted by u3n\r\n");
    ret_i = 0;
  }
  else {
    u3_noun exp = u3nc(41, 41);

    if ( c3n == u3r_sing(pro, exp) ) {
      fprintf(stderr, "test fast: mismatch\r\n");
      u3m_p("u3n ", pro);
      ret_i = 0;
    }
    else {
      fprintf(stderr, "test fast: ok\r\n");
    }
    u3z(exp);
    u3z(pro);
  }

  u3z(dec);
  return ret_i;
}

/* _flags: flags to set on the arms of the cores with a name.
*/
typedef struct {
  const c3_c* nam_c;
  c3_o        liv;
  c3_o        ice;
} _flags;

/* _flags_cb(): u3h_walk callback over the rings: set the flags in ptr_v.
*/
static void
_flags_cb(u3_noun kev, void* ptr_v)
{
  const _flags*   fla_u = ptr_v;
  u3j_harm*       ham_u;
  const u3u_harm* arm_u;

  if (  (c3y == u3r_sing_c(fla_u->nam_c, u3h(u3h(u3h(kev)))))
     && (c3y == u3j_ring(u3h(kev), &ham_u, &arm_u)) )
  {
    ham_u->liv = fla_u->liv;
    ham_u->ice = fla_u->ice;
  }
}

/* _set_flags(): set the flags and feed the core the drivers there are now.
*/
static void
_set_flags(const c3_c* nam_c, c3_o liv, c3_o ice)
{
  _flags fla_u = { nam_c, liv, ice };

  u3h_walk_with(u3H->rod_u.ska.pax_p, _flags_cb, &fla_u);
  u3d_hot();
}

/* _off_cb(): u3h_walk callback over the rings: turn off (liv) every arm
**            under a core whose name is in ptr_v, a comma-separated string
**            with a leading and a trailing comma.
*/
static c3_w _off_w;

static void
_off_cb(u3_noun kev, void* ptr_v)
{
  const c3_c*     off_c = ptr_v;
  u3_noun         pax   = u3h(u3h(kev));
  u3j_harm*       ham_u;
  const u3u_harm* arm_u;

  for ( ; u3_nul != pax; pax = u3t(pax) ) {
    c3_c* nam_c = u3r_string(u3h(pax));
    c3_c  key_c[64];
    c3_t  hit_t;

    snprintf(key_c, sizeof(key_c), ",%s,", nam_c);
    hit_t = !!strstr(off_c, key_c);
    c3_free(nam_c);

    if ( hit_t && (c3y == u3j_ring(u3h(kev), &ham_u, &arm_u)) ) {
      ham_u->liv = c3n;
      _off_w++;
      return;
    }
  }
}

/* _test_flags(): the flags of a jet arm hold for the compiled calls
**                through its ring: +scot off, its calls run its program;
**                +scot not perfect, its driver is tested against nock,
**                and its punt (on %p) leaves the subject to the caller.
*/
static c3_i
_test_flags(void)
{
  c3_i ret_i = 1;

  _set_flags("scot", c3n, c3y);
  ret_i = _test_scot() && ret_i;

  _set_flags("scot", c3y, c3n);
  ret_i = _test_scot() && ret_i;

  _set_flags("scot", c3y, c3y);

  fprintf(stderr, "test flags: %s\r\n", ret_i ? "ok" : "failed");
  return ret_i;
}

/* _check(): report a condition.
*/
static c3_i
_check(const c3_c* nam_c, c3_t cnd_t)
{
  fprintf(stderr, "test %s: %s\r\n", nam_c, cnd_t ? "ok" : "failed");
  return cnd_t;
}

/* _has_entry(): is there an entry program for [bus fol]?  RETAINS.
*/
static c3_t
_has_entry(u3_noun bus, u3_noun fol)
{
  u3_weak lis = u3h_git(u3R->ska.ent_p, fol);

  return (u3_none != lis) && (u3_none != u3d_match(bus, lis));
}

/* _links: a walk over the programs of the road, first collecting them, then
**         counting the links to programs that aren't among them.
*/
typedef struct {
  u3p(u3h_root) pog_p;
  c3_t          chk_t;
  c3_w          bad_w;
} _links;

static void
_links_prog(_links* lin_u, u3_noun pog)
{
  u3nc_prog* pog_u = u3to(u3nc_prog, pog << u3a_vits);

  if ( !lin_u->chk_t ) {
    u3h_put(lin_u->pog_p, pog, u3_nul);
    return;
  }

  for ( c3_h i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3p(u3nc_prog) gop_p = pog_u->dir_u.dat_u[i_h].pog_p;

    if ( gop_p && (u3_none == u3h_git(lin_u->pog_p, gop_p >> u3a_vits)) ) {
      lin_u->bad_w++;
    }
  }
}

static void
_links_dir_cb(u3_noun kev, void* ptr_v)
{
  _links_prog(ptr_v, u3t(kev));
}

static void
_links_ent_cb(u3_noun kev, void* ptr_v)
{
  for ( u3_noun lis = u3t(kev); u3_nul != lis; lis = u3t(lis) ) {
    _links_prog(ptr_v, u3t(u3h(lis)));
  }
}

/* _links_ok(): no program is linked to a program that is gone.
*/
static c3_t
_links_ok(void)
{
  _links lin_u = { u3h_new(), 0, 0 };

  u3h_walk_with(u3R->ska.dir_p, _links_dir_cb, &lin_u);
  u3h_walk_with(u3R->ska.ent_p, _links_ent_cb, &lin_u);
  lin_u.chk_t = 1;
  u3h_walk_with(u3R->ska.dir_p, _links_dir_cb, &lin_u);
  u3h_walk_with(u3R->ska.ent_p, _links_ent_cb, &lin_u);

  u3h_free(lin_u.pog_p);
  return !lin_u.bad_w;
}

/* _test_hot(): turn off and on the jets of the cores named nam_c: with each
**              change the programs of a function that calls them go, and
**              those of one that doesn't stay; no program is left linked to
**              a dropped one; what is compiled again agrees with u3n.  No
**              change changes nothing.  src_c: source of a gate calling
**              them, on sample sam.
*/
static c3_i
_test_hot(const c3_c* nam_c, const c3_c* src_c, u3_noun sam)
{
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_noun cor = _gate(src_c, sam);
  u3_noun oth = _gate("|=  n=@\n^-  @\n+(n)\n", 5);
  c3_i    ret_i = 1;

  for ( c3_w i_w = 0; i_w < 2; i_w++ ) {
    c3_w wyt_w;

    u3z(_run("hot, caller", _nock_nc, cor, fol));
    u3z(_run("hot, other ", _nock_nc, oth, fol));
    wyt_w = u3h_wyt(u3R->ska.dir_p);

    _set_flags(nam_c, ( 0 == i_w ) ? c3n : c3y, c3y);
    fprintf(stderr, "test hot: %" PRIc3_w " of %" PRIc3_w " direct programs "
                    "left\r\n", (c3_w)u3h_wyt(u3R->ska.dir_p), wyt_w);

    ret_i = _check("hot: caller dropped", !_has_entry(cor, fol)) && ret_i;
    ret_i = _check("hot: other kept", _has_entry(oth, fol)) && ret_i;
    ret_i = _check("hot: links", _links_ok()) && ret_i;
    ret_i = _test("hot", u3k(cor), u3k(fol), 1) && ret_i;
  }

  //  once the registrations made meanwhile are settled
  //
  u3d_hot();

  {
    u3_noun old   = u3R->ska.cor;
    c3_w    wyt_w = u3h_wyt(u3R->ska.dir_p);

    u3d_hot();
    ret_i = _check("hot: no change", (old == u3R->ska.cor)
                                  && (wyt_w == u3h_wyt(u3R->ska.dir_p))
                                  && _has_entry(cor, fol)) && ret_i;
  }

  u3z(cor); u3z(oth); u3z(fol);
  return ret_i;
}

/* _rings: a walk over the programs of the road, counting the call sites of
**         a formula that go through a ring.
*/
typedef struct {
  u3_noun fol;
  c3_w    num_w;
} _rings;

static void
_rings_prog(_rings* rin_u, u3_noun pog)
{
  u3nc_prog* pog_u = u3to(u3nc_prog, pog << u3a_vits);

  for ( c3_h i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);

    if (  (u3_nul != dir_u->ring)
       && (c3y == u3r_sing(rin_u->fol, u3t(dir_u->bell))) )
    {
      rin_u->num_w++;
    }
  }
}

static void
_rings_dir_cb(u3_noun kev, void* ptr_v)
{
  _rings_prog(ptr_v, u3t(kev));
}

static void
_rings_ent_cb(u3_noun kev, void* ptr_v)
{
  for ( u3_noun lis = u3t(kev); u3_nul != lis; lis = u3t(lis) ) {
    _rings_prog(ptr_v, u3t(u3h(lis)));
  }
}

static c3_w
_rings_of(u3_noun fol)
{
  _rings rin_u = { fol, 0 };

  u3h_walk_with(u3R->ska.dir_p, _rings_dir_cb, &rin_u);
  u3h_walk_with(u3R->ska.ent_p, _rings_ent_cb, &rin_u);

  return rin_u.num_w;
}

/* _test_rung(): a core registered after a function calling it was analyzed.
**               Until the next u3d_hot() the function has no ring, so what
**               calls it compiles the same as before, even newly; then its
**               callers are dropped, and compile through the ring.
*/
static c3_i
_test_rung(void)
{
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_noun cor = _gate("|=  n=@\n"
                      "=/  g  =>(..add |=(a=@ (sub +(a) 2)))\n"
                      "(g n)\n", 5);
  u3_noun nex = _gate("|=  n=@\n"
                      "=/  g  =>(..add |=(a=@ (sub +(a) 2)))\n"
                      "+((g n))\n", 5);
  u3_noun oth = _gate("|=  n=@\n^-  @\n(mul n n)\n", 5);
  u3_noun reg = _gate("|=  *\n"
                      "=>  ..add\n"
                      "~%  %dec  +>  ~\n"
                      "|=  a=@\n"
                      "(sub +(a) 2)\n", 0);
  u3_noun gat, bat;
  c3_i    ret_i = 1;

  u3z(_run("rung, caller", _nock_nc, cor, fol));
  u3z(_run("rung, other ", _nock_nc, oth, fol));

  //  u3n registers the core; the compilation of the next function feeds
  //  the registration to the SKA core
  //
  gat = _run("rung, hint  ", _nock_n, reg, fol);
  bat = u3k(u3h(gat));
  u3z(gat);
  ret_i = _check("rung: no ring yet", 0 == _rings_of(bat)) && ret_i;

  u3z(_run("rung, newer ", _nock_nc, nex, fol));
  ret_i = _check("rung: the same after the registration",
                 _has_entry(cor, fol) && (0 == _rings_of(bat))) && ret_i;

  u3d_hot();
  ret_i = _check("rung: callers dropped", !_has_entry(cor, fol)
                                       && !_has_entry(nex, fol)) && ret_i;
  ret_i = _check("rung: other kept", _has_entry(oth, fol)) && ret_i;
  ret_i = _check("rung: links", _links_ok()) && ret_i;

  ret_i = _test("rung, ring", u3k(cor), u3k(fol), 1) && ret_i;
  ret_i = _check("rung: through the ring", 0 < _rings_of(bat)) && ret_i;
  ret_i = _test("rung, ring, newer", u3k(nex), u3k(fol), 1) && ret_i;

  u3z(cor); u3z(nex); u3z(oth); u3z(reg); u3z(fol); u3z(bat);
  return ret_i;
}

/* _test_boot(): u3d_boot() over an installed core.  With the pill's version
**               the core and the programs stay, and a run of a function
**               compiled before doesn't poke the core.  With another version
**               the core is the pill's +load of the installed one: the pill's
**               arms, the state of the installed one, no programs.
*/
static c3_i
_test_boot(void)
{
  u3_noun fol = u3nq(9, 2, 0, 1);
  u3_noun cor = _gate("|=  n=@\n^-  @\n+(n)\n", 5);
  u3_noun old, lon, pek, ver;
  c3_w    wyt_w;
  c3_i    ret_i = 1;

  u3z(_run("boot, first", _nock_nc, cor, fol));

  old   = u3R->ska.cor;
  wyt_w = u3h_wyt(u3R->ska.dir_p);

  {
    c3_d beg_d = _time();

    u3_mars_steel();
    fprintf(stderr, "boot, same version: %" PRIu64 " us\r\n", _time() - beg_d);
  }

  ret_i = _check("boot, same version: core", old == u3R->ska.cor) && ret_i;
  ret_i = _check("boot, same version: programs",
                 (wyt_w == u3h_wyt(u3R->ska.dir_p))
                 && _has_entry(cor, fol)) && ret_i;

  u3z(_run("boot, again", _nock_nc, cor, fol));
  ret_i = _check("boot, same version: no poke", old == u3R->ska.cor) && ret_i;

  //  an installed core whose +peek, at axis 22, gives another version; its
  //  state [%0 long-ska jets-hot cache] is at axis 14, long-ska at 58
  //
  lon = u3k(u3x_at(58, u3R->ska.cor));
  pek = u3k(u3x_at(22, u3R->ska.cor));
  ver = u3nc(1, u3nt(u3nc(1, u3nt(u3_nul, c3__ud, 99)), 0, 0));
  u3R->ska.cor = u3i_edit(u3R->ska.cor, 22, ver);

  u3_mars_steel();
  ret_i = _check("boot, other version: arms",
                 c3y == u3r_sing(pek, u3x_at(22, u3R->ska.cor))) && ret_i;
  ret_i = _check("boot, other version: state",
                 c3y == u3r_sing(lon, u3x_at(58, u3R->ska.cor))) && ret_i;
  ret_i = _check("boot, other version: programs",
                 (0 == u3h_wyt(u3R->ska.dir_p))
                 && !_has_entry(cor, fol)) && ret_i;

  u3z(lon); u3z(pek);

  ret_i = _test("boot, loaded", u3k(cor), u3k(fol), 1) && ret_i;

  //  the empty pill, (jam 0): no core, the installed one dropped with its
  //  programs, and u3n runs; the binary's pill installs one again
  //
  u3d_boot(2);
  ret_i = _check("boot, empty pill: no core",
                 (u3_nul == u3R->ska.cor) && (c3n == u3nc_able())
                 && (0 == u3h_wyt(u3R->ska.dir_p))
                 && (u3_nul == u3R->ska.new)) && ret_i;
  ret_i = _test("boot, empty pill", u3k(cor), u3k(fol), 1) && ret_i;
  ret_i = _check("boot, empty pill: still no core",
                 (u3_nul == u3R->ska.cor) && (u3_nul == u3R->ska.new))
       && ret_i;

  u3_mars_steel();
  ret_i = _check("boot, after the empty pill: core",
                 u3_nul != u3R->ska.cor) && ret_i;
  ret_i = _test("boot, after the empty pill", cor, fol, 1) && ret_i;

  return ret_i;
}

/* _crash(): the raw trace of [bus fol] crashing under fun_f, or u3_none
**           if it doesn't crash.  RETAINS.
*/
static u3_weak
_crash(u3_funq fun_f, u3_noun bus, u3_noun fol)
{
  u3_noun gon = u3m_soft_run(u3_nul, fun_f, u3k(bus), u3k(fol));
  u3_weak tax = u3_none;

  if ( 2 == u3h(gon) ) {
    tax = u3k(u3t(gon));
  }

  u3z(gon);
  return tax;
}

/* _has_tag(): does a frame of tax carry tag?  RETAINS.
*/
static c3_t
_has_tag(u3_noun tax, u3_noun tag)
{
  for ( ; u3_nul != tax; tax = u3t(tax) ) {
    if ( tag == u3h(u3h(tax)) ) {
      return 1;
    }
  }

  return 0;
}

/* _mean_both(): crash [bus fol] under u3n and u3nc and render both traces,
**               and the u3n one through +mook too; whether the u3nc trace
**               has a frame tagged tag, if tag is set.  For u3m_soft().
**               Produces [[ren rec mok has] tac], tac the raw u3nc trace,
**               or ~ if a run didn't crash.
*/
static u3_noun
_mean_both(u3_noun arg)
{
  u3_noun tag, bus, fol, ren, rec, mok, pro;
  u3_weak tan, tac;

  u3x_trel(arg, &tag, &bus, &fol);

  tan = _crash(u3n_nock_on, bus, fol);
  tac = _crash(u3nc_nock_on, bus, fol);

  if ( (u3_none == tan) || (u3_none == tac) ) {
    if ( u3_none != tan ) u3z(tan);
    if ( u3_none != tac ) u3z(tac);
    u3z(arg);
    return u3_nul;
  }

  ren = u3t_mook(u3k(tan));
  rec = u3t_mook(u3k(tac));
  mok = u3dc("mook", 2, u3k(tan));
  pro = u3nc(u3nq(ren, rec, u3k(u3t(mok)), __(!tag || _has_tag(tac, tag))),
             tac);

  u3z(mok);
  u3z(tan);
  u3z(arg);
  return pro;
}

/* _test_crash(): [bus fol] crashes: the rendered traces of u3n and u3nc
**                agree, and the renderer agrees with +mook on the u3n
**                trace; tag: the frame the u3nc trace must have, or 0.
**                TRANSFERS.
*/
static c3_i
_test_crash(const c3_c* nam_c, u3_noun bus, u3_noun fol, u3_noun tag)
{
  u3_noun gon = u3m_soft(0, _mean_both, u3nt(tag, bus, fol));
  u3_noun ren, rec, mok, has;
  c3_i    ret_i = 1;

  if ( 0 != u3h(gon) ) {
    u3_pier_punt_goof(nam_c, gon);
    fprintf(stderr, "test %s: failed\r\n", nam_c);
    return 0;
  }

  if ( u3_nul == u3t(gon) ) {
    fprintf(stderr, "test %s: no crash\r\n", nam_c);
    u3z(gon);
    return 0;
  }

  u3x_qual(u3h(u3t(gon)), &ren, &rec, &mok, &has);

  if ( c3n == u3r_sing(ren, rec) ) {
    fprintf(stderr, "test %s: u3n and u3nc traces differ\r\n", nam_c);
    u3_pier_punt(0, u3k(ren));
    u3_pier_punt(0, u3k(rec));
    ret_i = 0;
  }
  else if ( c3n == u3r_sing(ren, mok) ) {
    fprintf(stderr, "test %s: renderer and +mook differ\r\n", nam_c);
    u3_pier_punt(0, u3k(ren));
    u3_pier_punt(0, u3k(mok));
    ret_i = 0;
  }
  else if ( c3n == has ) {
    u3_noun tac = u3t(u3t(gon));

    fprintf(stderr, "test %s: u3nc pushed no %%%s frame\r\n", nam_c,
            ( c3__meaf == tag ) ? "meaf" : "meac");
    for ( ; u3_nul != tac; tac = u3t(tac) ) {
      u3m_p("frame tag", u3h(u3h(tac)));
    }
    ret_i = 0;
  }
  else {
    fprintf(stderr, "test %s: ok\r\n", nam_c);
    u3_pier_punt(2, u3k(ren));
  }

  u3z(gon);
  return ret_i;
}

/* _test_mean(): %mean traps: a trap with a known battery is pushed as
**               its arm and the parts of the trap the arm takes (%meaf),
**               the known product of a pure arm as a constant (%meac),
**               an unknown trap as itself.  Each gate on a sample that
**               runs through, and on 0, where +dec crashes; the arm of
**               one crashes too (####); one is deep enough to skip
**               frames; one is %spot-hinted.  The ~| case, a deferred
**               +sell, runs only with U3NC_SLOW set: the analysis of the
**               printer takes 25 s.
*/
static c3_i
_test_mean(void)
{
  static const struct {
    const c3_c* nam_c;
    const c3_c* src_c;
    c3_w        sam_w;
    u3_noun     tag;
  } cas_u[] = {
    { "deferred",
      "|=  a=@\n~>  %mean.|.(leaf+(scow %ud a))\n(dec a)\n",
      5, c3__meaf },
    { "constant cons",
      "|=  a=@\n~_  leaf+\"foo\"\n(dec a)\n",
      5, c3__meac },
    { "constant atom",
      "|=  a=@\n~_  %foo\n(dec a)\n",
      5, c3__meac },
    { "unknown",
      "|=  a=@\n~>  %mean.?:(=(a 5) |.(leaf+\"a\") |.(leaf+\"b\"))\n(dec a)\n",
      5, 0 },
    { "arm crash",
      "|=  a=@\n~>  %mean.|.(leaf+(scow %ud (dec a)))\n(dec a)\n",
      5, c3__meaf },
    { "spotted",
      "!:\n|=  a=@\n~>  %mean.|.(leaf+(scow %ud a))\n(dec a)\n",
      5, c3__meaf },
    { "deep",
      "|=  a=@\n|-  ^-  @\n~>  %mean.|.(leaf+(scow %ud a))\n"
      "?:  =(a 0)  !!\n$(a (dec a))\n",
      1100, c3__meaf },
    { "vase",
      "|=  a=@\n~|  [%foo a]\n(dec a)\n",
      5, c3__meaf },
  };
  c3_i ret_i = 1;
  c3_w i_w;

  for ( i_w = 0; i_w < (sizeof(cas_u) / sizeof(cas_u[0])); i_w++ ) {
    u3_noun fol;
    c3_c    nam_c[64];

    if ( !strcmp(cas_u[i_w].nam_c, "vase") && !getenv("U3NC_SLOW") ) {
      continue;
    }

    fol = u3nq(9, 2, 0, 1);

    //  the deep one runs through on no sample: it always crashes
    //
    if ( 1100 != cas_u[i_w].sam_w ) {
      snprintf(nam_c, sizeof(nam_c), "mean %s", cas_u[i_w].nam_c);
      ret_i = _test(nam_c, _gate(cas_u[i_w].src_c, cas_u[i_w].sam_w),
                    u3k(fol), 1)
           && ret_i;
    }

    snprintf(nam_c, sizeof(nam_c), "mean %s, crash", cas_u[i_w].nam_c);
    ret_i = _test_crash(nam_c, _gate(cas_u[i_w].src_c,
                                     ( 1100 == cas_u[i_w].sam_w ) ? 1100 : 0),
                        fol, cas_u[i_w].tag)
         && ret_i;
  }

  return ret_i;
}

/* _test_ska(): the %ska hint runs its formula compiled, from u3n: the
**              raw trace of a crash inside it carries a deferred %mean
**              frame, which only compiled nock pushes.  (The trap sits
**              in a called gate: the analysis defers a %mean trap of a
**              callee, not one of the entry function itself.)  Not
**              inside the +mink jet, whose raw trace goes to Hoon's
**              +mook: there the frame is a plain %mean.  +mock renders
**              the same crash in C, like u3t_mook on +mink's trace;
**              +mule and +mole run their traps through hooks.
*/
static c3_i
_test_ska(void)
{
  const c3_c* src_c = "|=  a=@\n"
                      "=/  foo\n"
                      "  |=  b=@\n"
                      "  ~>  %mean.|.(leaf+(scow %ud b))\n"
                      "  (dec b)\n"
                      "~>  %ska\n"
                      "(foo a)\n";
  const c3_c* gat_c = "|=  a=@\n"
                      "=/  g\n"
                      "  |=  b=@\n"
                      "  ~>  %ska\n"
                      "  ~>  %mean.|.(leaf+(scow %ud b))\n"
                      "  (dec b)\n";
  const c3_c* kin_c = "(mink [g(+< a) 9 2 0 1] ~)\n";
  const c3_c* kok_c = "(mock [g(+< a) 9 2 0 1] ~)\n";
  const c3_c* vir_c = "|=  a=@\n"
                      "[(mule |.((dec a))) (mole |.((dec a)))]\n";
  u3_noun fol = u3nq(9, 2, 0, 1);
  c3_c    mik_c[256], mok_c[256];
  u3_noun cor, kin, kok;
  u3_weak tax;
  c3_i    ret_i;

  snprintf(mik_c, sizeof(mik_c), "%s%s", gat_c, kin_c);
  snprintf(mok_c, sizeof(mok_c), "%s%s", gat_c, kok_c);

  ret_i = _test("ska hint", _gate(src_c, 5), u3k(fol), 1);

  cor   = _gate(src_c, 0);
  tax   = _crash(u3n_nock_on, cor, fol);
  ret_i = (u3_none != tax) && _has_tag(tax, c3__meaf) && ret_i;
  fprintf(stderr, "test ska hint, deferred frame: %s\r\n",
          ( (u3_none != tax) && _has_tag(tax, c3__meaf) ) ? "ok" : "failed");
  if ( u3_none != tax ) u3z(tax);
  u3z(cor);

  ret_i = _test("ska mink", _gate(mik_c, 5), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mock", _gate(mok_c, 5), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mink, crash", _gate(mik_c, 0), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mock, crash", _gate(mok_c, 0), u3k(fol), 1) && ret_i;

  //  +mink's raw trace has a plain %mean frame and no deferred one, and
  //  renders to what +mock produced
  //
  cor = _gate(mik_c, 0);
  kin = _run("u3n  mink  ", _nock_n, cor, fol);
  u3z(cor);
  cor = _gate(mok_c, 0);
  kok = _run("u3n  mock  ", _nock_n, cor, fol);
  u3z(cor);

  if ( (u3_none == kin) || (u3_none == kok) ) {
    ret_i = 0;
  }
  else {
    u3_noun tan = u3t_mook(u3k(u3t(kin)));
    c3_i    ok_i = (2 == u3h(kin))
                && _has_tag(u3t(kin), c3__mean)
                && !_has_tag(u3t(kin), c3__meaf)
                && (c3y == u3r_sing(tan, u3t(kok)));

    fprintf(stderr, "test ska mink, flag: %s\r\n", ok_i ? "ok" : "failed");
    ret_i = ok_i && ret_i;
    u3z(tan);
  }
  if ( u3_none != kin ) u3z(kin);
  if ( u3_none != kok ) u3z(kok);

  ret_i = _test("ska mule mole", _gate(vir_c, 5), u3k(fol), 1) && ret_i;
  ret_i = _test("ska mule mole, crash", _gate(vir_c, 0), u3k(fol), 1)
       && ret_i;

  u3z(fol);
  return ret_i;
}

/* _jets_pick(): is the case nam_c picked by U3NC_JETS: all of them
**               unless it is a comma-separated list of names.
*/
static c3_t
_jets_pick(const c3_c* nam_c)
{
  c3_c*  lis_c = getenv("U3NC_JETS");
  size_t len_i = strlen(nam_c);
  c3_c*  hit_c;

  if ( !lis_c || !strcmp(lis_c, "1") ) {
    return 1;
  }

  for ( hit_c = lis_c; (hit_c = strstr(hit_c, nam_c)); hit_c++ ) {
    if (  ((hit_c == lis_c) || (',' == hit_c[-1]))
       && ((',' == hit_c[len_i]) || ('\0' == hit_c[len_i])) )
    {
      return 1;
    }
  }

  return 0;
}

/* _test_jets(): one call of every array-driven and punting arm, from a
**               gate on the kernel whose sample carries the arguments,
**               so the compiler can't fold the call: both interpreters
**               agree, and, built with -Dska-stat, the compiled call
**               hit a driver.
**
**               With len_w, for the timings: each gate called on every
**               item of a list of len_w copies of its sample, by a loop
**               shaped like _test_iter_vint()'s, so that the entry into
**               the program is paid once, not per call.  A first case,
**               (vint), increments through the same gate call: the cost
**               of the traversal, to take out of the others.
*/
static c3_i
_test_jets(c3_w len_w)
{
  static const struct {
    const c3_c* nam_c;
    const c3_c* src_c;
    const c3_c* sam_c;
  } cas_u[] = {
    { "(vint)", "|=(a=@ +(a))", "1" },
    { "add", "|=([a=@ b=@] (add a b))", "[3 4]" },
    { "find", "|=([a=(list @) b=(list @)] (find a b))", "[~[3 4] ~[1 2 3 4 5]]" },
    { "flop", "|=(a=(list @) (flop a))", "~[1 2 3]" },
    { "lent", "|=(a=(list @) (lent a))", "~[1 2 3]" },
    { "need", "|=(a=(unit @) (need a))", "[~ 42]" },
    { "scag", "|=([a=@ b=(list @)] (scag a b))", "[2 ~[1 2 3 4]]" },
    { "slag", "|=([a=@ b=(list @)] (slag a b))", "[2 ~[1 2 3 4]]" },
    { "zing", "|=(a=(list (list @)) (zing a))", "~[~[1 2] ~[3] ~[4 5]]" },
    { "welp", "|=([a=(list @) b=(list @)] (welp a b))", "[~[1 2] ~[3 4]]" },
    { "cap", "|=(a=@ (cap a))", "6" },
    { "peg", "|=([a=@ b=@] (peg a b))", "[6 5]" },
    { "mas", "|=(a=@ (mas a))", "6" },
    { "can", "|=([a=bloq b=(list [p=step q=@])] (can a b))", "[3 ~[[1 1] [2 515]]]" },
    { "dor", "|=([a=* b=*] (dor a b))", "[[1 2] [1 3]]" },
    { "gor", "|=([a=* b=*] (gor a b))", "[[1 2] 3]" },
    { "mor", "|=([a=* b=*] (mor a b))", "[[1 2] 3]" },
    { "pow", "|=([a=@ b=@] (pow a b))", "[3 5]" },
    { "rep", "|=([a=bite b=(list @)] (rep a b))", "[[3 2] ~[1 2 3]]" },
    { "rev", "|=([a=bloq b=@ c=@] (rev a b c))", "[3 4 999]" },
    { "sew", "|=([a=bloq [b=step c=step d=@] e=@] (sew a [b c d] e))", "[3 [1 2 0xabcd] 0x1234.5678.9abc]" },
    { "swp", "|=([a=bloq b=@] (swp a b))", "[3 999]" },
    { "sqt", "|=(a=@ (sqt a))", "99" },
    { "xeb", "|=(a=@ (xeb a))", "99" },
    { "ind", "|=(a=@tas (ind:po a))", "%zod" },
    { "ins", "|=(a=@tas (ins:po a))", "%doz" },
    { "raw", "|=([a=@ b=@] (~(raw og a) b))", "[7 32]" },
    { "apt:by", "|=(m=(map @ @) ~(apt by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "del:by", "|=([m=(map @ @) k=@] (~(del by m) k))", "[(my ~[[1 2] [3 4] [5 6]]) 3]" },
    { "dif:by", "|=([m=(map @ @) n=(map @ @)] (~(dif by m) n))", "[(my ~[[1 2] [3 4] [5 6]]) (my ~[[3 4] [7 8]])]" },
    { "gas:by", "|=([m=(map @ @) l=(list [@ @])] (~(gas by m) l))", "[(my ~[[1 2] [3 4]]) ~[[5 6] [7 8]]]" },
    { "get:by", "|=([m=(map @ @) k=@] (~(get by m) k))", "[(my ~[[1 2] [3 4] [5 6]]) 3]" },
    { "has:by", "|=([m=(map @ @) k=@] (~(has by m) k))", "[(my ~[[1 2] [3 4] [5 6]]) 3]" },
    { "int:by", "|=([m=(map @ @) n=(map @ @)] (~(int by m) n))", "[(my ~[[1 2] [3 4] [5 6]]) (my ~[[3 4] [7 8]])]" },
    { "key:by", "|=(m=(map @ @) ~(key by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "put:by", "|=([m=(map @ @) k=@ v=@] (~(put by m) k v))", "[(my ~[[1 2] [3 4] [5 6]]) 7 8]" },
    { "tap:by", "|=(m=(map @ @) ~(tap by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "uni:by", "|=([m=(map @ @) n=(map @ @)] (~(uni by m) n))", "[(my ~[[1 2] [3 4] [5 6]]) (my ~[[3 4] [7 8]])]" },
    { "wyt:by", "|=(m=(map @ @) ~(wyt by m))", "(my ~[[1 2] [3 4] [5 6]])" },
    { "apt:in", "|=(s=(set @) ~(apt in s))", "(sy ~[1 3 5 7])" },
    { "bif:in", "|=([s=(set @) k=@] (~(bif in s) k))", "[(sy ~[1 3 5 7]) 4]" },
    { "del:in", "|=([s=(set @) k=@] (~(del in s) k))", "[(sy ~[1 3 5 7]) 3]" },
    { "dif:in", "|=([s=(set @) t=(set @)] (~(dif in s) t))", "[(sy ~[1 3 5 7]) (sy ~[3 4 5])]" },
    { "gas:in", "|=([s=(set @) l=(list @)] (~(gas in s) l))", "[(sy ~[1 3]) ~[5 7 9]]" },
    { "has:in", "|=([s=(set @) k=@] (~(has in s) k))", "[(sy ~[1 3 5 7]) 5]" },
    { "int:in", "|=([s=(set @) t=(set @)] (~(int in s) t))", "[(sy ~[1 3 5 7]) (sy ~[3 4 5])]" },
    { "put:in", "|=([s=(set @) k=@] (~(put in s) k))", "[(sy ~[1 3 5 7]) 4]" },
    { "tap:in", "|=(s=(set @) ~(tap in s))", "(sy ~[1 3 5 7])" },
    { "uni:in", "|=([s=(set @) t=(set @)] (~(uni in s) t))", "[(sy ~[1 3 5 7]) (sy ~[3 4 5])]" },
    { "wyt:in", "|=(s=(set @) ~(wyt in s))", "(sy ~[1 3 5 7])" },
    { "cue", "|=(a=@ (cue a))", "(jam [1 2 3])" },
    { "mat", "|=(a=@ (mat a))", "1.000" },
    { "rub", "|=([a=@ b=@] (rub a b))", "[0 q:(mat 1.000)]" },
    { "leer", "|=(a=@t (to-wain:format a))", "(rap 3 'foo' 10 'bar' 10 'baz' ~)" },
    { "loss", "|=([a=(list @) b=(list @)] (loss:differ a b))", "[~[1 2 3 4 5] ~[2 4 6]]" },
    { "lune", "|=(a=@t (lune:unix:userlib a))", "(rap 3 'foo' 10 'bar' 10 ~)" },
    { "drg:cofl", "|=(a=[e=@s a=@u] (drg:+:fl a))", "[--3 5]" },
    { "lug:cofl", "|=([t=?(%fl %ce %sm %lg %ne %na %nt) a=[e=@s a=@u] s=?] (lug:+:fl t a s))", "[%ne [--0 (add (bex 120) 1)] &]" },
    { "add:rd", "|=([a=@rd b=@rd] (add:rd a b))", "[.~1.5 .~2.25]" },
    { "sub:rd", "|=([a=@rd b=@rd] (sub:rd a b))", "[.~1.5 .~2.25]" },
    { "mul:rd", "|=([a=@rd b=@rd] (mul:rd a b))", "[.~1.5 .~2.25]" },
    { "div:rd %u", "|=([a=@rd b=@rd] (~(div rd %u) a b))", "[.~1 .~3]" },
    { "sqt:rd", "|=(a=@rd (sqt:rd a))", ".~2" },
    { "fma:rd", "|=([a=@rd b=@rd c=@rd] (fma:rd a b c))", "[.~1.5 .~2 .~0.25]" },
    { "lth:rd", "|=([a=@rd b=@rd] (lth:rd a b))", "[.~1.5 .~2.25]" },
    { "lte:rd", "|=([a=@rd b=@rd] (lte:rd a b))", "[.~1.5 .~2.25]" },
    { "equ:rd", "|=([a=@rd b=@rd] (equ:rd a b))", "[.~1.5 .~2.25]" },
    { "gte:rd", "|=([a=@rd b=@rd] (gte:rd a b))", "[.~1.5 .~2.25]" },
    { "gth:rd", "|=([a=@rd b=@rd] (gth:rd a b))", "[.~1.5 .~2.25]" },
    { "add:rs", "|=([a=@rs b=@rs] (add:rs a b))", "[.1.5 .2.25]" },
    { "sub:rs", "|=([a=@rs b=@rs] (sub:rs a b))", "[.1.5 .2.25]" },
    { "mul:rs", "|=([a=@rs b=@rs] (mul:rs a b))", "[.1.5 .2.25]" },
    { "div:rs %u", "|=([a=@rs b=@rs] (~(div rs %u) a b))", "[.1 .3]" },
    { "sqt:rs", "|=(a=@rs (sqt:rs a))", ".2" },
    { "fma:rs", "|=([a=@rs b=@rs c=@rs] (fma:rs a b c))", "[.1.5 .2 .0.25]" },
    { "lth:rs", "|=([a=@rs b=@rs] (lth:rs a b))", "[.1.5 .2.25]" },
    { "lte:rs", "|=([a=@rs b=@rs] (lte:rs a b))", "[.1.5 .2.25]" },
    { "equ:rs", "|=([a=@rs b=@rs] (equ:rs a b))", "[.1.5 .2.25]" },
    { "gte:rs", "|=([a=@rs b=@rs] (gte:rs a b))", "[.1.5 .2.25]" },
    { "gth:rs", "|=([a=@rs b=@rs] (gth:rs a b))", "[.1.5 .2.25]" },
    { "add:rq", "|=([a=@rq b=@rq] (add:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "sub:rq", "|=([a=@rq b=@rq] (sub:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "mul:rq", "|=([a=@rq b=@rq] (mul:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "div:rq %u", "|=([a=@rq b=@rq] (~(div rq %u) a b))", "[.~~~1 .~~~3]" },
    { "sqt:rq", "|=(a=@rq (sqt:rq a))", ".~~~2" },
    { "fma:rq", "|=([a=@rq b=@rq c=@rq] (fma:rq a b c))", "[.~~~1.5 .~~~2 .~~~0.25]" },
    { "lth:rq", "|=([a=@rq b=@rq] (lth:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "lte:rq", "|=([a=@rq b=@rq] (lte:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "equ:rq", "|=([a=@rq b=@rq] (equ:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "gte:rq", "|=([a=@rq b=@rq] (gte:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "gth:rq", "|=([a=@rq b=@rq] (gth:rq a b))", "[.~~~1.5 .~~~2.25]" },
    { "add:rh", "|=([a=@rh b=@rh] (add:rh a b))", "[.~~1.5 .~~2.25]" },
    { "sub:rh", "|=([a=@rh b=@rh] (sub:rh a b))", "[.~~1.5 .~~2.25]" },
    { "mul:rh", "|=([a=@rh b=@rh] (mul:rh a b))", "[.~~1.5 .~~2.25]" },
    { "div:rh %u", "|=([a=@rh b=@rh] (~(div rh %u) a b))", "[.~~1 .~~3]" },
    { "sqt:rh", "|=(a=@rh (sqt:rh a))", ".~~2" },
    { "fma:rh", "|=([a=@rh b=@rh c=@rh] (fma:rh a b c))", "[.~~1.5 .~~2 .~~0.25]" },
    { "lth:rh", "|=([a=@rh b=@rh] (lth:rh a b))", "[.~~1.5 .~~2.25]" },
    { "lte:rh", "|=([a=@rh b=@rh] (lte:rh a b))", "[.~~1.5 .~~2.25]" },
    { "equ:rh", "|=([a=@rh b=@rh] (equ:rh a b))", "[.~~1.5 .~~2.25]" },
    { "gte:rh", "|=([a=@rh b=@rh] (gte:rh a b))", "[.~~1.5 .~~2.25]" },
    { "gth:rh", "|=([a=@rh b=@rh] (gth:rh a b))", "[.~~1.5 .~~2.25]" },
    { "just fun", "|=([daf=char tub=nail] ((just daf) tub))", "['a' [1 1] \"abc\"]" },
    { "mask fun", "|=([bud=(list char) tub=nail] ((mask bud) tub))", "[\"xyz\" [1 1] \"yes\"]" },
    { "shim fun", "|=([les=@ mos=@ tub=nail] ((shim les mos) tub))", "['a' 'z' [1 1] \"hello\"]" },
    { "easy fun", "|=([huf=* tub=nail] ((easy huf) tub))", "[%foo [1 1] \"bar\"]" },
    { "slaw", "|=(a=@ta (slaw %ud a))", "'123'" },
    { "scow", "|=(a=@ (scow %ud a))", "123" },
    { "cell", "|=([a=type b=type] (cell a b))", "[-:!>(1) -:!>(~)]" },
    { "comb", "|=([a=nock b=nock] (comb a b))", "[[0 2] [2 [0 3] [0 4]]]" },
    { "cons", "|=([a=nock b=nock] (cons a b))", "[[1 2] [1 3]]" },
    { "core", "|=([a=type b=coil] (core a b))", "=/(t -:!>(|=(a=@ a)) ?>(?=([%core *] t) +.t))" },
    { "face", "|=([a=term b=type] (face a b))", "[%foo -:!>(1)]" },
    { "fitz", "|=([a=@ta b=@ta] (fitz a b))", "[%ud %u]" },
    { "fork", "|=(a=(list type) (fork a))", "~[-:!>(1) -:!>(~) -:!>(1)]" },
    { "look", "|=([a=term b=(map term hoon)] (look:..ride a b))", "[%bar (~(gas by *(map term hoon)) ~[[%foo [%bust %null]] [%bar [%bust %noun]]])]" },
    { "loot", "|=([a=term b=(map term tome)] (loot a b))", "[%qux (~(gas by *(map term tome)) ~[[%foo (~(gas by *tome) [%bar [%bust %null]]~)] [%baz (~(gas by *tome) [%qux [%bust %noun]]~)]])]" },
    { "en:base16:mimes:html", "|=(a=octs (en:base16:mimes:html a))", "[4 0xdead.beef]" },
    { "de:base16:mimes:html", "|=(a=cord (de:base16:mimes:html a))", "'deadbeef'" },
    { "en:ecba:aes", "|=([k=@H b=@H] (~(en ecba:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "de:ecba:aes", "|=([k=@H b=@H] (~(de ecba:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "en:ecbb:aes", "|=([k=@I b=@H] (~(en ecbb:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "de:ecbb:aes", "|=([k=@I b=@H] (~(de ecbb:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "en:ecbc:aes", "|=([k=@I b=@H] (~(en ecbc:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "de:ecbc:aes", "|=([k=@I b=@H] (~(de ecbc:aes:crypto k) b))", "[0x1234.5678 0xabcd]" },
    { "en:cbca:aes", "|=([k=@H p=@H t=@] (~(en cbca:aes:crypto k p) t))", "[0x1234.5678 0x9abc 'hello world']" },
    { "de:cbca:aes", "|=([k=@H p=@H t=@] (~(de cbca:aes:crypto k p) t))", "[0x1234.5678 0x9abc 0xdead.beef]" },
    { "en:cbcb:aes", "|=([k=@I p=@H t=@] (~(en cbcb:aes:crypto k p) t))", "[0x1234.5678 0x9abc 'hello world']" },
    { "de:cbcb:aes", "|=([k=@I p=@H t=@] (~(de cbcb:aes:crypto k p) t))", "[0x1234.5678 0x9abc 0xdead.beef]" },
    { "en:cbcc:aes", "|=([k=@I p=@H t=@] (~(en cbcc:aes:crypto k p) t))", "[0x1234.5678 0x9abc 'hello world']" },
    { "de:cbcc:aes", "|=([k=@I p=@H t=@] (~(de cbcc:aes:crypto k p) t))", "[0x1234.5678 0x9abc 0xdead.beef]" },
    { "puck:ed", "|=(s=@I (puck:ed:crypto s))", "(shax 'seed')" },
    { "sign:ed", "|=([m=@ s=@] (sign:ed:crypto m s))", "['hello' (shax 'seed')]" },
    { "sign-octs:ed", "|=([m=octs s=@] (sign-octs:ed:crypto m s))", "[[5 'hello'] (shax 'seed')]" },
    { "veri:ed", "|=([m=@ s=@] (veri:ed:crypto (sign:ed:crypto m s) m (puck:ed:crypto s)))", "['hello' (shax 'seed')]" },
    { "veri-octs:ed", "|=([m=octs s=@] (veri-octs:ed:crypto (sign-octs:ed:crypto m s) m (puck:ed:crypto s)))", "[[5 'hello'] (shax 'seed')]" },
    { "shar:ed", "|=([a=@ b=@] (shar:ed:crypto (puck:ed:crypto a) b))", "[(shax 'foo') (shax 'bar')]" },
    { "point-add:ed", "|=([a=@ b=@] (point-add:ed:crypto (puck:ed:crypto a) (puck:ed:crypto b)))", "[(shax 'foo') (shax 'bar')]" },
    { "scalarmult:ed", "|=([a=@ b=@] (scalarmult:ed:crypto a (puck:ed:crypto b)))", "[(shax 'foo') (shax 'bar')]" },
    { "scalarmult-base:ed", "|=(a=@ (scalarmult-base:ed:crypto a))", "(shax 'foo')" },
    { "add-scalarmult-scalarmult-base:ed", "|=([a=@ p=@ b=@] (add-scalarmult-scalarmult-base:ed:crypto a (puck:ed:crypto p) b))", "[(shax 'foo') (shax 'bar') (shax 'baz')]" },
    { "add-double-scalarmult:ed", "|=([a=@ p=@ b=@ q=@] (add-double-scalarmult:ed:crypto a (puck:ed:crypto p) b (puck:ed:crypto q)))", "[(shax 'foo') (shax 'bar') (shax 'baz') (shax 'qux')]" },
    { "hmac:hmac", "|=([k=@ m=@] (hmac:hmac:crypto [sha-256l:sha 64 32] [(met 3 k) k] [(met 3 m) m]))", "['key' 'hello world']" },
    { "hmac-sha1l:hmac", "|=([k=byts m=byts] (hmac-sha1l:hmac:crypto k m))", "[[3 'key'] [11 'hello world']]" },
    { "hmac-sha256l:hmac", "|=([k=byts m=byts] (hmac-sha256l:hmac:crypto k m))", "[[3 'key'] [11 'hello world']]" },
    { "hmac-sha512l:hmac", "|=([k=byts m=byts] (hmac-sha512l:hmac:crypto k m))", "[[3 'key'] [11 'hello world']]" },
    { "hmac-sha1l:hmac long key", "|=([k=byts m=byts] (hmac-sha1l:hmac:crypto k m))", "[[100 (fil 3 100 'k')] [0 0]]" },
    { "hmac-sha256l:hmac long key", "|=([k=byts m=byts] (hmac-sha256l:hmac:crypto k m))", "[[100 (fil 3 100 'k')] [0 0]]" },
    { "hmac-sha512l:hmac long key", "|=([k=byts m=byts] (hmac-sha512l:hmac:crypto k m))", "[[200 (fil 3 200 'k')] [5 'hello']]" },
    { "hmac-sha1:hmac", "|=([k=@ m=@] (hmac-sha1:hmac:crypto k m))", "['key' 'hello world']" },
    { "hmac-sha256:hmac", "|=([k=@ m=@] (hmac-sha256:hmac:crypto k m))", "['key' 'hello world']" },
    { "hmac-sha512:hmac", "|=([k=@ m=@] (hmac-sha512:hmac:crypto k m))", "['key' 'hello world']" },
    { "hmac-sha1t:hmac", "|=([k=@t m=@t] (hmac-sha1t:hmac:crypto k m))", "['key' 'hello world']" },
    { "hmac-sha256t:hmac", "|=([k=@t m=@t] (hmac-sha256t:hmac:crypto k m))", "['key' 'hello world']" },
    { "hmac-sha512t:hmac", "|=([k=@t m=@t] (hmac-sha512t:hmac:crypto k m))", "['key' 'hello world']" },
    { "pbk:scr", "|=([p=@ s=@ c=@ d=@] (pbk:scr:crypto p s c d))", "['password' 'salt' 2 32]" },
    { "pbl:scr", "|=([p=@ pl=@ s=@ sl=@ c=@ d=@] (pbl:scr:crypto p pl s sl c d))", "['password' 8 'salt' 4 2 32]" },
    { "verify:schnorr:secp256k1", "|=([pk=@I m=@I sig=@J] (verify:schnorr:secp256k1:secp:crypto pk m sig))", "[0xf930.8a01.9258.c310.4934.4f85.f89d.5229.b531.c845.836f.99b0.8601.f113.bce0.36f9 0 0xe907.831f.8084.8d10.69a5.371b.4024.1036.4bdf.1c5f.8307.b008.4c55.f1ce.2dca.8215.25f6.6a4a.85ea.8b71.e482.a74f.382d.2ce5.ebee.e8fd.b217.2f47.7df4.900d.3105.36c0]" },
    { "keccak-224:keccak", "|=(a=octs (keccak-224:keccak:crypto a))", "[5 'hello']" },
    { "keccak-256:keccak", "|=(a=octs (keccak-256:keccak:crypto a))", "[5 'hello']" },
    { "keccak-384:keccak", "|=(a=octs (keccak-384:keccak:crypto a))", "[5 'hello']" },
    { "keccak-512:keccak", "|=(a=octs (keccak-512:keccak:crypto a))", "[5 'hello']" },
    { "ripemd-160:ripemd", "|=(a=byts (ripemd-160:ripemd:crypto a))", "[5 'hello']" },
    { "sha-1l:sha", "|=(a=byts (sha-1l:sha a))", "[5 'hello']" },
    { "shax", "|=(a=@ (shax a))", "'hello world'" },
    { "shay", "|=([l=@ d=@] (shay l d))", "[11 'hello world']" },
    { "shas", "|=([s=@ r=@] (shas s r))", "['salt' 'hello world']" },
    { "shal", "|=([l=@ d=@] (shal l d))", "[11 'hello world']" },
    { "luck:ed", "|=(s=@I (luck:ed:crypto s))", "(shax 'seed')" },
    { "point-neg:ed", "|=(a=@ (point-neg:ed:crypto (puck:ed:crypto a)))", "(shax 'foo')" },
    { "recs:ed", "|=(a=@ (recs:ed:crypto a))", "(shax 'foo')" },
    { "scad:ed", "|=([s=@ c=@] =/(k (luck:ed:crypto s) (scad:ed:crypto pub.k sek.k c)))", "[(shax 'seed') (shax 'foo')]" },
    { "scas:ed", "|=([s=@ c=@] (scas:ed:crypto sek:(luck:ed:crypto s) c))", "[(shax 'seed') (shax 'foo')]" },
    { "scap:ed", "|=([s=@ c=@] (scap:ed:crypto (puck:ed:crypto s) c))", "[(shax 'seed') (shax 'foo')]" },
    { "sign-raw:ed", "|=([m=@ s=@] (sign-raw:ed:crypto m (luck:ed:crypto s)))", "['hello' (shax 'seed')]" },
    { "sign-octs-raw:ed", "|=([m=octs s=@] (sign-octs-raw:ed:crypto m (luck:ed:crypto s)))", "[[5 'hello'] (shax 'seed')]" },
    { "slar:ed", "|=([a=@ b=@] (slar:ed:crypto (puck:ed:crypto a) sek:(luck:ed:crypto b)))", "[(shax 'foo') (shax 'bar')]" },
    { "smac:ed", "|=([a=@ b=@ c=@] (smac:ed:crypto a b c))", "[(shax 'foo') (shax 'bar') (shax 'baz')]" },
    { "hash:blake3", "|=([out=@ud msg=octs] (blake3:blake:crypto out msg))", "[32 5 'hello']" },
    { "chunk-output:blake3", "|=([c=@ m=octs] (chunk-output:blake3:blake:crypto c m))", "[0 5 'hello']" },
    { "crc32", "|=(a=octs (crc32:crc:checksum a))", "[5 'hello']" },
    { "crypt:chacha", "|=(a=[rounds=@ud key=@uxI nonce=@uxG counter=@udG msg=octs] (crypt:chacha:crypto a))", "[20 0x1 0x2 0 [5 'hello']]" },
    { "xchacha:chacha", "|=(a=[rounds=@ud key=@uxI nonce=@ux] (xchacha:chacha:crypto a))", "[20 0x1 0x2]" },
    { "de:json", "|=(a=cord (de:json:html a))", "'{\"a\": [1, true, null, \"x\"]}'" },
    { "en:json", "|=(a=json (en:json:html a))", "[%a ~[[%n '1'] [%b &] ~ [%s 'x'] [%o (malt ['k' %s 'v']~)]]]" },
    { "mate", "|=([a=(unit @) b=(unit @)] (mate a b))", "[[~ 7] [~ 7]]" },
    { "clz", "|=([a=bite b=@] (clz a b))", "[[3 2] 5]" },
    { "ctz", "|=(a=@ (ctz a))", "48" },
    { "ham", "|=(a=@ (ham a))", "0xf.f00f" },
    { "hew", "|=([a=bite c=@ b=[@ @ @]] ((hew a c) b))", "[[3 0] 0x1234.5678 [1 2 1]]" },
    { "rig", "|=([a=bite b=bloq] (rig a b))", "[[3 2] 0]" },
    { "bif:by", "|=([a=(map @ @) b=@] (~(bif by a) b))", "[(my ~[[1 10] [2 20] [3 30]]) 2]" },
    { "fax:plot", "|=(p=plot (fax:plot p))", "[3 ~[0x11 0x22 0x33]]" },
    { "met:plot", "|=(p=plot (met:plot p))", "[3 ~[0x11 0x22 0x33]]" },
    { "nest:ut", "|=([a=type b=type] (~(nest ut a) & b))", "[-:!>(1) -:!>(2)]" },
    { "mint:ut", "|=([a=type b=hoon] (~(mint ut a) %noun b))", "[-:!>(.) (ream '(add 2 2)')]" },
  };
  u3_noun fol   = u3nq(9, 2, 0, 1);
  c3_i    ret_i = 1;
  c3_w    i_w;

  //  the (vint) case is only a baseline for the loops
  //
  for ( i_w = len_w ? 0 : 1; i_w < (sizeof(cas_u) / sizeof(cas_u[0])); i_w++ ) {
    u3_noun gat, sam, cor;
    c3_c    nam_c[64];

    //  U3NC_JETS=a,b: those cases only, and the baseline
    //
    if ( i_w && !_jets_pick(cas_u[i_w].nam_c) ) {
      continue;
    }

    sam = u3m_soft(0, u3v_wish_w, u3i_string(cas_u[i_w].sam_c));

    if ( !len_w ) {
      gat = u3m_soft(0, u3v_wish_w, u3i_string(cas_u[i_w].src_c));
    }
    else {
      //  the gate, by a face in the context of the loop, is a known call
      //
      c3_c* src_c = c3_malloc(strlen(cas_u[i_w].src_c) + 128);

      sprintf(src_c, "=/  f  %s\n"
                     "|=  l=(list _+<.f)\n"
                     "^-  (list)\n"
                     "?~  l  ~\n"
                     "[(f i.l) $(l t.l)]\n", cas_u[i_w].src_c);
      gat = u3m_soft(0, u3v_wish_w, u3i_string(src_c));
      c3_free(src_c);

      //  the list of samples, the same noun len_w times
      //
      if ( 0 == u3h(sam) ) {
        u3_noun lis = u3_nul;
        c3_w    j_w;

        for ( j_w = 0; j_w < len_w; j_w++ ) {
          lis = u3nc(u3k(u3t(sam)), lis);
        }
        u3z(sam);
        sam = u3nc(0, lis);
      }
    }

    snprintf(nam_c, sizeof(nam_c), "jet %s", cas_u[i_w].nam_c);

    //  a source that doesn't compile fails its case, not the run
    //
    if ( (0 != u3h(gat)) || (0 != u3h(sam)) ) {
      if ( 0 != u3h(gat) ) {
        u3_pier_punt_goof(nam_c, gat);
        u3z(sam);
      }
      else {
        u3_pier_punt_goof(nam_c, sam);
        u3z(gat);
      }
      fprintf(stderr, "test %s: bad source\r\n", nam_c);
      ret_i = 0;
      continue;
    }

    cor = u3nc(u3k(u3h(u3t(gat))),
               u3nc(u3k(u3t(sam)), u3k(u3t(u3t(u3t(gat))))));
    u3z(gat); u3z(sam);

    ret_i = _test(nam_c, u3k(cor), u3k(fol), 1) && ret_i;

    //  the entry lookup alone: u3d_match() checks the subject against
    //  every program entered with this formula, the gates of every test
    //  run before this one (all are entered with [9 2 0 1])
    //
    {
      u3_weak lis = u3h_git(u3R->ska.ent_p, fol);
      c3_w    ent_w = 0, rep_w;
      c3_d    beg_d;

      for ( u3_weak l = lis; (u3_none != l) && (u3_nul != l); l = u3t(l) ) {
        ent_w++;
      }
      beg_d = _time();
      for ( rep_w = 0; rep_w < 1000; rep_w++ ) {
        if ( u3_none != lis ) {
          (void)u3d_match(cor, lis);
        }
      }
      fprintf(stderr, "entry lookup: %" PRIc3_w " programs, %.2f us\r\n",
              ent_w, (double)(_time() - beg_d) / 1000.0);
    }
    u3z(cor);

    //  a small arm with a known battery is inlined rather than called,
    //  so no driver runs for it: a note, not a failure.  Counted over the
    //  last run, a second one: in the first, the SKA core compiling the
    //  program hits jets of its own, which would hide a miss
    //
#ifdef U3NC_STAT
    if ( !_run_jet_d && i_w ) {
      fprintf(stderr, "test %s: no jet hit\r\n", nam_c);
    }
#endif
  }

  u3z(fol);
  return ret_i;
}

/* _jets_len(): the length of the lists _test_jets() loops on:
**              U3NC_JETS_N, else 1000 under U3NC_BENCH, else none.
*/
static c3_w
_jets_len(void)
{
  c3_c* len_c = getenv("U3NC_JETS_N");
  return len_c ? (c3_w)atoi(len_c) : ( getenv("U3NC_BENCH") ? 1000 : 0 );
}

/* _stat_sum(): the sizes of everything compiled, for comparing
**              encodings, and the time spent building it (-Dska-stat).
*/
static void
_stat_sum(void)
{
#ifdef U3NC_STAT
  fprintf(stderr, "stat: %" PRIu64 " programs, %" PRIu64 " bytes, "
                  "%" PRIu64 " literals, %" PRIu64 " IML, %" PRIu64 " IMM "
                  "(%" PRIu64 " of 0, %" PRIu64 " of 1, %" PRIu64 " wide slot), "
                  "%" PRIu64 " KIL, %" PRIu64 " DRO in %" PRIu64 " bytes, "
                  "%" PRIu64 " edge blocks, %" PRIu64 " us building\r\n",
          u3nc_Stat.com_d, u3nc_Stat.byc_d, u3nc_Stat.lit_d, u3nc_Stat.iml_d,
          u3nc_Stat.imm_d, u3nc_Stat.imz_d, u3nc_Stat.imo_d, u3nc_Stat.imw_d,
          u3nc_Stat.kil_d, u3nc_Stat.dre_d, u3nc_Stat.drb_d,
          u3nc_Stat.edb_d, u3nc_Stat.bld_d);
#endif
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  //  the build step runs the test; with U3NC_BUILD_ONLY it only builds
  //
  //  the quick tests, while the code changes:
  //    zig build nock-compile-test -Doptimize=ReleaseFast -Dno-lto
  //  all of them, the slow ones too, and the reference counts checked
  //  after every run, once it is done:
  //    U3NC_ALL=1 zig build nock-compile-test
  //
  if ( getenv("U3NC_BUILD_ONLY") ) {
    return 0;
  }

  _setup();

  //  U3NC_STEEL_GEN: write a steel pill to that file, as the generator
  //  desk/gen/steel-pill.hoon of the SKA repository does on a ship: the
  //  core's own hoon.hoon (U3NC_STEEL_SYS) built from nothing, the
  //  library (U3NC_STEEL_LIB) on it.  In u3n, and nothing else
  //
  if ( getenv("U3NC_STEEL_GEN") ) {
    size_t  sys_i, lib_i;
    c3_c*   sys_c = _slurp(getenv("U3NC_STEEL_SYS"), &sys_i);
    c3_c*   lib_c = _slurp(getenv("U3NC_STEEL_LIB"), &lib_i);
    u3_noun cor, fol;
    u3_weak pro;

    if ( !sys_c || !lib_c ) return 1;
    cor = _gate(
      "|=  [sys=@t lib=@t]\n"
      "^-  [%steel p=(list)]\n"
      "=/  mist\n"
      "  |=  [bon=path txt=@]\n"
      "  ^-  hoon\n"
      "  =+  vas=vast\n"
      "  ~|  bon\n"
      "  %+  scan  (trip txt)\n"
      "  %-  full\n"
      "  =/  skip-ford\n"
      "    %-  star\n"
      "    ;~  pose  vul\n"
      "      %+  ifix  [fas (just `@`10)]\n"
      "      (star ;~(less (just `@`10) next))\n"
      "    ==\n"
      "  %+  ifix  [;~(plug gay skip-ford) gay]\n"
      "  (stag %tssg (most gap tall:vas(wer bon, bug &)))\n"
      "=/  trap\n"
      "  =/  sub  *(trap vase)\n"
      "  =.  sub  (swat sub (rain /sys/hoon/hoon sys))\n"
      "  (swat sub (mist /lib/nock-compilation/hoon lib))\n"
      "=/  nok  !.\n"
      "  =>  [=_trap ~]  !=\n"
      "  q:$:trap\n"
      "[%steel nok trap ~]\n",
      u3nc(u3i_bytes(sys_i, (c3_y*)sys_c), u3i_bytes(lib_i, (c3_y*)lib_c)));
    fol = u3nq(9, 2, 0, 1);
    pro = _run("steel: build", _nock_n, cor, fol);
    u3z(cor); u3z(fol);
    if ( u3_none == pro ) return 1;
    {
      u3_noun jam   = u3qe_jam(pro);
      c3_w    len_w = u3r_met(3, jam);
      c3_y*   buf_y = c3_malloc(len_w);
      FILE*   fil_u = fopen(getenv("U3NC_STEEL_GEN"), "wb");

      u3r_bytes(0, len_w, buf_y, jam);
      if ( !fil_u || (len_w != fwrite(buf_y, 1, len_w, fil_u)) ) {
        fprintf(stderr, "steel: can't write %s\r\n", getenv("U3NC_STEEL_GEN"));
        return 1;
      }
      fclose(fil_u);
      fprintf(stderr, "steel: %u bytes\r\n", len_w);
    }
    return 0;
  }

  //  U3NC_PROBE: the source of a gate on the kernel, run on the sample
  //  U3NC_PROBE_SAM (default 5) by both interpreters, and nothing else
  //
  //  U3NC_PROBE_OFF: cores whose jets to turn off first, comma-separated
  //
  if ( getenv("U3NC_PROBE") ) {
    c3_c*   sam_c = getenv("U3NC_PROBE_SAM");

    //  as the quick tests do, unless U3NC_ALL: else the stats would count
    //  the SKA core compiling itself
    //
    if ( !getenv("U3NC_ALL") ) {
      u3nc_Self = 0;
    }
    c3_c*   off_c = getenv("U3NC_PROBE_OFF");
    c3_c*   src_c = getenv("U3NC_PROBE");
    u3_noun sam   = sam_c ? atoi(sam_c) : 5;

    //  U3NC_PROBE=@file: the gate's source is that file's contents;
    //  U3NC_PROBE_FILE: the sample is that file's contents, as an atom
    //
    if ( '@' == src_c[0] ) {
      if ( !(src_c = _slurp(src_c + 1, 0)) ) return 1;
    }
    if ( getenv("U3NC_PROBE_FILE") ) {
      size_t len_i;
      c3_c*  buf_c = _slurp(getenv("U3NC_PROBE_FILE"), &len_i);

      if ( !buf_c ) return 1;
      sam = u3i_bytes(len_i, (c3_y*)buf_c);
      c3_free(buf_c);
    }

    u3_noun cor   = _gate(src_c, sam);

    if ( off_c ) {
      c3_c* key_c = c3_malloc(strlen(off_c) + 3);
      snprintf(key_c, strlen(off_c) + 3, ",%s,", off_c);
      u3h_walk_with(u3H->rod_u.ska.pax_p, _off_cb, key_c);
      u3d_hot();
      c3_free(key_c);
      fprintf(stderr, "probe: %" PRIc3_w " arms off\r\n", _off_w);
    }

    c3_i ret_i = _test("probe", cor, u3nq(9, 2, 0, 1), 1);

    //  U3NC_REGS: the argument counts of the programs compiled
    //
    if ( getenv("U3NC_REGS") ) {
      u3nc_regs();
    }

    //  U3NC_HOT=n: the n programs that made the most cells, and
    //  U3NC_HOT_DUMP=k of them in full (-Dska-stat)
    //
#ifdef U3NC_STAT
    if ( getenv("U3NC_HOT") ) {
      u3nc_hot(atoi(getenv("U3NC_HOT")),
               getenv("U3NC_HOT_DUMP") ? atoi(getenv("U3NC_HOT_DUMP")) : 0);
    }
#endif

    _stat_sum();
    return ret_i ? 0 : 1;
  }

  //  U3NC_MINT: a hoon file, parsed in u3n untimed as a list of hoons
  //  (like ford and the steel pill generator), then
  //  (~(mint ut -:!>(..ride)) %noun gen) in both interpreters, and nothing
  //  else
  //
  if ( getenv("U3NC_MINT") ) {
    FILE*   fil_u = fopen(getenv("U3NC_MINT"), "rb");
    c3_y*   buf_y;
    long    len_l;
    u3_noun txt, gen, cor;

    if ( !fil_u ) {
      fprintf(stderr, "mint: can't open %s\r\n", getenv("U3NC_MINT"));
      return 1;
    }
    fseek(fil_u, 0, SEEK_END);
    len_l = ftell(fil_u);
    fseek(fil_u, 0, SEEK_SET);
    buf_y = c3_malloc(len_l);
    if ( (size_t)len_l != fread(buf_y, 1, len_l, fil_u) ) {
      fprintf(stderr, "mint: short read\r\n");
      return 1;
    }
    fclose(fil_u);
    txt = u3i_bytes(len_l, buf_y);
    c3_free(buf_y);

    {
      //  U3NC_MINT_REAM: one hoon, as +ream parses it (hoon.hoon)
      //
      u3_noun gat = getenv("U3NC_MINT_REAM")
        ? u3v_wish("ream")
        : u3v_wish("|=  txt=@t\n"
                   "%+  scan  (trip txt)\n"
                   "%-  full\n"
                   "(ifix [gay gay] (stag %tssg (most gap tall:vast)))\n");
      u3_noun rem = u3nc(u3k(u3h(gat)), u3nc(txt, u3k(u3t(u3t(gat)))));
      u3_noun fol = u3nq(9, 2, 0, 1);
      u3_weak pro = _run("mint: parse", _nock_n, rem, fol);

      u3z(gat); u3z(rem); u3z(fol);
      if ( u3_none == pro ) {
        return 1;
      }
      gen = pro;
    }
    //  U3NC_MINT_SUT=noun: against %noun, as hoon.hoon is compiled
    //
    cor = ( getenv("U3NC_MINT_SUT") && !strcmp(getenv("U3NC_MINT_SUT"), "noun") )
        ? _gate("|=  gen=hoon\n"
                "(~(mint ut %noun) %noun gen)\n", gen)
        : _gate("=/  sut  -:!>(..ride)\n"
                "|=  gen=hoon\n"
                "(~(mint ut sut) %noun gen)\n", gen);
    {
      c3_i    ret_i = _test("mint", u3k(cor), u3nq(9, 2, 0, 1), 1);
      u3_noun fol   = u3nq(9, 2, 0, 1);

      //  U3NC_MINT_REPS: that many more compiled runs, for the steady
      //  state (a second run may still compile entries)
      //
      for ( c3_w i_w = 0;
            i_w < (getenv("U3NC_MINT_REPS") ? (c3_w)atoi(getenv("U3NC_MINT_REPS")) : 0);
            i_w++ )
      {
        u3z(_run("u3nc again ", _nock_nc, cor, fol));
      }
      u3z(fol);
      u3z(cor);
      return ret_i ? 0 : 1;
    }
  }

  //  U3NC_BOOT: the tests of u3d_boot() and u3d_hot(), and nothing else
  //
  if ( getenv("U3NC_BOOT") ) {
    return (  _test_hot("scot", "|=  n=@\n^-  @t\n(scot %ud n)\n", 5)
           && _test_rung()
           && _test_boot() )
           ? 0 : 1;
  }

  //  the quick tests run the SKA core in u3n: compiling its own code, on
  //  its first poke, would be most of their time
  //
  if ( !getenv("U3NC_ALL") ) {
    u3nc_Self = 0;
  }

  //  U3NC_JETS: the tests of the jets, and nothing else; =1 all of
  //  them, else a comma-separated list of their names
  //
  if ( getenv("U3NC_JETS") ) {
    c3_i ret_i = _test_jets(_jets_len());
    _stat_sum();
    return ret_i ? 0 : 1;
  }

  if ( !_test("inc", 42, u3nt(4, 0, 1), 0) ) {
    exit(1);
  }

  if ( !_test_ackermann() ) {
    exit(1);
  }

  if ( !_test_iter_dec() ) {
    exit(1);
  }

  if ( !_test_iter_vint() ) {
    exit(1);
  }

  if ( !_test_iter_add() ) {
    exit(1);
  }

  if ( !_test_scot_tail() ) {
    exit(1);
  }

  if ( !_test_scot() ) {
    exit(1);
  }

  if ( !_test_scot_loop() ) {
    exit(1);
  }

  if ( !_test_memo_toss() ) {
    exit(1);
  }

  if ( !_test_memo_keep() ) {
    exit(1);
  }

  if ( !_test_memo_jet() ) {
    exit(1);
  }

  if ( !_test_memo_mono() ) {
    exit(1);
  }

  if ( !_test_dies() ) {
    exit(1);
  }

  if ( !_test_twice() ) {
    exit(1);
  }

  if ( !_test_cycle() ) {
    exit(1);
  }

  if ( !_test_swap() ) {
    exit(1);
  }

  if ( !_test_partial() ) {
    exit(1);
  }

  if ( !_test_equal() ) {
    exit(1);
  }

  if ( !_test_fork() ) {
    exit(1);
  }

  if ( !_test_unused() ) {
    exit(1);
  }

  if ( !_test_bignum() ) {
    exit(1);
  }

  if ( !_test_rebuild() ) {
    exit(1);
  }

  if ( !_test_owned() ) {
    exit(1);
  }

  if ( !_test_sew() ) {
    exit(1);
  }

  if ( !_test_fast() ) {
    exit(1);
  }

  if ( !_test_flags() ) {
    exit(1);
  }

  if ( !_test_mean() ) {
    exit(1);
  }

  if ( !_test_ska() ) {
    exit(1);
  }

  if ( !_test_hot("scot", "|=  n=@\n^-  @t\n(scot %ud n)\n", 5) ) {
    exit(1);
  }

  if ( !_test_rung() ) {
    exit(1);
  }

  //  the slow tests, U3NC_ALL: each boot of the core recompiles it, and
  //  every jet compiles a caller of its own
  //
  if ( getenv("U3NC_ALL") ) {
    if ( !_test_boot() ) {
      exit(1);
    }

    if ( !_test_jets(_jets_len()) ) {
      exit(1);
    }
  }

  //  GC
  //
  u3m_grab(u3_none);

  _stat_sum();

  fprintf(stderr, "test nock compile: ok%s\r\n",
          getenv("U3NC_ALL") ? "" : " (quick: U3NC_ALL=1 for all of them)");
  return 0;
}
