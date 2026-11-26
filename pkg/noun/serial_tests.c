/// @file

#include "noun.h"
#include "crib.h"
#include "events.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_boot_lite(1 << 24);
}

static void
_byte_print(c3_d        out_d,
            c3_y*       out_y,
            c3_w        len_w,
            const c3_y* byt_y)
{
  c3_d i_d;

  fprintf(stderr, "  actual: { ");
  for ( i_d = 0; i_d < out_d; i_d++ ) {
    fprintf(stderr, "0x%x, ", out_y[i_d]);
  }
  fprintf(stderr, "}\r\n");
  fprintf(stderr, "  expect: { ");
  for ( i_d = 0; i_d < len_w; i_d++ ) {
    fprintf(stderr, "0x%x, ", byt_y[i_d]);
  }
  fprintf(stderr, "}\r\n");
}

/* _test_share(): [a] with its equal subnouns at one address. RETAIN.
*/
static u3_noun
_test_share(u3p(u3h_root) har_p, u3_noun a)
{
  u3_weak pro = u3h_git(har_p, a);

  if ( u3_none != pro ) {
    return u3k(pro);
  }

  pro = ( c3y == u3a_is_atom(a) )
      ? u3k(a)
      : u3nc(_test_share(har_p, u3h(a)), _test_share(har_p, u3t(a)));

  u3h_put(har_p, a, u3k(pro));
  return pro;
}

static c3_i
_test_jam_spec(const c3_c* cap_c,
               u3_noun       ref,
               c3_w        len_w,
               const c3_y* byt_y)
{
  c3_i  ret_i = 1;
  c3_d  out_d;
  c3_y* out_y;

  {
    u3s_jam_xeno(ref, &out_d, &out_y);

    if ( 0 != memcmp(out_y, byt_y, len_w) ) {
      fprintf(stderr, "\033[31mjam xeno %s fail\033[0m\r\n", cap_c);
      _byte_print(out_d, out_y, len_w, byt_y);
      ret_i = 0;
    }

    free(out_y);
  }

  //  u3s_ram_xeno() tells nouns apart by pointer: the noun as built
  //  cues back, and with its equal parts shared, it's +jam's bytes.
  //  (first: _test_share() unifies the noun's equal parts)
  //
  {
    u3_weak out;

    u3s_ram_xeno(ref, &out_d, &out_y);
    out = u3s_cue_xeno(out_d, out_y);

    if ( (u3_none == out) || (c3n == u3r_sing(ref, out)) ) {
      fprintf(stderr, "\033[31mram xeno %s: cue fail\033[0m\r\n", cap_c);
      ret_i = 0;
    }

    u3z(out);
    free(out_y);
  }

  {
    u3p(u3h_root) har_p = u3h_new();
    u3_noun         sar = _test_share(har_p, ref);

    u3h_free(har_p);
    u3s_ram_xeno(sar, &out_d, &out_y);

    if ( (out_d != len_w) || (0 != memcmp(out_y, byt_y, len_w)) ) {
      fprintf(stderr, "\033[31mram xeno %s fail\033[0m\r\n", cap_c);
      _byte_print(out_d, out_y, len_w, byt_y);
      ret_i = 0;
    }

    free(out_y);
    u3z(sar);
  }

  {
    u3i_slab sab_u;
    c3_w     bit_w = u3s_jam_fib(&sab_u, ref);

    out_d = ((c3_d)bit_w + 0x7) >> 3;
    //  XX assumes little-endian
    //
    out_y = sab_u.buf_y;

    if ( 0 != memcmp(out_y, byt_y, len_w) ) {
      fprintf(stderr, "\033[31mjam fib %s fail\033[0m\r\n", cap_c);
      _byte_print(out_d, out_y, len_w, byt_y);
      ret_i = 0;
    }

    u3i_slab_free(&sab_u);
  }

  return ret_i;
}

