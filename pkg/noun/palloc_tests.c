/// @file

#include "events.h"

#include <sys/wait.h>

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

#define PACK_LIVE 4096

static u3_post liv_p[PACK_LIVE];  //  tracked allocations (current)
static u3_post org_p[PACK_LIVE];  //  tracked allocations (pre-pack)
static c3_w    liv_w;

/* _pack_heap(): reset to a fresh, empty test heap.
*/
static void
_pack_heap(void)
{
  memset(&(HEAP), 0x0, sizeof(HEAP));
  u3R->hat_p = u3R->rut_p;
  HEAP.rut_p = u3R->rut_p;
  _init_heap();
  liv_w = 0;
}

/* _pack_class(): find a chunk class with off-page metadata ([lag_g]),
**                whose metadata class ([met_g]) isn't the free-list class.
*/
static c3_t
_pack_class(c3_g* lag_g, c3_g* met_g)
{
  c3_w dex_w = c3_max(c3_wiseof(u3a_dell), u3a_minimum);
  c3_g del_g = (c3_g)c3_bits_word(dex_w - 1) - u3a_min_log;

  for ( c3_g bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
    const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
    c3_w siz_w = c3_max((c3_w)hun_u->siz_s, u3a_minimum);
    c3_g cag_g = (c3_g)c3_bits_word(siz_w - 1) - u3a_min_log;

    if ( !hun_u->hun_s && (cag_g != del_g) ) {
      *lag_g = bit_g;
      *met_g = cag_g;
      return 1;
    }
  }

  return 0;
}

/* _pack_live(): allocate and track [len_w] words.
*/
static u3_post
_pack_live(c3_w len_w)
{
  u3_post som_p = _imalloc(len_w);

  u3_assert( liv_w < PACK_LIVE );
  liv_p[liv_w++] = som_p;
  return som_p;
}

/* _pack_drop(): free a tracked allocation.
*/
static void
_pack_drop(u3_post som_p)
{
  for ( c3_w i_w = 0; i_w < liv_w; i_w++ ) {
    if ( som_p == liv_p[i_w] ) {
      _ifree(som_p);
      liv_p[i_w] = liv_p[--liv_w];
      return;
    }
  }

  u3_assert(!"pack: untracked");
}

/* _pack_fill(): allocate (tracked) from [bit_g] until no partial pages remain.
*/
static c3_w
_pack_fill(c3_g bit_g, u3_post* out_p)
{
  c3_w len_w = 0;

  while ( HEAP.wee_p[bit_g] ) {
    u3_post som_p = _pack_live(u3a_Hunk[bit_g].len_s);

    if ( out_p ) {
      out_p[len_w] = som_p;
    }

    len_w++;
  }

  return len_w;
}

/* _pack_page(): allocate a single page.
*/
static u3_post
_pack_page(void)
{
  return _imalloc(((c3_w)1) << u3a_page);
}

/* _pack_tag(): distinctive contents for an allocation.
*/
static inline c3_w
_pack_tag(u3_post som_p)
{
  return (c3_w)0x5ca1ab1e ^ (c3_w)som_p;
}

