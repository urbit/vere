
#include "c3/c3.h"
#include "allocate.h"

#define LOG_MINIMUM  2
#define MINIMUM (1U << LOG_MINIMUM)
#define MAXIMUM (1U << (u3a_page - 1))

#define FREE    (u3p(struct pginfo))0
#define FIRST   (u3p(struct pginfo))1
#define FOLLOW  (u3p(struct pginfo))2
#define MAGIC   (u3p(struct pginfo))3

struct pginfo {
  u3p(struct pginfo) nex_p;  /* next on the free list */
  u3_post            pag_p;  //  post to page start
  c3_s               len_s;  /* word-size of this page's chunks */
  c3_s               log_s;  /* How far to shift for this size chunks */
  c3_s               fre_s;  /* How many free chunks */
  c3_s               tot_s;  /* How many chunk */
  c3_w               map_w[1]; /* Which chunks are free */
};

struct pgfree {
  u3p(struct pgfree) nex_p;  /* next run of free pages */
  u3p(struct pgfree) pre_p;  /* prev run of free pages */
  u3_post            pag_p;  //  post to page start */
  u3_post            end_p;  //  post to 1+page end
  c3_w               siz_w;  /* number of pages free */
};

struct heap {
  u3p(struct pgfree)  fre_p;  //  free list
  u3p(struct pgfree)  cac_p;  //  cached pgfree struct
  c3_w                siz_w;  //  directory size
  c3_w                len_w;  //  directory entries
  u3p(struct pginfo*) pag_p;  //  directory       //  WRONG XX reserve pages
  u3p(struct pginfo)  wee_p[u3a_page - LOG_MINIMUM];  // chunk lists
};

//  WRONG: XX offset post-values in pag_p to account for FREE|FIRST|FOLLOW
//
//  XX sign-invert south pages
//  XX offset global page index to road-scoped heap
//

#define page_to_post(pag_w)  ((pag_w) << u3a_page)
#define post_to_page(som_p)  ((som_p) >> u3a_page)

struct heap hep_u;

static u3_post _imalloc(c3_w);
static void  _ifree(u3_post);

static void
_init(void)
{
  u3p(struct pginfo) *dir_u;

  //  align hat
  if ( u3R->hat_p & ((1U << u3a_page) - 1) ) {
    u3R->hat_p += (1U << u3a_page) - (u3R->hat_p & ((1U << u3a_page) - 1));
  }

  //  XX assert hat >= MAGIC

  hep_u.pag_p = u3R->hat_p;  //  XX offset
  hep_u.siz_w = 1;
  hep_u.len_w = 1;

  u3R->hat_p += 1U << u3a_page;  // XX direction, overflow

  dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);
  dir_u[0] = FIRST;
  hep_u.cac_p = _imalloc(c3_wiseof(struct pgfree));
}

static void
_extend_directory(c3_w siz_w)  // num pages
{
  u3p(struct pginfo) *dir_u, *old_u;
  c3_w pag_w, nex_w;
  u3_post old_p = hep_u.pag_p;

  old_u = u3to(u3p(struct pginfo), hep_u.pag_p);
  nex_w = ((siz_w + hep_u.len_w) >> u3a_page) - hep_u.siz_w;

  hep_u.pag_p = u3R->hat_p;  //  XX offset

  u3R->hat_p += nex_w << u3a_page;  //  XX direction, overflow

  dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);

  pag_w = post_to_page(hep_u.pag_p);

  dir_u[pag_w] = FIRST;

  for ( c3_w i_w = 1; i_w < nex_w; i_w++ ) {
    dir_u[pag_w + i_w] = FOLLOW;
  }

  memcpy(dir_u, old_u, siz_w << (u3a_page + 2));
  _ifree(old_p);
}

static u3_post
_extend_heap(c3_w siz_w)  // num pages
{
  u3_post pag_p;

  if ( (siz_w + hep_u.len_w) > (hep_u.siz_w << u3a_page) ) {
    _extend_directory(siz_w);
  }

  pag_p = u3R->hat_p;  //  XX offset

  u3R->hat_p += siz_w << u3a_page;  //  XX direction overflow

  return pag_p;
}