static c3_i
_test_cue_spec(const c3_c* cap_c,
               u3_noun       ref,
               c3_w        len_w,
               const c3_y* byt_y)
{
  c3_i ret_i = 1;

  {
    u3_noun pro = u3m_soft(0, u3s_cue_atom, u3i_bytes(len_w, byt_y));
    u3_noun tag, out;

    u3x_cell(pro, &tag, &out);

    if ( u3_blip != tag ) {
      fprintf(stderr, "\033[31mcue %s fail 1\033[0m\r\n", cap_c);
      ret_i = 0;
    }
    else if ( c3n == u3r_sing(ref, out) ) {
      fprintf(stderr, "\033[31mcue %s fail 2\033[0m\r\n", cap_c);
      u3m_p("ref", ref);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(pro);
  }

  {
    u3_noun out;

    if ( u3_none == (out = u3s_cue_xeno(len_w, byt_y)) ) {
      fprintf(stderr, "\033[31mcue %s fail 3\033[0m\r\n", cap_c);
      ret_i = 0;
    }
    else if ( c3n == u3r_sing(ref, out) ) {
      fprintf(stderr, "\033[31mcue %s fail 4\033[0m\r\n", cap_c);
      u3m_p("ref", ref);
      u3m_p("out", out);
      ret_i = 0;
    }

    u3z(out);
  }

  return ret_i;
}

static c3_i
_test_jam_roundtrip(void)
{
  c3_i ret_i = 1;

#     define TEST_CASE(a, b)                                        \
        const c3_c* cap_c = a;                                      \
        u3_noun       ref = b;                                      \
        ret_i &= _test_jam_spec(cap_c, ref, sizeof(res_y), res_y);  \
        ret_i &= _test_cue_spec(cap_c, ref, sizeof(res_y), res_y);  \
        u3z(ref);

//  {
//    c3_y res_y[1] = { 0x2 };
//    TEST_CASE("0", 0);
//  }
//
//  {
//    c3_y res_y[1] = { 0xc };
//    TEST_CASE("1", 1);
//  }
//
//  {
//    c3_y res_y[1] = { 0x48 };
//    TEST_CASE("2", 2);
//  }
//
//  {
//    c3_y res_y[6] = { 0xc0, 0x37, 0xb, 0x9b, 0xa3, 0x3 };
//    TEST_CASE("%fast", c3__fast);
//  }
//
//  {
//    c3_y res_y[6] = { 0xc0, 0x37, 0xab, 0x63, 0x63, 0x3 };
//    TEST_CASE("%full", c3__full);
//  }
//
//  {
//    c3_y res_y[1] = { 0x29 };
//    TEST_CASE("[0 0]", u3nc(0, 0));
//  }
//
//  {
//    c3_y res_y[2] = { 0x31, 0x3 };
//    TEST_CASE("[1 1]", u3nc(1, 1));
//  }
//
//  {
//    c3_y res_y[2] = { 0x31, 0x12 };
//    TEST_CASE("[1 2]", u3nc(1, 2));
//  }
//
//  {
//    c3_y res_y[2] = { 0x21, 0xd1 };
//    TEST_CASE("[2 3]", u3nc(2, 3));
//  }
//
//  {
//    c3_y res_y[11] = { 0x1, 0xdf, 0x2c, 0x6c, 0x8e, 0xe, 0x7c, 0xb3, 0x3a, 0x36, 0x36 };
//    TEST_CASE("[%fast %full]", u3nc(c3__fast, c3__full));
//  }
//
//  {
//    c3_y res_y[2] = { 0x71, 0xcc };
//    TEST_CASE("[1 1 1]", u3nc(1, u3nc(1, 1)));
//  }
//
//  {
//    c3_y res_y[3] = { 0x71, 0x48, 0x34 };
//    TEST_CASE("[1 2 3]", u3nt(1, 2, 3));
//  }

  {
    c3_y res_y[12] = { 0x1, 0xdf, 0x2c, 0x6c, 0x8e, 0x1e, 0xf0, 0xcd, 0xea, 0xd8, 0xd8, 0x93 };
    TEST_CASE("[%fast %full %fast]", u3nc(c3__fast, u3nc(c3__full, c3__fast)));
  }

  {
    c3_y res_y[3] = { 0xc5, 0x48, 0x34 };
    TEST_CASE("[[1 2] 3]", u3nc(u3nc(1, 2), 3));
  }

  {
    c3_y res_y[5] = { 0xc5, 0xc8, 0x26, 0x27, 0x1 };
    TEST_CASE("[[1 2] [1 2] 1 2]", u3nt(u3nc(1, 2), u3nc(1, 2), u3nc(1, 2)));
  }

  {
    c3_y res_y[6] = { 0xa5, 0x35, 0x19, 0xf3, 0x18, 0x5 };
    TEST_CASE("[[0 0] [[0 0] 1 1] 1 1]", u3nc(u3nc(0, 0), u3nc(u3nc(u3nc(0, 0), u3nc(1, 1)), u3nc(1, 1))));
  }

  {
    c3_y res_y[14] = { 0x15, 0x17, 0xb2, 0xd0, 0x85, 0x59, 0xb8, 0x61, 0x87, 0x5f, 0x10, 0x54, 0x55, 0x5 };
    TEST_CASE("deep", u3nc(u3nc(u3nc(1, u3nc(u3nc(2, u3nc(u3nc(3, u3nc(u3nc(4, u3nc(u3nt(5, 6, u3nc(7, u3nc(u3nc(8, 0), 0))), 0)), 0)), 0)), 0)), 0), 0));
  }

  {
    c3_y inp_y[33] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    c3_y res_y[35] = { 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8 };
    TEST_CASE("wide", u3i_bytes(sizeof(inp_y), inp_y));
  }

  {
    c3_y inp_y[16] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xc, 0xa8, 0xab, 0x60, 0xef, 0x2d, 0xd, 0x0, 0x0, 0x80 };
    c3_y res_y[19] = { 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x50, 0x57, 0xc1, 0xde, 0x5b, 0x1a, 0x0, 0x0, 0x0, 0x1 };
    TEST_CASE("date", u3i_bytes(sizeof(inp_y), inp_y));
  }

  {
    u3_noun a = u3i_string("abcdefjhijklmnopqrstuvwxyz");
    c3_y res_y[32] = {
       0x1, 0xf8,  0xc, 0x13, 0x1b, 0x23, 0x2b, 0x33, 0x53, 0x43, 0x4b,
      0x53, 0x5b, 0x63, 0x6b, 0x73, 0x7b, 0x83, 0x8b, 0x93, 0x9b, 0xa3,
      0xab, 0xb3, 0xbb, 0xc3, 0xcb, 0xd3, 0x87,  0xc, 0x3d,  0x9
    };
    TEST_CASE("alpha", u3nq(u3k(a), 2, 3, a));
  }

  return ret_i;
}

