#include "v5.h"
    
/***  palloc.c
***/

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

#ifndef BASE_v5
  #define BASE_v5 u3R_v5->rut_p
#endif

#define page_to_post_v5(pag_w)  (BASE_v5 + (HEAP_v5.dir_ws * (c3_v5_ws)((c3_v5_w)(pag_w - HEAP_v5.off_ws) << u3a_v5_page)))
#define post_to_page_v5(som_p)  (_abs_dif_v5(som_p, (c3_v5_ws)BASE_v5 + HEAP_v5.off_ws) >> u3a_v5_page)

#ifndef HEAP_v5
  #define HEAP_v5 u3R_v5->hep
#endif

static u3_v5_post _imalloc_v5(c3_v5_w);
static void  _ifree_v5(u3_v5_post);

static __inline__ c3_v5_w
_abs_dif_v5(c3_v5_w a_w, c3_v5_w b_w)
{
  // c3_ds dif_ds = a_w - b_w;
  // c3_d  mas_d  = dif_ds >> 63;
  // return (dif_ds + mas_d) ^ mas_d;
  return (a_w > b_w ) ? a_w - b_w : b_w - a_w;
}

static void
_init_once(void)
{
  u3a_v5_hunk_dose *hun_u;
  c3_s mun_w = 0;

  for (c3_g bit_g = 0; bit_g < u3a_v5_crag_no; bit_g++ ) {
    hun_u = &(u3a_v5_Hunk[bit_g]);
    hun_u->log_s = bit_g + u3a_min_log;
    hun_u->len_s = 1U << hun_u->log_s;
    hun_u->tot_s = 1U << (u3a_v5_page - hun_u->log_s);
    hun_u->map_s = (hun_u->tot_s + 31) >> 5;
    hun_u->siz_s = c3_v5_wiseof(u3a_v5_crag) + hun_u->map_s - 1;

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
_drop(u3_v5_post som_p, c3_v5_w len_w)
{
  ASAN_UNPOISON_MEMORY_REGION(u3a_into(som_p), (c3_z)len_w << 2);
}

static void
_init_heap(void)
{
  u3v5p(u3a_v5_crag) *dir_u;

  if ( c3y == u3a_is_north(u3R) ) {
    HEAP_v5.dir_ws = 1;
    HEAP_v5.off_ws = 0;
  }
  else {
    HEAP_v5.dir_ws = -1;
    HEAP_v5.off_ws = -1;
  }

  assert( !(u3R_v5->hat_p & ((1U << u3a_v5_page) - 1)) );
  assert( u3R_v5->hat_p > u3a_rest_pg );
  assert( u3R_v5->hat_p == u3R_v5->rut_p );

  //  XX check for overflow

  HEAP_v5.pag_p  = u3R_v5->hat_p;
  HEAP_v5.pag_p += HEAP_v5.off_ws * (c3_v5_ws)(1U << u3a_v5_page);
  HEAP_v5.siz_w  = 1U << u3a_v5_page;
  HEAP_v5.len_w  = 1;

  u3R_v5->hat_p += HEAP_v5.dir_ws * (c3_v5_ws)(1U << u3a_v5_page);

  dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);

  memset(dir_u, 0, 1U << (u3a_v5_page + 2));
  dir_u[0] = u3a_head_pg;

#ifdef SANITY
  assert( 0 == post_to_page_v5(HEAP_v5.pag_p) );
  assert( HEAP_v5.pag_p == page_to_post_v5(0) );
  assert( HEAP_v5.len_w == post_to_page_v5(u3R_v5->hat_p + HEAP_v5.off_ws) );
#endif

  HEAP_v5.cac_p = _imalloc_v5(c3_v5_wiseof(u3a_dell));
}

static void
_extend_directory(c3_v5_w siz_w)  // num pages
{
  u3v5p(u3a_v5_crag) *dir_u, *old_u;
  u3_v5_post old_p = HEAP_v5.pag_p;
  old_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);

  c3_v5_w nex_w, pif_w, dif_w = 0, pag_w;
  do {
    pif_w = dif_w;
    nex_w = HEAP_v5.len_w + siz_w + dif_w; // num words
    nex_w += (1U << u3a_v5_page) - 1;
    nex_w &= ~((1U << u3a_v5_page) - 1);
    dif_w = nex_w >> u3a_v5_page; // new pages
  } while ( dif_w != pif_w );
  HEAP_v5.pag_p  = u3R_v5->hat_p;
  HEAP_v5.pag_p += HEAP_v5.off_ws * (c3_v5_ws)nex_w;
  u3R_v5->hat_p += HEAP_v5.dir_ws * (c3_v5_ws)nex_w; //  XX overflow

  //  XX depend on the guard page for these?
  //
  if ( 1 == HEAP_v5.dir_ws ) {
    if ( u3R_v5->hat_p >= u3R_v5->cap_p ) {
      u3m_bail(c3__meme); return;
    }
  }
  else {
    if ( u3R_v5->hat_p <= u3R_v5->cap_p ) {
      u3m_bail(c3__meme); return;
    }
  }

  dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);
  pag_w = post_to_page_v5(HEAP_v5.pag_p);

#ifdef SANITY
  assert( pag_w == (HEAP_v5.len_w - (HEAP_v5.off_ws * (dif_w - 1))) );
#endif

  {
    c3_z len_z = (c3_z)HEAP_v5.len_w << 2;
    c3_z dif_z = (c3_z)dif_w << 2;

    ASAN_UNPOISON_MEMORY_REGION(dir_u, (c3_z)nex_w << 2);

    memcpy(dir_u, old_u, len_z);

    dir_u[pag_w] = u3a_head_pg;

    for ( c3_v5_w i_w = 1; i_w < dif_w; i_w++ ) {
      dir_u[pag_w + (HEAP_v5.dir_ws * (c3_v5_ws)i_w)] = u3a_rest_pg;
    }

    memset((c3_y*)dir_u + len_z + dif_z, 0, ((c3_z)nex_w << 2) - len_z - dif_z);
  }

  HEAP_v5.len_w += dif_w;
  HEAP_v5.siz_w  = nex_w;

  _ifree_v5(old_p);

#ifdef SANITY
  assert( HEAP_v5.len_w == post_to_page_v5(u3R_v5->hat_p + HEAP_v5.off_ws) );
  assert( dir_u[HEAP_v5.len_w - 1] );
#endif
}

static u3_v5_post
_extend_heap(c3_v5_w siz_w)  // num pages
{
  u3_v5_post pag_p;

#ifdef SANITY
  assert( HEAP_v5.siz_w >= HEAP_v5.len_w );
#endif

  if ( (HEAP_v5.siz_w - HEAP_v5.len_w) < siz_w ) {
    _extend_directory(siz_w);
  }

  pag_p  = u3R_v5->hat_p;
  pag_p += HEAP_v5.off_ws * (c3_v5_ws)(siz_w << u3a_v5_page);

  u3R_v5->hat_p += HEAP_v5.dir_ws * (c3_v5_ws)(siz_w << u3a_v5_page);

  //  XX depend on the guard page for these?
  //
  if ( 1 == HEAP_v5.dir_ws ) {
    if ( u3R_v5->hat_p >= u3R_v5->cap_p ) {
      return u3m_bail(c3__meme);
    }
  }
  else {
    if ( u3R_v5->hat_p <= u3R_v5->cap_p ) {
      return u3m_bail(c3__meme);
    }
  }

  HEAP_v5.len_w += siz_w;

  ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), siz_w << (u3a_v5_page + 2));

#ifdef SANITY
  assert( HEAP_v5.len_w == post_to_page_v5(u3R_v5->hat_p + HEAP_v5.off_ws) );
#endif

  return pag_p;
}

