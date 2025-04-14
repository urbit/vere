
#include "c3/c3.h"
#include "allocate.h"
#include "options.h"
#include "vortex.h"


#ifdef ASAN_ENABLED
  //  XX build problems importing <sanitizers/asan_interface.h>
  //
  void __asan_poison_memory_region(void const volatile *addr, size_t size);
  void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#  define ASAN_POISON_MEMORY_REGION(addr, size) \
     __asan_poison_memory_region((addr), (size))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
     __asan_unpoison_memory_region((addr), (size))
#else
#  define ASAN_POISON_MEMORY_REGION(addr, size) ((void) (addr), (void) (size))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void) (addr), (void) (size))
#endif

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

#define page_to_post(pag_w)  (HEAP.bot_p + (HEAP.dir_ws * (c3_ws)((c3_w)(pag_w - HEAP.off_ws) << u3a_page)))
#define post_to_page(som_p)  (_abs_dif(som_p, (c3_ws)HEAP.bot_p + HEAP.off_ws) >> u3a_page)


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
_drop(u3_post som_p, c3_w len_w)
{
  ASAN_UNPOISON_MEMORY_REGION(u3a_into(som_p), (c3_z)len_w << 2);
}

static void
_init(void)
{
  u3p(u3a_crag) *dir_u;

  if ( c3y == u3a_is_north(u3R) ) {
    HEAP.dir_ws = 1;
    HEAP.off_ws = 0;
  }
  else {
    HEAP.dir_ws = -1;
    HEAP.off_ws = -1;
  }

  // XX and rut
  //
  assert ( !(u3R->hat_p & ((1U << u3a_page) - 1)) );

  HEAP.bot_p = u3R->hat_p;

  assert( u3R->hat_p > u3a_rest_pg );

  //  XX check for overflow

  HEAP.pag_p  = u3R->hat_p;
  HEAP.pag_p += HEAP.off_ws * (c3_ws)(1U << u3a_page);
  HEAP.siz_w  = 1U << u3a_page;
  HEAP.len_w  = 1;

  u3R->hat_p += HEAP.dir_ws * (c3_ws)(1U << u3a_page);

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);

  memset(dir_u, 0, 1U << (u3a_page + 2));
  dir_u[0] = u3a_head_pg;

#ifdef SANITY
  assert( 0 == post_to_page(HEAP.pag_p) );
  assert( HEAP.pag_p == page_to_post(0) );
#endif

  HEAP.cac_p = _imalloc(c3_wiseof(u3a_dell));
}

static void
_extend_directory(c3_w siz_w)  // num pages
{
  u3p(u3a_crag) *dir_u, *old_u;
  u3_post old_p = HEAP.pag_p;
  c3_w nex_w, dif_w, pag_w;

  old_u  = u3to(u3p(u3a_crag), HEAP.pag_p);
  nex_w  = HEAP.len_w + siz_w;       // num words
  nex_w +=   (1U << u3a_page) - 1;
  nex_w &= ~((1U << u3a_page) - 1);
  dif_w  = nex_w >> u3a_page;        //  new pages

  HEAP.pag_p  = u3R->hat_p;
  HEAP.pag_p += HEAP.off_ws * (c3_ws)nex_w;
  u3R->hat_p += HEAP.dir_ws * (c3_ws)nex_w; //  XX overflow

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

#ifdef SANITY
  assert( pag_w == (HEAP.len_w - (HEAP.off_ws * (dif_w - 1))) );
#endif

  {
    c3_z len_z = (c3_z)HEAP.len_w << 2;
    c3_z dif_z = (c3_z)dif_w << 2;

    ASAN_UNPOISON_MEMORY_REGION(dir_u, (c3_z)nex_w << 2);

    memcpy(dir_u, old_u, len_z);

    dir_u[pag_w] = u3a_head_pg;

    for ( c3_w i_w = 1; i_w < dif_w; i_w++ ) {
      dir_u[pag_w + (HEAP.dir_ws * (c3_ws)i_w)] = u3a_rest_pg;
    }

    memset((c3_y*)dir_u + len_z + dif_z, 0, ((c3_z)nex_w << 2) - len_z - dif_z);
  }

  HEAP.len_w += dif_w;
  HEAP.siz_w  = nex_w;

  _ifree(old_p);
}

static u3_post
_extend_heap(c3_w siz_w)  // num pages
{
  u3_post pag_p;

#ifdef SANITY
  assert( HEAP.siz_w >= HEAP.len_w );
#endif

  if ( (HEAP.siz_w - HEAP.len_w) < siz_w ) {
    _extend_directory(siz_w);
  }

  pag_p  = u3R->hat_p;
  pag_p += HEAP.off_ws * (c3_ws)(siz_w << u3a_page);

  u3R->hat_p += HEAP.dir_ws * (c3_ws)(siz_w << u3a_page);

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

  ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), siz_w << (u3a_page + 2));

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
    }
    else {
      pag_w = fre_u->pag_w;
      fre_u->pag_w += siz_w;
      fre_u->siz_w -= siz_w;
    }
    break;
  }

  u3_post pag_p;

  if ( pag_w ) {
    //  XX groace
    //
    pag_w -= HEAP.off_ws * (siz_w - 1);
    pag_p  = page_to_post(pag_w);

#ifdef SANITY
    assert( pag_w < HEAP.len_w );

    //  XX sanity
    assert( u3a_free_pg == dir_u[pag_w] );
    for ( c3_w i_w = 1; i_w < siz_w; i_w++ ) {
      assert( u3a_free_pg == dir_u[pag_w + (HEAP.dir_ws * (c3_ws)i_w)] );
    }
#endif
  }
  else {
    pag_p = _extend_heap(siz_w);
    pag_w = post_to_page(pag_p);
    dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);

