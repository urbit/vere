#include "v1.h"

  /***  allocate.h
  ***/

#     define u3a_v1_botox     u3a_v2_botox
#     define u3a_v1_box       u3a_v2_box
#     define u3a_v1_cell      u3a_v2_cell
#     define u3a_v1_fbox      u3a_v2_fbox
#     define u3a_v1_fbox_no   u3a_v2_fbox_no
#     define u3a_v1_into      u3a_v2_into
#     define u3a_v1_is_cat    u3a_v2_is_cat
#     define u3a_v1_is_north  u3a_v2_is_north
#     define u3a_v1_is_pom    u3a_v2_is_pom
#     define u3a_v1_minimum   u3a_v2_minimum
#     define u3a_v1_outa      u3a_v2_outa

#     define  u3v1to          u3v2to
#     define  u3v1of          u3v2of

  /***  hashtable.h
  ***/

#     define  u3h_v1_buck          u3h_v2_buck
#     define  u3h_v1_node          u3h_v2_node
#     define  u3h_v1_root          u3h_v2_root
#     define  u3h_v1_slot_is_node  u3h_v2_slot_is_node
#     define  u3h_v1_slot_is_noun  u3h_v2_slot_is_noun
#     define  u3h_v1_slot_to_noun  u3h_v2_slot_to_noun

     /* u3h_v1_free(): free hashtable.
      */
        void
        u3h_v1_free_nodes(u3p(u3h_v1_root) har_p);


  /***  jets.h
  ***/
#     define u3j_v1_fink       u3j_v2_fink
#     define u3j_v1_fist       u3j_v2_fist
#     define u3j_v1_hank       u3j_v2_hank
#     define u3j_v1_rite       u3j_v2_rite
#     define u3j_v1_site       u3j_v2_site


  /***  nock.h
  ***/
#     define  u3n_v1_memo  u3n_v2_memo
#     define  u3n_v1_prog  u3n_v2_prog


  /***  vortex.h
  ***/
#     define  u3A_v1       u3A_v2


/***  allocate.c
***/

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

  return box_u;
}

/* _box_v1_attach(): attach a box to the free list.
*/
static void
_box_v1_attach(u3a_v1_box* box_u)
{
  u3_assert(box_u->siz_w >= (1 + c3_wiseof(u3a_v1_fbox)));
  u3_assert(0 != u3v1of(u3a_v1_fbox, box_u));

  {
    c3_w           sel_w = _box_v1_slot(box_u->siz_w);
    u3p(u3a_v1_fbox)  fre_p = u3v1of(u3a_v1_fbox, box_u);
    u3p(u3a_v1_fbox)* pfr_p = &u3R_v1->all.fre_p[sel_w];
    u3p(u3a_v1_fbox)  nex_p = *pfr_p;

    u3v1to(u3a_v1_fbox, fre_p)->pre_p = 0;
    u3v1to(u3a_v1_fbox, fre_p)->nex_p = nex_p;
    if ( u3v1to(u3a_v1_fbox, fre_p)->nex_p ) {
      u3v1to(u3a_v1_fbox, u3v1to(u3a_v1_fbox, fre_p)->nex_p)->pre_p = fre_p;
    }
    (*pfr_p) = fre_p;
  }
}

/* _box_v1_detach(): detach a box from the free list.
*/
static void
_box_v1_detach(u3a_v1_box* box_u)
{
  u3p(u3a_v1_fbox) fre_p = u3v1of(u3a_v1_fbox, box_u);
  u3p(u3a_v1_fbox) pre_p = u3v1to(u3a_v1_fbox, fre_p)->pre_p;
  u3p(u3a_v1_fbox) nex_p = u3v1to(u3a_v1_fbox, fre_p)->nex_p;


  if ( nex_p ) {
    if ( u3v1to(u3a_v1_fbox, nex_p)->pre_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3v1to(u3a_v1_fbox, nex_p)->pre_p = pre_p;
  }
  if ( pre_p ) {
    if( u3v1to(u3a_v1_fbox, pre_p)->nex_p != fre_p ) {
      u3_assert(!"loom: corrupt");
    }
    u3v1to(u3a_v1_fbox, pre_p)->nex_p = nex_p;
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
        fprintf(stderr, "bail: foul\r\n");
        abort();
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


/***  hashtable.c
***/

/* _ch_v1_free_buck(): free bucket
*/
static void
_ch_v1_free_buck(u3h_v1_buck* hab_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    u3a_v1_lose(u3h_v1_slot_to_noun(hab_u->sot_w[i_w]));
  }
  u3a_v1_wfree(hab_u);
}

/* _ch_v1_free_node(): free node.
*/
static void
_ch_v1_free_node(u3h_v1_node* han_u, c3_w lef_w)
{
  c3_w len_w = c3_pc_w(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3a_v1_lose(u3h_v1_slot_to_noun(sot_w));
    }
    else {
      void* hav_v = u3h_v1_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v1_free_buck(hav_v);
      } else {
        _ch_v1_free_node(hav_v, lef_w);
      }
    }
  }
  u3a_v1_wfree(han_u);
}

