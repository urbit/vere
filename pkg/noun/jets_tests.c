/// @file

#include "noun.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);
}

#define _neq_etch_out(sa, sb, len) ((strlen((sa)) != strlen((sb))) || (0 != strncmp((sa), (sb), (len))))

static inline c3_i
_da_etch(mpz_t a_mp, const c3_c* dat_c)
{
  u3_atom dat = u3i_mp(a_mp);
  c3_c*  out_c;
  c3_i   ret_i = 1;
  size_t len_i;

  len_i = u3s_etch_da_c(dat, &out_c);

  if ( _neq_etch_out(dat_c, out_c, len_i) ) {
    fprintf(stderr, "etch_da: 0x");
    mpz_out_str(stderr, 16, a_mp);
    fprintf(stderr, " fail; expected %s, got '%s'\r\n",
                    dat_c, out_c);
    ret_i = 0;
  }

  else {
    u3_noun out = u3s_etch_da(dat);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_da: 0x");
      mpz_out_str(stderr, 16, a_mp);
      fprintf(stderr, " mismatch; expected %s\r\n", dat_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(dat);

  return ret_i;
}

#define _init_date_atom(mp, hi, lo) \
  { \
    mpz_init(mp); \
    mpz_set_ui(dat_mp, hi); \
    mpz_mul_2exp(dat_mp, dat_mp, 64); \
    mpz_add_ui(dat_mp, dat_mp, lo); \
  } \

#define _init_date_atom_big(mp, ex, hi, lo) \
  { \
    mpz_init(mp); \
    mpz_set_ui(dat_mp, ex); \
    mpz_mul_2exp(dat_mp, dat_mp, 64); \
    mpz_add_ui(dat_mp, dat_mp, hi); \
    mpz_mul_2exp(dat_mp, dat_mp, 64); \
    mpz_add_ui(dat_mp, dat_mp, lo); \
  } \

static c3_i
_test_etch_da(void)
{
  mpz_t dat_mp;
  c3_i ret_i = 1;

  // In the beginning was the Word
  _init_date_atom(dat_mp, 0x0, 0x0);
  ret_i &= _da_etch(dat_mp, "~292277024401-.1.1");

  // the Word was with God
  _init_date_atom(dat_mp,
      0x7ffffffe58e40f80,
      0xbabe000000000000);
  ret_i &= _da_etch(dat_mp, "~1.12.25..00.00.00..babe");

  // and the Word was God - John 1:1
  _init_date_atom(dat_mp,
      0x7ffffffe93b72f70,
      0x3300000000000000)
  ret_i &= _da_etch(dat_mp, "~33.4.3..15.00.00..3300");

  // Test fractional seconds
  //
  _init_date_atom(dat_mp,
      0x8000000d32bb462f,
      0xcafe000000000000);
  ret_i &= _da_etch(dat_mp, "~2023.3.24..05.44.15..cafe");

  _init_date_atom(dat_mp,
      0x8000000d32bb462f,
      0x0000cafe00000000);
  ret_i &= _da_etch(dat_mp, "~2023.3.24..05.44.15..0000.cafe");

  _init_date_atom(dat_mp,
      0x8000000d32bb462f,
      0x00000000cafe0000);
  ret_i &= _da_etch(dat_mp, "~2023.3.24..05.44.15..0000.0000.cafe");

  _init_date_atom(dat_mp,
      0x8000000d32bb462f,
      0x000000000000cafe);
  ret_i &= _da_etch(dat_mp, "~2023.3.24..05.44.15..0000.0000.0000.cafe");

  // General tests
  //
  _init_date_atom(dat_mp,
      0x8000000d329d6f76,
      0xadef000000000000);
  ret_i &= _da_etch(dat_mp, "~2023.3.1..14.32.22..adef");

  _init_date_atom(dat_mp,
      0x8000000d32c33b88,
      0x2d00000000000000);
  ret_i &= _da_etch(dat_mp, "~2023.3.30..06.36.56..2d00");

  _init_date_atom(dat_mp,
      0x8000000d32c51c00,
      0x2d00000000000000);
  ret_i &= _da_etch(dat_mp, "~2023.3.31..16.46.56..2d00");

  _init_date_atom(dat_mp,
      0x8000000d3a19f0c0,
      0x2d00000000000000);
  ret_i &= _da_etch(dat_mp, "~2027.2.22..07.26.56..2d00");

  _init_date_atom(dat_mp,
      0x80000029dd78fec0,
      0x2d00000000000000);
  ret_i &= _da_etch(dat_mp, "~5924.11.10..10.06.56..2d00");

  _init_date_atom(dat_mp,
      0x8000700808c7aec0,
      0x2d00000000000000);
  ret_i &= _da_etch(dat_mp, "~3903639.9.11..12.46.56..2d00");

  _init_date_atom_big(dat_mp,
      0xcafeabcd,
      0x8000000d330a6fca,
      0xd022000000000000);
  ret_i &= _da_etch(dat_mp, "~1990808568848630424650.2.5..23.52.42..d022");

  _init_date_atom_big(dat_mp,
      0xcafeabcd,
      0x8000000d330a6fca,
      0xd0220000cafe0000);
  ret_i &= _da_etch(dat_mp, "~1990808568848630424650.2.5..23.52.42..d022.0000.cafe");

  return ret_i;
}

static inline c3_i
_p_etch(c3_d pun_d, const c3_c* pun_c)
{
  u3_atom pun = u3i_chub(pun_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_p_c(pun, &out_c);
  c3_i   ret_i = 1;

  if ( _neq_etch_out(out_c, pun_c, len_i) ) {
    fprintf(stderr, "etch_p: %" PRIu64 " fail; expected %s, got '%s'\r\n",
                    pun_d, pun_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_p(pun);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_p: %" PRIu64 " mismatch; expected %s\r\n", pun_d, pun_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(pun);

  return ret_i;
}
static c3_i
_big_p_etch(c3_c* big_str, c3_c* pun_c)
{
  size_t ret_i = 1;

  u3_atom big;
  mpz_t big_mp;

  mpz_init(big_mp);
  mpz_set_str(big_mp, big_str, 16);

  big = u3i_mp(big_mp);

  c3_c*  out_c;
  size_t len_i = u3s_etch_p_c(big, &out_c);

  if ( _neq_etch_out(pun_c, out_c, len_i) ) {
    fprintf(stderr, "etch_p_big: %s fail; expected %s, got '%s'\r\n",
        big_str, pun_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_p(big);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_ud: %s mismatch; expected %s\r\n", big_str, pun_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(big);

  return ret_i;
}

static c3_i
_test_etch_p(void)
{
  c3_i ret_i = 1;

  ret_i &= _p_etch(0x0, "~zod");

  ret_i &= _p_etch(0x3, "~wes");
  ret_i &= _p_etch(0x17, "~dep");
  ret_i &= _p_etch(0x29, "~led");
  ret_i &= _p_etch(0xbf, "~myl");
  ret_i &= _p_etch(0xcf, "~wel");

  ret_i &= _p_etch(0xff, "~fes");

  ret_i &= _p_etch(0x1cc, "~marryd");
  ret_i &= _p_etch(0x2513, "~dalnup");
  ret_i &= _p_etch(0x753b, "~dacwyc");
  ret_i &= _p_etch(0xb365, "~dibwet");
  ret_i &= _p_etch(0xdcaa, "~rislep");

  ret_i &= _p_etch(0xffff, "~fipfes");

  ret_i &= _p_etch(0x6d2030, "~hocmeb-dapsen");
  ret_i &= _p_etch(0x108deca3, "~divbud-ladbyn");
  ret_i &= _p_etch(0x64f4eace, "~mopten-hilfex");
  ret_i &= _p_etch(0xa1ae3130, "~tinbyn-fammun");
  ret_i &= _p_etch(0xb91f853a, "~dinnex-sonnum");

  ret_i &= _p_etch(0xffffffff, "~dostec-risfen");

  ret_i &= _p_etch(0x6bfc3f1881b, "~sigmyl-bintus-sovpet");
  ret_i &= _p_etch(0x46f6e0458bc7, "~novweg-bilnet-radfep");
  ret_i &= _p_etch(0xab36928a695b, "~boswyd-lagdut-tobhes");
  ret_i &= _p_etch(0xe1a670e9eebd, "~larpub-bacfus-nisbex");
  ret_i &= _p_etch(0xf6b014781344, "~tonbyl-dasryg-bitlen");

  ret_i &= _p_etch(0xffffffffffff, "~fipfes-dostec-risfen");

  ret_i &= _p_etch(0x94fede64d31f2a0, "~lisnet-rivnys-natdem-donful");
  ret_i &= _p_etch(0xb39638ae3f909214, "~dibryg-bichut-witsev-fanpub");
  ret_i &= _p_etch(0xd226683f5a2fa433, "~nilsul-picpur-nocsem-tasrys");
  ret_i &= _p_etch(0xd5bc5e03458e7790, "~fopbyt-worwes-rolput-nodruc");
  ret_i &= _p_etch(0xe203169849fc1124, "~fitwes-hopfep-bitwyd-doswer");

  ret_i &= _p_etch(0xffffffffffffffff, "~fipfes-fipfes-dostec-risfen");

  ret_i &= _big_p_etch("b46c9c7817150a23781c15d2c20c2f3",
      "~dirrul-radner-dister-ritnus--taddus-digmus-torlun-filnyt");
  ret_i &= _big_p_etch("9fd9c878ceb2bdd0db35376f6b8b495f",
      "~patdun-hinfel-fadpem-tocnyd--nactyl-tadpel-balrev-tipsym");
  ret_i &= _big_p_etch("c9ee5fffc61ce2d923149c405dd2ab6c",
      "~radbec-sipfes-harryt-fitdun--rovheb-haprys-morrel-bostux");
  ret_i &= _big_p_etch("5172f9231ededdfaa56cddc363bd703d",
      "~podmeb-sovsep-silryl-fotrep--pintux-fotfex-wictyp-sivder");
  ret_i &= _big_p_etch("54c36542cc9f897c1b8587e4dc95fff1",
      "~tolfex-watden-ragmer-navren--folseg-lonryc-rispyx-fiptes");

  ret_i &= _big_p_etch("4524acf6cfe1ed7c974843bb918e0cae4c01", "~locpes--batweg-toplys-rivren-ripreg--moldyt-hadted-wachut-witnec");

  ret_i &= _big_p_etch("5711709a8c188c2ca4439727e342dc3f20e8b66769e1", "~napsyx-sivtus-nibdys--nibwen-sonnut-ripped-walden--rispur-hollyn-bitmyn-davlys");

  return ret_i;
}

static inline c3_i
_ud_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_ud_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( _neq_etch_out(num_c, out_c, len_i) ) {
    fprintf(stderr, "etch_ud: %" PRIu64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_ud(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_ud: %" PRIu64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_ud(void)
{
  c3_i ret_i = 1;

  ret_i &= _ud_etch(0, "0");
  ret_i &= _ud_etch(1, "1");
  ret_i &= _ud_etch(12, "12");
  ret_i &= _ud_etch(123, "123");
  ret_i &= _ud_etch(1234, "1.234");
  ret_i &= _ud_etch(12345, "12.345");
  ret_i &= _ud_etch(123456, "123.456");
  ret_i &= _ud_etch(1234567, "1.234.567");
  ret_i &= _ud_etch(12345678, "12.345.678");
  ret_i &= _ud_etch(123456789, "123.456.789");
  ret_i &= _ud_etch(100000000, "100.000.000");
  ret_i &= _ud_etch(101101101, "101.101.101");
  ret_i &= _ud_etch(201201201, "201.201.201");
  ret_i &= _ud_etch(302201100, "302.201.100");

  ret_i &= _ud_etch(8589934592ULL, "8.589.934.592");
  ret_i &= _ud_etch(2305843009213693952ULL, "2.305.843.009.213.693.952");
  ret_i &= _ud_etch(18446744073709551615ULL, "18.446.744.073.709.551.615");

  {
    c3_c* num_c = "340.282.366.920.938.463.463.374.607.431.768.211.456";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_ud_c(num, &out_c);

    if ( _neq_etch_out(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_ud: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_ud(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_ud: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_ui_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_ui_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( _neq_etch_out(num_c, out_c, len_i) ) {
    fprintf(stderr, "etch_ui: %" PRIu64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_ui(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_ui: %" PRIu64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_ui(void)
{
  c3_i ret_i = 1;

  ret_i &= _ui_etch(0, "0i0");
  ret_i &= _ui_etch(1, "0i1");
  ret_i &= _ui_etch(12, "0i12");
  ret_i &= _ui_etch(123, "0i123");
  ret_i &= _ui_etch(1234, "0i1234");
  ret_i &= _ui_etch(12345, "0i12345");
  ret_i &= _ui_etch(123456, "0i123456");
  ret_i &= _ui_etch(1234567, "0i1234567");
  ret_i &= _ui_etch(12345678, "0i12345678");
  ret_i &= _ui_etch(123456789, "0i123456789");
  ret_i &= _ui_etch(100000000, "0i100000000");
  ret_i &= _ui_etch(101101101, "0i101101101");
  ret_i &= _ui_etch(201201201, "0i201201201");
  ret_i &= _ui_etch(302201100, "0i302201100");

  ret_i &= _ui_etch(8589934592ULL, "0i8589934592");
  ret_i &= _ui_etch(2305843009213693952ULL, "0i2305843009213693952");
  ret_i &= _ui_etch(18446744073709551615ULL, "0i18446744073709551615");

  {
    c3_c* num_c = "0i340282366920938463463374607431768211456";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_ui_c(num, &out_c);

    if ( _neq_etch_out(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_ui: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_ui(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_ui: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_ux_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_ux_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( _neq_etch_out(num_c, out_c, len_i) ) {
    fprintf(stderr, "etch_ux: 0x%" PRIx64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_ux(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_ux: 0x%" PRIx64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_ux(void)
{
  c3_i ret_i = 1;

  ret_i &= _ux_etch(0x0, "0x0");
  ret_i &= _ux_etch(0x1, "0x1");
  ret_i &= _ux_etch(0x12, "0x12");
  ret_i &= _ux_etch(0x123, "0x123");
  ret_i &= _ux_etch(0x1234, "0x1234");
  ret_i &= _ux_etch(0x12345, "0x1.2345");
  ret_i &= _ux_etch(0x123456, "0x12.3456");
  ret_i &= _ux_etch(0x1234567, "0x123.4567");
  ret_i &= _ux_etch(0x12345678, "0x1234.5678");
  ret_i &= _ux_etch(0x123456789, "0x1.2345.6789");
  ret_i &= _ux_etch(0x100000000, "0x1.0000.0000");
  ret_i &= _ux_etch(0x101101101, "0x1.0110.1101");
  ret_i &= _ux_etch(0x201201201, "0x2.0120.1201");
  ret_i &= _ux_etch(0x302201100, "0x3.0220.1100");

  ret_i &= _ux_etch(0x123456789abcdefULL, "0x123.4567.89ab.cdef");
  ret_i &= _ux_etch(0x8589934592ULL, "0x85.8993.4592");
  ret_i &= _ux_etch(0x5843009213693952ULL, "0x5843.0092.1369.3952");
  ret_i &= _ux_etch(0x6744073709551615ULL, "0x6744.0737.0955.1615");

  {
    c3_c* num_c = "0x1.0000.0000.0000.0000.0000.0000.0000.0000";
    u3_atom num = u3qc_bex(128);
    c3_c*  out_c;
    size_t len_i = u3s_etch_ux_c(num, &out_c);

    if ( _neq_etch_out(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_ux: (bex 128) fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }
    else {
      u3_noun out = u3s_etch_ux(num);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_ux: (bex 128) mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(num);
  }

  return ret_i;
}

static inline c3_i
_uv_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_uv_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( _neq_etch_out(num_c, out_c, len_i) ) {
    fprintf(stderr, "etch_uv: 0x%" PRIx64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_uv(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_uv: 0x%" PRIx64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_uv(void)
{
  c3_i ret_i = 1;

  ret_i &= _uv_etch(0x0, "0v0");
  ret_i &= _uv_etch(0x1, "0v1");
  ret_i &= _uv_etch(0x10, "0vg");
  ret_i &= _uv_etch(0x12, "0vi");
  ret_i &= _uv_etch(0x123, "0v93");
  ret_i &= _uv_etch(0x1234, "0v4hk");
  ret_i &= _uv_etch(0x12345, "0v28q5");
  ret_i &= _uv_etch(0x123456, "0v14d2m");
  ret_i &= _uv_etch(0x1234567, "0vi6hb7");
  ret_i &= _uv_etch(0x12345678, "0v9.38ljo");
  ret_i &= _uv_etch(0x123456789, "0v4h.kaps9");
  ret_i &= _uv_etch(0x100000000, "0v40.00000");
  ret_i &= _uv_etch(0x101101101, "0v40.h0481");
  ret_i &= _uv_etch(0x201201201, "0v80.i04g1");
  ret_i &= _uv_etch(0x302201100, "0vc1.20480");

  ret_i &= _uv_etch(0x123456789abcdefULL, "0v28.q5cu4.qnjff");
  ret_i &= _uv_etch(0x8589934592ULL, "0vgm4.p6hci");
  ret_i &= _uv_etch(0x5843009213693952ULL, "0v5gg.o0i89.mieai");
  ret_i &= _uv_etch(0x6744073709551615ULL, "0v6eh.076s4.la5gl");

  {
    c3_c* hex_c = "0x1.1234.5678.9abc.def0.1234.5678.9abc.def0";
    c3_c* num_c = "0v8.i6hb7.h6lsr.ro14d.2mf2d.bpnng";

    u3_noun hot = u3i_bytes(strlen(hex_c), (c3_y*)hex_c);
    u3_weak hou = u3s_sift_ux(hot);

    if ( u3_none == hou ) {
      fprintf(stderr, "etch_uv: big hex fail\r\n");
      ret_i = 0;
    }

    c3_c*  out_c;
    size_t len_i = u3s_etch_uv_c(hou, &out_c);

    if ( _neq_etch_out(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_uv: big viz fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }

    else {
      u3_noun out = u3s_etch_uv(hou);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_uv: big viz mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(hot);
    u3z(hou);
  }

  return ret_i;
}

static inline c3_i
_uw_etch(c3_d num_d, const c3_c* num_c)
{
  u3_atom  num = u3i_chub(num_d);
  c3_c*  out_c;
  size_t len_i = u3s_etch_uw_c(num, &out_c);
  c3_i   ret_i = 1;

  if ( _neq_etch_out(num_c, out_c, len_i) ) {
    fprintf(stderr, "etch_uw: 0x%" PRIx64 " fail; expected %s, got '%s'\r\n",
                    num_d, num_c, out_c);
    ret_i = 0;
  }
  else {
    u3_noun out = u3s_etch_uw(num);
    u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

    if ( c3n == u3r_sing(tou, out) ) {
      fprintf(stderr, "etch_uw: 0x%" PRIx64 " mismatch; expected %s\r\n", num_d, num_c);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
    u3z(tou);
  }

  c3_free(out_c);
  u3z(num);

  return ret_i;
}

static c3_i
_test_etch_uw(void)
{
  c3_i ret_i = 1;

  ret_i &= _uw_etch(0x0, "0w0");
  ret_i &= _uw_etch(0x1, "0w1");
  ret_i &= _uw_etch(0x10, "0wg");
  ret_i &= _uw_etch(0x12, "0wi");
  ret_i &= _uw_etch(0x123, "0w4z");
  ret_i &= _uw_etch(0x1234, "0w18Q");
  ret_i &= _uw_etch(0x12345, "0wid5");
  ret_i &= _uw_etch(0x123456, "0w4zhm");
  ret_i &= _uw_etch(0x1234567, "0w18QlD");
  ret_i &= _uw_etch(0x12345678, "0wid5pU");
  ret_i &= _uw_etch(0x123456789, "0w4.zhmu9");
  ret_i &= _uw_etch(0x100000000, "0w4.00000");
  ret_i &= _uw_etch(0x101101101, "0w4.14141");
  ret_i &= _uw_etch(0x201201201, "0w8.18181");
  ret_i &= _uw_etch(0x302201100, "0wc.28140");

  ret_i &= _uw_etch(0x123456789abcdefULL, "0w4zhmu.9GYTL");
  ret_i &= _uw_etch(0x8589934592ULL, "0w8m.9AQmi");
  ret_i &= _uw_etch(0x5843009213693952ULL, "0w5.x3098.jqjBi");
  ret_i &= _uw_etch(0x6744073709551615ULL, "0w6.t41Ps.9lhol");

  {
    c3_c* num_hex_c = "0x1.1234.5678.9abc.def0.1234.5678.9abc.def0";
    c3_c* num_c = "0w4i.d5pUC.HPuY1.8QlDy.qLdXM";

    u3_noun hot = u3i_bytes(strlen(num_hex_c), (c3_y*)num_hex_c);
    u3_weak hou = u3s_sift_ux(hot);

    if ( u3_none == hou ) {
      fprintf(stderr, "etch_uw: big hex fail\r\n");
      ret_i = 0;
    }

    c3_c*  out_c;
    size_t len_i = u3s_etch_uw_c(hou, &out_c);

    if ( _neq_etch_out(num_c, out_c, len_i) ) {
      fprintf(stderr, "etch_uw: big wiz fail; expected %s, got '%s'\r\n",
                      num_c, out_c);
      ret_i = 0;
    }

    else {
      u3_noun out = u3s_etch_uw(hou);
      u3_noun tou = u3i_bytes(len_i, (c3_y*)out_c);

      if ( c3n == u3r_sing(tou, out) ) {
        fprintf(stderr, "etch_uw: big wiz mismatch; expected %s\r\n", num_c);
        u3m_p("out", out);
        ret_i = 0;
      }

      u3z(out);
      u3z(tou);
    }

    c3_free(out_c);
    u3z(hot);
    u3z(hou);
  }

  return ret_i;
}

#undef _neq_etch_out

static inline c3_i
_da_good(c3_d hi, c3_d lo, const c3_c* dat_c)
{
  u3_weak out;

  out = u3s_sift_da_bytes(strlen(dat_c), (c3_y*)dat_c);

  if ( u3_none == out) {
    fprintf(stderr, "sift_da: %s fail; expected hi: 0x%llx, lo: 0x%llx\r\n",
        dat_c, hi, lo);

    return 0;
  }

  c3_d out_lo = u3r_chub(0, out);

  // Careful, works only on 128-bit dates
  //
  c3_d out_hi = u3r_chub(1, out);

  if ( out_hi != hi || out_lo != lo ) {
    fprintf(stderr, "sift_da: %s fail; expected 0x%llx,0x%llx: actual 0x%llx,0x%llx\r\n",dat_c, hi, lo, out_hi, out_lo);

    u3z(out);

    return 0;
  }

  u3z(out);

  return 1;
}

static inline c3_i
_da_fail(const c3_c* dat_c)
{
  u3_weak out;

  if ( u3_none != (out = u3s_sift_da_bytes(strlen(dat_c), (c3_y*)dat_c)) ) {
    u3m_p("out", out);
    fprintf(stderr, "sift_da: %s expected fail\r\n", dat_c);

    u3z(out);

    return 0;
  }

  u3z(out);

  return 1;
}

static c3_i
_test_sift_da(void)
{
  c3_i ret_i = 1;

  ret_i &= _da_good(0x0, 0x0, "~292277024401-.1.1");
  ret_i &= _da_good(0x7ffffffe58e40f80,
                    0xbabe000000000000,
                    "~1.12.25..00.00.00..babe");
  ret_i &= _da_good(0x7ffffffe93b72f70,
                    0x3300000000000000,
                    "~33.4.3..15.00.00..3300");
  ret_i &= _da_good(0x7ffffffe93b72f70,
                    0x3300000000000000,
                    "~33.4.3..15.00.00..3300");
  ret_i &= _da_good(0x8000000d32bb462f,
                    0xcafe000000000000,
                    "~2023.3.24..05.44.15..cafe");
  ret_i &= _da_good(0x8000000d32bb462f,
                    0xcafe00000000,
                    "~2023.3.24..05.44.15..0000.cafe");
  ret_i &= _da_good(0x8000000d32bb462f,
                    0xcafe0000,
                    "~2023.3.24..05.44.15..0000.0000.cafe");
  ret_i &= _da_good(0x8000000d32bb462f,
                    0xcafe,
                    "~2023.3.24..05.44.15..0000.0000.0000.cafe");
  ret_i &= _da_good(0x8000000d329d6f76,
                    0xadef000000000000,
                    "~2023.3.1..14.32.22..adef");
  ret_i &= _da_good(0x8000000d32c33b88,
                    0x2d00000000000000,
                    "~2023.3.30..06.36.56..2d00");
  ret_i &= _da_good(0x8000000d32c51c00,
                    0x2d00000000000000,
                    "~2023.3.31..16.46.56..2d00");
  ret_i &= _da_good(0x8000000d3a19f0c0,
                    0x2d00000000000000,
                    "~2027.2.22..07.26.56..2d00");
  ret_i &= _da_good(0x80000029dd78fec0,
                    0x2d00000000000000,
                    "~5924.11.10..10.06.56..2d00");
  ret_i &= _da_good(0x8000700808c7aec0,
                    0x2d00000000000000,
                    "~3903639.9.11..12.46.56..2d00");

  ret_i &= _da_fail("~2023--.1.1");
  ret_i &= _da_fail("~2.023.1.1");
  ret_i &= _da_fail("~2023.01.1");
  ret_i &= _da_fail("~2023.1.01");
  ret_i &= _da_fail("~2023.13.1");
  ret_i &= _da_fail("~2023.12.32");
  ret_i &= _da_fail("~2023.2.31");
  ret_i &= _da_fail("~2023.2.29");

  ret_i &= _da_fail("~2023.3.3..24.00.00");
  ret_i &= _da_fail("~2023.3.3..24.00.00..ca");
  ret_i &= _da_fail("~2023.3.3..24.00.00..cAFE");
  ret_i &= _da_fail("~2023.3.3..24.00.00..cAFE");
  ret_i &= _da_fail("~2023.3.3..24.00.00..cafe.cafe.");

  return ret_i;
}

static inline u3_noun
_p_good(c3_d num_d, const c3_c* num_c)
{
  u3_weak out;

  out = u3s_sift_p_bytes(strlen(num_c), (c3_y*)num_c);

  if ( c3y == u3a_is_cat(out) ) {
    if ( num_d != out) {
        fprintf(stderr, "sift_p: %s wrong; expected 0x%llx: actual 0x%x\r\n", num_c, num_d, out);
      return 0;
    }

    return 1;
  }
  else {

    if ( u3_none == out ) {
      fprintf(stderr, "sift_p: %s fail; expected 0x%llx\r\n", num_c, num_d);
      return 0;
    }

    c3_d out_d = u3r_chub(0, out);

    if ( num_d != out_d ) {
        fprintf(stderr, "sift_p: %s wrong; expected 0x%llx: actual 0x%llx\r\n", num_c, num_d, out_d);

        u3z(out);
        return 0;
    }

    u3z(out);
    return 1;
  }

}

static inline c3_i
_p_fail(const c3_c* num_c)
{
  u3_weak out;
  if ( u3_none != (out = u3s_sift_p_bytes(strlen(num_c), (c3_y*)num_c)) ) {
    u3m_p("out", out);
    fprintf(stderr, "sift_p: %s expected fail\r\n", num_c);
    return 0;
  }

  return 1;
}

static c3_i
_test_sift_p(void)
{
  c3_i ret_i = 1;

  ret_i &= _p_good(0x0, "~zod");
  ret_i &= _p_good(0x3, "~wes");
  ret_i &= _p_good(0x10, "~ryp");
  ret_i &= _p_good(0x17, "~dep");
  ret_i &= _p_good(0x1b, "~hec");
  ret_i &= _p_good(0x26, "~sul");
  ret_i &= _p_good(0x29, "~led");
  ret_i &= _p_good(0x2e, "~hex");
  ret_i &= _p_good(0x31, "~dul");
  ret_i &= _p_good(0x3e, "~nep");
  ret_i &= _p_good(0x56, "~mut");
  ret_i &= _p_good(0x66, "~dyl");
  ret_i &= _p_good(0x7c, "~ren");
  ret_i &= _p_good(0x8a, "~fun");
  ret_i &= _p_good(0x92, "~dux");
  ret_i &= _p_good(0xac, "~ber");
  ret_i &= _p_good(0xbf, "~myl");
  ret_i &= _p_good(0xcf, "~wel");
  ret_i &= _p_good(0xd2, "~rel");
  ret_i &= _p_good(0xd4, "~nes");
  ret_i &= _p_good(0xf9, "~tel");
  ret_i &= _p_good(0xff, "~fes");

  ret_i &= _p_good(0x1cc, "~marryd");
  ret_i &= _p_good(0xf18, "~sibdys");
  ret_i &= _p_good(0x134b, "~modsem");
  ret_i &= _p_good(0x18c7, "~dorner");
  ret_i &= _p_good(0x2513, "~dalnup");
  ret_i &= _p_good(0x2570, "~dalsyp");
  ret_i &= _p_good(0x39f6, "~difweg");
  ret_i &= _p_good(0x4a94, "~sicnum");
  ret_i &= _p_good(0x5cfa, "~banrep");
  ret_i &= _p_good(0x63c6, "~wiclen");
  ret_i &= _p_good(0x753b, "~dacwyc");
  ret_i &= _p_good(0x8b45, "~nompet");
  ret_i &= _p_good(0xa03c, "~tacbur");
  ret_i &= _p_good(0xa2b4, "~moglur");
  ret_i &= _p_good(0xad0a, "~pocsyt");
  ret_i &= _p_good(0xb365, "~dibwet");
  ret_i &= _p_good(0xba42, "~lodden");
  ret_i &= _p_good(0xdcaa, "~rislep");
  ret_i &= _p_good(0xeec2, "~bacfur");
  ret_i &= _p_good(0xf674, "~tondut");
  ret_i &= _p_good(0xffff, "~fipfes");

  ret_i &= _p_good(0x6d2030, "~hocmeb-dapsen");
  ret_i &= _p_good(0x19e3826, "~ladlen-nidrev");
  ret_i &= _p_good(0x60e5726, "~ropsyn-magtyl");
  ret_i &= _p_good(0x108deca3, "~divbud-ladbyn");
  ret_i &= _p_good(0x1cb220fb, "~dathut-miplep");
  ret_i &= _p_good(0x2a84b998, "~haplun-savruc");
  ret_i &= _p_good(0x2e380f98, "~darben-firlyx");
  ret_i &= _p_good(0x3e2f64cc, "~hodbep-lavmep");
  ret_i &= _p_good(0x64f4eace, "~mopten-hilfex");
  ret_i &= _p_good(0x7c0fdcda, "~sipsyt-simweg");
  ret_i &= _p_good(0x7d0a9aa1, "~tocseg-fitneb");
  ret_i &= _p_good(0x82622083, "~wicwyt-marsur");
  ret_i &= _p_good(0x9266739d, "~widfen-tadmut");
  ret_i &= _p_good(0x95f01ec8, "~foddus-sabden");
  ret_i &= _p_good(0xa1ae3130, "~tinbyn-fammun");
  ret_i &= _p_good(0xaf7c1801, "~molryn-nisnux");
  ret_i &= _p_good(0xb91f853a, "~dinnex-sonnum");
  ret_i &= _p_good(0xc14c7ccf, "~morwes-pasbyn");
  ret_i &= _p_good(0xca76d018, "~borred-dozrus");
  ret_i &= _p_good(0xf2ea4743, "~bansec-tabnus");
  ret_i &= _p_good(0xffffffff, "~dostec-risfen");

  ret_i &= _p_good(0x6bfc3f1881b, "~sigmyl-bintus-sovpet");
  ret_i &= _p_good(0x37e37b1a3551, "~tadwer-ropfed-binleg");
  ret_i &= _p_good(0x410347ee002e, "~narwes-tidlud-fasmyn");
  ret_i &= _p_good(0x46f6e0458bc7, "~novweg-bilnet-radfep");
  ret_i &= _p_good(0x47c87321d50b, "~sitlex-tocrul-lodsep");
  ret_i &= _p_good(0x51353dce0067, "~podtyl-sicnes-samfet");
  ret_i &= _p_good(0x518ce06c70e1, "~podref-worlex-doclep");
  ret_i &= _p_good(0x5c68ea866ab7, "~banmes-bisryt-ralrul");
  ret_i &= _p_good(0x611273cbe100, "~nordyr-dacpel-libsud");
  ret_i &= _p_good(0x71191aad547c, "~tagput-batteg-dirdyn");
  ret_i &= _p_good(0x73f1ea2b6764, "~saltes-faddyn-norpur");
  ret_i &= _p_good(0x8a44e8857186, "~figsub-fabwed-lasnys");
  ret_i &= _p_good(0x93da8f14e8eb, "~ridler-tastel-roctul");
  ret_i &= _p_good(0xab36928a695b, "~boswyd-lagdut-tobhes");
  ret_i &= _p_good(0xae9859f74a22, "~hacfep-dibled-moddet");
  ret_i &= _p_good(0xb04e0a68a36d, "~havmeg-dirsev-padtem");
  ret_i &= _p_good(0xd2a8b958c1ec, "~niltuc-rolfur-ricref");
  ret_i &= _p_good(0xd682b6a7a9c1, "~famneb-tarnut-rilnes");
  ret_i &= _p_good(0xe1a670e9eebd, "~larpub-bacfus-nisbex");
  ret_i &= _p_good(0xf6b014781344, "~tonbyl-dasryg-bitlen");
  ret_i &= _p_good(0xffffffffffff, "~fipfes-dostec-risfen");

  ret_i &= _p_good(0x94fede64d31f2a0, "~lisnet-rivnys-natdem-donful");
  ret_i &= _p_good(0xf4baddc87e49501, "~sibsem-pocseb-balduc-davbus");
  ret_i &= _p_good(0x354e583df2681571, "~tirmeg-nopder-hinwes-micdur");
  ret_i &= _p_good(0x3d0b51f8a79c9cbb, "~dasdur-podmur-doswed-motlys");
  ret_i &= _p_good(0x3e2dc8e804dda5f7, "~midbyn-hinlyn-dossub-faslyt");
  ret_i &= _p_good(0x4974294fa5c476be, "~tipdut-hannet-talpec-dasted");
  ret_i &= _p_good(0x51caf3d176a3e85c, "~podned-namhus-dirtes-moglud");
  ret_i &= _p_good(0x5f1a0462f14c6a4e, "~siplug-samdec-pinsev-rigwes");
  ret_i &= _p_good(0x648393ba45a204bc, "~socrum-ridpex-hanlyx-nidfyn");
  ret_i &= _p_good(0x681e0b656bf4a5ba, "~picsyd-dirwet-rabdyt-davtul");
  ret_i &= _p_good(0x767340cdb232bbdd, "~tanset-rillyd-rovdet-sondeg");
  ret_i &= _p_good(0xa3a6f3dffaa1b143, "~simpub-namlud-dovnux-fampun");
  ret_i &= _p_good(0xa9ec7fcb9023e486, "~firfed-pallec-tonzod-monbep");
  ret_i &= _p_good(0xb39638ae3f909214, "~dibryg-bichut-witsev-fanpub");
  ret_i &= _p_good(0xc356a0bec7c9f106, "~fasmut-taclev-hocmun-pidnel");
  ret_i &= _p_good(0xd226683f5a2fa433, "~nilsul-picpur-nocsem-tasrys");
  ret_i &= _p_good(0xd5bc5e03458e7790, "~fopbyt-worwes-rolput-nodruc");
  ret_i &= _p_good(0xe203169849fc1124, "~fitwes-hopfep-bitwyd-doswer");
  ret_i &= _p_good(0xfb0b09b610c92278, "~sordur-lisbus-ritsyd-wanpet");
  ret_i &= _p_good(0xfe96342d19d7cf69, "~mipryg-rambyn-livdyr-paglun");
  ret_i &= _p_good(0xffffffffffffffff, "~fipfes-fipfes-dostec-risfen");

  ret_i &= _p_fail("~");
  ret_i &= _p_fail("~doz");
  ret_i &= _p_fail("~dozzod");
  ret_i &= _p_fail("~bin-zod");
  ret_i &= _p_fail("~zod-mipryg-rambyn");
  ret_i &= _p_fail("~doz-mipryg-rambyn");
  ret_i &= _p_fail("dozzod-fipfes-dostec-risfen");
  ret_i &= _p_fail("fipfes-fipfes-dostec-risfen--");
  ret_i &= _p_fail("fipfes-fipfes--dostec-risfen");

  {
    c3_c* hex_c = "0x1.1234.5678.9abc.def0.1234.5678.9abc.def0";
    c3_c* pun_c = "~doznec--doprut-posfel-tilbyt-riblyr--doprut-posfel-tilbyt-riblyr";

    u3_weak out = u3s_sift_p_bytes(strlen(pun_c), (c3_y*)pun_c);
    u3_weak hot = u3s_sift_ux_bytes(strlen(hex_c), (c3_y*)hex_c);


    if ( u3_none == out ) {
      fprintf(stderr, "sift_p: big p fail\r\n");
      ret_i = 0;
    }

    if ( u3_none == hot ) {
      fprintf(stderr, "sift_p: big hex fail during big p test\r\n");
      ret_i = 0;
    }

    else {
      if ( c3n == u3r_sing(hot, out) ) {
        u3m_p("hot", hot);
        u3m_p("out", out);
        fprintf(stderr, "sift_p: big p wrong\r\n");
        ret_i = 0;
      }
    }

    u3z(out);
    u3z(hot);
  }

  return ret_i;
}

static inline c3_i
_ud_good(c3_w num_w, const c3_c* num_c)
{
  u3_weak out;
  if ( num_w != (out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c)) ) {
    if ( u3_none == out ) {
      fprintf(stderr, "sift_ud: %s fail; expected %u\r\n", num_c, num_w);
    }
    else {
      fprintf(stderr, "sift_ud: %s wrong; expected %u: actual %u\r\n", num_c, num_w, out);
    }
    return 0;
  }

  return 1;
}

static inline c3_i
_ud_fail(const c3_c* num_c)
{
  u3_weak out;
  if ( u3_none != (out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c)) ) {
    u3m_p("out", out);
    fprintf(stderr, "sift_ud: %s expected fail\r\n", num_c);
    return 0;
  }

  return 1;
}

static c3_i
_test_sift_ud(void)
{
  c3_i ret_i = 1;

  ret_i &= _ud_good(0, "0");
  ret_i &= _ud_good(1, "1");
  ret_i &= _ud_good(12, "12");
  ret_i &= _ud_good(123, "123");
  ret_i &= _ud_good(1234, "1.234");
  ret_i &= _ud_good(12345, "12.345");
  ret_i &= _ud_good(123456, "123.456");
  ret_i &= _ud_good(1234567, "1.234.567");
  ret_i &= _ud_good(12345678, "12.345.678");
  ret_i &= _ud_good(123456789, "123.456.789");
  ret_i &= _ud_good(100000000, "100.000.000");
  ret_i &= _ud_good(101101101, "101.101.101");
  ret_i &= _ud_good(201201201, "201.201.201");
  ret_i &= _ud_good(302201100, "302.201.100");

  ret_i &= _ud_fail("01");
  ret_i &= _ud_fail("02");
  ret_i &= _ud_fail("003");
  ret_i &= _ud_fail("1234");
  ret_i &= _ud_fail("1234.5");
  ret_i &= _ud_fail("1234.567.8");
  ret_i &= _ud_fail("1234.56..78.");
  ret_i &= _ud_fail("123.45a");
  ret_i &= _ud_fail(".123.456");

  {
    c3_c* num_c = "4.294.967.296";
    u3_weak out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c);
    u3_atom pro = u3qc_bex(32);

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ud: (bex 32) fail\r\n");
      ret_i = 0;
    }

    if ( c3n == u3r_sing(pro, out) ) {
      u3m_p("out", out);
      fprintf(stderr, "sift_ud: (bex 32) wrong\r\n");
      ret_i = 0;
    }

    u3z(out); u3z(pro);
  }


  {
    c3_c* num_c = "340.282.366.920.938.463.463.374.607.431.768.211.456";
    u3_weak out = u3s_sift_ud_bytes(strlen(num_c), (c3_y*)num_c);
    u3_atom pro = u3qc_bex(128);

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ud: (bex 128) fail\r\n");
      ret_i = 0;
    }

    if ( c3n == u3r_sing(pro, out) ) {
      u3m_p("out", out);
      fprintf(stderr, "sift_ud: (bex 128) wrong\r\n");
      ret_i = 0;
    }

    u3z(out); u3z(pro);
  }

  return ret_i;
}

static inline c3_i
_ux_good(c3_d num_d, const c3_c* num_c)
{
  u3_weak out;

  out = u3s_sift_ux_bytes(strlen(num_c), (c3_y*)num_c);

  if ( c3y == u3a_is_cat(out) ) {
    if ( num_d != out ) {
        fprintf(stderr, "sift_ux: %s wrong; expected 0x%llx: actual 0x%x\r\n", num_c, num_d, out);
      return 0;
    }

    return 1;
  }
  else {

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ux: %s fail; expected 0x%llx\r\n", num_c, num_d);
      return 0;
    }

    c3_d out_d = u3r_chub(0, out);

    if ( num_d != out_d ) {
        fprintf(stderr, "sift_ux: %s wrong; expected 0x%llx: actual 0x%llx\r\n", num_c, num_d, out_d);

        u3z(out);
        return 0;
    }

    u3z(out);
    return 1;
  }

}

static inline c3_i
_ux_fail(const c3_c* num_c)
{
  u3_weak out;
  if ( u3_none != (out = u3s_sift_ux_bytes(strlen(num_c), (c3_y*)num_c)) ) {
    u3m_p("out", out);
    fprintf(stderr, "sift_ux: %s expected fail\r\n", num_c);
    return 0;
  }

  return 1;
}

static c3_i
_test_sift_ux(void)
{
  c3_i ret_i = 1;

  ret_i &= _ux_good(0x0, "0x0");
  ret_i &= _ux_good(0x1, "0x1");
  ret_i &= _ux_good(0x12, "0x12");
  ret_i &= _ux_good(0x1a3, "0x1a3");
  ret_i &= _ux_good(0x123b, "0x123b");
  ret_i &= _ux_good(0x1234c, "0x1.234c");
  ret_i &= _ux_good(0x12e3e56, "0x12e.3e56");
  ret_i &= _ux_good(0x1234e67, "0x123.4e67");
  ret_i &= _ux_good(0x1234567f, "0x1234.567f");
  ret_i &= _ux_good(0x123456789, "0x1.2345.6789");
  ret_i &= _ux_good(0x100000000, "0x1.0000.0000");
  ret_i &= _ux_good(0x101101101, "0x1.0110.1101");
  ret_i &= _ux_good(0x201201201, "0x2.0120.1201");
  ret_i &= _ux_good(0x302201100, "0x3.0220.1100");

  ret_i &= _ux_fail("0x");
  ret_i &= _ux_fail("x0");
  ret_i &= _ux_fail("0x01");
  ret_i &= _ux_fail("0x12.345");
  ret_i &= _ux_fail("0x12.3456.789");
  ret_i &= _ux_fail("0x1.2.3456.789");

  {
    c3_c* num_c = "0x1.0000.0000";
    u3_weak out = u3s_sift_ux_bytes(strlen(num_c), (c3_y*)num_c);
    u3_atom pro = u3qc_bex(32);

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ux: (bex 32) fail\r\n");
      ret_i = 0;
    }

    else {
      if ( c3n == u3r_sing(pro, out) ) {
        u3m_p("out", out);
        fprintf(stderr, "sift_ux: (bex 32) wrong\r\n");
        ret_i = 0;
      }
    }

    u3z(out); u3z(pro);
  }

  {
    c3_c* num_c = "0x1.1234.5678.9abc.def0.1234.5678.9abc.def0";
    c3_c* bnum_c = "0x1234.5678.9abc.def0";

    u3_weak out = u3s_sift_ux_bytes(strlen(num_c), (c3_y*)num_c);
    u3_atom bout = u3s_sift_ux_bytes(strlen(bnum_c), (c3_y*)bnum_c);

    u3_atom pro = u3qc_bex(128);
    u3_atom bpro = u3qa_add(pro, bout);
    u3_atom cpro = u3qc_lsh(6,1, bout);
    u3_atom dpro = u3qa_add(bpro, cpro);

    if ( u3_none == out ) {
      fprintf(stderr, "sift_ux: big hex fail\r\n");
      ret_i = 0;
    }

    else {
      if ( c3n == u3r_sing(dpro, out) ) {
        u3m_p("out", out);
        fprintf(stderr, "sift_ux: big hex wrong\r\n");
        ret_i = 0;
      }
    }

    u3z(out); u3z(bout);
    u3z(pro); u3z(bpro);
    u3z(cpro); u3z(dpro);
  }

  return ret_i;
}

static c3_i
_test_en_base16(void)
{
  c3_i ret_i = 1;

  {
    u3_atom dat = 0xaa;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("aa", pro) ) {
      fprintf(stderr, "en_base16: fail (a)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom dat = 0x1234;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("1234", pro) ) {
      fprintf(stderr, "en_base16: fail (b)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom dat = 0xf012;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("f012", pro) ) {
      fprintf(stderr, "en_base16: fail (c)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom dat = 0x10b;
    u3_atom pro = u3qe_en_base16(u3r_met(3, dat), dat);

    if ( c3n == u3r_sing_c("010b", pro) ) {
      fprintf(stderr, "en_base16: fail (d)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom pro = u3qe_en_base16(3, 0x1234);

    if ( c3n == u3r_sing_c("001234", pro) ) {
      fprintf(stderr, "en_base16: fail (e)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_atom pro = u3qe_en_base16(1, 0x1234);

    if ( c3n == u3r_sing_c("34", pro) ) {
      fprintf(stderr, "en_base16: fail (f)\r\n");
      ret_i = 0;
    }

    u3z(pro);
  }

  return ret_i;
}


static c3_i
_test_de_base16(void)
{
  c3_i ret_i = 1;

  {
    u3_noun inp = u3i_string("aa");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (a)\r\n");
      ret_i = 0;
    }

    if ( 1 != len ) {
      fprintf(stderr, "de_base16: fail len (a)\r\n");
      ret_i = 0;
    }

    if ( 0xaa != dat ) {
      fprintf(stderr, "de_base16: fail dat (a)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("1234");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (b)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (b)\r\n");
      ret_i = 0;
    }

    if ( 0x1234 != dat ) {
      fprintf(stderr, "de_base16: fail dat (b)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("f012");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (c)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (c)\r\n");
      ret_i = 0;
    }

    if ( 0xf012 != dat ) {
      fprintf(stderr, "de_base16: fail dat (c)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("010b");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (d)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (d)\r\n");
      ret_i = 0;
    }

    if ( 0x10b != dat ) {
      fprintf(stderr, "de_base16: fail dat (d)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("10b");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (e)\r\n");
      ret_i = 0;
    }

    if ( 2 != len ) {
      fprintf(stderr, "de_base16: fail len (e)\r\n");
      ret_i = 0;
    }

    if ( 0x10b != dat ) {
      fprintf(stderr, "de_base16: fail dat (e)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  {
    u3_noun inp = u3i_string("001234");
    u3_noun pro = u3qe_de_base16(inp);
    u3_atom len, dat;

    if ( c3n == u3r_pq(pro, u3_nul, &len, &dat) ) {
      fprintf(stderr, "de_base16: fail cell (f)\r\n");
      ret_i = 0;
    }

    if ( 3 != len ) {
      fprintf(stderr, "de_base16: fail len (f)\r\n");
      ret_i = 0;
    }

    if ( 0x1234 != dat ) {
      fprintf(stderr, "de_base16: fail dat (f)\r\n");
      ret_i = 0;
    }

    u3z(inp); u3z(pro);
  }

  return ret_i;
}

static c3_i
_test_base16(void)
{
  c3_i ret_i = 1;

  ret_i &= _test_en_base16();
  ret_i &= _test_de_base16();

  return ret_i;
}

static c3_w
_fein_ob_w(c3_w inp_w)
{
  u3_atom inp = u3i_word(inp_w);
  u3_atom act = u3qe_fein_ob(inp);
  c3_w  act_w = u3r_word(0, act);
  u3z(inp); u3z(act);
  return act_w;
}

static c3_i
_expect_fein_ob_w(c3_w inp_w, c3_w exp_w)
{
  c3_w act_w = _fein_ob_w(inp_w);

  if ( act_w != exp_w ) {
    fprintf(stderr, "fein: inp=0x%08x exp=0x%08x act=0x%08x\n",
                    inp_w, exp_w, act_w);
    return 0;
  }

  return 1;
}

static c3_i
_test_fein_ob(void)
{
  c3_i ret_i = 1;

  ret_i &= _expect_fein_ob_w(0, 0);
  ret_i &= _expect_fein_ob_w(0xffff, 0xffff);
  ret_i &= _expect_fein_ob_w(0x1b08f, 0x76b920e5);
  ret_i &= _expect_fein_ob_w(0x10000, 0x423e60bf);
  ret_i &= _expect_fein_ob_w(0x10001, 0xd4400acb);
  ret_i &= _expect_fein_ob_w(0x10002, 0xf429043);
  ret_i &= _expect_fein_ob_w(0x10000000, 0xa04bc7fa);
  ret_i &= _expect_fein_ob_w(0x1234abcd, 0x686f6c25);
  ret_i &= _expect_fein_ob_w(0xabcd1234, 0x4a220c8);
  ret_i &= _expect_fein_ob_w(0xdeadbeef, 0x909bc4a9);
  ret_i &= _expect_fein_ob_w(0xfffff, 0x6746b96b);
  ret_i &= _expect_fein_ob_w(0xffffffff, 0xbba4dcce);

  return ret_i;
}

static c3_w
_fynd_ob_w(c3_w inp_w)
{
  u3_atom inp = u3i_word(inp_w);
  u3_atom act = u3qe_fynd_ob(inp);
  c3_w  act_w = u3r_word(0, act);
  u3z(inp); u3z(act);
  return act_w;
}

static c3_i
_expect_fynd_ob_w(c3_w exp_w, c3_w inp_w)
{
  c3_w act_w = _fynd_ob_w(inp_w);

  if ( act_w != exp_w ) {
    fprintf(stderr, "fynd: inp=0x%08x exp=0x%08x act=0x%08x\n",
                    inp_w, exp_w, act_w);
    return 0;
  }

  return 1;
}

static c3_i
_test_fynd_ob(void)
{
  c3_i ret_i = 1;

  ret_i &= _expect_fynd_ob_w(0, 0);
  ret_i &= _expect_fynd_ob_w(0xffff, 0xffff);
  ret_i &= _expect_fynd_ob_w(0x10000, 0x423e60bf);
  ret_i &= _expect_fynd_ob_w(0x10001, 0xd4400acb);
  ret_i &= _expect_fynd_ob_w(0x10002, 0xf429043);
  ret_i &= _expect_fynd_ob_w(0x10000000, 0xa04bc7fa);
  ret_i &= _expect_fynd_ob_w(0x1234abcd, 0x686f6c25);
  ret_i &= _expect_fynd_ob_w(0xabcd1234, 0x4a220c8);
  ret_i &= _expect_fynd_ob_w(0xdeadbeef, 0x909bc4a9);
  ret_i &= _expect_fynd_ob_w(0xfffff, 0x6746b96b);
  ret_i &= _expect_fynd_ob_w(0xffffffff, 0xbba4dcce);

  return ret_i;
}

static c3_i
_exhaust_roundtrip_fein_fynd_ob(void)
{
  c3_i ret_i = 1;
  c3_w fyn_w, i_w;

  {
    u3_atom fen, fyn;

    for ( i_w = 0x10000; i_w < 0x80000000; i_w++ ) {
      fen   = u3qe_fein_ob(i_w);
      fyn   = u3qe_fynd_ob(fen);
      fyn_w = u3r_word(0, fyn);

      if ( i_w != fyn_w ) {
        fprintf(stderr, "fein/fynd: inp=0x%08x fein=0x%08x fynd=0x%08x\n",
                        i_w, u3r_word(0, fen), fyn_w);
        ret_i = 0;
      }
      u3z(fen); u3z(fyn);

      if ( !(i_w % 0x1000000) ) {
        fprintf(stderr, "fein/fynd: 0x%x done\n", i_w);
      }
    }
  }

  {
    c3_w fen_w;

    do {
      fen_w = _fein_ob_w(i_w);
      fyn_w = _fynd_ob_w(fen_w);
      if ( i_w != fyn_w ) {
        fprintf(stderr, "fein/fynd: inp=0x%08x fein=0x%08x fynd=0x%08x\n",
                        i_w, fen_w, fyn_w);
        ret_i = 0;
      }

      if ( !(i_w % 0x1000000) ) {
        fprintf(stderr, "fein/fynd: 0x%x done\n", i_w);
      }
    }
    while ( ++i_w );
  }

  return ret_i;
}

static c3_i
_test_ob(void)
{
  c3_i ret_i = 1;
  ret_i &= _test_fein_ob();
  ret_i &= _test_fynd_ob();
  //  disabled, takes almost ~m15
  //
  // ret_i &= _exhaust_roundtrip_fein_fynd_ob();
  return ret_i;
}

static c3_i
_test_jets(void)
{
  c3_i ret_i = 1;

  if ( !_test_etch_da() ) {
    fprintf(stderr, "test jets: etch_da: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_p() ) {
    fprintf(stderr, "test jets: etch_p: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_ud() ) {
    fprintf(stderr, "test jets: etch_ud: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_ui() ) {
    fprintf(stderr, "test jets: etch_ui: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_ux() ) {
    fprintf(stderr, "test jets: etch_ux: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_uv() ) {
    fprintf(stderr, "test jets: etch_uv: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_etch_uw() ) {
    fprintf(stderr, "test jets: etch_uw: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sift_da() ) {
    fprintf(stderr, "test jets: sift_da: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sift_p() ) {
    fprintf(stderr, "test jets: sift_ud: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sift_ud() ) {
    fprintf(stderr, "test jets: sift_ud: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_sift_ux() ) {
    fprintf(stderr, "test jets: sift_ux: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_base16() ) {
    fprintf(stderr, "test jets: base16: failed\r\n");
    ret_i = 0;
  }

  if ( !_test_ob() ) {
    fprintf(stderr, "test jets: ob: failed\r\n");
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

  if ( !_test_jets() ) {
    fprintf(stderr, "test jets: failed\r\n");
    exit(1);
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test jets: ok\r\n");
  return 0;
}
