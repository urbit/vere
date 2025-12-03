/// @file

#include "noun.h"
#include "vere.h"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);
}

/* _newt_encode(): synchronous serialization into a single buffer, for test purposes
*/
static c3_y*
_newt_encode(u3_atom mat, c3_d* len_d)
{
  c3_w  met_w = u3r_met(3, mat);
  c3_y* buf_y;

  //  validate that message size fits in 32-bit wire format
  //
  if ( 0xffffffff < met_w ) {
    fprintf(stderr, "newt: message too large for wire format\n");
    u3z(mat);
    return 0;
  }

  *len_d = 5 + met_w;
  buf_y  = c3_malloc(*len_d);

  //  write header (only 32 bits for length, matching wire format)
  //
  buf_y[0] = 0x0;
  buf_y[1] = ( met_w        & 0xff);
  buf_y[2] = ((met_w >>  8) & 0xff);
  buf_y[3] = ((met_w >> 16) & 0xff);
  buf_y[4] = ((met_w >> 24) & 0xff);

  u3r_bytes(0, met_w, buf_y + 5, mat);
  u3z(mat);

  return buf_y;
}

static c3_d
_moat_length(u3_moat* mot_u)
{
  u3_meat* met_u = mot_u->ext_u;
  c3_d     len_d = 0;

  while ( met_u ) {
    met_u = met_u->nex_u;
    len_d++;
  }

  return len_d;
}

/* _test_newt_smol(): various scenarios with small messages
*/
static void
_test_newt_smol(void)
{
  //  =(2 (jam 0))
  //
  u3_atom     a = u3ke_jam(0);
  u3_moat mot_u;
  c3_d    len_d;
  c3_y*   buf_y;

  memset(&mot_u, 0, sizeof(u3_moat));

  //  one message one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);
    u3_newt_decode(&mot_u, buf_y, len_d);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (a)\n");
      exit(1);
    }
  }

  //  two messages one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    buf_y = c3_realloc(buf_y, 2 * len_d);
    memcpy(buf_y + len_d, buf_y, len_d);
    len_d = 2 * len_d;

    u3_newt_decode(&mot_u, buf_y, len_d);

    if ( 2 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (b)\n");
      exit(1);
    }
  }

  //  one message two buffers
  //
  {
    c3_y* end_y;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    end_y = c3_malloc(1);
    end_y[0] = buf_y[len_d - 1];

    u3_newt_decode(&mot_u, buf_y, len_d - 1);

    if ( 0 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (c)\n");
      exit(1);
    }

    u3_newt_decode(&mot_u, end_y, 1);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (d)\n");
      exit(1);
    }
  }

  //  two messages two buffers (overlapping length)
  //
  {
    c3_y* haf_y;
    c3_d  haf_d, dub_d;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    dub_d = 2 * len_d;
    haf_d = len_d / 2;

    //  buf_y is all of message one, half of message two (not a full length)
    //
    buf_y = c3_realloc(buf_y, dub_d - haf_d);
    memcpy(buf_y + len_d, buf_y, len_d - haf_d);

    //  haf_y is the second half of message two
    //
    haf_y = c3_malloc(haf_d);
    memcpy(haf_y, buf_y + (len_d - haf_d), haf_d);

    u3_newt_decode(&mot_u, buf_y, dub_d - haf_d);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (e)\n");
      exit(1);
    }

    u3_newt_decode(&mot_u, haf_y, haf_d);

    if ( 2 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt smol fail (f)\n");
      exit(1);
    }
  }

  u3z(a);
}

