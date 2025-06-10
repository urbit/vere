#include "v4.h"


/***  allocate.c
***/

static c3_w
_box_v4_slot(c3_w siz_w)
{
  if ( u3a_v4_minimum == siz_w ) {
    return 0;
  }
  else if ( !(siz_w >> 4) ) {
    return 1;
  }
  else {
    c3_w bit_w = c3_bits_word(siz_w) - 3;
    c3_w max_w = u3a_v4_fbox_no - 1;
    return c3_min(bit_w, max_w);
  }
}

static u3a_v4_box*
_box_v4_make(void* box_v, c3_w siz_w, c3_w use_w)
{
  u3a_v4_box* box_u = box_v;
  c3_w*    box_w = box_v;

  u3_assert(siz_w >= u3a_v4_minimum);

  box_u->siz_w = siz_w;
  box_w[siz_w - 1] = siz_w;     /* stor size at end of allocation as well */
  box_u->use_w = use_w;

  return box_u;
}

/* _box_v4_attach(): attach a box to the free list.
*/
static void
_box_v4_attach(u3a_v4_box* box_u)
{
  u3_assert(box_u->siz_w >= (1 + c3_wiseof(u3a_v4_fbox)));
  u3_assert(0 != u3v4of(u3a_v4_fbox, box_u));

  {
    c3_w           sel_w = _box_v4_slot(box_u->siz_w);
    u3p(u3a_v4_fbox)  fre_p = u3v4of(u3a_v4_fbox, box_u);
    u3p(u3a_v4_fbox)* pfr_p = &u3R_v4->all.fre_p[sel_w];
    u3p(u3a_v4_fbox)  nex_p = *pfr_p;

    u3v4to(u3a_v4_fbox, fre_p)->pre_p = 0;
    u3v4to(u3a_v4_fbox, fre_p)->nex_p = nex_p;
    if ( u3v4to(u3a_v4_fbox, fre_p)->nex_p ) {
      u3v4to(u3a_v4_fbox, u3v4to(u3a_v4_fbox, fre_p)->nex_p)->pre_p = fre_p;
    }
    (*pfr_p) = fre_p;
  }
}

