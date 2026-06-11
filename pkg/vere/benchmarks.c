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
  u3m_boot_lite(1 << 24);
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
  _edit_bench();

  //  GC
  //
  u3m_grab(u3_none);

  return 0;
}
