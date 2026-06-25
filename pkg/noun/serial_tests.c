/// @file

#include "noun.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

/* _test_ram_spec(): encode [ref] as ram, decode, and compare to [ref].
**
**   Verifies the wire format's 5-byte header and that the decoded
**   noun equals the input.  For bob atoms, equality is mug+seq only
**   (no materialization).
*/
static c3_i
_test_ram_spec(const c3_c* cap_c, u3_noun ref)
{
  c3_i  ret_i = 1;
  c3_d  len_d = 0;
  c3_y* byt_y = 0;

  u3s_ram_xeno(ref, &len_d, &byt_y);

  //  validate wire header: "RAM\0" + version 0x01
  //
  if (  (len_d < 5)
     || (byt_y[0] != 'R')
     || (byt_y[1] != 'A')
     || (byt_y[2] != 'M')
     || (byt_y[3] != 0x00)
     || (byt_y[4] != 0x01) )
  {
    fprintf(stderr, "\033[31mram header %s fail\033[0m\r\n", cap_c);
    free(byt_y);
    return 0;
  }

  //  round-trip via tap
  //
  u3_weak out = u3s_tap_xeno(len_d, byt_y);
  free(byt_y);

  if ( u3_none == out ) {
    fprintf(stderr, "\033[31mtap %s fail: u3_none\033[0m\r\n", cap_c);
    return 0;
  }
  if ( c3n == u3r_sing(ref, out) ) {
    fprintf(stderr, "\033[31mtap %s fail: mismatch\033[0m\r\n", cap_c);
    u3m_p("ref", ref);
    u3m_p("out", out);
    ret_i = 0;
  }
  u3z(out);
  return ret_i;
}

static c3_i
_test_ram_roundtrip(void)
{
  c3_i ret_i = 1;

  //  atoms (cat + indirect)
  //
  { u3_noun ref = 0;       ret_i &= _test_ram_spec("0",       ref); u3z(ref); }
  { u3_noun ref = 1;       ret_i &= _test_ram_spec("1",       ref); u3z(ref); }
  { u3_noun ref = 42;      ret_i &= _test_ram_spec("42",      ref); u3z(ref); }
  { u3_noun ref = c3__fast; ret_i &= _test_ram_spec("%fast",  ref); u3z(ref); }

  //  wide atom (forces indirect path)
  //
  {
    c3_y inp_y[33] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    u3_noun ref = u3i_bytes(sizeof(inp_y), inp_y);
    ret_i &= _test_ram_spec("wide", ref);
    u3z(ref);
  }

  //  cells
  //
  { u3_noun ref = u3nc(0, 0);    ret_i &= _test_ram_spec("[0 0]", ref); u3z(ref); }
  { u3_noun ref = u3nc(1, 2);    ret_i &= _test_ram_spec("[1 2]", ref); u3z(ref); }
  { u3_noun ref = u3nt(1, 2, 3); ret_i &= _test_ram_spec("[1 2 3]", ref); u3z(ref); }
  {
    u3_noun ref = u3nc(u3nc(1, 2), 3);
    ret_i &= _test_ram_spec("[[1 2] 3]", ref);
    u3z(ref);
  }

  //  deep nesting
  //
  {
    u3_noun ref = u3nc(
      u3nc(u3nc(u3nc(1, 2), 3), 4),
      u3nc(5, u3nc(6, 7)));
    ret_i &= _test_ram_spec("deep", ref);
    u3z(ref);
  }

  //  backref: repeated cell — second occurrence should be encoded as backref
  //
  {
    u3_noun sub = u3nt(c3__fast, c3__full, c3__fast);
    u3_noun ref = u3nc(u3k(sub), u3nc(u3k(sub), sub));
    ret_i &= _test_ram_spec("backref-cell", ref);
    u3z(ref);
  }

  //  backref: repeated indirect atom
  //
  {
    u3_noun a = u3i_string("abcdefghijklmnopqrstuvwxyz");
    u3_noun ref = u3nc(u3k(a), a);
    ret_i &= _test_ram_spec("backref-atom", ref);
    u3z(ref);
  }

  return ret_i;
}