/* _box_v4_detach(): detach a box from the free list.
*/
static void
_box_v4_detach(u3a_v4_box* box_u)
{
  u3p(u3a_v4_fbox) fre_p = u3v4of(u3a_v4_fbox, box_u);
  u3p(u3a_v4_fbox) pre_p = u3v4to(u3a_v4_fbox, fre_p)->pre_p;
  u3p(u3a_v4_fbox) nex_p = u3v4to(u3a_v4_fbox, fre_p)->nex_p;

  if ( nex_p ) {
    if ( u3v4to(u3a_v4_fbox, nex_p)->pre_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3v4to(u3a_v4_fbox, nex_p)->pre_p = pre_p;
  }
  if ( pre_p ) {
    if( u3v4to(u3a_v4_fbox, pre_p)->nex_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3v4to(u3a_v4_fbox, pre_p)->nex_p = nex_p;
  }
  else {
    c3_w sel_w = _box_v4_slot(box_u->siz_w);

    if ( fre_p != u3R_v4->all.fre_p[sel_w] ) {
      u3_assert(!"loom: corrupt");
    }
    u3R_v4->all.fre_p[sel_w] = nex_p;
  }
}

/* _ca_box_make_hat(): allocate directly on the hat.
*/
static u3a_v4_box*
_ca_v4_box_make_hat(c3_w len_w, c3_w ald_w, c3_w off_w, c3_w use_w)
{
  c3_w
    pad_w,                      /* padding between returned pointer and box */
    siz_w;                      /* total size of allocation */
  u3_post
    box_p,                      /* start of box */
    all_p;                      /* start of returned pointer */

  //  NB: always north
  {
    box_p = all_p = u3R_v4->hat_p;
    all_p += c3_wiseof(u3a_v4_box) + off_w;
    pad_w = c3_align(all_p, ald_w, C3_ALGHI)
      - all_p;
    siz_w = c3_align(len_w + pad_w, u3a_v4_walign, C3_ALGHI);

    if ( (siz_w >= (u3R_v4->cap_p - u3R_v4->hat_p)) ) {
      //  XX wat do
      fprintf(stderr, "bail: meme\r\n");
      abort();
      return 0;
    }
    u3R_v4->hat_p += siz_w;
  }
  

  return _box_v4_make(u3a_v4_into(box_p), siz_w, use_w);
}


/* _ca_willoc(): u3a_v4_walloc() internals.
*/
static void*
_ca_v4_willoc(c3_w len_w, c3_w ald_w, c3_w off_w)
{
  c3_w siz_w = c3_max(u3a_v4_minimum, u3a_v4_boxed(len_w));
  c3_w sel_w = _box_v4_slot(siz_w);

  if ( (sel_w != 0) && (sel_w != u3a_v4_fbox_no - 1) ) {
    sel_w += 1;
  }

  while ( 1 ) {
    u3p(u3a_v4_fbox) *pfr_p = &u3R_v4->all.fre_p[sel_w];

    while ( 1 ) {
      /* increment until we get a non-null freelist */
      if ( 0 == *pfr_p ) {
        if ( sel_w < (u3a_v4_fbox_no - 1) ) {
          sel_w += 1;
          break;
        }
        else {
          //  nothing in top free list; chip away at the hat
          //
          u3a_v4_box* box_u;
          box_u = _ca_v4_box_make_hat(siz_w, ald_w, off_w, 1);
          return u3a_v4_boxto(box_u);
        }
      }
      else {                    /* we got a non-null freelist */
        u3_post all_p = *pfr_p;
        all_p += c3_wiseof(u3a_v4_box) + off_w;
        c3_w pad_w = c3_align(all_p, ald_w, C3_ALGHI) - all_p;
        c3_w des_w = c3_align(siz_w + pad_w, u3a_v4_walign, C3_ALGHI);

        if ( (des_w) > u3v4to(u3a_v4_fbox, *pfr_p)->box_u.siz_w ) {
          /* This free block is too small.  Continue searching.
          */
          pfr_p = &(u3v4to(u3a_v4_fbox, *pfr_p)->nex_p);
          continue;
        }
        else {                  /* free block fits desired alloc size */
          u3a_v4_box* box_u = &(u3v4to(u3a_v4_fbox, *pfr_p)->box_u);

          /* We have found a free block of adequate size.  Remove it
          ** from the free list.
          */
          /* misc free list consistency checks.
            TODO: in the future should probably only run for C3DBG builds */
          {
            if ( (0 != u3v4to(u3a_v4_fbox, *pfr_p)->pre_p) &&
                 (u3v4to(u3a_v4_fbox, u3v4to(u3a_v4_fbox, *pfr_p)->pre_p)->nex_p
                    != (*pfr_p)) )
            {                   /* this->pre->nex isn't this */
              u3_assert(!"loom: corrupt");
            }

            if( (0 != u3v4to(u3a_v4_fbox, *pfr_p)->nex_p) &&
                (u3v4to(u3a_v4_fbox, u3v4to(u3a_v4_fbox, *pfr_p)->nex_p)->pre_p
                   != (*pfr_p)) )
            {                   /* this->nex->pre isn't this */
              u3_assert(!"loom: corrupt");
            }

            /* pop the block */
            /* this->nex->pre = this->pre  */
            if ( 0 != u3v4to(u3a_v4_fbox, *pfr_p)->nex_p ) {
              u3v4to(u3a_v4_fbox, u3v4to(u3a_v4_fbox, *pfr_p)->nex_p)->pre_p =
                u3v4to(u3a_v4_fbox, *pfr_p)->pre_p;
            }
            /* this = this->nex */
            *pfr_p = u3v4to(u3a_v4_fbox, *pfr_p)->nex_p;
          }

          /* If we can chop off another block, do it.
          */
          if ( (des_w + u3a_v4_minimum) <= box_u->siz_w ) {
            /* Split the block.
            */
            c3_w* box_w = ((c3_w *)(void *)box_u);
            c3_w* end_w = box_w + des_w;
            c3_w  lef_w = (box_u->siz_w - des_w);

            _box_v4_attach(_box_v4_make(end_w, lef_w, 0));
            return u3a_v4_boxto(_box_v4_make(box_w, des_w, 1));
          }
          else {
            u3_assert(0 == box_u->use_w);
            box_u->use_w = 1;
            return u3a_v4_boxto(box_u);
          }
        }
      }
    }
  }
}

static void*
_ca_v4_walloc(c3_w len_w, c3_w ald_w, c3_w off_w)
{
  void* ptr_v;
  ptr_v = _ca_v4_willoc(len_w, ald_w, off_w);
  return ptr_v;
}

/* u3a_v4_walloc(): allocate storage words on hat heap.
*/
void*
u3a_v4_walloc(c3_w len_w)
{
  void* ptr_v;
  ptr_v = _ca_v4_walloc(len_w, 1, 0);
  return ptr_v;
}


/* _box_v4_free(): free and coalesce.
*/
static void
_box_v4_free(u3a_v4_box* box_u)
{
  c3_w* box_w = (c3_w *)(void *)box_u;

  u3_assert(box_u->use_w != 0);
  box_u->use_w -= 1;
  if ( 0 != box_u->use_w ) {
    return;
  }

  //  always north
  { /* north */
    /* Try to coalesce with the block below.
    */
    if ( box_w != u3a_v4_into(u3R_v4->rut_p) ) {
      c3_w       laz_w = *(box_w - 1); /* the size of a box stored at the end of its allocation */
      u3a_v4_box* pox_u = (u3a_v4_box*)(void *)(box_w - laz_w); /* the head of the adjacent box below */

      if ( 0 == pox_u->use_w ) {
        _box_v4_detach(pox_u);
        _box_v4_make(pox_u, (laz_w + box_u->siz_w), 0);

        box_u = pox_u;
        box_w = (c3_w*)(void *)pox_u;
      }
    }

    /* Try to coalesce with the block above, or the wilderness.
    */
    if ( (box_w + box_u->siz_w) == u3a_v4_into(u3R_v4->hat_p) ) {
      u3R_v4->hat_p = u3a_v4_outa(box_w);
    }
    else {
      u3a_v4_box* nox_u = (u3a_v4_box*)(void *)(box_w + box_u->siz_w);

      if ( 0 == nox_u->use_w ) {
        _box_v4_detach(nox_u);
        _box_v4_make(box_u, (box_u->siz_w + nox_u->siz_w), 0);
      }
      _box_v4_attach(box_u);
    }
  }      /* end north */
}

/* u3a_v4_wfree(): free storage.
*/
void
u3a_v4_wfree(void* tox_v)
{
  _box_v4_free(u3a_v4_botox(tox_v));
}

/* u3a_v4_free(): free for aligned malloc.
*/
void
u3a_v4_free(void* tox_v)
{
  if (NULL == tox_v)
    return;

  c3_w* tox_w = tox_v;
  c3_w  pad_w = tox_w[-1];
  c3_w* org_w = tox_w - (pad_w + 1);

  u3a_v4_wfree(org_w);
}

/* _me_lose_north(): lose on a north road.
*/
static void
_me_v4_lose_north(u3_noun dog)
{
top:
  if ( c3y == u3a_v4_north_is_normal(u3R_v4, dog) ) {
    c3_w*       dog_w = u3a_v4_to_ptr(dog);
    u3a_v4_box* box_u = u3a_v4_botox(dog_w);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        fprintf(stderr, "bail: foul\r\n");
        abort();
      }
      else {
        if ( _(u3a_v4_is_pom(dog)) ) {
          u3a_v4_cell* dog_u = (void *)dog_w;
          u3_noun     h_dog = dog_u->hed;
          u3_noun     t_dog = dog_u->tel;

          if ( !_(u3a_v4_is_cat(h_dog)) ) {
            _me_v4_lose_north(h_dog);
          }
          u3a_v4_wfree(dog_w); //  same as cfree at home
          if ( !_(u3a_v4_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_v4_wfree(dog_w);
        }
      }
    }
  }
}

/* u3a_v4_head(): get the head of som noun.
*/
u3_noun
u3a_v4_head(u3_noun som)
{
  return u3a_v4_h(som);
}

/* u3a_v4_tail(): get the tail of som noun.
*/
u3_noun
u3a_v4_tail(u3_noun som)
{
  return u3a_v4_t(som);
}

/* u3a_v4_lose(): lose a reference count.
*/
void
u3a_v4_lose(u3_noun som)
{
  if ( !_(u3a_v4_is_cat(som)) ) {
    _me_v4_lose_north(som);
  }
}

/* u3a_v4_pile_prep(): initialize stack control.
*/
void
u3a_v4_pile_prep(u3a_v4_pile* pil_u, c3_w len_w)
{
  //  frame size, in words
  //
  c3_w wor_w = (len_w + 3) >> 2;
  c3_o nor_o = u3a_v4_is_north(u3R_v4);

  pil_u->mov_ws = (c3y == nor_o) ? -wor_w :  wor_w;
  pil_u->off_ws = (c3y == nor_o) ?      0 : -wor_w;
  pil_u->top_p  = u3R_v4->cap_p;

#ifdef U3_MEMORY_DEBUG
  pil_u->rod_u  = u3R_v4;
#endif
}

/* u3a_v4_ream(): ream free-lists.
*/
void
u3a_v4_ream(void)
{
  u3p(u3a_v4_fbox) lit_p;
  u3a_v4_fbox*     fox_u;
  c3_w     sel_w, i_w;

  for ( i_w = 0; i_w < u3a_v4_fbox_no; i_w++ ) {
    lit_p = u3R_v4->all.fre_p[i_w];

    while ( lit_p ) {
      fox_u = u3v4to(u3a_v4_fbox, lit_p);
      lit_p = fox_u->nex_p;
      sel_w = _box_v4_slot(fox_u->box_u.siz_w);

      if ( sel_w != i_w ) {
        //  inlined _box_detach()
        //
        {
          u3p(u3a_v4_fbox) fre_p = u3v4of(u3a_v4_fbox, &(fox_u->box_u));
          u3p(u3a_v4_fbox) pre_p = u3v4to(u3a_v4_fbox, fre_p)->pre_p;
          u3p(u3a_v4_fbox) nex_p = u3v4to(u3a_v4_fbox, fre_p)->nex_p;

          if ( nex_p ) {
            if ( u3v4to(u3a_v4_fbox, nex_p)->pre_p != fre_p ) {
              u3_assert(!"loom: corrupt");
            }
            u3v4to(u3a_v4_fbox, nex_p)->pre_p = pre_p;
          }
          if ( pre_p ) {
            if( u3v4to(u3a_v4_fbox, pre_p)->nex_p != fre_p ) {
              u3_assert(!"loom: corrupt");
            }
            u3v4to(u3a_v4_fbox, pre_p)->nex_p = nex_p;
          }
          else {
            if ( fre_p != u3R_v4->all.fre_p[i_w] ) {
              u3_assert(!"loom: corrupt");
            }
            u3R_v4->all.fre_p[i_w] = nex_p;
          }
        }

        //  inlined _box_attach()
        {
          u3p(u3a_v4_fbox)  fre_p = u3v4of(u3a_v4_fbox, &(fox_u->box_u));
          u3p(u3a_v4_fbox)* pfr_p = &u3R_v4->all.fre_p[sel_w];
          u3p(u3a_v4_fbox)  nex_p = *pfr_p;

          u3v4to(u3a_v4_fbox, fre_p)->pre_p = 0;
          u3v4to(u3a_v4_fbox, fre_p)->nex_p = nex_p;
          if ( nex_p ) {
            u3v4to(u3a_v4_fbox, nex_p)->pre_p = fre_p;
          }
          (*pfr_p) = fre_p;
        }
      }
    }
  }
}

/* u3a_v4_rewrite_ptr(): mark a pointer as already having been rewritten
*/
c3_o
u3a_v4_rewrite_ptr(void* ptr_v)
{
  u3a_v4_box* box_u = u3a_v4_botox(ptr_v);
  if ( box_u->use_w & 0x80000000 ) {
    /* Already rewritten.
    */
    return c3n;
  }
  box_u->use_w |= 0x80000000;
  return c3y;
}

u3_post
u3a_v4_rewritten(u3_post ptr_v)
{
  u3a_v4_box* box_u = u3a_v4_botox(u3a_v4_into(ptr_v));
  c3_w* box_w = (c3_w*) box_u;
  return (u3_post)box_w[box_u->siz_w - 1];
}

/* u3a_v4_loom_sane(): sanity checks the state of the loom for obvious corruption
 */
void
u3a_v4_loom_sane(void)
{
  /*
    Only checking validity of freelists for now. Other checks could be added,
    e.g. noun HAMT traversal, boxwise traversal of loom validating `siz_w`s,
    `use_w`s, no empty space, etc. If added, some of that may need to be guarded
    behind C3DBG flags. Freelist traversal is probably fine to always do though.
  */
  for (c3_w i_w = 0; i_w < u3a_v4_fbox_no; i_w++) {
    u3p(u3a_v4_fbox) this_p = u3R_v4->all.fre_p[i_w];
    u3a_v4_fbox     *this_u = u3v4to(u3a_v4_fbox, this_p);
    for (; this_p
           ; this_p = this_u->nex_p
           , this_u = u3v4to(u3a_v4_fbox, this_p)) {
      u3p(u3a_v4_fbox) pre_p = this_u->pre_p
        ,           nex_p = this_u->nex_p;
      u3a_v4_fbox *pre_u = u3v4to(u3a_v4_fbox, this_u->pre_p)
        ,      *nex_u = u3v4to(u3a_v4_fbox, this_u->nex_p);

      if (nex_p && nex_u->pre_p != this_p) u3_assert(!"loom: wack");
      if (pre_p && pre_u->nex_p != this_p) u3_assert(!"loom: wack");
      if (!pre_p                /* this must be the head of a freelist */
          && u3R_v4->all.fre_p[_box_v4_slot(this_u->box_u.siz_w)] != this_p)
        u3_assert(!"loom: wack");
    }
  }
}


  /***  events.c
  ***/
/* _ce_v4_len:       byte length of pages
** _ce_v4_len_words: word length of pages
** _ce_v4_page:      byte length of a single page
** _ce_v4_ptr:       void pointer to a page
*/
#define _ce_v4_len(i)        ((size_t)(i) << (u3a_v4_page + 2))
#define _ce_v4_len_words(i)  ((size_t)(i) << u3a_v4_page)
#define _ce_v4_page          _ce_v4_len(1)
#define _ce_v4_ptr(i)        ((void *)((c3_c*)u3_Loom + _ce_v4_len(i)))

/// Snapshotting system.
u3e_v4_pool u3e_v4_Pool;

static c3_l
_ce_v4_mug_page(void* ptr_v)
{
  //  XX trailing zeros
  // return u3r_mug_bytes(ptr_v, _ce_v4_page);
  return u3r_mug_words(ptr_v, _ce_v4_len_words(1));
}

#ifdef U3_SNAPSHOT_VALIDATION
/* Image check.
*/
struct {
  c3_w nor_w;
  c3_w sou_w;
  c3_w mug_w[u3a_v4_pages];
} u3K;

/* u3e_v4_check(): compute a checksum on all memory within the watermarks.
*/
void
u3e_v4_check(c3_c* cap_c)
{
  c3_w nor_w = 0;
  c3_w sou_w = 0;

  {
    u3_post low_p, hig_p;
    u3m_water(&low_p, &hig_p);

    nor_w = (low_p + (_ce_v4_len_words(1) - 1)) >> u3a_v4_page;
    sou_w = u3P_v4.pag_w - (hig_p >> u3a_v4_page);
  }

  /* compute checksum over active pages.
  */
  {
    c3_w i_w, sum_w, mug_w;

    sum_w = 0;
    for ( i_w = 0; i_w < nor_w; i_w++ ) {
      mug_w = _ce_v4_mug_page(_ce_v4_ptr(i_w));
      if ( strcmp(cap_c, "boot") ) {
        u3_assert(mug_w == u3K.mug_w[i_w]);
      }
      sum_w += mug_w;
    }
    for ( i_w = 0; i_w < sou_w; i_w++ ) {
      mug_w = _ce_v4_mug_page(_ce_v4_ptr((u3P_v4.pag_w - (i_w + 1))));
      if ( strcmp(cap_c, "boot") ) {
        u3_assert(mug_w == u3K.mug_w[(u3P_v4.pag_w - (i_w + 1))]);
      }
      sum_w += mug_w;
    }
    u3l_log("%s: sum %x (%x, %x)", cap_c, sum_w, nor_w, sou_w);
  }
}
#endif

/* _ce_v4_flaw_mmap(): remap non-guard page after fault.
*/
static inline c3_i
_ce_v4_flaw_mmap(c3_w pag_w)
{
  // NB: must be static, since the stack is grown via page faults, and
  // we're already in a page fault handler.
  //
  static c3_y con_y[_ce_v4_page];

  // save contents of page, to be restored after the mmap
  //
  memcpy(con_y, _ce_v4_ptr(pag_w), _ce_v4_page);

  // map the dirty page into the ephemeral file
  //
  if ( MAP_FAILED == mmap(_ce_v4_ptr(pag_w),
                          _ce_v4_page,
                          (PROT_READ | PROT_WRITE),
                          (MAP_FIXED | MAP_SHARED),
                          u3P_v4.eph_i, _ce_v4_len(pag_w)) )
  {
    fprintf(stderr, "loom: fault mmap failed (%u): %s\r\n",
                     pag_w, strerror(errno));
    return 1;
  }

  // restore contents of page
  //
  memcpy(_ce_v4_ptr(pag_w), con_y, _ce_v4_page);

  return 0;
}

/* _ce_v4_flaw_mprotect(): protect page after fault.
*/
static inline c3_i
_ce_v4_flaw_mprotect(c3_w pag_w)
{
  if ( 0 != mprotect(_ce_v4_ptr(pag_w), _ce_v4_page, (PROT_READ | PROT_WRITE)) ) {
    fprintf(stderr, "loom: fault mprotect (%u): %s\r\n",
                     pag_w, strerror(errno));
    return 1;
  }

  return 0;
}

#ifdef U3_GUARD_PAGE
/* _ce_v4_ward_protect(): protect the guard page.
*/
static inline c3_i
_ce_v4_ward_protect(void)
{
  if ( 0 != mprotect(_ce_v4_ptr(u3P_v4.gar_w), _ce_v4_page, PROT_NONE) ) {
    fprintf(stderr, "loom: failed to protect guard page (%u): %s\r\n",
                    u3P_v4.gar_w, strerror(errno));
    return 1;
  }

  return 0;
}

/* _ce_v4_ward_post(): set the guard page.
*/
static inline c3_i
_ce_v4_ward_post(c3_w nop_w, c3_w sop_w)
{
  u3P_v4.gar_w = nop_w + ((sop_w - nop_w) / 2);
  return _ce_v4_ward_protect();
}

/* _ce_v4_ward_clip(): hit the guard page.
*/
static inline u3e_v4_flaw
_ce_v4_ward_clip(c3_w nop_w, c3_w sop_w)
{
  c3_w old_w = u3P_v4.gar_w;

  if ( !u3P_v4.gar_w || ((nop_w < u3P_v4.gar_w) && (sop_w > u3P_v4.gar_w)) ) {
    fprintf(stderr, "loom: ward bogus (>%u %u %u<)\r\n",
                    nop_w, u3P_v4.gar_w, sop_w);
    return u3e_v4_flaw_sham;
  }

  if ( sop_w <= (nop_w + 1) ) {
    return u3e_v4_flaw_meme;
  }

  if ( _ce_v4_ward_post(nop_w, sop_w) ) {
    return u3e_v4_flaw_base;
  }

  u3_assert( old_w != u3P_v4.gar_w );

  return u3e_v4_flaw_good;
}
#endif /* ifdef U3_GUARD_PAGE */

/* u3e_v4_fault(): handle a memory fault.
*/
u3e_v4_flaw
u3e_v4_fault(u3_post low_p, u3_post hig_p, u3_post off_p)
{
  c3_w pag_w = off_p >> u3a_v4_page;
  c3_w blk_w = pag_w >> 5;
  c3_w bit_w = pag_w & 31;

#ifdef U3_GUARD_PAGE
  c3_w gar_w = u3P_v4.gar_w;

  if ( pag_w == gar_w ) {
    u3e_v4_flaw fal_e = _ce_v4_ward_clip(low_p >> u3a_v4_page, hig_p >> u3a_v4_page);

    if ( u3e_v4_flaw_good != fal_e ) {
      return fal_e;
    }

    if ( !(u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w)) ) {
      fprintf(stderr, "loom: strange guard (%d)\r\n", pag_w);
      return u3e_v4_flaw_sham;
    }

    if ( _ce_v4_flaw_mprotect(pag_w) ) {
      return u3e_v4_flaw_base;
    }

    return u3e_v4_flaw_good;
  }
#endif

  if ( u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
    fprintf(stderr, "loom: strange page (%d): %x\r\n", pag_w, off_p);
    return u3e_v4_flaw_sham;
  }

  u3P_v4.dit_w[blk_w] |= ((c3_w)1 << bit_w);

  if ( u3P_v4.eph_i ) {
    if ( _ce_v4_flaw_mmap(pag_w) ) {
      return u3e_v4_flaw_base;
    }
  }
  else if ( _ce_v4_flaw_mprotect(pag_w) ) {
    return u3e_v4_flaw_base;
  }

  return u3e_v4_flaw_good;
}

typedef enum {
  _ce_v4_img_good = 0,
  _ce_v4_img_fail = 1,
  _ce_v4_img_size = 2
} _ce_v4_img_stat;

/* _ce_v4_image_stat(): measure image.
*/
static _ce_v4_img_stat
_ce_v4_image_stat(u3e_v4_image* img_u, c3_w* pgs_w)
{
  struct stat buf_u;

  if ( -1 == fstat(img_u->fid_i, &buf_u) ) {
    fprintf(stderr, "loom: stat %s: %s\r\n", img_u->nam_c, strerror(errno));
    u3_assert(0);
    return _ce_v4_img_fail;
  }
  else {
    c3_z siz_z = buf_u.st_size;
    c3_z pgs_z = (siz_z + (_ce_v4_page - 1)) >> (u3a_v4_page + 2);

    if ( !siz_z ) {
      *pgs_w = 0;
      return _ce_v4_img_good;
    }
    else if ( siz_z != _ce_v4_len(pgs_z) ) {
      fprintf(stderr, "loom: %s corrupt size %zu\r\n", img_u->nam_c, siz_z);
      return _ce_v4_img_size;
    }
    else if ( pgs_z > UINT32_MAX ) {
      fprintf(stderr, "loom: %s overflow %zu\r\n", img_u->nam_c, siz_z);
      return _ce_v4_img_fail;
    }
    else {
      *pgs_w = (c3_w)pgs_z;
      return _ce_v4_img_good;
    }
  }
}

/* _ce_v4_ephemeral_open(): open or create ephemeral file
*/
static c3_o
_ce_v4_ephemeral_open(c3_i* eph_i)
{
  c3_i mod_i = O_RDWR | O_CREAT;
  c3_c ful_c[8193];

  if ( u3C.eph_c == 0 ) {
    snprintf(ful_c, 8192, "%s", u3P_v4.dir_c);
    c3_mkdir(ful_c, 0700);

    snprintf(ful_c, 8192, "%s/.urb", u3P_v4.dir_c);
    c3_mkdir(ful_c, 0700);

    snprintf(ful_c, 8192, "%s/.urb/chk", u3P_v4.dir_c);
    c3_mkdir(ful_c, 0700);

    snprintf(ful_c, 8192, "%s/.urb/chk/limbo.bin", u3P_v4.dir_c);
    u3C.eph_c = strdup(ful_c);
  }

  if ( -1 == (*eph_i = c3_open(u3C.eph_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: ephemeral c3_open %s: %s\r\n", u3C.eph_c,
            strerror(errno));
    return c3n;
  }

  if ( ftruncate(*eph_i, _ce_v4_len(u3P_v4.pag_w)) < 0 ) {
    fprintf(stderr, "loom: ephemeral ftruncate %s: %s\r\n", u3C.eph_c,
            strerror(errno));
    return c3n;
  }
  return c3y;
}

/* _ce_v4_image_open(): open or create image.
*/
static _ce_v4_img_stat
_ce_v4_image_open(u3e_v4_image* img_u, c3_c* ful_c)
{
  c3_i mod_i = O_RDWR | O_CREAT;

  c3_c pax_c[8192];
  snprintf(pax_c, 8192, "%s/%s.bin", ful_c, img_u->nam_c);
  if ( -1 == (img_u->fid_i = c3_open(pax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", pax_c, strerror(errno));
    return _ce_v4_img_fail;
  }

  return _ce_v4_image_stat(img_u, &img_u->pgs_w);
}

/* _ce_v4_patch_write_control(): write control block file.
*/
static void
_ce_v4_patch_write_control(u3_ce_v4_patch* pat_u)
{
  ssize_t ret_i;
  c3_w    len_w = sizeof(u3e_v4_control) +
                  (pat_u->con_u->pgs_w * sizeof(u3e_v4_line));

  if ( len_w != (ret_i = write(pat_u->ctl_i, pat_u->con_u, len_w)) ) {
    if ( 0 < ret_i ) {
      fprintf(stderr, "loom: patch ctl partial write: %zu\r\n", (size_t)ret_i);
    }
    else {
      fprintf(stderr, "loom: patch ctl write: %s\r\n", strerror(errno));
    }
    u3_assert(0);
  }
}

/* _ce_v4_patch_read_control(): read control block file.
*/
static c3_o
_ce_v4_patch_read_control(u3_ce_v4_patch* pat_u)
{
  c3_w len_w;

  u3_assert(0 == pat_u->con_u);
  {
    struct stat buf_u;

    if ( -1 == fstat(pat_u->ctl_i, &buf_u) ) {
      u3_assert(0);
      return c3n;
    }
    len_w = (c3_w) buf_u.st_size;
  }

  if (0 == len_w) {
    return c3n;
  }
  
  pat_u->con_u = c3_malloc(len_w);
  if ( (len_w != read(pat_u->ctl_i, pat_u->con_u, len_w)) ||
        (len_w != sizeof(u3e_v4_control) +
                  (pat_u->con_u->pgs_w * sizeof(u3e_v4_line))) )
  {
    c3_free(pat_u->con_u);
    pat_u->con_u = 0;
    return c3n;
  }
  return c3y;
}

/* _ce_v4_patch_create(): create patch files.
*/
static void
_ce_v4_patch_create(u3_ce_v4_patch* pat_u)
{
  c3_c ful_c[8193];

  snprintf(ful_c, 8192, "%s", u3P_v4.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb", u3P_v4.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb/chk/control.bin", u3P_v4.dir_c);
  if ( -1 == (pat_u->ctl_i = c3_open(ful_c, O_RDWR | O_CREAT | O_EXCL, 0600)) ) {
    fprintf(stderr, "loom: patch c3_open control.bin: %s\r\n", strerror(errno));
    u3_assert(0);
  }

  snprintf(ful_c, 8192, "%s/.urb/chk/memory.bin", u3P_v4.dir_c);
  if ( -1 == (pat_u->mem_i = c3_open(ful_c, O_RDWR | O_CREAT | O_EXCL, 0600)) ) {
    fprintf(stderr, "loom: patch c3_open memory.bin: %s\r\n", strerror(errno));
    u3_assert(0);
  }
}

/* _ce_v4_patch_delete(): delete a patch.
*/
static void
_ce_v4_patch_delete(void)
{
  c3_c ful_c[8193];

  snprintf(ful_c, 8192, "%s/.urb/chk/control.bin", u3P_v4.dir_c);
  if ( unlink(ful_c) ) {
    fprintf(stderr, "loom: failed to delete control.bin: %s\r\n",
                    strerror(errno));
  }

  snprintf(ful_c, 8192, "%s/.urb/chk/memory.bin", u3P_v4.dir_c);
  if ( unlink(ful_c) ) {
    fprintf(stderr, "loom: failed to remove memory.bin: %s\r\n",
                    strerror(errno));
  }
}

/* _ce_v4_patch_verify(): check patch data mug.
*/
static c3_o
_ce_v4_patch_verify(u3_ce_v4_patch* pat_u)
{
  c3_w  pag_w, mug_w;
  c3_y  buf_y[_ce_v4_page];
  c3_zs ret_zs;
  c3_o  sou_o = c3n;  // south seen

  if ( U3P_VERLAT != pat_u->con_u->ver_w ) {
    fprintf(stderr, "loom: patch version mismatch: have %"PRIc3_w", need %u\r\n",
                    pat_u->con_u->ver_w,
                    U3P_VERLAT);
    return c3n;
  }

  if ( pat_u->con_u->sou_w > 1 ) {
    fprintf(stderr, "loom: patch strange south size: %u\r\n",
                    pat_u->con_u->sou_w);
    return c3n;
  }

  for ( c3_z i_z = 0; i_z < pat_u->con_u->pgs_w; i_z++ ) {
    pag_w = pat_u->con_u->mem_u[i_z].pag_w;
    mug_w = pat_u->con_u->mem_u[i_z].mug_w;

    if ( _ce_v4_page !=
         (ret_zs = pread(pat_u->mem_i, buf_y, _ce_v4_page, _ce_v4_len(i_z))) )
    {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: patch partial read: %"PRIc3_zs"\r\n", ret_zs);
      }
      else {
        fprintf(stderr, "loom: patch read: fail %s\r\n", strerror(errno));
      }
      return c3n;
    }

    {
      c3_w nug_w = _ce_v4_mug_page(buf_y);

      if ( mug_w != nug_w ) {
        fprintf(stderr, "loom: patch mug mismatch"
                        " %"PRIc3_w"/%"PRIc3_z"; (%"PRIxc3_w", %"PRIxc3_w")\r\n",
                        pag_w, i_z, mug_w, nug_w);
        return c3n;
      }
#if 0
      else {
        u3l_log("verify: patch %"PRIc3_w"/%"PRIc3_z", %"PRIxc3_w"\r\n", pag_w, i_z, mug_w);
      }
#endif
    }

    if ( pag_w >= pat_u->con_u->nor_w ) {
      if ( c3n == sou_o ) {
        sou_o = c3y;
      }
      else {
        fprintf(stderr, "loom: patch multiple south pages\r\n");
        return c3n;
      }
    }
  }
  return c3y;
}

/* _ce_v4_patch_free(): free a patch.
*/
static void
_ce_v4_patch_free(u3_ce_v4_patch* pat_u)
{
  c3_free(pat_u->con_u);
  close(pat_u->ctl_i);
  close(pat_u->mem_i);
  c3_free(pat_u);
}

/* _ce_v4_patch_open(): open patch, if any.
*/
static u3_ce_v4_patch*
_ce_v4_patch_open(void)
{
  u3_ce_v4_patch* pat_u;
  c3_c ful_c[8193];
  c3_i ctl_i, mem_i;

  snprintf(ful_c, 8192, "%s", u3P_v4.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb", u3P_v4.dir_c);
  c3_mkdir(ful_c, 0700);

  snprintf(ful_c, 8192, "%s/.urb/chk/control.bin", u3P_v4.dir_c);
  if ( -1 == (ctl_i = c3_open(ful_c, O_RDWR)) ) {
    return 0;
  }

  snprintf(ful_c, 8192, "%s/.urb/chk/memory.bin", u3P_v4.dir_c);
  if ( -1 == (mem_i = c3_open(ful_c, O_RDWR)) ) {
    close(ctl_i);

    _ce_v4_patch_delete();
    return 0;
  }
  pat_u = c3_malloc(sizeof(u3_ce_v4_patch));
  pat_u->ctl_i = ctl_i;
  pat_u->mem_i = mem_i;
  pat_u->con_u = 0;

  if ( c3n == _ce_v4_patch_read_control(pat_u) ) {
    close(pat_u->ctl_i);
    close(pat_u->mem_i);
    c3_free(pat_u);

    _ce_v4_patch_delete();
    return 0;
  }
  if ( c3n == _ce_v4_patch_verify(pat_u) ) {
    _ce_v4_patch_free(pat_u);
    _ce_v4_patch_delete();
    return 0;
  }
  return pat_u;
}

/* _ce_v4_patch_write_page(): write a page of patch memory.
*/
static void
_ce_v4_patch_write_page(u3_ce_v4_patch* pat_u,
                     c3_w         pgc_w,
                     c3_w*        mem_w)
{
  c3_zs ret_zs;

  if ( _ce_v4_page !=
       (ret_zs = pwrite(pat_u->mem_i, mem_w, _ce_v4_page, _ce_v4_len(pgc_w))) )
  {
    if ( 0 < ret_zs ) {
      fprintf(stderr, "loom: patch partial write: %"PRIc3_zs"\r\n", ret_zs);
    }
    else {
      fprintf(stderr, "loom: patch write: fail: %s\r\n", strerror(errno));
    }
    fprintf(stderr, "info: you probably have insufficient disk space");
    u3_assert(0);
  }
}

/* _ce_v4_patch_count_page(): count a page, producing new counter.
*/
static c3_w
_ce_v4_patch_count_page(c3_w pag_w,
                     c3_w pgc_w)
{
  c3_w blk_w = (pag_w >> 5);
  c3_w bit_w = (pag_w & 31);

  if ( u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
    pgc_w += 1;
  }
  return pgc_w;
}

/* _ce_v4_patch_save_page(): save a page, producing new page counter.
*/
static c3_w
_ce_v4_patch_save_page(u3_ce_v4_patch* pat_u,
                    c3_w         pag_w,
                    c3_w         pgc_w)
{
  c3_w  blk_w = (pag_w >> 5);
  c3_w  bit_w = (pag_w & 31);

  if ( u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
    c3_w* mem_w = _ce_v4_ptr(pag_w);

    pat_u->con_u->mem_u[pgc_w].pag_w = pag_w;
    pat_u->con_u->mem_u[pgc_w].mug_w = _ce_v4_mug_page(mem_w);

#if 0
    fprintf(stderr, "loom: save page %d %x\r\n",
                    pag_w, pat_u->con_u->mem_u[pgc_w].mug_w);
#endif
    _ce_v4_patch_write_page(pat_u, pgc_w, mem_w);

    pgc_w += 1;
  }
  return pgc_w;
}

/* _ce_v4_patch_compose(): make and write current patch.
*/
static u3_ce_v4_patch*
_ce_v4_patch_compose(c3_w nor_w, c3_w sou_w)
{
  c3_w pgs_w = 0;

#ifdef U3_SNAPSHOT_VALIDATION
  u3K.nor_w = nor_w;
  u3K.sou_w = sou_w;
#endif

  /* Count dirty pages.
  */
  {
    c3_w i_w;

    for ( i_w = 0; i_w < nor_w; i_w++ ) {
      pgs_w = _ce_v4_patch_count_page(i_w, pgs_w);
    }
    for ( i_w = 0; i_w < sou_w; i_w++ ) {
      pgs_w = _ce_v4_patch_count_page((u3P_v4.pag_w - (i_w + 1)), pgs_w);
    }
  }

  if ( !pgs_w ) {
    return 0;
  }
  else {
    u3_ce_v4_patch* pat_u = c3_malloc(sizeof(u3_ce_v4_patch));
    c3_w i_w, pgc_w;

    _ce_v4_patch_create(pat_u);
    pat_u->con_u = c3_malloc(sizeof(u3e_v4_control) + (pgs_w * sizeof(u3e_v4_line)));
    pat_u->con_u->ver_w = U3P_VERLAT;
    pgc_w = 0;

    for ( i_w = 0; i_w < nor_w; i_w++ ) {
      pgc_w = _ce_v4_patch_save_page(pat_u, i_w, pgc_w);
    }
    for ( i_w = 0; i_w < sou_w; i_w++ ) {
      pgc_w = _ce_v4_patch_save_page(pat_u, (u3P_v4.pag_w - (i_w + 1)), pgc_w);
    }

    u3_assert( pgc_w == pgs_w );

    pat_u->con_u->nor_w = nor_w;
    pat_u->con_u->sou_w = sou_w;
    pat_u->con_u->pgs_w = pgc_w;

    _ce_v4_patch_write_control(pat_u);
    return pat_u;
  }
}

/* _ce_v4_patch_sync(): make sure patch is synced to disk.
*/
static void
_ce_v4_patch_sync(u3_ce_v4_patch* pat_u)
{
  if ( -1 == c3_sync(pat_u->ctl_i) ) {
    fprintf(stderr, "loom: control file sync failed: %s\r\n",
                    strerror(errno));
    u3_assert(!"loom: control sync");
  }

  if ( -1 == c3_sync(pat_u->mem_i) ) {
    fprintf(stderr, "loom: patch file sync failed: %s\r\n",
                    strerror(errno));
    u3_assert(!"loom: patch sync");
  }
}

/* _ce_v4_image_sync(): make sure image is synced to disk.
*/
static c3_o
_ce_v4_image_sync(u3e_v4_image* img_u)
{
  if ( -1 == c3_sync(img_u->fid_i) ) {
    fprintf(stderr, "loom: image (%s) sync failed: %s\r\n",
                    img_u->nam_c, strerror(errno));
    return c3n;
  }

  return c3y;
}

/* _ce_v4_image_resize(): resize image, truncating if it shrunk.
*/
static void
_ce_v4_image_resize(u3e_v4_image* img_u, c3_w pgs_w)
{
  c3_z  off_z = _ce_v4_len(pgs_w);
  off_t off_i = (off_t)off_z;

  if ( img_u->pgs_w > pgs_w ) {
    if ( off_z != (size_t)off_i ) {
      fprintf(stderr, "loom: image (%s) truncate: "
                      "offset overflow (%" PRId64 ") for page %u\r\n",
                      img_u->nam_c, (c3_ds)off_i, pgs_w);
      u3_assert(0);
    }

    if ( ftruncate(img_u->fid_i, off_i) ) {
      fprintf(stderr, "loom: image (%s) truncate: %s\r\n",
                      img_u->nam_c, strerror(errno));
      u3_assert(0);
    }
  }

  img_u->pgs_w = pgs_w;
}

/* _ce_v4_patch_apply(): apply patch to images.
*/
static void
_ce_v4_patch_apply(u3_ce_v4_patch* pat_u)
{
  c3_zs ret_zs;
  c3_w     i_w;

  //  resize images
  //
  _ce_v4_image_resize(&u3P_v4.nor_u, pat_u->con_u->nor_w);
  _ce_v4_image_resize(&u3P_v4.sou_u, pat_u->con_u->sou_w);

  //  seek to begining of patch
  //
  if ( -1 == lseek(pat_u->mem_i, 0, SEEK_SET) ) {
    fprintf(stderr, "loom: patch apply seek: %s\r\n", strerror(errno));
    u3_assert(0);
  }

  //  write patch pages into the appropriate image
  //
  for ( i_w = 0; i_w < pat_u->con_u->pgs_w; i_w++ ) {
    c3_w pag_w = pat_u->con_u->mem_u[i_w].pag_w;
    c3_y buf_y[_ce_v4_page];
    c3_i fid_i;
    c3_z off_z;

    if ( pag_w < pat_u->con_u->nor_w ) {
      fid_i = u3P_v4.nor_u.fid_i;
      off_z = _ce_v4_len(pag_w);
    }
    //  NB: this assumes that there never more than one south page,
    //  as enforced by _ce_v4_patch_verify()
    //
    else {
      fid_i = u3P_v4.sou_u.fid_i;
      off_z = 0;
    }

    if ( _ce_v4_page != (ret_zs = read(pat_u->mem_i, buf_y, _ce_v4_page)) ) {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: patch apply partial read: %"PRIc3_zs"\r\n",
                        ret_zs);
      }
      else {
        fprintf(stderr, "loom: patch apply read: %s\r\n", strerror(errno));
      }
      u3_assert(0);
    }
    else {
      if ( _ce_v4_page !=
           (ret_zs = pwrite(fid_i, buf_y, _ce_v4_page, off_z)) )
      {
        if ( 0 < ret_zs ) {
          fprintf(stderr, "loom: patch apply partial write: %"PRIc3_zs"\r\n",
                          ret_zs);
        }
        else {
          fprintf(stderr, "loom: patch apply write: %s\r\n", strerror(errno));
        }
        fprintf(stderr, "info: you probably have insufficient disk space");
        u3_assert(0);
      }
    }
#if 0
    u3l_log("apply: %d, %x", pag_w, _ce_v4_mug_page(buf_y));
#endif
  }
}

/* _ce_v4_loom_track_sane(): quiescent page state invariants.
*/
static c3_o
_ce_v4_loom_track_sane(void)
{
  c3_w blk_w, bit_w, max_w, i_w = 0;
  c3_o san_o = c3y;

  max_w = u3P_v4.nor_u.pgs_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;

    if ( u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
      fprintf(stderr, "loom: insane north %u\r\n", i_w);
      san_o = c3n;
    }
  }

  max_w = u3P_v4.pag_w - u3P_v4.sou_u.pgs_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;

    if ( !(u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w)) ) {
      fprintf(stderr, "loom: insane open %u\r\n", i_w);
      san_o = c3n;
    }
  }

  max_w = u3P_v4.pag_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;

    if ( u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w) ) {
      fprintf(stderr, "loom: insane south %u\r\n", i_w);
      san_o = c3n;
    }
  }

  return san_o;
}

