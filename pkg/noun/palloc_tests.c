/// @file

#include "events.h"

struct heap {
  u3p(u3a_dell)  fre_p;               //  free list
  u3p(u3a_dell)  erf_p;               //  free list
  u3p(u3a_dell)  cac_p;               //  cached pgfree struct
  u3_post        rut_p;               //  bottom
  c3_ws          dir_ws;              //  1 || -1 (multiplicand for local offsets)
  c3_ws          off_ws;              //  0 || -1 (word-offset for hat && rut)
  c3_w           siz_w;               //  directory size
  c3_w           len_w;               //  directory entries
  u3p(u3a_crag*) pag_p;               //  directory
  u3p(u3a_crag)  wee_p[u3a_crag_no];  //  chunk lists
};

struct heap hep_u;

#define HEAP  (hep_u)
#define BASE  (hep_u.rut_p)

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
  hep_u.rut_p  = 0x1000;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    pot_p = page_to_post(i_w);
    fprintf(stderr, "north at rut=0x%"PRIxc3_w" pag=%"PRIc3_w" == 0x%"PRIxc3_w" == pag=%"PRIc3_w"\n",
                    hep_u.rut_p, i_w, pot_p, post_to_page(pot_p));
  }


  hep_u.dir_ws = -1;
  hep_u.off_ws = -1;
  hep_u.rut_p += max_w << u3a_page;

  for ( i_w = 0; i_w < max_w; i_w++ ) {
    pot_p = page_to_post(i_w);
    fprintf(stderr, "south at rut=0x%"PRIxc3_w" pag=%"PRIc3_w" == 0x%"PRIxc3_w" == pag=%"PRIc3_w"\n",
                    hep_u.rut_p, i_w, pot_p, post_to_page(pot_p));
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
  u3R->hat_p = u3R->rut_p;  // reset heap to empty state
  HEAP.rut_p = u3R->rut_p;  // set base pointer for test heap
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
  u3m_leap(((c3_w)1) << u3a_page);

  fprintf(stderr, "palloc_tests: post-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memset(&(HEAP), 0x0, sizeof(HEAP));
  u3R->hat_p = u3R->rut_p;  // reset heap to empty state
  HEAP.rut_p = u3R->rut_p;  // set base pointer for test heap
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

/* _test_jump_check(): class [bit_g]'s list holds exactly its chunk pages
**                     with a free chunk; returns out-of-order links.
*/
static c3_w
_test_jump_check(c3_g bit_g)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_y*  see_y = c3_calloc(HEAP.len_w);
  c3_w   lis_w = 0, liv_w = 0, inv_w = 0, pag_w;
  u3_post bit_p = HEAP.wee_p[bit_g];
  u3a_crag* pre_u = 0;

  while ( bit_p ) {
    u3a_crag* bit_u = u3to(u3a_crag, bit_p);

    if (  (bit_u->pag_w >= HEAP.len_w)
       || (dir_u[bit_u->pag_w] != bit_p)
       || ((bit_g + u3a_min_log) != bit_u->log_s)
       || !bit_u->fre_s
       || see_y[bit_u->pag_w] )
    {
      fprintf(stderr, "palloc: jump: bad list entry, page %"PRIc3_w"\r\n",
                      bit_u->pag_w);
      exit(1);
    }

    see_y[bit_u->pag_w] = 1;
    lis_w++;
    if ( pre_u && (pre_u->pag_w > bit_u->pag_w) ) inv_w++;
    pre_u = bit_u;
    bit_p = bit_u->nex_p;
  }

  for ( pag_w = 0; pag_w < HEAP.len_w; pag_w++ ) {
    u3_post dir_p = dir_u[pag_w];

    if ( dir_p > u3a_rest_pg ) {
      u3a_crag* pag_u = u3to(u3a_crag, dir_p);

      if ( ((bit_g + u3a_min_log) == pag_u->log_s) && pag_u->fre_s ) {
        liv_w++;
      }
    }
  }

  if ( lis_w != liv_w ) {
    fprintf(stderr, "palloc: jump: %"PRIc3_w" listed, %"PRIc3_w" with room\r\n",
                    lis_w, liv_w);
    exit(1);
  }

  c3_free(see_y);
  return inv_w;
}

/* _test_jump(): random allocations and frees on the home road keep the
**               chunk-page lists exact, with stale hints dropped.
*/
static void
_test_jump(void)
{
  const c3_w num_w = 1U << 17;
  u3_post*   pos_p = c3_calloc(sizeof(u3_post) * num_w);
  c3_d       ran_d = 0x9e3779b97f4a7c15ULL;
  c3_w       rnd_w, i_w, j_w, inv_w = 0;

  memset(&(HEAP), 0x0, sizeof(HEAP));
  u3R->hat_p = u3R->rut_p;
  HEAP.rut_p = u3R->rut_p;
  _init_heap();
  _jump_reset();

#define _RAND() ( ran_d ^= ran_d << 13, ran_d ^= ran_d >> 7, \
                  ran_d ^= ran_d << 17, (c3_w)ran_d )

  for ( rnd_w = 0; rnd_w < 8; rnd_w++ ) {
    //  fill empty slots: mostly the smallest classes, some larger,
    //  some multi-page, so emptied pages get reused differently
    //
    for ( i_w = 0; i_w < num_w; i_w++ ) {
      if ( !pos_p[i_w] ) {
        c3_w r_w   = _RAND() & 255;
        c3_w len_w = ( r_w < 200 ) ? u3a_minimum
                   : ( r_w < 250 ) ? (c3_w)1 << (u3a_min_log + 1 + (r_w & 3))
                   : ((c3_w)1 << u3a_page) + (r_w & 7);
        pos_p[i_w] = _imalloc(len_w);
      }
    }

    //  free a random half, so pages go non-full and empty in random order
    //
    for ( j_w = 0; j_w < (num_w >> 1); j_w++ ) {
      i_w = _RAND() & (num_w - 1);
      if ( pos_p[i_w] ) {
        _ifree(pos_p[i_w]);
        pos_p[i_w] = 0;
      }
    }

    for ( c3_g bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
      inv_w += _test_jump_check(bit_g);
    }
  }

#undef _RAND

  for ( i_w = 0; i_w < num_w; i_w++ ) {
    if ( pos_p[i_w] ) _ifree(pos_p[i_w]);
  }

  //  NB: free page runs' records are chunks, so lists needn't be empty
  //
  for ( c3_g bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
    (void)_test_jump_check(bit_g);
  }

  fprintf(stderr, "palloc: jump: %"PRIc3_w" out-of-order links\r\n", inv_w);
  c3_free(pos_p);
}

