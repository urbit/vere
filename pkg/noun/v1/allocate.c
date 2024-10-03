/// @file

#include "pkg/noun/allocate.h"
#include "pkg/noun/v1/allocate.h"

#include "pkg/noun/v1/hashtable.h"

/* _box_v1_slot(): select the right free list to search for a block.
*/
static c3_w
_box_v1_slot(c3_w siz_w)
{
  if ( siz_w < u3a_v1_minimum ) {
    return 0;
  }
  else {
    c3_w i_w = 1;

    while ( 1 ) {
      if ( i_w == u3a_v1_fbox_no ) {
        return (i_w - 1);
      }
      if ( siz_w < 16 ) {
        return i_w;
      }
      siz_w = (siz_w + 1) >> 1;
      i_w += 1;
    }
  }
}

/* _box_v1_make(): construct a box.
*/
static u3a_v1_box*
_box_v1_make(void* box_v, c3_w siz_w, c3_w use_w)
{
  u3a_v1_box* box_u = box_v;
  c3_w*    box_w = box_v;

  u3_assert(siz_w >= u3a_v1_minimum);

  box_w[0] = siz_w;
  box_w[siz_w - 1] = siz_w;
  box_u->use_w = use_w;

# ifdef  U3_MEMORY_DEBUG
    box_u->cod_w = u3_Code;
    box_u->eus_w = 0;
# endif

  return box_u;
}

/* _box_v1_attach(): attach a box to the free list.
*/
static void
_box_v1_attach(u3a_v1_box* box_u)
{
  u3_assert(box_u->siz_w >= (1 + c3_wiseof(u3a_v1_fbox)));
  u3_assert(0 != u3of(u3a_v1_fbox, box_u));

  {
    c3_w           sel_w = _box_v1_slot(box_u->siz_w);
    u3p(u3a_v1_fbox)  fre_p = u3of(u3a_v1_fbox, box_u);
    u3p(u3a_v1_fbox)* pfr_p = &u3R_v1->all.fre_p[sel_w];
    u3p(u3a_v1_fbox)  nex_p = *pfr_p;

    u3to(u3a_v1_fbox, fre_p)->pre_p = 0;
    u3to(u3a_v1_fbox, fre_p)->nex_p = nex_p;
    if ( u3to(u3a_v1_fbox, fre_p)->nex_p ) {
      u3to(u3a_v1_fbox, u3to(u3a_v1_fbox, fre_p)->nex_p)->pre_p = fre_p;
    }
    (*pfr_p) = fre_p;
  }
}

/* _box_v1_detach(): detach a box from the free list.
*/
static void
_box_v1_detach(u3a_v1_box* box_u)
{
  u3p(u3a_v1_fbox) fre_p = u3of(u3a_v1_fbox, box_u);
  u3p(u3a_v1_fbox) pre_p = u3to(u3a_v1_fbox, fre_p)->pre_p;
  u3p(u3a_v1_fbox) nex_p = u3to(u3a_v1_fbox, fre_p)->nex_p;


  if ( nex_p ) {
    if ( u3to(u3a_v1_fbox, nex_p)->pre_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3to(u3a_v1_fbox, nex_p)->pre_p = pre_p;
  }
  if ( pre_p ) {
    if( u3to(u3a_v1_fbox, pre_p)->nex_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3to(u3a_v1_fbox, pre_p)->nex_p = nex_p;
  }
  else {
    c3_w sel_w = _box_v1_slot(box_u->siz_w);

    if ( fre_p != u3R_v1->all.fre_p[sel_w] ) {
      u3_assert(!"loom: corrupt");
    }
    u3R_v1->all.fre_p[sel_w] = nex_p;
  }
}

/* _box_v1_free(): free and coalesce.
*/
static void
_box_v1_free(u3a_v1_box* box_u)
{
  c3_w* box_w = (c3_w *)(void *)box_u;

  u3_assert(box_u->use_w != 0);
  box_u->use_w -= 1;
  if ( 0 != box_u->use_w ) {
    return;
  }

  //  we're always migrating a north road, so no need to check for it
  {
    /* Try to coalesce with the block below.
    */
    if ( box_w != u3a_v1_into(u3R_v1->rut_p) ) {
      c3_w       laz_w = *(box_w - 1);
      u3a_v1_box* pox_u = (u3a_v1_box*)(void *)(box_w - laz_w);

      if ( 0 == pox_u->use_w ) {
        _box_v1_detach(pox_u);
        _box_v1_make(pox_u, (laz_w + box_u->siz_w), 0);

        box_u = pox_u;
        box_w = (c3_w*)(void *)pox_u;
      }
    }

    /* Try to coalesce with the block above, or the wilderness.
    */
    if ( (box_w + box_u->siz_w) == u3a_v1_into(u3R_v1->hat_p) ) {
      u3R_v1->hat_p = u3a_v1_outa(box_w);
    }
    else {
      u3a_v1_box* nox_u = (u3a_v1_box*)(void *)(box_w + box_u->siz_w);

      if ( 0 == nox_u->use_w ) {
        _box_v1_detach(nox_u);
        _box_v1_make(box_u, (box_u->siz_w + nox_u->siz_w), 0);
      }
      _box_v1_attach(box_u);
    }
  }
}

/* u3a_v1_wfree(): free storage.
*/
void
u3a_v1_wfree(void* tox_v)
{
  _box_v1_free(u3a_v1_botox(tox_v));
}

/* u3a_v1_free(): free for aligned malloc.
*/
void
u3a_v1_free(void* tox_v)
{
  if (NULL == tox_v)
    return;

  c3_w* tox_w = tox_v;
  c3_w  pad_w = tox_w[-1];
  c3_w* org_w = tox_w - (pad_w + 1);

  // u3l_log("free %p %p", org_w, tox_w);
  u3a_v1_wfree(org_w);
}

/* u3a_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3a_v1_reclaim(void)
{
  //  clear the memoization cache
  //
  u3h_v1_free_nodes(u3R_v1->cax.har_p);
}

/* _me_v1_lose_north(): lose on a north road.
*/
static void
_me_v1_lose_north(u3_noun dog)
{
top:
  {
    c3_w* dog_w      = u3a_v1_to_ptr(dog);
    u3a_v1_box* box_u = u3a_v1_botox(dog_w);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_v1_bail(c3__foul);
      }
      else {
        if ( _(u3a_v1_is_pom(dog)) ) {
          u3a_v1_cell* dog_u = (void *)dog_w;
          u3_noun     h_dog = dog_u->hed;
          u3_noun     t_dog = dog_u->tel;

          if ( !_(u3a_v1_is_cat(h_dog)) ) {
            _me_v1_lose_north(h_dog);
          }
          u3a_v1_wfree(dog_w);
          if ( !_(u3a_v1_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_v1_wfree(dog_w);
        }
      }
    }
  }
}

/* u3a_v1_lose(): lose a reference count.
*/
void
u3a_v1_lose(u3_noun som)
{
  if ( !_(u3a_v1_is_cat(som)) ) {
    _me_v1_lose_north(som);
  }
}
