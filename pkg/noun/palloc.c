
#include "c3/c3.h"
#include "allocate.h"

#define u3a_min_log  2

STATIC_ASSERT( (1U << u3a_min_log) == u3a_minimum,
               "log2 minimum allocation" );

#define u3a_free_pg  (u3p(u3a_crag))0
#define u3a_head_pg  (u3p(u3a_crag))1
#define u3a_rest_pg  (u3p(u3a_crag))2

// #define u3a_crag_no  (u3a_page - u3a_min_log)

typedef struct _u3a_crag {
  u3p(struct _u3a_crag) nex_p;     //  next
  c3_w                  pag_w;     //  page index
  c3_s                  len_s;     //  chunk word-size
  c3_s                  log_s;     //  size log2
  c3_s                  fre_s;     //  free chunks
  c3_s                  tot_s;     //  total chunks
  c3_w                  map_w[1];  //  free-chunk bitmap
} u3a_crag;

typedef struct _u3a_dell {
  u3p(struct _u3a_dell) nex_p;     //  next
  u3p(struct _u3a_dell) pre_p;     //  prev
  c3_w                  pag_w;     //  page index
  c3_w                  siz_w;     //  number of pages
} u3a_dell;

struct heap {
  u3p(u3a_dell)  fre_p;               //  free list
  u3p(u3a_dell)  cac_p;               //  cached pgfree struct
  u3_post        bot_p;               //  XX s/b rut_p
  c3_ws          dir_ws;              //  1 || -1 (multiplicand for local offsets)
  c3_ws          off_ws;              //  0 || -1 (word-offset for hat && rut)
  c3_w           siz_w;               //  directory size
  c3_w           len_w;               //  directory entries
  u3p(u3a_crag*) pag_p;               //  directory
  u3p(u3a_crag)  wee_p[u3a_crag_no];  //  chunk lists
};

#define page_to_post(pag_w)  (HEAP.bot_p + (((HEAP.dir_ws * (pag_w)) + HEAP.off_ws) << u3a_page))
#define post_to_page(som_p)  (_abs_dif(som_p, HEAP.bot_p + HEAP.off_ws) >> u3a_page)


#ifndef HEAP
  #define HEAP u3R->hep
#endif

static u3_post _imalloc(c3_w);
static void  _ifree(u3_post);

static __inline__ c3_w
_abs_dif(c3_w a_w, c3_w b_w)
{
  // c3_ds dif_ds = a_w - b_w;
  // c3_d  mas_d  = dif_ds >> 63;
  // return (dif_ds + mas_d) ^ mas_d;
  return (a_w > b_w ) ? a_w - b_w : b_w - a_w;
}

static void
_init(void)
{
  memset(&(HEAP), 0x0, sizeof(HEAP)); //  XX s/b unnecessary
  u3p(u3a_crag) *dir_u;

  //  align hat
  //
  //   XX s/b done elsewhere, along with rut_p
  //
  if ( c3y == u3a_is_north(u3R) ) {
    // fprintf(stderr, "palloc: init north: hat=0x%x cap=0x%x\n", u3R->hat_p, u3R->cap_p);
    u3R->hat_p += (1U << u3a_page) - 1;
    HEAP.dir_ws = 1;
    HEAP.off_ws = 0;
  }
  else {
    // fprintf(stderr, "palloc: init south: hat=0x%x cap=0x%x\n", u3R->hat_p, u3R->cap_p);
    HEAP.dir_ws = -1;
    HEAP.off_ws = -1;
  }

  u3R->hat_p &= ~((1U << u3a_page) - 1);
  HEAP.bot_p = u3R->hat_p;

  // fprintf(stderr, "palloc: init1 hat=0x%x cap=0x%x bot=0x%x\n", u3R->hat_p, u3R->cap_p, HEAP.bot_p);

  assert( u3R->hat_p > u3a_rest_pg );

  //  XX check for overflow

  HEAP.pag_p  = u3R->hat_p;
  HEAP.pag_p += HEAP.off_ws * (1U << u3a_page);
  HEAP.siz_w  = 1U << u3a_page;
  HEAP.len_w  = 1;

  u3R->hat_p += HEAP.dir_ws * (1U << u3a_page);

  // fprintf(stderr, "palloc: init2 hat=0x%x cap=0x%x bot=0x%x pag=0x%x\n", u3R->hat_p, u3R->cap_p, HEAP.bot_p, HEAP.pag_p);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  dir_u[0] = u3a_head_pg;

  assert( 0 == post_to_page(HEAP.pag_p) );
  assert( HEAP.pag_p == page_to_post(0) );

  // fprintf(stderr, "palloc: init3 hat=0x%x cap=0x%x bot=0x%x pag=0x%x dir=%p out-adir=%x\n",
  //                 u3R->hat_p, u3R->cap_p, HEAP.bot_p, HEAP.pag_p, (void*)dir_u, (c3_w)u3a_outa(dir_u));

  HEAP.cac_p = _imalloc(c3_wiseof(u3a_dell));

  // fprintf(stderr, "palloc: init4 hat=0x%x cap=0x%x bot=0x%x pag=0x%x dir=%p cac=0x%x\n",
  //                 u3R->hat_p, u3R->cap_p, HEAP.bot_p, HEAP.pag_p, (void*)dir_u, HEAP.cac_p);
}