/* _test_jam_deep(): a noun [n_w] cells deep, by its heads. its values
**   are distinct, and the walk keeps a tail for each. freeing it recurses
**   once a cell (u3a_lose()), and windows gives the tests a small stack:
**   keep [n_w] to a few dozen.
*/
static u3_noun
_test_jam_deep(c3_w n_w)
{
  u3_noun a = 0;
  c3_w  i_w;

  for ( i_w = 0; i_w < n_w; i_w++ ) {
    a = u3nc(a, i_w);
  }
  return a;
}

/* _test_jam_list(): a list of [n_w] distinct atoms: deep by its tails,
**   which nothing recurses on.
*/
static u3_noun
_test_jam_list(c3_w n_w)
{
  u3_noun a = u3_nul;

  while ( n_w-- ) {
    a = u3nc(n_w, a);
  }
  return a;
}

/* _test_jam_dag(): [x_n x_n] for x_0 = [1 2], [n_w] deep: 2^n_w leaves.
*/
static u3_noun
_test_jam_dag(c3_w n_w)
{
  u3_noun x = u3nc(1, 2);

  while ( n_w-- ) {
    x = u3nc(u3k(x), x);
  }
  return x;
}

/* _test_jam_same(): u3s_jam_xeno() of [a] gives +jam's bytes, and leaves
**   the yards as they were. +jam is computed after, since its own table
**   unifies what it compares.
*/
static c3_i
_test_jam_same(const c3_c* cap_c, u3_noun a)
{
  c3_d     out_d;
  c3_y*    out_y;
  c3_i     ret_i = 1;
  u3i_slab sab_u;
  c3_w     bit_w;

  u3s_jam_xeno(a, &out_d, &out_y);
  bit_w = u3s_jam_fib(&sab_u, a);

  if ( (out_d != (((c3_d)bit_w + 7) >> 3)) || memcmp(out_y, sab_u.buf_y, out_d) ) {
    fprintf(stderr, "\033[31mjam xeno %s: %" PRIu64 " bytes against %"
                    PRIu64 "\033[0m\r\n",
            cap_c, out_d, ((c3_d)bit_w + 7) >> 3);
    ret_i = 0;
  }
  if ( u3a_Yard_res.use_i || u3a_Yard_wok.use_i ) {
    fprintf(stderr, "\033[31mjam xeno %s: yards in use\033[0m\r\n", cap_c);
    ret_i = 0;
  }

  free(out_y);
  u3i_slab_free(&sab_u);
  return ret_i;
}

