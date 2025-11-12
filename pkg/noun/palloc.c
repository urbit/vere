
#include "c3/c3.h"
#include "allocate.h"
#include "options.h"
#include "vortex.h"

#undef SANITY
#undef PACK_CHECK

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

#ifndef BASE
  #define BASE u3R->rut_p
#endif

#define page_to_post(pag_w)  (BASE + (HEAP.dir_ws * (c3_ws)((c3_w)(pag_w - HEAP.off_ws) << u3a_page)))
#define post_to_page(som_p)  (_abs_dif(som_p, (c3_ws)BASE + HEAP.off_ws) >> u3a_page)

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
_init_once(void)
{
  u3a_hunk_dose *hun_u;
  c3_s mun_w = 0;

  for (c3_g bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
    hun_u = &(u3a_Hunk[bit_g]);
    hun_u->log_s = bit_g + u3a_min_log;
    hun_u->len_s = 1U << hun_u->log_s;
    hun_u->tot_s = 1U << (u3a_page - hun_u->log_s);
    hun_u->map_s = (hun_u->tot_s + 31) >> 5;
    hun_u->siz_s = c3_wiseof(u3a_crag) + hun_u->map_s - 1;

    //  metacircular base case
    //
    //    trivially deducible from exhaustive enumeration
    //
    if ( hun_u->len_s <= (hun_u->siz_s << 1) ) {
      hun_u->hun_s = 1U + ((hun_u->siz_s - 1) >> hun_u->log_s);
    }
    else {
      hun_u->hun_s = 0;
    }

    hun_u->ful_s = hun_u->tot_s - hun_u->hun_s;
    mun_w = c3_max(mun_w, hun_u->hun_s);
  }

  u3_assert( 32 > mun_w );
}

static void
_drop(u3_post som_p, c3_w len_w)
{
  ASAN_UNPOISON_MEMORY_REGION(u3a_into(som_p), (c3_z)len_w << 2);
}

static void
_init_heap(void)
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

  assert( !(u3R->hat_p & ((1U << u3a_page) - 1)) );
  assert( u3R->hat_p > u3a_rest_pg );
  assert( u3R->hat_p == u3R->rut_p );

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
  assert( HEAP.len_w == post_to_page(u3R->hat_p + HEAP.off_ws) );
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

  //  XX depend on the guard page for these?
  //
  if ( 1 == HEAP.dir_ws ) {
    if ( u3R->hat_p >= u3R->cap_p ) {
      u3m_bail(c3__meme); return;
    }
  }
  else {
    if ( u3R->hat_p <= u3R->cap_p ) {
      u3m_bail(c3__meme); return;
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

#ifdef SANITY
  assert( HEAP.len_w == post_to_page(u3R->hat_p + HEAP.off_ws) );
  assert( dir_u[HEAP.len_w - 1] );
#endif
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

  //  XX depend on the guard page for these?
  //
  if ( 1 == HEAP.dir_ws ) {
    if ( u3R->hat_p >= u3R->cap_p ) {
      return u3m_bail(c3__meme);
    }
  }
  else {
    if ( u3R->hat_p <= u3R->cap_p ) {
      return u3m_bail(c3__meme);
    }
  }

  HEAP.len_w += siz_w;

  ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), siz_w << (u3a_page + 2));

#ifdef SANITY
  assert( HEAP.len_w == post_to_page(u3R->hat_p + HEAP.off_ws) );
#endif

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
      else {
        HEAP.erf_p = fre_u->pre_p;
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

#ifdef SANITY
  assert( HEAP.len_w == post_to_page(u3R->hat_p + HEAP.off_ws) );
  assert( dir_u[HEAP.len_w - 1] );
#endif

  return pag_p;
}

static void
_rake_chunks(c3_w len_w, c3_w max_w, c3_t rak_t, c3_w* out_w, u3_post* out_p)
{
  c3_g      bit_g = (c3_g)c3_bits_word(len_w - 1) - u3a_min_log;  // 0-9, inclusive
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
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

          ASAN_UNPOISON_MEMORY_REGION(u3a_into(out_p[hav_w - 1]), (c3_z)hun_u->len_s << 2);

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

  //  XX s/b ful_s
  if ( hun_u->tot_s > (max_w - hav_w) ) {
    *out_w = hav_w;
    return;
  }

  //  manually inlined _make_chunks(), storing each chunk-post to [*out_p]
  //
  {
    u3_post hun_p;
    c3_w    pag_w;

    pag_p = _alloc_pages(1);
    pag_w = post_to_page(pag_p);
    hun_p = ( hun_u->hun_s ) ? pag_p : _imalloc(hun_u->siz_s);
    pag_u = u3to(u3a_crag, hun_p);
    pag_u->pag_w = pag_w;
    pag_u->log_s = hun_u->log_s;
    pag_u->fre_s = 0;
    pag_u->nex_p = 0;
    //  initialize bitmap (zeros, none free)
    //
    memset(pag_u->map_w, 0, (c3_z)hun_u->map_s << 2);

    {
      u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
      dir_u[pag_w] = hun_p;
    }

    for ( c3_s i_s = hun_u->hun_s; i_s < hun_u->tot_s; i_s++ ) {
      out_p[hav_w++] = pag_p + (i_s << pag_u->log_s);
    }

    *out_w = hav_w;
  }
}

static u3_post
_make_chunks(c3_g bit_g)  // 0-9, inclusive
{
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
  u3a_crag      *pag_u;
  u3_post hun_p, pag_p = _alloc_pages(1);
  c3_w    pag_w = post_to_page(pag_p);

  ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), 1U << (u3a_page + 2));

  if ( hun_u->hun_s ) {
    hun_p = pag_p;
    ASAN_UNPOISON_MEMORY_REGION(u3a_into(pag_p), (c3_z)hun_u->hun_s << (hun_u->log_s + 2));
  }
  else {
    hun_p = _imalloc(hun_u->siz_s);
  }

  pag_u = u3to(u3a_crag, hun_p);
  pag_u->pag_w = pag_w;
  pag_u->log_s = hun_u->log_s;
  pag_u->fre_s = hun_u->ful_s;

  //  initialize bitmap (ones, all free)
  //
  if ( hun_u->tot_s < 32 ) {
    pag_u->map_w[0] = (1U << hun_u->tot_s) - 1;
  }
  else {
    memset(pag_u->map_w, 0xff, (c3_z)hun_u->map_s << 2);
  }

  //  reserve chunks stolen for pginfo
  //  NB: max [hun_s] guarded by assertion in _init_once()
  //
  pag_u->map_w[0] &= ~0U << hun_u->hun_s;

  {
    u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
    dir_u[pag_w] = hun_p;
  }

  pag_u->nex_p = HEAP.wee_p[bit_g];
  HEAP.wee_p[bit_g] = hun_p;

  return hun_p;
}

