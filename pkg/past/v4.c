#include "v4.h"

#include "nock.h"

  /***  current
  ***/
#     define  u3a_v4_is_pom           u3a_v5_is_pom
#     define  u3a_v4_north_is_normal  u3a_v5_north_is_normal
#     define  u3n_v4_prog             u3n_v5_prog

#     define  u3a_v4_boxed(len_w)  (len_w + c3_wiseof(u3a_v4_box) + 1)
#     define  u3a_v4_boxto(box_v)  ( (void *) \
                                   ( (u3a_v4_box *)(void *)(box_v) + 1 ) )
#     define  u3a_v4_botox(tox_v)  ( (u3a_v4_box *)(void *)(tox_v) - 1 )
#     define  u3h_v4_slot_to_node(sot)  (u3a_v4_into(((sot) & 0x3fffffff) << u3a_v4_vits))

u3a_v4_road* u3a_v4_Road;
u3v_v4_home* u3v_v4_Home;

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

/* u3a_v4_lose(): lose a reference count.
*/
void
u3a_v4_lose(u3_noun som)
{
  if ( !_(u3a_v4_is_cat(som)) ) {
    _me_v4_lose_north(som);
  }
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

/* u3j_v4_site_lose(): lose references of u3j_site (but do not free).
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
  u3a_v4_cell* kev_u = (u3a_v4_cell*) u3a_v4_to_ptr(kev);
  u3j_v4_hank* han_u = u3v4to(u3j_v4_hank, kev_u->tel);
  if ( u3_none != han_u->hax ) {
    u3a_v4_lose(han_u->hax);
    u3j_v4_site_lose(&(han_u->sit_u));
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
  //  ream pointers inline
  //
  c3_w pad_w = (8 - pog_u->byc_u.len_w % 8) % 8;
  c3_w pod_w = pog_u->lit_u.len_w % 2;
  c3_w ped_w = pog_u->mem_u.len_w % 2;

  pog_u->byc_u.ops_y = (c3_y*)((void*) pog_u) + sizeof(u3n_v4_prog);
  pog_u->lit_u.non   = (u3_noun*) (pog_u->byc_u.ops_y + pog_u->byc_u.len_w + pad_w);
  pog_u->mem_u.sot_u = (u3n_memo*) (pog_u->lit_u.non + pog_u->lit_u.len_w + pod_w);
  pog_u->cal_u.sit_u = (u3j_v4_site*) (pog_u->mem_u.sot_u + pog_u->mem_u.len_w + ped_w);
  pog_u->reg_u.rit_u = (u3j_v4_rite*) (pog_u->cal_u.sit_u + pog_u->cal_u.len_w);

  //  NB: site reaming elided

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

/* _n_feb(): u3h_walk helper for u3n_free
 */
static void
_n_v4_feb(u3_noun kev)
{
  u3a_v4_cell* kev_u = (u3a_v4_cell*) u3a_v4_to_ptr(kev);
  _cn_v4_prog_free(u3v4to(u3n_v4_prog, kev_u->tel));
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