/* _ce_v4_loom_track_north(): [pgs_w] clean, followed by [dif_w] dirty.
*/
void
_ce_v4_loom_track_north(c3_w pgs_w, c3_w dif_w)
{
  c3_w blk_w, bit_w, i_w = 0, max_w = pgs_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;
    u3P_v4.dit_w[blk_w] &= ~((c3_w)1 << bit_w);
  }

  max_w += dif_w;

  for ( ; i_w < max_w; i_w++ ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;
    u3P_v4.dit_w[blk_w] |= ((c3_w)1 << bit_w);
  }
}

/* _ce_v4_loom_track_south(): [pgs_w] clean, preceded by [dif_w] dirty.
*/
void
_ce_v4_loom_track_south(c3_w pgs_w, c3_w dif_w)
{
  c3_w blk_w, bit_w, i_w = u3P_v4.pag_w - 1, max_w = u3P_v4.pag_w - pgs_w;

  for ( ; i_w >= max_w; i_w-- ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;
    u3P_v4.dit_w[blk_w] &= ~((c3_w)1 << bit_w);
  }

  max_w -= dif_w;

  for ( ; i_w >= max_w; i_w-- ) {
    blk_w = i_w >> 5;
    bit_w = i_w & 31;
    u3P_v4.dit_w[blk_w] |= ((c3_w)1 << bit_w);
  }
}

