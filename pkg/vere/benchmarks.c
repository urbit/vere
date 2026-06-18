/// @file

#include "noun.h"
#include "jets/q.h"
#include "ur/ur.h"
#include "vere.h"

#include <time.h>

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 25);
}

/* _bench_ns(): monotonic timestamp in nanoseconds.
*/
static c3_d
_bench_ns(void)
{
  struct timespec tim_u;
  clock_gettime(CLOCK_MONOTONIC, &tim_u);
  return ((c3_d)tim_u.tv_sec * 1000000000ULL) + (c3_d)tim_u.tv_nsec;
}

/* _bench_print(): print elapsed time under a label.
*/
static void
_bench_print(const c3_c* lab_c, c3_d ned_d)
{
  fprintf(stderr, "  %-18s %8.2f ms\r\n", lab_c, (double)ned_d / 1e6);
}

/* _ames_writ_ex(): |hi packet from fake ~zod to fake ~nec
*/
static u3_noun
_ames_writ_ex(void)
{
  c3_y  bod_y[63] = {
    0x30, 0x90, 0x2d,  0x0,  0x0,  0x0,  0x1,  0x0,  0x9, 0xc0, 0xd0,
     0x0,  0x4, 0x40, 0x30, 0xf4,  0xa, 0x3d, 0x45, 0x86, 0x66, 0x2c,
     0x2, 0x38, 0xf8, 0x72, 0xa3,  0x9, 0xf6,  0x6, 0xf3,  0x0, 0xbe,
    0x67, 0x61, 0x49, 0x50,  0x4, 0x3c, 0x13, 0xb2, 0x96, 0x42, 0x1b,
    0x62, 0xac, 0x97, 0xff, 0x24, 0xeb, 0x69, 0x1b, 0xb2, 0x60, 0x72,
     0xa, 0x53, 0xdf, 0xe8, 0x8a, 0x9c, 0x6f, 0xb3
  };
  u3_noun lan = u3nc(0, 1);
  u3_noun cad = u3nt(c3__send, lan, u3i_bytes(sizeof(bod_y), bod_y));
  u3_noun wir = u3nt(c3__newt, 0x1234, u3_nul);
  u3_noun ovo = u3nc(u3nc(u3_blip, wir), cad);
  u3_noun wen;

  {
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);
    wen = u3m_time_in_tv(&tim_u);
  }

  return u3nt(c3__work, 0, u3nc(wen, ovo));
}

static void
_jam_bench(void)
{
  c3_d  ber_d;
  c3_w  i_w, max_w = 10000;
  u3_noun wit = _ames_writ_ex();

  fprintf(stderr, "\r\njam microbenchmark:\r\n");

  {
    ber_d = _bench_ns();

    {
      u3i_slab sab_u;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        u3s_jam_fib(&sab_u, wit);
        u3i_slab_free(&sab_u);
      }
    }

    _bench_print("jam og:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      c3_d  len_d;
      c3_y* byt_y;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        u3s_jam_xeno(wit, &len_d, &byt_y);
        c3_free(byt_y);
      }
    }

    _bench_print("jam xeno:", _bench_ns() - ber_d);
  }

  while ( 1 ) {
    ur_root_t* rot_u = ur_root_init();
    c3_d       len_d;
    c3_y*      byt_y;
    ur_nref      ref;

    u3s_jam_xeno(wit, &len_d, &byt_y);
    if ( ur_cue_good != ur_cue(rot_u, len_d, byt_y, &ref) ) {
      fprintf(stderr, " jam bench: cue failed wtf\r\n");
      break;
    }

    c3_free(byt_y);

    {
      ber_d = _bench_ns();

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        ur_jam(rot_u, ref, &len_d, &byt_y);
        c3_free(byt_y);
      }

      _bench_print("jam cons:", _bench_ns() - ber_d);
    }

    {
      ber_d = _bench_ns();

      {
        ur_jam_t *jam_u = ur_jam_init(rot_u);
        c3_d      len_d;
        c3_y*     byt_y;

        for ( i_w = 0; i_w < max_w; i_w++ ) {
          ur_jam_with(jam_u, ref, &len_d, &byt_y);
          c3_free(byt_y);
        }

        ur_jam_done(jam_u);
      }

      _bench_print("jam cons with:", _bench_ns() - ber_d);
    }

    ur_root_free(rot_u);
    break;
  }

  u3z(wit);
}

