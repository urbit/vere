/// @file

#include "./palloc.c"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 22);
  u3m_pave(c3y);
}

static void
_test_palloc(void)
{
  _init();

  c3_w *wor_w;
  u3_post pos_p = _imalloc(4);

  fprintf(stderr, "pos_p %x\n", pos_p);

  wor_w = u3a_into(pos_p);

  wor_w[0] = 0;
  wor_w[1] = 1;
  wor_w[2] = 2;
  wor_w[3] = 3;

  u3_post sop_p = _imalloc(4);

  fprintf(stderr, "sop_p %x\n", sop_p);

  _ifree(pos_p);
  _ifree(pos_p);
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_palloc();

  fprintf(stderr, "palloc okeedokee\n");
  return 0;
}