static u3_v5_post
_alloc_pages_v5(c3_v5_w siz_w)  // num pages
{
  u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);
  u3a_dell*      fre_u = u3tn(u3a_dell, HEAP_v5.fre_p);
  u3a_dell*      del_u = NULL;
  c3_v5_w           pag_w = 0;

  while ( fre_u ) {
    //  XX sanity

    if ( fre_u->siz_w < siz_w ) {
      fre_u = u3tn(u3a_dell, fre_u->nex_p);
      continue;
    }
    else if ( fre_u->siz_w == siz_w ) {
      pag_w = fre_u->pag_w;

      if ( fre_u->nex_p ) {
        u3v5to(u3a_dell, fre_u->nex_p)->pre_p = fre_u->pre_p;
      }
      else {
        HEAP_v5.erf_p = fre_u->pre_p;
      }

      if ( fre_u->pre_p ) {
        u3v5to(u3a_dell, fre_u->pre_p)->nex_p = fre_u->nex_p;
      }
      else {
        HEAP_v5.fre_p = fre_u->nex_p;
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

  u3_v5_post pag_p;

  if ( pag_w ) {
    //  XX groace
    //
    pag_w -= HEAP_v5.off_ws * (siz_w - 1);
    pag_p  = page_to_post_v5(pag_w);

#ifdef SANITY
    assert( pag_w < HEAP_v5.len_w );

    //  XX sanity
    assert( u3a_free_pg == dir_u[pag_w] );
    for ( c3_v5_w i_w = 1; i_w < siz_w; i_w++ ) {
      assert( u3a_free_pg == dir_u[pag_w + (HEAP_v5.dir_ws * (c3_v5_ws)i_w)] );
    }
#endif
  }
  else {
    pag_p = _extend_heap(siz_w);
    pag_w = post_to_page_v5(pag_p);
    dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);

#ifdef SANITY
    assert( pag_w < HEAP_v5.len_w );
    assert( (pag_w + ((HEAP_v5.off_ws + 1) * siz_w) - HEAP_v5.off_ws) == HEAP_v5.len_w );
#endif
  }

  dir_u[pag_w] = u3a_head_pg;

  for ( c3_v5_w i_w = 1; i_w < siz_w; i_w++ ) {
    dir_u[pag_w + (HEAP_v5.dir_ws * (c3_v5_ws)i_w)] = u3a_rest_pg;
  }

  //  XX junk

  ASAN_UNPOISON_MEMORY_REGION(u3a_into(pag_p), siz_w << (u3a_v5_page + 2));

  if ( del_u ) {
    if ( !HEAP_v5.cac_p ) {
      HEAP_v5.cac_p = u3of(u3a_dell, del_u);
    }
    else {
      _ifree_v5(u3of(u3a_dell, del_u));
    }
  }

#ifdef SANITY
  assert( HEAP_v5.len_w == post_to_page_v5(u3R_v5->hat_p + HEAP_v5.off_ws) );
  assert( dir_u[HEAP_v5.len_w - 1] );
#endif

  return pag_p;
}

static void
_rake_chunks(c3_v5_w len_w, c3_v5_w max_w, c3_t rak_t, c3_v5_w* out_w, u3_v5_post* out_p)
{
  c3_g      bit_g = (c3_g)c3_bits_word(len_w - 1) - u3a_min_log;  // 0-9, inclusive
  const u3a_v5_hunk_dose *hun_u = &(u3a_v5_Hunk[bit_g]);
  u3_v5_post   pag_p = HEAP_v5.wee_p[bit_g];
  u3a_v5_crag *pag_u;
  c3_v5_w      hav_w = *out_w;

  if ( rak_t ) {
    c3_v5_w    *map_w;
    c3_g    pos_g;
    u3_v5_post bas_p;
    c3_v5_w    off_w;

    while ( pag_p ) {
      pag_u = u3v5to(u3a_v5_crag, pag_p);
      bas_p = page_to_post_v5(pag_u->pag_w);
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

        HEAP_v5.wee_p[bit_g] = pag_p;
        *out_w = hav_w;
        return;
      }

      ASAN_UNPOISON_MEMORY_REGION(u3a_into(bas_p), 1U << (u3a_v5_page + 2));

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
        HEAP_v5.wee_p[bit_g] = pag_p;
        *out_w = hav_w;
        return;
      }
    }

    HEAP_v5.wee_p[bit_g] = 0;
  }

  //  XX s/b ful_s
  if ( hun_u->tot_s > (max_w - hav_w) ) {
    *out_w = hav_w;
    return;
  }

  //  manually inlined _make_chunks_v5(), storing each chunk-post to [*out_p]
  //
  {
    u3_v5_post hun_p;
    c3_v5_w    pag_w;

    pag_p = _alloc_pages_v5(1);
    pag_w = post_to_page_v5(pag_p);
    hun_p = ( hun_u->hun_s ) ? pag_p : _imalloc_v5(hun_u->siz_s);
    pag_u = u3v5to(u3a_v5_crag, hun_p);
    pag_u->pag_w = pag_w;
    pag_u->log_s = hun_u->log_s;
    pag_u->fre_s = 0;
    pag_u->nex_p = 0;
    //  initialize bitmap (zeros, none free)
    //
    memset(pag_u->map_w, 0, (c3_z)hun_u->map_s << 2);

    {
      u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);
      dir_u[pag_w] = hun_p;
    }

    for ( c3_s i_s = hun_u->hun_s; i_s < hun_u->tot_s; i_s++ ) {
      out_p[hav_w++] = pag_p + (i_s << pag_u->log_s);
    }

    *out_w = hav_w;
  }
}

static u3_v5_post
_make_chunks_v5(c3_g bit_g)  // 0-9, inclusive
{
  const u3a_v5_hunk_dose *hun_u = &(u3a_v5_Hunk[bit_g]);
  u3a_v5_crag      *pag_u;
  u3_v5_post hun_p, pag_p = _alloc_pages_v5(1);
  c3_v5_w    pag_w = post_to_page_v5(pag_p);

  ASAN_POISON_MEMORY_REGION(u3a_into(pag_p), 1U << (u3a_v5_page + 2));

  if ( hun_u->hun_s ) {
    hun_p = pag_p;
    ASAN_UNPOISON_MEMORY_REGION(u3a_into(pag_p), (c3_z)hun_u->hun_s << (hun_u->log_s + 2));
  }
  else {
    hun_p = _imalloc_v5(hun_u->siz_s);
  }

  pag_u = u3v5to(u3a_v5_crag, hun_p);
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
    u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);
    dir_u[pag_w] = hun_p;
  }

  pag_u->nex_p = HEAP_v5.wee_p[bit_g];
  HEAP_v5.wee_p[bit_g] = hun_p;

  return hun_p;
}

static u3_v5_post
_alloc_words_v5(c3_v5_w len_w)  //  4-2.048, inclusive
{
  c3_g      bit_g = (c3_g)c3_v5_bits_word(len_w - 1) - u3a_v5_min_log;  // 0-9, inclusive
  const u3a_v5_hunk_dose *hun_u = &(u3a_v5_Hunk[bit_g]);
  u3a_v5_crag *pag_u;
  c3_v5_w     *map_w;
  c3_g      pos_g;

  if ( !HEAP_v5.wee_p[bit_g] ) {
    pag_u = u3v5to(u3a_v5_crag, _make_chunks_v5(bit_g));
  }
  else {
    pag_u = u3v5to(u3a_v5_crag, HEAP_v5.wee_p[bit_g]);
    //  XX sanity

    if ( 1 == pag_u->fre_s ) {
      HEAP_v5.wee_p[bit_g] = pag_u->nex_p;
      pag_u->nex_p = 0;
    }
  }

  pag_u->fre_s--;
  map_w = pag_u->map_w;
  while ( !*map_w ) { map_w++; }

  pos_g   = c3_tz_w(*map_w);
  *map_w &= ~(1U << pos_g);

  {
    u3_v5_post out_p, bas_p = page_to_post_v5(pag_u->pag_w);
    c3_v5_w    off_w = (map_w - pag_u->map_w) << 5;

    out_p = bas_p + ((off_w + pos_g) << pag_u->log_s);
    ASAN_UNPOISON_MEMORY_REGION(u3a_into(out_p), hun_u->len_s << 2);
    //  XX poison suffix

    return out_p;
  }
}

static u3_v5_post
_imalloc_v5(c3_v5_w len_w)
{
  if ( len_w > (1U << (u3a_v5_page - 1)) ) {
    len_w  += (1U << u3a_v5_page) - 1;
    len_w >>= u3a_v5_page;
    //  XX poison suffix
    return _alloc_pages_v5(len_w);
  }

  return _alloc_words_v5(c3_max(len_w, u3a_v5_minimum));
}