static void
_cue_bench(void)
{
  c3_d  ber_d;
  c3_w  i_w, max_w = 20000;
  u3_atom vat = u3ke_jam(_ames_writ_ex());

  fprintf(stderr, "\r\ncue microbenchmark:\r\n");

  {
    ber_d = _bench_ns();

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      u3z(u3s_cue(vat));
    }

    _bench_print("cue og:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      u3z(u3s_cue_atom(vat));
    }

    _bench_print("cue atom:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      c3_w  len_w = u3r_met(3, vat);
      // XX assumes little-endian
      //
      c3_y* byt_y = ( c3y == u3a_is_cat(vat) )
                  ? (c3_y*)&vat
                  : (c3_y*)((u3a_atom*)u3a_to_ptr(vat))->buf_w;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        u3z(u3s_cue_xeno(len_w, byt_y));
      }
    }

    _bench_print("cue xeno:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      u3_cue_xeno* sil_u = u3s_cue_xeno_init();

      c3_w  len_w = u3r_met(3, vat);
      // XX assumes little-endian
      //
      c3_y* byt_y = ( c3y == u3a_is_cat(vat) )
                  ? (c3_y*)&vat
                  : (c3_y*)((u3a_atom*)u3a_to_ptr(vat))->buf_w;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        u3z(u3s_cue_xeno_with(sil_u, len_w, byt_y));
      }

      u3s_cue_xeno_done(sil_u);
    }

    _bench_print("cue xeno with:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      c3_w  len_w = u3r_met(3, vat);
      // XX assumes little-endian
      //
      c3_y* byt_y = ( c3y == u3a_is_cat(vat) )
                  ? (c3_y*)&vat
                  : (c3_y*)((u3a_atom*)u3a_to_ptr(vat))->buf_w;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        ur_cue_test(len_w, byt_y);
      }
    }

    _bench_print("cue test:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      ur_cue_test_t *t = ur_cue_test_init();

      c3_w  len_w = u3r_met(3, vat);
      // XX assumes little-endian
      //
      c3_y* byt_y = ( c3y == u3a_is_cat(vat) )
                  ? (c3_y*)&vat
                  : (c3_y*)((u3a_atom*)u3a_to_ptr(vat))->buf_w;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        ur_cue_test_with(t, len_w, byt_y);
      }

      ur_cue_test_done(t);
    }

    _bench_print("cue test with:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      ur_root_t* rot_u = ur_root_init();
      ur_nref      ref;
      c3_w  len_w = u3r_met(3, vat);
      // XX assumes little-endian
      //
      c3_y* byt_y = ( c3y == u3a_is_cat(vat) )
                  ? (c3_y*)&vat
                  : (c3_y*)((u3a_atom*)u3a_to_ptr(vat))->buf_w;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        ur_cue(rot_u, len_w, byt_y, &ref);
      }

      ur_root_free(rot_u);
    }

    _bench_print("cue cons:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    {
      ur_root_t* rot_u;
      ur_nref      ref;
      c3_w  len_w = u3r_met(3, vat);
      // XX assumes little-endian
      //
      c3_y* byt_y = ( c3y == u3a_is_cat(vat) )
                  ? (c3_y*)&vat
                  : (c3_y*)((u3a_atom*)u3a_to_ptr(vat))->buf_w;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        rot_u = ur_root_init();
        ur_cue(rot_u, len_w, byt_y, &ref);
        ur_root_free(rot_u);
      }
    }

    _bench_print("cue re-cons:", _bench_ns() - ber_d);
  }

  u3z(vat);
}

static u3_noun
_cue_loop(u3_atom a)
{
  c3_w i_w, max_w = 20000;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3z(u3s_cue(a));
  }

  return u3_blip;
}

static u3_noun
_cue_atom_loop(u3_atom a)
{
  c3_w i_w, max_w = 20000;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    u3z(u3s_cue_atom(a));
  }

  return u3_blip;
}