/* _ce_v4_loom_protect_north(): protect/track pages from the bottom of memory.
*/
static void
_ce_v4_loom_protect_north(c3_w pgs_w, c3_w old_w)
{
  c3_w dif_w = 0;

  if ( pgs_w ) {
    if ( 0 != mprotect(_ce_v4_ptr(0), _ce_v4_len(pgs_w), PROT_READ) ) {
      fprintf(stderr, "loom: pure north (%u pages): %s\r\n",
                      pgs_w, strerror(errno));
      u3_assert(0);
    }
  }

  if ( old_w > pgs_w ) {
    dif_w = old_w - pgs_w;

    if ( 0 != mprotect(_ce_v4_ptr(pgs_w),
                       _ce_v4_len(dif_w),
                       (PROT_READ | PROT_WRITE)) )
    {
      fprintf(stderr, "loom: foul north (%u pages, %u old): %s\r\n",
                      pgs_w, old_w, strerror(errno));
      u3_assert(0);
    }

#ifdef U3_GUARD_PAGE
    //  protect guard page if clobbered
    //
    //    NB: < pgs_w is precluded by assertion in u3e_v4_save()
    //
    if ( u3P_v4.gar_w < old_w ) {
      fprintf(stderr, "loom: guard on reprotect\r\n");
      u3_assert( !_ce_v4_ward_protect() );
    }
#endif
  }

  _ce_v4_loom_track_north(pgs_w, dif_w);
}