static c3_v5_w
_free_pages_v5(u3_v5_post som_p, c3_v5_w pag_w, u3_v5_post dir_p)
{
  u3a_v5_dell *cac_u, *fre_u, *del_u = NULL;
  c3_v5_w      nex_w,  siz_w = 1;
  u3_v5_post   fre_p;

  if ( u3a_v5_free_pg == dir_p ) {
    fprintf(stderr, "\033[31m"
                    "palloc: double free page som_p=0x%x pag_w=%u\r\n"
                    "\033[0m",
                    som_p, pag_w);
    u3_assert(!"loom: corrupt"); return 0;
  }

  if ( u3a_v5_head_pg != dir_p ) {
    fprintf(stderr, "\033[31m"
                    "palloc: wrong page som_p=0x%x dir_p=0x%x\r\n"
                    "\033[0m",
                    som_p, dir_p);
    u3_assert(!"loom: corrupt"); return 0;
  }

  if ( som_p & ((1U << u3a_v5_page) - 1) ) {
    fprintf(stderr, "\033[31m"
                    "palloc: bad page alignment som_p=0x%x\r\n"
                    "\033[0m",
                    som_p);
    u3_assert(!"loom: corrupt"); return 0;
  }

  {
    u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);

    dir_u[pag_w] = u3a_v5_free_pg;

    //  head-page 0 in a south road can only have a size of 1
    //
    if ( pag_w || !HEAP_v5.off_ws ) {
      while( dir_u[pag_w + (HEAP_v5.dir_ws * (c3_v5_ws)siz_w)] == u3a_v5_rest_pg ) {
        dir_u[pag_w + (HEAP_v5.dir_ws * (c3_v5_ws)siz_w)] = u3a_v5_free_pg;
        siz_w++;
      }
    }
  }

  //  XX groace
  //
  if ( HEAP_v5.off_ws ) {
    nex_w = pag_w + 1;
    pag_w = nex_w - siz_w;
  }
  else {
    nex_w = pag_w + siz_w;
  }

#ifdef SANITY
  assert( pag_w < HEAP_v5.len_w );
  assert( HEAP_v5.len_w == post_to_page_v5(u3R_v5->hat_p + HEAP_v5.off_ws) );
#endif

  if ( nex_w == HEAP_v5.len_w ) {
    u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);
    c3_v5_w           wiz_w = siz_w;

    // check if prior pages are already free
    //
    if ( !dir_u[HEAP_v5.len_w - (siz_w + 1)] ) {
      assert( HEAP_v5.erf_p );
      fre_u = u3v5to(u3a_v5_dell, HEAP_v5.erf_p);
      assert( (fre_u->pag_w + fre_u->siz_w) == pag_w );

      if ( fre_u->pre_p ) {
        HEAP_v5.erf_p = fre_u->pre_p;
        u3v5to(u3a_v5_dell, fre_u->pre_p)->nex_p = 0;
      }
      else {
        HEAP_v5.fre_p = HEAP_v5.erf_p = 0;
      }

      pag_w  = fre_u->pag_w;  // NB: clobbers
      siz_w += fre_u->siz_w;

      // XX groace
      //
      pag_w -= HEAP_v5.off_ws * (siz_w - 1);
      som_p  = page_to_post_v5(pag_w);
    }
    else {
      fre_u = NULL;
    }

    ASAN_UNPOISON_MEMORY_REGION(u3a_v5_into(som_p), siz_w << (u3a_v5_page + 2));
    u3R_v5->hat_p -= HEAP_v5.dir_ws * (c3_v5_ws)(siz_w << u3a_v5_page);
    HEAP_v5.len_w -= siz_w;

    // fprintf(stderr, "shrink heap 0x%x 0x%x %u:%u (%u) 0x%x\r\n",
    //                 som_p, som_p + (siz_w << u3a_v5_page), pag_w, wiz_w, siz_w, u3R_v5->hat_p);

#ifdef SANITY
    assert( HEAP_v5.len_w == post_to_page_v5(u3R_v5->hat_p + HEAP_v5.off_ws) );
    assert( dir_u[HEAP_v5.len_w - 1] );
#endif

    if ( fre_u ) {
      _ifree_v5(u3of(u3a_v5_dell, fre_u));
    }

    return wiz_w;
  }

  //  XX madv_free

  ASAN_POISON_MEMORY_REGION(u3a_v5_into(som_p), siz_w << (u3a_v5_page + 2));

  //  XX add temporary freelist entry?
  // if (  !HEAP_v5.cac_p && !HEAP_v5.fre_p
  //    && !HEAP_v5.wee_p[(c3_bits_word(c3_v5_wiseof(*cac_u) - 1) - u3a_v5in_log)] )
  // {
  //   fprintf(stderr, "palloc: growing heap to free pages\r\n");
  // }

  if ( !HEAP_v5.cac_p ) {
    fre_p = _imalloc_v5(c3_v5_wiseof(*cac_u));
  }
  else {
    fre_p = HEAP_v5.cac_p;
    HEAP_v5.cac_p = 0;
  }

  cac_u = u3v5to(u3a_v5_dell, fre_p);
  cac_u->pag_w = pag_w;
  cac_u->siz_w = siz_w;

  if ( !(fre_u = u3tn(u3a_v5_dell, HEAP_v5.fre_p)) ) {
    // fprintf(stderr, "free pages 0x%x (%u) via 0x%x\r\n", som_p, siz_w, HEAP_v5.cac_p);
    cac_u->nex_p = 0;
    cac_u->pre_p = 0;
    HEAP_v5.fre_p = HEAP_v5.erf_p = fre_p;
    fre_p = 0;
  }
  else {
    c3_v5_w fex_w;

    while (  ((fex_w = fre_u->pag_w + fre_u->siz_w) < pag_w)
          && fre_u->nex_p )
    {
      fre_u = u3v5to(u3a_v5_dell, fre_u->nex_p);
    }

    if ( fre_u->pag_w > nex_w ) {        //  insert before
      cac_u->nex_p = u3of(u3a_v5_dell, fre_u);
      cac_u->pre_p = fre_u->pre_p;

      fre_u->pre_p = fre_p;

      //  XX sanity
      if ( cac_u->pre_p ) {
        u3v5to(u3a_v5_dell, cac_u->pre_p)->nex_p = fre_p;
      }
      else {
        HEAP_v5.fre_p = fre_p;
      }

      fre_p = 0;
    }
    else if ( fex_w == pag_w ) {  //  append to entry
      fre_u->siz_w += siz_w;

      //  coalesce with next entry
      //
      if (  fre_u->nex_p
         && ((fex_w + siz_w) == u3v5to(u3a_v5_dell, fre_u->nex_p)->pag_w) )
      {
        del_u = u3v5to(u3a_v5_dell, fre_u->nex_p);
        fre_u->siz_w += del_u->siz_w;
        fre_u->nex_p  = del_u->nex_p;

        //  XX sanity
        if ( del_u->nex_p ) {
          u3v5to(u3a_v5_dell, del_u->nex_p)->pre_p = u3of(u3a_v5_dell, fre_u);
        }
        else {
          HEAP_v5.erf_p = u3of(u3a_v5_dell, fre_u);
        }
      }
    }
    else if ( fre_u->pag_w == nex_w ) {  //  prepend to entry
      fre_u->siz_w += siz_w;
      fre_u->pag_w  = pag_w;
    }
    else if ( !fre_u->nex_p ) {          //  insert after
      cac_u->nex_p = 0;
      cac_u->pre_p = u3of(u3a_v5_dell, fre_u);
      HEAP_v5.erf_p = fre_u->nex_p = fre_p;
      fre_p = 0;
    }
    else {
      fprintf(stderr, "\033[31m"
                      "palloc: free list hosed at som_p=0x%x pag=%u len=%u\r\n"
                      "\033[0m",
                      (u3_v5_post)u3of(u3a_v5_dell, fre_u), fre_u->pag_w, fre_u->siz_w);
      u3_assert(!"loom: corrupt"); return 0;
    }
  }

  if ( fre_p ) {
    if ( !HEAP_v5.cac_p ) {
      HEAP_v5.cac_p = fre_p;
    }
    else {
      _ifree_v5(fre_p);
    }

    if ( del_u ) {
      _ifree_v5(u3of(u3a_v5_dell, del_u));
    }
  }

  return siz_w;
}

static void
_free_words_v5(u3_v5_post som_p, c3_v5_w pag_w, u3_v5_post dir_p)
{
  u3a_v5_crag *pag_u = u3v5to(u3a_v5_crag, dir_p);
  u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);

#ifdef SANITY
  assert( page_to_post_v5(pag_u->pag_w) == (som_p & ~((1U << u3a_v5_page) - 1)) );
  assert( pag_u->log_s < u3a_v5_page );
