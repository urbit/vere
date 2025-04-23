/// @file

#include "./io/xmas.c"

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
_test_xmas(void)
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



  u3_xmas_name pok_u = (u3_xmas_name) {
    .her_d = {43, 0},
    .pat_c = "/foo/bar",
    .pat_s = 8,
    .boq_s = 13,
    .fra_s = 1,
  };
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_xmas();

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "xmas okeedokee\n");
  return 0;
}