/* u3h_v1_free_nodes(): free hashtable nodes.
*/
void
u3h_v1_free_nodes(u3p(u3h_v1_root) har_p)
{
  u3h_v1_root* har_u = u3v1to(u3h_v1_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3a_v1_lose(u3h_v1_slot_to_noun(sot_w));
    }
    else if ( _(u3h_v1_slot_is_node(sot_w)) ) {
      u3h_v1_node* han_u = (u3h_v1_node*) u3h_v1_slot_to_node(sot_w);

      _ch_v1_free_node(han_u, 25);
    }
    har_u->sot_w[i_w] = 0;
  }
  har_u->use_w       = 0;
  har_u->arm_u.mug_w = 0;
  har_u->arm_u.inx_w = 0;
}

/* _ch_v1_walk_buck(): walk bucket for gc.
*/
static void
_ch_v1_walk_buck(u3h_v1_buck* hab_u, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hab_u->len_w; i_w++ ) {
    fun_f(u3h_v1_slot_to_noun(hab_u->sot_w[i_w]), wit);
  }
}

/* _ch_v1_walk_node(): walk node for gc.
*/
static void
_ch_v1_walk_node(u3h_v1_node* han_u, c3_w lef_w, void (*fun_f)(u3_noun, void*), void* wit)
{
  c3_w len_w = c3_pc_w(han_u->map_w);
  c3_w i_w;

  lef_w -= 5;

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    c3_w sot_w = han_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v1_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else {
      void* hav_v = u3h_v1_slot_to_node(sot_w);

      if ( 0 == lef_w ) {
        _ch_v1_walk_buck(hav_v, fun_f, wit);
      } else {
        _ch_v1_walk_node(hav_v, lef_w, fun_f, wit);
      }
    }
  }
}

/* u3h_v1_walk_with(): traverse hashtable with key, value fn and data
 *                  argument; RETAINS.
*/
void
u3h_v1_walk_with(u3p(u3h_v1_root) har_p,
              void (*fun_f)(u3_noun, void*),
              void* wit)
{
  u3h_v1_root* har_u = u3v1to(u3h_v1_root, har_p);
  c3_w        i_w;

  for ( i_w = 0; i_w < 64; i_w++ ) {
    c3_w sot_w = har_u->sot_w[i_w];

    if ( _(u3h_v1_slot_is_noun(sot_w)) ) {
      u3_noun kev = u3h_v1_slot_to_noun(sot_w);

      fun_f(kev, wit);
    }
    else if ( _(u3h_v1_slot_is_node(sot_w)) ) {
      u3h_v1_node* han_u = (u3h_v1_node*) u3h_v1_slot_to_node(sot_w);

      _ch_v1_walk_node(han_u, 25, fun_f, wit);
    }
  }
}

/* _ch_v1_walk_plain(): use plain u3_noun fun_f for each node
 */
static void
_ch_v1_walk_plain(u3_noun kev, void* wit)
{
  void (*fun_f)(u3_noun) = (void (*)(u3_noun))wit;
  fun_f(kev);
}

/* u3h_v1_walk(): u3h_v1_walk_with, but with no data argument
 */
void
u3h_v1_walk(u3p(u3h_v1_root) har_p, void (*fun_f)(u3_noun))
{
  u3h_v1_walk_with(har_p, _ch_v1_walk_plain, (void *)fun_f);
}


/***  jets.c
***/

/* _cj_fink_free(): lose and free everything in a u3j_v1_fink.
*/
static void
_cj_v1_fink_free(u3p(u3j_v1_fink) fin_p)
{
  c3_w i_w;
  u3j_v1_fink* fin_u = u3v1to(u3j_v1_fink, fin_p);
  u3a_v1_lose(fin_u->sat);
  for ( i_w = 0; i_w < fin_u->len_w; ++i_w ) {
    u3j_v1_fist* fis_u = &(fin_u->fis_u[i_w]);
    u3a_v1_lose(fis_u->bat);
    u3a_v1_lose(fis_u->pax);
  }
  u3a_v1_wfree(fin_u);
}

/* u3j_v1_rite_lose(): lose references of u3j_v1_rite (but do not free).
 */
void
u3j_v1_rite_lose(u3j_v1_rite* rit_u)
{
  if ( (c3y == rit_u->own_o) && u3_none != rit_u->clu ) {
    u3a_v1_lose(rit_u->clu);
    _cj_v1_fink_free(rit_u->fin_p);
  }
}


/* u3j_v1_site_lose(): lose references of u3j_v1_site (but do not free).
 */
void
u3j_v1_site_lose(u3j_v1_site* sit_u)
{
  u3a_v1_lose(sit_u->axe);
  if ( u3_none != sit_u->bat ) {
    u3a_v1_lose(sit_u->bat);
  }
  if ( u3_none != sit_u->bas ) {
    u3a_v1_lose(sit_u->bas);
  }
  if ( u3_none != sit_u->loc ) {
    u3a_v1_lose(sit_u->loc);
    u3a_v1_lose(sit_u->lab);
    if ( c3y == sit_u->fon_o ) {
      if ( sit_u->fin_p ) {
      _cj_v1_fink_free(sit_u->fin_p);
      }
    }
  }
}