#endif

  c3_g bit_g = pag_u->log_s - u3a_v5_min_log;
  c3_v5_w pos_w = (som_p & ((1U << u3a_v5_page) - 1)) >> pag_u->log_s;
  const u3a_v5_hunk_dose *hun_u = &(u3a_v5_Hunk[bit_g]);

  if ( som_p & (hun_u->len_s - 1) ) {
    fprintf(stderr, "\033[31m"
                    "palloc: bad alignment som_p=0x%x pag=%u cag=0x%x len_s=%u\r\n"
                    "\033[0m",
                    som_p, post_to_page_v5(som_p), dir_p, hun_u->len_s);
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

  ASAN_POISON_MEMORY_REGION(u3a_v5_into(som_p), hun_u->len_s << 2);

  {
    u3_v5_post *bit_p = &(HEAP_v5.wee_p[bit_g]);
    u3a_v5_crag *bit_u, *nex_u;

    if ( 1 == pag_u->fre_s ) {
      //  page newly non-full, link
      //

      if ( &(u3H_v5->rod_u) == u3R_v5 ) {
        while ( *bit_p ) {
          bit_u = u3v5to(u3a_v5_crag, *bit_p);
          nex_u = u3tn(u3a_v5_crag, bit_u->nex_p);

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
        bit_u = u3v5to(u3a_v5_crag, *bit_p);
        bit_p = &(bit_u->nex_p);

        //  XX sanity: must be in list
      }

      *bit_p = pag_u->nex_p;

      dir_u[pag_u->pag_w] = u3a_v5_head_pg;
      som_p = page_to_post_v5(pag_u->pag_w); // NB: clobbers
      if ( som_p != dir_p ) {
        _ifree_v5(dir_p);
      }
      _ifree_v5(som_p);
    }
  }
}

static void
_ifree_v5(u3_v5_post som_p)
{
  u3v5p(u3a_v5_crag) *dir_u = u3v5to(u3v5p(u3a_v5_crag), HEAP_v5.pag_p);
  c3_v5_w pag_w = post_to_page_v5(som_p);

  if ( pag_w >= HEAP_v5.len_w ) {
    fprintf(stderr, "\033[31m"
                    "palloc: page out of heap som_p=0x%x pag_w=%u len_w=%u\r\n"
                    "\033[0m",
                    som_p, pag_w, HEAP_v5.len_w);
    u3_assert(!"loom: corrupt"); return;
  }

  u3_v5_post dir_p = dir_u[pag_w];

  if ( dir_p <= u3a_v5_rest_pg ) {
    (void)_free_pages_v5(som_p, pag_w, dir_p);
  }
  else {
    _free_words_v5(som_p, pag_w, dir_p);
  }
}

/***  allocate.c
***/

u3a_v5_road* u3a_v5_Road;
u3v_v5_home* u3v_v5_Home;
u3a_v5_hunk_dose u3a_v5_Hunk[u3a_v5_crag_no];
c3_v5_w u3a_v5_to_pug(c3_v5_w off);
c3_v5_w u3a_v5_to_pom(c3_v5_w off);

void
u3a_v5_drop(const u3a_v5_pile* pil_u);
void*
u3a_v5_peek(const u3a_v5_pile* pil_u);
void*
u3a_v5_pop(const u3a_v5_pile* pil_u);
void*
u3a_v5_push(const u3a_v5_pile* pil_u);
c3_o
u3a_v5_pile_done(const u3a_v5_pile* pil_u);

static void
_me_gain_use_v5(u3_v5_noun dog)
{
  u3a_v5_noun* box_u = u3a_v5_to_ptr(dog);

  if ( 0x7fffffff == box_u->use_w ) {
    u3m_bail(c3__fail);
  }
  else {
    if ( box_u->use_w == 0 ) {
      u3m_bail(c3__foul);
    }
    box_u->use_w += 1;

#ifdef U3_MEMORY_DEBUG
    //  enable to (maybe) help track down leaks
    //
    // if ( u3_Code && !box_u->cod_w ) { box_u->cod_w = u3_Code; }
#endif
  }
}

static u3_v5_noun
_me_gain_north_v5(u3_v5_noun dog)
{
  if ( c3y == u3a_v5_north_is_senior(u3R, dog) ) {
    /*  senior pointers are not refcounted
    */
    return dog;
  }
  else {
    /* junior nouns are disallowed
    */
    u3_assert(!_(u3a_v5_north_is_junior(u3R_v5, dog)));

    /* normal pointers are refcounted
    */
    _me_gain_use_v5(dog);
    return dog;
  }
}

static u3_v5_noun
_me_gain_south_v5(u3_v5_noun dog)
{
  if ( c3y == u3a_v5_south_is_senior(u3R_v5, dog) ) {
    /*  senior pointers are not refcounted
    */
    return dog;
  }
  else {
    /* junior nouns are disallowed
    */
    u3_assert(!_(u3a_v5_south_is_junior(u3R_v5, dog)));

    /* normal nouns are refcounted
    */
    _me_gain_use_v5(dog);
    return dog;
  }
}

u3_v5_noun
u3a_v5_gain(u3_v5_noun som)
{
  u3_assert(u3_v5_none != som);

  if ( !_(u3a_v5_is_cat(som)) ) {
    som = _(u3a_v5_is_north(u3R_v5))
              ? _me_gain_north_v5(som)
              : _me_gain_south_v5(som);
  }

  return som;
}

void*
u3a_v5_walloc(c3_v5_w len_w)
{
  return u3a_v5_into(_imalloc_v5(len_w));
}

void
u3a_v5_pile_prep(u3a_v5_pile* pil_u, c3_v5_w len_w)
{
  //  frame size, in words
  //
  c3_v5_w wor_w = (len_w + 3) >> 2;
  c3_o nor_o = u3a_v5_is_north(u3R_v5);

  pil_u->mov_ws = (c3y == nor_o) ? -wor_w :  wor_w;
  pil_u->off_ws = (c3y == nor_o) ?      0 : -wor_w;
  pil_u->top_p  = u3R_v5->cap_p;
}

void
u3a_v5_wfree(void* tox_v)
{
  if ( tox_v ) {
    _ifree_v5(u3a_v5_outa(tox_v));
  }
}


void
u3a_v5_cfree(c3_v5_w* cel_w)
{
  u3_v5_post *cel_p;

  if ( u3R_v5->cel.cel_p ) {
    if ( u3R_v5->cel.hav_w < (1U << u3a_v5_page) ) {
      cel_p = u3v5to(u3_v5_post, u3R_v5->cel.cel_p);
      cel_p[u3R_v5->cel.hav_w++] = u3a_v5_outa(cel_w);
      return;
    }
  }

  u3a_v5_wfree(cel_w);
}

static void
_me_lose_north_v5(u3_v5_noun dog)
{
top:
  if ( c3y == u3a_v5_north_is_normal(u3R_v5, dog) ) {
    u3a_v5_noun* box_u = u3a_v5_to_ptr(dog);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_v5_is_pom(dog)) ) {
          u3a_v5_cell* dog_u = (void*)box_u;
          u3_v5_noun   h_dog = dog_u->hed;
          u3_v5_noun   t_dog = dog_u->tel;

          if ( !_(u3a_v5_is_cat(h_dog)) ) {
            _me_lose_north_v5(h_dog);
          }
          u3a_v5_cfree((c3_v5_w*)dog_u);
          if ( !_(u3a_v5_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_v5_wfree(box_u);
        }
      }
    }
  }
}

static void
_me_lose_south_v5(u3_v5_noun dog)
{
top:
  if ( c3y == u3a_v5_south_is_normal(u3R, dog) ) {
    u3a_v5_noun* box_u = u3a_v5_to_ptr(dog);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_v5_is_pom(dog)) ) {
          u3a_v5_cell* dog_u = (void*)box_u;
          u3_v5_noun   h_dog = dog_u->hed;
          u3_v5_noun   t_dog = dog_u->tel;

          if ( !_(u3a_v5_is_cat(h_dog)) ) {
            _me_lose_south_v5(h_dog);
          }
          u3a_v5_cfree((c3_v5_w*)dog_u);
          if ( !_(u3a_v5_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_v5_wfree(box_u);
        }
      }
    }
  }
}

void
u3a_v5_lose(u3_v5_noun som)
{
  if ( !_(u3a_v5_is_cat(som)) ) {
    if ( _(u3a_v5_is_north(u3R_v5)) ) {
      _me_lose_north_v5(som);
    } else {
      _me_lose_south_v5(som);
    }
  }
}

#define SWAP_V5(l, r)    \
  do { typeof(l) t = l; l = r; r = t; } while (0)