static void
_extend_directory(c3_w siz_w)  // num pages
{
  u3p(u3a_crag) *dir_u, *old_u;
  u3_post old_p = HEAP.pag_p;
  c3_w nex_w, pag_w;

  old_u  = u3to(u3p(u3a_crag), HEAP.pag_p);
  nex_w  = HEAP.len_w + siz_w + (1U << u3a_page) - 1;
  nex_w &= ~((1U << u3a_page) - 1);

  HEAP.pag_p  = u3R->hat_p;
  HEAP.pag_p += HEAP.off_ws * (1U << u3a_page);
  u3R->hat_p  += HEAP.dir_ws * nex_w; //  XX overflow

  if ( 1 == HEAP.dir_ws ) {
    if ( u3R->hat_p >= u3R->cap_p ) {
      fprintf(stderr, "\033[31mpalloc: directory overflow\n\033[0m");
      abort();
    }
  }
  else {
    if ( u3R->hat_p <= u3R->cap_p ) {
      fprintf(stderr, "\033[31mpalloc: directory overflow\n\033[0m");
      abort();
    }
  }

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  pag_w = post_to_page(HEAP.pag_p);

  dir_u[pag_w] = u3a_head_pg;

  {
    c3_w max_w = nex_w >> u3a_page;
    for ( c3_w i_w = 1; i_w < max_w; i_w++ ) {
      dir_u[pag_w + i_w] = u3a_rest_pg;
    }
  }

  memcpy(dir_u, old_u, (c3_z)HEAP.len_w << 2);

  HEAP.len_w += (nex_w >> u3a_page);
  HEAP.siz_w  = nex_w;

  _ifree(old_p);
}

static u3_post
_extend_heap(c3_w siz_w)  // num pages
{
  u3_post pag_p;

  assert( HEAP.siz_w >= HEAP.len_w );

  if ( (HEAP.siz_w - HEAP.len_w) < siz_w ) {
    _extend_directory(siz_w);
  }

  pag_p  = u3R->hat_p;
  pag_p += HEAP.off_ws * (1U << u3a_page);

  u3R->hat_p += HEAP.dir_ws * (siz_w << u3a_page);

  //  XX bail, optimize
  if ( 1 == HEAP.dir_ws ) {
    if ( u3R->hat_p >= u3R->cap_p ) {
      fprintf(stderr, "\033[31mpalloc: heap overflow\n\033[0m");
      abort();
    }
  }
  else {
    if ( u3R->hat_p <= u3R->cap_p ) {
      fprintf(stderr, "\033[31mpalloc: heap overflow\n\033[0m");
      abort();
    }
  }

  HEAP.len_w += siz_w;

  return pag_p;
}