/* _ram_tmp_dir / _ram_setup_tmp() / _ram_cleanup_tmp() / _ram_make_blob():
**
**   Helpers for bob-atom round-trip tests.  The ram encoder calls
**   u3r_blob_met() which reads the blob file at
**   $u3C.dir_c/.urb/bob/<mug>/<seq>, so actual files must exist.
*/

static c3_c _ram_tmp_dir[1024];

static c3_o
_ram_setup_tmp(void)
{
  snprintf(_ram_tmp_dir, sizeof(_ram_tmp_dir), "/tmp/vere-serial-test-XXXXXX");
  if ( !mkdtemp(_ram_tmp_dir) ) {
    fprintf(stderr, "serial_tests: mkdtemp failed\r\n");
    return c3n;
  }
  u3C.dir_c = _ram_tmp_dir;

  c3_c pax_c[2048];
  snprintf(pax_c, sizeof(pax_c), "%s/.urb", _ram_tmp_dir);
  mkdir(pax_c, 0755);
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob", _ram_tmp_dir);
  mkdir(pax_c, 0755);
  return c3y;
}

static void
_ram_cleanup_tmp(void)
{
  c3_c cmd_c[2048];
  snprintf(cmd_c, sizeof(cmd_c), "rm -rf %s", _ram_tmp_dir);
  (void)system(cmd_c);
}

static c3_o
_ram_make_blob(c3_h mug_h, c3_h seq_h, const c3_y* dat_y, c3_d len_d)
{
  c3_c pax_c[2048];
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h, _ram_tmp_dir, mug_h);
  mkdir(pax_c, 0755);
  snprintf(pax_c, sizeof(pax_c), "%s/.urb/bob/%" PRIc3_h "/%" PRIc3_h,
           _ram_tmp_dir, mug_h, seq_h);
  FILE* fil_f = fopen(pax_c, "wb");
  if ( !fil_f ) {
    fprintf(stderr, "serial_tests: fopen %s: %s\r\n", pax_c, strerror(errno));
    return c3n;
  }
  if ( len_d && (len_d != fwrite(dat_y, 1, (size_t)len_d, fil_f)) ) {
    fclose(fil_f);
    return c3n;
  }
  fclose(fil_f);
  return c3y;
}

/* _test_ram_bob_spec(): round-trip a bob-containing noun via ram/tap.
**
**   Cannot use u3r_sing for bob-containing refs unless the blob file
**   exists and is decodable — and u3r_sing materializes bob vs normal.
**   Since our reference IS the bob atom, u3r_sing_atom's bob-vs-bob
**   fast path handles it by mug+seq.
*/
static c3_i
_test_ram_bob_spec(const c3_c* cap_c, u3_noun ref)
{
  c3_i  ret_i = 1;
  c3_d  len_d = 0;
  c3_y* byt_y = 0;

  u3s_ram_xeno(ref, &len_d, &byt_y);
  u3_weak out = u3s_tap_xeno(len_d, byt_y);
  free(byt_y);

  if ( u3_none == out ) {
    fprintf(stderr, "\033[31mram/tap bob %s fail: u3_none\033[0m\r\n", cap_c);
    return 0;
  }
  if ( c3n == u3r_sing(ref, out) ) {
    fprintf(stderr, "\033[31mram/tap bob %s fail: mismatch\033[0m\r\n", cap_c);
    ret_i = 0;
  }
  u3z(out);
  return ret_i;
}