/* _pack_sane(): check the free list against the page directory (north).
*/
static c3_t
_pack_sane(const c3_c* cap_c)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3p(u3a_dell)  pre_p = 0, fre_p = HEAP.fre_p;
  c3_w           pag_w = 0, bad_w = 0;

  while ( fre_p ) {
    u3a_dell* fre_u = u3to(u3a_dell, fre_p);
    c3_w      nex_w = fre_u->pag_w + fre_u->siz_w;

    if (  (fre_u->pre_p != pre_p)
       || (fre_u->pag_w < pag_w)
       || (nex_w > HEAP.len_w) )
    {
      fprintf(stderr, "pack: %s: free list hosed at page %"PRIc3_w"\r\n",
                      cap_c, fre_u->pag_w);
      return 0;
    }

    for ( ; pag_w < fre_u->pag_w; pag_w++ ) {
      if ( u3a_free_pg == dir_u[pag_w] ) {
        fprintf(stderr, "pack: %s: page %"PRIc3_w" free, not listed\r\n", cap_c, pag_w);
        bad_w++;
      }
    }

    for ( ; pag_w < nex_w; pag_w++ ) {
      if ( u3a_free_pg != dir_u[pag_w] ) {
        fprintf(stderr, "pack: %s: page %"PRIc3_w" listed, not free\r\n", cap_c, pag_w);
        bad_w++;
      }
    }

    pre_p = fre_p;
    fre_p = fre_u->nex_p;
  }

  for ( ; pag_w < HEAP.len_w; pag_w++ ) {
    if ( u3a_free_pg == dir_u[pag_w] ) {
      fprintf(stderr, "pack: %s: page %"PRIc3_w" free, not listed\r\n", cap_c, pag_w);
      bad_w++;
    }
  }

  if ( HEAP.erf_p != pre_p ) {
    fprintf(stderr, "pack: %s: free list tail wrong\r\n", cap_c);
    return 0;
  }

  if ( bad_w ) {
    fprintf(stderr, "pack: %s: %"PRIc3_w" pages disagree with free list\r\n",
                    cap_c, bad_w);
    return 0;
  }

  return 1;
}

/* _pack_check(): tag tracked allocations, compact the test heap
**                (as u3m_pack() sans road roots), and verify.
*/
static c3_t
_pack_check(const c3_c* cap_c)
{
  c3_t ret_t = 1;

  for ( c3_w i_w = 0; i_w < liv_w; i_w++ ) {
    c3_w* wor_w = u3a_into(liv_p[i_w]);
    org_p[i_w] = liv_p[i_w];
    wor_w[0]   = _pack_tag(org_p[i_w]);
    wor_w[1]   = ~_pack_tag(org_p[i_w]);
  }

  //  NB: u3a_pack_init() sizes from u3R->hep, not the test heap
  //
  {
    c3_w bit_w = (HEAP.len_w + (u3a_word_bits-1)) >> u3a_word_bits_log;
    u3a_Gack.bit_w = c3_calloc(sizeof(c3_w) * bit_w);
    u3a_Gack.pap_w = c3_calloc(sizeof(c3_w) * bit_w);
    u3a_Gack.pum_w = c3_calloc(sizeof(c3_w) * bit_w);
    u3a_Gack.siz_w = HEAP.siz_w * 2;
    u3a_Gack.len_w = HEAP.len_w;
    u3a_Gack.buf_w = c3_calloc(sizeof(c3_w) * u3a_Gack.siz_w);
  }

  _pack_seek();
  _pack_relocate_heap();

  for ( c3_w i_w = 0; i_w < liv_w; i_w++ ) {
    liv_p[i_w] = _pack_relocate(liv_p[i_w]);
  }

  _pack_move();
  u3a_pack_done();
  c3_free(u3a_Gack.bit_w);

  u3R->hat_p = u3R->rut_p + (HEAP.dir_ws * (c3_ws)(HEAP.len_w << u3a_page));

  for ( c3_w i_w = 0; i_w < liv_w; i_w++ ) {
    c3_w* wor_w = u3a_into(liv_p[i_w]);

    if (  (wor_w[0] != _pack_tag(org_p[i_w]))
       || (wor_w[1] != ~_pack_tag(org_p[i_w])) )
    {
      fprintf(stderr, "pack: %s: allocation 0x%"PRIxc3_w" -> 0x%"PRIxc3_w" clobbered\r\n",
                      cap_c, org_p[i_w], liv_p[i_w]);
      ret_t = 0;
    }
  }

  ret_t &= _pack_sane(cap_c);

  while ( liv_w ) {
    _pack_drop(liv_p[liv_w - 1]);
  }

  ret_t &= _pack_sane(cap_c);

  return ret_t;
}