static u3_post
_alloc_pages(c3_w siz_w)  // num pages
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3a_dell*      fre_u = u3tn(u3a_dell, HEAP.fre_p);
  u3a_dell*      del_u = NULL;
  c3_w           pag_w = 0;

  while ( fre_u ) {
    //  XX sanity

    if ( fre_u->siz_w < siz_w ) {
      fre_u = u3tn(u3a_dell, fre_u->nex_p);
      continue;
    }
    else if ( fre_u->siz_w == siz_w ) {
      pag_w = fre_u->pag_w;
      if ( fre_u->nex_p ) {
        u3to(u3a_dell, fre_u->nex_p)->pre_p = fre_u->pre_p;
      }
      if ( fre_u->pre_p ) {
        u3to(u3a_dell, fre_u->pre_p)->nex_p = fre_u->nex_p;
      }
      else {
        HEAP.fre_p = fre_u->nex_p;
      }
      del_u = fre_u;
      // fprintf(stderr, "alloc pages take %u at %u from 0x%x\n", siz_w,
      //                 pag_w, (c3_w)u3of(u3a_dell, fre_u));
    }
    else {
      pag_w = fre_u->pag_w;
      fre_u->pag_w += siz_w;
      fre_u->siz_w -= siz_w;

      // fprintf(stderr, "alloc pages split %u at %u (%u remaining) from 0x%x\n",
      //                 siz_w, pag_w, fre_u->siz_w, (c3_w)u3of(u3a_dell, fre_u));
    }
    break;
  }

  u3_post pag_p;

  if ( pag_w ) {
    //  XX sanity
    assert( u3a_free_pg == dir_u[pag_w] );
    for ( c3_w i_w = 1; i_w < siz_w; i_w++ ) {
      assert( u3a_free_pg == dir_u[pag_w + i_w] );
    }

    pag_p = page_to_post(pag_w);
  }
  else {
    pag_p = _extend_heap(siz_w);
    pag_w = post_to_page(pag_p);
    dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
    // fprintf(stderr, "alloc pages grow 0x%x\n", pag_p);
  }

  dir_u[pag_w] = u3a_head_pg;

  for ( c3_w i_w = 1; i_w < siz_w; i_w++ ) {
    dir_u[pag_w + i_w] = u3a_rest_pg;
  }

  //  XX junk

  if ( del_u ) {
    if ( !HEAP.cac_p ) {
      HEAP.cac_p = u3of(u3a_dell, del_u);
    }
    else {
      _ifree(u3of(u3a_dell, del_u));
    }
  }

  return pag_p;
}

static u3_post
_make_chunks(c3_g bit_g)  // 0-9, inclusive
{
  u3_post pag_p = _alloc_pages(1);
  c3_w    pag_w = post_to_page(pag_p);
  c3_s    log_s = bit_g + u3a_min_log;
  c3_s    len_s = 1U << log_s;
  c3_s    tot_s = 1U << (u3a_page - log_s);  // 2-1.024, inclusive
  c3_s    siz_s = c3_wiseof(u3a_crag);
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3a_crag *pag_u;
  u3_post hun_p;

  siz_s += tot_s >> 5;
  siz_s += !!(tot_s & 31);
  siz_s--;

  //  metacircular base case
  //
  //    trivially deducible from exhaustive enumeration
  //
  if ( len_s <= (siz_s << 1) ) {
    hun_p = pag_p;
  }
  else {
    hun_p = _imalloc(siz_s);
  }

  // fprintf(stderr, "make chunks: pag_p: %x, pag_w %u\n", pag_p, pag_w);

  pag_u = u3to(u3a_crag, hun_p);
  pag_u->pag_w = pag_w;
  pag_u->log_s = log_s;
  pag_u->len_s = len_s;
  pag_u->tot_s = pag_u->fre_s = tot_s;

  {
    c3_w *map_w = pag_u->map_w;
    c3_w  len_w = tot_s >> 5;

    while ( len_w-- ) {
      *map_w++ = ~0;
    }

    len_w = tot_s & 31;

    if ( len_w ) {
      *map_w = (c3_w)~0 >> (32 - len_w);
    }


    //  reserve chunks stolen for pginfo
    //
    if ( len_s <= (siz_s << 1) ) {
      len_w = 1U + ((siz_s - 1) / len_s);

      //  XX pag_u->map_w[0] &= ~0 << len_w

      for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
        pag_u->map_w[i_w >> 5] &= ~(1U << (i_w & 31));
      }

      pag_u->fre_s -= len_w;
      pag_u->tot_s -= len_w;
    }
  }

  dir_u[pag_w] = hun_p;
  pag_u->nex_p = HEAP.wee_p[bit_g];

  // fprintf(stderr, "store wee_p %u 0x%x\n", bit_g, hun_p);
  HEAP.wee_p[bit_g] = hun_p;

  return hun_p;
}