#ifdef SANITY
    assert( pag_w < HEAP.len_w );
    assert( (pag_w + ((HEAP.off_ws + 1) * siz_w) - HEAP.off_ws) == HEAP.len_w );
#endif
  }

  dir_u[pag_w] = u3a_head_pg;

  for ( c3_w i_w = 1; i_w < siz_w; i_w++ ) {
    dir_u[pag_w + (HEAP.dir_ws * (c3_ws)i_w)] = u3a_rest_pg;
  }

  //  XX junk

  ASAN_UNPOISON_MEMORY_REGION(u3a_into(pag_p), siz_w << (u3a_page + 2));

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

static void
_rake_chunks(c3_w len_w, c3_w max_w, c3_t rak_t, c3_w* out_w, u3_post* out_p)
{
  c3_g      bit_g = (c3_g)c3_bits_word(len_w - 1) - u3a_min_log;  // 0-9, inclusive
  u3_post   pag_p = HEAP.wee_p[bit_g];
  u3a_crag *pag_u;
  c3_w      hav_w = *out_w;

  if ( rak_t ) {
    c3_w    *map_w;
    c3_g    pos_g;
    u3_post bas_p;
    c3_w    off_w;

    while ( pag_p ) {
      pag_u = u3to(u3a_crag, pag_p);
      bas_p = page_to_post(pag_u->pag_w);
      map_w = pag_u->map_w;

      while ( !*map_w ) { map_w++; }
      off_w = (map_w - pag_u->map_w) << 5;

      if ( (max_w - hav_w) < pag_u->fre_s ) {
        while ( hav_w < max_w ) {
          pos_g   = c3_tz_w(*map_w);
          *map_w &= ~(1U << pos_g);

          out_p[hav_w++] = bas_p + ((off_w + pos_g) << pag_u->log_s);
          pag_u->fre_s--;

          ASAN_UNPOISON_MEMORY_REGION(u3a_into(out_p[hav_w - 1]), pag_u->len_s << 2);

          if ( !*map_w ) {
            do { map_w++; } while ( !*map_w );
            off_w = (map_w - pag_u->map_w) << 5;
          }
        }

        HEAP.wee_p[bit_g] = pag_p;
        *out_w = hav_w;
        return;
      }

      ASAN_UNPOISON_MEMORY_REGION(u3a_into(bas_p), 1U << (u3a_page + 2));

      while ( 1 ) {
        pos_g   = c3_tz_w(*map_w);
        *map_w &= ~(1U << pos_g);

        out_p[hav_w++] = bas_p + ((off_w + pos_g) << pag_u->log_s);

        if ( !--pag_u->fre_s ) {
          break;
        }

        if ( !*map_w ) {
          do { map_w++; } while ( !*map_w );
          off_w = (map_w - pag_u->map_w) << 5;
        }
      }

      pag_p = pag_u->nex_p;
      pag_u->nex_p = 0;

      if ( hav_w == max_w ) {
        HEAP.wee_p[bit_g] = pag_p;
        *out_w = hav_w;
        return;
      }
    }

    HEAP.wee_p[bit_g] = 0;
  }

  //  manually inlined _make_chunks(), storing each chunk-post to [*out_p]
  //
  {
    u3p(u3a_crag) *dir_u;
    c3_s    log_s, len_s, tot_s, siz_s;
    c3_w    pag_w, hun_w;
    u3_post hun_p;

    log_s = bit_g + u3a_min_log;
    len_s = 1U << log_s;
    tot_s = 1U << (u3a_page - log_s);  // 2-1.024, inclusive

    if ( tot_s > (max_w - hav_w) ) {
      *out_w = hav_w;
      return;
    }

    pag_p  = _alloc_pages(1);
    pag_w  = post_to_page(pag_p);
    siz_s  = c3_wiseof(u3a_crag) - 1;
    siz_s += (tot_s + 31) >> 5;

    //  metacircular base case
    //
    //    trivially deducible from exhaustive enumeration
    //
    if ( len_s <= (siz_s << 1) ) {
      hun_p = pag_p;
      hun_w = 1U + ((siz_s - 1) >> log_s);
    }
    else {
      hun_p = _imalloc(siz_s);
      hun_w = 0;
    }

    pag_u = u3to(u3a_crag, hun_p);
    pag_u->pag_w = pag_w;
    pag_u->log_s = log_s;
    pag_u->len_s = len_s;
    pag_u->fre_s = 0;
    pag_u->nex_p = 0;

    dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
    dir_u[pag_w] = hun_p;

    //  initialize bitmap (zeros, none free)
    //
    {
      c3_w *map_w = pag_u->map_w;
      c3_w  len_w = (tot_s + 31) >> 5;

      while ( len_w-- ) {
        *map_w++ = 0;
      }
    }

    //  reserve chunks stolen for pginfo
    //
    hun_p  = pag_p + (hun_w << log_s);
    tot_s -= hun_w;

    pag_u->tot_s = tot_s;

    while ( tot_s-- ) {
      out_p[hav_w++] = hun_p;
      hun_p += len_s;
    }

    *out_w = hav_w;
  }
}