static u3_post
_alloc_words(c3_w len_w)  //  4-2.048, inclusive
{
  c3_g      bit_g = (c3_g)c3_bits_word(len_w - 1) - u3a_min_log;  // 0-9, inclusive
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
  u3a_crag *pag_u;
  c3_w     *map_w;
  c3_g      pos_g;

  if ( !HEAP.wee_p[bit_g] ) {
    pag_u = u3to(u3a_crag, _make_chunks(bit_g));
  }
  else {
    pag_u = u3to(u3a_crag, HEAP.wee_p[bit_g]);
    //  XX sanity

    if ( 1 == pag_u->fre_s ) {
      HEAP.wee_p[bit_g] = pag_u->nex_p;
      pag_u->nex_p = 0;
    }
  }

  pag_u->fre_s--;
  map_w = pag_u->map_w;
  while ( !*map_w ) { map_w++; }

  pos_g   = c3_tz_w(*map_w);
  *map_w &= ~(1U << pos_g);

  {
    u3_post out_p, bas_p = page_to_post(pag_u->pag_w);
    c3_w    off_w = (map_w - pag_u->map_w) << 5;

    out_p = bas_p + ((off_w + pos_g) << pag_u->log_s);
    ASAN_UNPOISON_MEMORY_REGION(u3a_into(out_p), hun_u->len_s << 2);
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
      siz_w++;
    }
  }

  return siz_w;
}

static c3_w
_free_pages(u3_post som_p, c3_w pag_w, u3_post dir_p)
{
  u3a_dell *cac_u, *fre_u, *del_u = NULL;
  c3_w      nex_w,  siz_w = 1;
  u3_post   fre_p;

  if ( u3a_free_pg == dir_p ) {
    fprintf(stderr, "\033[31m"
                    "palloc: double free page som_p=0x%x pag_w=%u\r\n"
                    "\033[0m",
                    som_p, pag_w);
    u3_assert(!"loom: corrupt"); return 0;
  }

  if ( u3a_head_pg != dir_p ) {
    fprintf(stderr, "\033[31m"
                    "palloc: wrong page som_p=0x%x dir_p=0x%x\r\n"
                    "\033[0m",
                    som_p, dir_p);
    u3_assert(!"loom: corrupt"); return 0;
  }

  if ( som_p & ((1U << u3a_page) - 1) ) {
    fprintf(stderr, "\033[31m"
                    "palloc: bad page alignment som_p=0x%x\r\n"
                    "\033[0m",
                    som_p);
    u3_assert(!"loom: corrupt"); return 0;
  }

  {
    u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);

    dir_u[pag_w] = u3a_free_pg;

    //  head-page 0 in a south road can only have a size of 1
    //
    if ( pag_w || !HEAP.off_ws ) {
      while( dir_u[pag_w + (HEAP.dir_ws * (c3_ws)siz_w)] == u3a_rest_pg ) {
        dir_u[pag_w + (HEAP.dir_ws * (c3_ws)siz_w)] = u3a_free_pg;
        siz_w++;
      }
    }
  }

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
  assert( HEAP.len_w == post_to_page(u3R->hat_p + HEAP.off_ws) );
