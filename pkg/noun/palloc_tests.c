/// @file

#include "events.h"

struct heap {
  u3p(u3a_dell)  fre_p;               //  free list
  u3p(u3a_dell)  erf_p;               //  free list
  u3p(u3a_dell)  cac_p;               //  cached pgfree struct
  u3_post        bot_p;               //  XX s/b rut_p
  c3_ws          dir_ws;              //  1 || -1 (multiplicand for local offsets)
  c3_ws          off_ws;              //  0 || -1 (word-offset for hat && rut)
  c3_w           siz_w;               //  directory size
  c3_w           len_w;               //  directory entries
  u3p(u3a_crag*) pag_p;               //  directory
  u3p(u3a_crag)  wee_p[u3a_crag_no];  //  chunk lists
};

struct heap hep_u;

#define HEAP  (hep_u)
#define BASE  (hep_u.bot_p)

#include "./palloc.c"

/* _setup(): prepare for tests.
*/
static void
_setup(size_t len_i)
{
  u3m_init((size_t)1 << len_i);
  u3e_init();
  u3m_pave(c3y);
}

static void
_test_print_chunks(void)
{
  u3a_hunk_dose *hun_u;
  c3_g met_g;
  c3_w hun_w;

  for ( c3_g bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
    hun_u = &(u3a_Hunk[bit_g]);
    met_g = (c3_g)c3_bits_word((c3_w)hun_u->siz_s - 1) - u3a_min_log;
    hun_w = 1U + ((hun_u->siz_s - 1) >> hun_u->log_s);

    fprintf(stderr, "chunks: %s pginfo: bit=%"PRIc3_s" log=%"PRIc3_s" len=%"PRIc3_s" tot=%"PRIc3_s", siz=%"PRIc3_s", chunks=%"PRIc3_w" met=%"PRIc3_s"\n",
                    ( hun_u->hun_s ? "inline" : "malloc" ), bit_g,
                    hun_u->log_s, hun_u->len_s, hun_u->tot_s, hun_u->siz_s, hun_w, met_g);
  }
}

static void
_test_print_pages(c3_w max_w)
{
  u3_post pot_p;
  c3_w i_w;

  hep_u.dir_ws = 1;
  hep_u.off_ws = 0;
  hep_u.bot_p  = 0x1000;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    pot_p = page_to_post(i_w);
    fprintf(stderr, "north at bot=0x%"PRIxc3_w" pag=%"PRIc3_w" == 0x%"PRIxc3_w" == pag=%"PRIc3_w"\n",
                    hep_u.bot_p, i_w, pot_p, post_to_page(pot_p));
  }


  hep_u.dir_ws = -1;
  hep_u.off_ws = -1;
  hep_u.bot_p += max_w << u3a_page;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    pot_p = page_to_post(i_w);
    fprintf(stderr, "south at bot=0x%"PRIxc3_w" pag=%"PRIc3_w" == 0x%"PRIxc3_w" == pag=%"PRIc3_w"\n",
                    hep_u.bot_p, i_w, pot_p, post_to_page(pot_p));
  }
}

void
u3m_fall(void);
void
u3m_leap(c3_w pad_w);