static u3_post
_alloc_pages(c3_w len_w)
{
  u3p(struct pginfo) *dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);
  c3_w    pag_w;
  u3_post pag_p = 0;
  c3_w    siz_w = (len_w + (1U << u3a_page) - 1) >> u3a_page;
  struct pgfree* del_u = NULL;
  struct pgfree* fre_u = ( hep_u.fre_p )
                       ? u3to(struct pgfree, hep_u.fre_p)
                       : NULL;

  while ( fre_u ) {
    //  XX sanity

    if ( fre_u->siz_w < siz_w ) {
      continue;
    }
    else if ( fre_u->siz_w == siz_w ) {
      pag_p = fre_u->pag_p;
      if ( fre_u->nex_p ) {
        u3to(struct pgfree, fre_u->nex_p)->pre_p = fre_u->pre_p;
      }
      if ( fre_u->pre_p ) {
        u3to(struct pgfree, fre_u->pre_p)->nex_p = fre_u->nex_p;
      }
      del_u = fre_u;
    }
    else {
      pag_p = fre_u->pag_p;
      fre_u->pag_p += siz_w;
      fre_u->siz_w -= siz_w;
    }
    break;
  }

  if ( pag_p ) {
    //  XX sanity
    // dir_u[pag_w] == FREE;
  }

  if ( !pag_p ) {
    pag_p = _extend_heap(siz_w);
  }

  pag_w = post_to_page(pag_p);

  dir_u[pag_w] = FIRST;

  for ( c3_w i_w = 1; i_w < siz_w; i_w++ ) {
    dir_u[pag_w + i_w] = FOLLOW;
  }

  //  XX junk

  if ( del_u ) {
    if ( !hep_u.cac_p ) {
      hep_u.cac_p = u3of(struct pgfree, del_u);
    }
    else {
      _ifree(u3of(struct pgfree, del_u));
    }
  }

  return pag_p;
}

static u3_post
_make_chunks(c3_g bit_g)  // 0-9, inclusive
{
  struct pginfo *pag_u;
  u3_post pag_p = _alloc_pages(1U << u3a_page);
  c3_w    pag_w = post_to_page(pag_p);
  c3_s    log_s = bit_g + LOG_MINIMUM;
  c3_s    len_s = 1U << log_s;
  c3_s    tot_s = 1U << (u3a_page - log_s);  // 2-1.024, inclusive
  c3_s    siz_s = c3_wiseof(struct pginfo);

  siz_s += tot_s >> 5;
  siz_s += !!(tot_s & 31);
  siz_s--;

  //  metacircular base case
  //
  //    trivially deducible from exhaustive enumeration
  //
  if ( len_s <= (siz_s << 1) ) {
    pag_u = u3to(struct pginfo, pag_p);
  }
  else {
    pag_u = u3to(struct pginfo, _imalloc(siz_s));
  }

  pag_u->pag_p = pag_p;
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

  {
    u3p(struct pginfo) *dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);
    u3_post hun_p = u3a_outa(pag_u);

    dir_u[pag_w] = hun_p;
    pag_u->nex_p = hep_u.wee_p[bit_g];
    hep_u.wee_p[bit_g] = hun_p;

    return hun_p;
  }
}

static u3_post
_alloc_words(c3_w len_w)  //  4-2.048, inclusive
{
  struct pginfo *pag_u;
  c3_w          *map_w;
  c3_g    pos_g, bit_g = (c3_g)c3_bits_word(len_w - 1) - LOG_MINIMUM;  // 0-9, inclusive
  u3_post        pag_p = hep_u.wee_p[bit_g];

  if ( !pag_p ) {
    pag_p = _make_chunks(bit_g);
  }
  else {
    //  XX sanity
  }

  pag_u = u3to(struct pginfo, pag_p);
  map_w = pag_u->map_w;

  // fprintf(stderr, "page: pag=%x len=%u log=%u fre=%u tot=%u\n",
  //                 pag_u->pag_p, pag_u->len_s, pag_u->log_s, pag_u->fre_s, pag_u->tot_s);
  // fprintf(stderr, "page: 1 map=%p map[0]=%x\n", (void*)map_w, *map_w);

  while ( !*map_w ) { map_w++; }
  // fprintf(stderr, "page: 2 map=%p map[0]=%x\n", (void*)map_w, *map_w);

  pos_g   = c3_tz_w(*map_w);
  *map_w &= ~(1U << pos_g);

  // fprintf(stderr, "page: 3 map=%p map[0]=%x\n", (void*)map_w, *map_w);

  if ( !--pag_u->fre_s ) {
    hep_u.wee_p[bit_g] = pag_u->nex_p;
    pag_u->nex_p = 0;
  }

  {
    c3_w off_w = map_w - pag_u->map_w;
    off_w <<= 5;
    off_w  += pos_g;
    off_w <<= pag_u->log_s;

    // fprintf(stderr, "alloc_bytes: pos_g: %u, pag_p: %x, off_w %x, log %u\n", pos_g, pag_p, off_w, pag_u->log_s);
    return pag_p + off_w;
  }
}

static u3_post
_imalloc(c3_w len_w)
{
  if ( len_w > MAXIMUM ) {
    return _alloc_pages(len_w);
  }
  else if ( len_w < MINIMUM ) {
    len_w = MINIMUM;
  }

  return _alloc_words(len_w);
}

