/// @file

#include "allocate.h"

#include "hashtable.h"
#include "log.h"
#include "manage.h"
#include "options.h"
#include "retrieve.h"
#include "trace.h"
#include "vortex.h"

u3_road* u3a_Road;

#ifdef U3_MEMORY_DEBUG
c3_w u3_Code;
#endif

c3_w u3a_to_pug(c3_w off);
c3_w u3a_to_pom(c3_w off);

void
u3a_drop(const u3a_pile* pil_u);
void*
u3a_peek(const u3a_pile* pil_u);
void*
u3a_pop(const u3a_pile* pil_u);
void*
u3a_push(const u3a_pile* pil_u);
c3_o
u3a_pile_done(const u3a_pile* pil_u);

/* _box_count(): adjust memory count.
*/
#ifdef  U3_CPU_DEBUG
static void
_box_count(c3_ws siz_ws)
{
  u3R->all.fre_w += siz_ws;

  {
    c3_w end_w = u3a_heap(u3R);
    c3_w all_w = (end_w - u3R->all.fre_w);

    if ( all_w > u3R->all.max_w ) {
      u3R->all.max_w = all_w;
    }
  }
}
#else
static void
_box_count(c3_ws siz_ws) { }
#endif

/* _box_vaal(): validate box alignment. no-op without C3DBG

   TODO: I think validation code that might be compiled out like this,
   _box_count, (others?) should have perhaps its own header and certainly its
   own prefix. having to remind yourself that _box_count doesn't actually do
   anything unless U3_CPU_DEBUG is defined is annoying. */
#define _box_vaal(box_u)                                        \
  do {                                                          \
  c3_dessert(((uintptr_t)u3a_boxto(box_u)                       \
              & u3a_balign-1) == 0);                            \
  c3_dessert((((u3a_box*)(box_u))->siz_w                        \
              & u3a_walign-1) == 0);                            \
  } while(0)

/* _box_slot(): select the right free list to search for a block.
   TODO: do we really need a loop to do this?

   so our free list logic looks like this:
   siz_w < 6 words then [0]
   siz_w < 16      then [1]
   siz_w < 32      then [2]
   siz_w < 64      then [3]
   ...
   siz_w > 4G      then [26]
*/
static c3_w
_box_slot(c3_w siz_w)
{
  if ( siz_w < u3a_minimum ) {
    return 0;
  }

  for (c3_w i_w = 1; i_w < u3a_fbox_no; i_w++) {
    if ( siz_w < 16 ) return i_w;
    siz_w = (siz_w + 1) >> 1;
  }
  return u3a_fbox_no - 1;
}

/* _box_make(): construct a box.
   box_v - start addr of box
   siz_w - size of allocated space adjacent to block
   use_w - box's refcount
*/
static u3a_box*
_box_make(void* box_v, c3_w siz_w, c3_w use_w)
{
  u3a_box* box_u = box_v;
  c3_w*    box_w = box_v;

  u3_assert(siz_w >= u3a_minimum);

  box_u->siz_w = siz_w;
  box_w[siz_w - 1] = siz_w;     /* stor size at end of allocation as well */
  box_u->use_w = use_w;

  _box_vaal(box_u);

# ifdef  U3_MEMORY_DEBUG
    box_u->cod_w = u3_Code;
    box_u->eus_w = 0;
# endif

  return box_u;
}

/* _box_attach(): attach a box to the free list.
*/
static void
_box_attach(u3a_box* box_u)
{
  u3_assert(box_u->siz_w >= (1 + c3_wiseof(u3a_fbox)));
  u3_assert(0 != u3of(u3a_fbox, box_u));

#if 0
  //  For debugging, fill the box with beef.
  {
    c3_w* box_w = (void *)box_u;
    c3_w  i_w;

    for ( i_w = c3_wiseof(u3a_box); (i_w + 1) < box_u->siz_w; i_w++ ) {
      box_w[i_w] = 0xdeadbeef;
    }
  }
#endif

  _box_count(box_u->siz_w);
  {
    c3_w           sel_w = _box_slot(box_u->siz_w);
    u3p(u3a_fbox)  fre_p = u3of(u3a_fbox, box_u);
    u3p(u3a_fbox)* pfr_p = &u3R->all.fre_p[sel_w];
    u3p(u3a_fbox)  nex_p = *pfr_p;

    u3to(u3a_fbox, fre_p)->pre_p = 0;
    u3to(u3a_fbox, fre_p)->nex_p = nex_p;
    if ( u3to(u3a_fbox, fre_p)->nex_p ) {
      u3to(u3a_fbox, u3to(u3a_fbox, fre_p)->nex_p)->pre_p = fre_p;
    }
    (*pfr_p) = fre_p;
  }
}