/* _test_jam_apart(): [x y w v] for x and y equal, and built apart, with
**   cell children; w equal to their heads, and v to their tails, also
**   apart. comparing them unifies, and frees what was unified away.
*/
static u3_noun
_test_jam_apart(c3_t swp_t)
{
  u3_noun x = u3nc(u3nc(1, u3i_string("abcdefghijklmnopqrstuvwxyz")),
                   u3nc(3, 4));
  u3_noun y = u3nc(u3nc(1, u3i_string("abcdefghijklmnopqrstuvwxyz")),
                   u3nc(3, 4));
  u3_noun w = u3nc(1, u3i_string("abcdefghijklmnopqrstuvwxyz"));
  u3_noun v = u3nc(3, 4);

  return ( swp_t ) ? u3nq(y, x, w, v) : u3nq(x, y, w, v);
}

//  _test_jam_junior(): u3s_jam_xeno() of nouns built apart on an inner
//  road: there they're the caller's own, junior to the jam's road
//
static u3_noun
_test_jam_junior(u3_noun arg)
{
  c3_i ret_i = 1;

  {
    u3_noun a = _test_jam_apart(0);
    ret_i &= _test_jam_same("junior apart", a);
    u3z(a);
  }
  {
    u3_noun a = _test_jam_apart(1);
    ret_i &= _test_jam_same("junior apart, swapped", a);
    u3z(a);
  }

  u3z(arg);
  return ret_i ? c3y : c3n;
}

/* _test_jam_road(): u3s_jam_xeno() compares on a road of its own: equal
**   nouns built apart are unified there, their old values lost only when
**   the road exits, and every jam gives +jam's bytes.
*/
static c3_i
_test_jam_road(void)
{
  c3_i ret_i = 1;

  //  whichever copy comes first, the table's keys stay live: the losses
  //  wait on the home lose list
  //
  {
    c3_t swp_t;

    for ( swp_t = 0; swp_t < 2; swp_t++ ) {
      u3_noun a     = _test_jam_apart(swp_t);
      c3_d    put_d = u3e_Meter.wlp_d[0];

      ret_i &= _test_jam_same(swp_t ? "apart, swapped" : "apart", a);

      if ( put_d == u3e_Meter.wlp_d[0] ) {
        fprintf(stderr, "\033[31mjam xeno apart: no loss deferred\033[0m\r\n");
        ret_i = 0;
      }
      u3z(a);
    }
  }

  //  two DAGs of 2^64 leaves, built apart: unification from the jam's
  //  road keeps the comparison linear, or it would never finish
  //
  {
    u3_noun a = u3nc(_test_jam_dag(64), _test_jam_dag(64));
    ret_i &= _test_jam_same("two dags", a);
    u3z(a);
  }

  //  a table that grows, and a deep stack in the scratch yard
  //
  {
    u3_noun a = u3nt(_test_jam_list(5000), _test_jam_deep(64),
                     _test_jam_dag(48));
    ret_i &= _test_jam_same("large", a);
    u3z(a);
  }

  //  from an inner road, on nouns of that road
  //
  {
    u3_noun pro = u3m_soft(0, _test_jam_junior, u3_nul);

    if ( (0 != u3h(pro)) || (c3y != u3t(pro)) ) {
      fprintf(stderr, "\033[31mjam xeno: from an inner road\033[0m\r\n");
      ret_i = 0;
    }
    u3z(pro);
  }

  return ret_i;
}

/* _test_ram_same(): u3s_ram_xeno() of [a] gives [ref_d] bytes at
**   [ref_y].
*/
static c3_i
_test_ram_same(const c3_c* cap_c, u3_noun a, c3_d ref_d, const c3_y* ref_y)
{
  c3_d  out_d;
  c3_y* out_y;
  c3_i  ret_i = 1;

  u3s_ram_xeno(a, &out_d, &out_y);

  if ( (out_d != ref_d) || memcmp(out_y, ref_y, out_d) ) {
    fprintf(stderr, "\033[31mram xeno %s: %" PRIu64 " bytes against %"
                    PRIu64 "\033[0m\r\n", cap_c, out_d, ref_d);
    ret_i = 0;
  }
  if ( u3a_Yard_res.use_i || u3a_Yard_wok.use_i ) {
    fprintf(stderr, "\033[31mram xeno %s: yards in use\033[0m\r\n", cap_c);
    ret_i = 0;
  }

  free(out_y);
  return ret_i;
}

//  _test_ram_road(): u3s_ram_xeno() on an inner road
//
static c3_d  _test_ram_d;
static c3_y* _test_ram_y;

