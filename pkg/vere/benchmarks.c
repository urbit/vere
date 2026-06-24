/// @file

#include "noun.h"
#include "jets/q.h"
#include "tracy.h"
#include "ur/ur.h"
#include "vere.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
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
  struct timeval b4, f2, d0;
  c3_w  mil_w, i_w, max_w = 10000;
  u3_noun wit = _ames_writ_ex();

  fprintf(stderr, "\r\njam microbenchmark:\r\n");

  {
    gettimeofday(&b4, 0);

    {
      u3i_slab sab_u;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        u3s_jam_fib(&sab_u, wit);
        u3i_slab_free(&sab_u);
      }
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  jam og: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

    {
      c3_d  len_d;
      c3_y* byt_y;

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        u3s_jam_xeno(wit, &len_d, &byt_y);
        c3_free(byt_y);
      }
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  jam xeno: %u ms\r\n", mil_w);
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
      gettimeofday(&b4, 0);

      for ( i_w = 0; i_w < max_w; i_w++ ) {
        ur_jam(rot_u, ref, &len_d, &byt_y);
        c3_free(byt_y);
      }

      gettimeofday(&f2, 0);
      timersub(&f2, &b4, &d0);
      mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
      fprintf(stderr, "  jam cons: %u ms\r\n", mil_w);
    }

    {
      gettimeofday(&b4, 0);

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

      gettimeofday(&f2, 0);
      timersub(&f2, &b4, &d0);
      mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
      fprintf(stderr, "  jam cons with: %u ms\r\n", mil_w);
    }

    ur_root_free(rot_u);
    break;
  }

  u3z(wit);
}

