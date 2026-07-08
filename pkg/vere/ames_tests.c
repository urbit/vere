/// @file

#include "./io/ames.c"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 22);
  u3m_pave(c3y);
}

/* _test_ames(): spot check ames helpers
*/
static void
_test_ames(void)
{
  u3_lane lan_u;
  lan_u.pip_w = 0x7f000001;
  lan_u.por_s = 12345;

  u3_noun lan = u3_ames_encode_lane(lan_u);
  u3_lane nal_u = u3_ames_decode_lane(u3k(lan));
  u3_lane nal_u2 = u3_ames_decode_lane(lan);

  if ( !(lan_u.pip_w == nal_u.pip_w && lan_u.por_s == nal_u.por_s) ) {
    fprintf(stderr, "ames: lane fail (a)\r\n");
    fprintf(stderr, "pip: %d, por: %d\r\n", nal_u.pip_w, nal_u.por_s);
    exit(1);
  }
}

static c3_i
_test_stun_addr_roundtrip(u3_lane* inn_u)
{
  c3_c    res_c[16] = {0};
  c3_y    rep_y[40];
  c3_y    req_y[20] = {0};
  c3_i    ret_i     = 0;

  u3_stun_make_response(req_y, inn_u, rep_y);

  u3_lane lan_u;

  if ( c3n == u3_stun_find_xor_mapped_address(rep_y, sizeof(rep_y), &lan_u) ) {
    fprintf(stderr, "stun: failed to find addr in response\r\n");
    ret_i = 1;
  }
  else {
    if ( lan_u.pip_w != inn_u->pip_w ) {
      fprintf(stderr, "stun: addr mismatch %x %x\r\n", lan_u.pip_w, inn_u->pip_w);
      ret_i = 1;
    }

    if ( lan_u.por_s != inn_u->por_s ) {
      fprintf(stderr, "stun: addr mismatch %u %u\r\n", lan_u.por_s, inn_u->por_s);
      ret_i = 1;
    }
  }

  return ret_i;
}

static c3_i
_test_stun(void)
{
  u3_lane inn_u = { .pip_w = 0x7f000001, .por_s = 13337 };
  c3_w    len_w = 256;

  while ( len_w-- ) {
    if ( _test_stun_addr_roundtrip(&inn_u) ) {
      return 1;
    }

    inn_u.pip_w++;
    inn_u.por_s++;
  }

  return 0;
}

/* _test_mug_underflow(): C1 (SECURITY-AUDIT) — _ames_check_mug OOB read.
**
**  _ames_check_mug computes `len_w - rog_w` where rog_w = HEAD_SIZE +
**  origin_size. _ames_hear runs this check while only guaranteeing
**  len_w >= HEAD_SIZE (4); the prelude-size check that would require a
**  bigger packet runs *after*. A 4-byte packet with the relay bit set gives
**  rog_w = 10, so `len_w - rog_w` underflows to ~0xFFFFFFFF and u3r_mug_bytes
**  reads ~4 GB off the buffer. We allocate the buffer at exactly len_w so
**  ASan flags the over-read; the fix must drop (c3n) without dereferencing.
*/
static c3_i
_test_mug_underflow(void)
{
  c3_w  len_w = HEAD_SIZE;             // 4: smallest packet that reaches the mug check
  c3_y* hun_y = c3_malloc(len_w);      // exact-size: no slack for the OOB read

  memset(hun_y, 0, len_w);

  u3_pact pac_u;
  memset(&pac_u, 0, sizeof(pac_u));
  pac_u.len_w        = len_w;
  pac_u.hun_y        = hun_y;
  pac_u.hed_u.rel_o  = c3y;            // relay bit set -> origin_size 6 -> rog_w 10
  pac_u.hed_u.mug_l  = 0;

  c3_o ret_o = _ames_check_mug(&pac_u);

  c3_free(hun_y);

  if ( c3y == ret_o ) {
    fprintf(stderr, "ames: mug underflow: undersized relay packet not dropped\r\n");
    return 1;
  }

  return 0;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_ames();

  if ( _test_mug_underflow() ) {
    fprintf(stderr, "ames: mug underflow test failed\r\n");
    exit(1);
  }

  if ( _test_stun() ) {
    fprintf(stderr, "ames: stun tests failed\r\n");
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "ames okeedokee\n");
  return 0;
}