/* _cj_v1_free_hank(): free an entry from the hank cache.
*/
static void
_cj_v1_free_hank(u3_noun kev)
{
  u3a_v1_cell* cel_u = (u3a_v1_cell*) u3a_v1_to_ptr(kev);
  u3j_v1_hank* han_u = u3v1to(u3j_v1_hank, cel_u->tel);
  if ( u3_none != han_u->hax ) {
    u3a_v1_lose(han_u->hax);
    u3j_v1_site_lose(&(han_u->sit_u));
  }
  u3a_v1_wfree(han_u);
}

/* u3j_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3j_v1_reclaim(void)
{
  //  clear the jet hank cache
  //
  u3h_v1_walk(u3R_v1->jed.han_p, _cj_v1_free_hank);
  u3h_v1_free_nodes(u3R_v1->jed.han_p);
}


/***  nock.c
***/

/* _cn_v1_prog_free(): free memory retained by program pog_u
*/
static void
_cn_v1_prog_free(u3n_v1_prog* pog_u)
{
  // fix up pointers for loom portability
  pog_u->byc_u.ops_y = (c3_y*) ((void*) pog_u) + sizeof(u3n_v1_prog);
  pog_u->lit_u.non   = (u3_noun*) (pog_u->byc_u.ops_y + pog_u->byc_u.len_w);
  pog_u->mem_u.sot_u = (u3n_v1_memo*) (pog_u->lit_u.non + pog_u->lit_u.len_w);
  pog_u->cal_u.sit_u = (u3j_v1_site*) (pog_u->mem_u.sot_u + pog_u->mem_u.len_w);
  pog_u->reg_u.rit_u = (u3j_v1_rite*) (pog_u->cal_u.sit_u + pog_u->cal_u.len_w);

  c3_w dex_w;
  for (dex_w = 0; dex_w < pog_u->lit_u.len_w; ++dex_w) {
    u3a_v1_lose(pog_u->lit_u.non[dex_w]);
  }
  for (dex_w = 0; dex_w < pog_u->mem_u.len_w; ++dex_w) {
    u3a_v1_lose(pog_u->mem_u.sot_u[dex_w].key);
  }
  for (dex_w = 0; dex_w < pog_u->cal_u.len_w; ++dex_w) {
    u3j_v1_site_lose(&(pog_u->cal_u.sit_u[dex_w]));
  }
  for (dex_w = 0; dex_w < pog_u->reg_u.len_w; ++dex_w) {
    u3j_v1_rite_lose(&(pog_u->reg_u.rit_u[dex_w]));
  }
  u3a_v1_free(pog_u);
}

/* _n_v1_feb(): u3h_v1_walk helper for u3n_v1_free
 */
static void
_n_v1_feb(u3_noun kev)
{
  u3a_v1_cell *cel_u = (u3a_v1_cell*) u3a_v1_to_ptr(kev);
  _cn_v1_prog_free(u3v1to(u3n_v1_prog, cel_u->tel));
}

/* u3n_v1_free(): free bytecode cache
 */
void
u3n_v1_free(void)
{
  u3p(u3h_v1_root) har_p = u3R_v1->byc.har_p;
  u3h_v1_walk(har_p, _n_v1_feb);
  u3h_v1_free_nodes(har_p);
}

/* u3n_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3n_v1_reclaim(void)
{
  //  clear the bytecode cache
  //
  //    We can't just u3h_v1_free() -- the value is a post to a u3n_v1_prog.
  //    Note that the hank cache *must* also be freed (in u3j_v1_reclaim())
  //
  u3n_v1_free();
}


/***  vortex.c
***/

/* u3v_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3v_v1_reclaim(void)
{
  //  clear the u3v_wish cache
  //
  u3a_v1_lose(u3A_v1->yot);
  u3A_v1->yot = u3_nul;
}

/***  init
***/

void
u3_v1_load(c3_z wor_i)
{
  c3_w len_w = wor_i - 1;
  c3_w ver_w = *(u3_Loom_v1 + len_w);

  u3_assert( U3V_VER1 == ver_w );

  c3_w* mem_w = u3_Loom_v1 + 1;
  c3_w  siz_w = c3_wiseof(u3v_v1_home);
  c3_w* mat_w = (mem_w + len_w) - siz_w;

  u3H_v1 = (void *)mat_w;
  u3R_v1 = &u3H_v1->rod_u;

  u3R_v1->cap_p = u3R_v1->mat_p = u3a_v1_outa(u3H_v1);
}


/***  manage.c
***/

void
u3m_v1_reclaim(void)
{
  u3v_v1_reclaim();
  u3j_v1_reclaim();
  u3n_v1_reclaim();
  u3a_v1_reclaim();
}