static void
_cue_bench(void)
{
  struct timeval b4, f2, d0;
  c3_w  mil_w, i_w, max_w = 20000;
  u3_atom vat = u3ke_jam(_ames_writ_ex());

  fprintf(stderr, "\r\ncue microbenchmark:\r\n");

  {
    gettimeofday(&b4, 0);

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      u3z(u3s_cue(vat));
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue og: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      u3z(u3s_cue_atom(vat));
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue atom: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

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

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue xeno: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

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

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue xeno with: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

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

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue test: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

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

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue test with: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

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

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue cons: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

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

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue re-cons: %u ms\r\n", mil_w);
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
  struct timeval b4, f2, d0;
  u3_atom vat = u3ke_jam(_ames_writ_ex());
  c3_w  mil_w;

  fprintf(stderr, "\r\ncue virtual microbenchmark:\r\n");

  {
    gettimeofday(&b4, 0);

    u3z(u3m_soft(0, _cue_loop, u3k(vat)));

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue virtual og: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

    u3z(u3m_soft(0, _cue_atom_loop, u3k(vat)));

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cue virtual atom: %u ms\r\n", mil_w);
  }

  u3z(vat);
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
  struct timeval b4, f2, d0;
  c3_w mil_w;

  fprintf(stderr, "\r\nopcode 10 microbenchmark:\r\n");

  {
    gettimeofday(&b4, 0);

    _edit_bench_impl(1000);

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  opcode 10 1k list: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

    _edit_bench_impl(10000);

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  opcode 10 10k list: %u ms\r\n", mil_w);
  }
}

/* _bench_dec_fol(): the canonical Nock decrement formula.
**
**   *[a FOL] == (a - 1), for a > 0.  Nock has no decrement opcode, so this is
**   a tight interpreter loop: per step, nock-6 (if), nock-5 (eq), nock-4
**   (increment) and a nock-9 self-kick (warm after the first lookup). It thus
**   exercises the bytecode dispatch loop (_n_burn) and the kick/jet-dispatch
**   path together — neither of which the jam/cue/edit benchmarks touch.
*/
static u3_noun
_bench_dec_fol(void)
{
  u3_noun a7  = u3nc(0, 7);                          //  [0 7]   sample (a)
  u3_noun a6  = u3nc(0, 6);                          //  [0 6]   counter
  u3_noun in6 = u3nt(4, 0, 6);                       //  [4 0 6] +(counter)
  u3_noun eqt = u3nt(5, u3k(a7), u3k(in6));          //  [5 [0 7] [4 0 6]]
  u3_noun cor = u3nc(u3nc(0, 2), u3nc(in6, a7));     //  [[0 2] [[4 0 6] [0 7]]]
  u3_noun rec = u3nt(9, 2, cor);                     //  [9 2 [...]]  rebuild+kick
  u3_noun lop = u3nq(6, eqt, a6, rec);               //  [6 eq [0 6] rec]
  u3_noun ic8 = u3nt(8, u3nc(1, lop), u3nq(9, 2, 0, 1));
  return u3nt(8, u3nc(1, 0), ic8);                   //  [8 [1 0] [8 [1 lop] [9 2 0 1]]]
}

static u3_noun _dec_fol_g = 0;

/* _dec_run(): u3m_soft-compatible decrement of [a].
*/
static u3_noun
_dec_run(u3_noun a)
{
  return u3n_nock_on(a, u3k(_dec_fol_g));
}

/* _nock_bench(): pure-interpreter (Nock dispatch + kick) microbenchmark.
*/
static void
_nock_bench(void)
{
  struct timeval b4, f2, d0;
  c3_w mil_w;
  c3_w cnt_w = 200000;

  fprintf(stderr, "\r\nnock interpreter microbenchmark:\r\n");

  _dec_fol_g = _bench_dec_fol();

  //  validate the formula (soft, so a mistake can't crash the suite)
  //
  {
    u3_noun tot = u3m_soft(0, _dec_run, 7);
    c3_o     ok = c3n;

    if ( (c3y == u3a_is_cell(tot)) && (0 == u3h(tot)) ) {
      ok = ( 6 == u3t(tot) ) ? c3y : c3n;
    }
    u3z(tot);

    if ( c3n == ok ) {
      fprintf(stderr, "  nock dec: formula invalid, skipping\r\n");
      u3z(_dec_fol_g);
      _dec_fol_g = 0;
      return;
    }
  }

  {
    gettimeofday(&b4, 0);

    u3z(u3m_soft(0, _dec_run, cnt_w));   //  dec(cnt_w): cnt_w loop iterations

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  nock dec %uk loop: %u ms\r\n", cnt_w / 1000, mil_w);
  }

  u3z(_dec_fol_g);
  _dec_fol_g = 0;
}

static u3_noun _slam_gate_g = 0;

/* _slam_run(): u3m_soft-compatible slam of the benchmark gate.
*/
static u3_noun
_slam_run(u3_noun sam)
{
  return u3n_slam_on(u3k(_slam_gate_g), sam);
}

/* _slam_bench(): nock-9 kick / jet-dispatch microbenchmark.
**
**   Builds a trivial gate (arm = +(sample)) and slams it repeatedly. The arm
**   is tiny, so this isolates the per-call kick path: core construction,
**   u3j_kick warm/cold lookup, and bytecode dispatch.
*/
static void
_slam_bench(void)
{
  struct timeval b4, f2, d0;
  c3_w  mil_w, i_w, max_w = 500000;
  //  gate = [arm [sample context]] ; arm [4 0 6] = +(sample)
  //
  u3_noun gat = u3nc(u3nt(4, 0, 6), u3nc(0, 0));

  fprintf(stderr, "\r\nnock-9 slam (dispatch) microbenchmark:\r\n");

  _slam_gate_g = gat;

  //  validate (soft)
  //
  {
    u3_noun tot = u3m_soft(0, _slam_run, 5);
    c3_o     ok = ( (c3y == u3a_is_cell(tot)) && (0 == u3h(tot))
                    && (6 == u3t(tot)) ) ? c3y : c3n;
    u3z(tot);
    if ( c3n == ok ) {
      fprintf(stderr, "  slam: gate invalid, skipping\r\n");
      u3z(gat);
      _slam_gate_g = 0;
      return;
    }
  }

  {
    gettimeofday(&b4, 0);

    for ( i_w = 0; i_w < max_w; i_w++ ) {
      u3z(u3n_slam_on(u3k(gat), i_w));
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  slam 500k: %u ms\r\n", mil_w);
  }

  u3z(gat);
  _slam_gate_g = 0;
}

/* _alloc_list(): build a list of [len_w] cells: [len-1 [len-2 ... [0 0]]].
*/
static u3_noun
_alloc_list(c3_w len_w)
{
  u3_noun lit = 0;
  c3_w    i_w;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    lit = u3nc(i_w, lit);
  }
  return lit;
}

/* _alloc_bench(): allocator churn (u3a_celloc + free + free-list reuse).
**
**   perf shows the suite is allocation/page-fault-bound; this isolates the
**   cell allocate/free fast path. Sized to fit the 16MB lite loom.
*/
static void
_alloc_bench(void)
{
  struct timeval b4, f2, d0;
  c3_w  mil_w, i_w;
  c3_w  len_w = 100000;   //  ~1.6MB of cells; fits, leaves headroom
  c3_w  rep_w = 10;

  fprintf(stderr, "\r\nallocator churn microbenchmark:\r\n");

  {
    gettimeofday(&b4, 0);

    for ( i_w = 0; i_w < rep_w; i_w++ ) {
      u3z(_alloc_list(len_w));
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  cons/free 100k x10: %u ms\r\n", mil_w);
  }

  {
    gettimeofday(&b4, 0);

    for ( i_w = 0; i_w < rep_w; i_w++ ) {
      u3z(u3qb_reap(len_w, 0));
    }

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  reap/free 100k x10: %u ms\r\n", mil_w);
  }
}

/* _bench_ack_fol(): the Ackermann function as a Nock formula.
**
**   *[ [m n] FOL ] == ack(m, n), where
**     ack(0,n) = n+1 ; ack(m,0) = ack(m-1,1) ;
**     ack(m,n) = ack(m-1, ack(m,n-1))
**
**   Doubly-recursive and non-tail: unlike dec it drives the interpreter's
**   frame stack deep, so it stresses kick/dispatch under deep recursion. The
**   core is [BAT [m n]] (m at axis 6, n at axis 7); dec(m)/dec(n) reuse the
**   validated decrement formula via Nock-7 composition.
*/
static u3_noun
_bench_ack_fol(void)
{
  u3_noun dec  = _bench_dec_fol();
  u3_noun decm = u3nt(7, u3nc(0, 6), u3k(dec));      //  [7 [0 6] DEC]  = m-1
  u3_noun decn = u3nt(7, u3nc(0, 7), dec);           //  [7 [0 7] DEC]  = n-1

  //  inner = ack(m, n-1)  ; recurse with core [BAT [m (n-1)]]
  u3_noun inr = u3nt(9, 2, u3nc(u3nc(0, 2), u3nc(u3nc(0, 6), decn)));
  //  els  = ack(m-1, ack(m,n-1))  ; recurse with [BAT [(m-1) inner]]
  u3_noun els = u3nt(9, 2, u3nc(u3nc(0, 2), u3nc(u3k(decm), inr)));
  //  th2  = ack(m-1, 1)           ; recurse with [BAT [(m-1) 1]]
  u3_noun th2 = u3nt(9, 2, u3nc(u3nc(0, 2), u3nc(decm, u3nc(1, 1))));

  u3_noun n0  = u3nt(5, u3nc(1, 0), u3nc(0, 7));     //  n == 0 ?
  u3_noun in6 = u3nq(6, n0, th2, els);              //  [6 (n==0) th2 els]
  u3_noun m0  = u3nt(5, u3nc(1, 0), u3nc(0, 6));     //  m == 0 ?
  u3_noun bat = u3nq(6, m0, u3nt(4, 0, 7), in6);     //  [6 (m==0) [4 0 7] in6]

  return u3nt(8, u3nc(1, bat), u3nq(9, 2, 0, 1));    //  [8 [1 BAT] [9 2 0 1]]
}

static u3_noun _ack_fol_g = 0;

/* _ack_run(): u3m_soft-compatible ack([m n]).
*/
static u3_noun
_ack_run(u3_noun mn)
{
  return u3n_nock_on(mn, u3k(_ack_fol_g));
}

/* _ack_bench(): Ackermann — deep doubly-recursive interpreter stress.
*/
static void
_ack_bench(void)
{
  struct timeval b4, f2, d0;
  c3_w mil_w;

  fprintf(stderr, "\r\nackermann microbenchmark:\r\n");

  _ack_fol_g = _bench_ack_fol();

  //  validate ack(2,2) == 7 (soft)
  //
  {
    u3_noun tot = u3m_soft(0, _ack_run, u3nc(2, 2));
    c3_o     ok = ( (c3y == u3a_is_cell(tot)) && (0 == u3h(tot))
                    && (7 == u3t(tot)) ) ? c3y : c3n;
    u3z(tot);
    if ( c3n == ok ) {
      fprintf(stderr, "  ack: formula invalid, skipping\r\n");
      u3z(_ack_fol_g);
      _ack_fol_g = 0;
      return;
    }
  }

  //  ack(3,5) == 253 ; ~42k recursive calls.  Bump to (3,6)/(3,7) for a
  //  heavier deep-recursion stress (≈170k / ≈690k calls).
  //
  {
    gettimeofday(&b4, 0);

    u3z(u3m_soft(0, _ack_run, u3nc(3, 5)));

    gettimeofday(&f2, 0);
    timersub(&f2, &b4, &d0);
    mil_w = (d0.tv_sec * 1000) + (d0.tv_usec / 1000);
    fprintf(stderr, "  ack(3,5): %u ms\r\n", mil_w);
  }

  u3z(_ack_fol_g);
  _ack_fol_g = 0;
}

/* main(): run all benchmarks
*/
int
main(int argc, char* argv[])
{
  _setup();

  u3_tc_msg("vere microbenchmarks");

  //  each group is an outer Tracy zone; runtime entry-point zones
  //  (u3s_jam_*, u3s_cue*, u3n_nock_on) nest inside. one frame per group.
  //
  { u3_tc_zone_named(zon, "bench:jam");
    _jam_bench();      u3_tc_zone_end(zon); u3_tc_frame_named("jam"); }
  { u3_tc_zone_named(zon, "bench:cue");
    _cue_bench();      u3_tc_zone_end(zon); u3_tc_frame_named("cue"); }
  { u3_tc_zone_named(zon, "bench:cue_soft");
    _cue_soft_bench(); u3_tc_zone_end(zon); u3_tc_frame_named("cue_soft"); }
  { u3_tc_zone_named(zon, "bench:edit");
    _edit_bench();     u3_tc_zone_end(zon); u3_tc_frame_named("edit"); }
  { u3_tc_zone_named(zon, "bench:nock");
    _nock_bench();     u3_tc_zone_end(zon); u3_tc_frame_named("nock"); }
  { u3_tc_zone_named(zon, "bench:ack");
    _ack_bench();      u3_tc_zone_end(zon); u3_tc_frame_named("ack"); }
  { u3_tc_zone_named(zon, "bench:slam");
    _slam_bench();     u3_tc_zone_end(zon); u3_tc_frame_named("slam"); }
  { u3_tc_zone_named(zon, "bench:alloc");
    _alloc_bench();    u3_tc_zone_end(zon); u3_tc_frame_named("alloc"); }

  //  GC
  //
  u3m_grab(u3_none);

  return 0;
}
