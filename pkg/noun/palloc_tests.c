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
_setup(void)
{
  u3m_init(1 << 22);
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

    fprintf(stderr, "chunks: %s pginfo: bit=%u log=%u len=%u tot=%u, siz=%u, chunks=%u met=%u\n",
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
    fprintf(stderr, "north at bot=0x%x pag=%u == 0x%x == pag=%u\n",
                    hep_u.bot_p, i_w, pot_p, post_to_page(pot_p));
  }


  hep_u.dir_ws = -1;
  hep_u.off_ws = -1;
  hep_u.bot_p += max_w << u3a_page;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    pot_p = page_to_post(i_w);
    fprintf(stderr, "south at bot=0x%x pag=%u == 0x%x == pag=%u\n",
                    hep_u.bot_p, i_w, pot_p, post_to_page(pot_p));
  }
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_print_chunks();
  _test_print_pages(10);
  return 0;
}