/* _box_detach(): detach a box from the free list.
*/
static void
_box_detach(u3a_box* box_u)
{
  u3p(u3a_fbox) fre_p = u3of(u3a_fbox, box_u);
  u3p(u3a_fbox) pre_p = u3to(u3a_fbox, fre_p)->pre_p;
  u3p(u3a_fbox) nex_p = u3to(u3a_fbox, fre_p)->nex_p;

  _box_count(-(box_u->siz_w));

  if ( nex_p ) {
    if ( u3to(u3a_fbox, nex_p)->pre_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3to(u3a_fbox, nex_p)->pre_p = pre_p;
  }
  if ( pre_p ) {
    if( u3to(u3a_fbox, pre_p)->nex_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3to(u3a_fbox, pre_p)->nex_p = nex_p;
  }
  else {
    c3_w sel_w = _box_slot(box_u->siz_w);

    if ( fre_p != u3R->all.fre_p[sel_w] ) {
      u3_assert(!"loom: corrupt");
    }
    u3R->all.fre_p[sel_w] = nex_p;
  }
}

/* _box_free(): free and coalesce.
*/
static void
_box_free(u3a_box* box_u)
{
  c3_w* box_w = (c3_w *)(void *)box_u;

  u3_assert(box_u->use_w != 0);
  box_u->use_w -= 1;
  if ( 0 != box_u->use_w ) {
    return;
  }

  _box_vaal(box_u);

#if 0
  /* Clear the contents of the block, for debugging.
  */
  {
    c3_w i_w;

    for ( i_w = c3_wiseof(u3a_box); (i_w + 1) < box_u->siz_w; i_w++ ) {
      box_w[i_w] = 0xdeadbeef;
    }
  }
#endif

  if ( c3y == u3a_is_north(u3R) ) { /* north */
    /* Try to coalesce with the block below.
    */
    if ( box_w != u3a_into(u3R->rut_p) ) {
      c3_w       laz_w = *(box_w - 1); /* the size of a box stored at the end of its allocation */
      u3a_box* pox_u = (u3a_box*)(void *)(box_w - laz_w); /* the head of the adjacent box below */

      if ( 0 == pox_u->use_w ) {
        _box_detach(pox_u);
        _box_make(pox_u, (laz_w + box_u->siz_w), 0);

        box_u = pox_u;
        box_w = (c3_w*)(void *)pox_u;
      }
    }

    /* Try to coalesce with the block above, or the wilderness.
    */
    if ( (box_w + box_u->siz_w) == u3a_into(u3R->hat_p) ) {
      u3R->hat_p = u3a_outa(box_w);
    }
    else {
      u3a_box* nox_u = (u3a_box*)(void *)(box_w + box_u->siz_w);

      if ( 0 == nox_u->use_w ) {
        _box_detach(nox_u);
        _box_make(box_u, (box_u->siz_w + nox_u->siz_w), 0);
      }
      _box_attach(box_u);
    }
  }      /* end north */
  else {                        /* south */
    /* Try to coalesce with the block above.
    */
    if ( (box_w + box_u->siz_w) != u3a_into(u3R->rut_p) ) {
      u3a_box* nox_u = (u3a_box*)(void *)(box_w + box_u->siz_w);

      if ( 0 == nox_u->use_w ) {
        _box_detach(nox_u);
        _box_make(box_u, (box_u->siz_w + nox_u->siz_w), 0);
      }
    }

    /* Try to coalesce with the block below, or with the wilderness.
    */
    if ( box_w == u3a_into(u3R->hat_p) ) {
      u3R->hat_p = u3a_outa(box_w + box_u->siz_w);
    }
    else {
      c3_w laz_w = box_w[-1];
      u3a_box* pox_u = (u3a_box*)(void *)(box_w - laz_w);

      if ( 0 == pox_u->use_w ) {
        _box_detach(pox_u);
        _box_make(pox_u, (laz_w + box_u->siz_w), 0);
        box_u = pox_u;
      }
      _box_attach(box_u);
    }
  } /* end south */
}

/* _ca_box_make_hat(): in u3R, allocate directly on the hat.
*/
static u3a_box*
_ca_box_make_hat(c3_w len_w, c3_w ald_w, c3_w off_w, c3_w use_w)
{
  c3_w
    pad_w,                      /* padding between returned pointer and box */
    siz_w;                      /* total size of allocation */
  u3_post
    box_p,                      /* start of box */
    all_p;                      /* start of returned pointer */

  if ( c3y == u3a_is_north(u3R) ) {
    box_p = all_p = u3R->hat_p;
    all_p += c3_wiseof(u3a_box) + off_w;
    pad_w = c3_align(all_p, ald_w, C3_ALGHI)
      - all_p;
    siz_w = c3_align(len_w + pad_w, u3a_walign, C3_ALGHI);

    //  hand-inlined: siz_w >= u3a_open(u3R)
    //
    if ( (siz_w >= (u3R->cap_p - u3R->hat_p)) ) {
      return 0;
    }
    u3R->hat_p += siz_w;
  }
  else {
    box_p = all_p = u3R->hat_p - len_w;
    all_p += c3_wiseof(u3a_box) + off_w;
    pad_w = all_p
      - c3_align(all_p, ald_w, C3_ALGLO);
    siz_w = c3_align(len_w + pad_w, u3a_walign, C3_ALGHI);

    //  hand-inlined: siz_w >= u3a_open(u3R)
    //
    if ( siz_w >= (u3R->hat_p - u3R->cap_p) ) {
      return 0;
    }
    box_p = u3R->hat_p -= siz_w;
  }
  c3_dessert(!(ald_w <= 2 && off_w == 0) || (0 == pad_w));
  c3_dessert(pad_w <= 4);

  return _box_make(u3a_into(box_p), siz_w, use_w);
}

#if 0
/* _me_road_all_hat(): in u3R, allocate directly on the hat.
*/
static u3a_box*
_ca_box_make_hat(c3_w len_w, c3_w alm_w, c3_w use_w)
{
  return _box_make(_me_road_all_hat(len_w), len_w, use_w);
}
#endif

#if 0  // not yet used
/* _me_road_all_cap(): in u3R, allocate directly on the cap.
*/
static c3_w*
_me_road_all_cap(c3_w len_w)
{
  if ( len_w > u3a_open(u3R) ) {
    u3m_bail(c3__meme); return 0;
  }

  if ( c3y == u3a_is_north(u3R) ) {
    u3R->cap_p -= len_w;
    return u3a_into(u3R->cap_p);
  }
  else {
    u3_post all_p;

    all_p = u3R->cap_p;
    u3R->cap_p += len_w;
    return u3a_into(all_p);
  }
}
#endif

#if 0
/* u3a_sane(): check allocator sanity.
*/
void
u3a_sane(void)
{
  c3_w i_w;

  for ( i_w = 0; i_w < u3a_fbox_no; i_w++ ) {
    u3a_fbox* fre_u = u3R->all.fre_u[i_w];

    while ( fre_u ) {
      if ( fre_u == u3R->all.fre_u[i_w] ) {
        u3_assert(fre_u->pre_u == 0);
      }
      else {
        u3_assert(fre_u->pre_u != 0);
        u3_assert(fre_u->pre_u->nex_u == fre_u);
        if ( fre_u->nex_u != 0 ) {
          u3_assert(fre_u->nex_u->pre_u == fre_u);
        }
      }
      fre_u = fre_u->nex_u;
    }
  }
}
#endif

/* u3a_reflux(): dump 1K cells from the cell list into regular memory.
*/
void
u3a_reflux(void)
{
  c3_w i_w;

  for ( i_w = 0; u3R->all.cel_p && (i_w < 1024); i_w++ ) {
    u3_post  cel_p = u3R->all.cel_p;
    u3a_box* box_u = &(u3to(u3a_fbox, cel_p)->box_u);

    u3R->all.cel_p = u3to(u3a_fbox, cel_p)->nex_p;

    // otherwise _box_free() will double-count it
    //
    _box_count(-(u3a_minimum));
    _box_free(box_u);

  }
}

/* _ca_reclaim_half(): reclaim from memoization cache.
*/
static void
_ca_reclaim_half(void)
{
  //  XX u3l_log avoid here, as it can
  //  cause problems when handling errors

  if ( (0 == u3R->cax.har_p) ||
       (0 == u3to(u3h_root, u3R->cax.har_p)->use_w) )
  {
    fprintf(stderr, "allocate: reclaim: memo cache: empty\r\n");
    u3m_bail(c3__meme);
  }

#if 1
  fprintf(stderr, "allocate: reclaim: half of %d entries\r\n",
          u3to(u3h_root, u3R->cax.har_p)->use_w);

  u3h_trim_to(u3R->cax.har_p, u3to(u3h_root, u3R->cax.har_p)->use_w / 2);
#else
  /*  brutal and guaranteed effective
  */
  u3h_free(u3R->cax.har_p);
  u3R->cax.har_p = u3h_new();
#endif
}

/* _ca_willoc(): u3a_walloc() internals.
*/
static void*
_ca_willoc(c3_w len_w, c3_w ald_w, c3_w off_w)
{
  c3_w siz_w = c3_max(u3a_minimum, u3a_boxed(len_w));
  c3_w sel_w = _box_slot(siz_w);

  /*  XX: this logic is totally bizarre, but preserve it.
  **
  **  This means we use the next size bigger instead of the "correct"
  **  size.  For example, a 20 word allocation will be freed into free
  **  list 2 but will be allocated from free list 3.
  **
  **  This is important to preserve because the sequential search may be
  **  very slow.  On a real-world task involving many compilations,
  **  removing this line made this function appear in ~80% of samples.
  **
  **  For reference, this was added in cgyarvin/urbit ffed9e748d8f6c.
  */
  if ( (sel_w != 0) && (sel_w != u3a_fbox_no - 1) ) {
    sel_w += 1;
  }

  // u3l_log("walloc %d: *pfr_p %x", len_w, u3R->all.fre_p[sel_w]);
  while ( 1 ) {
    u3p(u3a_fbox) *pfr_p = &u3R->all.fre_p[sel_w];

    while ( 1 ) {
      /* increment until we get a non-null freelist */
      if ( 0 == *pfr_p ) {
        if ( sel_w < (u3a_fbox_no - 1) ) {
          sel_w += 1;
          break;
        }
        else {
          //  nothing in top free list; chip away at the hat
          //
          u3a_box* box_u;

          //  memory nearly empty; reclaim; should not be needed
          //
          // if ( (u3a_open(u3R) + u3R->all.fre_w) < 65536 ) { _ca_reclaim_half(); }
          box_u = _ca_box_make_hat(siz_w, ald_w, off_w, 1);

          /* Flush a bunch of cell cache, then try again.
          */
          if ( 0 == box_u ) {
            if ( u3R->all.cel_p ) {
              u3a_reflux();

              return _ca_willoc(len_w, ald_w, off_w);
            }
            else {
              _ca_reclaim_half();
              return _ca_willoc(len_w, ald_w, off_w);
            }
          }
          else return u3a_boxto(box_u);
        }
      }
      else {                    /* we got a non-null freelist */
        u3_post box_p, all_p;
        box_p = all_p = *pfr_p;
        all_p += c3_wiseof(u3a_box) + off_w;
        c3_w pad_w = c3_align(all_p, ald_w, C3_ALGHI) - all_p;
        c3_w des_w = c3_align(siz_w + pad_w, u3a_walign, C3_ALGHI);

        /* calls maximally requesting DWORD alignment of returned pointer
           shouldn't require padding. */
        c3_dessert(!(ald_w <= 2 && off_w == 0) || (0 == pad_w));
        c3_dessert(pad_w <= 4);

        if ( (des_w) > u3to(u3a_fbox, *pfr_p)->box_u.siz_w ) {
          /* This free block is too small.  Continue searching.
          */
          pfr_p = &(u3to(u3a_fbox, *pfr_p)->nex_p);
          continue;
        }
        else {                  /* free block fits desired alloc size */
          u3a_box* box_u = &(u3to(u3a_fbox, *pfr_p)->box_u);

          /* We have found a free block of adequate size.  Remove it
          ** from the free list.
          */

          _box_count(-(box_u->siz_w));
          /* misc free list consistency checks.
            TODO: in the future should probably only run for C3DBG builds */
          {
            if ( (0 != u3to(u3a_fbox, *pfr_p)->pre_p) &&
                 (u3to(u3a_fbox, u3to(u3a_fbox, *pfr_p)->pre_p)->nex_p
                    != (*pfr_p)) )
            {                   /* this->pre->nex isn't this */
              u3_assert(!"loom: corrupt");
            }

            if( (0 != u3to(u3a_fbox, *pfr_p)->nex_p) &&
                (u3to(u3a_fbox, u3to(u3a_fbox, *pfr_p)->nex_p)->pre_p
                   != (*pfr_p)) )
            {                   /* this->nex->pre isn't this */
              u3_assert(!"loom: corrupt");
            }

            /* pop the block */
            /* this->nex->pre = this->pre  */
            if ( 0 != u3to(u3a_fbox, *pfr_p)->nex_p ) {
              u3to(u3a_fbox, u3to(u3a_fbox, *pfr_p)->nex_p)->pre_p =
                u3to(u3a_fbox, *pfr_p)->pre_p;
            }
            /* this = this->nex */
            *pfr_p = u3to(u3a_fbox, *pfr_p)->nex_p;
          }

          /* If we can chop off another block, do it.
          */
          if ( (des_w + u3a_minimum) <= box_u->siz_w ) {
            /* Split the block.
            */

            /* XXX: Despite the fact that we're making a box here, we don't
               actually have to ensure it's aligned, since des_w and all boxes
               already on the loom /are/ aligned. A debug break here implies
               that you broke those conditions, not that this needs to handle
               alignment. abandon hope. */
            c3_w* box_w = ((c3_w *)(void *)box_u);
            c3_w* end_w = box_w + des_w;
            c3_w  lef_w = (box_u->siz_w - des_w);

            _box_attach(_box_make(end_w, lef_w, 0));
            return u3a_boxto(_box_make(box_w, des_w, 1));
          }
          else {
            u3_assert(0 == box_u->use_w);
            box_u->use_w = 1;

#ifdef      U3_MEMORY_DEBUG
              box_u->cod_w = u3_Code;
#endif
            return u3a_boxto(box_u);
          }
        }
      }
    }
  }
}

/* _ca_walloc(): u3a_walloc() internals.

   - len_w: allocation length in words
   - ald_w: desired alignment. N.B. the void * returned is not guaranteed to be
     aligned on this value. But the allocation will be sized such that the
     caller can independently align the value.
   - off_w: alignment offset to use when sizing request.

   void * returned guaranteed to be DWORD (8-byte) aligned.
*/
static void*
_ca_walloc(c3_w len_w, c3_w ald_w, c3_w off_w)
{
  void* ptr_v;

  for (;;) {
    ptr_v = _ca_willoc(len_w, ald_w, off_w);
    if ( 0 != ptr_v ) {
      break;
    }
    _ca_reclaim_half();
  }
  _box_vaal(u3a_botox(ptr_v));
  return ptr_v;
}

/* u3a_walloc(): allocate storage words on hat heap.
*/
void*
u3a_walloc(c3_w len_w)
{
  void* ptr_v;

  ptr_v = _ca_walloc(len_w, 1, 0);

#if 0
  if ( (703 == u3_Code) &&
      u3a_botox(ptr_v) == (u3a_box*)(void *)0x200dfe3e4 ) {
    static int xuc_i;

    u3l_log("xuc_i %d", xuc_i);
    if ( 1 == xuc_i ) {
      u3a_box* box_u = u3a_botox(ptr_v);

      box_u->cod_w = 999;
    }
    xuc_i++;
  }
#endif
  _box_vaal(u3a_botox(ptr_v));
  return ptr_v;
}

/* u3a_wealloc(): realloc in words.
*/
void*
u3a_wealloc(void* lag_v, c3_w len_w)
{
  if ( !lag_v ) {
    return u3a_walloc(len_w);
  }
  else {
    u3a_box* box_u = u3a_botox(lag_v);
    c3_w*    old_w = lag_v;
    c3_w     tiz_w = c3_min(box_u->siz_w, len_w);
    {
      c3_w* new_w = u3a_walloc(len_w);
      c3_w  i_w;

      for ( i_w = 0; i_w < tiz_w; i_w++ ) {
        new_w[i_w] = old_w[i_w];
      }
      u3a_wfree(lag_v);
      return new_w;
    }
  }
}

/* u3a_pile_prep(): initialize stack control.
*/
void
u3a_pile_prep(u3a_pile* pil_u, c3_w len_w)
{
  //  frame size, in words
  //
  c3_w wor_w = (len_w + 3) >> 2;
  c3_o nor_o = u3a_is_north(u3R);

  pil_u->mov_ws = (c3y == nor_o) ? -wor_w :  wor_w;
  pil_u->off_ws = (c3y == nor_o) ?      0 : -wor_w;
  pil_u->top_p  = u3R->cap_p;

#ifdef U3_MEMORY_DEBUG
  pil_u->rod_u  = u3R;
#endif
}

/* u3a_wfree(): free storage.
*/
void
u3a_wfree(void* tox_v)
{
  _box_free(u3a_botox(tox_v));
}

/* u3a_wtrim(): trim storage.

   old_w - old length
   len_w - new length
*/
void
u3a_wtrim(void* tox_v, c3_w old_w, c3_w len_w)
{
  c3_w* nov_w = tox_v;

  if ( (old_w > len_w)
       && ((old_w - len_w) >= u3a_minimum) )
    {
    u3a_box* box_u = u3a_botox(nov_w);
    c3_w*    box_w = (void*)u3a_botox(nov_w);

    c3_w* end_w = c3_align(nov_w + len_w + 1, /* +1 for trailing allocation size */
                           u3a_balign,
                           C3_ALGHI);

    c3_w  asz_w = (end_w - box_w);      /* total size in words of new allocation */
    if (box_u->siz_w <= asz_w) return;
    c3_w  bsz_w = box_u->siz_w - asz_w; /* size diff in words between old and new */

    c3_dessert(asz_w && ((asz_w & u3a_walign-1) == 0)); /* new allocation size must be non-zero and DWORD multiple */
    c3_dessert(end_w < (box_w + box_u->siz_w));         /* desired alloc end must not exceed existing boundaries */
    c3_dessert(((uintptr_t)end_w & u3a_balign-1) == 0); /* address of box getting freed must be DWORD aligned */
    c3_dessert((bsz_w & u3a_walign-1) == 0);            /* size of box getting freed must be DWORD multiple */

    _box_attach(_box_make(end_w, bsz_w, 0)); /* free the unneeded space */

    box_u->siz_w = asz_w;
    box_w[asz_w - 1] = asz_w;
  }
}

/* u3a_calloc(): allocate and zero-initialize array
*/
void*
u3a_calloc(size_t num_i, size_t len_i)
{
  size_t byt_i = num_i * len_i;
  c3_w* out_w;

  u3_assert(byt_i / len_i == num_i);
  out_w = u3a_malloc(byt_i);
  memset(out_w, 0, byt_i);

  return out_w;
}

/* u3a_malloc(): aligned storage measured in bytes.

   Internally pads allocations to 16-byte alignment independent of DWORD
   alignment ensured for word sized allocations.

*/
void*
u3a_malloc(size_t len_i)
{
  c3_w  len_w = (c3_w)((len_i + 3) >> 2);
  c3_w *ptr_w = _ca_walloc(len_w +1, 4, 1); /* +1 for word storing pad size */
  c3_w *out_w = c3_align(ptr_w + 1, 16, C3_ALGHI);
  c3_w  pad_w = u3a_outa(out_w) - u3a_outa(ptr_w);

  out_w[-1] = pad_w - 1; /* the size of the pad doesn't include the word storing the size (-1) */

  c3_dessert(&out_w[len_w] /* alloced space after alignment is sufficient */
             <= &((c3_w*)u3a_botox(ptr_w))[u3a_botox(ptr_w)->siz_w]);
  c3_dessert(pad_w <= 4 && pad_w > 0);
  c3_dessert(&out_w[-1] > ptr_w);

  return out_w;
}

/* u3a_cellblock(): allocate a block of cells on the hat.

   XXX beware when we stop boxing cells and QWORD align references. Alignment
   not guaranteed to be preserved after a call.
*/
static c3_o
u3a_cellblock(c3_w num_w)
{
  u3p(u3a_fbox) fre_p;
  c3_w          i_w;

  if ( c3y == u3a_is_north(u3R) ) {
    if ( u3R->cap_p <= (u3R->hat_p + (num_w * u3a_minimum) + (1 << u3a_page)) ) {
      return c3n;
    }
    else {
      u3_post cel_p = u3R->all.cel_p;
      u3_post hat_p = u3R->hat_p;
      u3R->hat_p   += (num_w * u3a_minimum);

      for ( i_w = 0; i_w < num_w; i_w++) {
        u3_post  all_p = hat_p;
        void*    box_v = u3a_into(all_p);
        u3a_box* box_u = box_v;
        c3_w*    box_w = box_v;

        //  hand inline of _box_make(u3a_into(all_p), u3a_minimum, 1)
        {
          box_u->siz_w = u3a_minimum;
          box_w[u3a_minimum - 1] = u3a_minimum;
          box_u->use_w = 1;
#ifdef U3_MEMORY_DEBUG
            box_u->cod_w = 0;
            box_u->eus_w = 0;
#endif
        }
        hat_p += u3a_minimum;

        fre_p = u3of(u3a_fbox, box_u);
        u3to(u3a_fbox, fre_p)->nex_p = cel_p;
        cel_p = fre_p;
      }

      u3R->all.cel_p = cel_p;
    }
  }
  else {
    if ( (u3R->cap_p + (num_w * u3a_minimum) + (1 << u3a_page)) >= u3R->hat_p ) {
      return c3n;
    }
    else {
      u3_post cel_p = u3R->all.cel_p;
      u3_post hat_p = u3R->hat_p;
      u3R->hat_p   -= (num_w * u3a_minimum);

      for ( i_w = 0; i_w < num_w; i_w++ ) {
        u3_post  all_p = (hat_p -= u3a_minimum);
        void*    box_v = u3a_into(all_p);
        u3a_box* box_u = box_v;
        c3_w*    box_w = box_v;

        //  hand inline of _box_make(u3a_into(all_p), u3a_minimum, 1);
        {
          box_u->siz_w = u3a_minimum;
          box_w[u3a_minimum - 1] = u3a_minimum;
          box_u->use_w = 1;
# ifdef U3_MEMORY_DEBUG
            box_u->cod_w = 0;
            box_u->eus_w = 0;
# endif
        }
        fre_p = u3of(u3a_fbox, box_u);
        u3to(u3a_fbox, fre_p)->nex_p = cel_p;
        cel_p = fre_p;
      }

      u3R->all.cel_p = cel_p;
    }
  }
  _box_count(num_w * u3a_minimum);
  return c3y;
}

/* u3a_celloc(): allocate a cell.
   XXX beware when we stop boxing cells and QWORD align references
*/
c3_w*
u3a_celloc(void)
{
#ifdef U3_CPU_DEBUG
  u3R->pro.cel_d++;
#endif

#ifdef U3_MEMORY_DEBUG
  if ( u3C.wag_w & u3o_debug_ram ) {
    return u3a_walloc(c3_wiseof(u3a_cell));
  }
#endif

  u3p(u3a_fbox) cel_p;

  if ( !(cel_p = u3R->all.cel_p) ) {
    if ( u3R == &(u3H->rod_u) ) {
      // no cell allocator on home road
      //
      return u3a_walloc(c3_wiseof(u3a_cell));
    }
    else {
      if ( c3n == u3a_cellblock(4096) ) {
        return u3a_walloc(c3_wiseof(u3a_cell));
      }
      cel_p = u3R->all.cel_p;
    }
  }

  {
    u3a_box* box_u = &(u3to(u3a_fbox, cel_p)->box_u);


    box_u->use_w = 1;
    u3R->all.cel_p = u3to(u3a_fbox, cel_p)->nex_p;

    _box_count(-(u3a_minimum));

    return u3a_boxto(box_u);
  }
}

/* u3a_cfree(): free a cell.
*/
void
u3a_cfree(c3_w* cel_w)
{
#ifdef U3_MEMORY_DEBUG
  if ( u3C.wag_w & u3o_debug_ram ) {
    return u3a_wfree(cel_w);
  }
#endif

  if ( u3R == &(u3H->rod_u) ) {
    return u3a_wfree(cel_w);
  }
  else {
    u3a_box*      box_u = u3a_botox(cel_w);
    u3p(u3a_fbox) fre_p = u3of(u3a_fbox, box_u);

    _box_count(u3a_minimum);

    u3to(u3a_fbox, fre_p)->nex_p = u3R->all.cel_p;
    u3R->all.cel_p = fre_p;
  }
}

/* u3a_realloc(): aligned realloc in bytes.
*/
void*
u3a_realloc(void* lag_v, size_t len_i)
{
  if ( !lag_v ) {
    return u3a_malloc(len_i);
  }
  else {
    c3_w     len_w = (c3_w)((len_i + 3) >> 2);
    c3_w*    lag_w = lag_v;
    c3_w     pad_w = lag_w[-1];
    c3_w*    org_w = lag_w - (pad_w + 1);
    u3a_box* box_u = u3a_botox((void *)org_w);
    c3_w*    old_w = lag_v;
    c3_w     tiz_w = c3_min(box_u->siz_w, len_w);
    {
      c3_w* new_w = u3a_malloc(len_i);
      c3_w  i_w;

      for ( i_w = 0; i_w < tiz_w; i_w++ ) {
        new_w[i_w] = old_w[i_w];
      }
      u3a_wfree(org_w);
      return new_w;
    }
  }
}

/* u3a_free(): free for aligned malloc.
*/
void
u3a_free(void* tox_v)
{
  if (NULL == tox_v)
    return;

  c3_w* tox_w = tox_v;
  c3_w  pad_w = tox_w[-1];
  c3_w* org_w = tox_w - (pad_w + 1);

  // u3l_log("free %p %p", org_w, tox_w);
  u3a_wfree(org_w);
}

/* _me_wash_north(): clean up mug slots after copy.
*/
static void _me_wash_north(u3_noun dog);
static void
_me_wash_north_in(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) return;
  if ( !_(u3a_north_is_junior(u3R, som)) ) return;

  _me_wash_north(som);
}
static void
_me_wash_north(u3_noun dog)
{
  u3_assert(c3y == u3a_is_dog(dog));
  // u3_assert(c3y == u3a_north_is_junior(u3R, dog));
  {
    u3a_noun* dog_u = u3a_to_ptr(dog);

    if ( dog_u->mug_w == 0 ) return;

    dog_u->mug_w = 0;    //  power wash
    // if ( dog_u->mug_w >> 31 ) { dog_u->mug_w = 0; }

    if ( _(u3a_is_pom(dog)) ) {
      u3a_cell* god_u = (u3a_cell *)(void *)dog_u;

      _me_wash_north_in(god_u->hed);
      _me_wash_north_in(god_u->tel);
    }
  }
}

/* _me_wash_south(): clean up mug slots after copy.
*/
static void _me_wash_south(u3_noun dog);
static void
_me_wash_south_in(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) return;
  if ( !_(u3a_south_is_junior(u3R, som)) ) return;

  _me_wash_south(som);
}
static void
_me_wash_south(u3_noun dog)
{
  u3_assert(c3y == u3a_is_dog(dog));
  // u3_assert(c3y == u3a_south_is_junior(u3R, dog));
  {
    u3a_noun* dog_u = u3a_to_ptr(dog);

    if ( dog_u->mug_w == 0 ) return;

    dog_u->mug_w = 0;    //  power wash
    //  if ( dog_u->mug_w >> 31 ) { dog_u->mug_w = 0; }

    if ( _(u3a_is_pom(dog)) ) {
      u3a_cell* god_u = (u3a_cell *)(void *)dog_u;

      _me_wash_south_in(god_u->hed);
      _me_wash_south_in(god_u->tel);
    }
  }
}