static u3_post
_make_chunks(c3_g bit_g)  // 0-9, inclusive
{
  u3p(u3a_crag) *dir_u;
  u3a_crag      *pag_u;
  u3_post pag_p, hun_p;
  c3_w    pag_w, hun_w;
  c3_s    log_s, len_s, tot_s, siz_s;

  pag_p = _alloc_pages(1);
  pag_w = post_to_page(pag_p);
  log_s = bit_g + u3a_min_log;
  len_s = 1U << log_s;
  tot_s = 1U << (u3a_page - log_s);  // 2-1.024, inclusive

  siz_s  = c3_wiseof(u3a_crag) - 1;
  siz_s += (tot_s + 31) >> 5;

  ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), 1U << (u3a_page + 2));

  //  metacircular base case
  //
  //    trivially deducible from exhaustive enumeration
  //
  if ( len_s <= (siz_s << 1) ) {
    hun_p = pag_p;
    hun_w = 1U + ((siz_s - 1) >> log_s);
    ASAN_UNPOISON_MEMORY_REGION(u3a_into(pag_p), hun_w << (log_s + 2));
  }
  else {
    hun_p = _imalloc(siz_s);
    hun_w = 0;
  }

  pag_u = u3to(u3a_crag, hun_p);
  pag_u->pag_w = pag_w;
  pag_u->log_s = log_s;
  pag_u->len_s = len_s;

  //  initialize bitmap (ones, all free)
  //
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
  }

  //  reserve chunks stolen for pginfo
  //
  //    XX need static assert that max(hun_w) < 32
  //    pag_u->map_w[0] &= ~0 << hun_w
  //
  for ( c3_w i_w = 0; i_w < hun_w; i_w++ ) {
    pag_u->map_w[i_w >> 5] &= ~(1U << (i_w & 31));
  }

  tot_s -= hun_w;

  pag_u->tot_s = pag_u->fre_s = tot_s;

  dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  dir_u[pag_w] = hun_p;
  pag_u->nex_p = HEAP.wee_p[bit_g];

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

#ifdef SANITY
  assert( pag_u->log_s < u3a_page );
#endif

  while ( !*map_w ) { map_w++; }

  pos_g   = c3_tz_w(*map_w);
  *map_w &= ~(1U << pos_g);

  if ( !--pag_u->fre_s ) {
    HEAP.wee_p[bit_g] = pag_u->nex_p;
    pag_u->nex_p = 0;
  }

  {
    c3_w off_w = map_w - pag_u->map_w;  //  bitmap words
    off_w <<= 5;                        //    (in bits)
    off_w  += pos_g;                    //  chunk index
    off_w <<= pag_u->log_s;             //    (in words)

    u3_post out_p = page_to_post(pag_u->pag_w) + off_w;

    ASAN_UNPOISON_MEMORY_REGION(u3a_into(out_p), pag_u->len_s << 2);
    //  XX poison suffix

    return out_p;
  }
}

static u3_post
_imalloc(c3_w len_w)
{
  if ( len_w > (1U << (u3a_page - 1)) ) {
    len_w  += (1U << u3a_page) - 1;
    len_w >>= u3a_page;
    //  XX poison suffix
    return _alloc_pages(len_w);
  }

  return _alloc_words(c3_max(len_w, u3a_minimum));
}

static inline c3_w
_pages_size(c3_w pag_w)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w           siz_w = 1;

  //  head-page 0 in a south road can only have a size of 1
  //
  if ( pag_w || !HEAP.off_ws ) {
    while( dir_u[pag_w + (HEAP.dir_ws * (c3_ws)siz_w)] == u3a_rest_pg ) {
      dir_u[pag_w + (HEAP.dir_ws * (c3_ws)siz_w)] = u3a_free_pg;
      siz_w++;
    }
  }

  return siz_w;
}

static c3_w
_free_pages(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  u3p(u3a_crag)    *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3a_dell *cac_u, *del_u = NULL, *fre_u = u3tn(u3a_dell, HEAP.fre_p);
  c3_w      nex_w,  siz_w = 1;

  if ( u3a_free_pg == dir_p ) {
    //  XX double free
    fprintf(stderr, "\033[31m"
                    "palloc: double free page som_p=0x%x pag_w=%u\n"
                    "\033[0m",
                    som_p, pag_w);
    return 0; // XX bail
  }

  if ( u3a_head_pg != dir_p ) {
    //  XX pointer to wrong page
    fprintf(stderr, "\033[31m"
                    "palloc: wrong page som_p=0x%x dir_p=0x%x\n"
                    "\033[0m",
                    som_p, dir_p);
    return 0; // XX bail
  }

  if ( som_p & ((1U << u3a_page) - 1) ) {
    //  XX pointer not aligned to page
    fprintf(stderr, "\033[31m"
                    "palloc: bad page alignment som_p=0x%x\n"
                    "\033[0m",
                    som_p);
    return 0; // XX bail
  }

  dir_u[pag_w] = u3a_free_pg;
  siz_w = _pages_size(pag_w);

  //  XX groace
  //
  if ( HEAP.off_ws ) {
    nex_w = pag_w + 1;
    pag_w = nex_w - siz_w;
  }
  else {
    nex_w = pag_w + siz_w;
  }

#ifdef SANITY
  assert( pag_w < HEAP.len_w );
#endif

  if ( nex_w == HEAP.len_w ) {
    ASAN_UNPOISON_MEMORY_REGION(u3a_into(som_p), siz_w << (u3a_page + 2));
    u3R->hat_p -= HEAP.dir_ws * (c3_ws)(siz_w << u3a_page);
    HEAP.len_w -= siz_w;
    return siz_w;
  }

  //  XX madv_free

  ASAN_POISON_MEMORY_REGION(u3a_into(som_p), siz_w << (u3a_page + 2));

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
    }
    else if ( !fre_u->nex_p ) {          //  insert after
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

  if ( del_u ) {
    _ifree(u3of(u3a_dell, del_u));
  }

  return siz_w;
}