/* _test_pack_stale_tail(): pack frees the last heap page while the page
**   before it is free, after pack released that page's free-list entry.
**
**   pages: [dir] [dell] [A: full-1] [met] [B: 1 chunk] [gap: free] [tail: B's crag]
*/
static c3_t
_test_pack_stale_tail(void)
{
  static u3_post fil_p[PACK_LIVE];
  const u3a_hunk_dose *lag_u, *met_u;
  u3p(u3a_crag)       *dir_u;
  u3_post  hol_p, gap_p, spa_p, cag_p, one_p;
  c3_w     fil_w, tal_w;
  c3_g     lag_g, met_g;

  _pack_heap();

  if ( !_pack_class(&lag_g, &met_g) ) {
    fprintf(stderr, "pack: stale tail: no suitable class\r\n");
    return 0;
  }

  lag_u = &(u3a_Hunk[lag_g]);
  met_u = &(u3a_Hunk[met_g]);

  //  pin the free-list entry page, so releasing entries can't free it
  //
  (void)_pack_live(c3_wiseof(u3a_dell));

  //  page A (full), and its metadata page (full)
  //
  (void)_pack_live(lag_u->len_s);
  fil_w = _pack_fill(lag_g, fil_p);
  (void)_pack_fill(met_g, 0);

  hol_p = _pack_page();                 //  future page B
  gap_p = _pack_page();                 //  future free page
  spa_p = _imalloc(met_u->len_s);       //  opens tail metadata page
  cag_p = page_to_post(post_to_page(spa_p));

  _ifree(hol_p);
  one_p = _pack_live(lag_u->len_s);     //  page B, crag on tail page
  _ifree(spa_p);
  _ifree(gap_p);

  //  B's chunk now fits on page A
  //
  _pack_drop(fil_p[fil_w - 1]);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  tal_w = HEAP.len_w - 1;

  if (  (post_to_page(cag_p) != tal_w)
     || (post_to_page(gap_p) != (tal_w - 1))
     || (post_to_page(one_p) != post_to_page(hol_p))
     || (u3a_free_pg != dir_u[tal_w - 1])
     || (u3to(u3a_crag, dir_u[tal_w])->fre_s != (met_u->ful_s - 1))
     || !HEAP.erf_p
     || (u3to(u3a_dell, HEAP.erf_p)->pag_w != (tal_w - 1)) )
  {
    fprintf(stderr, "pack: stale tail: setup failed\r\n");
    return 0;
  }

  return _pack_check("stale tail");
}