static u3_noun
_test_ram_road(u3_noun a)
{
  u3s_ram_xeno(a, &_test_ram_d, &_test_ram_y);
  u3z(a);
  return u3_nul;
}

/* _test_ram_roads(): u3s_ram_xeno() gives the same bytes on the home
**   road, on an inner one, and inside a user of the yards, whose ends
**   it leaves where they were.
*/
static c3_i
_test_ram_roads(void)
{
  c3_i  ret_i = 1;
  c3_d  ref_d;
  c3_y* ref_y;

  //  a table that grows several times; [dag] is 2^48 cells as a tree
  //
  {
    u3_noun a = u3nc(_test_jam_list(2000), _test_jam_dag(48));

    u3s_ram_xeno(a, &ref_d, &ref_y);

    if ( ref_d > (16 << 10) ) {
      fprintf(stderr, "test ram: %" PRIu64 " bytes\r\n", ref_d);
      ret_i = 0;
    }
    ret_i &= _test_ram_same("again", a, ref_d, ref_y);

    //  on an inner road: its stack in the scratch yard there too
    //
    {
      u3_noun pro = u3m_soft(0, _test_ram_road, u3k(a));

      if (  (u3_blip != u3h(pro))
         || (_test_ram_d != ref_d)
         || memcmp(_test_ram_y, ref_y, ref_d) )
      {
        fprintf(stderr, "\033[31mram xeno inner road: %" PRIu64
                        " bytes against %" PRIu64 "\033[0m\r\n",
                _test_ram_d, ref_d);
        ret_i = 0;
      }
      if ( u3a_Yard_res.use_i || u3a_Yard_wok.use_i ) {
        fprintf(stderr, "\033[31mram xeno inner road: yards in use\033[0m\r\n");
        ret_i = 0;
      }

      u3z(pro);
      free(_test_ram_y);
    }

    free(ref_y);
    u3z(a);
  }

  //  inside an enclosing user that left both ends unaligned: the same
  //  bytes from both jams (the by-value one leaps from here), the ends
  //  back where they were, and the user's bytes as they were
  //
  {
    c3_d     out_d, jam_d;
    c3_y*    out_y;
    c3_y*    jam_y;
    c3_y*    wok_y;
    c3_y*    res_y;
    u3i_slab sab_u;
    c3_w     bit_w;
    u3_noun  a = u3nc(_test_jam_deep(64), _test_jam_list(1000));

    u3s_ram_xeno(a, &ref_d, &ref_y);
    wok_y = u3a_yard_take(&u3a_Yard_wok, 1, 1);
    res_y = u3a_yard_take(&u3a_Yard_res, 3, 1);
    *wok_y = 0xa5;
    memset(res_y, 0x5a, 3);

    u3s_ram_xeno(a, &out_d, &out_y);
    u3s_jam_xeno(a, &jam_d, &jam_y);
    bit_w = u3s_jam_fib(&sab_u, a);

    if (  (out_d != ref_d) || memcmp(out_y, ref_y, out_d)
       || (jam_d != (((c3_d)bit_w + 7) >> 3))
       || memcmp(jam_y, sab_u.buf_y, jam_d)
       || (1 != u3a_Yard_wok.use_i) || (3 != u3a_Yard_res.use_i)
       || (0xa5 != *wok_y) || (0x5a != res_y[0]) || (0x5a != res_y[2]) )
    {
      fprintf(stderr, "\033[31mxeno nested: ram %" PRIu64 " bytes against %"
                      PRIu64 ", jam %" PRIu64 ", ends at %zu and %zu\033[0m\r\n",
              out_d, ref_d, jam_d, u3a_Yard_wok.use_i, u3a_Yard_res.use_i);
      ret_i = 0;
    }

    u3a_yard_back(&u3a_Yard_wok, 0);
    u3a_yard_back(&u3a_Yard_res, 0);
    u3i_slab_free(&sab_u);
    free(out_y);
    free(jam_y);
    free(ref_y);
    u3z(a);
  }

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  if ( !_test_jam_roundtrip() ) {
    fprintf(stderr, "test jam: failed\r\n");
    exit(1);
  }

  if ( !_test_jam_road() ) {
    fprintf(stderr, "test jam: road failed\r\n");
    exit(1);
  }

  if ( !_test_ram_roads() ) {
    fprintf(stderr, "test ram: yards failed\r\n");
    exit(1);
  }

  //  GC, failing on a leak
  //
  u3C.wag_h |= u3o_leak_crash;
  u3m_grab();

  fprintf(stderr, "test jam: ok\r\n");
  return 0;
}