/* _test_newt_vast(): various scenarios with larger messages
*/
static void
_test_newt_vast(void)
{
  //  =(53 (met 3 (jam "abcdefghijklmnopqrstuvwxyz")))
  //
  u3_atom     a = u3ke_jam(u3i_tape("abcdefghijklmnopqrstuvwxyz"));
  u3_moat mot_u;
  c3_d    len_d;
  c3_y*   buf_y;

  memset(&mot_u, 0, sizeof(u3_moat));

  //  one message one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);
    u3_newt_decode(&mot_u, buf_y, len_d);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (a)\n");
      exit(1);
    }
  }

  //  two messages one buffer
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    buf_y = c3_realloc(buf_y, 2 * len_d);
    memcpy(buf_y + len_d, buf_y, len_d);
    len_d = 2 * len_d;

    u3_newt_decode(&mot_u, buf_y, len_d);

    if ( 2 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (b)\n");
      exit(1);
    }
  }

  //  one message many buffers
  //
  {
    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    {
      c3_y* cop_y = c3_malloc(len_d);
      c3_d  haf_d = len_d / 2;
      memcpy(cop_y, buf_y, len_d);

      u3_newt_decode(&mot_u, buf_y, haf_d);

      while ( haf_d < len_d ) {
        c3_y* end_y = c3_malloc(1);
        end_y[0] = cop_y[haf_d];

        if ( 0 != _moat_length(&mot_u) ) {
          fprintf(stderr, "newt vast fail (c) %" PRIc3_d "\n", haf_d);
          exit(1);
        }

        u3_newt_decode(&mot_u, end_y, 1);
        haf_d++;
      }

      c3_free(cop_y);
    }

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (d)\n");
      exit(1);
    }
  }

  //  two messages two buffers
  //
  {
    c3_y* haf_y;
    c3_d  haf_d, dub_d;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    dub_d = 2 * len_d;
    haf_d = len_d / 2;

    //  buf_y is all of message one, half of message two
    //
    buf_y = c3_realloc(buf_y, dub_d - haf_d);
    memcpy(buf_y + len_d, buf_y, len_d - haf_d);

    //  haf_y is the second half of message two
    //
    haf_y = c3_malloc(haf_d);
    memcpy(haf_y, buf_y + (len_d - haf_d), haf_d);

    u3_newt_decode(&mot_u, buf_y, dub_d - haf_d);

    if ( 1 !=  _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (e)\n");
      exit(1);
    }

    u3_newt_decode(&mot_u, haf_y, haf_d);

    if ( 2 !=  _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (f)\n");
      exit(1);
    }
  }

  //  two messages many buffers
  //
  {
    c3_d dub_d;

    mot_u.ent_u = mot_u.ext_u = 0;

    buf_y = _newt_encode(u3k(a), &len_d);

    dub_d = 2 * len_d;

    //  buf_y is two copies of message
    //
    buf_y = c3_realloc(buf_y, dub_d);
    memcpy(buf_y + len_d, buf_y, len_d);

    {
      c3_y* cop_y = c3_malloc(dub_d);
      c3_d  haf_d = len_d + 1;
      memcpy(cop_y, buf_y, dub_d);

      u3_newt_decode(&mot_u, buf_y, haf_d);

      while ( haf_d < dub_d ) {
        c3_y* end_y = c3_malloc(1);
        end_y[0] = cop_y[haf_d];

        if ( 1 !=  _moat_length(&mot_u) ) {
          fprintf(stderr, "newt vast fail (g) %" PRIc3_d "\n", haf_d);
          exit(1);
        }

        u3_newt_decode(&mot_u, end_y, 1);
        haf_d++;
      }

      c3_free(cop_y);
    }

    if ( 2 !=  _moat_length(&mot_u) ) {
      fprintf(stderr, "newt vast fail (h)\n");
      exit(1);
    }
  }

  u3z(a);
}

/* _test_newt_head_rift(): test header parsing split across buffers
*/
static void
_test_newt_head_rift(void)
{
  //  test message with known jammed size
  //
  u3_atom     a = u3ke_jam(u3i_tape("test"));
  u3_moat mot_u;
  c3_d    len_d;
  c3_y*   buf_y;

  memset(&mot_u, 0, sizeof(u3_moat));

  buf_y = _newt_encode(u3k(a), &len_d);

  //  test splitting header at each byte position (0-4)
  //
  for ( c3_y i_y = 1; i_y < 5; i_y++ ) {
    c3_y* haf_y;
    c3_d  haf_d = len_d - i_y;

    mot_u.ent_u = mot_u.ext_u = 0;
    memset(&mot_u.mes_u, 0, sizeof(u3_mess));

    haf_y = c3_malloc(i_y);
    memcpy(haf_y, buf_y + haf_d, i_y);

    //  decode first part (should not complete)
    //
    u3_newt_decode(&mot_u, buf_y, haf_d);

    if ( 0 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt header split fail (a) at byte %u\n", i_y);
      exit(1);
    }

    //  decode second part (should complete)
    //
    u3_newt_decode(&mot_u, haf_y, i_y);

    if ( 1 != _moat_length(&mot_u) ) {
      fprintf(stderr, "newt header split fail (b) at byte %u\n", i_y);
      exit(1);
    }
  }

  c3_free(buf_y);
  u3z(a);
}