static u3_post
_alloc_words(c3_w len_w)  //  4-2.048, inclusive
{
  c3_g      bit_g = (c3_g)c3_bits_word(len_w - 1) - u3a_min_log;  // 0-9, inclusive
  u3_post   pag_p = HEAP.wee_p[bit_g];
  u3a_crag *pag_u;
  c3_w     *map_w;
  c3_g      pos_g;

  if ( !pag_p ) {
    pag_p = _make_chunks(bit_g);
  }
  else {
    //  XX sanity
  }

  pag_u = u3to(u3a_crag, pag_p);
  map_w = pag_u->map_w;

  assert( pag_u->log_s < u3a_page );

  // fprintf(stderr, "page: 0x%x bit=%u pag=%u len=%u log=%u fre=%u tot=%u\n",
  //                 pag_p, bit_g, pag_u->pag_w, pag_u->len_s, pag_u->log_s, pag_u->fre_s, pag_u->tot_s);
  // fprintf(stderr, "page: 1 map=%p map[0]=%x\n", (void*)map_w, *map_w);

  while ( !*map_w ) { map_w++; }
  // fprintf(stderr, "page: 2 map=%p map[0]=%x\n", (void*)map_w, *map_w);

  pos_g   = c3_tz_w(*map_w);
  *map_w &= ~(1U << pos_g);

  // fprintf(stderr, "page: 3 map=%p map[0]=%x\n", (void*)map_w, *map_w);

  if ( !--pag_u->fre_s ) {
    // fprintf(stderr, "store wee_p %u 0x%x\n", bit_g, pag_u->nex_p);
    HEAP.wee_p[bit_g] = pag_u->nex_p;
    pag_u->nex_p = 0;
  }

  {
    c3_w off_w = map_w - pag_u->map_w;  //  bitmap words
    off_w <<= 5;                        //    (in bits)
    off_w  += pos_g;                    //  chunk index
    off_w <<= pag_u->log_s;             //    (in words)

    u3_post out_p = page_to_post(pag_u->pag_w) + off_w;

    // fprintf(stderr, "alloc_bytes: pos_g: %u, pag_p: %x, off_w %x, log %u\n",
    //                 pos_g, pag_p, off_w, pag_u->log_s);

    return out_p;
  }
}

static u3_post
_imalloc(c3_w len_w)
{
  if ( len_w > (1U << (u3a_page - 1)) ) {
    len_w  += (1U << u3a_page) - 1;
    len_w >>= u3a_page;
    return _alloc_pages(len_w);
  }

  return _alloc_words(c3_max(len_w, u3a_minimum));
}