static inline c3_o
_ca_wed_our_v5(u3_v5_noun *restrict a, u3_v5_noun *restrict b)
{
  c3_t asr_t = ( c3y == u3a_v5_is_senior(u3R_v5, *a) );
  c3_t bsr_t = ( c3y == u3a_v5_is_senior(u3R_v5, *b) );

  if ( asr_t == bsr_t ) {
    //  both [a] and [b] are senior; we can't unify on u3R
    //
    if ( asr_t ) return c3n;

    //  both are on u3R; keep the deeper address
    //  (and gain a reference)
    //
    //    (N && <) || (S && >)
    //    XX consider keeping higher refcount instead
    //
    if ( (*a > *b) == (c3y == u3a_v5_is_north(u3R_v5)) ) SWAP_V5(a, b);

    _me_gain_use_v5(*a);
  }
  //  one of [a] or [b] are senior; keep it
  //
  else if ( !asr_t ) SWAP_V5(a, b);

  u3v5z(*b);
  *b = *a;
  return c3y;
}

static c3_o
_ca_wed_you_v5(u3a_v5_road* rod_u, u3_v5_noun *restrict a, u3_v5_noun *restrict b)
{
  //  XX assume( rod_u != u3R_v5 )
  c3_t asr_t = ( c3y == u3a_v5_is_senior(rod_u, *a) );
  c3_t bsr_t = ( c3y == u3a_v5_is_senior(rod_u, *b) );

  if ( asr_t == bsr_t ) {
    //  both [a] and [b] are senior; we can't unify on [rod_u]
    //
    if ( asr_t ) return c3n;

    //  both are on [rod_u]; keep the deeper address
    //  (and gain a reference)
    //
    //    (N && <) || (S && >)
    //    XX consider keeping higher refcount instead
    //
    if ( (*a > *b) == (c3y == u3a_is_north(rod_u)) ) SWAP_V5(a, b);

    _me_gain_use_v5(*a);
  }
  //  one of [a] or [b] are senior; keep it
  //
  else if ( !asr_t ) SWAP_V5(a, b);

  *b = *a;
  return c3y;
}

#undef SWAP_V5

void
u3a_v5_wed(u3_v5_noun *restrict a, u3_v5_noun *restrict b)
{
  //  XX assume( *a != *b )
  u3_v5_road* rod_u = u3R_v5;
  c3_o     wed_o;

  if ( rod_u->kid_p ) return;

  wed_o = _ca_wed_our_v5(a, b);

  while (  (c3n == wed_o)
        && rod_u->par_p
        && (&u3H_v5->rod_u != (rod_u = u3v5to(u3_v5_road, rod_u->par_p))) )
  {
    wed_o = _ca_wed_you_v5(rod_u, a, b);
  }
}

/***  hashtable.c
***/
#define CUT_END_V5(a_w, b_w) ((a_w) & (((c3_v5_w)1 << (b_w)) - 1))
#define BIT_SET_V5(a_w, b_w) ((a_w) & ((c3_v5_w)1 << (b_w)))

static inline u3_v5_noun
_h_need_v5(u3_v5_noun som)
{
  u3_assert( _(u3a_v5_is_cell(som)) );
  return ((u3a_v5_cell *)u3a_v5_to_ptr(som))->hed;
}


static inline u3_v5_noun
_t_need_v5(u3_v5_noun som)
{
  u3_assert( _(u3a_v5_is_cell(som)) );
  return ((u3a_v5_cell *)u3a_v5_to_ptr(som))->tel;
}

u3v5p(u3h_v5_root)
u3h_v5_new_cache(c3_v5_w max_w)
{
  u3h_v5_root*     har_u = u3a_v5_walloc(c3_v5_wiseof(u3h_v5_root));
  u3v5p(u3h_v5_root) har_p = u3of(u3h_v5_root, har_u);
  c3_v5_w        i_w;

  har_u->max_w       = max_w;
  har_u->use_w       = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    har_u->sot_w[i_w] = 0;
  }
  return har_p;
}

u3v5p(u3h_v5_root)
u3h_v5_new(void)
{
  return u3h_v5_new_cache(0);
}

static c3_v5_w
_ch_popcount_v5(c3_v5_w num_w)
{
  return c3_v5_pc_w(num_w);
}

static u3_v5_weak
_ch_buck_git(u3h_v5_buck* hab_u, u3_v5_noun key)
{
  c3_v5_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_v5_noun kev = u3h_v5_slot_to_noun(hab_u->sot_w[i_w]);
    if ( _(u3r_sing(key, _h_need_v5(kev))) ) {
      return _t_need_v5(kev);
    }
  }
  return u3_v5_none;
}

static u3_v5_weak
_ch_buck_git_v5(u3h_v5_buck* hab_u, u3_v5_noun key)
{
  c3_v5_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_v5_noun kev = u3h_v5_slot_to_noun(hab_u->sot_w[i_w]);
    if ( _(u3r_sing(key, _h_need_v5(kev))) ) {
      return _t_need_v5(kev);
    }
  }
  return u3_v5_none;
}

static u3_v5_weak
_ch_node_git_v5(u3h_v5_node* han_u, c3_v5_w lef_w, c3_v5_w rem_w, u3_v5_noun key)
{
  c3_v5_w bit_w, map_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = CUT_END_V5(rem_w, lef_w);
  map_w = han_u->map_w;

  if ( !BIT_SET_V5(map_w, bit_w) ) {
    return u3_v5_none;
  }
  else {
    c3_v5_w inx_w = _ch_popcount_v5(CUT_END_V5(map_w, bit_w));
    c3_v5_w sot_w = han_u->sot_w[inx_w];

    if ( _(u3h_v5_slot_is_noun(sot_w)) ) {
      u3_v5_noun kev = u3h_v5_slot_to_noun(sot_w);

      if ( _(u3r_v5_sing(key, _h_need_v5(kev))) ) {
        return _t_need_v5(kev);
      }
      else {
        return u3_v5_none;
      }
    }
    else {
      void* hav_v = u3h_v5_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        return _ch_buck_git_v5(hav_v, key);
      }
      else return _ch_node_git_v5(hav_v, lef_w, rem_w, key);
    }
  }
}

u3_v5_weak
u3h_v5_git(u3v5p(u3h_v5_root) har_p, u3_v5_noun key)
{
  u3h_v5_root* har_u = u3v5to(u3h_v5_root, har_p);
  c3_v5_w      mug_w = u3r_v5_mug(key);
  c3_v5_w      inx_w = (mug_w >> 25);
  c3_v5_w      rem_w = CUT_END_V5(mug_w, 25);
  c3_v5_w      sot_w = har_u->sot_w[inx_w];

  if ( _(u3h_v5_slot_is_null(sot_w)) ) {
    return u3_v5_none;
  }
  else if ( _(u3h_v5_slot_is_noun(sot_w)) ) {
    u3_v5_noun kev = u3h_slot_to_noun(sot_w);

    if ( _(u3r_v5_sing(key, _h_need_v5(kev))) ) {
      har_u->sot_w[inx_w] = u3h_v5_noun_be_warm(sot_w);
      return _t_need_v5(kev);
    }
    else {
      return u3_v5_none;
    }
  }
  else {
    u3h_v5_node* han_u = u3h_v5_slot_to_node(sot_w);

    return _ch_node_git_v5(han_u, 25, rem_w, key);
  }
}

static void
_ch_free_buck_v5(u3h_v5_buck* hab_u)
{
  //fprintf(stderr, "free buck\r\n");
  c3_v5_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3v5z(u3h_v5_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_v5_wfree(hab_u);
}

static void
_ch_free_node_v5(u3h_v5_node* han_u, c3_v5_w lef_w, c3_o pin_o)
{
  c3_v5_w len_w = _ch_popcount_v5(han_u->map_w);
  c3_v5_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_v5_w sot_w = han_u->sot_w[i_w];
    if ( _(u3h_v5_slot_is_null(sot_w))) {
    }  else if ( _(u3h_v5_slot_is_noun(sot_w)) ) {
      u3v5z(u3h_v5_slot_to_noun(sot_w));
    } else {
      void* hav_v = u3h_v5_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_free_buck_v5(hav_v);
      } else {
        _ch_free_node_v5(hav_v, lef_w, pin_o);
      }
    }
  }
  u3a_v5_wfree(han_u);
}

void
u3h_v5_free(u3v5p(u3h_v5_root) har_p)
{
  u3h_v5_root* har_u = u3v5to(u3h_v5_root, har_p);
  c3_v5_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_v5_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v5_slot_is_noun(sot_w)) ) {
      u3v5z(u3h_v5_slot_to_noun(sot_w));
    }
    else if ( _(u3h_v5_slot_is_node(sot_w)) ) {
      u3h_v5_node* han_u = u3h_v5_slot_to_node(sot_w);

      _ch_free_node_v5(han_u, 25, i_w == 57);
    }
  }
  u3a_v5_wfree(har_u);
}