/* u3a_wash(): wash all lazy mugs.  RETAIN.
*/
void
u3a_wash(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) {
    return;
  }
  if ( _(u3a_is_north(u3R)) ) {
    if ( _(u3a_north_is_junior(u3R, som)) ) {
      _me_wash_north(som);
    }
  }
  else {
    if ( _(u3a_south_is_junior(u3R, som)) ) {
      _me_wash_south(som);
    }
  }
}

/* _me_gain_use(): increment use count.
*/
static void
_me_gain_use(u3_noun dog)
{
  c3_w*    dog_w = u3a_to_ptr(dog);
  u3a_box* box_u = u3a_botox(dog_w);

  if ( 0x7fffffff == box_u->use_w ) {
    u3l_log("fail in _me_gain_use");
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

#undef VERBOSE_TAKE

/* _ca_take_atom(): reallocate an indirect atom off the stack.
*/
static inline u3_atom
_ca_take_atom(u3a_atom* old_u)
{
  c3_w*     new_w = u3a_walloc(old_u->len_w + c3_wiseof(u3a_atom));
  u3a_atom* new_u = (u3a_atom*)(void *)new_w;
  u3_noun     new = u3a_to_pug(u3a_outa(new_u));

#ifdef VERBOSE_TAKE
  u3l_log("%s: atom %p to %p", ( c3y == u3a_is_north(u3R) )
                                   ? "north"
                                   : "south",
                                   old_u,
                                   new_u);
#endif

  //  XX use memcpy?
  //
  new_u->mug_w = old_u->mug_w;
  new_u->len_w = old_u->len_w;
  {
    c3_w i_w;

    for ( i_w=0; i_w < old_u->len_w; i_w++ ) {
      new_u->buf_w[i_w] = old_u->buf_w[i_w];
    }
  }

  //  borrow mug slot to record new destination in [old_u]
  //
  old_u->mug_w = new;

  return new;
}

/* _ca_take_cell(): reallocate a cell off the stack.
*/
static inline u3_cell
_ca_take_cell(u3a_cell* old_u, u3_noun hed, u3_noun tel)
{
  //  XX use u3a_celloc?
  //
  c3_w*     new_w = u3a_walloc(c3_wiseof(u3a_cell));
  u3a_cell* new_u = (u3a_cell*)(void *)new_w;
  u3_cell     new = u3a_to_pom(u3a_outa(new_u));

#ifdef VERBOSE_TAKE
  u3l_log("%s: cell %p to %p", ( c3y == u3a_is_north(u3R) )
                                   ? "north"
                                   : "south",
                                   old_u,
                                   new_u);
#endif

  new_u->mug_w = old_u->mug_w;
  new_u->hed   = hed;
  new_u->tel   = tel;

  //  borrow mug slot to record new destination in [old_u]
  //
  old_u->mug_w = new;

  return new;
}

/* _ca_take: stack frame for recording cell travesal
**           (u3_none == hed) == head-frame
*/
typedef struct _ca_take
{
  u3_weak hed;  //  taken head
  u3_cell old;  //  old cell
} _ca_take;

/* _ca_take_next_south: take next noun, pushing cells on stack.
*/
static inline u3_noun
_ca_take_next_north(u3a_pile* pil_u, u3_noun veb)
{
  while ( 1 ) {
    //  direct atoms and senior refs are not counted.
    //
    if (  (c3y == u3a_is_cat(veb))
       || (c3y == u3a_north_is_senior(u3R, veb)) )
    {
      return veb;
    }
    //  not junior; normal (heap) refs on our road are counted.
    //
    else if ( c3n == u3a_north_is_junior(u3R, veb) ) {
      _me_gain_use(veb); // bypass branches in u3k()
      return veb;
    }
    //  junior (stack) refs are copied.
    //
    else {
      u3a_noun* veb_u = u3a_to_ptr(veb);

      //  32-bit mug_w: already copied [veb] and [mug_w] is the new ref.
      //
      if ( veb_u->mug_w >> 31 ) {
        u3_noun nov = (u3_noun)veb_u->mug_w;

        u3_assert( c3y == u3a_north_is_normal(u3R, nov) );

#ifdef VERBOSE_TAKE
        u3l_log("north: %p is already %p", veb_u, u3a_to_ptr(nov));
#endif

        _me_gain_use(nov); // bypass branches in u3k()
        return nov;
      }
      else if ( c3y == u3a_is_atom(veb) ) {
        return _ca_take_atom((u3a_atom*)veb_u);
      }
      else {
        u3a_cell* old_u = (u3a_cell*)veb_u;
        _ca_take* fam_u = u3a_push(pil_u);

        fam_u->hed = u3_none;
        fam_u->old = veb;

        veb = old_u->hed;
        continue;
      }
    }
  }
}

/* _ca_take_next_south: take next noun, pushing cells on stack.
*/
static inline u3_noun
_ca_take_next_south(u3a_pile* pil_u, u3_noun veb)
{
  while ( 1 ) {
    //  direct atoms and senior refs are not counted.
    //
    if (  (c3y == u3a_is_cat(veb))
       || (c3y == u3a_south_is_senior(u3R, veb)) )
    {
      return veb;
    }
    //  not junior; a normal pointer in our road -- refcounted
    //
    else if ( c3n == u3a_south_is_junior(u3R, veb) ) {
      _me_gain_use(veb); // bypass branches in u3k()
      return veb;
    }
    //  junior (stack) refs are copied.
    //
    else {
      u3a_noun* veb_u = u3a_to_ptr(veb);

      //  32-bit mug_w: already copied [veb] and [mug_w] is the new ref.
      //
      if ( veb_u->mug_w >> 31 ) {
        u3_noun nov = (u3_noun)veb_u->mug_w;

        u3_assert( c3y == u3a_south_is_normal(u3R, nov) );

#ifdef VERBOSE_TAKE
        u3l_log("south: %p is already %p", veb_u, u3a_to_ptr(nov));
#endif

        _me_gain_use(nov); // bypass branches in u3k()
        return nov;
      }
      else if ( c3y == u3a_is_atom(veb) ) {
        return _ca_take_atom((u3a_atom*)veb_u);
      }
      else {
        u3a_cell* old_u = (u3a_cell*)veb_u;
        _ca_take* fam_u = u3a_push(pil_u);

        fam_u->hed = u3_none;
        fam_u->old = veb;

        veb = old_u->hed;
        continue;
      }
    }
  }
}

/* _ca_take_north(): in a north road, gain, copying juniors (from stack).
*/
static u3_noun
_ca_take_north(u3_noun veb)
{
  u3_noun     pro;
  _ca_take* fam_u;
  u3a_pile  pil_u;
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  commence taking
  //
  pro = _ca_take_next_north(&pil_u, veb);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash copy and continue into the tail
      //
      if ( u3_none == fam_u->hed ) {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        fam_u->hed = pro;
        pro        = _ca_take_next_north(&pil_u, old_u->tel);
        fam_u      = u3a_peek(&pil_u);
      }
      //  tail-frame: copy cell and pop the stack
      //
      else {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        pro   = _ca_take_cell(old_u, fam_u->hed, pro);
        fam_u = u3a_pop(&pil_u);
      }
    } while ( c3n == u3a_pile_done(&pil_u) );
  }

  return pro;
}
/* _ca_take_south(): in a south road, gain, copying juniors (from stack).
*/
static u3_noun
_ca_take_south(u3_noun veb)
{
  u3_noun     pro;
  _ca_take* fam_u;
  u3a_pile  pil_u;
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  commence taking
  //
  pro = _ca_take_next_south(&pil_u, veb);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash copy and continue into the tail
      //
      if ( u3_none == fam_u->hed ) {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        fam_u->hed = pro;
        pro        = _ca_take_next_south(&pil_u, old_u->tel);
        fam_u      = u3a_peek(&pil_u);
      }
      //  tail-frame: copy cell and pop the stack
      //
      else {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        pro   = _ca_take_cell(old_u, fam_u->hed, pro);
        fam_u = u3a_pop(&pil_u);
      }
    } while ( c3n == u3a_pile_done(&pil_u) );
  }

  return pro;
}