/* _ce_v4_loom_protect_south(): protect/track pages from the top of memory.
*/
static void
_ce_v4_loom_protect_south(c3_w pgs_w, c3_w old_w)
{
  c3_w dif_w = 0;

  if ( pgs_w ) {
    if ( 0 != mprotect(_ce_v4_ptr(u3P_v4.pag_w - pgs_w),
                       _ce_v4_len(pgs_w),
                       PROT_READ) )
    {
      fprintf(stderr, "loom: pure south (%u pages): %s\r\n",
                      pgs_w, strerror(errno));
      u3_assert(0);
    }
  }

  if ( old_w > pgs_w ) {
    c3_w off_w = u3P_v4.pag_w - old_w;
    dif_w = old_w - pgs_w;

    if ( 0 != mprotect(_ce_v4_ptr(off_w),
                       _ce_v4_len(dif_w),
                       (PROT_READ | PROT_WRITE)) )
    {
      fprintf(stderr, "loom: foul south (%u pages, %u old): %s\r\n",
                      pgs_w, old_w, strerror(errno));
      u3_assert(0);
    }

#ifdef U3_GUARD_PAGE
    //  protect guard page if clobbered
    //
    //    NB: >= pgs_w is precluded by assertion in u3e_v4_save()
    //
    if ( u3P_v4.gar_w >= off_w ) {
      fprintf(stderr, "loom: guard on reprotect\r\n");
      u3_assert( !_ce_v4_ward_protect() );
    }
#endif
  }

  _ce_v4_loom_track_south(pgs_w, dif_w);
}

/* _ce_v4_loom_mapf_ephemeral(): map entire loom into ephemeral file
*/
static void
_ce_v4_loom_mapf_ephemeral(void)
{
  if ( MAP_FAILED == mmap(_ce_v4_ptr(0),
                          _ce_v4_len(u3P_v4.pag_w),
                          (PROT_READ | PROT_WRITE),
                          (MAP_FIXED | MAP_SHARED),
                          u3P_v4.eph_i, 0) )
  {
    fprintf(stderr, "loom: initial ephemeral mmap failed (%u pages): %s\r\n",
                    u3P_v4.pag_w, strerror(errno));
    u3_assert(0);
  }
}

/* _ce_v4_loom_mapf_north(): map [pgs_w] of [fid_i] into the bottom of memory
**                        (and ephemeralize [old_w - pgs_w] after if needed).
**
**   NB: _ce_v4_loom_mapf_south() is possible, but it would make separate mappings
**       for each page since the south segment is reversed on disk.
**       in practice, the south segment is a single page (and always dirty);
**       a file-backed mapping for it is just not worthwhile.
*/
static void
_ce_v4_loom_mapf_north(c3_i fid_i, c3_w pgs_w, c3_w old_w)
{
  c3_w dif_w = 0;

  if ( pgs_w ) {
    if ( MAP_FAILED == mmap(_ce_v4_ptr(0),
                            _ce_v4_len(pgs_w),
                            PROT_READ,
                            (MAP_FIXED | MAP_PRIVATE),
                            fid_i, 0) )
    {
      fprintf(stderr, "loom: file-backed mmap failed (%u pages): %s\r\n",
                      pgs_w, strerror(errno));
      u3_assert(0);
    }
  }

  if ( old_w > pgs_w ) {
    dif_w = old_w - pgs_w;

    if ( u3C.wag_w & u3o_swap ) {
      if ( MAP_FAILED == mmap(_ce_v4_ptr(pgs_w),
                              _ce_v4_len(dif_w),
                              (PROT_READ | PROT_WRITE),
                              (MAP_FIXED | MAP_SHARED),
                              u3P_v4.eph_i, _ce_v4_len(pgs_w)) )
      {
        fprintf(stderr, "loom: ephemeral mmap failed (%u pages, %u old): %s\r\n",
                        pgs_w, old_w, strerror(errno));
        u3_assert(0);
      }
    }
    else {
      if ( MAP_FAILED == mmap(_ce_v4_ptr(pgs_w),
                              _ce_v4_len(dif_w),
                              (PROT_READ | PROT_WRITE),
                              (MAP_ANON | MAP_FIXED | MAP_PRIVATE),
                              -1, 0) )
      {
        fprintf(stderr, "loom: anonymous mmap failed (%u pages, %u old): %s\r\n",
                        pgs_w, old_w, strerror(errno));
        u3_assert(0);
      }
    }

#ifdef U3_GUARD_PAGE
    //  protect guard page if clobbered
    //
    //    NB: < pgs_w is precluded by assertion in u3e_v4_save()
    //
    if ( u3P_v4.gar_w < old_w ) {
      fprintf(stderr, "loom: guard on remap\r\n");
      u3_assert( !_ce_v4_ward_protect() );
    }
#endif
  }

  _ce_v4_loom_track_north(pgs_w, dif_w);
}

/* _ce_v4_loom_blit_north(): apply pages, in order, from the bottom of memory.
*/
static void
_ce_v4_loom_blit_north(c3_i fid_i, c3_w pgs_w)
{
  c3_w    i_w;
  void* ptr_v;
  c3_zs ret_zs;

  for ( i_w = 0; i_w < pgs_w; i_w++ ) {
    ptr_v = _ce_v4_ptr(i_w);

    if ( _ce_v4_page != (ret_zs = pread(fid_i, ptr_v, _ce_v4_page, _ce_v4_len(i_w))) ) {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: blit north partial read: %"PRIc3_zs"\r\n",
                        ret_zs);
      }
      else {
        fprintf(stderr, "loom: blit north read %s\r\n", strerror(errno));
      }
      u3_assert(0);
    }
  }

  _ce_v4_loom_protect_north(pgs_w, 0);
}