#ifdef VERE64
static void
_test_palloc_d(void)
{
  c3_w *wor_w;
  u3_post pos_p, sop_p;
  struct heap tmp_u;
  c3_w siz_w = (1ULL << 33) - ((1ULL << 33) / (1ULL << 10));  // just under 64GiB in words

  memset(&(HEAP), 0x0, sizeof(HEAP));
  u3R->hat_p = u3R->rut_p;  // reset heap to empty state
  HEAP.rut_p = u3R->rut_p;  // set base pointer for test heap
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

  fprintf(stderr, "palloc_tests_d: pre-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memcpy(&tmp_u, &hep_u, sizeof(tmp_u));
  u3m_leap(((c3_w)1) << u3a_page);

  fprintf(stderr, "palloc_tests_d: post-leap: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  memset(&(HEAP), 0x0, sizeof(HEAP));
  u3R->hat_p = u3R->rut_p;  // reset heap to empty state
  HEAP.rut_p = u3R->rut_p;  // set base pointer for test heap
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

  fprintf(stderr, "palloc_tests_d: pre-fall: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

  u3m_fall();
  memcpy(&hep_u, &tmp_u, sizeof(tmp_u));

  fprintf(stderr, "palloc_tests_d: post-fall: hat=0x%"PRIxc3_w" cap=0x%"PRIxc3_w"\n", u3R->hat_p, u3R->cap_p);

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
  _setup(40);  //  1TiB
#else
  _setup(32);  //  4GiB
#endif

  _test_print_chunks();
  _test_print_pages(10);
  fprintf(stderr, "\n");

  _test_jump();
  fprintf(stderr, "palloc jump okeedokee\n\n");

  _test_palloc();
  fprintf(stderr, "palloc okeedokee\n\n");

#ifdef VERE64
  _test_palloc_d();
  fprintf(stderr, "palloc_d okeedokee\n");
#endif

  return 0;
}