#endif

  if ( nex_w == HEAP.len_w ) {
    u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
    c3_w           wiz_w = siz_w;

    // check if prior pages are already free
    //
    if ( !dir_u[HEAP.len_w - (siz_w + 1)] ) {
      assert( HEAP.erf_p );
      fre_u = u3to(u3a_dell, HEAP.erf_p);
      assert( (fre_u->pag_w + fre_u->siz_w) == pag_w );

      if ( fre_u->pre_p ) {
        HEAP.erf_p = fre_u->pre_p;
        u3to(u3a_dell, fre_u->pre_p)->nex_p = 0;
      }
      else {
        HEAP.fre_p = HEAP.erf_p = 0;
      }

      pag_w  = fre_u->pag_w;  // NB: clobbers
      siz_w += fre_u->siz_w;

      // XX groace
      //
      pag_w -= HEAP.off_ws * (siz_w - 1);
      som_p  = page_to_post(pag_w);
    }
    else {
      fre_u = NULL;
    }

    ASAN_UNPOISON_MEMORY_REGION(u3a_into(som_p), siz_w << (u3a_page + 2));
    u3R->hat_p -= HEAP.dir_ws * (c3_ws)(siz_w << u3a_page);
    HEAP.len_w -= siz_w;

    // fprintf(stderr, "shrink heap 0x%x 0x%x %u:%u (%u) 0x%x\r\n",
    //                 som_p, som_p + (siz_w << u3a_page), pag_w, wiz_w, siz_w, u3R->hat_p);

#ifdef SANITY
    assert( HEAP.len_w == post_to_page(u3R->hat_p + HEAP.off_ws) );
    assert( dir_u[HEAP.len_w - 1] );
#endif

    if ( fre_u ) {
      _ifree(u3of(u3a_dell, fre_u));
    }

    return wiz_w;
  }

  //  XX madv_free

  ASAN_POISON_MEMORY_REGION(u3a_into(som_p), siz_w << (u3a_page + 2));

  //  XX add temporary freelist entry?
  // if (  !HEAP.cac_p && !HEAP.fre_p
  //    && !HEAP.wee_p[(c3_bits_word(c3_wiseof(*cac_u) - 1) - u3a_min_log)] )
  // {
  //   fprintf(stderr, "palloc: growing heap to free pages\r\n");
  // }

  if ( !HEAP.cac_p ) {
    fre_p = _imalloc(c3_wiseof(*cac_u));
  }
  else {
    fre_p = HEAP.cac_p;
    HEAP.cac_p = 0;
  }

  cac_u = u3to(u3a_dell, fre_p);
  cac_u->pag_w = pag_w;
  cac_u->siz_w = siz_w;

  if ( !(fre_u = u3tn(u3a_dell, HEAP.fre_p)) ) {
    // fprintf(stderr, "free pages 0x%x (%u) via 0x%x\r\n", som_p, siz_w, HEAP.cac_p);
    cac_u->nex_p = 0;
    cac_u->pre_p = 0;
    HEAP.fre_p = HEAP.erf_p = fre_p;
    fre_p = 0;
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

      fre_u->pre_p = fre_p;

      //  XX sanity
      if ( cac_u->pre_p ) {
        u3to(u3a_dell, cac_u->pre_p)->nex_p = fre_p;
      }
      else {
        HEAP.fre_p = fre_p;
      }

      fre_p = 0;
    }
    else if ( fex_w == pag_w ) {  //  append to entry
      fre_u->siz_w += siz_w;

      //  coalesce with next entry
      //
      if (  fre_u->nex_p
         && ((fex_w + siz_w) == u3to(u3a_dell, fre_u->nex_p)->pag_w) )
      {
        del_u = u3to(u3a_dell, fre_u->nex_p);
        fre_u->siz_w += del_u->siz_w;
        fre_u->nex_p  = del_u->nex_p;

        //  XX sanity
        if ( del_u->nex_p ) {
          u3to(u3a_dell, del_u->nex_p)->pre_p = u3of(u3a_dell, fre_u);
        }
        else {
          HEAP.erf_p = u3of(u3a_dell, fre_u);
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
      HEAP.erf_p = fre_u->nex_p = fre_p;
      fre_p = 0;
    }
    else {
      fprintf(stderr, "\033[31m"
                      "palloc: free list hosed at som_p=0x%x pag=%u len=%u\r\n"
                      "\033[0m",
                      (u3_post)u3of(u3a_dell, fre_u), fre_u->pag_w, fre_u->siz_w);
      u3_assert(!"loom: corrupt"); return 0;
    }
  }

  if ( fre_p ) {
    if ( !HEAP.cac_p ) {
      HEAP.cac_p = fre_p;
    }
    else {
      _ifree(fre_p);
    }

    if ( del_u ) {
      _ifree(u3of(u3a_dell, del_u));
    }
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
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);

  if ( som_p & (hun_u->len_s - 1) ) {
    fprintf(stderr, "\033[31m"
                    "palloc: bad alignment som_p=0x%x pag=%u cag=0x%x len_s=%u\r\n"
                    "\033[0m",
                    som_p, post_to_page(som_p), dir_p, hun_u->len_s);
    u3_assert(!"loom: corrupt"); return;
  }

  if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
    fprintf(stderr, "\033[31m"
                    "palloc: double free som_p=0x%x pag=0x%x\r\n"
                    "\033[0m",
                    som_p, dir_p);
    u3_assert(!"loom: corrupt"); return;
  }

  pag_u->map_w[pos_w >> 5] |= (1U << (pos_w & 31));
  pag_u->fre_s++;

  ASAN_POISON_MEMORY_REGION(u3a_into(som_p), hun_u->len_s << 2);

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
    else if ( pag_u->fre_s == hun_u->ful_s ) {
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
                    "palloc: page out of heap som_p=0x%x pag_w=%u len_w=%u\r\n"
                    "\033[0m",
                    som_p, pag_w, HEAP.len_w);
    u3_assert(!"loom: corrupt"); return;
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
                    "palloc: realloc page out of heap som_p=0x%x pag_w=%u\r\n"
                    "\033[0m",
                    som_p, pag_w);
    u3_assert(!"loom: corrupt"); return 0;
  }

  u3_post dir_p = dir_u[pag_w];

  if ( u3a_head_pg == dir_p ) {
    if ( som_p & ((1U << u3a_page) - 1) ) {
      fprintf(stderr, "\033[31m"
                      "palloc: realloc bad page alignment som_p=0x%x\r\n"
                      "\033[0m",
                      som_p);
      u3_assert(!"loom: corrupt"); return 0;
    }

    {
      c3_w dif_w, siz_w = _pages_size(pag_w);

      old_w = siz_w << u3a_page;

      if (  (len_w < old_w)
         && (dif_w = (old_w - len_w) >> u3a_page) )
      {
        pag_w += HEAP.dir_ws * (siz_w - dif_w);
        (void)_free_pages(page_to_post(pag_w), pag_w, u3a_head_pg);

        // XX junk
        //  XX unpoison prefix, poison suffix
        return som_p;
      }

      //  XX also grow in place if sufficient adjacent pages are free?
    }
  }
  else if ( u3a_rest_pg >= dir_p ) {
    fprintf(stderr, "\033[31m"
                    "palloc: realloc wrong page som_p=0x%x\r\n"
                    "\033[0m",
                    som_p);
    u3_assert(!"loom: corrupt"); return 0;
  }
  else {
    u3a_crag *pag_u = u3to(u3a_crag, dir_p);
    c3_w pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;
    const u3a_hunk_dose *hun_u = &(u3a_Hunk[pag_u->log_s - u3a_min_log]);

    if ( som_p & (hun_u->len_s - 1) ) {
      fprintf(stderr, "\033[31m"
                      "palloc: realloc bad alignment som_p=0x%x pag=0x%x len_s=%u\r\n"
                      "\033[0m",
                      som_p, dir_p, hun_u->len_s);
      u3_assert(!"loom: corrupt"); return 0;
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "\033[31m"
                      "palloc: realloc free som_p=0x%x pag=0x%x\r\n"
                      "\033[0m",
                      som_p, dir_p);
      u3_assert(!"loom: corrupt"); return 0;
    }

    old_w = hun_u->len_s;

    if (  (len_w <= old_w)
       && ((len_w > (old_w >> 1)) || (u3a_minimum == old_w)) )
    {
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

static void
_post_status(u3_post som_p)
{
  u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w pag_w = post_to_page(som_p);

  if ( pag_w >= HEAP.len_w ) {
    fprintf(stderr, "palloc: out of heap: post som_p=0x%x pag_w=%u len_w=%u\r\n",
                    som_p, pag_w, HEAP.len_w);
    return;
  }

  u3_post dir_p = dir_u[pag_w];

  if ( dir_p <= u3a_rest_pg ) {
    if ( som_p & ((1U << u3a_page) - 1) ) {
      fprintf(stderr, "palloc: page not aligned som_p=0x%x (0x%x)\r\n",
                      som_p, som_p & ~(((1U << u3a_page) - 1)));
    }

    if ( u3a_free_pg == dir_p ) {
      fprintf(stderr, "palloc: free page som_p=0x%x pag_w=%u\r\n",
                      som_p, pag_w);
    }
    else if ( u3a_head_pg != dir_p ) {
      fprintf(stderr, "palloc: rest page som_p=0x%x dir_p=0x%x\r\n",
                      som_p, dir_p);
    }
    else {
      //  XX include size
      fprintf(stderr, "palloc: head page in-use som_p=0x%x\r\n",
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
    const u3a_hunk_dose *hun_u = &(u3a_Hunk[pag_u->log_s - u3a_min_log]);

    if ( som_p & (hun_u->len_s - 1) ) {
      fprintf(stderr, "palloc: bad alignment som_p=0x%x (0x%x) pag=0x%x len_s=%u\r\n",
                      som_p, som_p & ~((1U << u3a_page) - 1),
                      dir_p, hun_u->len_s);
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "palloc: words free som_p=0x%x pag=0x%x len=%u\r\n",
                      som_p, dir_p, hun_u->len_s);
    }
    else {
      fprintf(stderr, "palloc: words in-use som_p=0x%x pag=0x%x, len=%u\r\n",
                      som_p, dir_p, hun_u->len_s);
    }
  }
}

static void
_sane_dell(void)
{
  u3p(u3a_dell) pre_p, nex_p, fre_p = HEAP.fre_p;
  u3a_dell*     fre_u;

  if ( !HEAP.fre_p != !HEAP.erf_p ) {
    fprintf(stderr, "dell: insane: head %u tail %u\r\n", !!HEAP.fre_p, !!HEAP.erf_p);
  }

  while ( fre_p ) {
    fre_u = u3to(u3a_dell, fre_p);
    pre_p = fre_u->pre_p;
    nex_p = fre_u->nex_p;

    if ( pre_p ) {
      if ( u3to(u3a_dell, pre_p)->nex_p != fre_p ) {
        fprintf(stderr, "dell: insane: prev next not us\r\n");
      }
    }
    else if ( fre_p != HEAP.fre_p ) {
      fprintf(stderr, "dell: insane: missing previous\r\n");
    }

    if ( nex_p ) {
      if ( u3to(u3a_dell, nex_p)->pre_p != fre_p ) {
        fprintf(stderr, "dell: insane: next prev not us\r\n");
      }
    }
    else if ( fre_p != HEAP.erf_p ) {
      fprintf(stderr, "dell: insane: missing next\r\n");
    }

    fre_p = nex_p;
  }

  {
    u3p(u3a_crag)     *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
    c3_w fre_w, nex_w, pag_w = 0;

    fre_p = HEAP.fre_p;

    while ( pag_w < HEAP.len_w ) {
      fre_u = u3tn(u3a_dell, fre_p);
      fre_w = fre_u ? fre_u->pag_w : HEAP.len_w;   // XX wrong in south

      for ( ; pag_w < fre_w; pag_w++ ) {
        if ( !dir_u[pag_w] ) {
          fprintf(stderr, "dell: insane page %u free in directory, not in list\r\n", pag_w);
        }
      }

      if ( fre_u ) {
        nex_w = fre_u->pag_w + fre_u->siz_w;  // XX wrong in south

        for ( ; pag_w < nex_w; pag_w++ ) {
          if ( dir_u[pag_w] ) {
            fprintf(stderr, "dell: insane page %u free in list, not in directory\r\n", pag_w);
          }
        }

        fre_p = fre_u->nex_p;
      }
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
  c3_w pag_w, siz_w, tot_w = 0;

  for ( c3_w i_w = 0; i_w < u3a_crag_no; i_w++ ) {
    pag_u = u3tn(u3a_crag, HEAP.wee_p[i_w]);
    siz_w = pag_w = 0;

    while ( pag_u ) {
      siz_w += pag_u->fre_s;
      pag_u  = u3tn(u3a_crag, pag_u->nex_p);
      pag_w++;
    }

    if ( (u3C.wag_w & u3o_verbose) && siz_w ) {
      fprintf(stderr, "idle words: class=%u (%u words) blocks=%u (in %u pages) ",
                      i_w, (1U << (i_w + u3a_min_log)), siz_w, pag_w);
      u3a_print_memory(stderr, "total", siz_w << (i_w + u3a_min_log));
    }

    tot_w += siz_w << (i_w + u3a_min_log);
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
  const u3a_hunk_dose       *hun_u;
  u3_post     pag_p, hun_p;
  u3a_crag   *pag_u;
  c3_w off_w, wor_w, len_w, *map_w;
  c3_g pos_g;
  c3_s fre_s;

  for ( c3_w i_w = 0; i_w < u3a_crag_no; i_w++ ) {
    hun_u = &(u3a_Hunk[i_w]);
    pag_u = u3tn(u3a_crag, HEAP.wee_p[i_w]);

    while ( pag_u ) {
      pag_p = page_to_post(pag_u->pag_w);
      map_w = pag_u->map_w;
      len_w = (c3_w)hun_u->len_s << 2;
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
      fprintf(stderr, "palloc: mark: page not aligned som_p=0x%x (0x%x)\r\n",
                      som_p, som_p & ~(((1U << u3a_page) - 1)));
      return 0;
    }

    if ( u3a_free_pg == dir_p ) {
      fprintf(stderr, "palloc: mark: free page som_p=0x%x pag_w=%u\r\n",
                      som_p, pag_w);
      return 0;
    }
    else if ( u3a_head_pg != dir_p ) {
      fprintf(stderr, "palloc: mark: rest page som_p=0x%x dir_p=0x%x\r\n",
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
    c3_g bit_g = pag_u->log_s - u3a_min_log;
    const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
    c3_w     *mar_w;

    if ( som_p & (hun_u->len_s - 1) ) {
      fprintf(stderr, "palloc: mark: bad alignment som_p=0x%x (0x%x) pag=0x%x (%u) len_s=%u\r\n",
                      som_p, som_p & ~((1U << u3a_page) - 1),
                      dir_p, pag_u->pag_w, hun_u->len_s);
      return 0;
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "palloc: mark: words free som_p=0x%x pag=0x%x (%u) len=%u\r\n",
                      som_p, dir_p, pag_u->pag_w, hun_u->len_s);
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
      mar_w = u3a_mark_alloc(hun_u->map_s);
      u3a_Mark.buf_w[pag_w] = mar_w - u3a_Mark.buf_w;
      memset(mar_w, 0xff, (c3_z)hun_u->map_s << 2);

      //  mark page metadata
      //
      if ( !hun_u->hun_s ) {
        u3a_Mark.wee_w[bit_g] += _mark_post(dir_p);
        mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
      }
      else {
        //  NB: max [hun_s] guarded by assertion in _init_once()
        //
        mar_w[0] &= ~0U << hun_u->hun_s;
        u3a_Mark.wee_w[bit_g] += (c3_w)hun_u->hun_s << pag_u->log_s;
      }

      u3a_Mark.bit_w[blk_w] |= 1U << bit_w;
    }

    mar_w[pos_w >> 5] &= ~(1U << (pos_w & 31));
    siz_w = hun_u->len_s;

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

  for ( pag_w = 0; pag_w < HEAP.len_w; pag_w++ ) {
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
        c3_g bit_g = pag_u->log_s - u3a_min_log;
        const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
        u3_post   som_p, bas_p = page_to_post(pag_u->pag_w);
        c3_w     *mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];

        siz_w = hun_u->len_s;

        if ( 0 == memcmp(mar_w, pag_u->map_w, (c3_z)hun_u->map_s << 2) ) {
          tot_w += siz_w * (hun_u->tot_s - pag_u->fre_s);
        }
        //  NB: since at least one chunk is marked,
        //  _free_words() will never free [pag_u]
        //
        else {
          for ( c3_s i_s = 0; i_s < hun_u->tot_s; i_s++ ) {
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
      fprintf(stderr, "palloc: mark: page not aligned som_p=0x%x (0x%x)\r\n",
                      som_p, som_p & ~(((1U << u3a_page) - 1)));
      return 0;
    }

    if ( u3a_free_pg == dir_p ) {
      fprintf(stderr, "palloc: mark: free page som_p=0x%x pag_w=%u\r\n",
                      som_p, pag_w);
      return 0;
    }
    else if ( u3a_head_pg != dir_p ) {
      fprintf(stderr, "palloc: mark: rest page som_p=0x%x dir_p=0x%x\r\n",
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
    c3_g bit_g = pag_u->log_s - u3a_min_log;
    const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
    c3_w      pos_w = (som_p & ((1U << u3a_page) - 1)) >> pag_u->log_s;
    c3_w     *mar_w;

    if ( som_p & (hun_u->len_s - 1) ) {
      fprintf(stderr, "palloc: count: bad alignment som_p=0x%x (0x%x) pag=0x%x (%u) len_s=%u\r\n",
                      som_p, som_p & ~((1U << u3a_page) - 1),
                      dir_p, pag_u->pag_w, hun_u->len_s);
      return 0;
    }

    if ( pag_u->map_w[pos_w >> 5] & (1U << (pos_w & 31)) ) {
      fprintf(stderr, "palloc: count: words free som_p=0x%x pag=0x%x (%u) len=%u\r\n",
                      som_p, dir_p, pag_u->pag_w, hun_u->len_s);
      return 0;
    }

    //  page is marked
    //
    if ( u3a_Mark.bit_w[blk_w] & (1U << bit_w) ) {
      mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
      siz_w = (!mar_w[pos_w]) ? hun_u->len_s : 0;
    }
    //  page is unmarked, allocate and initialize mark-array
    //
    else {
      siz_w = hun_u->len_s;
      mar_w = u3a_mark_alloc(hun_u->tot_s);
      u3a_Mark.buf_w[pag_w] = mar_w - u3a_Mark.buf_w;
      memset(mar_w, 0, (c3_z)hun_u->tot_s << 2);

      //  mark page metadata
      //
      if ( !hun_u->hun_s ) {
        u3a_Mark.wee_w[bit_g] += _count_post(dir_p, 0);
        mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
      }
      else {
        memset(mar_w, 0xff, (c3_z)hun_u->hun_s << 2);
        u3a_Mark.wee_w[bit_g] += (c3_w)hun_u->hun_s << pag_u->log_s;
      }

      u3a_Mark.bit_w[blk_w] |= 1U << bit_w;
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

  for ( pag_w = 0; pag_w < HEAP.len_w; pag_w++ ) {
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;
    dir_p = dir_u[pag_w];

    if ( u3a_head_pg == dir_p ) {
      som_p = page_to_post(pag_w);

      if ( !(u3a_Mark.bit_w[blk_w] & (1U << bit_w)) ) {
        siz_w = _free_pages(som_p, pag_w, dir_p);
        if ( 1 == siz_w ) {
          fprintf(stderr, "palloc: leaked page %u (0x%x)\r\n", pag_w, page_to_post(pag_w));
        }
        else {
          fprintf(stderr, "palloc: leaked pages %u-%u (0x%x)\r\n",
                          pag_w, pag_w + siz_w - 1, page_to_post(pag_w));
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
        c3_g bit_g = pag_u->log_s - u3a_min_log;
        const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);
        u3_post   bas_p = page_to_post(pag_u->pag_w);
        c3_w     *mar_w = u3a_Mark.buf_w + u3a_Mark.buf_w[pag_w];
        c3_w pos_w;

        siz_w = hun_u->len_s;

        //  NB: since at least one chunk is marked,
        //  _free_words() will never free [pag_u]
        //
        for ( c3_s i_s = 0; i_s < hun_u->tot_s; i_s++ ) {
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

typedef struct {
  u3_post dir_p;
  c3_s    log_s;  //  log2(len)
  struct {
    c3_w  pag_w;  //  previous page in class (original number)
    c3_s  fre_s;  //  previous hunks available
    c3_s  pos_s;  //  previous hunks used
  } pre_u;
  c3_w    mar_w[0];
} _ca_prag;

typedef struct {
  u3_post dir_p;
  c3_s    log_s;
  c3_w    mar_w[0];
} _ca_frag;

#ifdef PACK_CHECK
static c3_d
_pack_hash(u3_post foo)
{
  return foo * 11400714819323198485ULL;
}

static c3_i
_pack_cmp(u3_noun a, u3_noun b)
{
  return a == b;
}

#define NAME    _pack_posts
#define KEY_TY  u3_post
#define VAL_TY  u3_post
#define HASH_FN _pack_hash
#define CMPR_FN _pack_cmp
#include "verstable.h"

_pack_posts pos_u;
#endif

static void
_pack_check_move(c3_w *dst_w, c3_w *src_w)
{
#ifdef PACK_CHECK
  u3_post src_p = u3a_outa(src_w);
  u3_post dst_p = u3a_outa(dst_w);
  _pack_posts_itr pit_u = vt_get(&pos_u, src_p);
  u3_assert( !vt_is_end(pit_u) );
  u3_assert( pit_u.data->val == dst_p );
#endif
}

static void
_pack_check_full(c3_w *dst_w, c3_w *src_w, _ca_frag* fag_u)
{
#ifdef PACK_CHECK
  c3_g      bit_g = fag_u->log_s - u3a_min_log;
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);

  _pack_check_move(dst_w, src_w);

  dst_w += hun_u->len_s * hun_u->hun_s;
  src_w += hun_u->len_s * hun_u->hun_s;

  for ( c3_s ful_s = 0; ful_s < hun_u->ful_s; ful_s++ ) {
    _pack_check_move(dst_w, src_w);
    dst_w += hun_u->len_s;
    src_w += hun_u->len_s;
  }
#endif
}

//  adapted from https://stackoverflow.com/a/27663998 and
//  https://gist.github.com/ideasman42/5921b0edfc6aa41a9ce0
//
static u3p(u3a_crag)
_sort_crag(u3p(u3a_crag) hed_p)
{
  c3_w bon_w, biz_w = 1; // block count, size
  u3p(u3a_crag) s_p, l_p, r_p, *tal_p;
  c3_w l_w, r_w;
  c3_t l_t, r_t;

  do {
    l_p = r_p = hed_p;

    hed_p = 0;
    tal_p = &hed_p;
    bon_w = 0;

    while ( l_p ) {
      r_w = biz_w;

      bon_w++;
      for ( l_w = 0; (l_w < biz_w) && r_p; l_w++) {
        r_p = u3to(u3a_crag, r_p)->nex_p;
      }

      l_t = (0 == l_w);
      r_t = (0 == r_w) || !r_p;

      while ( !l_t || !r_t ) {
        if ( r_t || (!l_t && (u3to(u3a_crag, l_p)->pag_w < u3to(u3a_crag, r_p)->pag_w)) ) {
          s_p = l_p;
          l_p = u3to(u3a_crag, l_p)->nex_p;
          l_w--;
          l_t = (0 == l_w);
        }
        else {
          s_p = r_p;
          r_p = u3to(u3a_crag, r_p)->nex_p;
          r_w--;
          r_t = (0 == r_w) || !r_p;
        }

        *tal_p = s_p;
        tal_p  = &(u3to(u3a_crag, s_p)->nex_p);
      }

      l_p = r_p;
    }

    *tal_p  = 0;
    biz_w <<= 1;
  }
  while ( bon_w > 1 );

  return hed_p;
}

static void
_pack_seek_hunks(void)
{
  const u3a_hunk_dose      *hun_u;
  u3_post     dir_p, nex_p, fre_p;
  u3p(u3a_crag)     *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w len_w, sum_w, i_w, *hap_w, *hum_w;
  c3_g bit_g = u3a_crag_no;
  u3a_crag   *pag_u;
  _ca_prag   *rag_u;

  struct {
    c3_w    pag_w;  //  previous page in class (original number)
    c3_s    fre_s;  //  previous hunks available
    c3_s    pos_s;  //  previous hunks used
    u3_post dir_p;
  } pre_u;

  while ( bit_g-- ) {
    if ( !HEAP.wee_p[bit_g] ) {
      continue;
    }

    //  XX investigate, should only be required on inner roads
    //
    HEAP.wee_p[bit_g] = _sort_crag(HEAP.wee_p[bit_g]);
    dir_p = HEAP.wee_p[bit_g];
    hun_u = &(u3a_Hunk[bit_g]);
    memset(&pre_u, 0, sizeof(pre_u));

    len_w = c3_wiseof(_ca_prag) + (3 * hun_u->map_s);

    while ( dir_p ) {
      pag_u = u3to(u3a_crag, dir_p);
      nex_p = pag_u->nex_p;
      pag_u->nex_p = 0;

      u3_assert(  (pre_u.pag_w < pag_u->pag_w)
               || (!pre_u.pag_w && !pag_u->pag_w) );

      rag_u = u3a_pack_alloc(len_w);
      hap_w = &(rag_u->mar_w[hun_u->map_s]);
      hum_w = &(hap_w[hun_u->map_s]);
      rag_u->log_s = hun_u->log_s;
      rag_u->pre_u.pag_w = pre_u.pag_w;
      rag_u->pre_u.fre_s = pre_u.fre_s;
      rag_u->pre_u.pos_s = pre_u.pos_s;

      memset(rag_u->mar_w, 0, hun_u->map_s << 2);

      for ( i_w = 0; i_w < hun_u->map_s; i_w++ ) {
        hap_w[i_w] = ~(pag_u->map_w[i_w]);
      }

      if ( hun_u->tot_s < 32 ) {
        hap_w[0] &= (1U << hun_u->tot_s) - 1;
      }

      sum_w = 0;
      for ( i_w = 0; i_w < hun_u->map_s; i_w++ ) {
        hum_w[i_w] = sum_w;
        sum_w += c3_pc_w(hap_w[i_w]);
      }

      u3a_Gack.buf_w[pag_u->pag_w] = ((c3_w*)rag_u - u3a_Gack.buf_w) | (1U << 31);

      c3_s pos_s = hun_u->ful_s - pag_u->fre_s;

      u3_assert( (pos_s + hun_u->hun_s) == sum_w );

      //  there is a previous page, and it will be full
      //
      if (  (pos_s == pre_u.fre_s)
         || (pre_u.dir_p && (pos_s > pre_u.fre_s)) )
      {
        u3a_crag *peg_u = u3to(u3a_crag, pre_u.dir_p);
        memset(peg_u->map_w, 0, hun_u->map_s << 2);
        peg_u->fre_s = 0;
      }

      //  current page will be empty
      //
      if ( pos_s <= pre_u.fre_s ) {
        rag_u->dir_p = 0;
        dir_u[pag_u->pag_w] = u3a_free_pg;
        fre_p = hun_u->hun_s ? 0 : dir_p;
      }
      else {
        fre_p = 0;
      }

      //  previous page is the same
      //
      if ( pos_s < pre_u.fre_s ) {
        pre_u.fre_s -= pos_s;
        pre_u.pos_s += pos_s;
      }
      //  current becomes previous
      //
      else if ( pos_s > pre_u.fre_s ) {
        rag_u->dir_p = dir_p;
        pos_s       -= pre_u.fre_s;
        pre_u.fre_s += pag_u->fre_s;
        pre_u.pag_w  = pag_u->pag_w;
        pre_u.pos_s  = pos_s;
        pre_u.dir_p  = dir_p;
      }
      //  no previous page
      //
      else {
        memset(&pre_u, 0, sizeof(pre_u));
      }

      if ( fre_p ) {
        _ifree(fre_p);
      }

      dir_p = nex_p;
    }

    HEAP.wee_p[bit_g] = pre_u.dir_p;

    if ( pre_u.dir_p ) {
      u3a_crag *peg_u = u3to(u3a_crag, pre_u.dir_p);
      c3_s      pos_s = pre_u.pos_s + hun_u->hun_s;
      c3_s      max_s = pos_s >> 5;

      memset(peg_u->map_w, 0, max_s << 2);
      peg_u->map_w[max_s++] = ~0U << (pos_s & 31);
      memset(&(peg_u->map_w[max_s]), 0xff, (hun_u->map_s - max_s) << 2);

      peg_u->fre_s = pre_u.fre_s;
    }
  }
}

static void
_pack_seek(void)
{
  u3p(u3a_crag)     *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
  c3_w blk_w, bit_w, fre_w = 0;
  u3_post     dir_p;

#ifdef PACK_CHECK
  vt_init(&pos_u);
#endif

  {
    u3_post   fre_p;
    u3a_dell *fre_u;

    while ( (fre_p = HEAP.fre_p) ) {
      fre_u = u3to(u3a_dell, fre_p);
      HEAP.fre_p = fre_u->nex_p;
      if ( HEAP.fre_p ) {
        u3to(u3a_dell, HEAP.fre_p)->pre_p = 0;
      }
      _ifree(fre_p);
    }
  }

  _pack_seek_hunks();

  for ( c3_w pag_w = 0; pag_w < HEAP.len_w; pag_w++ ) {
    if ( u3a_free_pg == (dir_p = dir_u[pag_w]) ) {
      fre_w++;
      continue;
    }

    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;
    u3a_Gack.pap_w[blk_w] |= 1U << bit_w;

    if ( u3a_rest_pg >= dir_p ) {
      u3a_Gack.buf_w[pag_w] = dir_p;
    }
    //  chunk page, not processed above (ie, full)
    //
    else if ( !u3a_Gack.buf_w[pag_w] ) {
      u3a_crag *pag_u = u3to(u3a_crag, dir_p);
      c3_g      bit_g = pag_u->log_s - u3a_min_log;
      const u3a_hunk_dose *hun_u = &(u3a_Hunk[bit_g]);

      _ca_frag *fag_u = u3a_pack_alloc(c3_wiseof(_ca_frag) + hun_u->map_s);
      u3a_Gack.buf_w[pag_w] = (c3_w*)fag_u - u3a_Gack.buf_w;

      fag_u->dir_p = dir_p;
      fag_u->log_s = pag_u->log_s;
      memset(fag_u->mar_w, 0, (c3_z)hun_u->map_s << 2);
    }
  }

  //  shrink page directory
  //
  {
    c3_w old_w = HEAP.siz_w >> u3a_page;
    c3_w dif_w = (HEAP.siz_w - (HEAP.len_w - fre_w)) >> u3a_page;
    c3_w pag_w = post_to_page(HEAP.pag_p);
    c3_w gap_w;

    for ( c3_w i_w = 0; i_w < dif_w; i_w++ ) {
      gap_w = pag_w + (HEAP.dir_ws * (old_w - i_w - 1));
      blk_w = gap_w >> 5;
      bit_w = gap_w & 31;
      u3a_Gack.buf_w[gap_w] = u3a_free_pg;
      u3a_Gack.pap_w[blk_w] &= ~(1U << bit_w);
    }

    HEAP.siz_w -= dif_w << u3a_page;
  }

  //  calculate cumulative sums of bitmap popcounts
  //
  {
    c3_w sum_w = 0, max_w = (HEAP.len_w + 31) >> 5;

    for ( c3_w i_w = 0; i_w < max_w; i_w++ ) {
      u3a_Gack.pum_w[i_w] = sum_w;
      sum_w += c3_pc_w(u3a_Gack.pap_w[i_w]);
    }
  }
}

static inline c3_w
_pack_relocate_page(c3_w pag_w)
{
  c3_w blk_w = pag_w >> 5;
  c3_w bit_w = pag_w & 31;
  c3_w top_w = u3a_Gack.pap_w[blk_w] & ((1U << bit_w) - 1);
  c3_w new_w = u3a_Gack.pum_w[blk_w];  //  XX blk_w - 1, since pum_w[0] is always 0?

  return new_w + c3_pc_w(top_w);
}

static inline u3_post
_pack_relocate_hunk(_ca_prag *rag_u, c3_w pag_w, c3_w pos_w)
{
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[rag_u->log_s - u3a_min_log]);
  c3_w  blk_w = pos_w >> 5;
  c3_w  bit_w = pos_w & 31;
  c3_w *hap_w = &(rag_u->mar_w[hun_u->map_s]);
  c3_w *hum_w = &(hap_w[hun_u->map_s]);
  c3_w  top_w = hap_w[blk_w] & ((1U << bit_w) - 1);
  c3_w  new_w = hum_w[blk_w];   //  XX blk_w - 1, since hum_w[0] is always 0?

  new_w += c3_pc_w(top_w);

  if ( new_w >= hun_u->hun_s ) {
    if ( new_w < (rag_u->pre_u.fre_s + hun_u->hun_s) ) {
      pag_w  = rag_u->pre_u.pag_w;
      new_w += rag_u->pre_u.pos_s;
    }
    else {
      new_w -= rag_u->pre_u.fre_s;
    }
  }

  {
    u3_post out_p = page_to_post(_pack_relocate_page(pag_w));
    out_p += new_w << rag_u->log_s;
    return out_p;
  }
}

static u3_post
_pack_relocate_mark(u3_post som_p, c3_t *fir_t)
{
  c3_w    pag_w = post_to_page(som_p);
  c3_w    dir_w = u3a_Gack.buf_w[pag_w];
  u3_post out_p = 0;
  c3_t    out_t = 0;
  c3_w    blk_w, bit_w;

  u3_assert(som_p);

  //  som_p is a one-or-more page allocation
  //
  if ( u3a_rest_pg >= dir_w ) {
    //  XX sanity
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;

    if ( !(u3a_Gack.bit_w[blk_w] & (1U << bit_w)) ) {
      u3a_Gack.bit_w[blk_w] |= (1U << bit_w);
      out_t = 1;
    }

    out_p = page_to_post(_pack_relocate_page(pag_w));
  }
  //  som_p is a chunk in a full page (map old pag_w to new)
  //
  else if ( !(dir_w >> 31) ) {
    //  XX sanity
    _ca_frag *fag_u = (void*)(u3a_Gack.buf_w + dir_w);
    c3_w  rem_w = som_p & ((1U << u3a_page) - 1);
    c3_w  pos_w = rem_w >> fag_u->log_s;  // XX c/b pos_s

    blk_w = pos_w >> 5;
    bit_w = pos_w & 31;

    if ( !(fag_u->mar_w[blk_w] & (1U << bit_w)) ) {
      fag_u->mar_w[blk_w] |= (1U << bit_w);
      out_t = 1;
    }

    out_p  = page_to_post(_pack_relocate_page(pag_w));
    out_p += rem_w;
  }
  //  som_p is a chunk in a partial page (map old pos_w to new)
  //
  else {
    _ca_prag *rag_u = (void*)(u3a_Gack.buf_w + (dir_w & ((1U << 31) - 1)));
    c3_w      pos_w = (som_p & ((1U << u3a_page) - 1)) >> rag_u->log_s;  // XX c/b pos_s

    //  XX sanity
    //  NB map inverted, free state updated

    blk_w = pos_w >> 5;
    bit_w = pos_w & 31;

    if ( !(rag_u->mar_w[blk_w] & (1U << bit_w)) ) {
      rag_u->mar_w[blk_w] |= (1U << bit_w);
      out_t = 1;
    }

    out_p = _pack_relocate_hunk(rag_u, pag_w, pos_w);
  }

  *fir_t = out_t;

#ifdef PACK_CHECK
  //  XX also consider fir_t?
  //
  c3_z siz_z = vt_size(&pos_u);
  _pack_posts_itr pit_u = vt_get_or_insert(&pos_u, som_p, out_p);
  u3_assert( !vt_is_end(pit_u) ); // OOM

  if ( vt_size(&pos_u) == siz_z ) {
    u3_assert( pit_u.data->val == out_p );
  }
#endif

  return out_p;
}

static u3_post
_pack_relocate(u3_post som_p)
{
  c3_w    pag_w = post_to_page(som_p);
  c3_w    dir_w = u3a_Gack.buf_w[pag_w];
  u3_post out_p;

  u3_assert(som_p);

  //  som_p is a one-or-more page allocation
  //
  if ( u3a_rest_pg >= dir_w ) {
    //  XX sanity
    out_p  = page_to_post(_pack_relocate_page(pag_w));
  }
  //  som_p is a chunk in a full page (map old pag_w to new)
  //
  else if ( !(dir_w >> 31) ) {
    //  XX sanity
    out_p  = page_to_post(_pack_relocate_page(pag_w));
    out_p += som_p & ((1U << u3a_page) - 1);
  }
  //  som_p is a chunk in a partial page (map old pos_w to new)
  //
  else {
    _ca_prag *rag_u = (void*)(u3a_Gack.buf_w + (dir_w & ((1U << 31) - 1)));
    c3_w      pos_w = (som_p & ((1U << u3a_page) - 1)) >> rag_u->log_s;  // XX c/b pos_s

    //  XX sanity
    //  NB map inverted, free state updated

    out_p = _pack_relocate_hunk(rag_u, pag_w, pos_w);
  }

#ifdef PACK_CHECK
  c3_z siz_z = vt_size(&pos_u);
  _pack_posts_itr pit_u = vt_get_or_insert(&pos_u, som_p, out_p);
  u3_assert( !vt_is_end(pit_u) ); // OOM

  if ( vt_size(&pos_u) == siz_z ) {
    u3_assert( pit_u.data->val == out_p );
  }
#endif

  return out_p;
}

static void
_pack_relocate_heap(void)
{
  //  this is possible if freeing unused u3a_crag's
  //  caused an entire hunk page to be free'd
  //
  if ( HEAP.fre_p ) {
    u3a_dell *fre_u;

    if ( !HEAP.cac_p ) {
      fre_u = u3to(u3a_dell, HEAP.fre_p);
      HEAP.cac_p = HEAP.fre_p;
      HEAP.fre_p = fre_u->nex_p;
    }

    u3_post *ref_p = &(HEAP.fre_p);

    while ( *ref_p ) {
      fre_u  = u3to(u3a_dell, *ref_p);
      //  NB: this corrupts pre_p
      //
      //    temporarily singly-linked,
      //    also avoids issues when freeing post pack
      //
      fre_u->pre_p = 0;
      *ref_p = _pack_relocate(*ref_p);
      ref_p  = &(fre_u->nex_p);
    }
  }

  HEAP.pag_p = _pack_relocate(HEAP.pag_p);

  if ( HEAP.cac_p ) {
    HEAP.cac_p = _pack_relocate(HEAP.cac_p);
  }

  for ( c3_g bit_g = 0; bit_g < u3a_crag_no; bit_g++ ) {
    if ( HEAP.wee_p[bit_g] ) {
      HEAP.wee_p[bit_g] = _pack_relocate(HEAP.wee_p[bit_g]);
    }
  }

  for ( c3_w pag_w = 0; pag_w < HEAP.len_w; pag_w++ ) {
    c3_w *wor_w, dir_w = u3a_Gack.buf_w[pag_w];

    if ( u3a_rest_pg < dir_w ) {
      dir_w &= (1U << 31) - 1;
      wor_w  = u3a_Gack.buf_w + dir_w;  //  (fag_u | rag_u)->dir_p
      if ( *wor_w ) {
        u3a_crag *pag_u = u3to(u3a_crag, *wor_w);
        pag_u->pag_w = _pack_relocate_page(pag_u->pag_w);
        *wor_w = _pack_relocate(*wor_w);
      }
    }
  }
}

static c3_i
_pack_move_chunks(c3_w pag_w, c3_w dir_w)
{
  _ca_prag *rag_u = (void*)(u3a_Gack.buf_w + dir_w);
  const u3a_hunk_dose *hun_u = &(u3a_Hunk[rag_u->log_s - u3a_min_log]);
  c3_w     *hap_w = &(rag_u->mar_w[hun_u->map_s]);
  c3_w      off_w = 1U << rag_u->log_s;
  c3_z      len_i = off_w << 2;
  c3_w     *src_w, *dst_w, new_w;
  c3_s      max_s,  fre_s, new_s, pos_s = hun_u->hun_s;

  src_w = u3to(c3_w, page_to_post(pag_w) + (pos_s << rag_u->log_s));

  max_s = 1U << (u3a_page - rag_u->log_s);

  if ( rag_u->pre_u.fre_s ) {
    new_w = _pack_relocate_page(rag_u->pre_u.pag_w);
    new_s = hun_u->hun_s + rag_u->pre_u.pos_s;
    fre_s = rag_u->pre_u.fre_s;

    dst_w = u3to(c3_w, page_to_post(new_w) + (new_s << rag_u->log_s));

    //  move up to [fre_s] chunks to (relocated) previous page
    //
    while ( (pos_s < max_s) && fre_s ) {
      if ( hap_w[pos_s >> 5] & (1U << (pos_s & 31)) ) {
        ASAN_UNPOISON_MEMORY_REGION(dst_w, len_i);
        _pack_check_move(dst_w, src_w);
        memcpy(dst_w, src_w, len_i);
        fre_s--;
        dst_w += off_w;
      }

      pos_s++;
      src_w += off_w;
    }

    //  advance src position past any free chunks
    //
    while ( (pos_s < max_s) ) {
      if ( hap_w[pos_s >> 5] & (1U << (pos_s & 31)) ) {
        break;
      }

      pos_s++;
      src_w += off_w;
    }

    if ( pos_s == max_s ) {
      return 0;
    }
  }

  new_w = _pack_relocate_page(pag_w);
  new_s = hun_u->hun_s;

  //  skip chunks that don't need to move
  //  (possible if a partial-chunk page is in the first
  //   contiguously used pages in the heap)
  //
  //     XX unlikely()
  //
  if ( new_w == pag_w ) {
    if ( new_s == pos_s ) {
      while (  (pos_s < max_s)
            && (hap_w[pos_s >> 5] & (1U << (pos_s & 31))) )
      {
        _pack_check_move(src_w, src_w);
        pos_s++;
        src_w += off_w;
      }

      new_s = pos_s++;
      src_w += off_w;
    }
  }
  //  relocate inline metadata
  //
  else if ( hun_u->hun_s ) {
    c3_w* soc_w = u3to(c3_w, page_to_post(pag_w));
    c3_w* doc_w = u3to(c3_w, page_to_post(new_w));

    ASAN_UNPOISON_MEMORY_REGION(doc_w, len_i * hun_u->hun_s);
    _pack_check_move(doc_w, soc_w);
    memcpy(doc_w, soc_w, len_i * hun_u->hun_s);

    // XX bump pos_s/src_w if !pos_s ?
  }

  u3_assert( (new_w < pag_w) || (new_s < pos_s) );

  dst_w = u3to(c3_w, page_to_post(new_w) + (new_s << rag_u->log_s));

  //  move remaining chunks to relocated page
  //
  while ( pos_s < max_s ) {
    if ( hap_w[pos_s >> 5] & (1U << (pos_s & 31)) ) {
      ASAN_UNPOISON_MEMORY_REGION(dst_w, len_i);
      _pack_check_move(dst_w, src_w);
      memcpy(dst_w, src_w, len_i);
      dst_w += off_w;
    }

    pos_s++;
    src_w += off_w;
  }

  //  XX assume(rag_u->dir_p)
  //
  u3a_Gack.buf_w[new_w] = rag_u->dir_p;
  return 1;
}

static void
_pack_move(void)
{
  c3_z   len_i = 1U << (u3a_page + 2);
  c3_ws off_ws = HEAP.dir_ws * (1U << u3a_page);
  c3_w   dir_w,  new_w, pag_w = 0;
  c3_w  *src_w, *dst_w;

  //  NB: these loops iterate over the temp page dir instead of
  //  the bitmap, as partial chunk pages can be marked free
  //

  //  skip pages that don't need to move,
  //  compacting partial chunk-pages in-place
  //
  while ( (pag_w < HEAP.len_w) && (dir_w = u3a_Gack.buf_w[pag_w]) ) {
    if ( u3a_rest_pg < dir_w ) {
      if ( !(dir_w >> 31) ) {
        u3a_Gack.buf_w[pag_w] = u3a_Gack.buf_w[dir_w]; // NB: fag_u->dir_p
      }
      else if ( !_pack_move_chunks(pag_w, dir_w & ((1U << 31) - 1)) ) {
        break;
      }
    }

    pag_w++;
  }

  new_w = pag_w++;
  dst_w = u3to(c3_w, page_to_post(new_w));
  src_w = u3to(c3_w, page_to_post(pag_w));

  while( pag_w < HEAP.len_w ) {
    //  XX assume(new_w < pag_w)
    //  XX assume(new_w == _pack_relocate_page(pag_w))
    //  XX assume(dst_w == u3to(c3_w, page_to_post(new_w)))
    //  XX assume(src_w == u3to(c3_w, page_to_post(pag_w)))
    //
    dir_w = u3a_Gack.buf_w[pag_w];

    if ( u3a_free_pg != dir_w ) {
      if ( (u3a_rest_pg >= dir_w) || !(dir_w >> 31) ) {
        ASAN_UNPOISON_MEMORY_REGION(dst_w, len_i);

        if ( u3a_head_pg == dir_w ) {
          _pack_check_move(dst_w, src_w);
        }
        else if ( u3a_rest_pg < dir_w ) {
          _pack_check_full(dst_w, src_w, (void*)&(u3a_Gack.buf_w[dir_w]));
        }

        memcpy(dst_w, src_w, len_i);
        u3a_Gack.buf_w[new_w] = (u3a_rest_pg >= dir_w)
                              ? dir_w
                              : u3a_Gack.buf_w[dir_w]; // NB: fag_u->dir_p
        new_w++;
        dst_w += off_ws;
      }
      else if ( _pack_move_chunks(pag_w, dir_w & ((1U << 31) - 1)) ) {
        new_w++;
        dst_w += off_ws;
      }
    }

    pag_w++;
    src_w += off_ws;
    fflush(stderr); // XX remove
  }

  _poison_words();

  {
    u3p(u3a_crag) *dir_u = u3to(u3p(u3a_crag), HEAP.pag_p);
    HEAP.len_w = new_w;
    memcpy(dir_u, u3a_Gack.buf_w, new_w << 2);
    memset(dir_u + new_w, 0, (HEAP.siz_w - new_w) << 2);
  }

  u3a_print_memory(stderr, "palloc: off-heap: used", u3a_Gack.len_w);
  u3a_print_memory(stderr, "palloc: off-heap: total", u3a_Gack.siz_w);

  {
    u3a_dell *fre_u;
    u3_post   fre_p;

    while ( (fre_p = HEAP.fre_p) ) {
      fre_u = u3to(u3a_dell, fre_p);
      HEAP.fre_p = fre_u->nex_p;
      _ifree(fre_p);
    }
  }

  HEAP.erf_p = 0;

#ifdef PACK_CHECK
  vt_cleanup(&pos_u);
#endif
}