/* _ce_v4_loom_blit_south(): apply pages, reversed, from the top of memory.
*/
static void
_ce_v4_loom_blit_south(c3_i fid_i, c3_w pgs_w)
{
  c3_w    i_w;
  void* ptr_v;
  c3_zs ret_zs;

  for ( i_w = 0; i_w < pgs_w; i_w++ ) {
    ptr_v = _ce_v4_ptr(u3P_v4.pag_w - (i_w + 1));

    if ( _ce_v4_page != (ret_zs = pread(fid_i, ptr_v, _ce_v4_page, _ce_v4_len(i_w))) ) {
      if ( 0 < ret_zs ) {
        fprintf(stderr, "loom: blit south partial read: %"PRIc3_zs"\r\n",
                        ret_zs);
      }
      else {
        fprintf(stderr, "loom: blit south read: %s\r\n", strerror(errno));
      }
      u3_assert(0);
    }
  }

  _ce_v4_loom_protect_south(pgs_w, 0);
}

#ifdef U3_SNAPSHOT_VALIDATION
/* _ce_v4_page_fine(): compare page in memory and on disk.
*/
static c3_o
_ce_v4_page_fine(u3e_v4_image* img_u, c3_w pag_w, c3_z off_z)
{
  ssize_t ret_i;
  c3_y    buf_y[_ce_v4_page];

  if ( _ce_v4_page !=
       (ret_i = pread(img_u->fid_i, buf_y, _ce_v4_page, off_z)) )
  {
    if ( 0 < ret_i ) {
      fprintf(stderr, "loom: image (%s) fine partial read: %zu\r\n",
                      img_u->nam_c, (size_t)ret_i);
    }
    else {
      fprintf(stderr, "loom: image (%s) fine read: %s\r\n",
                      img_u->nam_c, strerror(errno));
    }
    u3_assert(0);
  }

  {
    c3_w mug_w = _ce_v4_mug_page(_ce_v4_ptr(pag_w));
    c3_w fug_w = _ce_v4_mug_page(buf_y);

    if ( mug_w != fug_w ) {
      fprintf(stderr, "loom: image (%s) mismatch: "
                      "page %d, mem_w %x, fil_w %x, K %x\r\n",
                      img_u->nam_c, pag_w, mug_w, fug_w, u3K.mug_w[pag_w]);
      return c3n;
    }
  }

  return c3y;
}

/* _ce_v4_loom_fine(): compare clean pages in memory and on disk.
*/
static c3_o
_ce_v4_loom_fine(void)
{
  c3_w blk_w, bit_w, pag_w, i_w;
  c3_o fin_o = c3y;

  for ( i_w = 0; i_w < u3P_v4.nor_u.pgs_w; i_w++ ) {
    pag_w = i_w;
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;

    if ( !(u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w)) ) {
      fin_o = c3a(fin_o, _ce_v4_page_fine(&u3P_v4.nor_u, pag_w, _ce_v4_len(pag_w)));
    }
  }

  for ( i_w = 0; i_w < u3P_v4.sou_u.pgs_w; i_w++ ) {
    pag_w = u3P_v4.pag_w - (i_w + 1);
    blk_w = pag_w >> 5;
    bit_w = pag_w & 31;

    if ( !(u3P_v4.dit_w[blk_w] & ((c3_w)1 << bit_w)) ) {
      fin_o = c3a(fin_o, _ce_v4_page_fine(&u3P_v4.sou_u, pag_w, _ce_v4_len(i_w)));
    }
  }

  return fin_o;
}
#endif

/* _ce_v4_image_copy(): copy all of [fom_u] to [tou_u]
*/
static c3_o
_ce_v4_image_copy(u3e_v4_image* fom_u, u3e_v4_image* tou_u)
{
  ssize_t ret_i;
  c3_w      i_w;

  //  resize images
  //
  _ce_v4_image_resize(tou_u, fom_u->pgs_w);

  //  seek to begining of patch and images
  //
  if (  (-1 == lseek(fom_u->fid_i, 0, SEEK_SET))
     || (-1 == lseek(tou_u->fid_i, 0, SEEK_SET)) )
  {
    fprintf(stderr, "loom: image (%s) copy seek: %s\r\n",
                    fom_u->nam_c,
                    strerror(errno));
    return c3n;
  }

  //  copy pages into destination image
  //
  for ( i_w = 0; i_w < fom_u->pgs_w; i_w++ ) {
    c3_y buf_y[_ce_v4_page];
    c3_w off_w = i_w;

    if ( _ce_v4_page != (ret_i = read(fom_u->fid_i, buf_y, _ce_v4_page)) ) {
      if ( 0 < ret_i ) {
        fprintf(stderr, "loom: image (%s) copy partial read: %zu\r\n",
                        fom_u->nam_c, (size_t)ret_i);
      }
      else {
        fprintf(stderr, "loom: image (%s) copy read: %s\r\n",
                        fom_u->nam_c, strerror(errno));
      }
      return c3n;
    }
    else {
      if ( -1 == lseek(tou_u->fid_i, _ce_v4_len(off_w), SEEK_SET) ) {
        fprintf(stderr, "loom: image (%s) copy seek: %s\r\n",
                        tou_u->nam_c, strerror(errno));
        return c3n;
      }
      if ( _ce_v4_page != (ret_i = write(tou_u->fid_i, buf_y, _ce_v4_page)) ) {
        if ( 0 < ret_i ) {
          fprintf(stderr, "loom: image (%s) copy partial write: %zu\r\n",
                          tou_u->nam_c, (size_t)ret_i);
        }
        else {
          fprintf(stderr, "loom: image (%s) copy write: %s\r\n",
                          tou_u->nam_c, strerror(errno));
        }
        fprintf(stderr, "info: you probably have insufficient disk space");
        return c3n;
      }
    }
  }

  return c3y;
}