static void
_free_words(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  u3a_crag *pag_u = u3to(u3a_crag, dir_p);
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);

#ifdef SANITY
  assert( page_to_post(pag_u->pag_w) == (som_p & ~((1U << u3a_page) - 1)) );
  assert( pag_u->log_s < u3a_page );
#endif

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

  ASAN_POISON_MEMORY_REGION(u3a_into(som_p), pag_u->len_s << 2);

  {
    u3_post *bit_p = &(HEAP.wee_p[bit_g]);
    u3a_crag *bit_u, *nex_u;

    if ( 1 == pag_u->fre_s ) {
      //  page newly non-full, link
      //

      if ( &(u3H->rod_u) == u3R ) {
        while ( *bit_p ) {
          bit_u = u3to(u3a_crag, *bit_p);
          nex_u = u3tn(u3a_crag, bit_u->nex_p);

          if ( nex_u && (nex_u->pag_w < pag_u->pag_w) ) {
            bit_p = &(bit_u->nex_p);
            continue;
          }

          break;
        }
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
                    "palloc: page out of heap som_p=0x%x pag_w=%u len_w=%u\n"
                    "\033[0m",
                    som_p, pag_w, HEAP.len_w);
    return; // XX bail
  }

  u3_post dir_p = dir_u[pag_w];

  if ( dir_p <= u3a_rest_pg ) {
    (void)_free_pages(som_p, pag_w, dir_p);
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

    {
      c3_w dif_w, siz_w = _pages_size(pag_w);

      old_w = siz_w << u3a_page;

      if ( len_w <= old_w ) {
        dif_w = (old_w - len_w) >> u3a_page;

        // XX junk
        //  XX unpoison prefix, poison suffix

        while ( dif_w-- ) {
          dir_u[pag_w + (HEAP.dir_ws * (old_w - dif_w - 1))] = u3a_free_pg;
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
      //  XX unpoison prefix, poison suffix
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

//  XX declare wrapper in allocate.c for debugging
static void
_post_status(u3_post som_p)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w pag_w = post_to_page(som_p);

  if ( pag_w >= HEAP.len_w ) {
    fprintf(stderr, "palloc: out of heap: post som_p=0x%x pag_w=%u len_w=%u\n",
                    som_p, pag_w, HEAP.len_w);
    return;
  }

  u3_post dir_p = dir_u[pag_w];

  if ( dir_p <= u3a_rest_pg ) {
    if ( som_p & ((1U << u3a_page) - 1) ) {
      fprintf(stderr, "palloc: page not aligned som_p=0x%x (0x%x)\n",
                      som_p, som_p & ~(((1U << u3a_page) - 1)));
    }

    if ( u3a_free_pg == dir_p ) {
      fprintf(stderr, "palloc: free page som_p=0x%x pag_w=%u\n",
                      som_p, pag_w);
    }
    else if ( u3a_head_pg != dir_p ) {
      fprintf(stderr, "palloc: rest page som_p=0x%x dir_p=0x%x\n",
                      som_p, dir_p);
    }
    else {
      fprintf(stderr, "palloc: head page in-use som_p=0x%x\n",
                      som_p);
    }
  }
  else {
    u3a_crag *pag_u = u3to(u3a_crag, dir_p);

#ifdef SANITY
    assert( page_to_post(pag_u->pag_w) == (som_p & ~((1U << u3a_page) - 1)) );
    assert( pag_u->log_s < u3a_page );
#endif

    c3_w pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;

    if ( som_p & (pag_u->len_s - 1) ) {
      fprintf(stderr, "palloc: bad alignment som_p=0x%x (0x%x) pag=0x%x len_s=%u\n",
                      som_p, som_p & ~((1U << u3a_page) - 1),
                      dir_p, pag_u->len_s);
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "palloc: words free som_p=0x%x pag=0x%x len=%u\n",
                      som_p, dir_p, pag_u->len_s);
    }
    else {
      fprintf(stderr, "palloc: words in-use som_p=0x%x pag=0x%x, len=%u\n",
                      som_p, dir_p, pag_u->len_s);
    }
  }
}

static c3_w
_idle_pages(void)
{
  u3a_dell* fre_u = u3tn(u3a_dell, HEAP.fre_p);
  c3_w      tot_w = 0;

  while ( fre_u ) {
    tot_w += fre_u->siz_w;
    fre_u = u3tn(u3a_dell, fre_u->nex_p);
  }

  return tot_w;
}

static c3_w
_idle_words(void)
{
  u3a_crag *pag_u;
  c3_w len_w, pag_w, siz_w, tot_w = 0;

  for ( c3_w i_w = 0; i_w < u3a_crag_no; i_w++ ) {
    pag_u = u3tn(u3a_crag, HEAP.wee_p[i_w]);
    siz_w = pag_w = 0;

    while ( pag_u ) {
      len_w  = pag_u->len_s; // XX assert?
      siz_w += pag_u->fre_s;
      pag_u  = u3tn(u3a_crag, pag_u->nex_p);
      pag_w++;
    }

    if ( siz_w ) {
      fprintf(stderr, "idle words: class=%u (%u words) blocks=%u (in %u pages) ",
                      i_w, len_w, siz_w, pag_w);
      u3a_print_memory(stderr, "total", siz_w * len_w);
    }

    tot_w += siz_w * len_w;
  }

  return tot_w;
}

static void
_poison_pages(void)
{
  u3a_dell *fre_u = u3tn(u3a_dell, HEAP.fre_p);
  u3_post   pag_p;

  while ( fre_u ) {
    pag_p = page_to_post(fre_u->pag_w);
    ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), fre_u->siz_w << (u3a_page + 2));
    fre_u = u3tn(u3a_dell, fre_u->nex_p);
  }
}

static void
_poison_words(void)
{
  u3a_crag   *pag_u;
  u3_post     pag_p, hun_p;
  c3_w off_w, wor_w, len_w, *map_w;
  c3_g pos_g;
  c3_s fre_s;

  for ( c3_w i_w = 0; i_w < u3a_crag_no; i_w++ ) {
    pag_u = u3tn(u3a_crag, HEAP.wee_p[i_w]);

    while ( pag_u ) {
      pag_p = page_to_post(pag_u->pag_w);
      map_w = pag_u->map_w;
      len_w = pag_u->len_s << 2;
      fre_s = pag_u->fre_s;

      do {
        while ( !*map_w ) { map_w++; }
        wor_w = *map_w;
        off_w = (map_w - pag_u->map_w) << 5;

        do {
          pos_g  = c3_tz_w(wor_w);
          wor_w &= ~(1U << pos_g);
          hun_p  = pag_p + ((off_w + pos_g) << pag_u->log_s);

          ASAN_POISON_MEMORY_REGION(u3a_into(hun_p), len_w);

        } while ( --fre_s && wor_w );

        map_w++;
      } while ( fre_s );

      pag_u = u3tn(u3a_crag, pag_u->nex_p);
    }
  }
}

static void
_unpoison_words(void)
{
  u3a_crag   *pag_u;
  u3_post     pag_p;

  for ( c3_w i_w = 0; i_w < u3a_crag_no; i_w++ ) {
    pag_u = u3tn(u3a_crag, HEAP.wee_p[i_w]);

    while ( pag_u ) {
      pag_p = page_to_post(pag_u->pag_w);
      ASAN_UNPOISON_MEMORY_REGION(u3a_into(pag_p), 1U << (u3a_page + 2));
      pag_u = u3tn(u3a_crag, pag_u->nex_p);
    }
  }
}

static c3_w
_mark_post(u3_post som_p)
{
  c3_w pag_w = post_to_page(som_p);
  c3_w blk_w = pag_w >> 5;
  c3_w bit_w = pag_w & 31;
  c3_w siz_w;

  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3_post dir_p = dir_u[pag_w];

  //  som_p is a one-or-more page allocation
  //
  if ( dir_p <= u3a_rest_pg ) {
    if ( som_p & ((1U << u3a_page) - 1) ) {
      fprintf(stderr, "palloc: mark: page not aligned som_p=0x%x (0x%x)\n",
                      som_p, som_p & ~(((1U << u3a_page) - 1)));
      return 0;
    }

    if ( u3a_free_pg == dir_p ) {
      fprintf(stderr, "palloc: mark: free page som_p=0x%x pag_w=%u\n",
                      som_p, pag_w);
      return 0;
    }
    else if ( u3a_head_pg != dir_p ) {
      fprintf(stderr, "palloc: mark: rest page som_p=0x%x dir_p=0x%x\n",
                      som_p, dir_p);
      return 0;
    }

    //  page(s) already marked
    //
    if ( u3a_Mark.bit_w[blk_w] & (1U << bit_w) ) {
      return 0;
   }

    u3a_Mark.bit_w[blk_w] |= 1U << bit_w;

    siz_w   = _pages_size(pag_w);
    siz_w <<= u3a_page;

    return siz_w;
  }
  //  som_p is a chunk allocation
  //
  else {
    u3a_crag *pag_u = u3to(u3a_crag, dir_p);
    c3_w      pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;
    c3_w     *mar_w;

    if ( som_p & (pag_u->len_s - 1) ) {
      fprintf(stderr, "palloc: bad alignment som_p=0x%x (0x%x) pag=0x%x len_s=%u\n",
                      som_p, som_p & ~((1U << u3a_page) - 1),
                      dir_p, pag_u->len_s);
      return 0;
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "palloc: words free som_p=0x%x pag=0x%x len=%u\n",
                      som_p, dir_p, pag_u->len_s);
      return 0;
    }

    //  page is marked
    //
    if ( u3a_Mark.bit_w[blk_w] & (1U << bit_w) ) {
      mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];

      if ( !(mar_w[pos_w >> 5] & (1U << (pos_w & 31))) ) {
        return 0;
      }
    }
    //  page is unmarked, allocate and initialize mark-array
    //
    else {
      {
        c3_s tot_s = 1U << (u3a_page - pag_u->log_s);  //  NB: not pag_u->tot_s
        c3_g bit_g = pag_u->log_s - u3a_min_log;
        c3_w map_w = (tot_s + 31) >> 5;

        mar_w = u3a_mark_alloc(map_w);
        u3a_Mark.buf_w[pag_w] = mar_w - u3a_Mark.buf_w;

        for ( c3_w i_w = 0; i_w < map_w; i_w++ ) {
          mar_w[i_w] = ~0;
        }

        //  mark page metadata
        //
        if ( page_to_post(pag_u->pag_w) != dir_p ) {
          u3a_Mark.wee_w[bit_g] += _mark_post(dir_p);
        }
        else {
          c3_s siz_s;
          c3_w hun_w;

          siz_s  = c3_wiseof(u3a_crag) - 1;
          siz_s += (tot_s + 31) >> 5;
          hun_w  = 1U + ((siz_s - 1) >> pag_u->log_s);

          //  XX need static assert that max(hun_w) < 32
          //  mar_w[0] &= ~0 << hun_w
          //
          for ( c3_w i_w = 0; i_w < hun_w; i_w++ ) {
            mar_w[i_w >> 5] &= ~(1U << (i_w & 31));
          }

          u3a_Mark.wee_w[bit_g] += hun_w << pag_u->log_s;
        }

        u3a_Mark.bit_w[blk_w] |= 1U << bit_w;
      }
    }

    mar_w[pos_w >> 5] &= ~(1U << (pos_w & 31));
    siz_w = pag_u->len_s;

    return siz_w;
  }
}