/* _test_pack_split_tail(): as above, but pack first frees a page elsewhere,
**   so the free-list tail is valid but not adjacent to the heap tail.
**
**   pages: [dir] [dell] [A: full-2] [met] [Q: B1's crag] [B1: 1 chunk]
**          [B2: 1 chunk] [gap: free] [tail: B2's crag]
*/
static c3_t
_test_pack_split_tail(void)
{
  static u3_post fil_p[PACK_LIVE], fiq_p[PACK_LIVE], fib_p[PACK_LIVE];
  const u3a_hunk_dose *lag_u, *met_u;
  u3p(u3a_crag)       *dir_u;
  u3_post  hol_p, hom_p, gap_p, spa_p, quo_p, cag_p, one_p, two_p;
  c3_w     fil_w, fiq_w, fib_w, tal_w;
  c3_g     lag_g, met_g;

  _pack_heap();

  if ( !_pack_class(&lag_g, &met_g) ) {
    fprintf(stderr, "pack: split tail: no suitable class\r\n");
    return 0;
  }

  lag_u = &(u3a_Hunk[lag_g]);
  met_u = &(u3a_Hunk[met_g]);

  (void)_pack_live(c3_wiseof(u3a_dell));

  (void)_pack_live(lag_u->len_s);
  fil_w = _pack_fill(lag_g, fil_p);
  (void)_pack_fill(met_g, 0);

  spa_p = _imalloc(met_u->len_s);       //  opens metadata page Q
  quo_p = page_to_post(post_to_page(spa_p));
  hol_p = _pack_page();                 //  future page B1
  hom_p = _pack_page();                 //  future page B2

  _ifree(hol_p);
  one_p = _pack_live(lag_u->len_s);     //  page B1, crag on Q
  _ifree(spa_p);
  fiq_w = _pack_fill(met_g, fiq_p);     //  Q full
  fib_w = _pack_fill(lag_g, fib_p);     //  B1 full

  gap_p = _pack_page();                 //  future free page
  spa_p = _imalloc(met_u->len_s);       //  opens tail metadata page
  cag_p = page_to_post(post_to_page(spa_p));

  _ifree(hom_p);
  two_p = _pack_live(lag_u->len_s);     //  page B2, crag on tail page
  _ifree(spa_p);

  for ( c3_w i_w = 0; i_w < fiq_w; i_w++ ) {
    _pack_drop(fiq_p[i_w]);
  }

  for ( c3_w i_w = 0; i_w < fib_w; i_w++ ) {
    _pack_drop(fib_p[i_w]);
  }

  _ifree(gap_p);

  //  B1's and B2's chunks now fit on page A
  //
  _pack_drop(fil_p[fil_w - 1]);
  _pack_drop(fil_p[fil_w - 2]);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  tal_w = HEAP.len_w - 1;

  if (  (post_to_page(cag_p) != tal_w)
     || (post_to_page(gap_p) != (tal_w - 1))
     || ((post_to_page(quo_p) + 2) >= tal_w)
     || (post_to_page(one_p) != post_to_page(hol_p))
     || (post_to_page(two_p) != post_to_page(hom_p))
     || (u3a_free_pg != dir_u[tal_w - 1])
     || (u3to(u3a_crag, dir_u[tal_w])->fre_s != (met_u->ful_s - 1))
     || (u3to(u3a_crag, dir_u[post_to_page(quo_p)])->fre_s != (met_u->ful_s - 1))
     || !HEAP.erf_p
     || (u3to(u3a_dell, HEAP.erf_p)->pag_w != (tal_w - 1)) )
  {
    fprintf(stderr, "pack: split tail: setup failed\r\n");
    return 0;
  }

  return _pack_check("split tail");
}

/* _pack_dell_page(): page holding the dell for [som_p], or 0.
*/
static c3_w
_pack_dell_page(u3_post som_p)
{
  return post_to_page(som_p);
}

/* _test_pack_dell_page(): pack's free-list drain empties a page of dells,
**   so _free_pages lists a new entry while the drain is running.
**
**   pages: [dir] [dells: E1 E2] [free] [live] [free] [live]
*/
static c3_t
_test_pack_dell_page(void)
{
  u3p(u3a_crag) *dir_u;
  u3a_dell      *one_u, *two_u;
  u3_post        spa_p, hol_p, hom_p;
  c3_w           del_w;
  c3_g           del_g = (c3_g)c3_bits_word(c3_max(c3_wiseof(u3a_dell), u3a_minimum) - 1) - u3a_min_log;

  _pack_heap();

  //  drop the cached entry, so every dell comes from _imalloc()
  //
  _ifree(HEAP.cac_p);
  HEAP.cac_p = 0;

  spa_p = _imalloc(c3_wiseof(u3a_dell));  //  opens the dell page
  del_w = post_to_page(spa_p);

  hol_p = _pack_page();
  (void)_pack_live(((c3_w)1) << u3a_page);
  hom_p = _pack_page();
  (void)_pack_live(((c3_w)1) << u3a_page);

  _ifree(hol_p);
  _ifree(hom_p);
  _ifree(spa_p);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  one_u = u3tn(u3a_dell, HEAP.fre_p);
  two_u = one_u ? u3tn(u3a_dell, one_u->nex_p) : 0;

  if (  HEAP.cac_p
     || !two_u
     || two_u->nex_p
     || (HEAP.erf_p != one_u->nex_p)
     || (_pack_dell_page(HEAP.fre_p) != del_w)
     || (_pack_dell_page(one_u->nex_p) != del_w)
     || (u3to(u3a_crag, dir_u[del_w])->fre_s != (u3a_Hunk[del_g].ful_s - 2)) )
  {
    fprintf(stderr, "pack: dell page: setup failed\r\n");
    return 0;
  }

  return _pack_check("dell page");
}