/* u3e_v4_backup(): copy snapshot from [pux_c] to [pax_c],
 * overwriting optionally. note that image files must
 * be named "north" and "south".
*/
c3_o
u3e_v4_backup(c3_c* pux_c, c3_c* pax_c, c3_o ovw_o)
{
  //  source image files from [pux_c]
  u3e_v4_image nux_u = { .nam_c = "north", .pgs_w = 0 };
  u3e_v4_image sux_u = { .nam_c = "south", .pgs_w = 0 };

  //  destination image files to [pax_c]
  u3e_v4_image nax_u = { .nam_c = "north", .pgs_w = 0 };
  u3e_v4_image sax_u = { .nam_c = "south", .pgs_w = 0 };

  c3_i mod_i = O_RDWR | O_CREAT;

  if ( !pux_c || !pax_c ) {
    fprintf(stderr, "loom: image backup: bad path\r\n");
    return c3n;
  }

  if ( (c3n == ovw_o) && c3_mkdir(pax_c, 0700) ) {
    if ( EEXIST != errno ) {
      fprintf(stderr, "loom: image backup: %s\r\n", strerror(errno));
    }
    return c3n;
  }

  //  open source image files if they exist
  //
  c3_c nux_c[8193];
  snprintf(nux_c, 8192, "%s/%s.bin", pux_c, nux_u.nam_c);
  if (  (0 != access(nux_c, F_OK))
     || (_ce_v4_img_good != _ce_v4_image_open(&nux_u, pux_c)) )
  {
    fprintf(stderr, "loom: couldn't open north image at %s\r\n", pux_c);
    return c3n;
  }

  c3_c sux_c[8193];
  snprintf(sux_c, 8192, "%s/%s.bin", pux_c, sux_u.nam_c);
  if (  (0 != access(sux_c, F_OK))
     || (_ce_v4_img_good != _ce_v4_image_open(&sux_u, pux_c)) )
  {
    fprintf(stderr, "loom: couldn't open south image at %s\r\n", pux_c);
    return c3n;
  }

  //  open destination image files
  c3_c nax_c[8193];
  snprintf(nax_c, 8192, "%s/%s.bin", pax_c, nax_u.nam_c);
  if ( -1 == (nax_u.fid_i = c3_open(nax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", nax_c, strerror(errno));
    return c3n;
  }
  c3_c sax_c[8193];
  snprintf(sax_c, 8192, "%s/%s.bin", pax_c, sax_u.nam_c);
  if ( -1 == (sax_u.fid_i = c3_open(sax_c, mod_i, 0666)) ) {
    fprintf(stderr, "loom: c3_open %s: %s\r\n", sax_c, strerror(errno));
    return c3n;
  }

  if (  (c3n == _ce_v4_image_copy(&nux_u, &nax_u))
     || (c3n == _ce_v4_image_copy(&sux_u, &sax_u))
     || (c3n == _ce_v4_image_sync(&nax_u))
     || (c3n == _ce_v4_image_sync(&sax_u)) )
  {
    c3_unlink(nax_c);
    c3_unlink(sax_c);
    fprintf(stderr, "loom: image backup failed\r\n");
    return c3n;
  }

  close(nax_u.fid_i);
  close(sax_u.fid_i);
  fprintf(stderr, "loom: image backup complete\r\n");
  return c3y;
}

/*
  u3e_v4_save(): save current changes.

  If we are in dry-run mode, do nothing.

  First, call `_ce_v4_patch_compose` to write all dirty pages to disk and
  clear protection and dirty bits. If there were no dirty pages to write,
  then we're done.

  - Sync the patch files to disk.
  - Verify the patch (because why not?)
  - Write the patch data into the image file (This is idempotent.).
  - Sync the image file.
  - Delete the patchfile and free it.

  Once we've written the dirty pages to disk (and have reset their dirty bits
  and protection flags), we *could* handle the rest of the checkpointing
  process in a separate thread, but we'd need to wait until that finishes
  before we try to make another snapshot.
*/
void
u3e_v4_save(u3_post low_p, u3_post hig_p)
{
  u3_ce_v4_patch* pat_u;
  c3_w nod_w = u3P_v4.nor_u.pgs_w;
  c3_w sod_w = u3P_v4.sou_u.pgs_w;

  if ( u3C.wag_w & u3o_dryrun ) {
    return;
  }

  {
    c3_w nop_w = (low_p >> u3a_v4_page);
    c3_w nor_w = (low_p + (_ce_v4_len_words(1) - 1)) >> u3a_v4_page;
    c3_w sop_w = hig_p >> u3a_v4_page;

    u3_assert( (u3P_v4.gar_w > nop_w) && (u3P_v4.gar_w < sop_w) );

    if ( !(pat_u = _ce_v4_patch_compose(nor_w, u3P_v4.pag_w - sop_w)) ) {
      return;
    }
  }

  //  attempt to avoid propagating anything insane to disk
  //
  u3a_v4_loom_sane();

  if ( u3C.wag_w & u3o_verbose ) {
    u3a_v4_print_memory(stderr, "sync: save", pat_u->con_u->pgs_w << u3a_v4_page);
  }

  _ce_v4_patch_sync(pat_u);

  if ( c3n == _ce_v4_patch_verify(pat_u) ) {
    u3_assert(!"loom: save failed");
  }

#ifdef U3_SNAPSHOT_VALIDATION
  //  check that clean pages are correct
  //
  u3_assert( c3y == _ce_v4_loom_fine() );
#endif

  _ce_v4_patch_apply(pat_u);

  u3_assert( c3y == _ce_v4_image_sync(&u3P_v4.nor_u) );
  u3_assert( c3y == _ce_v4_image_sync(&u3P_v4.sou_u) );

  _ce_v4_patch_free(pat_u);
  _ce_v4_patch_delete();

#ifdef U3_SNAPSHOT_VALIDATION
  {
    c3_w pgs_w;
    u3_assert( _ce_v4_img_good == _ce_v4_image_stat(&u3P_v4.nor_u, &pgs_w) );
    u3_assert( pgs_w == u3P_v4.nor_u.pgs_w );
    u3_assert( _ce_v4_img_good == _ce_v4_image_stat(&u3P_v4.sou_u, &pgs_w) );
    u3_assert( pgs_w == u3P_v4.sou_u.pgs_w );
  }
#endif

  _ce_v4_loom_protect_south(u3P_v4.sou_u.pgs_w, sod_w);

#ifdef U3_SNAPSHOT_VALIDATION
  //  check that all pages in the north/south segments are clean,
  //  all between are dirty, and clean pages are *fine*
  //
  //    since total finery requires total cleanliness,
  //    pages of the north segment are protected twice.
  //
  _ce_v4_loom_protect_north(u3P_v4.nor_u.pgs_w, nod_w);

  u3_assert( c3y == _ce_v4_loom_track_sane() );
  u3_assert( c3y == _ce_v4_loom_fine() );
#endif

  if ( u3C.wag_w & u3o_no_demand ) {
#ifndef U3_SNAPSHOT_VALIDATION
    _ce_v4_loom_protect_north(u3P_v4.nor_u.pgs_w, nod_w);
#endif
  }
  else {
    _ce_v4_loom_mapf_north(u3P_v4.nor_u.fid_i, u3P_v4.nor_u.pgs_w, nod_w);
  }

  u3e_v4_toss(low_p, hig_p);
}

/* _ce_v4_toss_pages(): discard ephemeral pages.
*/
static void
_ce_v4_toss_pages(c3_w nor_w, c3_w sou_w)
{
  c3_w  pgs_w = u3P_v4.pag_w - (nor_w + sou_w);
  void* ptr_v = _ce_v4_ptr(nor_w);

  if ( -1 == madvise(ptr_v, _ce_v4_len(pgs_w), MADV_DONTNEED) ) {
      fprintf(stderr, "loom: madv_dontneed failed (%u pages at %u): %s\r\n",
                      pgs_w, nor_w, strerror(errno));
  }
}

/* u3e_v4_toss(): discard ephemeral pages.
*/
void
u3e_v4_toss(u3_post low_p, u3_post hig_p)
{
  c3_w nor_w = (low_p + (_ce_v4_len_words(1) - 1)) >> u3a_v4_page;
  c3_w sou_w = u3P_v4.pag_w - (hig_p >> u3a_v4_page);

  _ce_v4_toss_pages(nor_w, sou_w);
}

/* u3e_v4_live(): start the checkpointing system.
*/
c3_o
u3e_v4_live(c3_o nuu_o, c3_c* dir_c)
{
  //  require that our page size is a multiple of the system page size.
  //
  {
    size_t sys_i = sysconf(_SC_PAGESIZE);

    if ( _ce_v4_page % sys_i ) {
      fprintf(stderr, "loom: incompatible system page size (%zuKB)\r\n",
                      sys_i >> 10);
      exit(1);
    }
  }

  u3P_v4.dir_c = dir_c;
  u3P_v4.eph_i = 0;
  u3P_v4.nor_u.nam_c = "north";
  u3P_v4.sou_u.nam_c = "south";
  u3P_v4.pag_w = u3C.wor_i >> u3a_v4_page;

  //  XX review dryrun requirements, enable or remove
  //
#if 0
  if ( u3C.wag_w & u3o_dryrun ) {
    return c3y;
  } else
#endif
  {
    //  Open the ephemeral space file.
    //
    if ( u3C.wag_w & u3o_swap ) {
      if ( c3n == _ce_v4_ephemeral_open(&u3P_v4.eph_i) ) {
        fprintf(stderr, "boot: failed to load ephemeral file\r\n");
        exit(1);
      }
    }

    //  Open image files.
    //
    c3_c chk_c[8193];
    snprintf(chk_c, 8193, "%s/.urb/chk", u3P_v4.dir_c);

    _ce_v4_img_stat nor_e = _ce_v4_image_open(&u3P_v4.nor_u, chk_c);
    _ce_v4_img_stat sou_e = _ce_v4_image_open(&u3P_v4.sou_u, chk_c);

    if ( (_ce_v4_img_fail == nor_e) || (_ce_v4_img_fail == sou_e) ) {
      fprintf(stderr, "boot: image failed\r\n");
      exit(1);
    }
    else {
      u3_ce_v4_patch* pat_u;
      c3_w nor_w, sou_w;

      /* Load any patch files; apply them to images.
      */
      if ( 0 != (pat_u = _ce_v4_patch_open()) ) {
        _ce_v4_patch_apply(pat_u);
        u3_assert( c3y == _ce_v4_image_sync(&u3P_v4.nor_u) );
        u3_assert( c3y == _ce_v4_image_sync(&u3P_v4.sou_u) );
        _ce_v4_patch_free(pat_u);
        _ce_v4_patch_delete();
      }
      else if ( (_ce_v4_img_size == nor_e) || (_ce_v4_img_size == sou_e) ) {
        fprintf(stderr, "boot: image failed (size)\r\n");
        exit(1);
      }

      nor_w = u3P_v4.nor_u.pgs_w;
      sou_w = u3P_v4.sou_u.pgs_w;

      //  detect snapshots from a larger loom
      //
      if ( (nor_w + sou_w + 1) >= u3P_v4.pag_w ) {
        fprintf(stderr, "boot: snapshot too big for loom\r\n");
        exit(1);
      }

      //  mark all pages dirty (pages in the snapshot will be marked clean)
      //
      u3e_v4_foul();

      /* Write image files to memory; reinstate protection.
      */
      {
        if ( u3C.wag_w & u3o_swap ) {
          _ce_v4_loom_mapf_ephemeral();
        }

        if ( u3C.wag_w & u3o_no_demand ) {
          _ce_v4_loom_blit_north(u3P_v4.nor_u.fid_i, nor_w);
        }
        else {
          _ce_v4_loom_mapf_north(u3P_v4.nor_u.fid_i, nor_w, 0);
        }

        _ce_v4_loom_blit_south(u3P_v4.sou_u.fid_i, sou_w);

        u3l_log("boot: protected loom");
      }

      /* If the images were empty, we are logically booting.
      */
      if ( !nor_w && !sou_w ) {
        u3l_log("live: logical boot");
        nuu_o = c3y;
      }
      else if ( u3C.wag_w & u3o_no_demand ) {
        u3a_v4_print_memory(stderr, "live: loaded", _ce_v4_len_words(nor_w + sou_w));
      }
      else {
        u3a_v4_print_memory(stderr, "live: mapped", nor_w << u3a_v4_page);
        u3a_v4_print_memory(stderr, "live: loaded", sou_w << u3a_v4_page);
      }

#ifdef U3_GUARD_PAGE
      u3_assert( !_ce_v4_ward_post(nor_w, u3P_v4.pag_w - sou_w) );
#endif
    }
  }

  return nuu_o;
}

/* u3e_v4_stop(): gracefully stop the persistence system.
*/
void
u3e_v4_stop(void)
{
  if ( u3P_v4.eph_i ) {
    _ce_v4_toss_pages(u3P_v4.nor_u.pgs_w, u3P_v4.sou_u.pgs_w);
    close(u3P_v4.eph_i);
    unlink(u3C.eph_c);
  }

  close(u3P_v4.sou_u.fid_i);
  close(u3P_v4.sou_u.fid_i);
}

/* u3e_v4_yolo(): disable dirty page tracking, read/write whole loom.
*/
c3_o
u3e_v4_yolo(void)
{
  //  NB: u3e_v4_save() will reinstate protection flags
  //
  if ( 0 != mprotect(_ce_v4_ptr(0),
                     _ce_v4_len(u3P_v4.pag_w),
                     (PROT_READ | PROT_WRITE)) )
  {
    //  XX confirm recoverable errors
    //
    fprintf(stderr, "loom: yolo: %s\r\n", strerror(errno));
    return c3n;
  }

  u3_assert( !_ce_v4_ward_protect() );

  return c3y;
}

/* u3e_v4_foul(): dirty all the pages of the loom.
*/
void
u3e_v4_foul(void)
{
  memset((void*)u3P_v4.dit_w, 0xff, sizeof(u3P_v4.dit_w));
}

/* u3e_v4_init(): initialize guard page tracking, dirty loom
*/
void
u3e_v4_init(void)
{
  u3P_v4.pag_w = u3C.wor_i >> u3a_v4_page;

  u3P_v4.nor_u.fid_i = u3P_v4.sou_u.fid_i = -1;

  u3e_v4_foul();

#ifdef U3_GUARD_PAGE
  u3_assert( !_ce_v4_ward_post(0, u3P_v4.pag_w) );
#endif
}

/* u3e_v4_ward(): reposition guard page if needed.
*/
void
u3e_v4_ward(u3_post low_p, u3_post hig_p)
{
#ifdef U3_GUARD_PAGE
  c3_w nop_w = low_p >> u3a_v4_page;
  c3_w sop_w = hig_p >> u3a_v4_page;
  c3_w pag_w = u3P_v4.gar_w;

  if ( !((pag_w > nop_w) && (pag_w < sop_w)) ) {
    u3_assert( !_ce_v4_ward_post(nop_w, sop_w) );
    u3_assert( !_ce_v4_flaw_mprotect(pag_w) );
    u3_assert( u3P_v4.dit_w[pag_w >> 5] & ((c3_w)1 << (pag_w & 31)) );
  }
#endif
}

  /***  jets.h
  ***/
/* _cj_fink_free(): lose and free everything in a u3j_fink.
*/
static void
_cj_v4_fink_free(u3p(u3j_v4_fink) fin_p)
{
  c3_w i_w;
  u3j_v4_fink* fin_u = u3v4to(u3j_v4_fink, fin_p);
  u3a_v4_lose(fin_u->sat);
  for ( i_w = 0; i_w < fin_u->len_w; ++i_w ) {
    u3j_v4_fist* fis_u = &(fin_u->fis_u[i_w]);
    u3a_v4_lose(fis_u->bat);
    u3a_v4_lose(fis_u->pax);
  }
  u3a_v4_wfree(fin_u);
}

/* u3j_site_lose(): lose references of u3j_site (but do not free).
 */
void
u3j_v4_site_lose(u3j_v4_site* sit_u)
{
  u3a_v4_lose(sit_u->axe);
  if ( u3_none != sit_u->bat ) {
    u3a_v4_lose(sit_u->bat);
  }
  if ( u3_none != sit_u->bas ) {
    u3a_v4_lose(sit_u->bas);
  }
  if ( u3_none != sit_u->loc ) {
    u3a_v4_lose(sit_u->loc);
    u3a_v4_lose(sit_u->lab);
    if ( c3y == sit_u->fon_o ) {
      _cj_v4_fink_free(sit_u->fin_p);
    }
  }
}

/* u3j_rite_lose(): lose references of u3j_rite (but do not free).
 */
void
u3j_v4_rite_lose(u3j_v4_rite* rit_u)
{
  if ( (c3y == rit_u->own_o) && u3_none != rit_u->clu ) {
    u3a_v4_lose(rit_u->clu);
    _cj_v4_fink_free(rit_u->fin_p);
  }
}

/* u3j_free_hank(): free an entry from the hank cache.
*/
void
u3j_v4_free_hank(u3_noun kev)
{
  u3j_v4_hank* han_u = u3v4to(u3j_v4_hank, u3t(kev));
  if ( u3_none != han_u->hax ) {
    u3a_v4_lose(han_u->hax);
    u3j_site_lose(&(han_u->sit_u));
  }
  u3a_v4_wfree(han_u);
}


  /***  hashtable.h
  ***/
/* u3h_v4_new_cache(): create hashtable with bounded size.
*/
u3p(u3h_v4_root)
u3h_v4_new_cache(c3_w max_w)
{
  u3h_v4_root*     har_u = u3a_v4_walloc(c3_wiseof(u3h_v4_root));
  u3p(u3h_v4_root) har_p = u3v4of(u3h_v4_root, har_u);
  c3_w        i_w;

  har_u->max_w       = max_w;
  har_u->use_w       = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    har_u->sot_w[i_w] = 0;
  }
  return har_p;
}

/* u3h_v4_new(): create hashtable.
*/
u3p(u3h_v4_root)
u3h_v4_new(void)
{
  return u3h_v4_new_cache(0);
}

/* _ch_free_buck(): free bucket
*/
static void
_ch_v4_free_buck(u3h_v4_buck* hab_u)
{
  //fprintf(stderr, "free buck\r\n");
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3a_v4_lose(u3h_v4_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_v4_wfree(hab_u);
}

/* _ch_free_node(): free node.
*/
static void
_ch_v4_free_node(u3h_v4_node* han_u, c3_w lef_w, c3_o pin_o)
{
  c3_w len_w = c3_pc_w(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];
    if ( _(u3h_v4_slot_is_null(sot_w))) {
    }  else if ( _(u3h_v4_slot_is_noun(sot_w)) ) {
      u3a_v4_lose(u3h_v4_slot_to_noun(sot_w));
    } else {
      void* hav_v = u3h_v4_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v4_free_buck(hav_v);
      } else {
        _ch_v4_free_node(hav_v, lef_w, pin_o);
      }
    }
  }
  u3a_v4_wfree(han_u);
}