static void
_print_chunk(FILE* fil_u, u3_post som_p, c3_w siz_w)
{
  c3_w *ptr_w = u3to(c3_w, som_p);

  fprintf(fil_u, "{ ");
  //  XX log minimum or u3a_minimum
  for ( c3_w j_w = 0; j_w < 4; j_w++ ) {
    fprintf(fil_u, "0x%x, ", ptr_w[j_w]);
  }

  if ( siz_w > 4 ) {
    fprintf(fil_u, "... ");
  }

  fprintf(fil_u, "}\r\n");
}

static c3_w
_sweep_directory(void)
{
  u3p(u3a_crag)     *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w pag_w, blk_w, bit_w, siz_w, leq_w = 0, tot_w = 0;
  u3_post     dir_p;

  //  unlink leaked whole chunk-pages
  //  (we do this first so we won't need to dereference the metadata)
  //
  {
    u3a_crag *pag_u;
    u3_post  *bit_p;
    c3_g      bit_g;

    for ( bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
      bit_p = &(HEAP.wee_p[bit_g]);

      while ( *bit_p ) {
        pag_u = u3to(u3a_crag, *bit_p);
        pag_w = pag_u->pag_w;
        blk_w = pag_w >> 5;
        bit_w = pag_w & 31;

        if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
          *bit_p = pag_u->nex_p;
        }
        else {
          bit_p  = &(pag_u->nex_p);
        }
      }
    }
  }

  pag_w = 0;
  while ( pag_w < HEAP.len_w ) {
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;
    dir_p = dir_u[pag_w];

    if ( u3a_head_pg == dir_p ) {
      if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
        siz_w = _free_pages(page_to_post(pag_w), pag_w, dir_p);
        if ( 1 == siz_w ) {
          fprintf(stderr, "palloc: leaked page %u\r\n", pag_w);
        }
        else {
          fprintf(stderr, "palloc: leaked pages %u-%u\r\n",
                          pag_w, pag_w + siz_w - 1);
        }
        leq_w += siz_w << u3a_page;
      }
      else {
        siz_w  = _pages_size(pag_w);
        tot_w += siz_w << u3a_page;
      }

      //  XX wrong in south roads?
      pag_w += siz_w;
      continue;
    }
    else if ( u3a_rest_pg < dir_p ) {
      //  entire chunk page is unmarked
      //
      if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
        fprintf(stderr, "palloc: leaked chunk page %u\r\n", pag_w);
        (void)_free_pages(page_to_post(pag_w), pag_w, u3a_head_pg);
        leq_w += 1U << u3a_page;
      }
      else {
        u3a_crag *pag_u = u3to(u3a_crag, dir_p);
        c3_s      tot_s = 1U << (u3a_page - pag_u->log_s);  //  NB: not pag_u->tot_s
        u3_post   som_p, bas_p = page_to_post(pag_u->pag_w);
        c3_w     *mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
        c3_w      max_w = ((tot_s + 31) >> 5) << 2;

        siz_w = pag_u->len_s;

        if ( 0 == memcmp(mar_w, pag_u->map_w, max_w) ) {
          tot_w += siz_w * (tot_s - pag_u->fre_s);
        }
        //  NB: since at least one chunk is marked,
        //  _free_words() will never free [pag_u]
        //
        else {
          for ( c3_s i_s = 0; i_s < tot_s; i_s++ ) {
            blk_w = i_s >> 5;
            bit_w = i_s & 31;

            if ( !(pag_u->map_w[blk_w] & (1U << bit_w)) ) {
              if ( !(mar_w[blk_w] & (1U << bit_w)) ) {
                tot_w += siz_w;
              }
              else {
                som_p = bas_p + ((c3_w)i_s << pag_u->log_s);

                fprintf(stderr, "palloc: leak: 0x%x (chunk %u in page %u)\r\n", som_p, i_s, pag_w);

                _print_chunk(stderr, som_p, siz_w);
                _free_words(som_p, pag_w, dir_p);
                leq_w += siz_w;
              }
            }
          }
        }
      }
    }

    pag_w++;
  }

  if ( leq_w ) {
    u3a_print_memory(stderr, "palloc: sweep: leaked", leq_w);
    // u3_assert(0);
  }

  if ( u3C.wag_w & u3o_verbose ) {
    u3a_print_memory(stderr, "palloc: off-heap: used", u3a_Mark.len_w);
    u3a_print_memory(stderr, "palloc: off-heap: total", u3a_Mark.siz_w);
  }

  u3a_mark_done(); // XX move

  return tot_w;
}