/* _test_pack_tail_dell(): the last free-list entry is the only chunk on the
**   last heap page, and describes the page right before it.
**
**   pages: [dir] [live] [free] [dells: T]
*/
static c3_t
_test_pack_tail_dell(void)
{
  u3p(u3a_crag) *dir_u;
  u3a_dell      *tel_u;
  u3_post        spa_p, hol_p;
  c3_w           tal_w;
  c3_g           del_g = (c3_g)c3_bits_word(c3_max(c3_wiseof(u3a_dell), u3a_minimum) - 1) - u3a_min_log;

  _pack_heap();

  _ifree(HEAP.cac_p);
  HEAP.cac_p = 0;

  (void)_pack_live(((c3_w)1) << u3a_page);
  hol_p = _pack_page();
  spa_p = _imalloc(c3_wiseof(u3a_dell));  //  opens the tail dell page

  _ifree(hol_p);
  _ifree(spa_p);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  tal_w = HEAP.len_w - 1;
  tel_u = u3tn(u3a_dell, HEAP.erf_p);

  if (  HEAP.cac_p
     || !tel_u
     || (HEAP.fre_p != HEAP.erf_p)
     || (_pack_dell_page(HEAP.erf_p) != tal_w)
     || (tel_u->pag_w != (tal_w - 1))
     || (tel_u->siz_w != 1)
     || (u3to(u3a_crag, dir_u[tal_w])->fre_s != (u3a_Hunk[del_g].ful_s - 1)) )
  {
    fprintf(stderr, "pack: tail dell: setup failed\r\n");
    return 0;
  }

  return _pack_check("tail dell");
}