static void
_free_pages(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3a_dell *fre_u = u3tn(u3a_dell, HEAP.fre_p);
  u3a_dell *cac_u, *del_u = NULL;
  c3_w siz_w, nex_w;

  if ( u3a_free_pg == dir_p ) {
    //  XX double free
    fprintf(stderr, "\033[31m"
                    "palloc: double free page som_p=0x%x pag_w=%u\n"
                    "\033[0m",
                    som_p, pag_w);
    return; // XX bail
  }

  if ( u3a_head_pg != dir_p ) {
    //  XX pointer to wrong page
    fprintf(stderr, "\033[31m"
                    "palloc: wrong page som_p=0x%x dir_p=0x%x\n"
                    "\033[0m",
                    som_p, dir_p);
    return; // XX bail
  }

  if ( som_p & ((1U << u3a_page) - 1) ) {
    //  XX pointer not aligned to page
    fprintf(stderr, "\033[31m"
                    "palloc: bad page alignment som_p=0x%x\n"
                    "\033[0m",
                    som_p);
    return; // XX bail
  }

  dir_u[pag_w] = u3a_free_pg;

  for ( siz_w = 1; dir_u[pag_w + siz_w] == u3a_rest_pg; siz_w++ ) {
    dir_u[pag_w + siz_w] = u3a_free_pg;
  }

  nex_w = pag_w + siz_w;

  //  XX madv_free

  if ( !HEAP.cac_p ) {
    HEAP.cac_p = _imalloc(c3_wiseof(*cac_u));
  }

  cac_u = u3to(u3a_dell, HEAP.cac_p);
  cac_u->pag_w = pag_w;
  cac_u->siz_w = siz_w;

  if ( !fre_u ) {
    // fprintf(stderr, "free pages 0x%x (%u) via 0x%x\n", som_p, siz_w, HEAP.cac_p);
    cac_u->nex_p = 0;
    cac_u->pre_p = 0;
    fre_u = cac_u;
    HEAP.fre_p = HEAP.cac_p;
    HEAP.cac_p = 0;
  }
  else {
    c3_w fex_w;

    while (  ((fex_w = fre_u->pag_w + fre_u->siz_w) < pag_w)
          && fre_u->nex_p )
    {
      fre_u = u3to(u3a_dell, fre_u->nex_p);
    }

    if ( fre_u->pag_w > nex_w ) {        //  insert before
      // fprintf(stderr, "free pages before 0x%x (%u) via 0x%x\n", som_p, siz_w, HEAP.cac_p);

      cac_u->nex_p = u3of(u3a_dell, fre_u);
      cac_u->pre_p = fre_u->pre_p;

      fre_u->pre_p = HEAP.cac_p;

      //  XX sanity
      if ( cac_u->pre_p ) {
        u3to(u3a_dell, cac_u->pre_p)->nex_p = HEAP.cac_p;
      }
      else {
        HEAP.fre_p = HEAP.cac_p;
      }

      fre_u = cac_u;
      HEAP.cac_p = 0;
    }
    else if ( fex_w == pag_w ) {  //  append to entry
      fre_u->siz_w += siz_w;

      // fprintf(stderr, "free pages append %u at %u to 0x%x\n",
      //                 siz_w, pag_w, (c3_w)u3of(u3a_dell, fre_u));

      //  coalesce with next entry
      //
      if (  fre_u->nex_p
         && (fex_w == u3to(u3a_dell, fre_u->nex_p)->pag_w) )
      {
        // fprintf(stderr, "free pages coalesce %u\n", del_u->siz_w);
        del_u = u3to(u3a_dell, fre_u->nex_p);
        fre_u->siz_w += del_u->siz_w;
        fre_u->nex_p  = del_u->nex_p;

        //  XX sanity
        if ( del_u->nex_p ) {
          u3to(u3a_dell, del_u->nex_p)->pre_p = u3of(u3a_dell, fre_u);
        }
      }
    }
    else if ( fre_u->pag_w == nex_w ) {  //  prepend to entry
      fre_u->siz_w += siz_w;
      fre_u->pag_w  = pag_w;

      // fprintf(stderr, "free pages prepend %u at %u to 0x%x\n",
      //                 siz_w, pag_w, (c3_w)u3of(u3a_dell, fre_u));
    }
    else if ( !fre_u->nex_p ) {          //  insert after
      // fprintf(stderr, "free pages before %u at %u via 0x%x\n",
      //                 siz_w, pag_w, HEAP.cac_p);
      cac_u->nex_p = 0;
      cac_u->pre_p = u3of(u3a_dell, fre_u);
      fre_u->nex_p = HEAP.cac_p;
      fre_u = cac_u;
      HEAP.cac_p = 0;
    }
    else {
      // XX hosed
      fprintf(stderr, "\033[31m"
                    "palloc: free list hosed at som_p=0x%x pag=%u len=%u\n"
                    "\033[0m",
                    (u3_post)u3of(u3a_dell, fre_u), fre_u->pag_w, fre_u->siz_w);
      abort();
    }
  }

  //  XX shrink heap

  if ( del_u ) {
    _ifree(u3of(u3a_dell, del_u));
  }
}

static void
_free_words(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  u3a_crag *pag_u = u3to(u3a_crag, dir_p);
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);

  assert( page_to_post(pag_u->pag_w) == (som_p & ~((1U << u3a_page) - 1)) );
  assert( pag_u->log_s < u3a_page );

  c3_g bit_g = pag_u->log_s - u3a_min_log;
  c3_w pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;

  if ( som_p & (pag_u->len_s - 1) ) {  //  XX just 1U << log_s and remove?
    //  XX  bad alignment
    fprintf(stderr, "\033[31m"
                    "palloc: bad alignment som_p=0x%x pag=0x%x len_s=%u\n"
                    "\033[0m",
                    som_p, dir_p, pag_u->len_s);
    return; // XX bail
  }

  if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
    //  XX double free
    fprintf(stderr, "\033[31m"
                    "palloc: double free som_p=0x%x pag=0x%x\n"
                    "\033[0m",
                    som_p, dir_p);
    return; // XX bail
  }

  pag_u->map_w[pos_w >> 5] |= (1U << (pos_w & 31));
  pag_u->fre_s++;

  {
    u3_post *bit_p = &(HEAP.wee_p[bit_g]);
    u3a_crag *bit_u, *nex_u;

    if ( 1 == pag_u->fre_s ) {
      //  page newly non-full, link
      //
      while ( *bit_p ) {
        bit_u = u3to(u3a_crag, *bit_p);
        nex_u = u3tn(u3a_crag, bit_u->nex_p);

        if ( nex_u && (nex_u->pag_w < pag_u->pag_w) ) {
          bit_p = &(bit_u->nex_p);
          continue;
        }

        break;
      }

      pag_u->nex_p = *bit_p;
      *bit_p = dir_p;
    }
    else if ( pag_u->fre_s == pag_u->tot_s ) {
      //  page now free
      //
      while ( *bit_p != dir_p ) {
        bit_u = u3to(u3a_crag, *bit_p);
        bit_p = &(bit_u->nex_p);

        //  XX sanity: must be in list
      }

      *bit_p = pag_u->nex_p;

      dir_u[pag_u->pag_w] = u3a_head_pg;
      som_p = page_to_post(pag_u->pag_w); // NB: clobbers
      if ( som_p != dir_p ) {
        _ifree(dir_p);
      }
      _ifree(som_p);
    }
  }
}