/* u3a_take(): gain, copying juniors.
*/
u3_noun
u3a_take(u3_noun veb)
{
  u3_noun pro;
  u3t_on(coy_o);

  u3_assert(u3_none != veb);

  pro = ( c3y == u3a_is_north(u3R) )
        ? _ca_take_north(veb)
        : _ca_take_south(veb);

  u3t_off(coy_o);
  return pro;
}

/* u3a_left(): true of junior if preserved.
*/
c3_o
u3a_left(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ||
       !_(u3a_is_junior(u3R, som)) )
  {
    return c3y;
  }
  else {
    u3a_noun* dog_u = u3a_to_ptr(som);

    return __(0 != (dog_u->mug_w >> 31));
  }
}

/* _me_gain_north(): gain on a north road.
*/
static u3_noun
_me_gain_north(u3_noun dog)
{
  if ( c3y == u3a_north_is_senior(u3R, dog) ) {
    /*  senior pointers are not refcounted
    */
    return dog;
  }
  else {
    /* junior nouns are disallowed
    */
    u3_assert(!_(u3a_north_is_junior(u3R, dog)));

    /* normal pointers are refcounted
    */
    _me_gain_use(dog);
    return dog;
  }
}

/* _me_gain_south(): gain on a south road.
*/
static u3_noun
_me_gain_south(u3_noun dog)
{
  if ( c3y == u3a_south_is_senior(u3R, dog) ) {
    /*  senior pointers are not refcounted
    */
    return dog;
  }
  else {
    /* junior nouns are disallowed
    */
    u3_assert(!_(u3a_south_is_junior(u3R, dog)));

    /* normal nouns are refcounted
    */
    _me_gain_use(dog);
    return dog;
  }
}

