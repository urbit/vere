/// @file

#include "noun.h"

//  include the jet source directly so we can drive the static
//  arm implementations without standing up full cores
//
#include "jets/e/bytestream.c"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 20);
  u3m_pave(c3y);
}

/* _test_skip_line_oob(): H1 (SECURITY-AUDIT) — OOB read in bytestream:skip-line.
**
**  `pos_w` comes straight from the attacker-supplied `pos` with no bound vs
**  `len_w`. When `pos >= len_w` the `while (pos_w < len_w)` loop never runs
**  and the unconditional `*(sea_y + pos_w)` at the no-newline check reads
**  far past the buffer (~4 GB with pos = 0xFFFFFFFF). The fix must guard
**  `pos_w >= len_w` before dereferencing.
*/
static c3_i
_test_skip_line_oob(void)
{
  c3_y    dat_y[4] = { 'a', 'b', 'c', 'd' };
  u3_atom dat      = u3i_bytes(sizeof(dat_y), dat_y);
  u3_noun octs     = u3nc(u3i_word(sizeof(dat_y)), dat);

  //  position far past the end of the 4-byte buffer
  //
  u3_atom pos = u3i_word(0xFFFFFFFF);

  u3_noun res = _qe_bytestream_skip_line(pos, octs);

  if ( u3_none == res ) {
    fprintf(stderr, "skip-line: unexpected u3_none\r\n");
    u3z(pos);
    u3z(octs);
    return 1;
  }

  u3z(res);
  u3z(pos);
  u3z(octs);
  return 0;
}

/* _test_skip_line_end(): H1 secondary — one-past-the-end read at buffer end.
**
**  Even with a valid `pos`, when no newline is present the loop exits at
**  `pos_w == len_w` and the no-newline check reads `sea_y[len_w]`, one byte
**  past the metered length. Must not dereference past the buffer.
*/
static c3_i
_test_skip_line_end(void)
{
  c3_y    dat_y[3] = { 'x', 'y', 'z' };  // no '\n'
  u3_atom dat      = u3i_bytes(sizeof(dat_y), dat_y);
  u3_noun octs     = u3nc(u3i_word(sizeof(dat_y)), dat);
  u3_atom pos      = u3i_word(0);

  u3_noun res = _qe_bytestream_skip_line(pos, octs);

  if ( u3_none == res ) {
    fprintf(stderr, "skip-line end: unexpected u3_none\r\n");
    u3z(pos);
    u3z(octs);
    return 1;
  }

  u3z(res);
  u3z(pos);
  u3z(octs);
  return 0;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  c3_i ret_i = 0;

  ret_i |= _test_skip_line_oob();
  ret_i |= _test_skip_line_end();

  if ( ret_i ) {
    fprintf(stderr, "test bytestream: failed\r\n");
    exit(1);
  }

  u3m_grab(u3_none);

  fprintf(stderr, "test bytestream: ok\r\n");
  return 0;
}