/* _test_newt_zero_mess(): test rejection of zero-length message
*/
static void
_test_newt_zero_mess(void)
{
  u3_moat mot_u;
  c3_y    buf_y[5];

  memset(&mot_u, 0, sizeof(u3_moat));

  //  construct zero-length message header
  //
  buf_y[0] = 0x0;  // version
  buf_y[1] = 0x0;  // length = 0
  buf_y[2] = 0x0;
  buf_y[3] = 0x0;
  buf_y[4] = 0x0;

  //  should reject zero-length message
  //
  if ( c3n != u3_newt_decode(&mot_u, buf_y, 5) ) {
    fprintf(stderr, "newt zero message fail: should have rejected\n");
    exit(1);
  }
}

/* _test_newt_sick_vers(): test rejection of invalid version byte
*/
static void
_test_newt_sick_vers(void)
{
  u3_moat mot_u;
  c3_y    buf_y[5];

  memset(&mot_u, 0, sizeof(u3_moat));

  //  construct message with invalid version
  //
  buf_y[0] = 0x1;  // invalid version (should be 0x0)
  buf_y[1] = 0x1;  // length = 1
  buf_y[2] = 0x0;
  buf_y[3] = 0x0;
  buf_y[4] = 0x0;

  //  should reject invalid version
  //
  if ( c3n != u3_newt_decode(&mot_u, buf_y, 5) ) {
    fprintf(stderr, "newt invalid version fail: should have rejected\n");
    exit(1);
  }
}

/* _test_newt_vast_size(): test handling of large 32-bit message sizes
*/
static void
_test_newt_vast_size(void)
{
  u3_moat mot_u;
  c3_y    buf_y[10];

  memset(&mot_u, 0, sizeof(u3_moat));

  //  construct header for large message (e.g., 16MB)
  //  note: we only test header parsing, not actual allocation
  //
  c3_h len_h = 0x01000000;  // 16MB

  buf_y[0] = 0x0;
  buf_y[1] = ( len_h        & 0xff);
  buf_y[2] = ((len_h >>  8) & 0xff);
  buf_y[3] = ((len_h >> 16) & 0xff);
  buf_y[4] = ((len_h >> 24) & 0xff);

  //  add a few body bytes
  //
  buf_y[5] = 0xaa;
  buf_y[6] = 0xbb;
  buf_y[7] = 0xcc;
  buf_y[8] = 0xdd;
  buf_y[9] = 0xee;

  //  should accept header and start accumulating body
  //
  if ( c3n == u3_newt_decode(&mot_u, buf_y, 10) ) {
    fprintf(stderr, "newt large length fail: should have accepted\n");
    exit(1);
  }

  //  verify we're in tail state waiting for more data
  //
  if ( u3_mess_tail != mot_u.mes_u.sat_e ) {
    fprintf(stderr, "newt large length fail: wrong state\n");
    exit(1);
  }

  //  verify partial length matches
  //
  if ( 5 != mot_u.mes_u.tal_u.has_d ) {
    fprintf(stderr, "newt large length fail: wrong partial length\n");
    exit(1);
  }

  //  cleanup
  //
  if ( mot_u.mes_u.tal_u.met_u ) {
    c3_free(mot_u.mes_u.tal_u.met_u);
  }
}

/* _test_newt_size_edge(): test maximum valid 32-bit message length
*/
static void
_test_newt_size_edge(void)
{
  u3_moat mot_u;
  c3_y    buf_y[6];

  memset(&mot_u, 0, sizeof(u3_moat));

  //  construct header for maximum 32-bit message (0xffffffff bytes)
  //
  buf_y[0] = 0x0;
  buf_y[1] = 0xff;
  buf_y[2] = 0xff;
  buf_y[3] = 0xff;
  buf_y[4] = 0xff;
  buf_y[5] = 0xaa;  // one body byte

  //  should accept maximum size
  //
  if ( c3n == u3_newt_decode(&mot_u, buf_y, 6) ) {
    fprintf(stderr, "newt length boundary fail: should have accepted\n");
    exit(1);
  }

  //  verify we're in tail state
  //
  if ( u3_mess_tail != mot_u.mes_u.sat_e ) {
    fprintf(stderr, "newt length boundary fail: wrong state\n");
    exit(1);
  }

  //  verify expected length
  //
  if ( 0xffffffff != mot_u.mes_u.tal_u.met_u->len_d ) {
    fprintf(stderr, "newt length boundary fail: wrong length\n");
    exit(1);
  }

  //  cleanup
  //
  if ( mot_u.mes_u.tal_u.met_u ) {
    c3_free(mot_u.mes_u.tal_u.met_u);
  }
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_newt_smol();
  _test_newt_vast();
  _test_newt_head_rift();
  _test_newt_zero_mess();
  _test_newt_sick_vers();
  _test_newt_vast_size();
  _test_newt_size_edge();

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "test_newt: ok\n");

  return 0;
}
