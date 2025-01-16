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
_print_chunks(c3_g bit_g)  // 0-9, inclusive
{
  c3_s    log_s = bit_g + LOG_MINIMUM;
  c3_s    len_s = 1U << log_s;
  c3_s    tot_s = 1U << (u3a_page - log_s);  // 2-1.024, inclusive
  c3_s    siz_s = c3_wiseof(struct pginfo);

  siz_s += tot_s >> 5;
  siz_s += !!(tot_s & 31);
  siz_s--;

  if ( len_s <= (siz_s << 1) ) {
    fprintf(stderr, "chunks: inline pginfo: bit=%u log=%u len=%u tot=%u, siz=%u, chunks=%u\n",
                    bit_g, log_s, len_s, tot_s, siz_s, (siz_s / len_s + !!(siz_s % len_s)));


  }
  else {
    fprintf(stderr, "chunks: malloc pginfo: bit=%u log=%u len=%u tot=%u, siz=%u chunks=%u\n",
                    bit_g, log_s, len_s, tot_s, siz_s, (siz_s / len_s + !!(siz_s % len_s)));
  }
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

  for ( c3_w i_w = 0; i_w < 10; i_w++ ) {
    _print_chunks(i_w);
  }

  _test_palloc();

  fprintf(stderr, "palloc okeedokee\n");
  return 0;
}