static c3_i
_test_ram_bob_roundtrip(void)
{
  c3_i ret_i = 1;

  if ( c3n == _ram_setup_tmp() ) {
    return 0;
  }

  //  create two distinct blob files for testing
  //
  const c3_y dat1_y[] = "blob contents one";
  const c3_y dat2_y[] = "a different blob payload";
  if (  (c3n == _ram_make_blob(0x12345678, 1, dat1_y, sizeof(dat1_y)))
     || (c3n == _ram_make_blob(0x12345678, 2, dat2_y, sizeof(dat2_y)))
     || (c3n == _ram_make_blob(0x7a0b0000, 7, dat1_y, sizeof(dat1_y))) )
  {
    _ram_cleanup_tmp();
    return 0;
  }

  //  single bob atom
  //
  {
    u3_noun ref = u3i_blob(0x12345678, 1);

    c3_d  len_d;
    c3_y* byt_y;
    u3s_ram_xeno(ref, &len_d, &byt_y);
    u3_weak out = u3s_tap_xeno(len_d, byt_y);
    free(byt_y);

    if ( u3_none == out ) {
      fprintf(stderr, "\033[31mram bob solo fail: u3_none\033[0m\r\n");
      ret_i = 0;
    }
    else if ( c3n == u3a_is_bob(out) ) {
      fprintf(stderr, "\033[31mram bob solo fail: decoded as non-bob\033[0m\r\n");
      ret_i = 0;
    }
    else if (  (u3a_bob_mug(out) != 0x12345678)
            || (u3a_bob_seq(out) != 1) )
    {
      fprintf(stderr, "\033[31mram bob solo fail: mug/seq mismatch "
                      "(got %" PRIc3_h "/%" PRIc3_h ")\033[0m\r\n",
              u3a_bob_mug(out), u3a_bob_seq(out));
      ret_i = 0;
    }
    if ( u3_none != out ) u3z(out);
    u3z(ref);
  }

  //  cell containing bob atom
  //
  {
    u3_noun ref = u3nt(42, u3i_blob(0x12345678, 1), 99);
    ret_i &= _test_ram_bob_spec("cell-with-bob", ref);
    u3z(ref);
  }

  //  repeated bob: should trigger backref path after first occurrence
  //
  {
    u3_noun bob = u3i_blob(0x12345678, 2);
    u3_noun ref = u3nc(u3k(bob), u3nc(u3k(bob), bob));
    ret_i &= _test_ram_bob_spec("bob-repeat", ref);
    u3z(ref);
  }

  //  mixed: two distinct bobs + normal atoms + nesting
  //
  {
    u3_noun ref = u3nq(
      u3i_blob(0x12345678, 1),
      u3nc(c3__fast, u3i_blob(0x7a0b0000, 7)),
      u3i_blob(0x12345678, 2),
      0x12345678);
    ret_i &= _test_ram_bob_spec("mixed", ref);
    u3z(ref);
  }

  _ram_cleanup_tmp();
  return ret_i;
}

/* _test_ram_invalid(): u3s_tap_xeno rejects malformed input.
*/
static c3_i
_test_ram_invalid(void)
{
  c3_i ret_i = 1;

  //  too short (< 5 byte header)
  //
  {
    c3_y byt_y[3] = { 'R', 'A', 'M' };
    if ( u3_none != u3s_tap_xeno(sizeof(byt_y), byt_y) ) {
      fprintf(stderr, "\033[31mram invalid short fail\033[0m\r\n");
      ret_i = 0;
    }
  }

  //  bad magic
  //
  {
    c3_y byt_y[6] = { 'J', 'A', 'M', 0x00, 0x01, 0x00 };
    if ( u3_none != u3s_tap_xeno(sizeof(byt_y), byt_y) ) {
      fprintf(stderr, "\033[31mram invalid magic fail\033[0m\r\n");
      ret_i = 0;
    }
  }

  //  unsupported version (0x02)
  //
  {
    c3_y byt_y[6] = { 'R', 'A', 'M', 0x00, 0x02, 0x00 };
    if ( u3_none != u3s_tap_xeno(sizeof(byt_y), byt_y) ) {
      fprintf(stderr, "\033[31mram invalid version fail\033[0m\r\n");
      ret_i = 0;
    }
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

  u3m_grab();
  fprintf(stderr, "test jam: ok\r\n");

  if ( !_test_ram_roundtrip() ) {
    fprintf(stderr, "test ram: failed\r\n");
    exit(1);
  }

  u3m_grab();
  fprintf(stderr, "test ram: ok\r\n");

  if ( !_test_ram_bob_roundtrip() ) {
    fprintf(stderr, "test ram bob: failed\r\n");
    exit(1);
  }

  u3m_grab();
  fprintf(stderr, "test ram bob: ok\r\n");

  if ( !_test_ram_invalid() ) {
    fprintf(stderr, "test ram invalid: failed\r\n");
    exit(1);
  }

  u3m_grab();
  fprintf(stderr, "test ram invalid: ok\r\n");

  return 0;
}