/* _test_pack_move_drain(): after compaction, freeing a leftover dell empties
**   its page, so _free_pages lists that page during _pack_move's final drain.
**
**   pages: [dir] [dells: full] [A: full-2] [crags] [Q1: B1's crag]
**          [B1: 1 chunk] [B2: 1 chunk] [Q2: B2's crag] [live]
**
**   during pack, Q1 empties and takes the cached dell; Q2 empties and needs
**   a new dell page, which takes Q1's page off the free list. that page
**   then holds only Q2's dell, which _pack_move frees at the end.
*/
static c3_t
_test_pack_move_drain(void)
{
  static u3_post fil_p[PACK_LIVE], fiq_p[PACK_LIVE], fib_p[PACK_LIVE];
  const u3a_hunk_dose *lag_u, *met_u;
  u3p(u3a_crag)       *dir_u;
  u3_post  hol_p, hom_p, spa_p, quo_p, qua_p, one_p, two_p, liv_p;
  c3_w     fil_w, fiq_w, fib_w;
  c3_g     lag_g, met_g;
  c3_g     del_g = (c3_g)c3_bits_word(c3_max(c3_wiseof(u3a_dell), u3a_minimum) - 1) - u3a_min_log;

  _pack_heap();

  if ( !_pack_class(&lag_g, &met_g) || (del_g == lag_g) ) {
    fprintf(stderr, "pack: move drain: no suitable class\r\n");
    return 0;
  }

  lag_u = &(u3a_Hunk[lag_g]);
  met_u = &(u3a_Hunk[met_g]);

  //  dell page full (including the cached dell), so a new dell needs a page
  //
  (void)_pack_fill(del_g, 0);

  (void)_pack_live(lag_u->len_s);
  fil_w = _pack_fill(lag_g, fil_p);     //  page A full
  (void)_pack_fill(met_g, 0);           //  its crag page full

  spa_p = _imalloc(met_u->len_s);       //  opens crag page Q1
  quo_p = page_to_post(post_to_page(spa_p));
  hol_p = _pack_page();                 //  future page B1
  hom_p = _pack_page();                 //  future page B2

  _ifree(hol_p);
  one_p = _pack_live(lag_u->len_s);     //  page B1, crag on Q1
  _ifree(spa_p);
  fiq_w = _pack_fill(met_g, fiq_p);     //  Q1 full
  fib_w = _pack_fill(lag_g, fib_p);     //  B1 full

  spa_p = _imalloc(met_u->len_s);       //  opens crag page Q2
  qua_p = page_to_post(post_to_page(spa_p));

  _ifree(hom_p);
  two_p = _pack_live(lag_u->len_s);     //  page B2, crag on Q2
  _ifree(spa_p);

  liv_p = _pack_live(((c3_w)1) << u3a_page);  //  keeps the dell page off the tail

  for ( c3_w i_w = 0; i_w < fiq_w; i_w++ ) {
    _pack_drop(fiq_p[i_w]);
  }

  for ( c3_w i_w = 0; i_w < fib_w; i_w++ ) {
    _pack_drop(fib_p[i_w]);
  }

  //  B1's and B2's chunks now fit on page A
  //
  _pack_drop(fil_p[fil_w - 1]);
  _pack_drop(fil_p[fil_w - 2]);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);

  if (  HEAP.fre_p
     || !HEAP.cac_p
     || HEAP.wee_p[del_g]
     || (post_to_page(one_p) != post_to_page(hol_p))
     || (post_to_page(two_p) != post_to_page(hom_p))
     || ((post_to_page(qua_p) + 1) != post_to_page(liv_p))
     || (post_to_page(liv_p) != (HEAP.len_w - 1))
     || (u3to(u3a_crag, dir_u[post_to_page(quo_p)])->fre_s != (met_u->ful_s - 1))
     || (u3to(u3a_crag, dir_u[post_to_page(qua_p)])->fre_s != (met_u->ful_s - 1)) )
  {
    fprintf(stderr, "pack: move drain: setup failed\r\n");
    return 0;
  }

  return _pack_check("move drain");
}

/* _test_pack_fork(): run a pack test in a child, surviving assertions.
*/
static c3_t
_test_pack_fork(c3_t (*fun_f)(void), const c3_c* nam_c)
{
  pid_t pid_i;
  c3_i  sat_i;

  fflush(stderr);

  if ( 0 == (pid_i = fork()) ) {
    exit( fun_f() ? 0 : 1 );
  }

  if ( (pid_i < 0) || (pid_i != waitpid(pid_i, &sat_i, 0)) ) {
    fprintf(stderr, "pack: %s: fork failed\r\n", nam_c);
    return 0;
  }

  if ( WIFEXITED(sat_i) && !WEXITSTATUS(sat_i) ) {
    fprintf(stderr, "pack: %s: ok\r\n", nam_c);
    return 1;
  }

  fprintf(stderr, "pack: %s: FAILED\r\n", nam_c);
  return 0;
}

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

  _test_palloc();
  fprintf(stderr, "palloc okeedokee\n\n");

#ifdef VERE64
  _test_palloc_d();
  fprintf(stderr, "palloc_d okeedokee\n");
#endif

  {
    c3_t ok_t = 1;
    ok_t &= _test_pack_fork(_test_pack_stale_tail, "stale tail");
    ok_t &= _test_pack_fork(_test_pack_split_tail, "split tail");
    ok_t &= _test_pack_fork(_test_pack_dell_page, "dell page");
    ok_t &= _test_pack_fork(_test_pack_tail_dell, "tail dell");
    ok_t &= _test_pack_fork(_test_pack_move_drain, "move drain");

    if ( !ok_t ) {
      exit(1);
    }
  }

  return 0;
}