/* _me_lose_north(): lose on a north road.
*/
static void
_me_lose_north(u3_noun dog)
{
top:
  if ( c3y == u3a_north_is_normal(u3R, dog) ) {
    c3_w* dog_w      = u3a_to_ptr(dog);
    u3a_box* box_u = u3a_botox(dog_w);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_is_pom(dog)) ) {
          u3a_cell* dog_u = (void *)dog_w;
          u3_noun     h_dog = dog_u->hed;
          u3_noun     t_dog = dog_u->tel;

          if ( !_(u3a_is_cat(h_dog)) ) {
            _me_lose_north(h_dog);
          }
          u3a_cfree(dog_w);
          if ( !_(u3a_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_wfree(dog_w);
        }
      }
    }
  }
}

/* _me_lose_south(): lose on a south road.
*/
static void
_me_lose_south(u3_noun dog)
{
top:
  if ( c3y == u3a_south_is_normal(u3R, dog) ) {
    c3_w* dog_w      = u3a_to_ptr(dog);
    u3a_box* box_u = u3a_botox(dog_w);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_is_pom(dog)) ) {
          u3a_cell* dog_u = (void *)dog_w;
          u3_noun     h_dog = dog_u->hed;
          u3_noun     t_dog = dog_u->tel;

          if ( !_(u3a_is_cat(h_dog)) ) {
            _me_lose_south(h_dog);
          }
          u3a_cfree(dog_w);
          if ( !_(u3a_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_wfree(dog_w);
        }
      }
    }
  }
}

/* u3a_gain(): gain a reference count in normal space.
*/
u3_noun
u3a_gain(u3_noun som)
{
  u3t_on(mal_o);
  u3_assert(u3_none != som);

  if ( !_(u3a_is_cat(som)) ) {
    som = _(u3a_is_north(u3R))
              ? _me_gain_north(som)
              : _me_gain_south(som);
  }
  u3t_off(mal_o);

  return som;
}

/* u3a_lose(): lose a reference count.
*/
void
u3a_lose(u3_noun som)
{
  u3t_on(mal_o);
  if ( !_(u3a_is_cat(som)) ) {
    if ( _(u3a_is_north(u3R)) ) {
      _me_lose_north(som);
    } else {
      _me_lose_south(som);
    }
  }
  u3t_off(mal_o);
}

/* u3a_use(): reference count.
*/
c3_w
u3a_use(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) {
    return 1;
  }
  else {
    c3_w*    dog_w = u3a_to_ptr(som);
    u3a_box* box_u = u3a_botox(dog_w);

    return box_u->use_w;
  }
}

/* _ca_wed_who(): unify [a] and [b] on [rod_u], keeping the senior
**
** NB: this leaks a reference when it unifies in a senior road
*/
static c3_o
_ca_wed_who(u3a_road* rod_u, u3_noun* a, u3_noun* b)
{
  c3_t asr_t = ( c3y == u3a_is_senior(rod_u, *a) );
  c3_t bsr_t = ( c3y == u3a_is_senior(rod_u, *b) );
  c3_t nor_t = ( c3y == u3a_is_north(rod_u) );
  c3_t own_t = ( rod_u == u3R );

  //  both are on [rod_u]; gain a reference to whichever we keep
  //
  if ( !asr_t && !bsr_t ) {
    //  keep [a]; it's deeper in the heap
    //
    //    (N && >) || (S && <)
    //
    if ( (*a > *b) == nor_t ) {
      _me_gain_use(*a);
      if ( own_t ) { u3z(*b); }
      *b = *a;
    }
    //  keep [b]; it's deeper in the heap
    //
    else {
      _me_gain_use(*b);
      if ( own_t ) { u3z(*a); }
      *a = *b;
    }

    return c3y;
  }
  //  keep [a]; it's senior
  //
  else if ( asr_t && !bsr_t ) {
    if ( own_t ) { u3z(*b); }
    *b = *a;
    return c3y;
  }
  //  keep [b]; it's senior
  //
  else if ( !asr_t && bsr_t ) {
    if ( own_t ) { u3z(*a); }
    *a = *b;
    return c3y;
  }

  //  both [a] and [b] are senior; we can't unify on [rod_u]
  //
  return c3n;
}

/* u3a_wed(): unify noun references.
*/
void
u3a_wed(u3_noun* a, u3_noun* b)
{
  if ( *a != *b ) {
    u3_road* rod_u = u3R;

    //  while not at home, attempt to unify
    //
    //    we try to unify on our road, and retry on senior roads
    //    until we succeed or reach the home road.
    //
    //    we can't perform this kind of butchery on the home road,
    //    where asynchronous things can allocate.
    //    (XX anything besides u3t_samp?)
    //
    //    when unifying on a higher road, we can't free nouns,
    //    because we can't track junior nouns that point into
    //    that road.
    //
    //    this is just an implementation issue -- we could set use
    //    counts to 0 without actually freeing.  but the allocator
    //    would have to be actually designed for this.
    //    (alternately, we could keep a deferred free-list)
    //
    //    not freeing may generate spurious leaks, so we disable
    //    senior unification when debugging memory.  this will
    //    cause a very slow boot process as the compiler compiles
    //    itself, constantly running into duplicates.
    //
    while ( (rod_u != &u3H->rod_u) &&
            (c3n == _ca_wed_who(rod_u, a, b)) )
    {
#ifdef U3_MEMORY_DEBUG
      break;
#else
      rod_u = u3to(u3_road, rod_u->par_p);
#endif
    }
  }
}

/* u3a_luse(): check refcount sanity.
*/
void
u3a_luse(u3_noun som)
{
  if ( 0 == u3a_use(som) ) {
    fprintf(stderr, "loom: insane %d 0x%x\r\n", som, som);
    abort();
  }
  if ( _(u3du(som)) ) {
    u3a_luse(u3h(som));
    u3a_luse(u3t(som));
  }
}

/* u3a_mark_ptr(): mark a pointer for gc.  Produce size if first mark.
*/
c3_w
u3a_mark_ptr(void* ptr_v)
{
  if ( _(u3a_is_north(u3R)) ) {
    if ( !((ptr_v >= u3a_into(u3R->rut_p)) &&
           (ptr_v < u3a_into(u3R->hat_p))) )
    {
      return 0;
    }
  }
  else {
    if ( !((ptr_v >= u3a_into(u3R->hat_p)) &&
           (ptr_v < u3a_into(u3R->rut_p))) )
    {
      return 0;
    }
  }
  {
    u3a_box* box_u  = u3a_botox(ptr_v);
    c3_w       siz_w;

#ifdef U3_MEMORY_DEBUG
    if ( 0 == box_u->eus_w ) {
      siz_w = box_u->siz_w;
    }
    else if ( 0xffffffff == box_u->eus_w ) {      // see u3a_prof()
      siz_w = 0xffffffff;
      box_u->eus_w = 0;
    }
    else {
      siz_w = 0;
    }
    box_u->eus_w += 1;
#else
    c3_ws use_ws = (c3_ws)box_u->use_w;

    if ( use_ws == 0 ) {
      fprintf(stderr, "%p is bogus\r\n", ptr_v);
      siz_w = 0;
    }
    else {
      u3_assert(use_ws != 0);

      if ( 0x80000000 == (c3_w)use_ws ) {    // see u3a_prof()
        use_ws = -1;
        siz_w = 0xffffffff;
      }
      else if ( use_ws < 0 ) {
        use_ws -= 1;
        siz_w = 0;
      }
      else {
        use_ws = -1;
        siz_w = box_u->siz_w;
      }
      box_u->use_w = (c3_w)use_ws;
    }
#endif
    return siz_w;
  }
}

u3_post
u3a_rewritten(u3_post ptr_v)
{
  u3a_box* box_u = u3a_botox(u3a_into(ptr_v));
  c3_w* box_w = (c3_w*) box_u;
  return (u3_post)box_w[box_u->siz_w - 1];
}

u3_noun
u3a_rewritten_noun(u3_noun som)
{
  if ( c3y == u3a_is_cat(som) ) {
    return som;
  }
  u3_post som_p = u3a_rewritten(u3a_to_off(som));
  if ( c3y == u3a_is_pug(som) ) {
    return u3a_to_pug(som_p);
  }
  else {
    return u3a_to_pom(som_p);
  }
}

/* u3a_mark_mptr(): mark a malloc-allocated ptr for gc.
*/
c3_w
u3a_mark_mptr(void* ptr_v)
{
  c3_w* ptr_w = ptr_v;
  c3_w  pad_w = ptr_w[-1];
  c3_w* org_w = ptr_w - (pad_w + 1);

  return u3a_mark_ptr(org_w);
}

/* u3a_mark_noun(): mark a noun for gc.  Produce size.
*/
c3_w
u3a_mark_noun(u3_noun som)
{
  c3_w siz_w = 0;

  while ( 1 ) {
    if ( _(u3a_is_senior(u3R, som)) ) {
      return siz_w;
    }
    else {
      c3_w* dog_w = u3a_to_ptr(som);
      c3_w  new_w = u3a_mark_ptr(dog_w);

      if ( 0 == new_w || 0xffffffff == new_w ) {      //  see u3a_mark_ptr()
        return siz_w;
      }
      else {
        siz_w += new_w;
        if ( _(u3du(som)) ) {
          siz_w += u3a_mark_noun(u3h(som));
          som = u3t(som);
        }
        else return siz_w;
      }
    }
  }
}