static c3_w
_count_post(u3_post som_p, c3_y rat_y)
{
  c3_w pag_w = post_to_page(som_p);
  c3_w blk_w = pag_w >> 5;
  c3_w bit_w = pag_w & 31;
  c3_w siz_w;

  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  u3_post dir_p = dir_u[pag_w];

  //  som_p is a one-or-more page allocation
  //
  if ( dir_p <= u3a_rest_pg ) {
    if ( som_p & ((1U << u3a_page) - 1) ) {
      fprintf(stderr, "palloc: mark: page not aligned som_p=0x%x (0x%x)\n",
                      som_p, som_p & ~(((1U << u3a_page) - 1)));
      return 0;
    }

    if ( u3a_free_pg == dir_p ) {
      fprintf(stderr, "palloc: mark: free page som_p=0x%x pag_w=%u\n",
                      som_p, pag_w);
      return 0;
    }
    else if ( u3a_head_pg != dir_p ) {
      fprintf(stderr, "palloc: mark: rest page som_p=0x%x dir_p=0x%x\n",
                      som_p, dir_p);
      return 0;
    }

    switch ( rat_y ) {
      case 0: {  //  not refcounted
        u3_assert( (c3_ws)u3a_Mark.buf_w[pag_w] <= 0 );
        u3a_Mark.buf_w[pag_w] -= 1;
      } break;

      case 1: {  //  refcounted
        //  NB: "premarked"
        //
        if ( 0x80000000 == u3a_Mark.buf_w[pag_w] ) {
          u3a_Mark.buf_w[pag_w] = 1;
        }
        else {
          u3_assert( (c3_ws)u3a_Mark.buf_w[pag_w] >= 0 );
          u3a_Mark.buf_w[pag_w] += 1;
        }
      } break;

      case 2: {  //  refcounted, pre-mark
        if ( !u3a_Mark.buf_w[pag_w] ) {
          u3a_Mark.buf_w[pag_w] = 0x80000000;
        }
      } break;

      default: u3_assert(0);
    }

    //  page(s) already marked
    //
    if ( u3a_Mark.bit_w[blk_w] & (1U << bit_w) ) {
      return 0;
   }

    u3a_Mark.bit_w[blk_w] |= 1U << bit_w;

    siz_w   = _pages_size(pag_w);
    siz_w <<= u3a_page;

    return siz_w;
  }
  //  som_p is a chunk allocation
  //
  else {
    u3a_crag *pag_u = u3to(u3a_crag, dir_p);
    c3_w      pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;
    c3_w     *mar_w;

    if ( som_p & (pag_u->len_s - 1) ) {
      fprintf(stderr, "palloc: bad alignment som_p=0x%x (0x%x) pag=0x%x len_s=%u\n",
                      som_p, som_p & ~((1U << u3a_page) - 1),
                      dir_p, pag_u->len_s);
      return 0;
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "palloc: words free som_p=0x%x pag=0x%x len=%u\n",
                      som_p, dir_p, pag_u->len_s);
      return 0;
    }

    //  page is marked
    //
    if ( u3a_Mark.bit_w[blk_w] & (1U << bit_w) ) {
      mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
      siz_w = (!mar_w[pos_w]) ? pag_u->len_s : 0;
    }
    //  page is unmarked, allocate and initialize mark-array
    //
    else {
      {
        c3_g bit_g = pag_u->log_s - u3a_min_log;
        c3_s tot_s = 1U << (u3a_page - pag_u->log_s);  //  NB: not pag_u->tot_s
        c3_w map_w = tot_s;

        siz_w = pag_u->len_s;

        mar_w = u3a_mark_alloc(map_w);
        u3a_Mark.buf_w[pag_w] = mar_w - u3a_Mark.buf_w;

        for ( c3_w i_w = 0; i_w < map_w; i_w++ ) {
          mar_w[i_w] = 0;
        }

        //  mark page metadata
        //
        if ( page_to_post(pag_u->pag_w) != dir_p ) {
          u3a_Mark.wee_w[bit_g] += _count_post(dir_p, 0);
        }
        else {
          c3_s siz_s;
          c3_w hun_w;

          siz_s  = c3_wiseof(u3a_crag) - 1;
          siz_s += (tot_s + 31) >> 5;
          hun_w  = 1U + ((siz_s - 1) >> pag_u->log_s);

          for ( c3_w i_w = 0; i_w < hun_w; i_w++ ) {
            mar_w[i_w]--;
          }

          u3a_Mark.wee_w[bit_g] += hun_w << pag_u->log_s;
        }

        u3a_Mark.bit_w[blk_w] |= 1U << bit_w;
      }
    }

    switch ( rat_y ) {
      case 0: {  //  not refcounted
        u3_assert( (c3_ws)mar_w[pos_w] <= 0 );
        mar_w[pos_w] -= 1;
      } break;

      case 1: {  //  refcounted
        //  NB: "premarked"
        //
        if ( 0x80000000 == mar_w[pos_w] ) {
          mar_w[pos_w] = 1;
        }
        else {
          u3_assert( (c3_ws)mar_w[pos_w] >= 0 );
          mar_w[pos_w] += 1;
        }
      } break;

      case 2: {  //  refcounted, pre-mark
        if ( !mar_w[pos_w] ) {
          mar_w[pos_w] = 0x80000000;
        }
      } break;

      default: u3_assert(0);
    }

    return siz_w;
  }
}