/* u3h_v4_free(): free hashtable.
*/
void
u3h_v4_free(u3p(u3h_v4_root) har_p)
{
  u3h_v4_root* har_u = u3v4to(u3h_v4_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v4_slot_is_noun(sot_w)) ) {
      u3a_v4_lose(u3h_v4_slot_to_noun(sot_w));
    }
    else if ( _(u3h_v4_slot_is_node(sot_w)) ) {
      u3h_v4_node* han_u = u3h_v4_slot_to_node(sot_w);

      _ch_v4_free_node(han_u, 25, i_w == 57);
    }
  }
  u3a_v4_wfree(har_u);
}


/* _ch_walk_buck(): walk bucket for gc.
*/
static void
_ch_v4_walk_buck(u3h_v4_buck* hab_u, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    fun_f(u3h_v4_slot_to_noun(hab_u->sot_w[i_w]), wit);
  }
}

/* _ch_walk_node(): walk node for gc.
*/
static void
_ch_v4_walk_node(u3h_v4_node* han_u, c3_w lef_w, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w len_w = c3_pc_w(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v4_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v4_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else {
      void* hav_v = u3h_v4_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v4_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_v4_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_v4_walk_with(): traverse hashtable with key, value fn and data
 *                  argument; RETAINS.
*/
void
u3h_v4_walk_with(u3p(u3h_v4_root) har_p,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  u3h_v4_root* har_u = u3v4to(u3h_v4_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v4_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v4_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else if ( _(u3h_v4_slot_is_node(sot_w)) ) {
      u3h_v4_node* han_u = u3h_v4_slot_to_node(sot_w);

      _ch_v4_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

/* _ch_walk_plain(): use plain u3_noun fun_f for each node
 */
static void
_ch_v4_walk_plain(u3_noun kev, void* wit)
{
  void (*fun_f)(u3_noun) = (void (*)(u3_noun))wit;
  fun_f(kev);
}

/* u3h_v4_walk(): u3h_v4_walk_with, but with no data argument
*/
void
u3h_v4_walk(u3p(u3h_v4_root) har_p, void (*fun_f)(u3_noun))
{
  u3h_v4_walk_with(har_p, _ch_v4_walk_plain, (void *)fun_f);
}

/* u3j_v4_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3j_v4_reclaim(void)
{
  //  clear the jet hank cache
  //
  u3h_v4_walk(u3R_v4->jed.han_p, u3j_v4_free_hank);
  u3h_v4_free(u3R_v4->jed.han_p);
  u3R_v4->jed.han_p = u3h_v4_new();
}


/* _cn_prog_free(): free memory retained by program pog_u
*/
static void
_cn_v4_prog_free(u3n_v4_prog* pog_u)
{
  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3a_v4_lose(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3a_v4_lose(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_v4_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_v4_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_v4_free(pog_u);
}

static inline u3n_v4_prog*
_cn_v4_to_prog(c3_w pog_w)
{
  u3_post pog_p = pog_w << u3a_v4_vits;
  return u3v4to(u3n_v4_prog, pog_p);
}

/* _n_feb(): u3h_walk helper for u3n_free
 */
static void
_n_v4_feb(u3_noun kev)
{
  _cn_v4_prog_free(_cn_v4_to_prog(u3t(kev)));
}

/* u3n_free(): free bytecode cache
 */
void
u3n_v4_free()
{
  u3p(u3h_v4_root) har_p = u3R_v4->byc.har_p;
  u3h_v4_walk(har_p, _n_v4_feb);
  u3h_v4_free(har_p);
}

/* u3n_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3n_v4_reclaim(void)
{
  //  clear the bytecode cache
  //
  //    We can't just u3h_free() -- the value is a post to a u3n_prog.
  //    Note that the hank cache *must* also be freed (in u3j_reclaim())
  //
  u3n_v4_free();
  u3R_v4->byc.har_p = u3h_v4_new();
}

  /***  manage.h
  ***/

/* u3m_reclaim: clear persistent caches to reclaim memory.
*/
void
u3m_v4_reclaim(void)
{
  //  NB: subset: u3a and u3v excluded
  u3j_v4_reclaim();
  u3n_v4_reclaim();
}

  /***  retrieve.h
  ***/

typedef struct {
  c3_l  mug_l;
  u3_cell cel;
} _cr_v4_mugf;

/* _cr_v4_mug_next(): advance mug calculation, pushing cells onto the stack.
*/
static inline c3_l
_cr_v4_mug_next(u3a_v4_pile* pil_u, u3_noun veb)
{
  while ( 1 ) {
    //  veb is a direct atom, mug is not memoized
    //
    if ( c3y == u3a_v4_is_cat(veb) ) {
      return (c3_l)u3r_v4_mug_words(&veb, 1);
    }
    //  veb is indirect, a pointer into the loom
    //
    else {
      u3a_v4_noun* veb_u = u3a_v4_to_ptr(veb);

      //  veb has already been mugged, return memoized value
      //
      //    XX add debug assertion that mug is 31-bit?
      //
      if ( veb_u->mug_w ) {
        return (c3_l)veb_u->mug_w;
      }
      //  veb is an indirect atom, mug its bytes and memoize
      //
      else if ( c3y == u3a_v4_is_atom(veb) ) {
        u3a_v4_atom* vat_u = (u3a_v4_atom*)veb_u;
        c3_l         mug_l = u3r_v4_mug_words(vat_u->buf_w, vat_u->len_w);
        vat_u->mug_w = mug_l;
        return mug_l;
      }
      //  veb is a cell, push a stack frame to mark head-recursion
      //  and read the head
      //
      else {
        u3a_v4_cell* cel_u = (u3a_v4_cell*)veb_u;
        _cr_v4_mugf* fam_u = u3a_v4_push(pil_u);

        fam_u->mug_l = 0;
        fam_u->cel   = veb;

        veb = cel_u->hed;
        continue;
      }
    }
  }
}

/* u3r_mug(): statefully mug a noun with 31-bit murmur3.
*/
c3_l
u3r_v4_mug(u3_noun veb)
{
  u3a_v4_pile  pil_u;
  _cr_v4_mugf* fam_u;
  c3_l         mug_l;

  //  sanity check
  //
  u3_assert( u3_none != veb );

  u3a_v4_pile_prep(&pil_u, sizeof(*fam_u));

  //  commence mugging
  //
  mug_l = _cr_v4_mug_next(&pil_u, veb);

  //  process cell results
  //
  if ( c3n == u3a_v4_pile_done(&pil_u) ) {
    fam_u = u3a_v4_peek(&pil_u);

    do {
      //  head-frame: stash mug and continue into the tail
      //
      if ( !fam_u->mug_l ) {
        u3a_v4_cell* cel_u = u3a_v4_to_ptr(fam_u->cel);

        fam_u->mug_l = mug_l;
        mug_l        = _cr_v4_mug_next(&pil_u, cel_u->tel);
        fam_u        = u3a_v4_peek(&pil_u);
      }
      //  tail-frame: calculate/memoize cell mug and pop the stack
      //
      else {
        u3a_v4_cell* cel_u = u3a_v4_to_ptr(fam_u->cel);

        mug_l        = u3r_v4_mug_both(fam_u->mug_l, mug_l);
        cel_u->mug_w = mug_l;
        fam_u        = u3a_v4_pop(&pil_u);
      }
    }
    while ( c3n == u3a_v4_pile_done(&pil_u) );
  }

  return mug_l;
}

/***  init
***/

void
u3_v4_load(c3_z wor_i)
{
  c3_w ver_w = *(u3_Loom_v4 + wor_i - 1);

  u3_assert( U3V_VER4 == ver_w );

  c3_w* mem_w = u3_Loom_v4 + u3a_v4_walign;
  c3_w  siz_w = c3_wiseof(u3v_v4_home);
  c3_w  len_w = wor_i - u3a_v4_walign;
  c3_w* mat_w = c3_align(mem_w + len_w - siz_w, u3a_v4_balign, C3_ALGLO);

  u3H_v4 = (void *)mat_w;
  u3R_v4 = &u3H_v4->rod_u;

  u3R_v4->cap_p = u3R_v4->mat_p = u3a_v4_outa(u3H_v4);
}