/* u3a_count_noun(): count size of pointer.
*/
c3_w
u3a_count_ptr(void* ptr_v)
{
  if ( _(u3a_is_north(u3R)) ) {
    if ( !((ptr_v >= u3a_into(u3R->rut_p)) &&
           (ptr_v < u3a_into(u3R->hat_p))) )
    {
      return 0;
    }
  }
  else {
    if ( !((ptr_v >= u3a_into(u3R->hat_p)) &&
           (ptr_v < u3a_into(u3R->rut_p))) )
    {
      return 0;
    }
  }
  {
    u3a_box* box_u  = u3a_botox(ptr_v);
    c3_w     siz_w;

    c3_ws use_ws = (c3_ws)box_u->use_w;

    if ( use_ws == 0 ) {
      fprintf(stderr, "%p is bogus\r\n", ptr_v);
      siz_w = 0;
    }
    else {
      u3_assert(use_ws != 0);

      if ( use_ws < 0 ) {
        siz_w = 0;
      }
      else {
        use_ws = -use_ws;
        siz_w = box_u->siz_w;
      }
      box_u->use_w = (c3_w)use_ws;
    }
    return siz_w;
  }
}

/* u3a_count_noun(): count size of noun.
*/
c3_w
u3a_count_noun(u3_noun som)
{
  c3_w siz_w = 0;

  while ( 1 ) {
    if ( _(u3a_is_senior(u3R, som)) ) {
      return siz_w;
    }
    else {
      c3_w* dog_w = u3a_to_ptr(som);
      c3_w  new_w = u3a_count_ptr(dog_w);

      if ( 0 == new_w ) {
        return siz_w;
      }
      else {
        siz_w += new_w;
        if ( _(u3du(som)) ) {
          siz_w += u3a_count_noun(u3h(som));
          som = u3t(som);
        }
        else return siz_w;
      }
    }
  }
}

/* u3a_discount_ptr(): clean up after counting a pointer.
*/
c3_w
u3a_discount_ptr(void* ptr_v)
{
  if ( _(u3a_is_north(u3R)) ) {
    if ( !((ptr_v >= u3a_into(u3R->rut_p)) &&
           (ptr_v < u3a_into(u3R->hat_p))) )
    {
      return 0;
    }
  }
  else {
    if ( !((ptr_v >= u3a_into(u3R->hat_p)) &&
           (ptr_v < u3a_into(u3R->rut_p))) )
    {
      return 0;
    }
  }
  u3a_box* box_u  = u3a_botox(ptr_v);
  c3_w     siz_w;

  c3_ws use_ws = (c3_ws)box_u->use_w;

  if ( use_ws == 0 ) {
    fprintf(stderr, "%p is bogus\r\n", ptr_v);
    siz_w = 0;
  }
  else {
    u3_assert(use_ws != 0);

    if ( use_ws < 0 ) {
      use_ws = -use_ws;
      siz_w = box_u->siz_w;
    }
    else {
      siz_w = 0;
    }
    box_u->use_w = (c3_w)use_ws;
  }

  return siz_w;
}

/* u3a_discount_noun(): clean up after counting a noun.
*/
c3_w
u3a_discount_noun(u3_noun som)
{
  c3_w siz_w = 0;

  while ( 1 ) {
    if ( _(u3a_is_senior(u3R, som)) ) {
      return siz_w;
    }
    else {
      c3_w* dog_w = u3a_to_ptr(som);
      c3_w  new_w = u3a_discount_ptr(dog_w);

      if ( 0 == new_w ) {
        return siz_w;
      }
      else {
        siz_w += new_w;
        if ( _(u3du(som)) ) {
          siz_w += u3a_discount_noun(u3h(som));
          som = u3t(som);
        }
        else return siz_w;
      }
    }
  }
}

/* u3a_print_time: print microsecond time.
*/
void
u3a_print_time(c3_c* str_c, c3_c* cap_c, c3_d mic_d)
{
  u3_assert( 0 != str_c );

  c3_w sec_w = (mic_d / 1000000);
  c3_w mec_w = (mic_d % 1000000) / 1000;
  c3_w mic_w = (mic_d % 1000);

  if ( sec_w ) {
    sprintf(str_c, "%s s/%d.%03d.%03d", cap_c, sec_w, mec_w, mic_w);
  }
  else if ( mec_w ) {
    sprintf(str_c, "%s ms/%d.%03d", cap_c, mec_w, mic_w);
  }
  else {
    sprintf(str_c, "%s \xc2\xb5s/%d", cap_c, mic_w);
  }
}