static c3_w
_sweep_counts(void)
{
  u3p(u3a_crag)     *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w pag_w, blk_w, bit_w, siz_w, weq_w = 0, leq_w = 0, tot_w = 0;
  u3_post     dir_p, som_p;
  c3_w       *use_w;

  //  unlink leaked whole chunk-pages
  //  (we do this first so we won't need to dereference the metadata)
  //
  {
    u3a_crag *pag_u;
    u3_post  *bit_p;
    c3_g      bit_g;

    for ( bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
      bit_p = &(HEAP.wee_p[bit_g]);

      while ( *bit_p ) {
        pag_u = u3to(u3a_crag, *bit_p);
        pag_w = pag_u->pag_w;
        blk_w = pag_w >> 5;
        bit_w = pag_w & 31;

        if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
          *bit_p = pag_u->nex_p;
        }
        else {
          bit_p  = &(pag_u->nex_p);
        }
      }
    }
  }

  pag_w = 0;
  while ( pag_w < HEAP.len_w ) {
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;
    dir_p = dir_u[pag_w];

    if ( u3a_head_pg == dir_p ) {
      som_p = page_to_post(pag_w);

      if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
        siz_w = _free_pages(som_p, pag_w, dir_p);
        if ( 1 == siz_w ) {
          fprintf(stderr, "palloc: leaked page %u\r\n", pag_w);
        }
        else {
          fprintf(stderr, "palloc: leaked pages %u-%u\r\n",
                          pag_w, pag_w + siz_w - 1);
        }
        leq_w += siz_w << u3a_page;
      }
      else {
        siz_w = _pages_size(pag_w);

        if ( (c3_ws)u3a_Mark.buf_w[pag_w] > 0 ) {
          use_w = u3to(c3_w, som_p);

          if ( *use_w != u3a_Mark.buf_w[pag_w] ) {
            fprintf(stderr, "weak: 0x%x have %u need %u\r\n", som_p, *use_w, u3a_Mark.buf_w[pag_w]);
            *use_w = u3a_Mark.buf_w[pag_w];
            weq_w += siz_w << u3a_page;;
          }
          else {
            tot_w += siz_w << u3a_page;
          }
        }
        else if ( 0x80000000 == u3a_Mark.buf_w[pag_w] ) {
          fprintf(stderr, "sweep: error: premarked %u pages at 0x%x\r\n",
                          siz_w, som_p);
          u3_assert(0);
        }
        else {
          tot_w += siz_w << u3a_page;
        }
      }

      //  XX wrong in south roads?
      pag_w += siz_w;
      continue;
    }
    else if ( u3a_rest_pg < dir_p ) {
      //  entire chunk page is unmarked
      //
      if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
        fprintf(stderr, "palloc: leaked chunk page %u\r\n", pag_w);
        (void)_free_pages(page_to_post(pag_w), pag_w, u3a_head_pg);
        leq_w += 1U << u3a_page;
      }
      else {
        u3a_crag *pag_u = u3to(u3a_crag, dir_p);
        c3_s      tot_s = 1U << (u3a_page - pag_u->log_s);  //  NB: not pag_u->tot_s
        u3_post   bas_p = page_to_post(pag_u->pag_w);
        c3_w     *mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
        c3_w pos_w;

        siz_w = pag_u->len_s;

        //  NB: since at least one chunk is marked,
        //  _free_words() will never free [pag_u]
        //
        for ( c3_s i_s = 0; i_s < tot_s; i_s++ ) {
          pos_w = i_s;
          blk_w = i_s >> 5;
          bit_w = i_s & 31;
          som_p = bas_p + ((c3_w)i_s << pag_u->log_s);

          if ( !(pag_u->map_w[blk_w] & (1U << bit_w)) ) {
            if ( mar_w[pos_w] ) {
              if ( (c3_ws)mar_w[pos_w] > 0 ) {
                use_w = u3to(c3_w, som_p);

                if ( *use_w != mar_w[pos_w] ) {
                  fprintf(stderr, "weak: 0x%x have %u need %u\r\n", som_p, *use_w, mar_w[pos_w]);
                  _print_chunk(stderr, som_p, siz_w);
                  *use_w = mar_w[pos_w];
                  weq_w += siz_w;
                }
                else {
                  tot_w += siz_w;
                }
              }
              else if ( 0x80000000 == mar_w[pos_w] ) {
                fprintf(stderr, "sweep: error: premarked 0x%x (chunk %u in page %u)\r\n",
                                som_p, i_s, pag_w);
                u3_assert(0);
              }
              else {
                if ( (c3_ws)mar_w[pos_w] < -1 ) {
                  fprintf(stderr, "alias: 0x%x count %d\r\n", som_p, (c3_ws)mar_w[pos_w]);
                }
                tot_w += siz_w;
              }
            }
            else {
              fprintf(stderr, "palloc: leak: 0x%x (chunk %u in page %u)\r\n", som_p, i_s, pag_w);

              _print_chunk(stderr, som_p, siz_w);
              _free_words(som_p, pag_w, dir_p);
              leq_w += siz_w;
            }
          }
        }
      }
    }

    pag_w++;
  }

  if ( leq_w ) {
    u3a_print_memory(stderr, "palloc: sweep: leaked", leq_w);
    // u3_assert(0);
  }
  if ( weq_w ) {
    u3a_print_memory(stderr, "palloc: sweep: weaked", weq_w);
    // u3_assert(0);
  }

  if ( u3C.wag_w & u3o_verbose ) {
    u3a_print_memory(stderr, "palloc: off-heap: used", u3a_Mark.len_w);
    u3a_print_memory(stderr, "palloc: off-heap: total", u3a_Mark.siz_w);
  }

  u3a_mark_done(); // XX move

  return tot_w;
}