static void
_test_palloc(void)
{
  c3_w *wor_w;
  u3_post pos_p, sop_p;
  struct heap tmp_u;

  memset(&(HEAP), 0x0, sizeof(HEAP));
  _init_heap();

  pos_p = _imalloc(4);

  fprintf(stderr, "north: pos_p %"PRIxc3_w"\n", pos_p);

  wor_w = u3a_into(pos_p);

  wor_w[0] = 0;
  wor_w[1] = 1;
  wor_w[2] = 2;
  wor_w[3] = 3;

  sop_p = _imalloc(4);

  fprintf(stderr, "north: sop_p %"PRIxc3_w"\n", sop_p);

  _ifree(pos_p);
  _ifree(sop_p);

  fprintf(stderr, "palloc_tests: pre-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memcpy(&tmp_u, &hep_u, sizeof(tmp_u));
  u3m_leap(1U << u3a_page);

  fprintf(stderr, "palloc_tests: post-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memset(&(HEAP), 0x0, sizeof(HEAP));
  _init_heap();

  pos_p = _imalloc(4);

  fprintf(stderr, "south: pos_p %"PRIxc3_w"\n", pos_p);

  wor_w = u3a_into(pos_p);

  wor_w[0] = 0;
  wor_w[1] = 1;
  wor_w[2] = 2;
  wor_w[3] = 3;

  sop_p = _imalloc(4);

  fprintf(stderr, "south: sop_p %"PRIxc3_w"\n", sop_p);

  _ifree(pos_p);
  _ifree(sop_p);

  fprintf(stderr, "palloc_tests: pre-fall: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  u3m_fall();
  memcpy(&hep_u, &tmp_u, sizeof(tmp_u));

  fprintf(stderr, "palloc_tests: post-fall: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  pos_p = _imalloc(4);

  fprintf(stderr, "north: pos_p %"PRIxc3_w"\n", pos_p);

  wor_w = u3a_into(pos_p);

  wor_w[0] = 0;
  wor_w[1] = 1;
  wor_w[2] = 2;
  wor_w[3] = 3;

  sop_p = _imalloc(4);

  fprintf(stderr, "north: sop_p %"PRIxc3_w"\n", sop_p);

  _ifree(pos_p);
  _ifree(sop_p);
}

#ifdef VERE64
static void
_test_palloc_64(void)
{
  c3_w *wor_w;
  u3_post pos_p, sop_p;
  struct heap tmp_u;
  c3_w siz_w = (1ULL << 33) - ((1ULL << 33) / (1ULL << 10));  // just under 64GiB in words

  memset(&(HEAP), 0x0, sizeof(HEAP));
  _init_heap();

  pos_p = _imalloc(siz_w);

  fprintf(stderr, "north: pos_p %"PRIxc3_w" (large)\n", pos_p);

  wor_w = u3a_into(pos_p);

  wor_w[0] = 0xdeadbeef;
  wor_w[1] = 0xcafebabe;
  wor_w[siz_w-2] = 0xfeedface;
  wor_w[siz_w-1] = 0xbaadf00d;

  sop_p = _imalloc(siz_w);

  fprintf(stderr, "north: sop_p %"PRIxc3_w" (large)\n", sop_p);

  _ifree(pos_p);
  _ifree(sop_p);

  fprintf(stderr, "palloc_tests_64: pre-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memcpy(&tmp_u, &hep_u, sizeof(tmp_u));
  u3m_leap(1U << u3a_page);

  fprintf(stderr, "palloc_tests_64: post-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memset(&(HEAP), 0x0, sizeof(HEAP));
  _init_heap();

  pos_p = _imalloc(siz_w);

  fprintf(stderr, "south: pos_p %"PRIxc3_w" (large)\n", pos_p);

  wor_w = u3a_into(pos_p);

  wor_w[0] = 0xdeadbeef;
  wor_w[1] = 0xcafebabe;
  wor_w[siz_w-2] = 0xfeedface;
  wor_w[siz_w-1] = 0xbaadf00d;

  sop_p = _imalloc(siz_w);

  _ifree(pos_p);
  _ifree(sop_p);

  fprintf(stderr, "palloc_tests_64: pre-fall: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  u3m_fall();
  memcpy(&hep_u, &tmp_u, sizeof(tmp_u));

  fprintf(stderr, "palloc_tests_64: post-fall: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  pos_p = _imalloc(siz_w);

  fprintf(stderr, "north: pos_p %"PRIxc3_w" (large)\n", pos_p);

  wor_w = u3a_into(pos_p);

  // Initialize first few and last few words
  wor_w[0] = 0xdeadbeef;
  wor_w[1] = 0xcafebabe;
  wor_w[siz_w-2] = 0xfeedface;
  wor_w[siz_w-1] = 0xbaadf00d;

  sop_p = _imalloc(siz_w);

  fprintf(stderr, "north: sop_p %"PRIxc3_w" (large)\n", sop_p);

  _ifree(pos_p);
  _ifree(sop_p);
}
#endif

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
#ifdef VERE64
  //  XX: test on x86_64-linux
  _setup(40);  // 1TiB
#else
  _setup(32);  //  32GiB
#endif

  _test_print_chunks();
  _test_print_pages(10);
  fprintf(stderr, "\n");

  _test_palloc();
  fprintf(stderr, "palloc okeedokee\n\n");

#ifdef VERE64
  _test_palloc_64();
  fprintf(stderr, "palloc_64 okeedokee\n");
#endif

  return 0;
}