static void
_free_pages(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  u3p(struct pginfo) *dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);
  struct pgfree* cac_u;
  struct pgfree *del_u = NULL;
  struct pgfree* fre_u = ( hep_u.fre_p )
                       ? u3to(struct pgfree, hep_u.fre_p)
                       : NULL;

  u3_post tal_p;
  c3_w    siz_w;

  if ( FREE == dir_p ) {
    //  XX double free
    return;
  }

  if ( FIRST != dir_p ) {
    //  XX pointer to wrong page
  }

  if ( som_p & ((1U << u3a_page) - 1) ) {
    //  XX pointer not aligned to page
  }

  dir_u[pag_w] = FREE;

  for ( siz_w = 0; dir_u[pag_w + siz_w] == FOLLOW; siz_w++ ) {
    dir_u[pag_w + siz_w] = FREE;
  }

  //  XX madv_free

  tal_p = som_p + (siz_w << u3a_page);

  if ( !hep_u.cac_p ) {
    hep_u.cac_p = _imalloc(c3_wiseof(*cac_u));
  }

  cac_u = u3to(struct pgfree, hep_u.cac_p);

  cac_u->pag_p = som_p;
  cac_u->end_p = tal_p;
  cac_u->siz_w = siz_w;

  if ( !fre_u ) {
    cac_u->nex_p = 0;
    cac_u->pre_p = 0;
    fre_u = cac_u;
    hep_u.fre_p = u3of(struct pgfree, fre_u);
    hep_u.cac_p = 0;
  }
  else {
    //  XX road direction
    while ( (fre_u->end_p < som_p) && fre_u->nex_p ) {
      fre_u = u3to(struct pgfree, fre_u->nex_p);
    }

    if ( fre_u->pag_p > tal_p ) {        //  insert before
      cac_u->nex_p = u3of(struct pgfree, fre_u);
      cac_u->pre_p = fre_u->pre_p;

      //  XX sanity
      if ( fre_u->pre_p ) {
        u3to(struct pgfree, fre_u->pre_p)->nex_p = u3of(struct pgfree, cac_u);
      }

      fre_u = cac_u;
      hep_u.cac_p = 0;
    }
    else if ( fre_u->end_p == som_p ) {  //  append to entry
      fre_u->end_p  = tal_p;
      fre_u->siz_w += siz_w;

      //  coalesce with next entry
      //
      if (  fre_u->nex_p
         && (fre_u->end_p == u3to(struct pgfree, fre_u->nex_p)->pag_p) )
      {
        del_u = u3to(struct pgfree, fre_u->nex_p);
        fre_u->end_p  = del_u->end_p;
        fre_u->siz_w += del_u->siz_w;
        fre_u->nex_p  = del_u->nex_p;

        //  XX sanity
        if ( del_u->nex_p ) {
          u3to(struct pgfree, del_u->nex_p)->pre_p = u3of(struct pgfree, fre_u);
        }
      }
    }
    else if ( fre_u->pag_p == tal_p ) {  //  prepend to entry
      fre_u->siz_w += siz_w;
      fre_u->pag_p  = som_p;
    }
    else if ( !fre_u->nex_p ) {          //  insert after
      cac_u->nex_p = 0;
      cac_u->pre_p = u3of(struct pgfree, fre_u);
      fre_u->nex_p = u3of(struct pgfree, cac_u);
      fre_u = cac_u;
      hep_u.cac_p = 0;
    }
    else {
      // XX hosed
    }
  }

  //  XX shrink heap

  if ( del_u ) {
    _ifree(u3of(struct pgfree, del_u));
  }
}

static void
_free_words(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  struct pginfo *pag_u = u3to(struct pginfo, dir_p);
  u3p(struct pginfo) *dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);

  c3_g bit_g = pag_u->log_s - LOG_MINIMUM;
  c3_w pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;

  if ( som_p & pag_u->len_s ) {  //  XX just 1U << log_s and remove?
    //  XX  bad alignment
  }

  if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
    //  XX double free
  }

  pag_u->map_w[pos_w >> 5] |= (1U << (pos_w & 31));
  pag_u->fre_s++;

  {
    u3_post *bit_p = &(hep_u.wee_p[bit_g]);
    struct pginfo *bit_u, *nex_u;

    if ( 1 == pag_u->fre_s ) {
      //  page newly non-full, link
      //
      while ( *bit_p ) {
        bit_u = u3to(struct pginfo, *bit_p);

        if ( !bit_u->nex_p ) break;

        nex_u = u3to(struct pginfo, bit_u->nex_p);

        //  XX road direction?
        if ( nex_u->pag_p > pag_u->pag_p ) break;

        bit_p = &(bit_u->nex_p);
      }

      pag_u->nex_p = *bit_p;
      *bit_p = dir_p;
    }
    else if ( pag_u->fre_s == pag_u->tot_s ) {
      //  page now free
      //
      while ( *bit_p != dir_p ) {
        bit_u = u3to(struct pginfo, *bit_p);
        bit_p = &(bit_u->nex_p);

        //  XX sanity
      }

      *bit_p = pag_u->nex_p;

      dir_u[post_to_page(pag_u->pag_p)] = FIRST;
      som_p = pag_u->pag_p; // NB: clobbers
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
  u3p(struct pginfo) *dir_u = u3to(u3p(struct pginfo), hep_u.pag_p);
  c3_w    pag_w = post_to_page(som_p);

  //  XX sanity: in page dir

  u3_post dir_p = dir_u[pag_w];

  if ( dir_p < MAGIC ) {
    _free_pages(som_p, pag_w, dir_p);
  }
  else {
    _free_words(som_p, pag_w, dir_p);
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