static void
_cue_soft_bench(void)
{
  c3_d ber_d;
  u3_atom vat = u3ke_jam(_ames_writ_ex());

  fprintf(stderr, "\r\ncue virtual microbenchmark:\r\n");

  {
    ber_d = _bench_ns();

    u3z(u3m_soft(0, _cue_loop, u3k(vat)));

    _bench_print("cue virtual og:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    u3z(u3m_soft(0, _cue_atom_loop, u3k(vat)));

    _bench_print("cue virtual atom:", _bench_ns() - ber_d);
  }

  u3z(vat);
}

/* _alloc_cons_work(): build and discard lists; arg=[rep len].
*/
static u3_noun
_alloc_cons_work(u3_noun arg)
{
  c3_w rep_w = u3h(arg);
  c3_w len_w = u3t(arg);
  c3_w i_w;

  for ( i_w = 0; i_w < rep_w; i_w++ ) {
    u3z(u3qb_reap(len_w, u3_blip));
  }

  return u3_blip;
}

/* _alloc_keep_work(): gain and lose an indirect noun; arg=[rep noun].
*/
static u3_noun
_alloc_keep_work(u3_noun arg)
{
  c3_w    rep_w = u3h(arg);
  u3_noun non   = u3t(arg);
  c3_w    i_w;

  for ( i_w = 0; i_w < rep_w; i_w++ ) {
    u3z(u3k(non));
  }

  return u3_blip;
}

/* _alloc_atom_work(): churn indirect atoms by repeated addition;
**                     arg=[rep bex].
*/
static u3_noun
_alloc_atom_work(u3_noun arg)
{
  c3_w    rep_w = u3h(arg);
  u3_noun big   = u3qc_bex(u3t(arg));
  u3_noun pro   = u3k(big);
  c3_w    i_w;

  for ( i_w = 0; i_w < rep_w; i_w++ ) {
    pro = u3ka_add(pro, u3k(big));
  }

  u3z(pro);
  u3z(big);
  return u3_blip;
}

/* _alloc_cue_work(): cue a jammed noun; arg=[rep vat].
*/
static u3_noun
_alloc_cue_work(u3_noun arg)
{
  c3_w    rep_w = u3h(arg);
  u3_atom vat   = u3t(arg);
  c3_w    i_w;

  for ( i_w = 0; i_w < rep_w; i_w++ ) {
    u3z(u3s_cue(vat));
  }

  return u3_blip;
}

/* _alloc_copy_work(): build a list to be copied off-road; arg=len.
*/
static u3_noun
_alloc_copy_work(u3_noun arg)
{
  return u3qb_reap(arg, u3_blip);
}

/* _alloc_run(): time [rep_w] u3m_soft() invocations of [fun_f].
*/
static c3_d
_alloc_run(u3_funk fun_f, u3_noun arg, c3_w rep_w)
{
  c3_d ber_d, end_d;
  c3_w i_w;

  ber_d = _bench_ns();

  for ( i_w = 0; i_w < rep_w; i_w++ ) {
    u3_noun pro = u3m_soft(0, fun_f, u3k(arg));

    if ( u3_blip != u3h(pro) ) {
      fprintf(stderr, "  alloc bench: workload failed\r\n");
    }

    u3z(pro);
  }

  end_d = _bench_ns();
  u3z(arg);
  return end_d - ber_d;
}

/* _alloc_run_home(): time [rep_w] direct invocations of [fun_f]
**                    on the home road.  [fun_f] must not consume
**                    or return allocations.
*/
static c3_d
_alloc_run_home(u3_funk fun_f, u3_noun arg, c3_w rep_w)
{
  c3_d ber_d, end_d;
  c3_w i_w;

  ber_d = _bench_ns();

  for ( i_w = 0; i_w < rep_w; i_w++ ) {
    u3z(fun_f(arg));
  }

  end_d = _bench_ns();
  u3z(arg);
  return end_d - ber_d;
}

/* _alloc_cell(): print a timing and its ratio to the bump baseline.
*/
static void
_alloc_cell(c3_d tim_d, c3_d bas_d)
{
  c3_c rat_c[16];

  snprintf(rat_c, sizeof(rat_c), "(%.2fx)",
           (double)tim_d / (double)bas_d);
  fprintf(stderr, "  %8.2f ms %-8s", (double)tim_d / 1e6, rat_c);
}

/* _alloc_row(): run a workload under both inner-road allocators,
**               and, if [hom_o], on the home road.
*/
static void
_alloc_row(const c3_c* lab_c,
           u3_funk     fun_f,
           u3_noun     arg,
           c3_w        rep_w,
           c3_o        hom_o)
{
  c3_d bum_d, pal_d, hom_d = 0;

  u3C.wag_w |= u3o_sand;
  bum_d = _alloc_run(fun_f, u3k(arg), rep_w);

  u3C.wag_w &= ~u3o_sand;
  pal_d = _alloc_run(fun_f, u3k(arg), rep_w);

  if ( c3y == hom_o ) {
    hom_d = _alloc_run_home(fun_f, u3k(arg), rep_w);
  }

  u3z(arg);

  fprintf(stderr, "  %-12s %8.2f ms", lab_c, (double)bum_d / 1e6);

  _alloc_cell(pal_d, bum_d);

  if ( c3y == hom_o ) {
    _alloc_cell(hom_d, bum_d);
  }
  else {
    fprintf(stderr, "  %11s %8s", "-", "");
  }

  fprintf(stderr, "\r\n");
}

/* _alloc_bench(): compare opt-in bump allocation (u3o_sand) against the
**                 default palloc free-list allocator on inner roads,
**                 with home-road timings for reference.
*/
static void
_alloc_bench(void)
{
  fprintf(stderr, "\r\nallocator microbenchmark "
                  "(inner roads, ratios vs. bump):\r\n");
  fprintf(stderr, "  %-12s %11s  %11s %8s  %11s\r\n",
                  "", "bump", "palloc", "", "home");

  _alloc_row("cons", _alloc_cons_work, u3nc(50, 10000), 10, c3y);

  _alloc_row("gain/lose", _alloc_keep_work,
             u3nc(1000000, u3i_string("a relatively unexceptional noun")),
             10, c3y);

  _alloc_row("atoms", _alloc_atom_work, u3nc(2000, 4096), 20, c3y);

  _alloc_row("cue", _alloc_cue_work,
             u3nc(2000, u3ke_jam(_ames_writ_ex())),
             10, c3y);

  //  no home-road run: the workload measures off-road copying
  //
  _alloc_row("copy out", _alloc_copy_work, 20000, 100, c3n);
}

static void
_edit_bench_impl(c3_w max_w)
{
  u3_assert( max_w && (c3y == u3a_is_cat(max_w)) );

  c3_w* axe_w = c3_calloc(((max_w + 31) >> 5) << 2);
  c3_w  bit_w;
  u3_noun lit = u3qb_reap(max_w, 1);
  u3_noun axe;

  axe_w[0] = bit_w = 2;

  do {
    axe = u3i_words((bit_w + 31) >> 5, axe_w);
    lit = u3i_edit(lit, axe, 2);
    u3z(axe);

    axe_w[bit_w >> 5] |= (c3_w)1 << (bit_w & 31);
    bit_w++;
  }
  while ( bit_w <= max_w );

  u3z(lit);
  c3_free(axe_w);
}

static void
_edit_bench(void)
{
  c3_d ber_d;

  fprintf(stderr, "\r\nopcode 10 microbenchmark:\r\n");

  {
    ber_d = _bench_ns();

    _edit_bench_impl(1000);

    _bench_print("opcode 10 1k list:", _bench_ns() - ber_d);
  }

  {
    ber_d = _bench_ns();

    _edit_bench_impl(10000);

    _bench_print("opcode 10 10k list:", _bench_ns() - ber_d);
  }
}

/* main(): run all benchmarks
*/
int
main(int argc, char* argv[])
{
  _setup();

  _jam_bench();
  _cue_bench();
  _cue_soft_bench();
  _alloc_bench();
  _edit_bench();

  //  GC
  //
  u3m_grab(u3_none);

  return 0;
}