/* u3a_print_memory: print memory amount.
*/
void
u3a_print_memory(FILE* fil_u, c3_c* cap_c, c3_w wor_w)
{
  u3_assert( 0 != fil_u );

  c3_z byt_z = ((c3_z)wor_w * 4);
  c3_z gib_z = (byt_z / 1000000000);
  c3_z mib_z = (byt_z % 1000000000) / 1000000;
  c3_z kib_z = (byt_z % 1000000) / 1000;
  c3_z bib_z = (byt_z % 1000);

  if ( byt_z ) {
    if ( gib_z ) {
      fprintf(fil_u, "%s: GB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, gib_z, mib_z, kib_z, bib_z);
    }
    else if ( mib_z ) {
      fprintf(fil_u, "%s: MB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, mib_z, kib_z, bib_z);
    }
    else if ( kib_z ) {
      fprintf(fil_u, "%s: KB/%" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, kib_z, bib_z);
    }
    else if ( bib_z ) {
      fprintf(fil_u, "%s: B/%" PRIc3_z "\r\n",
              cap_c, bib_z);
    }
  }
}

/* u3a_maid(): maybe print memory.
*/
c3_w
u3a_maid(FILE* fil_u, c3_c* cap_c, c3_w wor_w)
{
  if ( 0 != fil_u ) {
    u3a_print_memory(fil_u, cap_c, wor_w);
  }
  return wor_w;
}

/* _ca_print_memory(): un-captioned u3a_print_memory().
*/
static void
_ca_print_memory(FILE* fil_u, c3_w wor_w)
{
  c3_w byt_w = (wor_w * 4);
  c3_w gib_w = (byt_w / 1000000000);
  c3_w mib_w = (byt_w % 1000000000) / 1000000;
  c3_w kib_w = (byt_w % 1000000) / 1000;
  c3_w bib_w = (byt_w % 1000);

  if ( gib_w ) {
    fprintf(fil_u, "GB/%d.%03d.%03d.%03d\r\n",
            gib_w, mib_w, kib_w, bib_w);
  }
  else if ( mib_w ) {
    fprintf(fil_u, "MB/%d.%03d.%03d\r\n", mib_w, kib_w, bib_w);
  }
  else if ( kib_w ) {
    fprintf(fil_u, "KB/%d.%03d\r\n", kib_w, bib_w);
  }
  else {
    fprintf(fil_u, "B/%d\r\n", bib_w);
  }
}

/* u3a_prof(): mark/measure/print memory profile. RETAIN.
*/
c3_w
u3a_prof(FILE* fil_u, c3_w den_w, u3_noun mas)
{
  c3_w tot_w = 0;
  u3_noun h_mas, t_mas;

  if ( c3n == u3r_cell(mas, &h_mas, &t_mas) ) {
    fprintf(fil_u, "%.*smistyped mass\r\n", den_w, "");
    return tot_w;
  }
  else if ( _(u3du(h_mas)) ) {
    fprintf(fil_u, "%.*smistyped mass head\r\n", den_w, "");
    {
      c3_c* lab_c = u3m_pretty(h_mas);
      fprintf(fil_u, "h_mas: %s", lab_c);
      c3_free(lab_c);
    }
    return tot_w;
  }
  else {
    {
      c3_c* lab_c = u3m_pretty(h_mas);
      fprintf(fil_u, "%*s%s: ", den_w, "", lab_c);
      c3_free(lab_c);
    }

    u3_noun it_mas, tt_mas;

    if ( c3n == u3r_cell(t_mas, &it_mas, &tt_mas) ) {
      fprintf(fil_u, "%*smistyped mass tail\r\n", den_w, "");
      return tot_w;
    }
    else if ( c3y == it_mas ) {
      tot_w += u3a_mark_noun(tt_mas);
      _ca_print_memory(fil_u, tot_w);

#if 1
      /* The basic issue here is that tt_mas is included in .sac
       * (the whole profile), so they can't both be roots in the
       * normal sense. When we mark .sac later on, we want tt_mas
       * to appear unmarked, but its children should be already
       * marked.
       *
       * see u3a_mark_ptr().
      */
      if ( _(u3a_is_dog(tt_mas)) ) {
        u3a_box* box_u = u3a_botox(u3a_to_ptr(tt_mas));
#ifdef U3_MEMORY_DEBUG
        if ( 1 == box_u->eus_w ) {
          box_u->eus_w = 0xffffffff;
        }
        else {
          box_u->eus_w -= 1;
        }
#else
        if ( -1 == (c3_w)box_u->use_w ) {
          box_u->use_w = 0x80000000;
        }
        else {
          box_u->use_w += 1;
        }
#endif
      }
#endif

      return tot_w;
    }
    else if ( c3n == it_mas ) {
      fprintf(fil_u, "\r\n");

      while ( _(u3du(tt_mas)) ) {
        tot_w += u3a_prof(fil_u, den_w+2, u3h(tt_mas));
        tt_mas = u3t(tt_mas);
      }

      fprintf(fil_u, "%*s--", den_w, "");
      _ca_print_memory(fil_u, tot_w);

      return tot_w;

    }
    else {
      fprintf(fil_u, "%*smistyped (strange) mass tail\r\n", den_w, "");
      return tot_w;
    }
  }
}

/* u3a_mark_road(): mark ad-hoc persistent road structures.
*/
c3_w
u3a_mark_road(FILE* fil_u)
{
  c3_w tot_w = 0;
  tot_w += u3a_maid(fil_u, "  namespace", u3a_mark_noun(u3R->ski.gul));
  tot_w += u3a_maid(fil_u, "  trace stack", u3a_mark_noun(u3R->bug.tax));
  tot_w += u3a_maid(fil_u, "  trace buffer", u3a_mark_noun(u3R->bug.mer));
  tot_w += u3a_maid(fil_u, "  profile batteries", u3a_mark_noun(u3R->pro.don));
  tot_w += u3a_maid(fil_u, "  profile doss", u3a_mark_noun(u3R->pro.day));
  tot_w += u3a_maid(fil_u, "  new profile trace", u3a_mark_noun(u3R->pro.trace));
  tot_w += u3a_maid(fil_u, "  transient memoization cache", u3h_mark(u3R->cax.har_p));
  tot_w += u3a_maid(fil_u, "  persistent memoization cache", u3h_mark(u3R->cax.per_p));
  return   u3a_maid(fil_u, "total road stuff", tot_w);
}

/* u3a_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3a_reclaim(void)
{
  //  clear the memoization cache
  //
  u3h_free(u3R->cax.har_p);
  u3R->cax.har_p = u3h_new();
}

/* u3a_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
*/
void
u3a_rewrite_compact(void)
{
  u3a_rewrite_noun(u3R->ski.gul);
  u3a_rewrite_noun(u3R->bug.tax);
  u3a_rewrite_noun(u3R->bug.mer);
  u3a_rewrite_noun(u3R->pro.don);
  u3a_rewrite_noun(u3R->pro.day);
  u3a_rewrite_noun(u3R->pro.trace);
  u3h_rewrite(u3R->cax.har_p);
  u3h_rewrite(u3R->cax.per_p);

  u3R->ski.gul = u3a_rewritten_noun(u3R->ski.gul);
  u3R->bug.tax = u3a_rewritten_noun(u3R->bug.tax);
  u3R->bug.mer = u3a_rewritten_noun(u3R->bug.mer);
  u3R->pro.don = u3a_rewritten_noun(u3R->pro.don);
  u3R->pro.day = u3a_rewritten_noun(u3R->pro.day);
  u3R->pro.trace = u3a_rewritten_noun(u3R->pro.trace);
  u3R->cax.har_p = u3a_rewritten(u3R->cax.har_p);
  u3R->cax.per_p = u3a_rewritten(u3R->cax.per_p);
}

/* _ca_print_box(): heuristically print the contents of an allocation box.
*/
static c3_c*
_ca_print_box(u3a_box* box_u)
{
  //  the loom offset pointing to the contents of box_u
  //
  c3_w box_w = u3a_outa(u3a_boxto(box_u));
  //  box_u might not be a cell, we use the struct to inspect further
  //
  u3a_cell* cel_u = (u3a_cell*)box_u;

  if (  //  a cell will never be bigger than the minimum allocation size
        //
        (u3a_minimum < box_u->siz_w) ||
        //  this condition being true potentially corresponds to
        //  box_u containing an indirect atom of only one word.
        //  if the condition is false, we know box_u contains a cell.
        //
        ( (1 == (c3_w)cel_u->hed) &&
          (0x80000000 & (c3_w)cel_u->tel) ) )
  {
    //  box_u might not be an indirect atom,
    //  but it's always safe to print it as if it is one
    //
    u3a_atom* vat_u = (u3a_atom*)box_u;
    u3_atom   veb   = u3a_to_pug(box_w);

    //  skip atoms larger than 10 words
    //  XX print mugs or something
    //
    if ( 10 > vat_u->len_w ) {
#if 0
      /*  For those times when you've really just got to crack open
       *  the box and see what's inside
      */
      {
        int i;
        for ( i = 0; i < box_u->siz_w; i++ ) {
          fprintf(stderr, "%08x ", (unsigned int)(((c3_w*)box_u)[i]));
        }
        fprintf(stderr, "\r\n");
      }
#endif
      return 0;
    }

    return u3m_pretty(veb);
  }
  else {
    //  box_u is definitely a cell
    //
    return u3m_pretty(u3a_to_pom(box_w));
  }
}

/* _ca_print_leak(): print the details of a leaked allocation box.
*/
#ifdef U3_MEMORY_DEBUG

static void
_ca_print_leak(c3_c* cap_c, u3a_box* box_u, c3_w eus_w, c3_w use_w)
{
  fprintf(stderr, "%s: %p mug=%x (marked=%u swept=%u)\r\n",
                  cap_c,
                  box_u,
                  ((u3a_noun *)(u3a_boxto(box_u)))->mug_w,
                  eus_w,
                  use_w);

  if ( box_u->cod_w ) {
    c3_c* cod_c = u3m_pretty(box_u->cod_w);
    fprintf(stderr, "code: %s\r\n", cod_c);
    c3_free(cod_c);
  }

  u3a_print_memory(stderr, "    size", box_u->siz_w);

  {
    c3_c* dat_c = _ca_print_box(box_u);
    fprintf(stderr, "    data: %s\r\n", dat_c);
    c3_free(dat_c);
  }
}

#else

static void
_ca_print_leak(c3_c* cap_c, u3a_box* box_u, c3_ws use_ws)
{
  fprintf(stderr, "%s: %p mug=%x swept=%d\r\n",
                  cap_c,
                  box_u,
                  ((u3a_noun *)(u3a_boxto(box_u)))->mug_w,
                  use_ws);

  u3a_print_memory(stderr, "    size", box_u->siz_w);

  {
    c3_c* dat_c = _ca_print_box(box_u);
    fprintf(stderr, "    data: %s\r\n", dat_c);
    c3_free(dat_c);
  }
}

#endif

/* u3a_idle(): measure free-lists in [rod_u]
*/
c3_w
u3a_idle(u3a_road* rod_u)
{
  c3_w i_w, fre_w = 0;

  for ( i_w = 0; i_w < u3a_fbox_no; i_w++ ) {
    u3p(u3a_fbox) fre_p = rod_u->all.fre_p[i_w];

    while ( fre_p ) {
      u3a_fbox* fox_u = u3to(u3a_fbox, fre_p);

      fre_w += fox_u->box_u.siz_w;
      fre_p  = fox_u->nex_p;
    }
  }

  return fre_w;
}

/* u3a_sweep(): sweep a fully marked road.
*/
c3_w
u3a_sweep(void)
{
  c3_w neg_w, pos_w, leq_w, weq_w;

  /* Measure allocated memory by counting the free list.
  */
  {
    c3_w end_w = u3a_heap(u3R);
    c3_w fre_w = u3a_idle(u3R);

#ifdef U3_CPU_DEBUG
    if ( fre_w != u3R->all.fre_w ) {
      fprintf(stderr, "fre discrepancy (%x): %x, %x, %x\r\n", u3R->par_p,
                      fre_w, u3R->all.fre_w, (u3R->all.fre_w - fre_w));
    }
#endif
    neg_w = (end_w - fre_w);
  }

  /* Sweep through the arena, repairing and counting leaks.
  */
  pos_w = leq_w = weq_w = 0;
  {
    u3_post box_p = _(u3a_is_north(u3R)) ? u3R->rut_p : u3R->hat_p;
    u3_post end_p = _(u3a_is_north(u3R)) ? u3R->hat_p : u3R->rut_p;
    c3_w*   box_w = u3a_into(box_p);
    c3_w*   end_w = u3a_into(end_p);

    while ( box_w < end_w ) {
      u3a_box* box_u = (void *)box_w;

#ifdef U3_MEMORY_DEBUG
      /* I suspect these printfs fail hilariously in the case
       * of non-direct atoms. We shouldn't unconditionally run
       * u3a_to_pom(). In general, the condition
       * box_u->siz_w > u3a_minimum is sufficient, but not necessary,
       * for the box to represent an atom.  The atoms between
       * 2^31 and 2^32 are the exceptions.
       *
       * Update: so, apparently u3.md is incorrect, and a pug is just
       * an indirect atom.  This code should be altered to handle
       * that.
      */
      if ( box_u->use_w != box_u->eus_w ) {
        if ( box_u->eus_w != 0 ) {
          if ( box_u->use_w == 0 ) {
            _ca_print_leak("dank", box_u, box_u->eus_w, box_u->use_w);
          }
          else {
            _ca_print_leak("weak", box_u, box_u->eus_w, box_u->use_w);
          }

          weq_w += box_u->siz_w;
        }
        else {
          _ca_print_leak("leak", box_u, box_u->eus_w, box_u->use_w);

          leq_w += box_u->siz_w;
        }

        box_u->use_w = box_u->eus_w;
      }
      else {
        if ( box_u->use_w ) {
          pos_w += box_u->siz_w;
        }
      }
      box_u->eus_w = 0;
#else
      c3_ws use_ws = (c3_ws)box_u->use_w;

      if ( use_ws > 0 ) {
        _ca_print_leak("leak", box_u, use_ws);

        leq_w += box_u->siz_w;
        box_u->use_w = 0;

        _box_attach(box_u);
      }
      else if ( use_ws < 0 ) {
        pos_w += box_u->siz_w;
        box_u->use_w = (c3_w)(0 - use_ws);
      }
#endif
      box_w += box_u->siz_w;
    }
  }

#ifdef U3_MEMORY_DEBUG
  {
    c3_w tot_w = u3a_full(u3R);
    c3_w caf_w = u3a_temp(u3R);

#ifdef U3_CPU_DEBUG
    if ( (0 != u3R->par_p) && (u3R->all.max_w > 1000000) ) {
      u3a_print_memory(stderr, "available", (tot_w - pos_w));
      u3a_print_memory(stderr, "allocated", pos_w);
      u3a_print_memory(stderr, "volatile", caf_w);

      u3a_print_memory(stderr, "maximum", u3R->all.max_w);
    }
#endif

#if 0
    u3a_print_memory(stderr, "available", (tot_w - pos_w));
    u3a_print_memory(stderr, "allocated", pos_w);
    u3a_print_memory(stderr, "volatile", caf_w);
#endif
  }
#endif

  u3a_print_memory(stderr, "leaked", leq_w);
  u3a_print_memory(stderr, "weaked", weq_w);

  u3_assert( (pos_w + leq_w + weq_w) == neg_w );
  u3_assert( (0 == leq_w) && (0 == weq_w) );

  return neg_w;
}

/* u3a_pack_seek(): sweep the heap, modifying boxes to record new addresses.
*/
void
u3a_pack_seek(u3a_road* rod_u)
{
  //  the heap in [rod_u] is swept from "front" to "back".
  //  new locations are calculated for each in-use allocation box
  //  (simply the "deepest" linearly-available location),
  //  and stored in the box itself
  //
  //  box_w: front of the heap
  //  end_w: back of the heap
  //  new_p: initial new location (data of first box)
  //
  c3_w*    box_w = u3a_into(rod_u->rut_p);
  c3_w*    end_w = u3a_into(rod_u->hat_p);
  u3_post  new_p = (rod_u->rut_p + c3_wiseof(u3a_box));
  u3a_box* box_u;
  c3_w     siz_w;

  if ( c3y == u3a_is_north(rod_u) ) {
    //  north roads are swept low to high
    //
    //    new locations are recorded in the trailing size word
    //
    while ( box_w < end_w ) {
      box_u = (void *)box_w;
      siz_w = box_u->siz_w;

      if ( box_u->use_w ) {
        box_w[siz_w - 1] = new_p;
        new_p += siz_w;
      }

      box_w += siz_w;
    }
  }
  //  XX untested!
  //
  else {
    //  south roads are swept high to low
    //
    //    new locations are recorded in the leading size word
    //
    //    since we traverse backward, [siz_w] holds the size of the next box,
    //    and we must initially offset to point to the head of the first box
    //
    siz_w  = box_w[-1];
    box_w -= siz_w;
    new_p -= siz_w;

    while ( end_w < box_w ) {
      box_u = (void *)box_w;
      siz_w = box_w[-1];

      if ( box_u->use_w ) {
        box_u->siz_w = new_p;
        new_p -= siz_w;
      }

      box_w -= siz_w;
    }
  }
}
static u3_post
_ca_pack_move_north(c3_w* box_w, c3_w* end_w, u3_post new_p)
{
  u3a_box* old_u;
  c3_w     siz_w;

  //  relocate allocation boxes
  //
  //    new locations have been recorded in the trailing size word,
  //    and are recalculated and asserted to ensure sanity
  //
  while ( box_w < end_w ) {
    old_u = (void *)box_w;
    siz_w = old_u->siz_w;

    old_u->use_w &= 0x7fffffff;

    if ( old_u->use_w ) {
      c3_w* new_w = (void*)u3a_botox(u3a_into(new_p));

      u3_assert( box_w[siz_w - 1] == new_p );

      //  note: includes leading size
      //
      if ( new_w < box_w ) {
        c3_w i_w;

        for ( i_w = 0; i_w < siz_w - 1; i_w++ ) {
          new_w[i_w] = box_w[i_w];
        }
      }
      else {
        u3_assert( new_w == box_w );
      }

      //  restore trailing size
      //
      new_w[siz_w - 1] = siz_w;

      new_p += siz_w;
    }

    box_w += siz_w;
  }

  return new_p;
}

//  XX untested!
//
static u3_post
_ca_pack_move_south(c3_w* box_w, c3_w* end_w, u3_post new_p)
{
  u3a_box* old_u;
  c3_w     siz_w;
  c3_o     yuz_o;

  //  offset initial addresses (point to the head of the first box)
  //
  siz_w  = box_w[-1];
  box_w -= siz_w;
  new_p -= siz_w;

  //  relocate allocation boxes
  //
  //    new locations have been recorded in the leading size word,
  //    and are recalculated and asserted to ensure sanity
  //
  while ( 1 ) {
    old_u = (void *)box_w;

    old_u->use_w &= 0x7fffffff;

    if ( old_u->use_w ) {
      c3_w* new_w = (void*)u3a_botox(u3a_into(new_p));

      u3_assert( old_u->siz_w == new_p );

      //  note: includes trailing size
      //
      if ( new_w > box_w ) {
        c3_w i_w;

        for ( i_w = 1; i_w < siz_w; i_w++ ) {
          new_w[i_w] = box_w[i_w];
        }
      }
      else {
        u3_assert( new_w == box_w );
      }

      //  restore leading size
      //
      new_w[0] = siz_w;

      yuz_o = c3y;
    }
    else {
      yuz_o = c3n;
    }

    //  move backwards only if there is more work to be done
    //
    if ( box_w > end_w ) {
      siz_w  = box_w[-1];
      box_w -= siz_w;

      if ( c3y == yuz_o ) {
        new_p -= siz_w;
      }
    }
    else {
      u3_assert( end_w == box_w );
      break;
    }
  }

  return new_p;
}

/* u3a_pack_move(): sweep the heap, moving boxes to new addresses.
*/
void
u3a_pack_move(u3a_road* rod_u)
{
  //  box_w: front of the heap
  //  end_w: back of the heap
  //  new_p: initial new location (data of first box)
  //  las_p: newly calculated last location
  //
  c3_w*   box_w = u3a_into(rod_u->rut_p);
  c3_w*   end_w = u3a_into(rod_u->hat_p);
  u3_post new_p = (rod_u->rut_p + c3_wiseof(u3a_box));
  u3_post las_p = ( c3y == u3a_is_north(rod_u) )
                  ? _ca_pack_move_north(box_w, end_w, new_p)
                  : _ca_pack_move_south(box_w, end_w, new_p);

  rod_u->hat_p  = (las_p - c3_wiseof(u3a_box));

  //  clear free lists and cell allocator
  //
  {
    c3_w i_w;
    for ( i_w = 0; i_w < u3a_fbox_no; i_w++ ) {
      u3R->all.fre_p[i_w] = 0;
    }

    u3R->all.fre_w = 0;
    u3R->all.cel_p = 0;
  }
}

/* u3a_rewrite_ptr(): mark a pointer as already having been rewritten
*/
c3_o
u3a_rewrite_ptr(void* ptr_v)
{
  u3a_box* box_u = u3a_botox(ptr_v);
  if ( box_u->use_w & 0x80000000 ) {
    /* Already rewritten.
    */
    return c3n;
  }
  box_u->use_w |= 0x80000000;
  return c3y;
}

void
u3a_rewrite_noun(u3_noun som)
{
  if ( c3n == u3a_is_cell(som) ) {
    return;
  }

  if ( c3n == u3a_rewrite_ptr(u3a_to_ptr((som))) ) return;

  u3a_cell* cel = u3a_to_ptr(som);

  u3a_rewrite_noun(cel->hed);
  u3a_rewrite_noun(cel->tel);

  cel->hed = u3a_rewritten_noun(cel->hed);
  cel->tel = u3a_rewritten_noun(cel->tel);
}

#if 0
/* _ca_detect(): in u3a_detect().
*/
static c3_d
_ca_detect(u3p(u3h_root) har_p, u3_noun fum, u3_noun som, c3_d axe_d)
{
  while ( 1 ) {
    if ( som == fum ) {
      return axe_d;
    }
    else if ( !_(u3du(fum)) || (u3_none != u3h_get(har_p, fum)) ) {
      return 0;
    }
    else {
      c3_d eax_d;

      u3h_put(har_p, fum, 0);

      if ( 0 != (eax_d = _ca_detect(har_p, u3h(fum), som, 2ULL * axe_d)) ) {
        return c3y;
      }
      else {
        fum = u3t(fum);
        axe_d = (2ULL * axe_d) + 1;
      }
    }
  }
}

/* u3a_detect(): for debugging, check if (som) is referenced from (fum).
**
** (som) and (fum) are both RETAINED.
*/
c3_d
u3a_detect(u3_noun fum, u3_noun som)
{
  u3p(u3h_root) har_p = u3h_new();
  c3_o          ret_o;

  ret_o = _ca_detect(har_p, fum, som, 1);
  u3h_free(har_p);

  return ret_o;
}
#endif

#ifdef U3_MEMORY_DEBUG
/* u3a_lush(): leak push.
*/
c3_w
u3a_lush(c3_w lab_w)
{
  c3_w cod_w = u3_Code;

  u3_Code = lab_w;
  return cod_w;
}

/* u3a_lop(): leak pop.
*/
void
u3a_lop(c3_w lab_w)
{
  u3_Code = lab_w;
}
#else
/* u3a_lush(): leak push.
*/
c3_w
u3a_lush(c3_w lab_w)
{
  return 0;
}

/* u3a_lop(): leak pop.
*/
void
u3a_lop(c3_w lab_w)
{
}
#endif

/* u3a_walk_fore(): preorder traversal, visits ever limb of a noun.
**
**   cells are visited *before* their heads and tails
**   and can shortcircuit traversal by returning [c3n]
*/
void
u3a_walk_fore(u3_noun    a,
              void*      ptr_v,
              void     (*pat_f)(u3_atom, void*),
              c3_o     (*cel_f)(u3_noun, void*))
{
  u3_noun*   top;
  u3a_pile pil_u;

  //  initialize stack control; push argument
  //
  u3a_pile_prep(&pil_u, sizeof(u3_noun));
  top  = u3a_push(&pil_u);
  *top = a;

  while ( c3n == u3a_pile_done(&pil_u) ) {
    //  visit an atom, then pop the stack
    //
    if ( c3y == u3a_is_atom(a) ) {
      pat_f(a, ptr_v);
      top = u3a_pop(&pil_u);
    }
    //  vist a cell, if c3n, pop the stack
    //
    else if ( c3n == cel_f(a, ptr_v) ) {
      top = u3a_pop(&pil_u);
    }
    //  otherwise, push the tail and continue into the head
    //
    else {
      *top = u3t(a);
      top  = u3a_push(&pil_u);
      *top = u3h(a);
    }

    a = *top;
  }
}

/* u3a_string(): `a` as an on-loom c-string.
*/
c3_c*
u3a_string(u3_atom a)
{
  c3_w  met_w = u3r_met(3, a);
  c3_c* str_c = u3a_malloc(met_w + 1);

  u3r_bytes(0, met_w, (c3_y*)str_c, a);
  str_c[met_w] = 0;
  return str_c;
}

/* u3a_loom_sane(): sanity checks the state of the loom for obvious corruption
 */
void
u3a_loom_sane()
{
  /*
    Only checking validity of freelists for now. Other checks could be added,
    e.g. noun HAMT traversal, boxwise traversal of loom validating `siz_w`s,
    `use_w`s, no empty space, etc. If added, some of that may need to be guarded
    behind C3DBG flags. Freelist traversal is probably fine to always do though.
  */
  for (c3_w i_w = 0; i_w < u3a_fbox_no; i_w++) {
    u3p(u3a_fbox) this_p = u3R->all.fre_p[i_w];
    u3a_fbox     *this_u = u3to(u3a_fbox, this_p);
    for (; this_p
           ; this_p = this_u->nex_p
           , this_u = u3to(u3a_fbox, this_p)) {
      u3p(u3a_fbox) pre_p = this_u->pre_p
        ,           nex_p = this_u->nex_p;
      u3a_fbox *pre_u = u3to(u3a_fbox, this_u->pre_p)
        ,      *nex_u = u3to(u3a_fbox, this_u->nex_p);

      if (nex_p && nex_u->pre_p != this_p) u3_assert(!"loom: wack");
      if (pre_p && pre_u->nex_p != this_p) u3_assert(!"loom: wack");
      if (!pre_p                /* this must be the head of a freelist */
          && u3R->all.fre_p[_box_slot(this_u->box_u.siz_w)] != this_p)
        u3_assert(!"loom: wack");
    }
  }
}