static u3h_v5_buck*
_ch_buck_new_v5(c3_v5_w len_w)
{
  u3h_v5_buck* hab_u = u3a_v5_walloc(c3_v5_wiseof(u3h_v5_buck) +
                               (len_w * c3_v5_wiseof(u3h_v5_slot)));
  hab_u->len_w = len_w;
  return hab_u;
}

static u3h_v5_node*
_ch_node_new_v5(c3_v5_w len_w)
{
  u3h_v5_node* han_u = u3a_v5_walloc(c3_v5_wiseof(u3h_v5_node) +
                               (len_w * c3_v5_wiseof(u3h_v5_slot)));
  han_u->map_w = 0;
  return han_u;
}

static u3h_v5_buck*
_ch_buck_add_v5(u3h_v5_buck* hab_u, u3_v5_noun kev, c3_v5_w *use_w)
{
  c3_v5_w i_w;

  //  if our key is equal to any of the existing keys in the bucket,
  //  then replace that key-value pair with kev.
  //
  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3_v5_noun kov = u3h_v5_slot_to_noun(hab_u->sot_w[i_w]);
    if ( c3y == u3r_v5_sing(_h_need_v5(kev), _h_need_v5(kov)) ) {
      hab_u->sot_w[i_w] = u3h_v5_noun_to_slot(kev);
      u3v5z(kov);
      return hab_u;
    }
  }

  //  create mutant bucket with added key-value pair.
  //  Optimize: use u3a_wealloc().
  {
    u3h_v5_buck* bah_u = _ch_buck_new_v5(1 + hab_u->len_w);
    bah_u->sot_w[0] = u3h_v5_noun_be_warm(u3h_v5_noun_to_slot(kev));

    for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
      bah_u->sot_w[i_w + 1] = hab_u->sot_w[i_w];
    }

    u3a_v5_wfree(hab_u);
    *use_w += 1;
    return bah_u;
  }
}

static void _ch_slot_put_v5(u3h_v5_slot*, u3_v5_noun, c3_v5_w, c3_v5_w, c3_v5_w*);

static u3h_v5_node*
_ch_node_add_v5(u3h_v5_node* han_u, c3_v5_w lef_w, c3_v5_w rem_w, u3_v5_noun kev, c3_v5_w *use_w)
{
  c3_v5_w bit_w, inx_w, map_w, i_w;

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  rem_w = CUT_END_V5(rem_w, lef_w);
  map_w = han_u->map_w;
  inx_w = _ch_popcount_v5(CUT_END_V5(map_w, bit_w));

  if ( BIT_SET_V5(map_w, bit_w) ) {
    _ch_slot_put_v5(&(han_u->sot_w[inx_w]), kev, lef_w, rem_w, use_w);
    return han_u;
  }
  else {
    //  nothing was at this slot.
    //  Optimize: use u3a_wealloc.
    //
    c3_v5_w      len_w = _ch_popcount_v5(map_w);
    u3h_v5_node* nah_u = _ch_node_new_v5(1 + len_w);
    nah_u->map_w    = han_u->map_w | ((c3_v5_w)1 << bit_w);

    for ( i_w = 0; i_w < inx_w; i_w++ ) {
      nah_u->sot_w[i_w] = han_u->sot_w[i_w];
    }
    nah_u->sot_w[inx_w] = u3h_v5_noun_be_warm(u3h_v5_noun_to_slot(kev));
    for ( i_w = inx_w; i_w < len_w; i_w++ ) {
      nah_u->sot_w[i_w + 1] = han_u->sot_w[i_w];
    }

    u3a_v5_wfree(han_u);
    *use_w += 1;
    return nah_u;
  }
}

static void*
_ch_some_add_v5(void* han_v, c3_v5_w lef_w, c3_v5_w rem_w, u3_v5_noun kev, c3_v5_w *use_w)
{
  if ( 0 == lef_w ) {
    return _ch_buck_add_v5((u3h_v5_buck*)han_v, kev, use_w);
  }
  else return _ch_node_add_v5((u3h_v5_node*)han_v, lef_w, rem_w, kev, use_w);
}

u3h_v5_slot
_ch_two_v5(u3h_v5_slot had_w, u3h_v5_slot add_w, c3_v5_w lef_w, c3_v5_w ham_w, c3_v5_w mad_w)
{
  void* ret;

  if ( 0 == lef_w ) {
    u3h_v5_buck* hab_u = _ch_buck_new_v5(2);
    ret = hab_u;
    hab_u->sot_w[0] = had_w;
    hab_u->sot_w[1] = add_w;
  }
  else {
    c3_v5_w hop_w, tad_w;
    lef_w -= 5;
    hop_w = ham_w >> lef_w;
    tad_w = mad_w >> lef_w;
    if ( hop_w == tad_w ) {
      // fragments collide: store in a child node.
      u3h_v5_node* han_u = _ch_node_new_v5(1);
      ret             = han_u;
      han_u->map_w    = (c3_v5_w)1 << hop_w;
      ham_w           = CUT_END_V5(ham_w, lef_w);
      mad_w           = CUT_END_V5(mad_w, lef_w);
      han_u->sot_w[0] = _ch_two_v5(had_w, add_w, lef_w, ham_w, mad_w);
    }
    else {
      u3h_v5_node* han_u = _ch_node_new_v5(2);
      ret             = han_u;
      han_u->map_w    = ((c3_v5_w)1 << hop_w) | ((c3_v5_w)1 << tad_w);
      // smaller mug fragments go in earlier slots
      if ( hop_w < tad_w ) {
        han_u->sot_w[0] = had_w;
        han_u->sot_w[1] = add_w;
      }
      else {
        han_u->sot_w[0] = add_w;
        han_u->sot_w[1] = had_w;
      }
    }
  }

  return u3h_v5_node_to_slot(ret);
}

static void
_ch_slot_put_v5(u3h_v5_slot* sot_w, u3_v5_noun kev, c3_v5_w lef_w, c3_v5_w rem_w, c3_v5_w* use_w)
{
  if ( c3n == u3h_v5_slot_is_noun(*sot_w) ) {
    void* hav_v = _ch_some_add_v5(u3h_v5_slot_to_node(*sot_w),
                               lef_w,
                               rem_w,
                               kev,
                               use_w);

    u3_assert( c3y == u3h_v5_slot_is_node(*sot_w) );
    *sot_w = u3h_v5_node_to_slot(hav_v);
  }
  else {
    u3_v5_noun  kov   = u3h_v5_slot_to_noun(*sot_w);
    u3h_v5_slot add_w = u3h_v5_noun_be_warm(u3h_v5_noun_to_slot(kev));
    if ( c3y == u3r_v5_sing(_h_need_v5(kev), _h_need_v5(kov)) ) {
      // replace old value
      u3v5z(kov);
      *sot_w = add_w;
    }
    else {
      c3_v5_w ham_w = CUT_END_V5(u3r_v5_mug(_h_need_v5(kov)), lef_w);
      *sot_w     = _ch_two_v5(*sot_w, add_w, lef_w, ham_w, rem_w);
      *use_w    += 1;
    }
  }
}

static u3_v5_weak
_ch_trim_slot_v5(u3h_v5_root* har_u, u3h_v5_slot *sot_w, c3_v5_w lef_w, c3_v5_w rem_w);

static u3_v5_weak
_ch_trim_root_v5(u3h_v5_root* har_u);

c3_v5_w
_ch_skip_slot_v5(c3_v5_w mug_w, c3_v5_w lef_w);