static void
_ifree(u3_post som_p)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w pag_w = post_to_page(som_p);

  if ( pag_w >= HEAP.len_w ) {
    fprintf(stderr, "\033[31m"
                    "palloc: page out of heap som_p=0x%x pag_w=%u\n"
                    "\033[0m",
                    som_p, pag_w);
    return; // XX bail
  }

  u3_post dir_p = dir_u[pag_w];

  if ( dir_p <= u3a_rest_pg ) {
    _free_pages(som_p, pag_w, dir_p);
  }
  else {
    _free_words(som_p, pag_w, dir_p);
  }
}

static u3_post
_irealloc(u3_post som_p, c3_w len_w)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w pag_w = post_to_page(som_p);
  c3_w old_w;

    if ( pag_w >= HEAP.len_w ) {
    fprintf(stderr, "\033[31m"
                    "palloc: realloc page out of heap som_p=0x%x pag_w=%u\n"
                    "\033[0m",
                    som_p, pag_w);
    return 0; // XX bail
  }

  u3_post dir_p = dir_u[pag_w];

  if ( u3a_head_pg == dir_p ) {
    if ( som_p & ((1U << u3a_page) - 1) ) {
      //  XX pointer not aligned to page
      fprintf(stderr, "\033[31m"
                      "palloc: realloc bad page alignment som_p=0x%x\n"
                      "\033[0m",
                      som_p);
      return 0; // XX bail
    }

    for ( old_w = 1; dir_u[pag_w + old_w] == u3a_rest_pg; old_w++ ) {}

    {
      c3_w wor_w = old_w << u3a_page;

      if ( len_w <= wor_w ) {
        wor_w  -= len_w;
        wor_w >>= u3a_page;

        // XX junk

        while ( wor_w-- ) {
          dir_u[pag_w + wor_w] = u3a_free_pg;
        }

        return som_p;
      }

      //  XX also grow in place if sufficient adjacent pages are free?
    }
  }
  else if ( u3a_rest_pg >= dir_p ) {
    //  XX pointer to wrong page
    fprintf(stderr, "\033[31m"
                    "palloc: realloc wrong page som_p=0x%x\n"
                    "\033[0m",
                    som_p);
    return 0; // XX bail
  }
  else {
    u3a_crag *pag_u = u3to(u3a_crag, dir_p);
    c3_w pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;

    if ( som_p & (pag_u->len_s - 1) ) {  //  XX just 1U << log_s and remove?
      //  XX  bad alignment
      fprintf(stderr, "\033[31m"
                      "palloc: realloc bad alignment som_p=0x%x pag=0x%x len_s=%u\n"
                      "\033[0m",
                      som_p, dir_p, pag_u->len_s);
      return 0; // XX bail
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      //  XX double free
      fprintf(stderr, "\033[31m"
                      "palloc: realloc free som_p=0x%x pag=0x%x\n"
                      "\033[0m",
                      som_p, dir_p);
      return 0; // XX bail
    }

    old_w = pag_u->len_s;

    if ( (old_w >= len_w) && (len_w > (old_w >> 1)) ) {
      //  XX junk
      return som_p;
    }
  }

  {
    u3_post new_p = _imalloc(len_w);

    memcpy(u3a_into(new_p), u3a_into(som_p), (c3_z)c3_min(len_w, old_w) << 2);
    _ifree(som_p);

    return new_p;
  }
}

// static void
// _ifree_ptr(void* som_v)
// {
//   if ( !som_v ) {
//     return;
//   }

//   //  XX sanity: in loom
//   _ifree(u3a_outa(som_v));
// }