static u3_v5_weak
_ch_trim_node_v5(u3h_v5_root* har_u, u3h_v5_slot* sot_w, c3_v5_w lef_w, c3_v5_w rem_w)
{
  c3_v5_w bit_w, map_w, inx_w;
  u3h_v5_slot* tos_w;
  u3h_v5_node* han_u = (u3h_v5_node*) u3h_v5_slot_to_node(*sot_w);

  lef_w -= 5;
  bit_w = (rem_w >> lef_w);
  map_w = han_u->map_w;

  if ( !BIT_SET_V5(map_w, bit_w) ) {
    har_u->arm_u.mug_w = _ch_skip_slot_v5(har_u->arm_u.mug_w, lef_w);
    return c3n;
  }

  rem_w = CUT_END_V5(rem_w, lef_w);
  inx_w = _ch_popcount_v5(CUT_END_V5(map_w, bit_w));
  tos_w = &(han_u->sot_w[inx_w]);

  u3_v5_weak ret = _ch_trim_slot_v5(har_u, tos_w, lef_w, rem_w);
  if ( (u3_v5_none != ret) && (0 == *tos_w) ) {
    // shrink!
    c3_v5_w i_w, ken_w, len_w = _ch_popcount_v5(map_w);
    u3h_v5_slot  kes_w;

    if ( 2 == len_w && ((ken_w = (0 == inx_w) ? 1 : 0),
                        (kes_w = han_u->sot_w[ken_w]),
                        (c3y == u3h_v5_slot_is_noun(kes_w))) ) {
      // only one side left, and the other is a noun. debucketize.
      *sot_w = kes_w;
      u3a_wfree(han_u);
    }
    else {
      // shrink node in place; don't reallocate, we could be low on memory
      //
      han_u->map_w &= ~(1 << bit_w);
      --len_w;

      for ( i_w = inx_w; i_w < len_w; i_w++ ) {
        han_u->sot_w[i_w] = han_u->sot_w[i_w + 1];
      }
    }
  }
  return ret;
}

static u3_v5_weak
_ch_trim_kev_v5(u3h_v5_slot *sot_w)
{
  if ( _(u3h_v5_slot_is_warm(*sot_w)) ) {
    *sot_w = u3h_noun_be_cold(*sot_w);
    return u3_v5_none;
  }
  else {
    u3_v5_noun kev = u3h_v5_slot_to_noun(*sot_w);
    *sot_w = 0;
    return kev;
  }
}

static u3_v5_weak
_ch_trim_buck_v5(u3h_v5_root* har_u, u3h_v5_slot* sot_w)
{
  c3_v5_w i_w, len_w;
  u3h_v5_buck* hab_u = u3h_v5_slot_to_node(*sot_w);

  for ( len_w = hab_u->len_w;
        har_u->arm_u.inx_w < len_w;
        har_u->arm_u.inx_w += 1 )
  {
    u3_v5_weak ret = _ch_trim_kev_v5(&(hab_u->sot_w[har_u->arm_u.inx_w]));
    if ( u3_v5_none != ret ) {
      if ( 2 == len_w ) {
        // 2 things in bucket: debucketize to key-value pair, the next
        // run will point at this pair (same mug_w, no longer in bucket)
        *sot_w = hab_u->sot_w[ (0 == har_u->arm_u.inx_w) ? 1 : 0 ];
        u3a_wfree(hab_u);
        har_u->arm_u.inx_w = 0;
      }
      else {
        // shrink bucket in place; don't reallocate, we could be low on memory
        hab_u->len_w = --len_w;

        for ( i_w = har_u->arm_u.inx_w; i_w < len_w; ++i_w ) {
          hab_u->sot_w[i_w] = hab_u->sot_w[i_w + 1];
        }
        // leave the arm pointing at the next index in the bucket
        ++(har_u->arm_u.inx_w);
      }
      return ret;
    }
  }

  har_u->arm_u.mug_w = (har_u->arm_u.mug_w + 1) & 0x7FFFFFFF; // modulo 2^31
  har_u->arm_u.inx_w = 0;
  return u3_v5_none;
}

static u3_v5_weak
_ch_trim_some_v5(u3h_v5_root* har_u, u3h_v5_slot* sot_w, c3_v5_w lef_w, c3_v5_w rem_w)
{
  if ( 0 == lef_w ) {
    return _ch_trim_buck_v5(har_u, sot_w);
  }
  else {
    return _ch_trim_node_v5(har_u, sot_w, lef_w, rem_w);
  }
}

c3_v5_w
_ch_skip_slot_v5(c3_v5_w mug_w, c3_v5_w lef_w)
{
  c3_v5_w hig_w = mug_w >> lef_w;
  c3_v5_w new_w = CUT_END_V5(hig_w + 1, (31 - lef_w)); // modulo 2^(31 - lef_w)
  return new_w << lef_w;
}

static u3_v5_weak
_ch_trim_slot_v5(u3h_v5_root* har_u, u3h_v5_slot *sot_w, c3_v5_w lef_w, c3_v5_w rem_w)
{
  if ( c3y == u3h_v5_slot_is_noun(*sot_w) ) {
    har_u->arm_u.mug_w = _ch_skip_slot_v5(har_u->arm_u.mug_w, lef_w);
    return _ch_trim_kev_v5(sot_w);
  }
  else {
    return _ch_trim_some_v5(har_u, sot_w, lef_w, rem_w);
  }
}

static u3_v5_weak
_ch_trim_root_v5(u3h_v5_root* har_u)
{
  c3_v5_w      mug_w = har_u->arm_u.mug_w;
  c3_v5_w      inx_w = mug_w >> 25; // 6 bits
  u3h_v5_slot* sot_w = &(har_u->sot_w[inx_w]);

  if ( c3y == u3h_v5_slot_is_null(*sot_w) ) {
    har_u->arm_u.mug_w = _ch_skip_slot_v5(har_u->arm_u.mug_w, 25);
    return u3_v5_none;
  }

  return _ch_trim_slot_v5(har_u, sot_w, 25, CUT_END_V5(mug_w, 25));
}

u3_v5_weak
u3h_v5_put_get(u3v5p(u3h_v5_root) har_p, u3_v5_noun key, u3_v5_noun val)
{
  u3h_v5_root* har_u = u3v5to(u3h_v5_root, har_p);
  u3_v5_noun   kev   = u3v5nc(u3v5k(key), val);
  c3_v5_w      mug_w = u3r_v5_mug(key);
  c3_v5_w      inx_w = (mug_w >> 25);  //  6 bits
  c3_v5_w      rem_w = CUT_END_V5(mug_w, 25);
  u3h_v5_slot* sot_w = &(har_u->sot_w[inx_w]);

  if ( c3y == u3h_v5_slot_is_null(*sot_w) ) {
    *sot_w = u3h_v5_noun_be_warm(u3h_v5_noun_to_slot(kev));
    har_u->use_w += 1;
  }
  else {
    _ch_slot_put_v5(sot_w, kev, 25, rem_w, &(har_u->use_w));
  }

  {
    u3_v5_weak ret = u3_v5_none;

    if ( har_u->max_w && (har_u->use_w > har_u->max_w) ) {
      do {
        ret = _ch_trim_root_v5(har_u);
      }
      while ( u3_v5_none == ret );
      har_u->use_w -= 1;
    }

    return ret;
  }
}

void
u3h_v5_put(u3v5p(u3h_v5_root) har_p, u3_v5_noun key, u3_v5_noun val)
{
  u3_v5_weak del = u3h_v5_put_get(har_p, key, val);
  if ( u3_v5_none != del ) {
    u3v5z(del);
  }
}

#define SING_NONE 0
#define SING_HEAD 1
#define SING_TAIL 2

typedef struct {
  c3_y     sat_y;
  u3_v5_noun  a;
  u3_v5_noun  b;
} eqframev5;

static inline eqframev5*
_cr_sing_push_v5(u3a_v5_pile* pil_u, u3_v5_noun a, u3_v5_noun b)
{
  eqframev5* fam_u = u3a_v5_push(pil_u);
  fam_u->sat_y   = SING_NONE;
  fam_u->a       = a;
  fam_u->b       = b;
  return fam_u;
}

static inline c3_o
_cr_sing_mug_v5(u3a_v5_noun* a_u, u3a_v5_noun* b_u)
{
  //  XX add debug assertions that both mugs are 31-bit
  //  (ie, not u3a_take() relocation references)
  //
  if ( a_u->mug_w && b_u->mug_w && (a_u->mug_w != b_u->mug_w) ) {
    return c3n;
  }

  return c3y;
}

static inline c3_o
_cr_sing_atom_v5(u3_v5_atom a, u3_v5_noun b)
{
  //  [a] is an atom, not pointer-equal to noun [b].
  //  if they're not both indirect atoms, they can't be equal.
  //
  if (  (c3n == u3a_v5_is_pug(a))
     || (c3n == u3a_v5_is_pug(b)) )
  {
    return c3n;
  }
  else {
    u3a_v5_atom* a_u = u3a_v5_to_ptr(a);
    u3a_v5_atom* b_u = u3a_v5_to_ptr(b);

    //  [a] and [b] are not equal if their mugs are present and not equal.
    //
    if ( c3n == _cr_sing_mug_v5((u3a_v5_noun*)a_u, (u3a_v5_noun*)b_u) ) {
      return c3n;
    }
    else {
      c3_v5_w a_w = a_u->len_w;
      c3_v5_w b_w = b_u->len_w;

      //  [a] and [b] are not equal if their lengths are not equal
      //
      if ( a_w != b_w ) {
        return c3n;
      }
      else {
        c3_v5_w i_w;

        //  XX memcmp
        //
        for ( i_w = 0; i_w < a_w; i_w++ ) {
          if ( a_u->buf_w[i_w] != b_u->buf_w[i_w] ) {
            return c3n;
          }
        }
      }
    }
  }

  return c3y;
}

static inline c3_o
_cr_sing_cape_test(u3v5p(u3h_v5_root) har_p, u3_v5_noun a, u3_v5_noun b)
{
  u3_v5_noun key = u3v5nc(u3a_v5_to_off(a) >> u3a_v5_vits,
                     u3a_v5_to_off(b) >> u3a_v5_vits);
  u3_v5_noun val;

  val = u3h_v5_git(har_p, key);

  u3v5z(key);
  return ( u3_v5_none != val ) ? c3y : c3n;
}

static inline void
_cr_sing_cape_keep_v5(u3v5p(u3h_v5_root) har_p, u3_v5_noun a, u3_v5_noun b)
{
  //  only store if [a] and [b] are copies of each other
  //
  if ( a != b ) {
    c3_dessert( (c3n == u3a_v5_is_cat(a)) && (c3n == u3a_v5_is_cat(b)) );

    u3_v5_noun key = u3v5nc(u3a_v5_to_off(a) >> u3a_v5_vits,
                       u3a_v5_to_off(b) >> u3a_v5_vits);
    u3h_v5_put(har_p, key, c3y);
    u3v5z(key);
  }
}

static inline __attribute__((always_inline)) void
_cr_sing_wed_v5(u3_v5_noun *restrict a, u3_v5_noun *restrict b)
{
  if ( *a != *b ) {
    u3a_v5_wed(a, b);
  }
}

static c3_o
_cr_sing_cape_v5(u3a_v5_pile* pil_u, u3v5p(u3h_v5_root) har_p)
{
  eqframev5* fam_u = u3a_v5_peek(pil_u);
  u3_v5_noun   a, b;
  u3a_v5_cell* a_u;
  u3a_v5_cell* b_u;

  //  loop while arguments remain on the stack
  //
  do {
    a = fam_u->a;
    b = fam_u->b;

    switch ( fam_u->sat_y ) {

      //  [a] and [b] are arbitrary nouns
      //
      case SING_NONE: {
        if ( a == b ) {
          break;
        }
        else if ( c3y == u3a_v5_is_atom(a) ) {
          if ( c3n == _cr_sing_atom_v5(a, b) ) {
            return c3n;
          }
          else {
            break;
          }
        }
        else if ( c3y == u3a_v5_is_atom(b) ) {
          return c3n;
        }
        //  [a] and [b] are cells
        //
        else {
          a_u = u3a_v5_to_ptr(a);
          b_u = u3a_v5_to_ptr(b);

          //  short-circuiting mug check
          //
          if ( c3n == _cr_sing_mug_v5((u3a_v5_noun*)a_u, (u3a_v5_noun*)b_u) ) {
            return c3n;
          }
          //  short-circuiting re-comparison check
          //
          else if ( c3y == _cr_sing_cape_test(har_p, a, b) ) {
            fam_u = u3a_v5_pop(pil_u);
            continue;
          }
          //  upgrade none-frame to head-frame, check heads
          //
          else {
            fam_u->sat_y = SING_HEAD;
            fam_u = _cr_sing_push_v5(pil_u, a_u->hed, b_u->hed);
            continue;
          }
        }
      } break;

      //  cells [a] and [b] have equal heads
      //
      case SING_HEAD: {
        a_u = u3a_v5_to_ptr(a);
        b_u = u3a_v5_to_ptr(b);
        _cr_sing_wed_v5(&(a_u->hed), &(b_u->hed));

        //  upgrade head-frame to tail-frame, check tails
        //
        fam_u->sat_y = SING_TAIL;
        fam_u = _cr_sing_push_v5(pil_u, a_u->tel, b_u->tel);
        continue;
      }

      //  cells [a] and [b] are equal
      //
      case SING_TAIL: {
        a_u = u3a_v5_to_ptr(a);
        b_u = u3a_v5_to_ptr(b);
        _cr_sing_wed_v5(&(a_u->tel), &(b_u->tel));
      } break;

      default: {
        u3_assert(0);
      } break;
    }

    //  track equal pairs to short-circuit possible (re-)comparison
    //
    _cr_sing_cape_keep_v5(har_p, a, b);

    fam_u = u3a_v5_pop(pil_u);
  }
  while ( c3n == u3a_v5_pile_done(pil_u) );

  return c3y;
}

/***  retrieve.c
***/
static c3_o
_cr_sing_v5(u3_v5_noun a, u3_v5_noun b)
{
  c3_s     ovr_s = 0;
  u3a_v5_cell*  a_u;
  u3a_v5_cell*  b_u;
  eqframev5* fam_u;
  u3a_v5_pile pil_u;

  //  initialize stack control, push arguments onto the stack (none-frame)
  //
  u3a_v5_pile_prep(&pil_u, sizeof(eqframev5));
  fam_u = _cr_sing_push_v5(&pil_u, a, b);

  //  loop while arguments are on the stack
  //
  while ( c3n == u3a_v5_pile_done(&pil_u) ) {
    a = fam_u->a;
    b = fam_u->b;

    switch ( fam_u->sat_y ) {

      //  [a] and [b] are arbitrary nouns
      //
      case SING_NONE: {
        if ( a == b ) {
          break;
        }
        else if ( c3y == u3a_v5_is_atom(a) ) {
          if ( c3n == _cr_sing_atom_v5(a, b) ) {
            u3R_v5->cap_p = pil_u.top_p;
            return c3n;
          }
          else {
            break;
          }
        }
        else if ( c3y == u3a_v5_is_atom(b) ) {
          u3R_v5->cap_p = pil_u.top_p;
          return c3n;
        }
        //  [a] and [b] are cells
        //
        else {
          a_u = u3a_v5_to_ptr(a);
          b_u = u3a_v5_to_ptr(b);

          //  short-circuiting mug check
          //
          if ( c3n == _cr_sing_mug_v5((u3a_v5_noun*)a_u, (u3a_v5_noun*)b_u) ) {
            u3R_v5->cap_p = pil_u.top_p;
            return c3n;
          }
          //  upgrade none-frame to head-frame, check heads
          //
          else {
            fam_u->sat_y = SING_HEAD;
            fam_u = _cr_sing_push_v5(&pil_u, a_u->hed, b_u->hed);
            continue;
          }
        }
      } break;

      //  cells [a] and [b] have equal heads
      //
      case SING_HEAD: {
        a_u = u3a_v5_to_ptr(a);
        b_u = u3a_v5_to_ptr(b);
        _cr_sing_wed_v5(&(a_u->hed), &(b_u->hed));

        //  upgrade head-frame to tail-frame, check tails
        //
        fam_u->sat_y = SING_TAIL;
        fam_u = _cr_sing_push_v5(&pil_u, a_u->tel, b_u->tel);
        continue;
      }

      //  cells [a] and [b] are equal
      //
      case SING_TAIL: {
        a_u = u3a_v5_to_ptr(a);
        b_u = u3a_v5_to_ptr(b);
        _cr_sing_wed_v5(&(a_u->tel), &(b_u->tel));
      } break;

      default: {
        u3_assert(0);
      } break;
    }

    //  [ovr_s] counts comparisons, if it overflows, we've likely hit
    //  a pathological case (highly duplicated tree), so we de-duplicate
    //  subsequent comparisons by maintaining a set of equal pairs.
    //
    if ( 0 == ++ovr_s ) {
      u3v5p(u3h_v5_root) har_p = u3h_v5_new();
      c3_o ret_o = _cr_sing_cape_v5(&pil_u, har_p);
      u3h_v5_free(har_p);
      u3R_v5->cap_p = pil_u.top_p;
      return ret_o;
    }

    fam_u = u3a_v5_pop(&pil_u);
  }

  return c3y;
}

c3_o
u3r_v5_sing(u3_v5_noun a, u3_v5_noun b)
{
  c3_o ret_o;
  ret_o = _cr_sing_v5(a, b);
  return ret_o;
}